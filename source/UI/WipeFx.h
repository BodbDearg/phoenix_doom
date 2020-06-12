#pragma once

#include "Base/Macros.h"
#include "Game/DoomMain.h"

BEGIN_NAMESPACE(WipeFx)

//------------------------------------------------------------------------------------------------------------------------------------------
// Runs the wipe effect for the game.
// Captures one frame of rendering using the given drawer, then does the wipe using that render and
// the contents of the previously saved framebuffer. Does not return until the wipe is complete or
// the user decides to quit the application.
//------------------------------------------------------------------------------------------------------------------------------------------
void doWipe(const GameLoopDrawFunc drawFunc) noexcept;

END_NAMESPACE(WipeFx)
