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
    if (System == SYS_NTSC) {
        CPUTimeStamp -= (NumCPUCycles_NTSC * CPUCycleDivider_NTSC);
        CPUCycleCount -= NumCPUCycles_NTSC;

        PPUTimeStamp -= (NumCPUCycles_NTSC * CPUCycleDivider_NTSC);
        PPUCycleCount -= (NumCPUCycles_NTSC * (PPUFinalDivider_NTSC));
    }
    else if (System == SYS_PAL) {
        CPUTimeStamp -= (NumCPUCycles_PAL * CPUCycleDivider_PAL);
        CPUCycleCount -= NumCPUCycles_PAL;

        PPUTimeStamp -= (NumCPUCycles_PAL * CPUCycleDivider_PAL);
        PPUCycleCount -= (NumCPUCycles_PAL * (PPUFinalDivider_PAL));
    }
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

bool IsRenderingEnabled() {
    if (!BGRenderingEnabled && !SPRRenderingEnabled) {
        return false;
    }
    
    return true;
}

void TriggerBankSwitch(uint16_t addr, uint8_t value) {
    if (CurROM->MapperNumber == (uint16_t)Map_AxROM) {
        uint8_t bankNum = value & 0b111;
        size_t offset = (0x8000U * bankNum);
        memcpy(&CPUMemory[ROM_Start], &ROM_PRG[offset], 0x8000U);

        uint8_t ntNum = (value >> 4) & 1;
        PPUAXSwapNT(ntNum);
    }
}

void RunCPU(uint32_t timestamp) {
    //printf("CPU timestamp: %u\n", timestamp);
    if (CPUTimeStamp >= timestamp) {
        return;
    }

    while (CPUTimeStamp < timestamp && !StopExecution) {
        if (!NMIOccured && QueueNMI) {
            NMIOccured = true;
            QueueNMI = false;
            TriggerNMI();
        }

        uint8_t instruction = ReadProgramByte();
        WriteStateLog(instruction);
        //printf("Addr: %04X, Instruction: %02X.    Next two bytes: %02X, %02X.    A: %02X, X: %02X, Y: %02X, Status: %02X\n", CCPU->PC - 1, instruction, CPUMemory[CCPU->PC], CPUMemory[CCPU->PC + 1], CCPU->Accumulator, CCPU->RegX, CCPU->RegY, CCPU->Status);

        /*
        if (CCPU->PC - 1 >= 0xCB57 && CCPU->PC - 1 <= 0xCB76) {
            printf("Addr: %04X, Instruction: %02X.    Next two bytes: %02X, %02X.    A: %02X, X: %02X, Y: %02X, Status: %02X. V: %04X\n", CCPU->PC - 1, instruction, CPUMemory[CCPU->PC], CPUMemory[CCPU->PC + 1], CCPU->Accumulator, CCPU->RegX, CCPU->RegY, CCPU->Status, CurPPU->RegV);
            //printf("A: %02X, X: %02X, Y: %02X, Status: %02X\n", CCPU->Accumulator, CCPU->RegX, CCPU->RegY, CCPU->Status);
        }
        */
        
        ExecuteInstruction(instruction);
        RunPPU(CPUTimeStamp);
    }
}

