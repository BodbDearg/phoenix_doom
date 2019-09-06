#pragma once

#include "Base/Input.h"
#include "Controls.h"

BEGIN_NAMESPACE(Config)

// Video settings
extern bool         gbFullscreen;
extern uint32_t     gRenderScale;
extern int32_t      gOutputResolutionW;
extern int32_t      gOutputResolutionH;
extern bool         gbIntegerOutputScaling;
extern bool         gbAspectCorrectOutputScaling;

// Keyboard key bindings
extern Controls::MenuActionBits gKeyboardMenuActions[Input::NUM_KEYBOARD_KEYS];
extern Controls::GameActionBits gKeyboardGameActions[Input::NUM_KEYBOARD_KEYS];

// Game controller and bindings
extern float gGamepadDeadZone;
extern float gGamepadAnalogToDigitalThreshold;

extern Controls::MenuActionBits gGamepadMenuActions[NUM_CONTROLLER_INPUTS];
extern Controls::GameActionBits gGamepadGameActions[NUM_CONTROLLER_INPUTS];

// Debug stuff
extern bool gbAllowDebugCameraUpDownMovement;

// Startup and shutdown the prefs module
void init() noexcept;
void shutdown() noexcept;

END_NAMESPACE(Config)
