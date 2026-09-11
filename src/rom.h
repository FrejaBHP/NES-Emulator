#ifndef def_ROM
#define def_ROM

#include <stdint.h>
#include <stdbool.h>

typedef enum ConsoleType {
    CT_NES,
    CT_VS,
    CT_Playchoice,
    CT_Extended
} ConsoleType;

typedef enum NametableLayout {
    NTL_Vertical,
    NTL_Horizontal
} NametableLayout;

typedef enum DefaultController {
    DC_Unspecified,
    DC_NESController
} DefaultController;

typedef enum TimingMode {
    TMode_RP2C02,       // NTSC NES
    TMode_RP2C07,       // Licensed PAL NES
    TMode_Multiple,
    TMode_UA6538        // Dendy (??)
} TimingMode;

typedef enum MapperName {
    Map_NROM = 0,
    Map_MMC3 = 4,
    Map_AxROM = 7
} MapperName;

typedef struct ROMData {
    ConsoleType ConsoleType;
    NametableLayout Layout;
    DefaultController DefController;
    TimingMode TimingMode;
    uint16_t MapperNumber;
    bool IsINES;
    bool IsNES2;
    bool HasBattery;
    bool HasAltNTL;

    uint16_t Trainer_Size;
    uint32_t PRG_ROM_Size;
    uint32_t CHR_ROM_Size;
    uint32_t CHR_RAM_Size;
} ROMData;

extern ROMData* CurROM;
extern uint8_t* ROM_PRG;
extern uint8_t* ROM_CHR;

#endif
