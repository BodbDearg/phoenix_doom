#pragma once

#include <cstdint>

enum gameaction_e : uint8_t;
struct mobj_t;
struct thinker_t;

extern bool     gTick4;         // True 4 times a second
extern bool     gTick2;         // True 2 times a second
extern bool     gTick1;         // True 1 time a second
extern bool     gGamePaused;    // True if the game is currently paused
extern mobj_t   gMObjHead;      // Head and tail of mobj list

typedef void (*ThinkerFunc)(thinker_t*);

void InitThinkers();
void* AddThinker(ThinkerFunc FuncProc, uint32_t MemSize);
void RemoveThinker(void* thinker);
void ChangeThinkCode(void* thinker, ThinkerFunc FuncProc);
void RunThinkers();
gameaction_e P_Ticker() noexcept;
void P_Drawer(const bool bPresent, const bool bSaveFrameBuffer) noexcept;
void P_Start() noexcept;
void P_Stop() noexcept;
