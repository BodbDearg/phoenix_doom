#pragma once

#include "Base/Macros.h"
#include <cstdint>

struct CelImage;

BEGIN_NAMESPACE(UIUtils)

// Flags for PrintNumber
static constexpr uint32_t PNFLAGS_PERCENT   = 1;    // Percent sign appended?
static constexpr uint32_t PNFLAGS_CENTER    = 2;    // Use X as center and not left x
static constexpr uint32_t PNFLAGS_RIGHT     = 4;    // Use right justification

// String and number drawing
void printBigFont(const int32_t x, const int32_t y, const char* const pStr) noexcept;
uint32_t getBigStringWidth(const char* const pStr) noexcept;
void printNumber(const int32_t x, const int32_t y, const uint32_t value, const uint32_t flags) noexcept;
void printBigFontCenter(const int32_t x, const int32_t y, const char* const pStr) noexcept;

// Draws a sprite which is scaled in accordance with the scale factor for the renderer.
// The coordinates are given in terms of the original 320x200 resolution.
void drawUISprite(const int32_t x, const int32_t y, const CelImage& image) noexcept;
void drawUISprite(const int32_t x, const int32_t y, const uint32_t resourceNum) noexcept;
void drawMaskedUISprite(const int32_t x, const int32_t y, const uint32_t resourceNum) noexcept;

// Draw a plaque in the center of the screen (loading or paused)
void drawPlaque(const uint32_t resourceNum) noexcept;

END_NAMESPACE(UIUtils)
