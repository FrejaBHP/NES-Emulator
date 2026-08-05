#ifndef def_EMU
#define def_EMU

#include <cpu.h>
#include <ppu.h>
#include <stdio.h>

extern FILE* file;

void TestFnc();
void Initialisation();
void LoadFile();
void LoadROM();
void ParseHeader(uint8_t* header);

#endif
