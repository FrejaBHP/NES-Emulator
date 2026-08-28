#ifndef def_EMU
#define def_EMU

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cpu.h>
#include <ppu.h>
#include <stdio.h>

#define Window_Height       240 * 2
#define Window_Width        256 * 2

#define DesiredFrameRateNTSC    60.0998
#define DesiredFrameRatePAL     50.0070

extern SDL_Window* Window;
extern SDL_Renderer* Renderer;
extern SDL_Texture* BGTexture;
extern SDL_Texture* SPRTexture;

extern uint64_t NextFrameTime;

extern FILE* file;
extern uint8_t ROMLoaded;

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]);
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event);
SDL_AppResult SDL_AppIterate(void* appstate);
void SDL_AppQuit(void* appstate, SDL_AppResult result);

void HandleNESInput();

void EmulatorStart();
void Initialisation();
void LoadROM();
void ParseHeader(uint8_t* header);

#endif
