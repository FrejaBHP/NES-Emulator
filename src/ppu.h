#ifndef def_PPU
#define def_PPU

#include <stdint.h>
#include <stdio.h>


// === Mapped into PPU addressing ===

#define PatternTable_Size           0x1000U
#define NameTable_Size              0x03C0U
#define AttributeTable_Size         0x0040U
#define PaletteRAMIndeces_Size      0x0020U
#define UnusedSection_Size          0x0F00U

// Usually mapped to Character RAM and ROM, bank switching?
#define PatternTable0_Start         0x0000U
#define PatternTable1_Start         0x1000U

// Usually mapped to internal VRAM, but can be remapped to another region for RAM/ROM
#define NameTable0_Start            0x2000U
#define AttributeTable0_Start       0x23C0U
#define NameTable1_Start            0x2400U
#define AttributeTable1_Start       0x27C0U
#define NameTable2_Start            0x2800U
#define AttributeTable2_Start       0x2BC0U
#define NameTable3_Start            0x2C00U
#define AttributeTable3_Start       0x2FC0U

// 0x3000-0x3EFF usually mirrors 0x2000-0x2EFF. PPU does not render this range
#define UnusedSection_Start         0x3000U

// Mirrored until 0x3FFF
#define PaletteRAMIndeces_Start     0x3F00U


typedef struct PPU {
    uint8_t* PPUCTRL;
    uint8_t* PPUMASK;
    uint8_t* PPUSTATUS;
    uint8_t* OAMADDR;
    uint8_t* OAMDATA;
    uint8_t* PPUSCROLL;
    uint8_t* PPUADDR;
    uint8_t* PPUDATA;
    uint8_t OAM[256];
} PPU;

typedef struct SpriteData {
    uint8_t PositionX;
    uint8_t TileIndex;
    uint8_t Attributes;
    uint8_t PositionY;
} SpriteData;

extern PPU* CurPPU;
extern uint8_t* PPUMemory;

void PPUInit();

void WriteToOAM();
//void OAMDMA();

#endif
