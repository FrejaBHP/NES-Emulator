#include <cpu.h>
#include <ppu.h>
#include <rom.h>
#include <stdlib.h>

CPU* CCPU = NULL;
uint8_t* CPUMemory = NULL;

void CPUInit() {
    CCPU = calloc(1, sizeof(CPU));
    //CPUMemory = calloc(1, 0xFFFF);
    CPUMemory = calloc(1, 0xFFFF + 1);

    CCPU->SP = 0xFFU;
    CCPU->PC = ROM_Start; // temp
}

void RunTestProgram() {
    uint8_t buffer[256];
    bool alternate = false;

    uint8_t recentOpcode[2];
    uint8_t recentCount[2];

    while(1) {
        uint8_t instruction = ReadInstruction();

        sprintf(buffer, "Addr: %04X, Instruction: %02X.    Next two bytes: %02X, %02X\n", CCPU->PC - 1, instruction, CPUMemory[CCPU->PC], CPUMemory[CCPU->PC + 1]);
        printf(buffer);

        if (recentOpcode[alternate] == instruction) {
            recentCount[alternate] += 1;
        }
        else {
            recentOpcode[alternate] = instruction;
            recentCount[alternate] = 0;
        }

        if (instruction == 0) {
            break;
        }

        if (recentCount[0] > 4 && recentCount[1] > 4) {
            printf("Stuck looping between two instructions, breaking...\n");
            break;
        }
        
        ExecuteInstruction(instruction);

        alternate = !alternate;
    }

    sprintf(buffer, "Acc: %u, X: %u, Y: %u, SP: 0x%02X, PC: 0x%04X\n", CCPU->Accumulator, CCPU->RegX, CCPU->RegY, CCPU->SP, CCPU->PC);
    printf(buffer);

    DumpMemory();
}

void DumpMemory() {
    if (!CCPU) {
        printf("Error: CPU not found.");
        return;
    }

    FILE* dumpFile = fopen("memdump.txt", "w");
    fprintf(dumpFile, "");
    fclose(dumpFile);
    
    dumpFile = fopen("memdump.txt", "a");
    
    fprintf(dumpFile, "Zero Page\n");
    uint16_t offset = 0U;
    for (size_t i = 0; i < 16; i++) {
        DumpWriteLine(dumpFile, (uint16_t)i * 16);
    }

    fprintf(dumpFile, "\nStack\n");
    offset = 256U;
    for (size_t i = 0; i < 16; i++) {
        DumpWriteLine(dumpFile, offset + (uint16_t)i * 16);
    }

    fprintf(dumpFile, "\nRest of RAM\n");
    offset = 512U;
    for (size_t i = 0; i < 96; i++) {
        DumpWriteLine(dumpFile, offset + (uint16_t)i * 16);
    }

    fprintf(dumpFile, "\nPPU\n");
    offset = 0x2000U;
    DumpWriteLineHalf(dumpFile, offset);

    fprintf(dumpFile, "\nTest write region?\n");
    offset = 0x6000U;
    for (size_t i = 0; i < 64; i++) {
        DumpWriteLine(dumpFile, offset + (uint16_t)i * 16);
    }
    
    fclose(dumpFile);
}

void DumpWriteLine(FILE* file, uint16_t startAddr) {
    uint8_t bufitoa[16];
    fprintf(file, "%04s: ", itoa(startAddr, bufitoa, 16));

    for (size_t i = 0; i < 16; i++) {
        if (i == 15) {
            fprintf(file, "%02hhX", CPUMemory[startAddr + i]);
        }
        else {
            fprintf(file, "%02hhX ", CPUMemory[startAddr + i]);
        }
    }

    fprintf(file, "\n");
}

void DumpWriteLineHalf(FILE* file, uint16_t startAddr) {
    uint8_t bufitoa[16];
    fprintf(file, "%04s: ", itoa(startAddr, bufitoa, 16));

    for (size_t i = 0; i < 8; i++) {
        if (i == 7) {
            fprintf(file, "%02hhX", CPUMemory[startAddr + i]);
        }
        else {
            fprintf(file, "%02hhX ", CPUMemory[startAddr + i]);
        }
    }

    fprintf(file, "\n");
}


uint8_t ReadProgramByte() {
    uint8_t value = CPUMemory[CCPU->PC];
    CCPU->PC++;
    return value;
}

uint8_t ReadInstruction() {
    uint8_t instruction = CPUMemory[CCPU->PC];
    CCPU->PC++;
    return instruction;
}


uint16_t AssembleAbsoluteAddress(uint8_t first, uint8_t second) {
    // Reverse order because the system uses little endian, so LLHH in ML, but HHLL in ASM
    uint16_t addr = (second << 8) | first;
    return addr;
}

uint8_t GetHighByte(uint16_t value) {
    uint16_t temp = value;
    temp = temp >> 8;
    uint8_t highByte = (uint8_t)temp;

    return highByte;
}

uint8_t GetLowByte(uint16_t value) {
    uint8_t lowByte = (uint8_t)value;
    
    return lowByte;
}


uint8_t FlipByteSign(uint8_t value) {
    uint8_t newByte = (value ^ 0xFF) + 1;
    return newByte;
}


void PushByte(uint8_t value) {
    uint16_t index = Stack_Start + CCPU->SP;
    CPUMemory[index] = value;

    //printf("Pushed %02hhX to %04hX\n", value, index);

    CCPU->SP--;
}

uint8_t PopByte() {
    CCPU->SP++;
    uint16_t index = Stack_Start + CCPU->SP;
    uint8_t value = CPUMemory[index];

    //printf("Popped %02hhX from %04hX\n", value, index);

    return value;
}

void BitwiseAccInstruction(AddrMode am, BitwiseOp op) {
    uint8_t value = ReadProgramByte();
    uint8_t number;

    switch (am) {
        case AM_Immediate: // 2 cycles
            number = value;
            break;

        case AM_ZeroPage: // 3 cycles
            number = GetZeroPage(value);
            break;

        case AM_ZeroPageX: // 4 cycles
            number = GetZeroPageX(value);
            break;

        case AM_Absolute: { // 4 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsolute(index);
            break;
        }

        case AM_AbsoluteX: { // 4 cycles (5 if crossing pages)
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsoluteX(index);
            break;
        }

        case AM_AbsoluteY: { // 4 cycles (5 if crossing pages)
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsoluteY(index);
            break;
        }

        case AM_IndirectX: { // 6 cycles
            uint16_t index = GetIndirectZPAddrX(value);
            number = GetAbsolute(index);
            break;
        }

        case AM_IndirectY: { // 5 cycles (6 if page crossing)
            uint16_t index = GetIndirectZPAddrY(value);
            number = GetAbsolute(index);
            break;
        }
        
        default:
            break;
    }

    if (op == BOP_AND) {
        CCPU->Accumulator = CCPU->Accumulator & number;
    }
    else if (op == BOP_OR) {
        CCPU->Accumulator = CCPU->Accumulator | number;
    }
    else if (op == BOP_XOR) {
        CCPU->Accumulator = CCPU->Accumulator ^ number;
    }

    CheckSetSFZero(CCPU->Accumulator);
    CheckSetSFNegative(CCPU->Accumulator);
}

