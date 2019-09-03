#pragma once

#include "Base/Input.h"
#include "Controls.h"

BEGIN_NAMESPACE(Prefs)

// Video settings
extern bool         gbFullscreen;
extern uint32_t     gRenderScale;

// Keyboard key bindings
extern Controls::MenuActionBits gKeyboardMenuActions[Input::NUM_KEYBOARD_KEYS];
extern Controls::GameActionBits gKeyboardGameActions[Input::NUM_KEYBOARD_KEYS];

// Startup and shutdown the prefs module
void init() noexcept;
void shutdown() noexcept;

END_NAMESPACE(Prefs)
