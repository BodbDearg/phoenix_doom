#pragma once

#include "Doom.h"

void S_Clear();
void S_StartSound(const Fixed* const pOriginXY, const uint32_t soundId);
void S_StartSong(const uint8_t musicId);
void S_StopSong();
