#pragma once

#include "Base/Macros.h"

struct CelImage;

BEGIN_NAMESPACE(Renderer)

// Dev/cheat z offset applied to the camera for testing
extern float gDebugCameraZOffset;

// The original maximum size of the 3D view
static constexpr uint32_t REFERENCE_3D_VIEW_WIDTH = 280;
static constexpr uint32_t REFERENCE_3D_VIEW_HEIGHT = 160;

void init() noexcept;               // Initialize the renderer (done once)
void initMathTables() noexcept;     // Re-initialize the renderer math tables; must be done if screen size changes!
void drawPlayerView() noexcept;     // Render the 3d view for the player

// Utility function: draws a sprite which is scaled in accordance with the scale factor for the renderer.
// The coordinates are given in terms of the original 320x200 resolution.
void drawUISprite(const int32_t x, const int32_t y, const CelImage& image) noexcept;
void drawUISprite(const int32_t x, const int32_t y, const uint32_t resourceNum) noexcept;
void drawMaskedUISprite(const int32_t x, const int32_t y, const uint32_t resourceNum) noexcept;

END_NAMESPACE(Renderer)
