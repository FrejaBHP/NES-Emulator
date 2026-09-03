#include <apu.h>
#include <console.h>
#include <cpu.h>
#include <stdbool.h>

#define SQ_VOLPos_Vol0              0U
#define SQ_VOLPos_Vol1              1U
#define SQ_VOLPos_Vol2              2U
#define SQ_VOLPos_Vol3              3U
#define SQ_VOLPos_CVol              4U
#define SQ_VOLPos_Loop              5U
#define SQ_VOLPos_Duty0             6U
#define SQ_VOLPos_Duty1             7U

#define SQ_SWEEPPos_Shift0          0U
#define SQ_SWEEPPos_Shift1          1U
#define SQ_SWEEPPos_Shift2          2U
#define SQ_SWEEPPos_Negate          3U
#define SQ_SWEEPPos_Period0         4U
#define SQ_SWEEPPos_Period1         5U
#define SQ_SWEEPPos_Period2         6U
#define SQ_SWEEPPos_Enable          7U

#define FC_4Step_0_NTSC             3728    // Quarter
#define FC_4Step_1_NTSC             7456    // Quarter + Half
#define FC_4Step_2_NTSC             11185   // Quarter
#define FC_4Step_3_NTSC             14914   // Quarter + Half, set Frame Interrupt Flag if InhibitInterrupt is set

#define FC_4Step_0_PAL              4156    // Quarter
#define FC_4Step_1_PAL              8313    // Quarter + Half
#define FC_4Step_2_PAL              12469   // Quarter
#define FC_4Step_3_PAL              16626   // Quarter + Half, set Frame Interrupt Flag if InhibitInterrupt is set

#define FC_5Step_0_NTSC             3728    // Quarter
#define FC_5Step_1_NTSC             7456    // Quarter + Half
#define FC_5Step_2_NTSC             11185   // Quarter
#define FC_5Step_3_NTSC             14914
#define FC_5Step_4_NTSC             18640   // Quarter + Half

#define FC_5Step_0_PAL              4156    // Quarter
#define FC_5Step_1_PAL              8313    // Quarter + Half
#define FC_5Step_2_PAL              12469   // Quarter
#define FC_5Step_3_PAL              16626
#define FC_5Step_4_PAL              20782   // Quarter + Half


SDL_AudioDeviceID AudioDevice = 0;
SDL_AudioStream* Stream = NULL;

APUStatus APU_Status = { 0 };
PulseChannel ST_SQ[2] = { 0 };
TriangleChannel ST_TRI = { 0 };
NoiseChannel ST_NOISE = { 0 };
FrameCounter FC = { 0 };

bool EvenTick = false;
bool FCShouldReset = false;
uint8_t FCResetCountdown = 0;

float IgnoreCounterF = 0.f;
uint8_t IgnoreCounter = 0;

int16_t* SQBuffers[2];

const uint8_t DutyTable[4][8] = {
    { 0, 1, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 1, 0, 0, 0, 0, 0 },
    { 0, 1, 1, 1, 1, 0, 0, 0 },
    { 1, 0, 0, 1, 1, 1, 1, 1 }
};

const uint8_t LengthTable[32] = {
    10, 254, 20, 2, 40, 4, 80, 6, 
    160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22,
    192, 24, 72, 26, 16, 28, 32, 30
};

const uint16_t NoiseTable_NTSC[16] = {
    4, 8, 16, 32, 64, 96, 128, 160,
    202, 254, 380, 508, 762, 1016, 2034, 4068
};

const uint16_t NoiseTable_PAL[16] = {
    4, 8, 14, 30, 60, 88, 118, 148,
    188, 236, 354, 472, 708, 944, 1890, 3778
};

void APUInit() {
    SDL_AudioSpec aspec;

    aspec.freq = SampleRate;
    aspec.format = SDL_AUDIO_S16;
    aspec.channels = 1;

    Stream = SDL_CreateAudioStream(&aspec, NULL);
    SDL_BindAudioStream(AudioDevice, Stream);

    SoundBuffer = calloc(1, sizeof(int16_t) * 800);

    if (System == SYS_NTSC) {
        IgnoreCounterF = APUSampleDivider_NTSC;
    }
    else {
        IgnoreCounterF = APUSampleDivider_PAL;
    }

    ST_NOISE.ShiftRegister = 1;
}

