#pragma once

#include "Base/Fixed.h"

void S_Clear() noexcept;
uint32_t S_StartSound(
    const Fixed* const pOriginXY,
    const uint32_t soundId,
    const bool bStopOtherInstances = false
) noexcept;

void S_StartSong(const uint8_t musicId) noexcept;
void S_StopSong() noexcept;
