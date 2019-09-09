#pragma once

#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Enum representing one of the 5 available mouse buttons in SDL
//----------------------------------------------------------------------------------------------------------------------
enum class MouseButton : uint8_t {
    LEFT,
    MIDDLE,
    RIGHT,
    X1,
    X2,
    INVALID
};

static constexpr uint8_t NUM_MOUSE_BUTTONS = (uint32_t) MouseButton::INVALID;
