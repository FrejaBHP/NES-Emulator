#ifndef def_Console
#define def_Console

#include <stdint.h>
#include <stdio.h>

typedef enum SystemType {
    SYS_NTSC,
    SYS_PAL
} SystemType;

#define Scanline_Length         341 // in pixels
#define HBlank_Length            85 // in pixels

#define Scanlines_NTSC          262
#define Scanlines_PAL           312

#define NumDots_NTSC            (Scanline_Length * Scanlines_NTSC) - 0.5 // Number of dots per frame. Every odd frame has -1 dot
#define NumCPUCycles_NTSC       NumDots_NTSC / 3 // Number of CPU cycles available per frame

#define NumDots_PAL             Scanline_Length * Scanlines_PAL // Number of dots per frame
#define NumCPUCycles_PAL        NumDots_PAL / 3 // Number of CPU cycles available per frame

#define CPUCycleDivider_NTSC     15
#define CPUCycleDivider_PAL      16
#define PPUCycleDivider           5

extern SystemType System;

extern uint32_t CPUTimeStamp;   // How many master cycles the CPU has used this frame
extern uint32_t PPUTimeStamp;   // How many master cycles the PPU has used this frame
extern uint16_t CPUCycleCount;  // How many CPU cycles has been used this frame
extern uint16_t PPUCycleCount;  // How many PPU cycles has been used this frame

void SetupConsole();
void ResetFrameCount();
void UseCPUCycles(uint8_t amount);
void UsePPUCycles(uint8_t amount);

#endif
