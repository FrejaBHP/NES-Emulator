#include <stdlib.h>
#include <string.h>
#include <console.h>
#include <cpu.h>
#include <ppu.h>
#include <apu.h>
#include <rom.h>

SystemType System = SYS_NTSC;

const float APUSampleDivider_NTSC = (float)CPUClockSpeed_NTSC / (float)SampleRate / 2;
const float APUSampleDivider_PAL = (float)CPUClockSpeed_PAL / (float)SampleRate / 2;

uint32_t CPUTimeStamp = 0;   // How many master cycles the CPU has used this frame
uint32_t PPUTimeStamp = 0;   // How many master cycles the PPU has used this frame
uint32_t CPUCycleCount = 0;  // How many CPU cycles has been used this frame
//uint32_t CPUCycleCountLast = 0;
uint32_t PPUCycleCount = 0;  // How many PPU cycles has been used this frame
uint8_t CPUCyclesCarry = 0;

uint8_t AlternateFrame = 0;
uint16_t SampleCounter = 0;
uint32_t FrameCount = 0;

int16_t CurScanline = -1; // There's a pre-render scanline, noted here with -1
uint16_t CurDot = 0;
uint8_t* BGFrameBuffer = NULL;
uint8_t* SPRFrameBuffer = NULL;

int16_t* SoundBuffer = NULL;

uint8_t DMAOccured = 0;
bool QueueNMI = false;
bool NMIOccured = false;

uint8_t StopExecution = 0;
uint8_t HasAnnouncedStop = 0;

ControllerInput Input0 = { 0 };
uint8_t Input0Conv = 0;
uint8_t Input0Buffer = 0;

EmuState States[16] = { 0 };
size_t StateIndex = 0;

uint16_t temp = 0x2000;

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
    SPRFrameBuffer = malloc(sizeof(uint8_t) * 256 * 240 * 4);
}

void ResetFrameCount() {
    // FIXME: This shouldn't fully reset, but not sure of exact numbers to deduct yet
    // Addendum: Even fixing the scanline count, this still leads to a grey screen

    //const uint32_t tempc1 = (NumCPUCycles_NTSC * CPUCycleDivider_NTSC);
    //const uint32_t tempc2 = NumCPUCycles_NTSC;
    //printf("CPU T: %u, -%u\nCPU C: %u, -%u\n", CPUTimeStamp, temp1, CPUCycleCount, temp2);

    CPUTimeStamp -= (NumCPUCycles_NTSC * CPUCycleDivider_NTSC);
    CPUCycleCount -= NumCPUCycles_NTSC;

    //CPUTimeStamp = 0;
    //CPUCycleCount = 0;

    PPUTimeStamp -= (NumCPUCycles_NTSC * CPUCycleDivider_NTSC);
    PPUCycleCount -= (NumCPUCycles_NTSC * (CPUCycleDivider_NTSC / PPUCycleDivider));

    //PPUTimeStamp = 0;
    //PPUCycleCount = 0;
}