uint8_t SetBit(uint8_t value, uint8_t bit) {
    return value | ((uint8_t)1 << bit);
}

uint8_t ClearBit(uint8_t value, uint8_t bit) {
    return value & ~((uint8_t)1 << bit);
}

uint8_t CheckBit(uint8_t value, uint8_t bit) {
    return ((value >> bit) & (uint8_t)1);
}


uint8_t GetZeroPage(uint8_t index) {
    return CPUMemory[index];
}

uint8_t GetZeroPageX(uint8_t index) {
    index += CCPU->RegX;
    return CPUMemory[index];
}

uint8_t GetZeroPageY(uint8_t index) {
    index += CCPU->RegY;
    return CPUMemory[index];
}

void StoreZeroPage(uint8_t index, const uint8_t value) {
    //printf("Storing to Zero Page, i%02X v%02X\n", index, value);
    CPUMemory[index] = value;
    //printf("Zero Page i%02X = %02X\n", index, Memory[index]);
}

void StoreZeroPageX(uint8_t index, const uint8_t value) {
    index += CCPU->RegX;
    StoreZeroPage(index, value);
    //Memory[index] = value;
}

void StoreZeroPageY(uint8_t index, const uint8_t value) {
    index += CCPU->RegY;
    StoreZeroPage(index, value);
    //Memory[index] = value;
}


uint8_t GetAbsolute(uint16_t index) {
    // 2KiB internal RAM mirrors 3 times from 0x800 to 0x1FFF
    if ((uint16_t)PPU_Start > index && index > (uint16_t)InternalRAM_Size) {
        index = index % (uint16_t)InternalRAM_Size;
    }
    // PPU register mirrors... a lot... from 0x2007 to 0x3FFF
    else if ((uint16_t)APU_Start > index && index > (uint16_t)PPU_Start) {
        index = (index % (uint16_t)PPU_Size) + (uint16_t)PPU_Start;
    }

    return CPUMemory[index];
}

uint8_t GetAbsoluteX(uint16_t index) {
    index += CCPU->RegX;
    return GetAbsolute(index);
}

uint8_t GetAbsoluteY(uint16_t index) {
    index += CCPU->RegY;
    return GetAbsolute(index);
}

void StoreAbsolute(uint16_t index, const uint8_t value) {
    //printf("Storing to Absolute memory, i%04X v%02X\n", index, value);

    // 2KiB internal RAM mirrors 3 times from 0x800 to 0x1FFF
    if ((uint16_t)PPU_Start > index && index > (uint16_t)InternalRAM_Size) {
        index = index % (uint16_t)InternalRAM_Size;
    }
    // PPU register mirrors... a lot... from 0x2007 to 0x3FFF
    else if ((uint16_t)APU_Start > index && index > (uint16_t)PPU_Start) {
        index = (index % (uint16_t)PPU_Size) + (uint16_t)PPU_Start;
    }

    switch (index) {
        case PPU_OAMDATA:
            CPUMemory[index] = value;
            OnWriteToOAMDATA();
            break;

        case OAMDMA:
            CPUMemory[index] = value;
            OnWriteToOAMDMA();
            break;
        
        default:
            CPUMemory[index] = value;
            break;
    }

    //printf("Absolute Memory i%04X = %02X\n", index, Memory[index]);
}

void StoreAbsoluteX(uint16_t index, const uint8_t value) {
    index += CCPU->RegX;
    StoreAbsolute(index, value);
}

void StoreAbsoluteY(uint16_t index, const uint8_t value) {
    index += CCPU->RegY;
    StoreAbsolute(index, value);
}


uint16_t GetIndirectAddr(uint16_t lookupAddr) {
    const uint8_t first = CPUMemory[lookupAddr];
    const uint8_t second = CPUMemory[++lookupAddr];

    return AssembleAbsoluteAddress(first, second);
}

uint16_t GetIndirectZPAddrX(uint8_t zpAddr) {
    uint8_t index = zpAddr + CCPU->RegX;
    const uint8_t first = CPUMemory[index];
    const uint8_t second = CPUMemory[++index];
    
    return AssembleAbsoluteAddress(first, second);
}

uint16_t GetIndirectZPAddrY(uint8_t zpAddr) {
    const uint8_t first = CPUMemory[zpAddr];
    const uint8_t second = CPUMemory[++zpAddr];

    const uint16_t newAddr = AssembleAbsoluteAddress(first, second) + CCPU->RegY;
    
    return newAddr;
}


void OnWriteToOAMDATA() {
    WriteToOAM();

    uint8_t* oamAddr = &CPUMemory[PPU_OAMADDR];
    *oamAddr++;
}

void OnWriteToOAMDMA() {
    const uint8_t highbyte = CPUMemory[OAMDMA];
    const uint16_t startIndex = AssembleAbsoluteAddress(0U, highbyte);

    uint8_t* addr = &CPUMemory[startIndex];

    uint8_t* oamAddr = &CPUMemory[PPU_OAMADDR];
    uint8_t* oamData = &CPUMemory[PPU_OAMDATA];

    for (size_t i = 0; i < 256; i++) { // 2 cycles per loop
        *oamData = *addr;

        WriteToOAM();

        addr++;
        *oamAddr++;
    }
}


void CheckSetSFZero(uint8_t value) {
    if (value == 0) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Zero);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Zero);
    }
}

void CheckSetSFNegative(uint8_t value) {
    if (CheckBit(value, 7)) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Negative);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Negative);
    }
}

