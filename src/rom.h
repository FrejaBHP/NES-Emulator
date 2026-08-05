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

typedef struct ROMData {
    ConsoleType ConsoleType;
    NametableLayout Layout;
    DefaultController DefController;
    TimingMode TimingMode;
    uint8_t MapperNumber;
    bool IsINES;
    bool IsNES2;
    bool HasBattery;
    bool HasAltNTL;

    uint16_t Trainer_Size;
    uint16_t PRG_ROM_Size;
    uint16_t CHR_ROM_Size;
} ROMData;

extern ROMData* CurROM;

#endif
