#pragma once

#include <cstdint>

struct mobj_t;

extern bool     Tick4;          // True 4 times a second
extern bool     Tick2;          // True 2 times a second
extern bool     Tick1;          // True 1 time a second
extern bool     gamepaused;     // True if the game is currently paused
extern mobj_t   mobjhead;       // Head and tail of mobj list

typedef void (*ThinkerFunc)(struct thinker_t*);

void InitThinkers();
void* AddThinker(ThinkerFunc FuncProc, uint32_t MemSize);
void RemoveThinker(void* thinker);
void ChangeThinkCode(void* thinker, ThinkerFunc FuncProc);
void RunThinkers();
uint32_t P_Ticker();
void P_Drawer();
void P_Start();
void P_Stop();
