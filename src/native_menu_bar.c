#include "native_menu_bar.h"

#include <stdio.h>

/* Select Backend */
#if defined(NMB_USE_GTK2) || defined(NMB_USE_GTK3)
    #define NMB_USE_GTK
#elif _WIN32
    #define NMB_USE_WIN32
#elif __APPLE__
    #define NMB_USE_COCOA
#else
    #error "Native Menu Bar backend not specified."
#endif

#define MAX_EVENTS 64
#define ERROR_BUFFER_SIZE 128
#define SCRATCH_BUFFER_SIZE 128

#define UNUSED(x) (void)(x)
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

static char errorBuffer[ERROR_BUFFER_SIZE];
static char scratchBuffer[SCRATCH_BUFFER_SIZE];

static struct
{
    size_t head;
    size_t tail;
    nmb_Event data[MAX_EVENTS];
} events;

static bool getEvent(nmb_Event* e)
{
    if (events.head == events.tail) return false; /* No events available */
    *e = events.data[events.head];
    events.head = (events.head + 1) % MAX_EVENTS;
    return true;
}

static void pushEvent(const nmb_Event* e)
{
    /* TODO: print a warning if the user isn't consuming events fast enough */
    if ((events.tail + 1) % MAX_EVENTS == events.head)
    {
        /* Buffer is full, overwrite the oldest event */
        events.head = (events.head + 1) % MAX_EVENTS;
    }
    events.data[events.tail] = *e;
    events.tail = (events.tail + 1) % MAX_EVENTS;
}

const char* nmb_getLastError(void)
{
    return errorBuffer;
}

void stringCopyOrSkip(char* dest, unsigned size, const char* str, char skip)
{
    if(!size || !dest || !str)
    {
        return;
    }

    unsigned iDest = 0;
    while(iDest + 1 < size && *str)
    {
        if(*str != skip)
        {
            dest[iDest] = *str;
            iDest++;
        }
        str++;
    }
    dest[iDest] = 0;
}

void stringCopyOrTranslate(char* dest, unsigned size, const char* str, char from, char to)
{
    if (!size || !dest || !str)
    {
        return;
    }

    unsigned iDest = 0;
    while (iDest + 1 < size && *str)
    {
        dest[iDest] = *str == from ? to : *str;
        iDest++;
        str++;
    }
    dest[iDest] = 0;
}

#ifdef NMB_USE_WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CAPTION_BUFFER_SIZE 256

static_assert(sizeof(HWND) == sizeof(nmb_Handle), "Window handles must be interchangeable with void*");
static_assert(sizeof(HMENU) == sizeof(nmb_Handle), "Menu handles must be interchangeable with void*");

static struct
{
    HWND hwnd;
    HMENU menuBar;
    WNDPROC originalWndProc;
    UINT nextId;
    WCHAR wcharBuffer[CAPTION_BUFFER_SIZE];
} g;

static LRESULT CALLBACK menuBarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_COMMAND)
    {
        nmb_Event e;
        e.sender = (nmb_Handle)(uintptr_t)(LOWORD(wParam));
        e.event = nmb_EventType_itemTriggered;
        pushEvent(&e);
        return 0;
    }

    return CallWindowProc(g.originalWndProc, hWnd, uMsg, wParam, lParam);
}

static WCHAR* utf8ToWide(const char* utf8)
{
    if (!utf8) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, g.wcharBuffer, CAPTION_BUFFER_SIZE);
    return g.wcharBuffer;
}

static int adjustIndex(nmb_Handle parent, int index)
{
    if(index < 0)
    {
        int numberOfItems = GetMenuItemCount((HMENU)parent);
        return numberOfItems + index + 1;
    }
    return index;
}

void nmb_setup(void* hWnd, nmb_SetupFlags flags)
{
    UNUSED(flags);
    memset(&g, 0, sizeof(g));
    errorBuffer[0] = 0;
    g.nextId = 1;
    g.hwnd = (HWND)hWnd;
    g.originalWndProc = (WNDPROC)SetWindowLongPtr(g.hwnd, GWLP_WNDPROC, (LONG_PTR)menuBarWndProc);
    g.menuBar = CreateMenu();
    if (!SetMenu(g.hwnd, g.menuBar))
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Failed to set menu on window: %lu\n", GetLastError());
    }
    if (!DrawMenuBar(g.hwnd))
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Failed to draw menu bar on window: %lu\n", GetLastError());
    }
}

