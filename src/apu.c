#include <apu.h>
#include <console.h>
#include <cpu.h>
#include <stdbool.h>

#define SampleRate                  44100 // Hz

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

PulseChannel ST_SQ[2] = { 0 };
FrameCounter FC = { 0 };

bool EvenTick = false;
bool FCShouldReset = false;
uint8_t FCResetCountdown = 0;

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

void APUInit() {
    SDL_AudioSpec aspec;

    aspec.freq = SampleRate;
    aspec.format = SDL_AUDIO_S16;
    aspec.channels = 1;

    ST_SQ[0].Stream = SDL_CreateAudioStream(&aspec, NULL);
    ST_SQ[1].Stream = SDL_CreateAudioStream(&aspec, NULL);

    SDL_BindAudioStream(AudioDevice, ST_SQ[0].Stream);
    SDL_BindAudioStream(AudioDevice, ST_SQ[1].Stream);

    SQ0SoundBuffer = calloc(1, sizeof(int16_t) * 800);
    SQ1SoundBuffer = calloc(1, sizeof(int16_t) * 800);

    SQBuffers[0] = SQ0SoundBuffer;
    SQBuffers[1] = SQ1SoundBuffer;
}

void ClockAPU() {
    if (!EvenTick) {
        EvenTick = true;
        return;
    }

    TickPulse();
    TickFC();
    
    EvenTick = false;

    IgnoreCounter++;
    
    if (IgnoreCounter > 19) {
        IgnoreCounter = 0;
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

    if (IgnoreCounter == 0 && SampleCounter < 800) {
        if (DutyTable[ST_SQ[0].Duty][ST_SQ[0].DutyCounter] && ST_SQ[0].LengthCounter != 0 && ST_SQ[0].PeriodCounter > 7) {
            SQBuffers[AlternateFrame][SampleCounter] = ST_SQ[0].Volume * 50;
        }
        else {
            SQBuffers[AlternateFrame][SampleCounter] = 0;
        }

        if (DutyTable[ST_SQ[1].Duty][ST_SQ[1].DutyCounter] && ST_SQ[1].LengthCounter != 0 && ST_SQ[1].PeriodCounter > 7) {
            SQBuffers[AlternateFrame][SampleCounter] += ST_SQ[1].Volume * 50;
        }
        else {
            SQBuffers[AlternateFrame][SampleCounter] += 0;
        }
        
        SampleCounter++;
    }
}

void WriteToSQ(uint8_t value, uint8_t byte, uint8_t ch) {
    switch (byte) {
        case 0: // SQ_VOL
            uint8_t duty = value >> 6;
            ST_SQ[ch].Duty = duty;

            ST_SQ[ch].Volume = 0b1111 & value;
            break;

        case 1: // SQ_SWEEP
            break;

        case 2: // SQ_LO
            UpdateSQTimer(ch);
            break;

        case 3: // SQ_HI
            UpdateSQTimer(ch);
            ST_SQ[ch].PeriodCounter = ST_SQ[ch].Period;
            ST_SQ[ch].DutyCounter = 0;

            uint8_t lc = value >> 3;
            ST_SQ[ch].LengthCounter = LengthTable[lc];
            break;
    
        default:
            break;
    }
}

void UpdateSQTimer(uint8_t num) {
    uint8_t low;
    uint8_t high;

    if (num == 0) {
        low = CPUMemory[0x4002];
        high = CPUMemory[0x4003];
    }
    else {
        low = CPUMemory[0x4006];
        high = CPUMemory[0x4007];
    }

    high &= 0b00000111;
    ST_SQ[num].Period = (high << 8) | low;
}

void WriteToTRI(uint8_t value, uint8_t byte) {

}

void WriteToNOISE(uint8_t value, uint8_t byte) {

}

void WriteToStatus(uint8_t value) {
    // FIXME: Incomplete implementation, is supposed to alter APU counters
    uint8_t write = value;
    write <<= 3;
    write >>= 3;

    uint8_t sndchn = CPUMemory[SND_CHN];
    //sndchn <<= 1;
    //sndchn >>= 6;
    //sndchn <<= 1;
    sndchn >>= 5;
    sndchn <<= 5;

    CPUMemory[SND_CHN] = sndchn | write;

    if (!CheckBit(value, 0)) {
        ST_SQ[0].LengthCounter = 0;
    }

    if (!CheckBit(value, 1)) {
        ST_SQ[1].LengthCounter = 0;
    }

    // other channels later
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

}

void FCHalfFrame() {
    if (!CheckBit(CPUMemory[SQ1_VOL], 5) && ST_SQ[0].LengthCounter > 0) {
        ST_SQ[0].LengthCounter--;

        /*
        if (ST_SQ[0].LengthCounter == 0) {
            ST_SQ[0].IsSilenced = true;
        }
        */
    }

    if (!CheckBit(CPUMemory[SQ2_VOL], 5) && ST_SQ[1].LengthCounter > 0) {
        ST_SQ[1].LengthCounter--;
    }

    // other channels later
}

void WriteToFrameCounter(uint8_t value) {
    FC.Is5StepMode = CheckBit(value, 7);
    FC.InhibitInterrupt = CheckBit(value, 6);

    FCShouldReset = true;
    FCResetCountdown = 2;
}
