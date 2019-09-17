#pragma once

#include "Base/Macros.h"

//----------------------------------------------------------------------------------------------------------------------
// Simple module that tracks for cheat key sequences in-game and applies cheats
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(Cheats)

void init() noexcept;
void shutdown() noexcept;
void update() noexcept;

END_NAMESPACE(Cheats)
