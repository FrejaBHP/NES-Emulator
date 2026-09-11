#include <ppu.h>
#include <cpu.h>
#include <rom.h>
#include <console.h>
#include <stdlib.h>

#define VMAX        0x7FFFU
#define TMAX        0x7FFFU
#define XMAX        0x00FFU
#define WMAX        0x0001U


PPU* CurPPU = NULL;
uint8_t* PPUMemory = NULL;

uint8_t PPUAXNTSelect = 0;
uint8_t* PPUAXMem = NULL;

uint32_t Palette_NTSC_old[64] = {
    0x7C7C7C, 0x0000FC, 0x0000BC, 0x4428BC, 0x940084, 0xA80020, 0xA81000, 0x881400, 0x503000, 0x007800, 0x006800, 0x005800, 0x004058, 0x000000, 0x000000, 0x000000,
    0xBCBCBC, 0x0078F8, 0x0058F8, 0x6844FC, 0xD800CC, 0xE40058, 0xF83800, 0xE45C10, 0xAC7C00, 0x00B800, 0x00A800, 0x00A844, 0x008888, 0x000000, 0x000000, 0x000000,
    0xF8F8F8, 0x3CBCFC, 0x6888FC, 0x9878F8, 0xF878F8, 0xF85898, 0xF87858, 0xFCA044, 0xF8B800, 0xB8F818, 0x58D854, 0x58F898, 0x00E8D8, 0x787878, 0x000000, 0x000000,
    0xFCFCFC, 0xA4E4FC, 0xB8B8F8, 0xD8B8F8, 0xF8B8F8, 0xF8A4C0, 0xF0D0B0, 0xFCE0A8, 0xF8D878, 0xD8F878, 0xB8F8B8, 0xB8F8D8, 0x00FCFC, 0xF8D8F8, 0x000000, 0x000000
};

uint32_t Palette_NTSC[64] = {
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00, 0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
    0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00, 0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
    0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22, 0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
    0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5, 0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000
};


int8_t BGRenderFlagCountdown = 0;
int8_t SPRRenderFlagCountdown = 0;

uint8_t PendingBGRenderFlag = 0;
uint8_t PendingSPRRenderFlag = 0;

Sprite0Data SPR0Data = { 0 };

bool BGRenderingEnabled = false;
bool SPRRenderingEnabled = false;