void ExecuteInstruction(uint8_t opcode) {
    switch (opcode) {
        case 0x00:
            BRK(); // Always implied
            break;

        case 0x01:
            ORA(AM_IndirectX);
            break;

        case 0x05:
            ORA(AM_ZeroPage);
            break;

        case 0x06:
            ASL(AM_ZeroPage);
            break;

        case 0x08:
            PHP(); // Always implied
            break;

        case 0x09:
            ORA(AM_Immediate);
            break;

        case 0x0A:
            ASL(AM_Accumulator);
            break;

        case 0x0D:
            ORA(AM_Absolute);
            break;

        case 0x0E:
            ASL(AM_Absolute);
            break;

        case 0x10:
            BPL(); // Always relative
            break;

        case 0x11:
            ORA(AM_IndirectY);
            break;

        case 0x15:
            ORA(AM_ZeroPageX);
            break;

        case 0x16:
            ASL(AM_ZeroPageX);
            break;

        case 0x18:
            CLC(); // Always implied
            break;

        case 0x19:
            ORA(AM_AbsoluteY);
            break;

        case 0x1D:
            ORA(AM_AbsoluteX);
            break;

        case 0x1E:
            ASL(AM_AbsoluteX);
            break;

        case 0x20:
            JSR(); // Always absolute
            break;

        case 0x21:
            AND(AM_IndirectX);
            break;

        case 0x24:
            BIT(AM_ZeroPage);
            break;

        case 0x25:
            AND(AM_ZeroPage);
            break;

        case 0x26:
            ROL(AM_ZeroPage);
            break;

        case 0x28:
            PLP(); // Always implied
            break;

        case 0x29:
            AND(AM_Immediate);
            break;

        case 0x2A:
            ROL(AM_Accumulator);
            break;

        case 0x2C:
            BIT(AM_Absolute);
            break;

        case 0x2D:
            AND(AM_Absolute);
            break;

        case 0x2E:
            ROL(AM_Absolute);
            break;

        case 0x30:
            BMI(); // Always relative
            break;

        case 0x31:
            AND(AM_IndirectY);
            break;

        case 0x35:
            AND(AM_ZeroPageX);
            break;

        case 0x36:
            ROL(AM_ZeroPageX);
            break;

        case 0x38:
            SEC(); // Always implied
            break;

        case 0x39:
            AND(AM_AbsoluteY);
            break;

        case 0x3D:
            AND(AM_AbsoluteX);
            break;

        case 0x3E:
            ROL(AM_AbsoluteX);
            break;

        case 0x40:
            RTI(); // Always implied
            break;

        case 0x41:
            EOR(AM_IndirectX);
            break;

        case 0x45:
            EOR(AM_ZeroPage);
            break;

        case 0x46:
            LSR(AM_ZeroPage);
            break;

        case 0x48:
            PHA(); // Always implied
            break;

        case 0x49:
            EOR(AM_Immediate);
            break;

        case 0x4A:
            LSR(AM_Accumulator);
            break;

        case 0x4C:
            JMP(AM_Absolute);
            break;

        case 0x4D:
            EOR(AM_Absolute);
            break;

        case 0x4E:
            LSR(AM_Absolute);
            break;

        case 0x50:
            BVC(); // Always relative
            break;

        case 0x51:
            EOR(AM_IndirectY);
            break;

        case 0x55:
            EOR(AM_ZeroPageX);
            break;

        case 0x56:
            LSR(AM_ZeroPageX);
            break;

        case 0x58:
            CLI(); // Always implied
            break;

        case 0x59:
            EOR(AM_AbsoluteY);
            break;

        case 0x5D:
            EOR(AM_AbsoluteX);
            break;

        case 0x5E:
            LSR(AM_AbsoluteX);
            break;

        case 0x60:
            RTS(); // Always implied
            break;

        case 0x61:
            ADC(AM_IndirectX);
            break;

        case 0x65:
            ADC(AM_ZeroPage);
            break;

        case 0x66:
            ROR(AM_ZeroPage);
            break;

        case 0x68:
            PLA(); // Always implied
            break;

        case 0x69:
            ADC(AM_Immediate);
            break;

        case 0x6A:
            ROR(AM_Accumulator);
            break;

        case 0x6C:
            JMP(AM_Indirect);
            break;

        case 0x6D:
            ADC(AM_Absolute);
            break;

        case 0x6E:
            ROR(AM_Absolute);
            break;

        case 0x70:
            BVS(); // Always relative
            break;

        case 0x71:
            ADC(AM_IndirectY);
            break;

        case 0x75:
            ADC(AM_ZeroPageX);
            break;

        case 0x76:
            ROR(AM_ZeroPageX);
            break;

        case 0x78:
            SEI(); // Always implied
            break;

        case 0x79:
            ADC(AM_AbsoluteY);
            break;

        case 0x7D:
            ADC(AM_AbsoluteX);
            break;

        case 0x7E:
            ROR(AM_AbsoluteX);
            break;

        case 0x81:
            STA(AM_IndirectX);
            break;

        case 0x84:
            STY(AM_ZeroPage);
            break;

        case 0x85:
            STA(AM_ZeroPage);
            break;

        case 0x86:
            STX(AM_ZeroPage);
            break;

        case 0x88:
            DEY(); // Always implied
            break;

        case 0x8A:
            TXA(); // Always implied
            break;

        case 0x8C:
            STY(AM_Absolute);
            break;

        case 0x8D:
            STA(AM_Absolute);
            break;

        case 0x8E:
            STX(AM_Absolute);
            break;

        case 0x90:
            BCC(); // Always relative
            break;

        case 0x91:
            STA(AM_IndirectY);
            break;

        case 0x94:
            STY(AM_ZeroPageX);
            break;

        case 0x95:
            STA(AM_ZeroPageX);
            break;

        case 0x96:
            STX(AM_ZeroPageY);
            break;

        case 0x98:
            TYA(); // Always implied
            break;

        case 0x99:
            STA(AM_AbsoluteY);
            break;

        case 0x9A:
            TXS(); // Always implied
            break;

        case 0x9D:
            STA(AM_AbsoluteX);
            break;

        case 0xA0:
            LDY(AM_Immediate);
            break;

        case 0xA1:
            LDA(AM_IndirectX);
            break;

        case 0xA2:
            LDX(AM_Immediate);
            break;

        case 0xA4:
            LDY(AM_ZeroPage);
            break;

        case 0xA5:
            LDA(AM_ZeroPage);
            break;

        case 0xA6:
            LDX(AM_ZeroPage);
            break;

        case 0xA8:
            TAY(); // Always implied
            break;

        case 0xA9:
            LDA(AM_Immediate);
            break;

        case 0xAA:
            TAX(); // Always implied
            break;

        case 0xAC:
            LDY(AM_Absolute);
            break;

        case 0xAD:
            LDA(AM_Absolute);
            break;

        case 0xAE:
            LDX(AM_Absolute);
            break;

        case 0xB0:
            BCS(); // Always relative
            break;

        case 0xB1:
            LDA(AM_IndirectY);
            break;

        case 0xB4:
            LDY(AM_ZeroPageX);
            break;

        case 0xB5:
            LDA(AM_ZeroPageX);
            break;

        case 0xB6:
            LDX(AM_ZeroPageY);
            break;

        case 0xB8:
            CLV(); // Always implied
            break;

        case 0xB9:
            LDA(AM_AbsoluteY);
            break;

        case 0xBA:
            TSX(); // Always implied
            break;

        case 0xBC:
            LDY(AM_AbsoluteX);
            break;

        case 0xBD:
            LDA(AM_AbsoluteX);
            break;

        case 0xBE:
            LDX(AM_AbsoluteY);
            break;

        case 0xC0:
            CPY(AM_Immediate);
            break;

        case 0xC1:
            CMP(AM_IndirectX);
            break;

        case 0xC4:
            CPY(AM_ZeroPage);
            break;

        case 0xC5:
            CMP(AM_ZeroPage);
            break;

        case 0xC6:
            DEC(AM_ZeroPage);
            break;

        case 0xC8:
            INY(); // Always implied
            break;

        case 0xC9:
            CMP(AM_Immediate);
            break;

        case 0xCA:
            DEX(); // Always implied
            break;

        case 0xCC:
            CPY(AM_Absolute);
            break;

        case 0xCD:
            CMP(AM_Absolute);
            break;

        case 0xCE:
            DEC(AM_Absolute);
            break;

        case 0xD0:
            BNE(); // Always relative
            break;

        case 0xD1:
            CMP(AM_IndirectY);
            break;

        case 0xD5:
            CMP(AM_ZeroPageX);
            break;

        case 0xD6:
            DEC(AM_ZeroPageX);
            break;

        case 0xD8:
            CLD(); // Always implied
            break;

        case 0xD9:
            CMP(AM_AbsoluteY);
            break;

        case 0xDD:
            CMP(AM_AbsoluteX);
            break;

        case 0xDE:
            DEC(AM_AbsoluteX);
            break;
        
        case 0xE0:
            CPX(AM_Immediate);
            break;

        case 0xE1:
            SBC(AM_IndirectX);
            break;

        case 0xE4:
            CPX(AM_ZeroPage);
            break;

        case 0xE5:
            SBC(AM_ZeroPage);
            break;

        case 0xE6:
            INC(AM_ZeroPage);
            break;

        case 0xE8:
            INX(); // Always implied
            break;

        case 0xE9:
            SBC(AM_Immediate);
            break;

        case 0xEA:
            NOP(); // Always implied
            break;

        case 0xEC:
            CPX(AM_Absolute);
            break;

        case 0xED:
            SBC(AM_Absolute);
            break;

        case 0xEE:
            INC(AM_Absolute);
            break;

        case 0xF0:
            BEQ(); // Always relative
            break;

        case 0xF1:
            SBC(AM_IndirectY);
            break;

        case 0xF5:
            SBC(AM_ZeroPageX);
            break;

        case 0xF6:
            INC(AM_ZeroPageX);
            break;

        case 0xF8:
            SED(); // Always implied
            break;

        case 0xF9:
            SBC(AM_AbsoluteY);
            break;

        case 0xFD:
            SBC(AM_AbsoluteX);
            break;

        case 0xFE:
            INC(AM_AbsoluteX);
            break;

        default:
            printf("%04hX Unhandled instruction %02hhX\n", CCPU->PC - 1, opcode);
            break;
    }
}


