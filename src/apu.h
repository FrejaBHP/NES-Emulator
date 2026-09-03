#ifndef def_APU
#define def_APU

#include <SDL3/SDL.h>

#define SampleRate                  44100 // Hz

//typedef struct SDL_AudioStream SDL_AudioStream;

typedef struct APUStatus {
    bool Pulse0Enabled;
    bool Pulse1Enabled;
    bool TriangleEnabled;
    bool NoiseEnabled;
    bool DMCEnabled;
} APUStatus;

typedef struct PulseChannel {
    uint8_t Duty;
    bool FreezeLCLoopEnvelope;
    bool UseConstantVolume;
    uint8_t Volume;

    uint16_t Period; // or Timer

    bool SweepEnabled;
    uint8_t DividerPeriod;
    bool IsNegated;
    uint8_t ShiftCount;

    uint16_t LengthCounter;
    uint16_t PeriodCounter;
    uint8_t DutyCounter;
    uint16_t SweepCounter;
    uint8_t DecayVolume;
    uint8_t DecayCounter;
    //int16_t SweepTargetPeriod;
    bool ReloadSweep;
    bool ResetDecay;
} PulseChannel;

typedef struct TriangleChannel {
    bool FreezeLCLinearControl;
    uint16_t LinearCounterLoad;

    uint16_t Period;

    uint16_t LengthCounter;

    uint16_t LinearCounter;
    uint16_t PeriodCounter;
    uint8_t Step;
    bool ReloadLinear;
} TriangleChannel;

typedef struct NoiseChannel {
    bool FreezeLCLoopEnvelope;
    bool UseConstantVolume;
    uint8_t Volume;

    bool NoiseMode;
    uint16_t Period; // or Timer

    uint16_t LengthCounter;

    uint16_t ShiftRegister;
    uint16_t PeriodCounter;
    uint8_t DecayVolume;
    uint8_t DecayCounter;
    bool ResetDecay;
} NoiseChannel;

typedef struct FrameCounter {
    bool Is5StepMode;
    bool InhibitInterrupt;
    uint16_t SequenceCounter;
} FrameCounter;

extern APUStatus APU_Status;
extern PulseChannel ST_SQ[2];
extern TriangleChannel ST_TRI;
extern NoiseChannel ST_NOISE;

extern SDL_AudioDeviceID AudioDevice;
extern SDL_AudioStream* Stream;

extern const uint8_t DutyTable[4][8];
extern const uint8_t LengthTable[32];
extern const uint16_t NoiseTable_NTSC[16];
extern const uint16_t NoiseTable_PAL[16];

void APUInit();

void ClockAPU();

void TickPulse();
void OutputPulse();

void TickTriangle();
void OutputTriangle();

void TickNoise();
void OutputNoise();

void TickDMC();

void TickFC();
void FCQuarterFrame();
void FCHalfFrame();

void WriteToSQ(uint8_t value, uint8_t byte, uint8_t ch);
void UpdateSQTimer(uint8_t num);
bool IsSweepForcingSilence(PulseChannel* ch);

void WriteToTRI(uint8_t value, uint8_t byte);
void UpdateTRITimer();

void WriteToNOISE(uint8_t value, uint8_t byte);

void WriteToNOISE(uint8_t value, uint8_t byte);

void WriteToStatus(uint8_t value);
void WriteToFrameCounter(uint8_t value);

#endif