void PPUInit() {
    CurPPU = malloc(sizeof(PPU));
    PPUMemory = calloc(1, 0x4000U);

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

void PPUPostInit() {
    if (CurROM->MapperNumber == (uint16_t)Map_AxROM) {
        PPUAXMem = malloc(0x400U);
    }
}


void PPUSetV(uint16_t value) {
    //printf("Set V - PreValue: %04X\n", value);
    if (value > VMAX) {
        value = value % (VMAX + 1U);
    }

    CurPPU->RegV = value;
    //printf("Set V - PostValue: %04X, RegV: %04X\n", value, CurPPU->RegV);
}

void PPUSetT(uint16_t value, uint8_t clearBit) {
    //printf("Set T - PreValue: %04X\n", value);
    if (value > TMAX) {
        value = value % (TMAX + 1U);
    }

    if (clearBit) {
        value = value & ~((uint16_t)1 << 14);
    }

    CurPPU->RegT = value;
    //printf("Set T - PostValue: %04X, RegT: %04X\n", value, CurPPU->RegT);
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

void PPUIncCoarseX() {
    // Bits 0-4 are incremented, with overflow toggling bit 10

    if ((CurPPU->RegV & 0x001F) == 31) {
        CurPPU->RegV &= ~0x001F;                // set Coarse X to 0
        CurPPU->RegV ^= 0x0400;                 // switch horizontal nametable
    }
    else {
        CurPPU->RegV += 1;                      // increment Coarse X
    }
}

uint16_t SimulateIncCoarseX() {
    uint16_t retValue = CurPPU->RegV;

    if ((CurPPU->RegV & 0x001F) == 31) {
        retValue &= ~0x001F;                // set Coarse X to 0
        retValue ^= 0x0400;                 // switch horizontal nametable
    }
    else {
        retValue += 1;                      // increment Coarse X
    }

    return retValue;
}

uint16_t SimulateIncCoarseY() {
    uint16_t retValue = CurPPU->RegV;

    uint16_t coarseY = (CurPPU->RegV & 0x03E0) >> 5;

    if (coarseY == 29) {                    // if last row of tiles in nametable
        coarseY = 0;
        retValue ^= 0x0800;                 // change vertical nametable
    }
    else if (coarseY == 31) {
        coarseY = 0;                        // wrap without changing nametable
    }
    else {
        coarseY += 1;                       // increment Coarse Y
    }

    retValue = (retValue & ~0x03E0) | (coarseY << 5);   // update V with Coarse Y value

    return retValue;
}

void PPUIncFineY() {
    // Bits 12-14 are fine Y. Bits 5-9 are coarse Y. Bit 11 selects the vertical nametable

    if ((CurPPU->RegV & 0x7000) != 0x7000) {    // if Fine Y < 7
        CurPPU->RegV += 0x1000;                 // increment Fine Y
    }
    else {
        CurPPU->RegV &= ~0x7000;                // set Fine Y to 0

        uint16_t coarseY = (CurPPU->RegV & 0x03E0) >> 5;

        if (coarseY == 29) {                    // if last row of tiles in nametable
            coarseY = 0;
            CurPPU->RegV ^= 0x0800;             // change vertical nametable
        }
        else if (coarseY == 31) {
            coarseY = 0;                        // wrap without changing nametable
        }
        else {
            coarseY += 1;                       // increment Coarse Y
        }

        CurPPU->RegV = (CurPPU->RegV & ~0x03E0) | (coarseY << 5);   // update V with Coarse Y value
    }
}

uint16_t GetTileAddress() {
    return (uint16_t)(0x2000U | (CurPPU->RegV & 0x0FFFU));
}

uint16_t GetOffsetTileAddress(uint16_t simV) {
    return (uint16_t)(0x2000U | (simV & 0x0FFFU));
}

uint16_t GetAttributeAddress() {
    return (uint16_t)(0x23C0U | (CurPPU->RegV & 0x0C00U) | ((CurPPU->RegV >> 4) & 0x38) | ((CurPPU->RegV >> 2) & 0x07));
}

uint16_t GetOffsetAttributeAddress(uint16_t simV) {
    return (uint16_t)(0x23C0U | (simV & 0x0C00U) | ((simV >> 4) & 0x38) | ((simV >> 2) & 0x07));
}

void PPUAXSwapNT(uint8_t num) {
    PPUAXNTSelect = num;
    //printf("Swapped nametable. Table: %u. PC: %04X. Scanline: %i\n", num, CCPU->PC - 3, CurScanline);
}


void PPUWrite(uint16_t index, uint8_t value) {
    if (index > 0x3FFFU) {
        //StopExecution = 1;
        printf("Tried to write above 0x3FFF in VRAM. Index: %04X. V: %04X, T: %04X. PC: %04X. Scanline: %i\n", index, CurPPU->RegV, CurPPU->RegT, CCPU->PC - 3, CurScanline);
        //printf("Wrote %02X to %04X. PC: %04X. Scanline: %i\n", value, index, CCPU->PC - 3, CurScanline);
        //index = index % 0x4000U;
        return;
    }
    else if (index < 0x2000U) {
        if (CurROM->CHR_ROM_Size != 0) {
            //printf("Tried to write below 0x2000 in VRAM. Index: %04X. V: %04X, T: %04X. PC: %04X. Scanline: %i\n", index, CurPPU->RegV, CurPPU->RegT, CCPU->PC - 3, CurScanline);
            return;
        }
        else {
            PPUMemory[index] = value;
            return;
        }
    }

    if (index >= 0x2000U && index < 0x3F00U) {
        if (CurROM->MapperNumber != (uint16_t)Map_AxROM) {
            if (!CurROM->HasAltNTL) {
                if (CurROM->Layout == NTL_Vertical) {
                    if ((index >= 0x2000U && index < 0x2400U) || (index >= 0x2800U && index < 0x2C00U)) {
                        PPUMemory[index] = value;
                        PPUMemory[index + 0x0400U] = value;
                    }
                }
                else if (CurROM->Layout == NTL_Horizontal) {
                    if ((index >= 0x2000U && index < 0x2400U) || (index >= 0x2400U && index < 0x2800U)) {
                        PPUMemory[index] = value;
                        PPUMemory[index + 0x0800U] = value;
                    }
                }
            }
        }
        else {
            if (PPUAXNTSelect == 0 && index < 0x2400U) {
                PPUMemory[index] = value;
                PPUMemory[index + 0x0400U] = value;
                PPUMemory[index + 0x0800U] = value;
                PPUMemory[index + 0x0C00U] = value;
            }
            else {
                PPUAXMem[(index - 0x2000U) % 0x0400U] = value;
                return;
            }
        }
    }

    // Each palette value 0 is mirrored between BG and SPR
    if (index >= 0x3F00U) {
        PPUMemory[index] = value;

        if (index % 4 == 0) {
            if (index >= 0x3F10U) {
                PPUMemory[index - 0x10U] = value;
            }
            else {
                PPUMemory[index + 0x10U] = value;
            }
        }
    }

    /*
    if (index >= 0x2BE0 && index < 0x2BF0) {
        printf("Wrote %02X to %04X. PC: %04X. Scanline: %i\n", value, index, CCPU->PC - 3, CurScanline);
    }
    */
}

uint8_t PPURead(uint16_t index) {
    // Safety precaution
    if (index > 0x3FFFU) {
        printf("Tried to read above 0x3FFF in VRAM. Index: %04X. V: %04X, T: %04X. PC: %04X. Scanline: %i\n", index, CurPPU->RegV, CurPPU->RegT, CCPU->PC - 3, CurScanline);
        index = index % 0x4000U;
    }
    else if (index >= 0x3F20U) {
        index = (index % (uint16_t)PaletteRAMIndeces_Size) + (uint16_t)PaletteRAMIndeces_Start;
    }
    else if (index >= (uint16_t)UnusedSection_Start && index < (uint16_t)PaletteRAMIndeces_Start) {
        index -= 0x1000;
    }

    if (CurROM->MapperNumber != (uint16_t)Map_AxROM) {
        if (!CurROM->HasAltNTL) {
            if (CurROM->Layout == NTL_Vertical) {
                if ((index >= 0x2000U && index < 0x2400U) || (index >= 0x2800U && index < 0x2C00U)) {
                    return PPUMemory[index + 0x0400U];
                }
            }
            else if (CurROM->Layout == NTL_Horizontal) {
                if ((index >= 0x2000U && index < 0x2400U) || (index >= 0x2400U && index < 0x2800U)) {
                    return PPUMemory[index + 0x0800U];
                }
            }
        }
    }
    else {
        if (index >= 0x2000U && index < 0x2400U) {
            if (PPUAXNTSelect == 1) {
                return PPUAXMem[index - 0x2000U];
            }
        }
        else if (index >= 0x2000U && index < 0x3F00) {
            if (PPUAXNTSelect == 1) {
                return PPUAXMem[(index - 0x2000U) % 0x0400U];
            }
        }
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

uint16_t GetBaseNameTableAddress() {
    uint8_t multValue = CheckBit(*CurPPU->PPUCTRL, PPUCTRL_BaseNameTableAddrLow) + (CheckBit(*CurPPU->PPUCTRL, PPUCTRL_BaseNameTableAddrHigh) * 2);

    uint16_t addr = 0x2000U + (0x0400U * multValue);
    return addr;
}

uint16_t GetBaseSPRPatternTableAddress() {
    uint16_t addr = CheckBit(*CurPPU->PPUCTRL, PPUCTRL_SPRPatternTableAddr);
    addr *= 0x1000;

    return addr;
}

uint16_t GetBaseBGPatternTableAddress() {
    uint16_t addr = CheckBit(*CurPPU->PPUCTRL, PPUCTRL_BGPatternTableAddr);
    addr *= 0x1000;

    return addr;
}

uint16_t GetAttribute(uint16_t addr) {
    const uint16_t base = (addr & 0xFC00) + 0x03C0;
    const uint16_t diff = addr - (addr & 0xFC00);
    const uint16_t cell = (diff / 128) * 8 + ((diff % 128) % 32 / 4);

    return PPURead(base + cell);
}

uint8_t GetAttributeTilePart(uint16_t addr) {
    const uint16_t diff = addr - (addr & 0xFC00);
    const uint8_t part = (diff / 2) % 2 + ((diff / 64) % 2) * 2;

    return part * 2;
}


void OnReadPPUSTATUS() {
    CurPPU->RegW = 0U;
    OverrideBit8(CurPPU->PPUSTATUS, PPUSTATUS_VBlank, 0);
    RunPPU(CPUTimeStamp);
}

void OnReadPPUDATA() {
    uint16_t addr = CurPPU->RegV;

    if (addr >= (uint16_t)PaletteRAMIndeces_Start) {
        addr -= 0x1000U;
    }

    CurPPU->DataReadBuffer = PPURead(addr);

    if (!CheckBit(*CurPPU->PPUCTRL, PPUCTRL_VRAMIncrement)) {
        PPUSetV(CurPPU->RegV + 1U);
    }
    else {
        PPUSetV(CurPPU->RegV + 32U);
    }

    RunPPU(CPUTimeStamp);
}


void OnWriteToPPUCTRL() {
    //printf("PPUCTRL: Wrote %02X. PC: %04X. Scanline: %u\n", *CurPPU->PPUCTRL, CCPU->PC - 3, CurScanline);

    OverrideBit16(&CurPPU->RegT, 10, CheckBit(*CurPPU->PPUCTRL, 0));
    OverrideBit16(&CurPPU->RegT, 11, CheckBit(*CurPPU->PPUCTRL, 1));
    RunPPU(CPUTimeStamp);
}

void OnWriteToPPUMASK() {
    BGRenderFlagCountdown = 3;
    PendingBGRenderFlag = CheckBit(*CurPPU->PPUMASK, 3);

    SPRRenderFlagCountdown = 3;
    PendingSPRRenderFlag = CheckBit(*CurPPU->PPUMASK, 4);
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
    
    RunPPU(CPUTimeStamp);
}

void OnWriteToPPUADDR() {
    uint16_t newValue;
    uint8_t highByte;
    uint8_t lowByte;

    //printf("PPUADDR: Wrote %02X. PC: %04X. Frame: %u, CPU cycle: %u\n", *CurPPU->PPUADDR, CCPU->PC - 3, FrameCount, CPUCycleCount);

    if (!CurPPU->RegW) { // Write high byte
        //printf("Writing high byte\n");
        highByte = *CurPPU->PPUADDR;
        highByte &= 0b00111111;
        lowByte = GetLowByte(CurPPU->RegT);

        newValue = AssembleAbsoluteAddress(lowByte, highByte);
        PPUSetT(newValue, 1);
        PPUSetW(1U);
    }
    else { // Write low byte
        //printf("Writing low byte\n");
        highByte = GetHighByte(CurPPU->RegT);
        lowByte = *CurPPU->PPUADDR;

        newValue = AssembleAbsoluteAddress(lowByte, highByte);
        PPUSetT(newValue, 0);
        PPUSetV(CurPPU->RegT);
        PPUSetW(0U);
    }

    //printf("High: %02X, Low: %02X, V: %04X\n", highByte, lowByte, CurPPU->RegV);

    RunPPU(CPUTimeStamp);
}

void OnWriteToPPUDATA() {
    PPUWrite(CurPPU->RegV, CurPPU->DataBus);

    if (!CheckBit(*CurPPU->PPUCTRL, PPUCTRL_VRAMIncrement)) {
        PPUSetV(CurPPU->RegV + 1U);
    }
    else {
        PPUSetV(CurPPU->RegV + 32U);
    }
    
    RunPPU(CPUTimeStamp);
}

void OnWriteToOAMDATA() { // 2 cycles
    CurPPU->OAM[*CurPPU->OAMADDR] = *CurPPU->OAMDATA;
    (*CurPPU->OAMADDR)++;
}

void DumpPPU() {
    if (!CurPPU) {
        printf("Error: PPU not found.");
        return;
    }

    FILE* dumpFile = fopen("ppudump.txt", "w");
    fprintf(dumpFile, "");
    fclose(dumpFile);
    
    dumpFile = fopen("ppudump.txt", "a");

    uint16_t offset = 0;
    size_t ptIterations = PatternTable_Size / 16;
    size_t ntIterations = NameTable_Size / 16;

    fprintf(dumpFile, "Patterntable 0\n");
    offset = PatternTable0_Start;
    for (size_t i = 0; i < ptIterations; i++) {
        DumpPPUWriteLine(dumpFile, offset + (uint16_t)i * 16);
    }

    fprintf(dumpFile, "\nPatterntable 1\n");
    offset = PatternTable1_Start;
    for (size_t i = 0; i < ptIterations; i++) {
        DumpPPUWriteLine(dumpFile, offset + (uint16_t)i * 16);
    }

    offset = NameTable0_Start;
    for (size_t i = 0; i < 4; i++) {
        fprintf(dumpFile, "\nName table %i\n", i);
        for (size_t i = 0; i < ntIterations; i++) {
            DumpPPUWriteLine(dumpFile, offset + (uint16_t)i * 16);
        }

        offset += NameTable_Size;
        fprintf(dumpFile, "\nAttribute table %i\n", i);
        for (size_t i = 0; i < 4; i++) {
            DumpPPUWriteLine(dumpFile, offset + (uint16_t)i * 16);
        }

        offset += AttributeTable_Size;
    }

    fprintf(dumpFile, "\nPalette\n");
    offset = PaletteRAMIndeces_Start;
    for (size_t i = 0; i < 2; i++) {
        DumpPPUWriteLine(dumpFile, offset + (uint16_t)i * 16);
    }

    fprintf(dumpFile, "\nOAM\n");
    DumpOAM(dumpFile);

    if (PPUAXMem) {
        DumpAXMem(dumpFile);
    }

    fclose(dumpFile);
}

void DumpPPUWriteLine(FILE* file, uint16_t startAddr) {
    if (startAddr > 0x3FFF) {
        startAddr = startAddr % 0x4000;
    }

    uint8_t bufitoa[16];
    fprintf(file, "%04s: ", itoa(startAddr, bufitoa, 16));

    for (size_t i = 0; i < 16; i++) {
        if (i == 15) {
            fprintf(file, "%02hhX", PPUMemory[startAddr + i]);
        }
        else {
            fprintf(file, "%02hhX ", PPUMemory[startAddr + i]);
        }
    }

    fprintf(file, "\n");
}

void DumpOAM(FILE* file) {
    uint8_t bufitoa[16];
    uint8_t addr = 0x00;

    for (size_t i = 0; i < 16; i++) {
        fprintf(file, "%04s: ", itoa(addr, bufitoa, 16));

        for (size_t j = 0; j < 16; j++) {
            if (j == 15) {
                fprintf(file, "%02hhX", CurPPU->OAM[addr + j]);
            }
            else {
                fprintf(file, "%02hhX ", CurPPU->OAM[addr + j]);
            }
        }

        fprintf(file, "\n");
        addr += 0x10;
    }
    
    fprintf(file, "\n");
}

void DumpAXMem(FILE* file) {
    uint8_t bufitoa[16];
    uint8_t addr = 0x00;

    for (size_t i = 0; i < 256; i++) {
        fprintf(file, "%04s: ", itoa(addr, bufitoa, 16));

        for (size_t j = 0; j < 16; j++) {
            if (j == 15) {
                fprintf(file, "%02hhX", PPUAXMem[addr + j]);
            }
            else {
                fprintf(file, "%02hhX ", PPUAXMem[addr + j]);
            }
        }

        fprintf(file, "\n");
        addr += 0x10;
    }

    fprintf(file, "\n");
    addr += 0x10;
}