void ADC(AddrMode am) {
    uint8_t value = ReadProgramByte();
    uint8_t secValue;
    uint16_t temp;

    switch (am) {
        case AM_Immediate: // 2 cycles
            secValue = value;
            temp = CCPU->Accumulator + secValue;
            break;

        case AM_ZeroPage: // 3 cycles
            secValue = GetZeroPage(value);
            temp = CCPU->Accumulator + secValue;
            break;

        case AM_ZeroPageX: // 4 cycles
            secValue = GetZeroPageX(value);
            temp = CCPU->Accumulator + secValue;
            break;

        case AM_Absolute: { // 4 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            secValue = GetAbsolute(index);
            temp = CCPU->Accumulator + secValue;
            break;
        }

        case AM_AbsoluteX: { // 4 cycles (5 if page crossing)
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            secValue = GetAbsoluteX(index);
            temp = CCPU->Accumulator + secValue;
            break;
        }

        case AM_AbsoluteY: { // 4 cycles (5 if page crossing)
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            secValue = GetAbsoluteY(index);
            temp = CCPU->Accumulator + secValue;
            break;
        }

        case AM_IndirectX: { // 6 cycles
            uint16_t index = GetIndirectZPAddrX(value);
            secValue = GetAbsolute(index);
            temp = CCPU->Accumulator + secValue;
            break;
        }

        case AM_IndirectY: { // 5 cycles (6 if page crossing)
            uint16_t index = GetIndirectZPAddrY(value);
            secValue = GetAbsolute(index);
            temp = CCPU->Accumulator + secValue;
            break;
        }
        
        default:
            printf("Unhandled addressing\n");
            break;
    }

    temp += CheckBit(CCPU->Status, SFPos_Carry);

    if (temp > UINT8_MAX) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);

        temp = temp % UINT8_MAX;
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
    }

    if ((temp ^ CCPU->Accumulator) & (temp ^ secValue) & 0x80U) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Overflow);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Overflow);
    }

    CCPU->Accumulator = temp;

    CheckSetSFZero(CCPU->Accumulator);
    CheckSetSFNegative(CCPU->Accumulator);
}


void AND(AddrMode am) {
    BitwiseAccInstruction(am, BOP_AND);
}


void ASL(AddrMode am) {
    uint8_t number;

    if (am == AM_Accumulator) { // 2 cycles
        number = CCPU->Accumulator;

        if (CheckBit(number, 7U)) {
            CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);
        }
        else {
            CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
        }

        number = number >> 1;
        CCPU->Accumulator = number;
    }
    else {
        uint8_t value = ReadProgramByte();

        if (am == AM_ZeroPage) { // 5 cycles
            number = GetZeroPage(value);

            if (CheckBit(number, 7U)) {
                CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);
            }
            else {
                CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
            }

            number = number >> 1;
            StoreZeroPage(value, number);
        }
        else if (am == AM_ZeroPageX) { // 6 cycles
            number = GetZeroPageX(value);

            if (CheckBit(number, 7U)) {
                CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);
            }
            else {
                CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
            }

            number = number >> 1;
            StoreZeroPageX(value, number);
        }
        else if (am == AM_Absolute) { // 6 cycles
            uint16_t addr = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsolute(addr);

            if (CheckBit(number, 7U)) {
                CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);
            }
            else {
                CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
            }

            number = number >> 1;
            StoreAbsolute(addr, number);
        }
        else if (am == AM_AbsoluteX) { // 7 cycles
            uint16_t addr = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsoluteX(addr);

            if (CheckBit(number, 7U)) {
                CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);
            }
            else {
                CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
            }

            number = number >> 1;
            StoreAbsoluteX(addr, number);
        }
        else {
            printf("Unhandled addressing\n");
        }
    }

    CheckSetSFZero(number);

    if (CheckBit(number, 7U)) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Negative);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Negative);
    }
}


void BCC() { // 2 cycles (3 cycles if successful, 4 if crossing pages)
    uint8_t value = ReadProgramByte();
    
    if (!CheckBit(CCPU->Status, SFPos_Carry)) {
        int8_t offset = (int8_t)value;
        CCPU->PC += offset;
    }
}