void nmb_shutdown(void)
{
    SetWindowLongPtr(g.hwnd, GWLP_WNDPROC, (LONG_PTR)g.originalWndProc); /* restore the old wndproc */
    DestroyMenu(g.menuBar);
    memset(&g, 0, sizeof(g));
}

bool nmb_pollEvent(nmb_Event* event)
{
    return getEvent(event);
};

nmb_Backend nmb_getBackend()
{
    return nmb_Backend_win32;
}

nmb_Handle nmb_appendMenu(nmb_Handle parent, const char* caption)
{
    return nmb_insertMenu(parent, -1, caption);
}

nmb_Handle nmb_insertMenu(nmb_Handle parent, int index, const char* caption)
{
    index = adjustIndex(parent, index);

    if (index < -1)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Invalid index '%d' passed to '%s'\n", index, __func__);
        return NULL;
    }

    if (!parent)
    {
        parent = g.menuBar;
    }

    nmb_Handle submenu = CreatePopupMenu();
    stringCopyOrTranslate(scratchBuffer, SCRATCH_BUFFER_SIZE, caption, '_', '&');
    BOOL result = InsertMenuW((HMENU)parent, (UINT)index, MF_BYPOSITION | MF_POPUP, (UINT_PTR)submenu, utf8ToWide(scratchBuffer));
    if (!result)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Failed to insert submenu '%s'. Windows error %lu\n", caption, GetLastError());
        return NULL;
    }
    DrawMenuBar(g.hwnd);
    return submenu;
}

nmb_Handle nmb_appendMenuItem(nmb_Handle parent, const char* caption)
{
    return nmb_insertMenuItem(parent, -1, caption);
}

nmb_Handle nmb_insertMenuItem(nmb_Handle parent, int index, const char* caption)
{
    index = adjustIndex(parent, index);

    if (index < -1)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Invalid index '%d' passed to '%s'\n", index, __func__);
        return NULL;
    }

    if (!parent)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Failed to create menu item because parent was NULL\n");
        return NULL;
    }

    UINT id = g.nextId++;
    stringCopyOrTranslate(scratchBuffer, SCRATCH_BUFFER_SIZE, caption, '_', '&');
    BOOL result = InsertMenuW((HMENU)parent, (UINT)index, MF_BYPOSITION | MF_STRING, id, utf8ToWide(scratchBuffer));
    if (!result)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Failed to insert menu item '%s'. Windows error %lu\n", caption, GetLastError());
        return NULL;
    }
    DrawMenuBar(g.hwnd);
    return (nmb_Handle)(uintptr_t)id;
}

nmb_Handle nmb_appendCheckMenuItem(nmb_Handle parent, const char* caption)
{
    return nmb_appendMenuItem(parent, caption);
}

nmb_Handle nmb_insertCheckMenuItem(nmb_Handle parent, int index, const char* caption)
{
    return nmb_insertMenuItem(parent, index, caption);
}

void nmb_appendSeparator(nmb_Handle parent)
{
    nmb_insertSeparator(parent, -1);
}

void nmb_insertSeparator(nmb_Handle parent, int index)
{
    index = adjustIndex(parent, index);

    if (index < -1)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Invalid index '%d' passed to '%s'\n", index, __func__);
        return;
    }

    if (!parent)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Failed to create separator because parent was NULL\n");
        return;
    }

    InsertMenu((HMENU)parent, (UINT)index, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    DrawMenuBar(g.hwnd);
}

void nmb_setMenuItemChecked(nmb_Handle menuItem, bool checked)
{
    if (!menuItem) return;

    UINT flags = MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(GetMenu(g.hwnd), (UINT)(uintptr_t)menuItem, flags);
    DrawMenuBar(g.hwnd);
}

