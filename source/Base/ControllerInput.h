#pragma once

#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Enum representing an input source on a controller (axis or button)
//----------------------------------------------------------------------------------------------------------------------
enum class ControllerInput : uint8_t {
    BTN_A,
    BTN_B,
    BTN_X,
    BTN_Y,
    BTN_BACK,
    BTN_GUIDE,
    BTN_START,
    BTN_LEFT_STICK,
    BTN_RIGHT_STICK,
    BTN_LEFT_SHOULDER,
    BTN_RIGHT_SHOULDER,
    BTN_DPAD_UP,
    BTN_DPAD_DOWN,
    BTN_DPAD_LEFT,
    BTN_DPAD_RIGHT,
    AXIS_LEFT_X,
    AXIS_LEFT_Y,
    AXIS_RIGHT_X,
    AXIS_RIGHT_Y,
    AXIS_TRIG_LEFT,
    AXIS_TRIG_RIGHT,
    INVALID // N.B: must keep last!
};

static constexpr uint8_t NUM_CONTROLLER_INPUTS = (uint32_t) ControllerInput::INVALID;
