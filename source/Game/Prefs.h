#pragma once

#include "Controls.h"

BEGIN_NAMESPACE(Prefs)

// N.B: MUST match the SDL headers! (I don't want to expose SDL via this header)
// Verified via static assert in the .cpp file. 
static constexpr uint32_t MAX_KEYBOARD_SCAN_CODES = 512;

// Keyboard key bindings
extern Controls::MenuActions gKeyboardMenuActions[MAX_KEYBOARD_SCAN_CODES];
extern Controls::GameActions gKeyboardGameActions[MAX_KEYBOARD_SCAN_CODES];

// Read the preferences from the config ini file on disk
void read() noexcept;

END_NAMESPACE(Prefs)