void BCS() { // 2 cycles (3 cycles if succeessful, 4 if crossing pages)
    uint8_t value = ReadProgramByte();
    
    if (CheckBit(CCPU->Status, SFPos_Carry)) {
        int8_t offset = (int8_t)value;
        CCPU->PC += offset;
    }
}

void BEQ() { // 2 cycles (3 cycles if succeessful, 4 if crossing pages)
    uint8_t value = ReadProgramByte();
    
    if (CheckBit(CCPU->Status, SFPos_Zero)) {
        int8_t offset = (int8_t)value;
        CCPU->PC += offset;
    }
}

void BMI() { // 2 cycles (3 cycles if succeessful, 4 if crossing pages)
    uint8_t value = ReadProgramByte();
    
    if (CheckBit(CCPU->Status, SFPos_Negative)) {
        int8_t offset = (int8_t)value;
        CCPU->PC += offset;
    }
}

void BNE() { // 2 cycles (3 cycles if succeessful, 4 if crossing pages)
    uint8_t value = ReadProgramByte();
    
    if (!CheckBit(CCPU->Status, SFPos_Zero)) {
        int8_t offset = (int8_t)value;
        CCPU->PC += offset;
    }
}

void BPL() { // 2 cycles (3 cycles if succeessful, 4 if crossing pages)
    uint8_t value = ReadProgramByte();
    
    if (!CheckBit(CCPU->Status, SFPos_Negative)) {
        int8_t offset = (int8_t)value;
        CCPU->PC += offset;
    }
}

void BVC() { // 2 cycles (3 cycles if succeessful, 4 if crossing pages)
    uint8_t value = ReadProgramByte();
    
    if (!CheckBit(CCPU->Status, SFPos_Overflow)) {
        int8_t offset = (int8_t)value;
        CCPU->PC += offset;
    }
}

void BVS() { // 2 cycles (3 cycles if succeessful, 4 if crossing pages)
    uint8_t value = ReadProgramByte();
    
    if (CheckBit(CCPU->Status, SFPos_Overflow)) {
        int8_t offset = (int8_t)value;
        CCPU->PC += offset;
    }
}


void BIT(AddrMode am) {
    uint8_t value = ReadProgramByte();
    uint8_t number;
    uint8_t result;
    uint8_t bitTest;

    if (am == AM_ZeroPage) { // 3 cycles
        number = GetZeroPage(value);
    }
    else { // Absolute, 4 cycles
        uint16_t addr = AssembleAbsoluteAddress(value, ReadProgramByte());
        number = GetAbsolute(addr);
    }

    result = CCPU->Accumulator & number;

    //printf("BIT - A: %02hhX, number: %02hhX result: %02hhX, Pre-Status: %hhu, ", CCPU->Accumulator, number, result, CCPU->Status);

    CheckSetSFZero(result);

    bitTest = CheckBit(number, SFPos_Overflow);
    if (bitTest) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Overflow);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Overflow);
    }

    bitTest = CheckBit(number, SFPos_Negative);
    if (bitTest) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Negative);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Negative);
    }

    //printf("Post-Status: %hhu\n", CCPU->Status);
}


void BRK() { // 7 cycles
    uint8_t statusCopy = CCPU->Status;
    statusCopy = SetBit(statusCopy, SFPos_BreakCommand);

    PushByte(GetHighByte((CCPU->PC + 2)));
    PushByte(GetLowByte((CCPU->PC + 2)));
    PushByte(statusCopy);
    CCPU->PC = 0xFFFEU;

    CCPU->Status = SetBit(CCPU->Status, SFPos_InterruptDisable);
}


void CMP(AddrMode am) {
    uint8_t value = ReadProgramByte();
    uint8_t valueToCompare;

    switch (am) {
        case AM_Immediate: // 2 cycles
            valueToCompare = value;
            break;

        case AM_ZeroPage: // 3 cycles
            valueToCompare = GetZeroPage(value);
            break;

        case AM_ZeroPageX: // 4 cycles
            valueToCompare = GetZeroPageX(value);
            break;

        case AM_Absolute: { // 4 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            valueToCompare = GetAbsolute(index);
            break;
        }

        case AM_AbsoluteX: { // 4 cycles (5 if crossing pages)
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            valueToCompare = GetAbsoluteX(index);
            break;
        }

        case AM_AbsoluteY: { // 4 cycles (5 if crossing pages)
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            valueToCompare = GetAbsoluteY(index);
            break;
        }
        
        case AM_IndirectX: { // 6 cycles
            uint16_t index = GetIndirectZPAddrX(value);
            valueToCompare = GetAbsolute(index);
            break;
        }

        case AM_IndirectY: { // 5 cycles (6 if crossing pages)
            uint16_t index = GetIndirectZPAddrY(value);
            valueToCompare = GetAbsolute(index);
            break;
        }

        default:
            printf("Unhandled addressing\n");
            break;
    }

    // Actual hardware does Source - CompValue and checks if it's still positive or not

    if (CCPU->Accumulator >= valueToCompare) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
    }

    if (CCPU->Accumulator == valueToCompare) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Zero);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Zero);
    }

    // Something about negative flag, but I don't know what the "result" is
}

void CPX(AddrMode am) {
    uint8_t value = ReadProgramByte();
    uint8_t valueToCompare;

    switch (am) {
        case AM_Immediate: // 2 cycles
            valueToCompare = value;
            break;

        case AM_ZeroPage: // 3 cycles
            valueToCompare = GetZeroPage(value);
            break;

        case AM_Absolute: { // 4 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            valueToCompare = GetAbsolute(index);
            break;
        }

        default:
            printf("Unhandled addressing\n");
            break;
    }

    if (CCPU->RegX >= valueToCompare) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
    }

    if (CCPU->RegX == valueToCompare) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Zero);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Zero);
    }

    // Something about negative flag, but I don't know what the "result" is
}

void CPY(AddrMode am) {
    uint8_t value = ReadProgramByte();
    uint8_t valueToCompare;

    switch (am) {
        case AM_Immediate: // 2 cycles
            valueToCompare = value;
            break;

        case AM_ZeroPage: // 3 cycles
            valueToCompare = GetZeroPage(value);
            break;

        case AM_Absolute: { // 4 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            valueToCompare = GetAbsolute(index);
            break;
        }

        default:
            printf("Unhandled addressing\n");
            break;
    }

    if (CCPU->RegY >= valueToCompare) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
    }

    if (CCPU->RegY == valueToCompare) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Zero);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Zero);
    }

    // Something about negative flag, but I don't know what the "result" is
}


