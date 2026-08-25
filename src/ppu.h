#ifndef def_PPU
#define def_PPU

#include <stdint.h>
#include <stdio.h>


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

#define SPRAttrPos_Priority				 5U
#define SPRAttrPos_FlipH				 6U
#define SPRAttrPos_FlipV				 7U


typedef struct PPU {
    uint16_t RegV;  // Current VRAM address. 15 bits - use setter for this register
    uint16_t RegT;  // Temp. VRAM address. 15 bits - use setter for this register
    uint8_t RegX;   // Fine X scroll. 3 bits - use setter for this register
    uint8_t RegW;   // Write latch, to distinguish first or second write. 1 bit - unless flipping, use setter for this register
    uint8_t DataBus;
    uint8_t DataReadBuffer;

    uint8_t* PPUCTRL;
    uint8_t* PPUMASK;
    uint8_t* PPUSTATUS;
    uint8_t* OAMADDR;
    uint8_t* OAMDATA;
    uint8_t* PPUSCROLL;
    uint8_t* PPUADDR;
    uint8_t* PPUDATA;
    uint8_t OAM[256];
    uint8_t SecOAM[32];
} PPU;

/* Register values

PPUCTRL
7  bit  0
---- ----
VPHB SINN
|||| ||||
|||| ||++- Base nametable address
|||| ||    (0 = $2000; 1 = $2400; 2 = $2800; 3 = $2C00)
|||| |+--- VRAM address increment per CPU read/write of PPUDATA
|||| |     (0: add 1, going across; 1: add 32, going down)
|||| +---- Sprite pattern table address for 8x8 sprites
||||       (0: $0000; 1: $1000; ignored in 8x16 mode)
|||+------ Background pattern table address (0: $0000; 1: $1000)
||+------- Sprite size (0: 8x8 pixels; 1: 8x16 pixels)
|+-------- PPU master/slave select
|          (0: read backdrop from EXT pins; 1: output color on EXT pins)
+--------- Vblank NMI enable (0: off, 1: on)


PPUMASK
7  bit  0
---- ----
BGRs bMmG
|||| ||||
|||| |||+- Greyscale (0: normal colour, 1: greyscale)
|||| ||+-- 1: Show background in leftmost 8 pixels of screen, 0: Hide
|||| |+--- 1: Show sprites in leftmost 8 pixels of screen, 0: Hide
|||| +---- 1: Enable background rendering
|||+------ 1: Enable sprite rendering
||+------- Emphasise red (green on PAL/Dendy)
|+-------- Emphasise green (red on PAL/Dendy)
+--------- Emphasise blue


PPUSTATUS
7  bit  0
---- ----
VSOx xxxx
|||| ||||
|||+-++++- (PPU open bus or 2C05 PPU identifier)
||+------- Sprite overflow flag
|+-------- Sprite 0 hit flag
+--------- Vblank flag, cleared on read


OAMADDR
7  bit  0
---- ----
AAAA AAAA
|||| ||||
++++-++++- OAM address


OAMDATA
7  bit  0
---- ----
DDDD DDDD
|||| ||||
++++-++++- OAM data


PPUSCROLL
1st write
7  bit  0
---- ----
XXXX XXXX
|||| ||||
++++-++++- X scroll bits 7-0 (bit 8 in PPUCTRL bit 0)

2nd write
7  bit  0
---- ----
YYYY YYYY
|||| ||||
++++-++++- Y scroll bits 7-0 (bit 8 in PPUCTRL bit 1)


PPUADDR
1st write  2nd write
15 bit  8  7  bit  0
---- ----  ---- ----
..AA AAAA  AAAA AAAA
  || ||||  |||| ||||
  ++-++++--++++-++++- VRAM address


PPUDATA
7  bit  0
---- ----
DDDD DDDD
|||| ||||
++++-++++- VRAM data
*/

extern PPU* CurPPU;
extern uint8_t* PPUMemory;

extern uint32_t Palette_NTSC[64];
extern uint32_t Palette_NTSC_old[64];

void PPUSetV(uint16_t value);
void PPUSetT(uint16_t value, uint8_t clearBit);
void PPUSetX(uint8_t value);
void PPUSetW(uint8_t value);

void PPUWrite(uint16_t index, uint8_t value);
uint8_t PPURead(uint16_t index);
uint8_t* PPUGetAddr(uint16_t index);

void PPUInit();

uint16_t GetBaseSPRPatternTableAddress();
uint16_t GetBaseBGPatternTableAddress();
uint16_t GetBaseNameTableAddress();
uint16_t GetAttribute(uint16_t addr);
uint8_t GetAttributeTilePart(uint16_t addr);

void OnReadPPUSTATUS();
void OnReadPPUDATA();

void OnWriteToPPUCTRL();
void OnWriteToPPUSCROLL();
void OnWriteToPPUADDR();
void OnWriteToPPUDATA();

void OnWriteToOAMDATA();

void DumpPPU();
void DumpPPUWriteLine(FILE* file, uint16_t startAddr);
void DumpOAM(FILE* file);

#endif
