#pragma once

#include <cstdint>

enum gameaction_e : uint8_t;
struct mobj_t;
struct thinker_t;

extern bool     gbIsPlayingMap;             // True if the player is inside a level rather than on a menu outside the level
extern bool     gbQuitToMainRequested;      // If true then we are to exit back to the main menu
extern bool     gbTick4;                    // True 4 times a second
extern bool     gbTick2;                    // True 2 times a second
extern bool     gbTick1;                    // True 1 time a second
extern bool     gbGamePaused;               // True if the game is currently paused
extern mobj_t   gMObjHead;                  // Head and tail of mobj list

typedef void (*ThinkerFunc)(thinker_t&) noexcept;

void InitThinkers() noexcept;
void* AddThinker(const ThinkerFunc funcProc, const uint32_t memSize) noexcept;
void RemoveThinker(void* const pThinker) noexcept;
void ChangeThinkCode(void* const pThinker, const ThinkerFunc funcProc) noexcept;
void RunThinkers() noexcept;
gameaction_e P_Ticker() noexcept;
void P_Drawer(const bool bPresent, const bool bSaveFrameBuffer) noexcept;
void P_Start() noexcept;
void P_Stop() noexcept;

// Template helpers to hide ugly casting etc.
template <class T>
T& AddThinker(void (* const funcProc)(T&) noexcept) noexcept {
    return *reinterpret_cast<T*>(AddThinker((ThinkerFunc) funcProc, (uint32_t) sizeof(T)));
}

template <class T>
inline void RemoveThinker(T& thinker) noexcept {
    RemoveThinker(&thinker);
}

template <class T>
inline void ChangeThinkCode(T& thinker, void (* const funcProc)(T&) noexcept) noexcept { 
    ChangeThinkCode(&thinker, (ThinkerFunc) funcProc);
}
