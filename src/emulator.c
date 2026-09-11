#include <emulator.h>
#include <console.h>
#include <apu.h>
#include <rom.h>
#include <string.h>
#include <stdlib.h>

SDL_Window* Window = NULL;
SDL_Renderer* Renderer = NULL;
SDL_Texture* BGTexture = NULL;
SDL_Texture* SPRTexture = NULL;

int DesiredFrameTime;
uint64_t NextFrameTime = 0;

FILE* ROMFile = NULL;

uint8_t ROMLoaded = 0;

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialise SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("NES Emulator, Maybe", Window_Width, Window_Height, SDL_WINDOW_RESIZABLE, &Window, &Renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderLogicalPresentation(Renderer, Window_Width, Window_Height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetDefaultTextureScaleMode(Renderer, SDL_SCALEMODE_PIXELART);

    BGTexture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 256, 240);
    SPRTexture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 256, 240);

    if (!BGTexture) {
        SDL_Log("Texture could not be created: %s", SDL_GetError());
    }
    if (!SPRTexture) {
        SDL_Log("Texture could not be created: %s", SDL_GetError());
    }

    AudioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!AudioDevice) {
        SDL_Log("Couldn't open audio device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /*
    SDL_Surface* icon = SDL_LoadSurface("nesderg2.bmp");
    SDL_SetWindowIcon(Window, icon);
    SDL_DestroySurface(icon);
    */

    EmulatorStart();

    // Calculates the amount of time a frame should ideally take in nanoseconds to sustain the set framerate
    if (System == SYS_NTSC) {
        DesiredFrameTime = 1000000000 / DesiredFrameRateNTSC;
    }
    else {
        DesiredFrameTime = 1000000000 / DesiredFrameRatePAL;
    }

    NextFrameTime = SDL_GetTicksNS();

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if(event->type == SDL_EVENT_KEY_DOWN) {
        switch (event->key.key) {
            case SDLK_ESCAPE:
                return SDL_APP_SUCCESS;
                break;

            case SDLK_RIGHT:
                Input0.RightHeld = true;
                break;

            case SDLK_LEFT:
                Input0.LeftHeld = true;
                break;

            case SDLK_DOWN:
                Input0.DownHeld = true;
                break;

            case SDLK_UP:
                Input0.UpHeld = true;
                break;

            case SDLK_PERIOD:
                Input0.StartHeld = true;
                break;

            case SDLK_COMMA:
                Input0.SelectHeld = true;
                break;

            case SDLK_X:
                Input0.BHeld = true;
                break;

            case SDLK_Z:
                Input0.AHeld = true;
                break;

            case SDLK_SPACE:
                if (StopExecution) {
                    HasAnnouncedStop = 0;
                }
                StopExecution = !StopExecution;
                break;

            default:
                break;
        }
    }

    if(event->type == SDL_EVENT_KEY_UP) {
        switch (event->key.key) {
            case SDLK_RIGHT:
                Input0.RightHeld = false;
                break;

            case SDLK_LEFT:
                Input0.LeftHeld = false;
                break;

            case SDLK_DOWN:
                Input0.DownHeld = false;
                break;

            case SDLK_UP:
                Input0.UpHeld = false;
                break;

            case SDLK_PERIOD:
                Input0.StartHeld = false;
                break;

            case SDLK_COMMA:
                Input0.SelectHeld = false;
                break;

            case SDLK_X:
                Input0.BHeld = false;
                break;

            case SDLK_Z:
                Input0.AHeld = false;
                break;

            default:
                break;
        }
    }

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void* appstate) {
    if (ROMLoaded) {
        if (!StopExecution) {
            HandleNESInput();
            NMIOccured = false;

            if (System == SYS_NTSC) {
                RunCPU(NumCPUCycles_NTSC * CPUCycleDivider_NTSC);
            }
            else {
                RunCPU(NumCPUCycles_PAL * CPUCycleDivider_PAL);
            }

            FrameCount++;
            ResetFrameCount();
        }
        else {
            if (!HasAnnouncedStop) {
                HasAnnouncedStop = 1;
                SDL_Log("Execution halted\n");
            }
        }
    }

    //SDL_SetRenderDrawColor(Renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);  /* black, full alpha */
    //SDL_RenderClear(Renderer);  /* start with a blank canvas. */
    //SDL_SetRenderDrawColor(Renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);  /* white, full alpha */

    if (!SDL_UpdateTexture(BGTexture, NULL, BGFrameBuffer, 256 * sizeof(uint8_t) * 3)) {
        SDL_Log("Can't update BG texture: %s", SDL_GetError());
    }
    if (!SDL_UpdateTexture(SPRTexture, NULL, SPRFrameBuffer, 256 * sizeof(uint8_t) * 4)) {
        SDL_Log("Can't update SPR texture: %s", SDL_GetError());
    }
    if (!SDL_RenderTexture(Renderer, BGTexture, NULL, NULL)) {
        SDL_Log("Can't render BG texture: %s", SDL_GetError());
    }
    if (!SDL_RenderTexture(Renderer, SPRTexture, NULL, NULL)) {
        SDL_Log("Can't render SPR texture: %s", SDL_GetError());
    }
    if (!SDL_RenderPresent(Renderer)) {
        SDL_Log("Can't render present: %s", SDL_GetError());
    }

    SDL_PutAudioStreamData(Stream, SoundBuffer, SampleCounter * 2);

    AlternateFrame = !AlternateFrame;
    SampleCounter = 0;

    const uint64_t now = SDL_GetTicksNS();
	const uint64_t executionTime = now - NextFrameTime;

	if (executionTime < DesiredFrameTime) {
		SDL_DelayNS(DesiredFrameTime - executionTime);
	}

	NextFrameTime += DesiredFrameTime;

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    DumpMemory();
    DumpPPU();
    DumpStateLog((size_t)result);

    SDL_CloseAudioDevice(AudioDevice);
    SDL_DestroyAudioStream(Stream);
    
    /* SDL will clean up the window/renderer for us. */
}

void HandleNESInput() {
    Input0Conv = 0;

    uint8_t* inputAddr = (uint8_t*)&Input0;

    for (size_t i = 0; i < 8; i++) {
        Input0Conv = Input0Conv | (inputAddr[i] << i);
    }

    if (CheckBit(JOY0Latch, 0U)) {
        Input0Buffer = Input0Conv;
    }
}

void EmulatorStart() {
    Initialisation();
    LoadROM();
    PPUPostInit();
    SetupConsole();
}

void Initialisation() {
    CPUInit();
    APUInit();
    PPUInit();
}

void LoadROM() {
    uint8_t headerBuffer[16] = { 0 };

    char ROMname[] = "Battletoads (Europe).nes";

    ROMFile = fopen(ROMname, "rb");
    fread(headerBuffer, 1, sizeof(headerBuffer), ROMFile);

    if (!CurROM) {
        CurROM = malloc(sizeof(ROMData));
    }

    ParseHeader(headerBuffer);

    if (CurROM->IsINES) {
        if (!ROM_PRG) {
            if (CurROM->PRG_ROM_Size < 0x80000) {
                ROM_PRG = malloc(CurROM->PRG_ROM_Size);
                fread(ROM_PRG, 1, CurROM->PRG_ROM_Size, ROMFile);
            }
            else {
                printf("PRGROM is fockin' massiv, bruv. Quitting.");
                abort();
            }
        }
        if (!ROM_CHR) {
            if (CurROM->CHR_ROM_Size == 0 && CurROM->CHR_RAM_Size != 0) {
                ROM_CHR = malloc(CurROM->CHR_RAM_Size);
                fread(ROM_CHR, 1, CurROM->CHR_RAM_Size, ROMFile);
            }
            else if (CurROM->CHR_ROM_Size < 0x40000) {
                ROM_CHR = malloc(CurROM->CHR_ROM_Size);
                fread(ROM_CHR, 1, CurROM->CHR_ROM_Size, ROMFile);
            }
            else {
                printf("CHRROM is fockin' massiv, bruv. Quitting.");
                abort();
            }
        }

        if (CurROM->PRG_ROM_Size == 0x4000U) {
            //fread(&CPUMemory[ROM_Start + 0x4000U], 1, 0x4000U, ROMFile);
            memcpy(&CPUMemory[ROM_Start + 0x4000U], ROM_PRG, 0x4000U);
        }
        else if (CurROM->PRG_ROM_Size == 0x8000U) {
            //fread(&CPUMemory[ROM_Start], 1, 0x8000U, ROMFile);
            memcpy(&CPUMemory[ROM_Start], ROM_PRG, 0x8000U);
        }
        else {
            //fread(&CPUMemory[ROM_Start], 1, 0x8000U, ROMFile);
            memcpy(&CPUMemory[ROM_Start], ROM_PRG, 0x8000U);
        }

        if (CurROM->CHR_ROM_Size == 0x2000U) {
            memcpy(&PPUMemory[0], ROM_CHR, 0x2000U);
            //fread(&PPUMemory[0], 1, 0x2000U, ROMFile);
        }
        else {
            memcpy(&PPUMemory[0], ROM_CHR, 0x2000U);
            //fread(&PPUMemory[0], 1, 0x2000U, ROMFile);
        }

        CCPU->PC = AssembleAbsoluteAddress(CPUMemory[0xFFFC], CPUMemory[0xFFFD]);
    }

    fclose(ROMFile);

    ROMLoaded = 1;

    // Probably should bounds check this eventually, as a precaution
    char appName[128] = "NESD  -  ";
    ROMname[strlen(ROMname) - 4] = '\0';
    strcat(appName, ROMname);

    if (CurROM->TimingMode == TMode_RP2C07) {
        strcat(appName, " [PAL]");
    }
    else {
        strcat(appName, " [NTSC]");
    }

    SDL_SetWindowTitle(Window, appName);
}

void ParseHeader(uint8_t* header) {
    bool isINES = false;
    if (header[0] == 0x4EU && header[1] == 0x45U && header[2] == 0x53U && header[3] == 0x1AU) {
        isINES = true;
    }

    CurROM->IsINES = isINES;

    if (CurROM->IsINES) {
        CurROM->Layout = CheckBit(header[6], 0);
        CurROM->HasAltNTL = CheckBit(header[6], 3);
        CurROM->ConsoleType = (header[7] << 6) >> 6;
        CurROM->IsNES2 = CheckBit(header[7], 3);
        CurROM->TimingMode = header[12];

        CurROM->PRG_ROM_Size = header[4] * 0x4000U;
        CurROM->CHR_ROM_Size = header[5] * 0x2000U;
        uint8_t shiftCount = header[11] & 0b1111;
        CurROM->CHR_RAM_Size = 64 << shiftCount;

        // 12 bits for mapper number, but only 255 valid mappers??
        //uint16_t mapperNumber = (header[6] >> 4) | ((header[7] >> 4) << 4) | ((header[8] >> 4) << 8);
        CurROM->MapperNumber = header[6] >> 4;

        CurROM->DefController = header[15];

        printf("PRG_ROM Size: %uB, CHR_ROM Size: %uB, CHR_RAM Size: %uB\n", CurROM->PRG_ROM_Size, CurROM->CHR_ROM_Size, CurROM->CHR_RAM_Size);
    }
}
