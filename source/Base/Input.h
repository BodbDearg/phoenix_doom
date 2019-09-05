#pragma once

#include "Base/Macros.h"
#include <vector>
#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Helper that manages user events and inputs received from the SDL library
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(Input)

// N.B: MUST match the SDL headers! (I don't want to expose SDL via this header)
// Verified via static assert in the .cpp file. 
inline constexpr uint16_t NUM_KEYBOARD_KEYS = 512;

// Initialize and shutdown input handling
void init() noexcept;
void shutdown() noexcept;

// Generates input events like key down etc.
// Should be called once per frame.
void update() noexcept;

// Discards input events
void consumeEvents() noexcept;

// Returns true if the user requested that the app be quit via close button.
// Also the ability to quit via code.
bool isQuitRequested() noexcept;
void requestQuit() noexcept;

// Returns true if any keys or buttons are pressed
bool areAnyKeysOrButtonsPressed() noexcept;

// Get a list of things pressed, just pressed or just released
const std::vector<uint16_t>& getKeyboardKeysPressed() noexcept;
const std::vector<uint16_t>& getKeyboardKeysJustPressed() noexcept;
const std::vector<uint16_t>& getKeyboardKeysJustReleased() noexcept;

// Query input state and whether something is just pressed or released
bool isKeyboardKeyPressed(const uint16_t key) noexcept;
bool isKeyboardKeyJustPressed(const uint16_t key) noexcept;
bool isKeyboardKeyReleased(const uint16_t key) noexcept;
bool isKeyboardKeyJustReleased(const uint16_t key) noexcept;

END_NAMESPACE(Input)
