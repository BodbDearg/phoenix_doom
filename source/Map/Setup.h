#pragma once

#include <cstdint>

struct mapthing_t;

extern mapthing_t   deathmatchstarts[10];   // Deathmatch starts
extern mapthing_t*  deathmatch_p;
extern mapthing_t   playerstarts;           // Starting position for players

void SetupLevel(uint32_t map);
void ReleaseMapMemory();
void P_Init();