void DEC(AddrMode am) {
    uint8_t value = ReadProgramByte();
    uint8_t number;

    switch (am) {
        case AM_ZeroPage: // 5 cycles
            number = GetZeroPage(value);
            number--;
            StoreZeroPage(value, number);
            break;
        
        case AM_ZeroPageX: // 6 cycles
            number = GetZeroPageX(value);
            number--;
            StoreZeroPageX(value, number);
            break;

        case AM_Absolute: { // 6 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsolute(index);
            number--;
            StoreAbsolute(index, number);
            break;
        }

        case AM_AbsoluteX: { // 7 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsoluteX(index);
            number--;
            StoreAbsoluteX(index, number);
            break;
        }
        
        default:
            printf("Unhandled addressing\n");
            break;
    }

    CheckSetSFZero(number);
    CheckSetSFNegative(number);
}

void DEX() { // 2 cycles
    CCPU->RegX--;

    CheckSetSFZero(CCPU->RegX);
    CheckSetSFNegative(CCPU->RegX);
}

void DEY() { // 2 cycles
    CCPU->RegY--;

    CheckSetSFZero(CCPU->RegY);
    CheckSetSFNegative(CCPU->RegY);
}


void EOR(AddrMode am) {
    BitwiseAccInstruction(am, BOP_XOR);
}


void INC(AddrMode am) {
    uint8_t value = ReadProgramByte();
    uint8_t number;

    switch (am) {
        case AM_ZeroPage: // 5 cycles
            number = GetZeroPage(value);
            number++;
            StoreZeroPage(value, number);
            break;
        
        case AM_ZeroPageX: // 6 cycles
            number = GetZeroPageX(value);
            number++;
            StoreZeroPageX(value, number);
            break;

        case AM_Absolute: { // 6 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsolute(index);
            number++;
            StoreAbsolute(index, number);
            break;
        }

        case AM_AbsoluteX: { // 7 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsoluteX(index);
            number++;
            StoreAbsoluteX(index, number);
            break;
        }

        default:
            printf("Unhandled addressing\n");
            break;
    }

    CheckSetSFZero(number);
    CheckSetSFNegative(number);
}

void INX() {
    CCPU->RegX++;

    CheckSetSFZero(CCPU->RegX);
    CheckSetSFNegative(CCPU->RegX);
}

void INY() {
    CCPU->RegY++;

    CheckSetSFZero(CCPU->RegY);
    CheckSetSFNegative(CCPU->RegY);
}


void JMP(AddrMode am) {
    uint8_t first = ReadProgramByte();
    uint8_t second = ReadProgramByte();
    uint16_t addr = AssembleAbsoluteAddress(first, second);

    if (am == AM_Absolute) { // 3 cycles
        CCPU->PC = addr;
    }
    else if (am == AM_Indirect) { // 5 cycles
        CCPU->PC = GetIndirectAddr(addr);
    }
}

void JSR() { // 6 cycles
    uint8_t first = ReadProgramByte();
    uint8_t second = ReadProgramByte();
    uint16_t addr = AssembleAbsoluteAddress(first, second);

    //PushByte(CCPU, GetHighByte(addr - 1));
    //PushByte(CCPU, GetLowByte(addr - 1));

    PushByte(GetHighByte(CCPU->PC - 1));
    PushByte(GetLowByte(CCPU->PC - 1));

    CCPU->PC = addr;
}

void LDA(AddrMode am) {
    uint8_t value = ReadProgramByte();

    switch (am) {
        case AM_Immediate: // 2 cycles
            CCPU->Accumulator = value;
            break;

        case AM_ZeroPage: // 3 cycles
            CCPU->Accumulator = GetZeroPage(value);
            break;

        case AM_ZeroPageX: // 4 cycles
            CCPU->Accumulator = GetZeroPageX(value);
            break;

        case AM_Absolute: { // 4 cycles
            uint16_t addr = AssembleAbsoluteAddress(value, ReadProgramByte());
            CCPU->Accumulator = GetAbsolute(addr);
            break;
        }

        case AM_AbsoluteX: { // 4 cycles (5 if crossing pages)
            uint16_t addr = AssembleAbsoluteAddress(value, ReadProgramByte());
            CCPU->Accumulator = GetAbsoluteX(addr);
            break;
        }

        case AM_AbsoluteY: { // 4 cycles (5 if crossing pages)
            uint16_t addr = AssembleAbsoluteAddress(value, ReadProgramByte());
            CCPU->Accumulator = GetAbsoluteY(addr);
            break;
        }

        case AM_IndirectX: { // 6 cycles
            uint16_t addr = GetIndirectZPAddrX(value);
            CCPU->Accumulator = GetAbsolute(addr);
            break;
        }

        case AM_IndirectY: { // 5 cycles (6 if crossing pages)
            uint16_t addr = GetIndirectZPAddrY(value);
            CCPU->Accumulator = GetAbsolute(addr);
            break;
        }
        
        default:
            printf("Unhandled addressing\n");
            break;
    }

    //printf("A: %02X\n", CCPU->Accumulator);

    CheckSetSFZero(CCPU->Accumulator);
    CheckSetSFNegative(CCPU->Accumulator);
}

void LDX(AddrMode am) {
    uint8_t value = ReadProgramByte();

    switch (am) {
        case AM_Immediate: // 2 cycles
            CCPU->RegX = value;
            break;

        case AM_ZeroPage: // 3 cycles
            CCPU->RegX = GetZeroPage(value);
            break;

        case AM_ZeroPageY: // 4 cycles
            CCPU->RegX = GetZeroPageY(value);
            break;

        case AM_Absolute: { // 4 cycles
            uint16_t addr = AssembleAbsoluteAddress(value, ReadProgramByte());
            CCPU->RegX = GetAbsolute(addr);
            break;
        }

        case AM_AbsoluteY: { // 4 cycles (5 if crossing pages)
            uint16_t addr = AssembleAbsoluteAddress(value, ReadProgramByte());
            CCPU->RegX = GetAbsoluteY(addr);
            break;
        }
        
        default:
            printf("Unhandled addressing\n");
            break;
    }

    CheckSetSFZero(CCPU->RegX);
    CheckSetSFNegative(CCPU->RegX);
}

void LDY(AddrMode am) {
    uint8_t value = ReadProgramByte();

    switch (am) {
        case AM_Immediate: // 2 cycles
            CCPU->RegY = value;
            break;

        case AM_ZeroPage: // 3 cycles
            CCPU->RegY = GetZeroPage(value);
            break;

        case AM_ZeroPageX: // 4 cycles
            CCPU->RegY = GetZeroPageX(value);
            break;

        case AM_Absolute: { // 4 cycles
            uint16_t addr = AssembleAbsoluteAddress(value, ReadProgramByte());
            CCPU->RegY = GetAbsolute(addr);
            break;
        }

        case AM_AbsoluteX: { // 4 cycles (5 if crossing pages)
            uint16_t addr = AssembleAbsoluteAddress(value, ReadProgramByte());
            CCPU->RegY = GetAbsoluteX(addr);
            break;
        }
        
        default:
            printf("Unhandled addressing\n");
            break;
    }

    CheckSetSFZero(CCPU->RegY);
    CheckSetSFNegative(CCPU->RegY);
}