bool nmb_isMenuItemChecked(nmb_Handle menuItem)
{
    if (!menuItem) return false;

    UINT state = GetMenuState(g.menuBar, (UINT)(uintptr_t)menuItem, MF_BYCOMMAND);
    if (state == (UINT)-1)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Failed to get menu item state: %lu\n", GetLastError());
        return false;
    }
    return (state & MF_CHECKED) == MF_CHECKED;
}


void nmb_setMenuItemEnabled(nmb_Handle menuItem, bool enabled)
{
    if (!menuItem) return;

    UINT flags = MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_GRAYED);
    BOOL result = EnableMenuItem(g.menuBar, (UINT)(uintptr_t)menuItem, flags);
    if (result == -1)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Failed to set menu item enabled state: %lu\n", GetLastError());
        return;
    }
    DrawMenuBar(g.hwnd);
}

bool nmb_isMenuItemEnabled(nmb_Handle menuItem)
{
    if (!menuItem) return false;

    UINT state = GetMenuState(g.menuBar, (UINT)(uintptr_t)menuItem, MF_BYCOMMAND);
    if (state == (UINT)-1)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Failed to get menu item state: %lu\n", GetLastError());
        return false;
    }
    return (state & MF_GRAYED) != MF_GRAYED;
}

#endif
#ifdef NMB_USE_COCOA

#import <Cocoa/Cocoa.h>

@interface MenuHandler : NSObject
- (void)handleAction : (id)sender;
@end

static struct
{
    MenuHandler* handler;
    NSMenuItem* preferences;
} g;

@implementation MenuHandler
- (void)handleAction:(id)sender
{
    nmb_Event e;
    e.sender = sender;
    if(sender == g.preferences)
    {
        e.event = nmb_EventType_openSettings;
    }
    else
    {
        e.event = nmb_EventType_itemTriggered;
    }
    pushEvent(&e);
}
@end

/*

 Mac features

Looks like both MAC and WINDOWS give the app some menus by default.

 Mac: Doesn't create the default menus for your, but the HIG described some minimum expected menus.
 Mac SDL: SDL creates an App menu using the Bundle Name from the Info.plist + a Window menu with some stuff in it
 Windows: A default menu when you click on the app icon.

On mac you can access the App menu with [NSApp mainMenu], and presuambly modify it from there?
On windows you can apparently use GetSystenMenu()?

On mac we are likely to want to insert our custom menus after the App menu but before the Window menu, plus some after the Window menu (e.g. Help)

Idea
 appMenu = nmb_getAppMenu() // Returns the app menu on Mac, and system icon menu on windows
 fileMenu = nmb_insertMenuAfter(appMenu, "File")
 windowMenu = nmb_getMenu("Window");
 if(!windowMenu)
 {
    windowMenu = nmb_appendMennu("window")
    ... make the window menu
 }
 nmb_insertMenuAfter(windowMenu, "Help")

Result
 mac: App (default) / File / Window (default) / Help
 windows: Icon (default) / File / Window / Help

 */

static NSString* getApplicationName(void)
{
    NSString *appName = nil;

    /* check the plist for CFBundleName first. This should be a short name of 16 characters or fewer. */
    if (!appName)
    {
        appName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];
    }

    /* if the CFBundleName was not found, try the CFBundleDisplayName */
    if (!appName)
    {
        appName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleDisplayName"];
    }

    /* failing that, use the process name */
    if (!appName || [appName length] == 0)
    {
        appName = [[NSProcessInfo processInfo] processName];
    }

    return appName;
}

