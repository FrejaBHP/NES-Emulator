#include <ppu.h>
#include <cpu.h>
#include <stdlib.h>

PPU* CurPPU = NULL;
uint8_t* PPUMemory = NULL;

void PPUInit() {
    CurPPU = malloc(sizeof(PPU));
    PPUMemory = calloc(1, 0x3FFF + 1);

    CurPPU->PPUCTRL = &CPUMemory[PPU_PPUCTRL];
    CurPPU->PPUMASK = &CPUMemory[PPU_PPUMASK];
    CurPPU->PPUSTATUS = &CPUMemory[PPU_PPUSTATUS];
    CurPPU->OAMADDR = &CPUMemory[PPU_OAMADDR];
    CurPPU->OAMDATA = &CPUMemory[PPU_OAMDATA];
    CurPPU->PPUSCROLL = &CPUMemory[PPU_PPUSCROLL];
    CurPPU->PPUADDR = &CPUMemory[PPU_PPUADDR];
    CurPPU->PPUDATA = &CPUMemory[PPU_PPUDATA];
}

void WriteToOAM() { // 2 cycles
    CurPPU->OAM[*CurPPU->OAMADDR] = *CurPPU->OAMDATA;
}