void ClockAPU() {
    if (!EvenTick) {
        EvenTick = true;
        TickTriangle();
        return;
    }

    IgnoreCounter--;
    IgnoreCounterF--;

    TickPulse();
    TickTriangle();
    TickNoise();
    TickFC();

    if (IgnoreCounter == 0 && SampleCounter < 800) {
        OutputPulse();
        OutputTriangle();
        OutputNoise();
        SampleCounter++;
    }
    
    EvenTick = false;

    if (IgnoreCounter == 0) {
        if (System == SYS_NTSC) {
            IgnoreCounterF += APUSampleDivider_NTSC;
        }
        else {
            IgnoreCounterF += APUSampleDivider_PAL;
        }
        
        IgnoreCounter = (uint8_t)IgnoreCounterF;
    }
}

void TickPulse() {
    if (ST_SQ[0].PeriodCounter > 0) {
        ST_SQ[0].PeriodCounter--;
    }
    else {
        ST_SQ[0].PeriodCounter = ST_SQ[0].Period;
        ST_SQ[0].DutyCounter = (ST_SQ[0].DutyCounter + 1) & 7;
    }

    if (ST_SQ[1].PeriodCounter > 0) {
        ST_SQ[1].PeriodCounter--;
    }
    else {
        ST_SQ[1].PeriodCounter = ST_SQ[1].Period;
        ST_SQ[1].DutyCounter = (ST_SQ[1].DutyCounter + 1) & 7;
    }
}

void OutputPulse() {
    int16_t pulse0Out = 0;
    int16_t pulse1Out = 0;

    if (DutyTable[ST_SQ[0].Duty][ST_SQ[0].DutyCounter] && ST_SQ[0].LengthCounter != 0 && !IsSweepForcingSilence(&ST_SQ[0])) {
        if (ST_SQ[0].UseConstantVolume) {
            pulse0Out = ST_SQ[0].Volume * 35;
        }
        else {
            pulse0Out = ST_SQ[0].DecayVolume * 35;
        }
    }

    if (DutyTable[ST_SQ[1].Duty][ST_SQ[1].DutyCounter] && ST_SQ[1].LengthCounter != 0 && !IsSweepForcingSilence(&ST_SQ[1])) {
        if (ST_SQ[1].UseConstantVolume) {
            pulse1Out = ST_SQ[1].Volume * 35;
        }
        else {
            pulse1Out = ST_SQ[1].DecayVolume * 35;
        }
    }
    
    SoundBuffer[SampleCounter] = pulse0Out + pulse1Out;
}

void WriteToSQ(uint8_t value, uint8_t byte, uint8_t ch) {
    switch (byte) {
        case 0: // SQ_VOL
            const uint8_t duty = value >> 6;
            ST_SQ[ch].Duty = duty;

            ST_SQ[ch].FreezeLCLoopEnvelope = CheckBit(value, 5);
            ST_SQ[ch].UseConstantVolume = CheckBit(value, 4);
            ST_SQ[ch].Volume = 0b1111 & value;
            break;

        case 1: // SQ_SWEEP
            ST_SQ[ch].SweepEnabled = CheckBit(value, 7);
            ST_SQ[ch].DividerPeriod = (0b01110000 & value) >> 4;
            ST_SQ[ch].IsNegated = CheckBit(value, 3);
            ST_SQ[ch].ShiftCount = 0b111 & value;

            ST_SQ[ch].ReloadSweep = true;
            break;

        case 2: // SQ_LO
            UpdateSQTimer(ch);
            break;

        case 3: // SQ_HI
            UpdateSQTimer(ch);
            ST_SQ[ch].PeriodCounter = ST_SQ[ch].Period;
            ST_SQ[ch].DutyCounter = 0;

            const bool enabled = ch ? APU_Status.Pulse1Enabled : APU_Status.Pulse0Enabled;
            
            if (enabled) {
                const uint8_t lc = value >> 3;
                ST_SQ[ch].LengthCounter = LengthTable[lc];
            }

            ST_SQ[ch].ResetDecay = true;
            break;
    
        default:
            break;
    }
}

void UpdateSQTimer(uint8_t num) {
    uint8_t low;
    uint8_t high;

    if (num == 0) {
        low = CPUMemory[SQ1_LO];
        high = CPUMemory[SQ1_HI];
    }
    else {
        low = CPUMemory[SQ2_LO];
        high = CPUMemory[SQ2_HI];
    }

    high &= 0b00000111;
    ST_SQ[num].Period = (high << 8) | low;
}

bool IsSweepForcingSilence(PulseChannel* ch) {
    if (ch->Period < 8) {
        return true;
    }
    else if (!ch->IsNegated && ch->Period + (ch->Period >> ch->ShiftCount) >= 0x800) {
        return true;
    }
    else {
        return false;
    }
}


