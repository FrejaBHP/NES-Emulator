#include <stdlib.h>
#include <console.h>
#include <cpu.h>
#include <ppu.h>
#include <rom.h>

SystemType System = SYS_NTSC;

uint32_t CPUTimeStamp = 0;   // How many master cycles the CPU has used this frame
uint32_t PPUTimeStamp = 0;   // How many master cycles the PPU has used this frame
uint32_t CPUCycleCount = 0;  // How many CPU cycles has been used this frame
uint32_t CPUCycleCountLast = 0;
uint32_t PPUCycleCount = 0;  // How many PPU cycles has been used this frame
uint8_t CPUCyclesCarry = 0;

uint32_t FrameCount = 0;

int16_t CurScanline = -1; // There's a pre-render scanline, noted here with -1
uint16_t CurDot = 0;
uint8_t* BGFrameBuffer = NULL;

uint8_t StopExecution = 0;
uint8_t HasAnnouncedStop = 0;

ControllerInput Input0 = { 0 };
uint8_t InputBuffer0 = 0;

void SetupConsole() {
    if (CurROM->TimingMode == TMode_RP2C02) {
        System = SYS_NTSC;
    }
    else if (CurROM->TimingMode == TMode_RP2C07) {
        System = SYS_PAL;
    }
    else if (CurROM->TimingMode == TMode_Multiple) {
        // TEMP
        System = SYS_NTSC;
    }
    else if (CurROM->TimingMode == TMode_UA6538) {
        System = SYS_PAL;
    }

    BGFrameBuffer = malloc(sizeof(uint8_t) * 256 * 240 * 3);
}

void ResetFrameCount() {
    // This shouldn't fully reset, but not sure of exact numbers to deduct yet
    //CPUTimeStamp -= (NumCPUCycles_NTSC * CPUCycleDivider_NTSC);
    //CPUCycleCount -= NumCPUCycles_NTSC;
    CPUTimeStamp = 0;
    CPUCycleCount = 0;
    CPUCycleCountLast = 0;

    //PPUTimeStamp -= (NumCPUCycles_NTSC * CPUCycleDivider_NTSC);
    //PPUCycleCount -= (NumCPUCycles_NTSC * (CPUCycleDivider_NTSC / PPUCycleDivider));
    PPUTimeStamp = 0;
    PPUCycleCount = 0;
}

void UseCPUCycles(uint8_t amount) {
    CPUCycleCount += amount;

    if (System == SYS_NTSC) {
        CPUTimeStamp = CPUCycleCount * CPUCycleDivider_NTSC;
    }
    else {
        CPUTimeStamp = CPUCycleCount * CPUCycleDivider_PAL;
    }
}

void UsePPUCycles(uint8_t amount) {
    PPUCycleCount += amount;

    PPUTimeStamp = PPUCycleCount * PPUCycleDivider;
}

uint8_t IsVisibleOnScanline(uint8_t scanline, uint8_t topY) {
    uint8_t bottomY = topY - 8;

    if (scanline <= topY && scanline >= bottomY) {
        return 1;
    }
    else {
        return 0;
    }  
}

void RunCPU(uint32_t timestamp) {
    //printf("CPU timestamp: %u\n", timestamp);
    if (CPUTimeStamp >= timestamp) {
        return;
    }

    /*
    uint8_t inc = 0;
    uint8_t recentOpcode[3];
    uint8_t recentCount[3];
    */

    while (CPUTimeStamp < timestamp && !StopExecution) {
        RunPPU(CPUTimeStamp);

        uint8_t instruction = ReadInstruction();
        //printf("Addr: %04X, Instruction: %02X.    Next two bytes: %02X, %02X.    A: %02X, X: %02X, Y: %02X, Status: %02X\n", CCPU->PC - 1, instruction, CPUMemory[CCPU->PC], CPUMemory[CCPU->PC + 1], CCPU->Accumulator, CCPU->RegX, CCPU->RegY, CCPU->Status);

        /*
        if (CCPU->PC - 1 >= 0xF207 && CCPU->PC - 1 <= 0xF23C) {
            printf("Addr: %04X, Instruction: %02X.    Next two bytes: %02X, %02X.    A: %02X, X: %02X, Y: %02X, Status: %02X\n", CCPU->PC - 1, instruction, CPUMemory[CCPU->PC], CPUMemory[CCPU->PC + 1], CCPU->Accumulator, CCPU->RegX, CCPU->RegY, CCPU->Status);
            //printf("A: %02X, X: %02X, Y: %02X, Status: %02X\n", CCPU->Accumulator, CCPU->RegX, CCPU->RegY, CCPU->Status);
        }
        */

        /*
        if (recentOpcode[inc] == instruction) {
            recentCount[inc] += 1;
        }
        else {
            recentOpcode[inc] = instruction;
            recentCount[inc] = 0;
        }
        
        if (recentCount[0] > 4 && recentCount[1] > 4 && recentCount[2] > 4) {
            printf("Stuck looping instructions, breaking...\n");
            StopExecution = 1;
            break;
        }

        inc++;
        if (inc == 3) {
            inc = 0;
        }
        */

        if (instruction == 0) {
            break;
        }
        
        ExecuteInstruction(instruction);
    }
}