static void createMacPlatformMenus(bool additionalPlatformMenus)
{
    if (NSApp == nil)
        return;

    /* Create the app menu */
    NSString* appName = getApplicationName();
    NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@""];

    /* Add some minimal default menu items (the HIG actually want us to add quite a few more) */
    [appMenu addItemWithTitle:[@"About " stringByAppendingString:appName] action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];

    if(additionalPlatformMenus)
    {
        g.preferences = [appMenu addItemWithTitle:@"Settings…" action:@selector(handleAction:) keyEquivalent:@","];
        [g.preferences setTarget:g.handler];

        [appMenu addItem:[NSMenuItem separatorItem]];

        /* services */
        NSMenu* serviceMenu = [[NSMenu alloc] initWithTitle:@""];
        NSMenuItem* serviceMenuItem = [appMenu addItemWithTitle:@"Services" action:nil keyEquivalent:@""];
        [serviceMenuItem setSubmenu:serviceMenu];
        [NSApp setServicesMenu:serviceMenu];

        [appMenu addItem:[NSMenuItem separatorItem]];

        /* hide app */
        [appMenu addItemWithTitle:[@"Hide " stringByAppendingString:appName] action:@selector(hide:) keyEquivalent:@"h"];

        /* hide others */
        NSMenuItem* hideMenuItem = [appMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
        [hideMenuItem setKeyEquivalentModifierMask:(NSEventModifierFlagOption | NSEventModifierFlagCommand)];

        /* show all */
        [appMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
    }

    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:[@"Quit " stringByAppendingString:appName] action:@selector(terminate:) keyEquivalent:@"q"];

    /* Attach it the app */
    NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
    [appMenuItem setSubmenu:appMenu];
    [[NSApp mainMenu] addItem:appMenuItem];

    if(additionalPlatformMenus)
    {
        NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];

        [windowMenu addItemWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"];
        [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
        [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];

        NSMenuItem* windowMenuItem = [[NSMenuItem alloc] init];
        [windowMenuItem setSubmenu:windowMenu];
        [[NSApp mainMenu] addItem:windowMenuItem];
        [NSApp setWindowsMenu:windowMenu]; /* This adds a bunch more default menu items for the Window menu */
    }

    [appMenu release];
    [appMenuItem release];
}

static NSInteger adjustIndex(nmb_Handle parent, int index)
{
    if(index < 0)
    {
        NSInteger numberOfItems = [(NSMenu*)parent numberOfItems];
        return numberOfItems + index + 1;
    }
    return index;
}

void nmb_setup(void* windowHandle /* unused on mac */, nmb_SetupFlags flags)
{
    UNUSED(windowHandle);
    memset(&g, 0, sizeof(g));
    errorBuffer[0] = 0;
    g.handler = [[MenuHandler alloc] init];

    /* Check if someone else (e.g. SDL) already built the app menu */
    NSInteger numItemsInAppleMenu = [[[[NSApp mainMenu] itemAtIndex:0] submenu] numberOfItems];
    bool addDefaultMenuItems = numItemsInAppleMenu == 0;

    /* If not, add some default menu items */
    if(addDefaultMenuItems)
    {
        /* To add a custom app menu, we have to create our own main menu (aka menu bar) */
        NSMenu* mainMenu = [[NSMenu alloc] init];
        [NSApp setMainMenu:mainMenu];
        [mainMenu release];
        createMacPlatformMenus(flags & nmb_SetupFlags_createPlatformMenus);
    }
}

void nmb_shutdown()
{
    [g.handler release];
    memset(&g, 0, sizeof(g));
}

bool nmb_pollEvent(nmb_Event* event)
{
    return getEvent(event);
}

nmb_Backend nmb_getBackend(void)
{
    return nmb_Backend_cocoa;
}

nmb_Handle nmb_appendMenu(nmb_Handle parent, const char* caption)
{
    return nmb_insertMenu(parent, -1, caption);
}

nmb_Handle nmb_insertMenu(nmb_Handle parent, int inputIndex, const char* caption)
{
    if(!parent)
    {
        /* If parent is null, insert into the menu bar */
        parent = [NSApp mainMenu];

        if(inputIndex >= 0)
        {
            /* Offset 0 index to be the menu item AFTER the application menu. */
            inputIndex++;
        }
    }

    NSInteger index = adjustIndex(parent, inputIndex);

    if (index < 0)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Invalid index '%ld' passed to '%s'\n", index, __func__);
        return NULL;
    }

    stringCopyOrSkip(scratchBuffer, SCRATCH_BUFFER_SIZE, caption, '_');
    NSMenuItem* item = [(NSMenu*)parent insertItemWithTitle:[NSString stringWithCString:scratchBuffer encoding:NSUTF8StringEncoding] action:nil keyEquivalent:@"" atIndex:index];
    NSMenu* menu = [[NSMenu alloc] init];
    [item setSubmenu:menu];
    [menu release];
    return menu;
}

