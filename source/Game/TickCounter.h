#pragma once

#include "Base/Macros.h"
#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Simple module that keeps track of time for the game and tells how many ticks need to be simulated.
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(TickCounter)

void init() noexcept;
void shutdown() noexcept;
uint32_t update() noexcept;     // Update time tracking and return the number of ticks that must be simulated

END_NAMESPACE(TickCounter)
