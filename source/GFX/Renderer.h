#pragma once

#include "Base/Macros.h"

BEGIN_NAMESPACE(Renderer)

// Dev/cheat z offset applied to the camera for testing
extern float gDebugCameraZOffset;

// The original maximum size of the 3D view
static constexpr uint32_t REFERENCE_3D_VIEW_WIDTH = 280;
static constexpr uint32_t REFERENCE_3D_VIEW_HEIGHT = 160;

void init() noexcept;               // Initialize the renderer (done once)
void initMathTables() noexcept;     // Re-initialize the renderer math tables; must be done if screen size changes!
void drawPlayerView() noexcept;     // Render the 3d view for the player

END_NAMESPACE(Renderer)