void UseCPUCycles(uint8_t amount) {
    CPUCycleCount += amount;

    if (System == SYS_NTSC) {
        CPUTimeStamp = CPUCycleCount * CPUCycleDivider_NTSC;
    }
    else {
        CPUTimeStamp = CPUCycleCount * CPUCycleDivider_PAL;
    }

    ClockAPU();

    //RunPPU(CPUTimeStamp);
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
        if (!NMIOccured && QueueNMI) {
            NMIOccured = true;
            QueueNMI = false;
            TriggerNMI();
        }

        

        IsExecutingInstruction = 1;
        uint8_t instruction = ReadProgramByte();
        WriteStateLog(instruction);
        //printf("Addr: %04X, Instruction: %02X.    Next two bytes: %02X, %02X.    A: %02X, X: %02X, Y: %02X, Status: %02X\n", CCPU->PC - 1, instruction, CPUMemory[CCPU->PC], CPUMemory[CCPU->PC + 1], CCPU->Accumulator, CCPU->RegX, CCPU->RegY, CCPU->Status);

        /*
        if (CCPU->PC - 1 >= 0xF207 && CCPU->PC - 1 <= 0xF23C) {
            printf("Addr: %04X, Instruction: %02X.    Next two bytes: %02X, %02X.    A: %02X, X: %02X, Y: %02X, Status: %02X\n", CCPU->PC - 1, instruction, CPUMemory[CCPU->PC], CPUMemory[CCPU->PC + 1], CCPU->Accumulator, CCPU->RegX, CCPU->RegY, CCPU->Status);
            //printf("A: %02X, X: %02X, Y: %02X, Status: %02X\n", CCPU->Accumulator, CCPU->RegX, CCPU->RegY, CCPU->Status);
        }
        */
        
        ExecuteInstruction(instruction);
        RunPPU(CPUTimeStamp);
        IsExecutingInstruction = 0;
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
            OverrideBit8(CurPPU->PPUSTATUS, PPUSTATUS_Sprite0Hit, 0);
        }

        // Drawing loop
        if (CurScanline >= 0 && CurScanline <= 239) {
            if (CurScanline == 30 && CurDot == 91) {
                if (CheckBit(*CurPPU->PPUMASK, PPUMASK_EnableBGRendering) && CheckBit(*CurPPU->PPUMASK, PPUMASK_EnableSPRRendering)) {
                    OverrideBit8(CurPPU->PPUSTATUS, PPUSTATUS_Sprite0Hit, 1);
                }
            }

            if (CurDot != 0 && CurDot < 257) {
                DrawBGPixel((uint8_t)(CurDot - 1), (uint8_t)CurScanline);
            }
        }
        // VBlank
        else if (CurScanline == 241 && CurDot == 1) {
            OverrideBit8(CurPPU->PPUSTATUS, PPUSTATUS_VBlank, 1);

            if (CheckBit(*CurPPU->PPUCTRL, PPUCTRL_VBlankNMIEnable)) {
                //TriggerNMI();
                QueueNMI = true;
            }

            //DrawBGLayer();
            DrawSPRLayer();
        }

        if (CurDot == 256) {
            
        }
        else if (CurDot == 257) {
            if (CheckBit(*CurPPU->PPUMASK, PPUMASK_EnableBGRendering)) {
                uint16_t temp = CurPPU->RegV;
                OverrideBit16(&temp, 10, CheckBit(GetHighByte(CurPPU->RegT), 2));
                temp >>= 5;
                temp <<= 5;

                uint16_t other = 0b00011111 & CurPPU->RegT;
                temp += other;

                CurPPU->RegV = temp;
                //CurPPU->RegV = CurPPU->RegT;
            }
        }

        CurDot++;
        UsePPUCycles(1U);

        if (CurDot >= Scanline_Length) {
            CurDot -= Scanline_Length;
            CurScanline++;

            if (CurScanline == 261) {
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

void DrawBGLayer() {
    // Cheating a bit
    const uint16_t ntBaseAddr = GetBaseNameTableAddress();
    const uint16_t coarseX = (uint16_t)0b0000000000011111 & CurPPU->RegV;

    for (size_t row = 0; row < 240; row++) {
        for (size_t col = 0; col < 256; col++) {
            //const uint16_t tileNum = ((row / 8) * 32) + (col / 8);
            const uint16_t tileNum = ((row / 8) * 32) + ((col + CurPPU->RegX % 8) / 8);
            
            const uint16_t natAddr = ntBaseAddr + tileNum;
            //const uint16_t natAddr = (ntBaseAddr + tileNum) + (0x40U * ((ntBaseAddr + tileNum) % 0x2000U) / 0x03C0);
            uint16_t scrAddr = natAddr + CurPPU->RegX / 8 + coarseX;
            
            // If nametable is crossed OR hitting a transfer tile past the middle of the screen
            if (((natAddr & 0xFFE0) != (scrAddr & 0xFFE0)) || ((col > 128) && ((tileNum % 32) == 0))) {
                scrAddr ^= 0x0400U;
                scrAddr -= 0x20U;
            }

            const uint16_t tileID = PPURead(scrAddr);
            const uint16_t bgTileAddr = GetBaseBGPatternTableAddress() + (tileID * 0x10) + (row % 8);
            //const uint16_t tileAttr = PPURead(((natTileAddr) & 0xFC00) + 0x03C0 + ((row / 32) * 8) + (col / 32));
            const uint16_t tileAttr = GetAttribute(scrAddr);
            //const uint16_t attrShift = (((tileNum % 32) / 2 % 2) + (tileNum / 64 % 2) * 2) * 2;
            const uint16_t attrShift = GetAttributeTilePart(scrAddr);
            const uint16_t paletteOffset = ((tileAttr >> attrShift) & 0x3) * 4;
            //const uint8_t pixel = ((PPURead(bgTileAddr) >> (7 - (col % 8))) & 1) + (((PPURead(bgTileAddr + 8) >> (7 - (col % 8))) & 1) * 2);
            const uint8_t pixel = ((PPURead(bgTileAddr) >> (7 - ((col + CurPPU->RegX % 8) % 8))) & 1) + (((PPURead(bgTileAddr + 8) >> (7 - ((col + CurPPU->RegX % 8) % 8))) & 1) * 2);

            uint16_t paletteIndex = PaletteRAMIndeces_Start + paletteOffset + pixel;

            // Might need some tweaking once sprites exist
            if (paletteIndex % 4 == 0) {
                paletteIndex = 0x3F00U;
            }

            BGFrameBuffer[(row * 256 * 3) + (col * 3)] = (Palette_NTSC[PPURead(paletteIndex)] >> 16) & 0xFF;
            BGFrameBuffer[(row * 256 * 3) + (col * 3) + 1] = (Palette_NTSC[PPURead(paletteIndex)] >> 8) & 0xFF;
            BGFrameBuffer[(row * 256 * 3) + (col * 3) + 2] = (Palette_NTSC[PPURead(paletteIndex)]) & 0xFF;
        }
    }
}

void DrawBGPixel(uint8_t x, uint8_t y) {
    /*
    Research results from Monday:
        - MESEN is annoying and doesn't show 2000.0/2000.1
        - There's a high possibility that scrolling and indexing relies entirely on V during rendering
        - Because I don't use V for anything except Coarse X, I run into problems
        - PPUCTRL is set to something else sometimes at the end of frame, and isn't reset
        - V, however, is set to 0, which would imply no scrolling and using the first table
        - Putting those elements together, it is likely V is what I should be using instead of the registers (and incomprehensible code) for scrolling and indexing
        - Meaning that V is progressively built up as rendering happens, and writing to the registers is to primarily copy specific values into V
        - Only problem is that V seems to always be 0 somehow??
    */

    const uint16_t ntBaseAddr = GetBaseNameTableAddress();
    const uint16_t coarseX = (uint16_t)0b0000000000011111 & CurPPU->RegV;

    const uint16_t tileNum = ((y / 8) * 32) + ((x + CurPPU->RegX % 8) / 8);
    
    const uint16_t natAddr = ntBaseAddr + tileNum;
    uint16_t scrAddr = natAddr + CurPPU->RegX / 8 + coarseX;
    
    // If nametable is crossed OR hitting a transfer tile past the middle of the screen
    if (((natAddr & 0xFFE0) != (scrAddr & 0xFFE0)) || ((x > 128) && ((tileNum % 32) == 0))) {
        scrAddr ^= 0x0400U;
        scrAddr -= 0x20U;
    }

    if (ntBaseAddr != temp) {
        //printf("NewBAd    BaseAddr: %04X, ScrAddr: %04X, V: %04X, X: %02u, Y: %02u, PC: %04X\n", ntBaseAddr, scrAddr, CurPPU->RegV, x, y, CCPU->PC);
    }

    if (x == 32 && y == 16) {
        //printf("X32Y16    BaseAddr: %04X, ScrAddr: %04X, V: %04X\n", ntBaseAddr, scrAddr, CurPPU->RegV);
    }

    temp = ntBaseAddr;

    const uint16_t tileID = PPURead(scrAddr);
    const uint16_t bgTileAddr = GetBaseBGPatternTableAddress() + (tileID * 0x10) + (y % 8);
    const uint16_t tileAttr = GetAttribute(scrAddr);
    const uint16_t attrShift = GetAttributeTilePart(scrAddr);
    const uint16_t paletteOffset = ((tileAttr >> attrShift) & 0x3) * 4;
    const uint8_t pixel = ((PPURead(bgTileAddr) >> (7 - ((x + CurPPU->RegX % 8) % 8))) & 1) + (((PPURead(bgTileAddr + 8) >> (7 - ((x + CurPPU->RegX % 8) % 8))) & 1) * 2);

    uint16_t paletteIndex = PaletteRAMIndeces_Start + paletteOffset + pixel;

    // Might need some tweaking once sprites exist
    if (paletteIndex % 4 == 0) {
        paletteIndex = 0x3F00U;
    }

    BGFrameBuffer[(y * 256 * 3) + (x * 3)] = (Palette_NTSC[PPURead(paletteIndex)] >> 16) & 0xFF;
    BGFrameBuffer[(y * 256 * 3) + (x * 3) + 1] = (Palette_NTSC[PPURead(paletteIndex)] >> 8) & 0xFF;
    BGFrameBuffer[(y * 256 * 3) + (x * 3) + 2] = (Palette_NTSC[PPURead(paletteIndex)]) & 0xFF;
}

// Don't use
void GetValidSPR(SpriteData* sprites) {
    SpriteData* spr = (SpriteData*)CurPPU->OAM;
    uint8_t validCount = 0;
    uint8_t index = 0;

    while (true) {
        if (index == 64) {
            break;
        }

        if (spr->PositionY < 0xEFU) {
            if (validCount < 8) {
                sprites[validCount] = *spr;

                validCount++;
            }
            else {
                // Set sprite overflow flag
                break;
            }
        }

        spr++;
        index++;
    }
}

void DrawSPRLayer() {
    // Zeroes buffer, effectively making it transparent
    memset(SPRFrameBuffer, 0, sizeof(uint8_t) * 256 * 240 * 4);

    if (!CheckBit(*CurPPU->PPUMASK, PPUMASK_EnableSPRRendering)) {
        return;
    }

    SpriteData* spr = (SpriteData*)CurPPU->OAM;
    
    for (size_t i = 0; i < 64; i++) {
        if (spr->PositionY < 0xEFU) {
            DrawSPR(spr);
        }
        spr++;
    }
}

void DrawSPR(SpriteData* spr) {
    // FIXME: Temporary routine to skip drawing low priority sprites - find solution later
    /*
    if (CheckBit(spr->Attributes, SPRAttrPos_Priority)) {
        return;
    }
    */

    uint16_t ntBaseAddr;
    const bool isBigSprite = CheckBit(*CurPPU->PPUCTRL, PPUCTRL_SpriteSize);

    if (isBigSprite) {
        ntBaseAddr = CheckBit(spr->TileIndex, 0U);
    }
    else {
        ntBaseAddr = GetBaseNameTableAddress();
    }

    const uint8_t actualPosY = spr->PositionY + 1;

    const uint16_t tileID = spr->TileIndex;
    const uint16_t sprTileAddr = GetBaseSPRPatternTableAddress() + (tileID * 0x10);
    const uint8_t paletteID = 0b00000011 & spr->Attributes;
    const uint16_t paletteAddr = PaletteRAMIndeces_Start + ((paletteID + 4) * 4);

    bool flipH = CheckBit(spr->Attributes, SPRAttrPos_FlipH);
    bool flipV = CheckBit(spr->Attributes, SPRAttrPos_FlipV);

    for (size_t row = 0; row < 8; row++) {
        if ((actualPosY + row) > 0xEFU) {
            break;
        }

        for (size_t col = 0; col < 8; col++) {
            const uint16_t sprOffset = sprTileAddr + row;

            // Pixel defines which colour value it should have from the palette, 0 - 3
            const uint8_t pixel = ((PPURead(sprOffset) >> (7 - (col % 8))) & 1) + (((PPURead(sprOffset + 8) >> (7 - (col % 8))) & 1) * 2);
            const uint32_t paletteValue = Palette_NTSC[PPURead(paletteAddr + pixel)];

            uint32_t bufferIndex;
            uint16_t spriteXOverflow; // To catch attempts at drawing at X > 255, value is stored in a 16-bit integer first
            uint8_t spriteX; // Dot to draw the pixel on
            uint8_t spriteY; // Scanline to draw the pixel on

            if (!flipH) {
                spriteXOverflow = spr->PositionX + col;
            }
            else {
                // If flipped, draw the sprite right to left
                spriteXOverflow = (spr->PositionX + 7) - col;
            }

            if (!flipV) {
                spriteY = actualPosY + row;
            }
            else {
                // If flipped, draw the sprite upside down
                spriteY = (actualPosY + 7) - row;
            }

            // If pixel would be drawn below the screen, stop drawing
            if (spriteY > 0xEFU) {
                break;
            }

            // If pixel would be drawn out of bounds to the right (onto the next scanline from the left), don't, and try the next pixel
            // If flipped horizontally, valid pixels might occur in a later loop (drawing right to left), so continue instead of break
            if (spriteXOverflow > 0xFFU) {
                continue;
            }

            spriteX = (uint8_t)spriteXOverflow;

            bufferIndex = (spriteY * 256 * 4) + (spriteX * 4);

            /*
            if (bufferIndex > (256*240*4)) {
                printf("Scanline: %u, Index: %u, FlipH: %u, FlipV: %u\n", actualPosY + row, bufferIndex, flipH, flipV);
            }
            */

            if (pixel) {
                SPRFrameBuffer[bufferIndex] = (paletteValue >> 16) & 0xFF;
                SPRFrameBuffer[bufferIndex + 1] = (paletteValue >> 8) & 0xFF;
                SPRFrameBuffer[bufferIndex + 2] = (paletteValue) & 0xFF;
                SPRFrameBuffer[bufferIndex + 3] = 0xFF;
            }
        }
    }
}

void WriteStateLog(uint8_t inst) {
    States[StateIndex].OpCode = inst;
    States[StateIndex].Acc = CCPU->Accumulator;
    States[StateIndex].RegX = CCPU->RegX;
    States[StateIndex].RegY = CCPU->RegY;
    States[StateIndex].Addr = CCPU->PC - 1;

    StateIndex++;

    if (StateIndex == 16) {
        StateIndex = 0;
    }
}

void DumpStateLog(size_t result) {
    FILE* log = fopen("log.txt", "w");
    fprintf(log, "");
    fclose(log);
    
    log = fopen("log.txt", "a");

    size_t i = StateIndex + 1;
    size_t n = 0;

    fprintf(log, "Result code: %u\nCall stack, oldest to newest\n\n", result);

    while (n < 16) {
        if (i == 16) {
            i = 0;
        }

        fprintf(log, "Addr: %04X    Opcode: %02X    A: %02X  X: %02X  Y: %02X\n", States[i].Addr, States[i].OpCode, States[i].Acc, States[i].RegX, States[i].RegY);
        n++;
        i++;
    }
    
    fclose(log);
}
