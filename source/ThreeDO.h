#pragma once

#include "Base/Fixed.h"
#include <cstddef>

struct vissprite_t;

extern uint32_t LastTics;       // Time elapsed since last page flip

void ThreeDOMain();
void WritePrefsFile();
void ClearPrefsFile();
void ReadPrefsFile();
void UpdateAndPageFlip(const bool bAllowDebugClear);
void DrawPlaque(uint32_t RezNum);
void DrawSkyLine();

extern void DrawWallColumn(
    const uint32_t y,
    const uint32_t Colnum,
    const uint32_t ColY,
    const uint32_t TexHeight,
    const std::byte* const Source,
    const uint32_t Run
);

extern void DrawFloorColumn(
    uint32_t ds_y,
    uint32_t ds_x1,
    uint32_t Count,
    uint32_t xfrac,
    uint32_t yfrac,
    Fixed ds_xstep,
    Fixed ds_ystep
);

void DrawSpriteNoClip(const vissprite_t* const pVisSprite);
void DrawSpriteClip(const uint32_t x1, const uint32_t x2, const vissprite_t* const pVisSprite);
void DrawSpriteCenter(uint32_t SpriteNum);
void EnableHardwareClipping();
void DisableHardwareClipping();
void DrawColors();