void LSR(AddrMode am) {
    uint8_t number;

    if (am == AM_Accumulator) { // 2 cycles
        number = CCPU->Accumulator;

        if (CheckBit(number, 0U)) {
            CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);
        }
        else {
            CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
        }

        number = number << 1;
        CCPU->Accumulator = number;
    }
    else {
        uint8_t value = ReadProgramByte();

        if (am == AM_ZeroPage) { // 5 cycles
            number = GetZeroPage(value);

            if (CheckBit(number, 0U)) {
                CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);
            }
            else {
                CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
            }

            number = number << 1;
            StoreZeroPage(value, number);
        }
        else if (am == AM_ZeroPageX) { // 6 cycles
            number = GetZeroPageX(value);

            if (CheckBit(number, 0U)) {
                CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);
            }
            else {
                CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
            }

            number = number << 1;
            StoreZeroPageX(value, number);
        }
        else if (am == AM_Absolute) { // 6 cycles
            uint16_t addr = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsolute(addr);

            if (CheckBit(number, 0U)) {
                CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);
            }
            else {
                CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
            }

            number = number << 1;
            StoreAbsolute(addr, number);
        }
        else if (am == AM_AbsoluteX) { // 7 cycles
            uint16_t addr = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsoluteX(addr);

            if (CheckBit(number, 0U)) {
                CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);
            }
            else {
                CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
            }

            number = number << 1;
            StoreAbsoluteX(addr, number);
        }
        else {
            printf("Unhandled addressing\n");
        }
    }

    CheckSetSFZero(number);

    if (CheckBit(number, 7U)) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Negative);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Negative);
    }
}


void ORA(AddrMode am) {
    BitwiseAccInstruction(am, BOP_OR);
}


void PHA() { // 3 cycles
    PushByte(CCPU->Accumulator);
}

void PHP() { // 3 cycles
    PushByte(CCPU->Status);
}


void PLA() { // 4 cycles
    CCPU->Accumulator = PopByte();

    CheckSetSFZero(CCPU->Accumulator);
    CheckSetSFNegative(CCPU->Accumulator);
}

void PLP() { // 4 cycles
    CCPU->Status = PopByte();
}


void ROL(AddrMode am) {
    uint8_t number;
    uint8_t carryBitSeven;

    if (am == AM_Accumulator) { // 2 cycles
        number = CCPU->Accumulator;
        carryBitSeven = CheckBit(number, 7U);

        number = number >> 1;

        if (CheckBit(CCPU->Status, SFPos_Carry)) {
            number = SetBit(number, 0U);
        }

        CCPU->Accumulator = number;
        CheckSetSFZero(CCPU->Accumulator);
    }
    else {
        uint8_t value = ReadProgramByte();

        if (am == AM_ZeroPage) { // 5 cycles
            number = GetZeroPage(value);

            carryBitSeven = CheckBit(number, 7U);

            number = number >> 1;

            if (CheckBit(CCPU->Status, SFPos_Carry)) {
                number = SetBit(number, 0U);
            }

            StoreZeroPage(value, number);
        }
        else if (am == AM_ZeroPageX) { // 6 cycles
            number = GetZeroPageX(value);

            carryBitSeven = CheckBit(number, 7U);

            number = number >> 1;

            if (CheckBit(CCPU->Status, SFPos_Carry)) {
                number = SetBit(number, 0U);
            }

            StoreZeroPageX(value, number);
        }
        else if (am == AM_Absolute) { // 6 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsolute(index);

            carryBitSeven = CheckBit(number, 7U);

            number = number >> 1;

            if (CheckBit(CCPU->Status, SFPos_Carry)) {
                number = SetBit(number, 0U);
            }

            StoreAbsolute(index, number);
        }
        else if (am == AM_AbsoluteX) { // 7 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsoluteX(index);

            carryBitSeven = CheckBit(number, 7U);

            number = number >> 1;

            if (CheckBit(CCPU->Status, SFPos_Carry)) {
                number = SetBit(number, 0U);
            }

            StoreAbsoluteX(index, number);
        }
        else {
            printf("Unhandled addressing\n");
        }
    }

    if (carryBitSeven) {
        SetBit(CCPU->Status, SFPos_Carry);
    }
    else {
        ClearBit(CCPU->Status, SFPos_Carry);
    }

    // Is resulting number negative (above 127)?
    if (CheckBit(number, 7U)) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Negative);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Negative);
    }
}

void ROR(AddrMode am) {
    uint8_t number;
    uint8_t carryBitZero;

    if (am == AM_Accumulator) { // 2 cycles
        number = CCPU->Accumulator;
        carryBitZero = CheckBit(number, 0U);

        number = number << 1;

        if (CheckBit(CCPU->Status, SFPos_Carry)) {
            number = SetBit(number, 7U);
        }

        CCPU->Accumulator = number;
        CheckSetSFZero(CCPU->Accumulator);
    }
    else {
        uint8_t value = ReadProgramByte();

        if (am == AM_ZeroPage) { // 5 cycles
            number = GetZeroPage(value);

            carryBitZero = CheckBit(number, 0U);

            number = number << 1;

            if (CheckBit(CCPU->Status, SFPos_Carry)) {
                number = SetBit(number, 7U);
            }

            StoreZeroPage(value, number);
        }
        else if (am == AM_ZeroPageX) { // 6 cycles
            number = GetZeroPageX(value);

            carryBitZero = CheckBit(number, 0U);

            number = number << 1;

            if (CheckBit(CCPU->Status, SFPos_Carry)) {
                number = SetBit(number, 7U);
            }

            StoreZeroPageX(value, number);
        }
        else if (am == AM_Absolute) { // 6 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsolute(index);

            carryBitZero = CheckBit(number, 0U);

            number = number << 1;

            if (CheckBit(CCPU->Status, SFPos_Carry)) {
                number = SetBit(number, 7U);
            }

            StoreAbsolute(index, number);
        }
        else if (am == AM_AbsoluteX) { // 7 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            number = GetAbsoluteX(index);

            carryBitZero = CheckBit(number, 0U);

            number = number << 1;

            if (CheckBit(CCPU->Status, SFPos_Carry)) {
                number = SetBit(number, 7U);
            }

            StoreAbsoluteX(index, number);
        }
        else {
            printf("Unhandled addressing\n");
        }
    }

    if (carryBitZero) {
        SetBit(CCPU->Status, SFPos_Carry);
    }
    else {
        ClearBit(CCPU->Status, SFPos_Carry);
    }

    // Is resulting number negative (above 127)?
    if (CheckBit(number, 7U)) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Negative);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Negative);
    }
}


void RTI() { // 6 cycles
    uint8_t status = PopByte();
    uint8_t low = PopByte();
    uint8_t high = PopByte();

    CCPU->Status = status;
    uint16_t addr = AssembleAbsoluteAddress(low, high);
    CCPU->PC = addr;
}

