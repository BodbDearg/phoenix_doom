#pragma once

#include "Base/Macros.h"

// Enable to allow moving up and down the camera along the Z axis.
// Useful to test how the renderer reacts to different height adjustments.
#ifndef ENABLE_DEBUG_CAMERA_Z_MOVEMENT
    #define ENABLE_DEBUG_CAMERA_Z_MOVEMENT 1
#endif

BEGIN_NAMESPACE(Renderer)

#if ENABLE_DEBUG_CAMERA_Z_MOVEMENT
    extern float gDebugCameraZOffset;
#endif

// Used to compute scale factors
static constexpr uint32_t REFERENCE_SCREEN_WIDTH = 320;
static constexpr uint32_t REFERENCE_SCREEN_HEIGHT = 240;
static constexpr uint32_t REFERENCE_3D_VIEW_WIDTH = 280;
static constexpr uint32_t REFERENCE_3D_VIEW_HEIGHT = 160;

void init() noexcept;               // Initialize the renderer (done once)
void initMathTables() noexcept;     // Re-initialize the renderer math tables; must be done if screen size changes!
void drawPlayerView() noexcept;     // Render the player's view

END_NAMESPACE(Renderer)
