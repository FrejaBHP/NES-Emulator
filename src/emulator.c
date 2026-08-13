#include <emulator.h>
#include <console.h>
#include <rom.h>
#include <string.h>
#include <stdlib.h>

SDL_Window* Window = NULL;
SDL_Renderer* Renderer = NULL;
SDL_Texture* BGTexture = NULL;
SDL_Texture* SPRTexture = NULL;

int DesiredFrameTime;
uint64_t NextFrameTime = 0;

FILE* file = NULL;

uint8_t ROMLoaded = 0;

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialise SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("NES Emulator, Maybe", Window_Width, Window_Height, SDL_WINDOW_RESIZABLE, &Window, &Renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderLogicalPresentation(Renderer, Window_Width, Window_Height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetDefaultTextureScaleMode(Renderer, SDL_SCALEMODE_PIXELART);

    BGTexture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_TARGET, 256, 240);
    //SPRTexture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_TARGET, 256, 240);

    if (!BGTexture) {
        SDL_Log("Texture could not be created: %s", SDL_GetError());
    }

    // Calculates the amount of time a frame should ideally take in nanoseconds to sustain the set framerate
	DesiredFrameTime = 1000000000 / DesiredFrameRateNTSC;

    NextFrameTime = SDL_GetTicksNS();

    TestFnc();

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

            if (System == SYS_NTSC) {
                RunCPU(NumCPUCycles_NTSC * CPUCycleDivider_NTSC);
            }
            else {
                RunCPU(NumCPUCycles_PAL * CPUCycleDivider_PAL);
            }
            //RunPPU(CPUTimeStamp);
            //StepPPU();

            FrameCount++;
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
        SDL_Log("Can't update texture: %s", SDL_GetError());
    }
    if (!SDL_RenderTexture(Renderer, BGTexture, NULL, NULL)) {
        SDL_Log("Can't render texture: %s", SDL_GetError());
    }
    if (!SDL_RenderPresent(Renderer)) {
        SDL_Log("Can't render present: %s", SDL_GetError());
    }

    const uint64_t now = SDL_GetTicksNS();
	const uint64_t executionTime = now - NextFrameTime;

	if (executionTime < DesiredFrameTime) {
		SDL_DelayNS(DesiredFrameTime - executionTime);
	}

	NextFrameTime += DesiredFrameTime;

    ResetFrameCount();

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    DumpMemory();
    DumpPPU();
    /* SDL will clean up the window/renderer for us. */
}

void HandleNESInput() {
    InputBuffer0 = 0;

    uint8_t* inputAddr = (uint8_t*)&Input0;

    for (size_t i = 0; i < 8; i++) {
        InputBuffer0 = InputBuffer0 | (inputAddr[i] << i);
    }
}

SDL_Surface* GetSurface() {
    return SDL_GetWindowSurface(Window);
}


void TestFnc() {
    Initialisation();
    //LoadFile();
    LoadROM();
    SetupConsole();
}

void Initialisation() {
    CPUInit();
    PPUInit();
}

void LoadFile() {
    uint8_t testbuffer[8] = { 0 };

    file = fopen("testhex.bin", "rb");
    fread(testbuffer, 1, sizeof(testbuffer), file);

    memcpy(&CPUMemory[ROM_Start], testbuffer, sizeof(testbuffer));

    uint8_t buffer[32];

    for (size_t i = 0; i < 8; i++) {
        sprintf(buffer, "%X\n", testbuffer[i]);
        printf(buffer);
    }

    fclose(file);

    RunTestProgram();
}

void LoadROM() {
    uint8_t headerBuffer[16] = { 0 };

    file = fopen("AccuracyCoin.nes", "rb");
    fread(headerBuffer, 1, sizeof(headerBuffer), file);

    if (!CurROM) {
        CurROM = malloc(sizeof(ROMData));
    }

    ParseHeader(headerBuffer);

    if (CurROM->IsINES) {
        if (CurROM->PRG_ROM_Size == 0x4000U) {
            fread(&CPUMemory[ROM_Start + 0x4000U], 1, 0x4000U, file);
        }
        else if (CurROM->PRG_ROM_Size == 0x8000U) {
            fread(&CPUMemory[ROM_Start], 1, 0x8000U, file);
        }

        if (CurROM->CHR_ROM_Size == 0x2000U) {
            fread(&PPUMemory[0], 1, 0x2000U, file);
        }

        CCPU->PC = AssembleAbsoluteAddress(CPUMemory[0xFFFC], CPUMemory[0xFFFD]);
    }

    fclose(file);

    ROMLoaded = 1;

    //RunTestProgram();
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

        // 12 bits for mapper number, but only 255 valid mappers??
        //uint16_t mapperNumber = (header[6] >> 4) | ((header[7] >> 4) << 4) | ((header[8] >> 4) << 8);
        CurROM->MapperNumber = header[6] >> 4;

        CurROM->DefController = header[15];
    }
}
