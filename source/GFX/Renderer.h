#pragma once

#include "Base/Macros.h"

BEGIN_NAMESPACE(Renderer)

void init() noexcept;               // Initialize the renderer (done once)
void initMathTables() noexcept;     // Re-initialize the renderer math tables; must be done if screen size changes!
void drawPlayerView() noexcept;     // Render the player's view

END_NAMESPACE(Renderer)
