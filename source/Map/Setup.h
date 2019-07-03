#pragma once

#include <cstdint>

struct mapthing_t;

extern mapthing_t   gDeathmatchStarts[10];      // Deathmatch starts
extern mapthing_t*  gpDeathmatch;
extern mapthing_t   gPlayerStarts;              // Starting position for players

void SetupLevel(uint32_t map);
void ReleaseMapMemory();
void P_Init();