void RTS() { // 6 cycles
    uint8_t first = PopByte();
    uint8_t second = PopByte();
    uint16_t addr = AssembleAbsoluteAddress(first, second);

    CCPU->PC = addr + 1;
}


void SBC(AddrMode am) {
    uint8_t value = ReadProgramByte();
    uint8_t secValue;
    uint16_t temp;

    switch (am) {
        case AM_Immediate: // 2 cycles
            secValue = value;
            temp = CCPU->Accumulator - secValue;
            break;

        case AM_ZeroPage: // 3 cycles
            secValue = GetZeroPage(value);
            temp = CCPU->Accumulator - secValue;
            break;

        case AM_ZeroPageX: // 4 cycles
            secValue = GetZeroPageX(value);
            temp = CCPU->Accumulator - secValue;
            break;

        case AM_Absolute: { // 4 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            secValue = GetAbsolute(index);
            temp = CCPU->Accumulator - secValue;
            break;
        }

        case AM_AbsoluteX: { // 4 cycles (5 if page crossing)
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            secValue = GetAbsoluteX(index);
            temp = CCPU->Accumulator - secValue;
            break;
        }

        case AM_AbsoluteY: { // 4 cycles (5 if page crossing)
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            secValue = GetAbsoluteY(index);
            temp = CCPU->Accumulator - secValue;
            break;
        }

        case AM_IndirectX: { // 6 cycles
            uint16_t index = GetIndirectZPAddrX(value);
            secValue = GetAbsolute(index);
            temp = CCPU->Accumulator - secValue;
            break;
        }

        case AM_IndirectY: { // 5 cycles (6 if page crossing)
            uint16_t index = GetIndirectZPAddrY(value);
            secValue = GetAbsolute(index);
            temp = CCPU->Accumulator - secValue;
            break;
        }
        
        default:
            printf("Unhandled addressing\n");
            break;
    }

    temp -= (1 - CheckBit(CCPU->Status, SFPos_Carry));

    if (temp > UINT8_MAX) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);

        temp = temp % UINT8_MAX;
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
    }

    if ((temp ^ CCPU->Accumulator) & (temp ^ secValue) & 0x80U) {
        CCPU->Status = SetBit(CCPU->Status, SFPos_Overflow);
    }
    else {
        CCPU->Status = ClearBit(CCPU->Status, SFPos_Overflow);
    }

    CCPU->Accumulator = temp;

    CheckSetSFZero(CCPU->Accumulator);
    CheckSetSFNegative(CCPU->Accumulator);
}


void TAX() { // 2 cycles
    CCPU->RegX = CCPU->Accumulator;

    CheckSetSFZero(CCPU->RegX);
    CheckSetSFNegative(CCPU->RegX);
}

void TAY() { // 2 cycles
    CCPU->RegY = CCPU->Accumulator;

    CheckSetSFZero(CCPU->RegY);
    CheckSetSFNegative(CCPU->RegY);
}

void TXA() { // 2 cycles
    CCPU->Accumulator = CCPU->RegX;

    CheckSetSFZero(CCPU->Accumulator);
    CheckSetSFNegative(CCPU->Accumulator);
}

void TYA() { // 2 cycles
    CCPU->Accumulator = CCPU->RegY;

    CheckSetSFZero(CCPU->Accumulator);
    CheckSetSFNegative(CCPU->Accumulator);
}


void TSX() { // 2 cycles
    CCPU->RegX = CCPU->SP;

    CheckSetSFZero(CCPU->RegX);
    CheckSetSFNegative(CCPU->RegX);
}

void TXS() { // 2 cycles
    CCPU->SP = CCPU->RegX;
}


void STA(AddrMode am) {
    const uint8_t value = ReadProgramByte();

    switch (am) {
        case AM_ZeroPage: // 3 cycles
            StoreZeroPage(value, CCPU->Accumulator);
            break;

        case AM_ZeroPageX: // 4 cycles
            StoreZeroPageX(value, CCPU->Accumulator);
            break;
    
        case AM_Absolute: { // 4 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            StoreAbsolute(index, CCPU->Accumulator);
            break;
        }

        case AM_AbsoluteX: { // 5 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            StoreAbsoluteX(index, CCPU->Accumulator);
            break;
        }

        case AM_AbsoluteY: { // 5 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            StoreAbsoluteY(index, CCPU->Accumulator);
            break;
        }

        case AM_IndirectX: { // 6 cycles
            uint16_t index = GetIndirectZPAddrX(value);
            StoreAbsolute(index, CCPU->Accumulator);
            break;
        }

        case AM_IndirectY: { // 6 cycles
            uint16_t index = GetIndirectZPAddrY(value);
            StoreAbsolute(index, CCPU->Accumulator);
            break;
        }

        default:
            printf("Unhandled addressing\n");
            break;
    }
}

void STX(AddrMode am) {
    const uint8_t value = ReadProgramByte();

    switch (am) {
        case AM_ZeroPage: // 3 cycles
            StoreZeroPage(value, CCPU->RegX);
            break;

        case AM_ZeroPageY: // 4 cycles
            StoreZeroPageY(value, CCPU->RegX);
            break;
    
        case AM_Absolute: // 4 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            StoreAbsolute(index, CCPU->RegX);
            break;

        default:
            printf("Unhandled addressing\n");
            break;
    }
}

void STY(AddrMode am) {
    const uint8_t value = ReadProgramByte();

    switch (am) {
        case AM_ZeroPage: // 3 cycles
            StoreZeroPage(value, CCPU->RegY);
            break;

        case AM_ZeroPageX: // 4 cycles
            StoreZeroPageX(value, CCPU->RegY);
            break;
    
        case AM_Absolute: // 4 cycles
            uint16_t index = AssembleAbsoluteAddress(value, ReadProgramByte());
            StoreAbsolute(index, CCPU->RegY);
            break;

        default:
            printf("Unhandled addressing\n");
            break;
    }
}


void SEC() { // 2 cycles
    CCPU->Status = SetBit(CCPU->Status, SFPos_Carry);
}

void CLC() { // 2 cycles
    CCPU->Status = ClearBit(CCPU->Status, SFPos_Carry);
}

void SEI() { // 2 cycles
    CCPU->Status = SetBit(CCPU->Status, SFPos_InterruptDisable);
}

void CLI() { // 2 cycles
    CCPU->Status = ClearBit(CCPU->Status, SFPos_InterruptDisable);
}

void SED() { // 2 cycles
    CCPU->Status = SetBit(CCPU->Status, SFPos_DecimalMode);
}

void CLD() { // 2 cycles
    CCPU->Status = ClearBit(CCPU->Status, SFPos_DecimalMode);
}

void CLV() { // 2 cycles
    CCPU->Status = ClearBit(CCPU->Status, SFPos_Overflow);
}

void NOP() { // 2 cycles
    
}
