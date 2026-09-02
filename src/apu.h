#ifndef def_APU
#define def_APU

#include <SDL3/SDL.h>

//typedef struct SDL_AudioStream SDL_AudioStream;

typedef struct PulseChannel {
    SDL_AudioStream* Stream;
    uint8_t Duty;
    uint8_t Volume;
    uint16_t Period; // or Timer
    //bool IsSilenced;

    uint16_t LengthCounter;
    uint16_t PeriodCounter;
    uint8_t DutyCounter;
} PulseChannel;

typedef struct FrameCounter {
    bool Is5StepMode;
    bool InhibitInterrupt;
    uint16_t SequenceCounter;
} FrameCounter;

extern PulseChannel ST_SQ[2];

extern SDL_AudioDeviceID AudioDevice;

extern const uint8_t DutyTable[4][8];
extern const uint8_t LengthTable[32];

void APUInit();

void ClockAPU();
void TickPulse();
void TickTriangle();
void TickNoise();
void TickDMC();
void TickFC();
void FCQuarterFrame();
void FCHalfFrame();

void WriteToSQ(uint8_t value, uint8_t byte, uint8_t ch);
void UpdateSQTimer(uint8_t num);

void WriteToTRI(uint8_t value, uint8_t byte);

void WriteToNOISE(uint8_t value, uint8_t byte);

void WriteToStatus(uint8_t value);
void WriteToFrameCounter(uint8_t value);

#endif
