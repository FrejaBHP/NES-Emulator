#ifndef def_Console
#define def_Console

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

typedef enum SystemType {
    SYS_NTSC,
    SYS_PAL
} SystemType;

typedef struct ControllerInput {
    bool AHeld;
    bool BHeld;
    bool SelectHeld;
    bool StartHeld;
    bool UpHeld;
    bool DownHeld;
    bool LeftHeld;
    bool RightHeld;
} ControllerInput;

typedef struct SpriteData {
    uint8_t PositionY;
    uint8_t TileIndex;
    uint8_t Attributes;
    uint8_t PositionX;
} SpriteData;

typedef struct EmuState {
    uint16_t Addr;
    uint8_t OpCode;
    uint8_t Acc;
    uint8_t RegX;
    uint8_t RegY;
} EmuState;

// Horizontal resolution is 256, with 85 extra for the horizontal blanking period, making up 256 + 85 = 341 scanline length
#define Scanline_Length         341 // in PPU pixels/cycles
#define HBlank_Length            85 // in PPU pixels/cycles

#define Scanlines_NTSC          262
#define Scanlines_PAL           312

#define VBlankScanlines_NTSC     20
#define VBlankScanlines_PAL      70

#define VBlankTime_NTSC         Scanline_Length * VBlankScanlines_NTSC * PPUCycleDivider
#define VBlankTime_PAL          Scanline_Length * VBlankScanlines_PAL * PPUCycleDivider

#define NumDots_NTSC            ((Scanline_Length * Scanlines_NTSC) - 0.5) // Number of dots per frame. Every odd frame has -1 dot
#define NumCPUCycles_NTSC       (NumDots_NTSC / 3) // Number of CPU cycles available per frame

#define NumDots_PAL             (Scanline_Length * Scanlines_PAL) // Number of dots per frame
#define NumCPUCycles_PAL        (NumDots_PAL / 3) // Number of CPU cycles available per frame

#define CPUCycleDivider_NTSC     15
#define CPUCycleDivider_PAL      16
#define PPUCycleDivider           5

extern SystemType System;

extern uint32_t CPUTimeStamp;   // How many master cycles the CPU has used this frame
extern uint32_t PPUTimeStamp;   // How many master cycles the PPU has used this frame
extern uint32_t CPUCycleCount;  // How many CPU cycles has been used this frame
extern uint32_t CPUCycleCountLast;
extern uint32_t PPUCycleCount;  // How many PPU cycles has been used this frame
extern uint8_t CPUCyclesCarry;

extern uint32_t FrameCount;

extern int16_t CurScanline;
extern uint16_t CurDot;
extern uint8_t* BGFrameBuffer;
extern uint8_t* SPRFrameBuffer;

extern uint8_t DMAOccured;

extern uint8_t StopExecution;
extern uint8_t HasAnnouncedStop;

extern ControllerInput Input0;
extern uint8_t Input0Conv;
extern uint8_t Input0Buffer;

extern EmuState States[16];
extern size_t StateIndex;

void SetupConsole();
void ResetFrameCount();
void UseCPUCycles(uint8_t amount);
void UsePPUCycles(uint8_t amount);

uint8_t IsVisibleOnScanline(uint8_t scanline, uint8_t topY);

// Execute until timestamp is reached
void RunCPU(uint32_t timestamp);
void RunPPU(uint32_t timestamp);

void DrawBGLayer();
void GetValidSPR(SpriteData* sprites);
void DrawSPRLayer();
void DrawSPR(SpriteData* spr);

void WriteStateLog(uint8_t inst);
void DumpStateLog(size_t result);

#endif