void RunPPU(uint32_t timestamp) {
    if (PPUTimeStamp >= timestamp) {
        return;
    }

    while (PPUTimeStamp < timestamp) {
        if (BGRenderFlagCountdown > 0) {
            BGRenderFlagCountdown--;
        }
        else if (BGRenderFlagCountdown == 0) {
            BGRenderFlagCountdown = -1;
            BGRenderingEnabled = PendingBGRenderFlag;
        }

        if (SPRRenderFlagCountdown > 0) {
            SPRRenderFlagCountdown--;
        }
        else if (SPRRenderFlagCountdown == 0) {
            SPRRenderFlagCountdown = -1;
            SPRRenderingEnabled = PendingSPRRenderFlag;
        }

        // End of VBlank
        if (CurScanline == -1) {
            if (CurDot == 1) {
                OverrideBit8(CurPPU->PPUSTATUS, PPUSTATUS_VBlank, 0);
                OverrideBit8(CurPPU->PPUSTATUS, PPUSTATUS_Sprite0Hit, 0);
            }
            else if (CurDot >= 280 && CurDot < 305 && IsRenderingEnabled()) {
                uint16_t v = CurPPU->RegV;
                uint16_t t = CurPPU->RegT;

                v &= 0b0000010000011111;
                t &= 0b0111101111100000;

                v |= t;

                CurPPU->RegV = v;
            }
        }

        if (CurScanline == 0 && CurDot == 0) {
            ProcessSPR0();
        }

        // Drawing loop
        if (CurScanline >= 0 && CurScanline <= 239) {
            /*
            if (CurScanline == 30 && CurDot == 91) {
                if (BGRenderingEnabled && SPRRenderingEnabled) {
                    OverrideBit8(CurPPU->PPUSTATUS, PPUSTATUS_Sprite0Hit, 1);
                }
            }
            */

            if (CurDot != 0 && CurDot < 257) {
                DrawBGPixelV((uint8_t)(CurDot - 1), (uint8_t)CurScanline);
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

        if (CurScanline < 240) {
            if (CurDot == 256) {
                if (IsRenderingEnabled()) {
                    PPUIncFineY();
                }
            }

            //if (CurDot != 0 && CurDot % 8 == 0 && (CurDot <= 256 || CurDot >= 328)) {
            if (CurDot != 0 && CurDot % 8 == 0 && CurDot <= 256) {
                if (IsRenderingEnabled()) {
                    PPUIncCoarseX();
                }
            }

            if (CurDot == 257) {
                if (IsRenderingEnabled()) {
                    uint16_t temp = CurPPU->RegV;
                    OverrideBit16(&temp, 10, CheckBit(GetHighByte(CurPPU->RegT), 2));
                    temp >>= 5;
                    temp <<= 5;

                    uint16_t other = 0b00011111 & CurPPU->RegT;
                    temp += other;

                    CurPPU->RegV = temp;
                }
            }
        }

        CurDot++;
        UsePPUCycles(1U);

        if (CurDot >= Scanline_Length) {
            CurDot -= Scanline_Length;
            CurScanline++;

            if (System == SYS_NTSC && CurScanline == (Scanlines_NTSC - 1)) {
                CurScanline = -1;
            }
            else if (System == SYS_PAL && CurScanline == (Scanlines_PAL - 1)) {
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

void DrawBGPixelV(uint8_t x, uint8_t y) {
    if (x < 8 && !CheckBit(*CurPPU->PPUMASK, 1)) {
        BGFrameBuffer[(y * 256 * 3) + (x * 3)] = (Palette_NTSC[PPURead(0x3F00U)] >> 16) & 0xFF;
        BGFrameBuffer[(y * 256 * 3) + (x * 3) + 1] = (Palette_NTSC[PPURead(0x3F00U)] >> 8) & 0xFF;
        BGFrameBuffer[(y * 256 * 3) + (x * 3) + 2] = (Palette_NTSC[PPURead(0x3F00U)]) & 0xFF;
        return;
    }

    // How many tiles we're offset in either direction
    const uint8_t coarseX = (uint8_t)(CurPPU->RegV & 0b11111);
    const uint8_t coarseY = (uint8_t)((CurPPU->RegV >> 5) & 0b11111);
    const uint8_t fineY = (uint8_t)((CurPPU->RegV >> 12) & 0b111);

    // Are we scrolling partially through a tile on the X-axis? (called crossing here)
    bool crossingX = ((x & 7) + CurPPU->RegX) > 7;
    //bool crossingY = ((y & 7) + fineY) > 7;
    bool crossingY = false;

    uint16_t tileAddr;
    uint16_t attrAddr;

    // If crossing X, grab the next tile if we've reached it
    if (crossingX) {
        const uint16_t simV = SimulateIncCoarseX();
        tileAddr = GetOffsetTileAddress(simV);
        attrAddr = GetOffsetAttributeAddress(simV);
    }
    /*
    else if (crossingY) {
        const uint16_t simV = SimulateIncCoarseY();
        tileAddr = GetOffsetTileAddress(simV);
        attrAddr = GetOffsetAttributeAddress(simV);
    }
    */
    else {
        tileAddr = GetTileAddress();
        attrAddr = GetAttributeAddress();
    }
    
    const uint8_t tileVal = PPURead(tileAddr);
    const uint8_t attrVal = PPURead(attrAddr);

    const uint16_t patternAddr = GetBaseBGPatternTableAddress() + (tileVal * 0x10) + (fineY % 8); // Changed y to fineY for Ice Climber

    uint8_t attrRegX;
    uint8_t attrRegY;

    // Also grab the appropriate attribute if crossing
    if (crossingX) {
        attrRegX = (coarseX + 1) & 2 ? 1 : 0;
    }
    else {
        attrRegX = coarseX & 2 ? 1 : 0;
    }

    
    if (crossingY) {
        attrRegY = (coarseY + 1) & 2 ? 1 : 0;
    }
    else {
        attrRegY = coarseY & 2 ? 1 : 0;
    }
    

    //attrRegY = coarseY & 2 ? 1 : 0;

    // Which quadrant is the pixel in? 0 = top left, 1 = top right, 2 = bottom left, 3 = bottom right
    const uint8_t attrIndex = attrRegX + (attrRegY * 2);

    uint8_t attrPartIndex = 0;

    switch (attrIndex) {
        case 0:
            attrPartIndex = attrVal & 0b00000011;
            break;

        case 1:
            attrPartIndex = (attrVal & 0b00001100) >> 2;
            break;

        case 2:
            attrPartIndex = (attrVal & 0b00110000) >> 4;
            break;

        case 3:
            attrPartIndex = (attrVal & 0b11000000) >> 6;
            break;
        
        default:
            break;
    }

    // Grabs the appropriate bitplane from the pattern table and gets the colour palette index value of the desired pixel in the tile we're drawing
    const uint8_t pixel = ((PPURead(patternAddr) >> (7 - ((x + CurPPU->RegX % 8) % 8))) & 1) + (((PPURead(patternAddr + 8) >> (7 - ((x + CurPPU->RegX % 8) % 8))) & 1) * 2);

    uint16_t paletteIndex = PaletteRAMIndeces_Start + (attrPartIndex * 4) + pixel;

    /*
    if (x > 235 && x % 4 == 0 && y > 206 && y < 226) {
        printf("X: %02u, Y: %03u, FX: %u, V: %04X, TA: %04X, TV: %02X, AA: %04X, AV: %02X, PA: %04X. CY: %02u, FY: %02u\n", x, y, CurPPU->RegX, CurPPU->RegV, tileAddr, tileVal, attrAddr, attrVal, patternAddr, coarseY, fineY);
        printf("Pixel: %u, AI: %02X, PI: %02X\n", pixel, attrIndex, attrPartIndex);
    }
    */

    // Needs some tweaks to work with sprite priority
    if (paletteIndex % 4 == 0) {
        paletteIndex = 0x3F00U;
    }

    BGFrameBuffer[(y * 256 * 3) + (x * 3)] = (Palette_NTSC[PPURead(paletteIndex)] >> 16) & 0xFF;
    BGFrameBuffer[(y * 256 * 3) + (x * 3) + 1] = (Palette_NTSC[PPURead(paletteIndex)] >> 8) & 0xFF;
    BGFrameBuffer[(y * 256 * 3) + (x * 3) + 2] = (Palette_NTSC[PPURead(paletteIndex)]) & 0xFF;

    if (!SPR0Data.HasHit && pixel != 0) {
        CheckSPR0Hit(x, y);
    }
}

void CheckSPR0Hit(uint8_t x, uint8_t y) {
    if (!BGRenderingEnabled || !SPRRenderingEnabled) {
        return;
    }

    int16_t row = y;
    row -= SPR0Data.y;

    int16_t col = x;
    col -= SPR0Data.x;

    if (row < 8 && row >= 0 && col < 8 && col >= 0) {
        if (SPR0Data.PixelData[(row * 8) + col] == 1) {
            SPR0Data.HasHit = true;
            OverrideBit8(CurPPU->PPUSTATUS, PPUSTATUS_Sprite0Hit, 1);
        }
    }
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

    SpriteData* spr = (SpriteData*)&CurPPU->OAM[252];
    
    for (size_t i = 0; i < 64; i++) {
        if (spr->PositionY < 0xEFU) {
            DrawSPR(spr);
        }
        spr--;
    }
}

void ProcessSPR0() {
    SpriteData* spr0 = (SpriteData*)CurPPU->OAM;

    uint16_t ntBaseAddr;
    const bool isBigSprite = CheckBit(*CurPPU->PPUCTRL, PPUCTRL_SpriteSize);

    if (isBigSprite) {
        ntBaseAddr = CheckBit(spr0->TileIndex, 0U);
    }
    else {
        ntBaseAddr = GetBaseNameTableAddress();
    }

    const uint8_t actualPosY = spr0->PositionY + 1;

    const uint16_t tileID = spr0->TileIndex;
    const uint16_t sprTileAddr = GetBaseSPRPatternTableAddress() + (tileID * 0x10);
    //const uint8_t paletteID = 0b00000011 & spr0->Attributes;
    //const uint16_t paletteAddr = PaletteRAMIndeces_Start + ((paletteID + 4) * 4);

    bool flipH = CheckBit(spr0->Attributes, SPRAttrPos_FlipH);
    bool flipV = CheckBit(spr0->Attributes, SPRAttrPos_FlipV);

    SPR0Data.x = spr0->PositionX;
    SPR0Data.y = actualPosY;
    SPR0Data.HasHit = false;

    for (size_t row = 0; row < 8; row++) {
        if ((actualPosY + row) > 0xEFU) {
            break;
        }

        for (size_t col = 0; col < 8; col++) {
            const uint16_t sprOffset = sprTileAddr + row;

            // Pixel defines which colour value it should have from the palette, 0 - 3
            const uint8_t pixel = ((PPURead(sprOffset) >> (7 - (col % 8))) & 1) + (((PPURead(sprOffset + 8) >> (7 - (col % 8))) & 1) * 2);
            //const uint32_t paletteValue = Palette_NTSC[PPURead(paletteAddr + pixel)];

            //uint32_t bufferIndex;
            uint16_t spriteXOverflow; // To catch attempts at drawing at X > 255, value is stored in a 16-bit integer first
            uint8_t spriteX; // Dot to draw the pixel on
            uint8_t spriteY; // Scanline to draw the pixel on

            uint8_t pixDataX = col;
            uint8_t pixDataY = row;

            if (!flipH) {
                spriteXOverflow = spr0->PositionX + col;
            }
            else {
                // If flipped, draw the sprite right to left
                spriteXOverflow = (spr0->PositionX + 7) - col;
                pixDataX = 7 - col;
            }

            // If pixel would be drawn out of bounds to the right (onto the next scanline from the left), don't, and try the next pixel
            // If flipped horizontally, valid pixels might occur in a later loop (drawing right to left), so continue instead of break
            if (spriteXOverflow > 0xFFU) {
                SPR0Data.PixelData[(pixDataY * 8) + pixDataX] = 0;
                continue;
            }

            spriteX = (uint8_t)spriteXOverflow;

            if (spriteX < 8 && !CheckBit(*CurPPU->PPUMASK, 2)) {
                SPR0Data.PixelData[(pixDataY * 8) + pixDataX] = 0;
                continue;
            }

            if (!flipV) {
                spriteY = actualPosY + row;
            }
            else {
                // If flipped, draw the sprite upside down
                spriteY = (actualPosY + 7) - row;
                pixDataY = 7 - row;
            }

            // If pixel would be drawn below the screen, stop drawing
            if (spriteY > 0xEFU) {
                SPR0Data.PixelData[(pixDataY * 8) + pixDataX] = 0;
                break;
            }

            //bufferIndex = (spriteY * 256 * 4) + (spriteX * 4);

            /*
            if (bufferIndex > (256*240*4)) {
                printf("Scanline: %u, Index: %u, FlipH: %u, FlipV: %u\n", actualPosY + row, bufferIndex, flipH, flipV);
            }
            */

            if (pixel) {
                //SPRFrameBuffer[bufferIndex] = (paletteValue >> 16) & 0xFF;
                //SPRFrameBuffer[bufferIndex + 1] = (paletteValue >> 8) & 0xFF;
                //SPRFrameBuffer[bufferIndex + 2] = (paletteValue) & 0xFF;
                //SPRFrameBuffer[bufferIndex + 3] = 0xFF;

                SPR0Data.PixelData[(pixDataY * 8) + pixDataX] = 1;
            }
            else {
                SPR0Data.PixelData[(pixDataY * 8) + pixDataX] = 0;
            }
        }
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

            // If pixel would be drawn out of bounds to the right (onto the next scanline from the left), don't, and try the next pixel
            // If flipped horizontally, valid pixels might occur in a later loop (drawing right to left), so continue instead of break
            if (spriteXOverflow > 0xFFU) {
                continue;
            }

            spriteX = (uint8_t)spriteXOverflow;

            if (spriteX < 8 && !CheckBit(*CurPPU->PPUMASK, 2)) {
                continue;
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
