#include <emulator.h>
#include <rom.h>
#include <string.h>
#include <stdlib.h>

SDL_Window* Window = NULL;
SDL_Renderer* Renderer = NULL;

int DesiredFrameTime;
uint64_t NextFrameTime = 0;

FILE* file = NULL;

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

    // Calculates the amount of time a frame should ideally take in nanoseconds to sustain the set framerate
	DesiredFrameTime = 1000000000 / DesiredFrameRateNTSC;

    NextFrameTime = SDL_GetTicksNS();

    TestFnc();

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if(event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
        return SDL_APP_SUCCESS;
    }

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void* appstate) {
    SDL_SetRenderDrawColor(Renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);  /* black, full alpha */
    SDL_RenderClear(Renderer);  /* start with a blank canvas. */
    SDL_SetRenderDrawColor(Renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);  /* white, full alpha */

    SDL_RenderPresent(Renderer);  /* put it all on the screen! */

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
    /* SDL will clean up the window/renderer for us. */
}


void TestFnc() {
    Initialisation();
    //LoadFile();
    LoadROM();
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

    file = fopen("01-implied.nes", "rb");
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

        CCPU->PC = AssembleAbsoluteAddress(CPUMemory[0xFFFC], CPUMemory[0xFFFD]);
    }

    fclose(file);

    RunTestProgram();
}

void ParseHeader(uint8_t* header) {
    bool isINES = false;
    if (header[0] == 0x4EU && header[1] == 0x45U && header[2] == 0x53U && header[3] == 0x1AU) {
        isINES = true;
    }

    CurROM->IsINES = isINES;

    if (CurROM->IsINES) {
        CurROM->Layout = CheckBit(header[6], 0);
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
