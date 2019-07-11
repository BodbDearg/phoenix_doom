#pragma once

#include "Base/Macros.h"

BEGIN_NAMESPACE(Renderer)

// Used to compute scale factors
static constexpr uint32_t REFERENCE_SCREEN_WIDTH = 320;
static constexpr uint32_t REFERENCE_SCREEN_HEIGHT = 240;
static constexpr uint32_t REFERENCE_3D_VIEW_WIDTH = 280;
static constexpr uint32_t REFERENCE_3D_VIEW_HEIGHT = 160;

void init() noexcept;               // Initialize the renderer (done once)
void initMathTables() noexcept;     // Re-initialize the renderer math tables; must be done if screen size changes!
void drawPlayerView() noexcept;     // Render the player's view

END_NAMESPACE(Renderer)
