#include <emulator.h>
#include <rom.h>
#include <string.h>
#include <stdlib.h>

FILE* file = NULL;

void TestFnc() {
    Initialisation();
    //LoadFile();
    LoadROM();
}

void Initialisation() {
    CPUInit();
    PPUInit();
}

void LoadFile() {
    uint8_t testbuffer[8] = { 0 };

    file = fopen("testhex.bin", "rb");
    fread(testbuffer, 1, sizeof(testbuffer), file);

    memcpy(&CPUMemory[ROM_Start], testbuffer, sizeof(testbuffer));

    uint8_t buffer[32];

    for (size_t i = 0; i < 8; i++) {
        sprintf(buffer, "%X\n", testbuffer[i]);
        printf(buffer);
    }

    fclose(file);

    RunTestProgram();
}

void LoadROM() {
    uint8_t headerBuffer[16] = { 0 };

    file = fopen("01-implied.nes", "rb");
    fread(headerBuffer, 1, sizeof(headerBuffer), file);

    if (!CurROM) {
        CurROM = malloc(sizeof(ROMData));
    }

    ParseHeader(headerBuffer);

    if (CurROM->IsINES) {
        if (CurROM->PRG_ROM_Size == 0x4000U) {
            fread(&CPUMemory[ROM_Start + 0x4000U], 1, 0x4000U, file);
        }
        else if (CurROM->PRG_ROM_Size == 0x8000U) {
            fread(&CPUMemory[ROM_Start], 1, 0x8000U, file);
        }

        CCPU->PC = AssembleAbsoluteAddress(CPUMemory[0xFFFC], CPUMemory[0xFFFD]);
    }

    fclose(file);

    RunTestProgram();
}

void ParseHeader(uint8_t* header) {
    bool isINES = false;
    if (header[0] == 0x4EU && header[1] == 0x45U && header[2] == 0x53U && header[3] == 0x1AU) {
        isINES = true;
    }

    CurROM->IsINES = isINES;

    if (CurROM->IsINES) {
        CurROM->Layout = CheckBit(header[6], 0);
        CurROM->ConsoleType = (header[7] << 6) >> 6;
        CurROM->IsNES2 = CheckBit(header[7], 3);
        CurROM->TimingMode = header[12];

        CurROM->PRG_ROM_Size = header[4] * 0x4000U;
        CurROM->CHR_ROM_Size = header[5] * 0x2000U;

        // 12 bits for mapper number, but only 255 valid mappers??
        //uint16_t mapperNumber = (header[6] >> 4) | ((header[7] >> 4) << 4) | ((header[8] >> 4) << 8);
        CurROM->MapperNumber = header[6] >> 4;

        CurROM->DefController = header[15];
    }
}
