#ifndef def_Console
#define def_Console

#include <cpu.h>
#include <ppu.h>

#include <stdint.h>
#include <stdio.h>

#define Scanline_Length     341 // in pixels
#define HBlank_Length        85 // in pixels

#define Scanlines_NTSC      261 // technically 262, but 1 is a pre-render line between VBlank and next frame
#define Scanlines_PAL       312

#endif