void TickTriangle() {
    bool ultrasonic = false;
    if (ST_TRI.Period < 2 && ST_TRI.PeriodCounter == 0) {
        ultrasonic = true;
    }

    bool clockUnit = true;
    if (!ST_TRI.LengthCounter || !ST_TRI.LinearCounter || ultrasonic) {
        clockUnit = false;
    }

    if (clockUnit) {
        if (ST_TRI.PeriodCounter > 0) {
            ST_TRI.PeriodCounter--;
        }
        else {
            ST_TRI.PeriodCounter = ST_TRI.Period;
            ST_TRI.Step = (ST_TRI.Step + 1) & 0x1F;
        }
    }
}

void OutputTriangle() {
    uint8_t triOutput = 0;

    bool ultrasonic = false;
    if (ST_TRI.Period < 2 && ST_TRI.PeriodCounter == 0) {
        ultrasonic = true;
    }

    if (ultrasonic) {
        triOutput = 7;
    }
    else if (ST_TRI.Step & 0x10) {
        triOutput = ST_TRI.Step ^ 0x1F;
    }
    else {
        triOutput = ST_TRI.Step;
    }

    SoundBuffer[SampleCounter] += triOutput * 25;
}

void WriteToTRI(uint8_t value, uint8_t byte) {
    switch (byte) {
        case 0: // TRI_LINEAR
            ST_TRI.FreezeLCLinearControl = CheckBit(value, 7);
            ST_TRI.LinearCounterLoad = 0b01111111 & value;
            break;

        case 1: // TRI_UNUSED
            break;

        case 2: // TRI_LO
            UpdateTRITimer();
            break;

        case 3: // TRI_HI
            UpdateTRITimer();

            if (APU_Status.TriangleEnabled) {
                const uint8_t lc = value >> 3;
                ST_TRI.LengthCounter = LengthTable[lc];
            }

            ST_TRI.ReloadLinear = true;
            break;
    
        default:
            break;
    }
}

void UpdateTRITimer() {
    const uint8_t low = CPUMemory[TRI_LO];
    const uint8_t high = 0b00000111 & CPUMemory[TRI_HI];

    ST_TRI.Period = (high << 8) | low;
}


void TickNoise() {
    if (ST_NOISE.PeriodCounter > 0) {
        ST_NOISE.PeriodCounter--;
    }
    else {
        ST_NOISE.PeriodCounter = ST_NOISE.Period;

        if (ST_NOISE.NoiseMode) {
            OverrideBit16(&ST_NOISE.ShiftRegister, 15, (CheckBit(GetLowByte(ST_NOISE.ShiftRegister), 6)) ^ (CheckBit(GetLowByte(ST_NOISE.ShiftRegister), 0)));
        }
        else {
            OverrideBit16(&ST_NOISE.ShiftRegister, 15, (CheckBit(GetLowByte(ST_NOISE.ShiftRegister), 1)) ^ (CheckBit(GetLowByte(ST_NOISE.ShiftRegister), 0)));
        }

        ST_NOISE.ShiftRegister >>= 1;
    }
}

void OutputNoise() {
    uint8_t noiseOutput = 0;

    // Output when noise is low, opposite of other channels
    if (!CheckBit(GetLowByte(ST_NOISE.ShiftRegister), 0) && ST_NOISE.LengthCounter > 0) {
        if (ST_NOISE.UseConstantVolume) {
            noiseOutput = ST_NOISE.Volume;
        }
        else {
            noiseOutput = ST_NOISE.DecayVolume;
        }
    }

    SoundBuffer[SampleCounter] += noiseOutput * 20;
}

void WriteToNOISE(uint8_t value, uint8_t byte) {
    switch (byte) {
        case 0: // NOISE_VOL
            ST_NOISE.FreezeLCLoopEnvelope = CheckBit(value, 5);
            ST_NOISE.UseConstantVolume = CheckBit(value, 4);
            ST_NOISE.Volume = 0b1111 & value;
            break;

        case 1: // NOISE_UNUSED
            break;

        case 2: // NOISE_LO
            ST_NOISE.NoiseMode = CheckBit(value, 7);

            const uint8_t ptv = 0b00001111 & value;
            if (System == SYS_NTSC) {
                ST_NOISE.Period = NoiseTable_NTSC[ptv];
            }
            else {
                ST_NOISE.Period = NoiseTable_PAL[ptv];
            }
            
            break;

        case 3: // NOISE_HI
            if (APU_Status.NoiseEnabled) {
                const uint8_t lc = value >> 3;
                ST_NOISE.LengthCounter = LengthTable[lc];
            }

            ST_NOISE.ResetDecay = true;
            break;
    
        default:
            break;
    }
}


