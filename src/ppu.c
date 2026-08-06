#include <ppu.h>
#include <cpu.h>
#include <console.h>
#include <stdlib.h>

#define VMAX        0x7FFFU
#define TMAX        0x7FFFU
#define XMAX        0x00FFU
#define WMAX        0x0001U

#define PPUCTRL_BaseNameTableAddrLow        0U
#define PPUCTRL_BaseNameTableAddrHigh       1U
#define PPUCTRL_VRAMIncrement               2U
#define PPUCTRL_SPRPatternTableAddr         3U
#define PPUCTRL_BGPatternTableAddr          4U
#define PPUCTRL_SpriteSize                  5U
#define PPUCTRL_MasterSlaveSelect           6U
#define PPUCTRL_VBlankNMIEnable             7U

#define PPUMASK_Greyscale                   0U
#define PPUMASK_ShowBGLeftmost8PX           1U
#define PPUMASK_ShowSPRLeftmost8PX          2U
#define PPUMASK_EnableBGRendering           3U
#define PPUMASK_EnableSPRRendering          4U
#define PPUMASK_EmphasiseRed_NTSC           5U
#define PPUMASK_EmphasiseGreen_PAL          5U
#define PPUMASK_EmphasiseGreen_NTSC         6U
#define PPUMASK_EmphasiseRed_PAL            6U
#define PPUMASK_EmphasiseBlue               7U

#define PPUSTATUS_OpenBus0                  0U
#define PPUSTATUS_OpenBus1                  1U
#define PPUSTATUS_OpenBus2                  2U
#define PPUSTATUS_OpenBus3                  3U
#define PPUSTATUS_OpenBus4                  4U
#define PPUSTATUS_SpriteOverflow            5U
#define PPUSTATUS_Sprite0Hit                6U
#define PPUSTATUS_VBlank                    7U // Cleared on read



PPU* CurPPU = NULL;
uint8_t* PPUMemory = NULL;

void PPUSetV(uint16_t value) {
    if (value > VMAX) {
        value = value % (VMAX + 1U);
    }

    CurPPU->RegV = value;
}

void PPUSetT(uint16_t value, uint8_t clearBit) {
    if (value > TMAX) {
        value = value % (TMAX + 1U);
    }

    if (clearBit) {
        value = value & ~((uint16_t)1 << 14);
    }

    CurPPU->RegT = value;
}

void PPUSetX(uint8_t value) {
    if (value > XMAX) {
        value = value % (XMAX + 1U);
    }

    CurPPU->RegX = value;
}

void PPUSetW(uint8_t value) {
    if (value > WMAX) {
        value = value % (WMAX + 1U);
    }

    CurPPU->RegW = value;
}

void PPUWrite(uint16_t index, uint8_t value) {
    // Safety precaution. Shouldn't happen, but hey
    if (index > 0x3FFFU) {
        index = index % 0x4000U;
    }

    PPUMemory[index] = value;
}

uint8_t PPURead(uint16_t index) {
    // Safety precaution
    if (index > 0x3FFFU) {
        index = index % 0x4000U;
    }

    return PPUMemory[index];
}

uint8_t* PPUGetAddr(uint16_t index) {
    // Safety precaution
    if (index > 0x3FFFU) {
        index = index % 0x4000U;
    }

    return &PPUMemory[index];
}


void PPUInit() {
    CurPPU = malloc(sizeof(PPU));
    PPUMemory = calloc(1, 0x3FFFU + 1U);

    CurPPU->RegV = 0;
    CurPPU->RegT = 0;
    CurPPU->RegX = 0;
    CurPPU->RegW = 0;

    CurPPU->PPUCTRL = &CPUMemory[PPU_PPUCTRL];
    CurPPU->PPUMASK = &CPUMemory[PPU_PPUMASK];
    CurPPU->PPUSTATUS = &CPUMemory[PPU_PPUSTATUS];
    CurPPU->OAMADDR = &CPUMemory[PPU_OAMADDR];
    CurPPU->OAMDATA = &CPUMemory[PPU_OAMDATA];
    CurPPU->PPUSCROLL = &CPUMemory[PPU_PPUSCROLL];
    CurPPU->PPUADDR = &CPUMemory[PPU_PPUADDR];
    CurPPU->PPUDATA = &CPUMemory[PPU_PPUDATA];
}

uint16_t GetBaseNameTableAddress() {
    uint8_t multValue = CheckBit(*CurPPU->PPUCTRL, PPUCTRL_BaseNameTableAddrLow) + (CheckBit(*CurPPU->PPUCTRL, PPUCTRL_BaseNameTableAddrHigh) * 2);

    uint16_t addr = 0x2000U + (0x0400U * multValue);
    return addr;
}


void OnReadPPUSTATUS() {
    CurPPU->RegW = 0U;
}

void OnReadPPUDATA() {
    if (!CheckBit(*CurPPU->PPUCTRL, PPUCTRL_VRAMIncrement)) {
        PPUSetV(CurPPU->RegV + 1U);
    }
    else {
        PPUSetV(CurPPU->RegV + 32U);
    }
}


void OnWriteToPPUCTRL() {
    OverrideBit16(&CurPPU->RegT, 10, CheckBit(*CurPPU->PPUCTRL, 0));
    OverrideBit16(&CurPPU->RegT, 11, CheckBit(*CurPPU->PPUCTRL, 1));
}

void OnWriteToPPUSCROLL() {
    if (!CurPPU->RegW) {
        for (size_t i = 0; i < 8; i++) {
            if (i < 3) {
                OverrideBit8(&CurPPU->RegX, i, CheckBit(*CurPPU->PPUSCROLL, i));
            }
            else {
                OverrideBit16(&CurPPU->RegT, i - 3, CheckBit(*CurPPU->PPUSCROLL, i));
            }
        }

        PPUSetW(1U);
    }
    else {
        for (size_t i = 0; i < 8; i++) {
            if (i < 3) {
                OverrideBit16(&CurPPU->RegT, i + 12, CheckBit(*CurPPU->PPUSCROLL, i));
            }
            else {
                OverrideBit16(&CurPPU->RegT, i + 2, CheckBit(*CurPPU->PPUSCROLL, i));
            }
        }

        PPUSetW(0U);
    }
}

void OnWriteToPPUADDR() {
    uint16_t newValue;
    uint8_t highByte;
    uint8_t lowByte;

    if (!CurPPU->RegW) { // Write high byte
        highByte = *CurPPU->PPUADDR;
        lowByte = GetLowByte(CurPPU->RegT);

        newValue = AssembleAbsoluteAddress(lowByte, highByte);
        PPUSetT(newValue, 1);
    }
    else { // Write low byte
        highByte = GetHighByte(CurPPU->RegT);
        lowByte = *CurPPU->PPUADDR;

        newValue = AssembleAbsoluteAddress(lowByte, highByte);
        PPUSetT(newValue, 0);
        PPUSetV(CurPPU->RegT);
    }
}

void OnWriteToPPUDATA() {
    PPUWrite(CurPPU->RegV, *CurPPU->PPUDATA);

    if (!CheckBit(*CurPPU->PPUCTRL, PPUCTRL_VRAMIncrement)) {
        PPUSetV(CurPPU->RegV + 1U);
    }
    else {
        PPUSetV(CurPPU->RegV + 32U);
    }
}

void OnWriteToOAMDATA() { // 2 cycles
    CurPPU->OAM[*CurPPU->OAMADDR] = *CurPPU->OAMDATA;
}
