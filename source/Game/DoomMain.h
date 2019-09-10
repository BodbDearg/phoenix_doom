#pragma once

#include "Base/Fixed.h"

enum gameaction_e : uint8_t;

// Function types for the game mini loop
typedef void (*GameLoopStartFunc)() noexcept;
typedef void (*GameLoopStopFunc)() noexcept;
typedef gameaction_e (*GameLoopTickFunc)() noexcept;
typedef void (*GameLoopDrawFunc)(const bool bPresent, const bool bSaveFrameBuffer) noexcept;

// Used for running one screen or section of the game.
// Runs a game loop with start, stop, update and draw functions.
// Certain stuff like ticking at the right rate, user inputs etc. are handled by the loop.
gameaction_e MiniLoop(
    const GameLoopStartFunc start,
    const GameLoopStopFunc stop,
    const GameLoopTickFunc ticker,
    const GameLoopDrawFunc drawer
) noexcept;

void D_DoomMain() noexcept;