void WriteToStatus(uint8_t value) {
    const uint8_t sndchn = 0b11100000 & CPUMemory[SND_CHN];
    const uint8_t write = 0b00011111 & value;

    CPUMemory[SND_CHN] = sndchn | write;

    bool* status = (bool*)&APU_Status;
    for (size_t i = 0; i < 5; i++) {
        status[i] = CheckBit(value, i);
    }

    if (!APU_Status.Pulse0Enabled) {
        ST_SQ[0].LengthCounter = 0;
        uint8_t thing = CPUMemory[SQ1_HI];
        thing &= 0b111;
        CPUMemory[SQ1_HI] = thing;
    }

    if (!APU_Status.Pulse1Enabled) {
        ST_SQ[1].LengthCounter = 0;
        uint8_t thing = CPUMemory[SQ2_HI];
        thing &= 0b111;
        CPUMemory[SQ2_HI] = thing;
    }

    if (!APU_Status.TriangleEnabled) {
        ST_TRI.LengthCounter = 0;
        uint8_t thing = CPUMemory[TRI_HI];
        thing &= 0b111;
        CPUMemory[TRI_HI] = thing;
    }

    if (!APU_Status.NoiseEnabled) {
        ST_NOISE.LengthCounter = 0;
        uint8_t thing = CPUMemory[NOISE_HI];
        thing &= 0b111;
        CPUMemory[NOISE_HI] = thing;
    }
}

void TickFC() {
    FC.SequenceCounter++;

    if (System == SYS_NTSC) {
        if (!FC.Is5StepMode) {
            if (FC.SequenceCounter == (FC_4Step_3_NTSC + 1)) {
                FC.SequenceCounter = 0;
            }
        }
        else {
            if (FC.SequenceCounter == (FC_5Step_4_NTSC + 1)) {
                FC.SequenceCounter = 0;
            }
        }
    }
    else {
        if (!FC.Is5StepMode) {
            if (FC.SequenceCounter == (FC_4Step_3_PAL + 1)) {
                FC.SequenceCounter = 0;
            }
        }
        else {
            if (FC.SequenceCounter == (FC_5Step_4_PAL + 1)) {
                FC.SequenceCounter = 0;
            }
        }
    }

    if (FCShouldReset) {
        FCResetCountdown--;

        if (!FCResetCountdown) {
            FCShouldReset = false;
            FC.SequenceCounter = 0;
        }
    }

    if (System == SYS_NTSC) {
        if (!FC.Is5StepMode) {
            if (FC.SequenceCounter == FC_4Step_0_NTSC) {
                FCQuarterFrame();
            }
            else if (FC.SequenceCounter == FC_4Step_1_NTSC) {
                FCQuarterFrame();
                FCHalfFrame();
            }
            else if (FC.SequenceCounter == FC_4Step_2_NTSC) {
                FCQuarterFrame();
            }
            else if (FC.SequenceCounter == FC_4Step_3_NTSC) {
                FCQuarterFrame();
                FCHalfFrame();
            }
        }
        else {
            if (FC.SequenceCounter == FC_5Step_0_NTSC) {
                FCQuarterFrame();
            }
            else if (FC.SequenceCounter == FC_5Step_1_NTSC) {
                FCQuarterFrame();
                FCHalfFrame();
            }
            else if (FC.SequenceCounter == FC_5Step_2_NTSC) {
                FCQuarterFrame();
            }
            else if (FC.SequenceCounter == FC_5Step_4_NTSC) {
                FCQuarterFrame();
                FCHalfFrame();
            }
        }
    }
    else {
        if (!FC.Is5StepMode) {
            if (FC.SequenceCounter == FC_4Step_0_PAL) {
                FCQuarterFrame();
            }
            else if (FC.SequenceCounter == FC_4Step_1_PAL) {
                FCQuarterFrame();
                FCHalfFrame();
            }
            else if (FC.SequenceCounter == FC_4Step_2_PAL) {
                FCQuarterFrame();
            }
            else if (FC.SequenceCounter == FC_4Step_3_PAL) {
                FCQuarterFrame();
                FCHalfFrame();
            }
        }
        else {
            if (FC.SequenceCounter == FC_5Step_0_PAL) {
                FCQuarterFrame();
            }
            else if (FC.SequenceCounter == FC_5Step_1_PAL) {
                FCQuarterFrame();
                FCHalfFrame();
            }
            else if (FC.SequenceCounter == FC_5Step_2_PAL) {
                FCQuarterFrame();
            }
            else if (FC.SequenceCounter == FC_5Step_4_PAL) {
                FCQuarterFrame();
                FCHalfFrame();
            }
        }
    }
}