nmb_Handle nmb_appendMenuItem(nmb_Handle parent, const char* caption)
{
    return nmb_insertMenuItem(parent, -1, caption);
}

nmb_Handle nmb_insertMenuItem(nmb_Handle parent, int inputIndex, const char* caption)
{
    if(!parent)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Failed to create menu item because parent was NULL\n");
        return NULL;
    }

    NSInteger index = adjustIndex(parent, inputIndex);

    if (index < 0)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Invalid index '%ld' passed to '%s'\n", index, __func__);
        return NULL;
    }

    stringCopyOrSkip(scratchBuffer, SCRATCH_BUFFER_SIZE, caption, '_');
    NSMenuItem* item = [(NSMenu*)parent insertItemWithTitle:[NSString stringWithCString:scratchBuffer encoding:NSUTF8StringEncoding] action:@selector(handleAction:) keyEquivalent:@"" atIndex:index];
    [item setTarget:g.handler];
    return item;
}

nmb_Handle nmb_appendCheckMenuItem(nmb_Handle parent, const char* caption)
{
    return nmb_insertMenuItem(parent, -1, caption);
}

nmb_Handle nmb_insertCheckMenuItem(nmb_Handle parent, int inputIndex, const char* caption)
{
    return nmb_insertMenuItem(parent, inputIndex, caption);
}

void nmb_appendSeparator(nmb_Handle parent)
{
    nmb_insertSeparator(parent, -1);
}

void nmb_insertSeparator(nmb_Handle parent, int inputIndex)
{
    if(!parent)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Failed to create menu item because parent was NULL\n");
        return;
    }

    NSInteger index = adjustIndex(parent, inputIndex);

    if (index < 0) // on mac the index must be positive
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Invalid index '%ld' passed to '%s'\n", index, __func__);
        return;
    }

    [(NSMenu*)parent insertItem:[NSMenuItem separatorItem] atIndex:index];
}

void nmb_setMenuItemChecked(nmb_Handle menuItem, bool checked)
{
    ((NSMenuItem*)menuItem).state = checked ? NSControlStateValueOn : NSControlStateValueOff;
}

bool nmb_isMenuItemChecked(nmb_Handle menuItem)
{
    return ((NSMenuItem*)menuItem).state == NSControlStateValueOn;
}

void nmb_setMenuItemEnabled(nmb_Handle menuItem, bool enabled)
{
    ((NSMenuItem*)menuItem).action = enabled ? @selector(handleAction:) : nil;
}

bool nmb_isMenuItemEnabled(nmb_Handle menuItem)
{
    return ((NSMenuItem*)menuItem).enabled;
}

#endif
#ifdef NMB_USE_GTK

#include <gtk/gtk.h>

static struct {
    GtkWidget* menuBar;
    bool shouldToggleCheckMenuItem;
    bool isInsideOnActivate;
} context;


static int adjustIndex(nmb_Handle parent, int index)
{
    if(index < 0)
    {
        GList* list = gtk_container_get_children(GTK_CONTAINER(parent));
        guint length = g_list_length(list);
        return length + index + 1;
    }
    return index;
}

static void onActivate(GtkWidget* widget, gpointer isCheckMenuItem)
{
    if(isCheckMenuItem)
    {
        if(context.shouldToggleCheckMenuItem)
        {
            return;
        }
        else
        {
            if(!context.isInsideOnActivate)
            {
                context.isInsideOnActivate = true;
                bool isActive = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), !isActive);
                return;
            }
            context.isInsideOnActivate = false;
        }
    }

    {
        nmb_Event e;
        e.sender = widget;
        e.event = nmb_EventType_itemTriggered;
        pushEvent(&e);
    }
}

void nmb_setup(void* menuBar, nmb_SetupFlags flags)
{
    context.shouldToggleCheckMenuItem = false;
    context.menuBar = menuBar;
}

