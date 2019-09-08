#pragma once

#include <cstdint>

struct mapthing_t;

extern mapthing_t   gDeathmatchStarts[10];      // Deathmatch starts
extern mapthing_t*  gpDeathmatch;
extern mapthing_t   gPlayerStarts;              // Starting position for players
extern uint32_t     gSkyTextureNum;             // The texture number to use to render the sky with

void SetupLevel(uint32_t map) noexcept;
void ReleaseMapMemory() noexcept;
void P_Init() noexcept;