void FCQuarterFrame() {
    for (size_t i = 0; i < 2; i++) {
        if (ST_SQ[i].ResetDecay) {
            ST_SQ[i].ResetDecay = false;
            ST_SQ[i].DecayVolume = 0xF;
            ST_SQ[i].DecayCounter = ST_SQ[i].Volume;
        }
        else {
            if (ST_SQ[i].DecayCounter > 0) {
                ST_SQ[i].DecayCounter--;
            }
            else {
                ST_SQ[i].DecayCounter = ST_SQ[i].DecayVolume;

                if (ST_SQ[i].DecayVolume > 0) {
                    ST_SQ[i].DecayVolume--;
                }
                else if (ST_SQ[i].FreezeLCLoopEnvelope) {
                    ST_SQ[i].DecayVolume = 0xF;
                }
            }
        }
    }

    if (ST_TRI.ReloadLinear) {
        ST_TRI.LinearCounter = ST_TRI.LinearCounterLoad;
    }
    else if (ST_TRI.LinearCounter > 0) {
        ST_TRI.LinearCounter--;
    }

    if (!ST_TRI.FreezeLCLinearControl) {
        ST_TRI.ReloadLinear = false;
    }

    if (ST_NOISE.ResetDecay) {
            ST_NOISE.ResetDecay = false;
            ST_NOISE.DecayVolume = 0xF;
            ST_NOISE.DecayCounter = ST_NOISE.Volume;
        }
        else {
            if (ST_NOISE.DecayCounter > 0) {
                ST_NOISE.DecayCounter--;
            }
            else {
                ST_NOISE.DecayCounter = ST_NOISE.DecayVolume;

                if (ST_NOISE.DecayVolume > 0) {
                    ST_NOISE.DecayVolume--;
                }
                else if (ST_NOISE.FreezeLCLoopEnvelope) {
                    ST_NOISE.DecayVolume = 0xF;
                }
            }
        }
}

void FCHalfFrame() {
    // Clock Pulse Sweep for both channels
    for (size_t i = 0; i < 2; i++) {
        if (ST_SQ[i].ReloadSweep) {
            ST_SQ[i].ReloadSweep = false;
            ST_SQ[i].SweepCounter = ST_SQ[i].DividerPeriod;
        }
        else if (ST_SQ[i].SweepCounter > 0) {
            ST_SQ[i].SweepCounter--;
        }
        else {
            ST_SQ[i].SweepCounter = ST_SQ[i].DividerPeriod;
            
            if (ST_SQ[i].SweepEnabled && ST_SQ[i].ShiftCount > 0 && !IsSweepForcingSilence(&ST_SQ[i])) {
                if (ST_SQ[i].IsNegated) {
                    if (i == 0) {
                        ST_SQ[i].Period -= (ST_SQ[i].Period >> ST_SQ[i].ShiftCount) + 1;
                    }
                    else {
                        ST_SQ[i].Period -= (ST_SQ[i].Period >> ST_SQ[i].ShiftCount);
                    }
                }
                else {
                    ST_SQ[i].Period += (ST_SQ[i].Period >> ST_SQ[i].ShiftCount);
                }

                uint16_t startIndex = SQ1_LO;
                if (i == 1) {
                    startIndex = SQ2_LO;
                }

                uint8_t* reg = &CPUMemory[startIndex];
                *reg = (uint8_t)ST_SQ[i].Period;
                reg++;

                uint8_t newHiValue = *reg;
                newHiValue &= 0b11111000;

                uint16_t pHighByte = ST_SQ[i].Period;
                pHighByte >>= 8;
                pHighByte &= 0b111;

                newHiValue |= pHighByte;
                *reg = newHiValue;
            }
        }
    }
    
    for (size_t i = 0; i < 2; i++) {
        if (!ST_SQ[i].FreezeLCLoopEnvelope && ST_SQ[i].LengthCounter > 0) {
            ST_SQ[i].LengthCounter--;
        }
    }

    if (!ST_TRI.FreezeLCLinearControl && ST_TRI.LengthCounter > 0) {
        ST_TRI.LengthCounter--;
    }

    if (!ST_NOISE.FreezeLCLoopEnvelope && ST_NOISE.LengthCounter > 0) {
        ST_NOISE.LengthCounter--;
    }
}

void WriteToFrameCounter(uint8_t value) {
    FC.Is5StepMode = CheckBit(value, 7);
    FC.InhibitInterrupt = CheckBit(value, 6);

    FCShouldReset = true;
    FCResetCountdown = 2;

    if (FC.Is5StepMode) {
        FCQuarterFrame();
        FCHalfFrame();
    }
}