void nmb_shutdown()
{
    /*
     TODO: cleanup all the menus and items
     It should be possible to call setup() and shutdown() on the lib multiple times in the same app without
     everythign breaking.
     */
    memset(&context, 0, sizeof(context));
}

bool nmb_pollEvent(nmb_Event* event)
{
    return getEvent(event);
}

nmb_Backend nmb_getBackend(void)
{
    return nmb_Backend_gtk;
}

nmb_Handle nmb_appendMenu(nmb_Handle parent, const char* caption)
{
    return nmb_insertMenu(parent, -1, caption);
}

nmb_Handle nmb_insertMenu(nmb_Handle parent, int index, const char* caption)
{
    if(!parent)
    {
        parent = context.menuBar;
    }

    index = adjustIndex(parent, index);

    if(index < 0)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Invalid index '%d' passed to '%s'\n", index, __func__);
        return NULL;
    }

    GtkWidget* new_item = gtk_menu_item_new_with_mnemonic(caption);
    gtk_menu_shell_insert(GTK_MENU_SHELL(parent), new_item, index);

    GtkWidget* new_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(new_item), new_menu);

    return new_menu;
}

nmb_Handle nmb_appendMenuItem(nmb_Handle parent, const char* caption)
{
    return nmb_insertMenuItem(parent, -1, caption);
}

typedef GtkWidget* (*ItemCreateFunc)(const gchar* label);

static nmb_Handle internal_insertItem(nmb_Handle parent, int index, const char* caption, ItemCreateFunc createFunc, bool isCheckMenuItem)
{
    if(!parent)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Failed to create menu item because parent was NULL\n");
        return NULL;
    }

    index = adjustIndex(parent, index);

    if(index < 0)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Invalid index '%d' passed to '%s'\n", index, __func__);
        return NULL;
    }

    GtkWidget* new_item = createFunc(caption);
    gtk_menu_shell_insert(GTK_MENU_SHELL(parent), new_item, index);
    g_signal_connect(new_item, "activate", G_CALLBACK(onActivate), isCheckMenuItem ? 1 : 0);

    return new_item;
}

nmb_Handle nmb_insertMenuItem(nmb_Handle parent, int index, const char* caption)
{
    return internal_insertItem(parent, index, caption, gtk_menu_item_new_with_mnemonic, false);
}

nmb_Handle nmb_appendCheckMenuItem(nmb_Handle parent, const char* caption)
{
    return nmb_insertCheckMenuItem(parent, -1, caption);
}

nmb_Handle nmb_insertCheckMenuItem(nmb_Handle parent, int index, const char* caption)
{
    return internal_insertItem(parent, index, caption, gtk_check_menu_item_new_with_mnemonic, true);
}

void nmb_appendSeparator(nmb_Handle parent)
{
    nmb_insertSeparator(parent, -1);
}

void nmb_insertSeparator(nmb_Handle parent, int index)
{
    if(!parent)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Failed to create separator because parent was NULL\n");
        return;
    }

    index = adjustIndex(parent, index);

    if(index < 0)
    {
        snprintf(errorBuffer, ERROR_BUFFER_SIZE, "Invalid index '%d' passed to '%s'\n", index, __func__);
        return;
    }

    GtkWidget* sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(parent), sep);

    return;
}

void nmb_setMenuItemChecked(nmb_Handle menuItem, bool checked)
{
    // NB: this only works because set_active() sents a signal which is handled immediately. If some GTK implementation
    // were to defer signal handling then this wouldn't work. Presumably that would break a lot of other peoples code
    // too though...
    context.shouldToggleCheckMenuItem = true;
    gtk_check_menu_item_set_active(menuItem, checked);
    context.shouldToggleCheckMenuItem = false;
}

bool nmb_isMenuItemChecked(nmb_Handle menuItem)
{
    return gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuItem));
}

void nmb_setMenuItemEnabled(nmb_Handle menuItem, bool enabled)
{
    gtk_widget_set_sensitive(GTK_WIDGET(menuItem), enabled);
}

bool nmb_isMenuItemEnabled(nmb_Handle menuItem)
{
    return gtk_widget_get_sensitive(GTK_WIDGET(menuItem));
}

#endif