void RunPPU(uint32_t timestamp) {
    if (PPUTimeStamp >= timestamp) {
        return;
    }

    while (PPUTimeStamp < timestamp) {
        // End of VBlank
        if (CurScanline == -1 && CurDot == 1) {
            OverrideBit8(CurPPU->PPUSTATUS, PPUSTATUS_VBlank, 0);
        }

        /*
        // Drawing loop
        if (CurScanline >= 0 && CurScanline <= 239) {

        }
        */

        // VBlank
        else if (CurScanline == 241 && CurDot == 1) {
            OverrideBit8(CurPPU->PPUSTATUS, PPUSTATUS_VBlank, 1);

            if (CheckBit(*CurPPU->PPUCTRL, PPUCTRL_VBlankNMIEnable)) {
                TriggerNMI();
            }

            DrawFrame();
        }

        CurDot++;
        UsePPUCycles(1U);

        if (CurDot >= Scanline_Length) {
            CurDot -= Scanline_Length;
            CurScanline++;

            if (CurScanline == 262) {
                CurScanline = -1;
            }
        }
    }

    /*
    uint8_t OAMindex = 0;
    uint8_t SecOAMindex = 0;
    uint8_t readValue = 0;
    uint8_t secOAMfull = 0;

    uint8_t spriteWithinY = 0;
    uint8_t spriteByteToCopy = 0;

    // pre-render scanline
    if (CurScanline == -1) {
        // if dot == 1
        OverrideBit8(CurPPU->PPUSTATUS, PPUSTATUS_VBlank, 0);
    }

    // Visible scanlines
    while (CurScanline < 240) {
        // Drawing the active part of the screen
        while (CurDot < 256) {
            // fetch tiles
            if (CurDot != 0) {

            }

            // Init SecOAM
            if (CurDot != 0 && CurDot <= 64) {
                if (CurDot % 2 == 0) {
                    CurPPU->SecOAM[CurDot / 2] = 0xFF;
                }
            }
            // Evaluate sprites for next scanline
            else if (CurDot <= 256) {
                // Even cycles, write
                if (CurDot % 2 == 0) {
                    if (SecOAMindex < 32) {
                        CurPPU->SecOAM[SecOAMindex] = readValue;

                        if (spriteWithinY) {
                            SecOAMindex++;
                            spriteByteToCopy++;

                            if (spriteByteToCopy > 3) {
                                spriteByteToCopy = 0;
                                spriteWithinY = 0;
                            }
                        }
                    }
                    else {
                        secOAMfull = 1;
                    }
                }
                // Odd cycles, read and increment index
                else if (CurDot % 2 == 1 || secOAMfull) {
                    readValue = CurPPU->OAM[OAMindex];

                    if (!spriteWithinY && IsVisibleOnScanline(CurScanline + 1, readValue)) {
                        spriteWithinY = 1;
                        OAMindex++;
                    }
                    else {
                        OAMindex += 4;
                    }
                }
            }

            CurDot++;
            UsePPUCycles(1U);

            if (PPUTimeStamp >= timestamp) {
                return;
            }
        }

        // Drawing outside
        while (CurDot < Scanline_Length - 1) {

        }

        CurDot = 0;
        CurScanline++;
    }

    // post-render scanline
    if (CurScanline == 240) {
        CurScanline++;
    }

    while (CurScanline < Scanlines_NTSC - 1) {
        if (CurScanline == 241) {
            // If dot == 1
            OverrideBit8(CurPPU->PPUSTATUS, PPUSTATUS_VBlank, 1);

            if (CheckBit(*CurPPU->PPUCTRL, PPUCTRL_VBlankNMIEnable)) {
                TriggerNMI();
            }
        }

        CurScanline++;
    }
    */
}

void DrawFrame() {
    // Cheating a bit
    uint16_t ntBaseAddr = GetBaseNameTableAddress();

    for (size_t row = 0; row < 240; row++) {
        for (size_t col = 0; col < 256; col++) {
            uint16_t tileNum = ((row / 8) * 32) + (col / 8);
            
            uint16_t tileID = PPURead(ntBaseAddr + tileNum);
            uint16_t bgTileAddr = GetBaseBGPatternTableAddress() + (tileID * 0x10) + (row % 8);
            uint16_t tileAttr = PPURead(((ntBaseAddr + tileNum) & 0xFC00) + 0x03C0 + ((row / 32) * 8) + (col / 32));
            uint16_t attrShift = (((tileNum % 32) / 2 % 2) + (tileNum / 64 % 2) * 2) * 2;
            uint16_t paletteOffset = ((tileAttr >> attrShift) & 0x3) * 4;
            uint8_t pixel = ((PPURead(bgTileAddr) >> (7 - (col % 8))) & 1) + (((PPURead(bgTileAddr + 8) >> (7 - (col % 8))) & 1) * 2);

            BGFrameBuffer[(row * 256 * 3) + (col * 3)] = (Palette_NTSC[PPURead(PaletteRAMIndeces_Start + paletteOffset + pixel)] >> 16) & 0xFF;
            BGFrameBuffer[(row * 256 * 3) + (col * 3) + 1] = (Palette_NTSC[PPURead(PaletteRAMIndeces_Start + paletteOffset + pixel)] >> 8) & 0xFF;
            BGFrameBuffer[(row * 256 * 3) + (col * 3) + 2] = (Palette_NTSC[PPURead(PaletteRAMIndeces_Start + paletteOffset + pixel)]) & 0xFF;
        }
    }
}
