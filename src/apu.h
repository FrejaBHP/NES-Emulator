#ifndef def_APU
#define def_APU

#include <SDL3/SDL.h>

//typedef struct SDL_AudioStream SDL_AudioStream;

extern SDL_AudioDeviceID AudioDevice;

extern SDL_AudioStream* ST_SQ1;
extern SDL_AudioStream* ST_SQ2;

extern uint16_t SQ1Timer;
extern uint16_t SQ2Timer;

extern uint8_t DutyTable[4][8];

void APUInit();
void UpdateSQTimer(uint8_t num);


#endif
