#include <apu.h>
#include <console.h>
#include <cpu.h>

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


SDL_AudioDeviceID AudioDevice = 0;
SDL_AudioStream* ST_SQ1 = NULL;
SDL_AudioStream* ST_SQ2 = NULL;

uint16_t SQ1Timer = 0;
uint16_t SQ2Timer = 0;

uint8_t DutyTable[4][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 1 },
    { 0, 0, 0, 0, 0, 0, 1, 1 },
    { 0, 0, 0, 0, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 0, 0 }
};

void APUInit() {
    SDL_AudioSpec aspec;

    aspec.freq = 44100;
    aspec.format = SDL_AUDIO_F32;
    aspec.channels = 1;

    ST_SQ1 = SDL_CreateAudioStream(&aspec, NULL);
    ST_SQ2 = SDL_CreateAudioStream(&aspec, NULL);

    SDL_BindAudioStream(AudioDevice, ST_SQ1);
    SDL_BindAudioStream(AudioDevice, ST_SQ2);
}

void UpdateSQTimer(uint8_t num) {
    uint8_t low;
    uint8_t high;

    if (num == 0) {
        low = CPUMemory[0x4002];
        high = CPUMemory[0x4003];
        high &= 0b00000111;
        SQ1Timer = (high << 8) | low;
    }
    else {
        low = CPUMemory[0x4006];
        high = CPUMemory[0x4007];
        high &= 0b00000111;
        SQ2Timer = (high << 8) | low;
    }
}
