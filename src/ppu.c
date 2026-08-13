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

uint32_t Palette_NTSC[64] = {
    0x7C7C7C, 0x0000FC, 0x0000BC, 0x4428BC, 0x940084, 0xA80020, 0xA81000, 0x881400, 0x503000, 0x007800, 0x006800, 0x005800, 0x004058, 0x000000, 0x000000, 0x000000,
    0xBCBCBC, 0x0078F8, 0x0058F8, 0x6844FC, 0xD800CC, 0xE40058, 0xF83800, 0xE45C10, 0xAC7C00, 0x00B800, 0x00A800, 0x00A844, 0x008888, 0x000000, 0x000000, 0x000000,
    0xF8F8F8, 0x3CBCFC, 0x6888FC, 0x9878F8, 0xF878F8, 0xF85898, 0xF87858, 0xFCA044, 0xF8B800, 0xB8F818, 0x58D854, 0x58F898, 0x00E8D8, 0x787878, 0x000000, 0x000000,
    0xFCFCFC, 0xA4E4FC, 0xB8B8F8, 0xD8B8F8, 0xF8B8F8, 0xF8A4C0, 0xF0D0B0, 0xFCE0A8, 0xF8D878, 0xD8F878, 0xB8F8B8, 0xB8F8D8, 0x00FCFC, 0xF8D8F8, 0x000000, 0x000000
};

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

void PPUWrite(uint16_t index, uint8_t value) {
    if (index > 0x3FFFU) {
        StopExecution = 1;
        //index = index % 0x4000U;
        return;
    }
    else if (index < 0x2000U) {
        //StopExecution = 1;
        return;
    }

    PPUMemory[index] = value;

    if (!CurROM->HasAltNTL) {
        if (CurROM->Layout == NTL_Vertical) {
            if ((index >= 0x2000U && index < 0x23FFU) || (index >= 0x2800U && index < 0x2BFFU)) {
                PPUMemory[index + 0x0400U] = value;
            }
        }
        else if (CurROM->Layout == NTL_Horizontal) {
            if ((index >= 0x2000U && index < 0x23FFU) || (index >= 0x2400U && index < 0x27FFU)) {
                PPUMemory[index + 0x0800U] = value;
            }
        }
    }
}

uint8_t PPURead(uint16_t index) {
    // Safety precaution
    if (index > 0x3FFFU) {
        index = index % 0x4000U;
        StopExecution = 1;
    }
    else if (index >= 0x3F20U) {
        index = (index % (uint16_t)PaletteRAMIndeces_Size) + (uint16_t)PaletteRAMIndeces_Start;
    }
    else if (index >= (uint16_t)UnusedSection_Start && index < (uint16_t)PaletteRAMIndeces_Start) {
        index -= 0x1000;
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


void OnReadPPUSTATUS() {
    CurPPU->RegW = 0U;
    OverrideBit8(CurPPU->PPUSTATUS, PPUSTATUS_VBlank, 0);
    RunPPU(CPUTimeStamp);
}

void OnReadPPUDATA() {
    if (!CheckBit(*CurPPU->PPUCTRL, PPUCTRL_VRAMIncrement)) {
        PPUSetV(CurPPU->RegV + 1U);
    }
    else {
        PPUSetV(CurPPU->RegV + 32U);
    }

    RunPPU(CPUTimeStamp);
}


void OnWriteToPPUCTRL() {
    //printf("PPUCTRL: Wrote %02X. PC: %04X. Frame: %u, CPU cycle: %u\n", *CurPPU->PPUCTRL, CCPU->PC - 3, FrameCount, CPUCycleCount);

    OverrideBit16(&CurPPU->RegT, 10, CheckBit(*CurPPU->PPUCTRL, 0));
    OverrideBit16(&CurPPU->RegT, 11, CheckBit(*CurPPU->PPUCTRL, 1));
    RunPPU(CPUTimeStamp);
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

    //printf("High: %02X, Low: %02X, New: %04X\n", highByte, lowByte, newValue);

    RunPPU(CPUTimeStamp);
}

void OnWriteToPPUDATA() {
    PPUWrite(CurPPU->RegV, *CurPPU->PPUDATA);
    //printf("PPUDATA: Wrote %02X to %04X. PC: %04X. Frame: %u, CPU cycle: %u\n", *CurPPU->PPUDATA, CurPPU->RegV, CCPU->PC - 3, FrameCount, CPUCycleCount);

    /*
    if ((CCPU->PC - 3) == 0xF216) {
        printf("PPUDATA: Wrote %02X to %04X. PC: %04X. Frame: %u, CPU cycle: %u\n", *CurPPU->PPUDATA, CurPPU->RegV, CCPU->PC - 3, FrameCount, CPUCycleCount);
        //printf("A: %02X, X: %02X, Y: %02X, Status: %02X\n", CCPU->Accumulator, CCPU->RegX, CCPU->RegY, CCPU->Status);
    }
    */

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
                fprintf(file, "%02hhX ", PPUMemory[addr + j]);
            }
        }

        fprintf(file, "\n");
        addr += 0x10;
    }
    
    fprintf(file, "\n");
}
