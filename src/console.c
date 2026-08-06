#include "console.h"
#include <cpu.h>
#include <ppu.h>
#include <rom.h>

SystemType System = SYS_NTSC;

uint32_t CPUTimeStamp = 0;   // How many master cycles the CPU has used this frame
uint32_t PPUTimeStamp = 0;   // How many master cycles the PPU has used this frame
uint16_t CPUCycleCount = 0;  // How many CPU cycles has been used this frame
uint16_t PPUCycleCount = 0;  // How many PPU cycles has been used this frame

void SetupConsole() {
    if (CurROM->TimingMode == TMode_RP2C02) {
        System = SYS_NTSC;
    }
    else if (CurROM->TimingMode == TMode_RP2C07) {
        System = SYS_PAL;
    }
    else if (CurROM->TimingMode == TMode_Multiple) {
        // TEMP
        System = SYS_NTSC;
    }
    else if (CurROM->TimingMode == TMode_UA6538) {
        System = SYS_PAL;
    }
}

void ResetFrameCount() {
    CPUTimeStamp = 0;
    CPUCycleCount = 0;

    PPUTimeStamp = 0;
    PPUCycleCount = 0;
}

void UseCPUCycles(uint8_t amount) {
    CPUCycleCount += amount;
}

void UsePPUCycles(uint8_t amount) {
    PPUCycleCount += amount;
}
