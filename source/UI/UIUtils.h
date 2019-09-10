#pragma once

#include "Base/Macros.h"
#include <cstdint>

struct CelImage;

BEGIN_NAMESPACE(UIUtils)

// Draws a sprite which is scaled in accordance with the scale factor for the renderer.
// The coordinates are given in terms of the original 320x200 resolution.
void drawUISprite(const int32_t x, const int32_t y, const CelImage& image) noexcept;
void drawUISprite(const int32_t x, const int32_t y, const uint32_t resourceNum) noexcept;
void drawMaskedUISprite(const int32_t x, const int32_t y, const uint32_t resourceNum) noexcept;

// Draw a plaque in the center of the screen (loading or paused)
void drawPlaque(const uint32_t resourceNum) noexcept;

END_NAMESPACE(UIUtils)
