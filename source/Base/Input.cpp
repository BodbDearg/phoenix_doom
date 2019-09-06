#include "Input.h"

#include "ControllerInput.h"
#include "Game/Config.h"
#include <SDL.h>

BEGIN_NAMESPACE(Input)

bool                            gbIsQuitRequested;
const Uint8*                    gpKeyboardState;
int                             gNumKeyboardStateKeys;
std::vector<uint16_t>           gKeyboardKeysPressed;
std::vector<uint16_t>           gKeyboardKeysJustPressed;
std::vector<uint16_t>           gKeyboardKeysJustReleased;
float                           gControllerInputs[NUM_CONTROLLER_INPUTS];
std::vector<ControllerInput>    gControllerInputsPressed;
std::vector<ControllerInput>    gControllerInputsJustPressed;
std::vector<ControllerInput>    gControllerInputsJustReleased;

static SDL_GameController*  gpGameController;
static SDL_Joystick*        gpJoystick;             // Note: this is managed by game controller, not freed by this code!
static SDL_JoystickID       gJoystickId;

//----------------------------------------------------------------------------------------------------------------------
// Updates the list of pressed keys
//----------------------------------------------------------------------------------------------------------------------
static void updatePressedKeyboardKeys() noexcept {
    // Check all keys and do 8 at a time
    gKeyboardKeysPressed.clear();

    // Process 8 keys at a time to try and speed this up; most WON'T be pressed!
    ASSERT(gpKeyboardState);
    const uint8_t* const pKbState = gpKeyboardState;
    const uint32_t numKeys = gNumKeyboardStateKeys;
    const uint32_t numKeys8 = (numKeys & 0xFFFFFFF8);

    for (uint32_t keyIdx = 0; keyIdx < numKeys8; keyIdx += 8) {
        // Quick reject of 8 keys at a time:
        // Read the current batch of keys as a u64 and ignore if all bits are zero:
        const uint64_t keyVector = ((const uint64_t*) &pKbState[keyIdx])[0];

        if (keyVector == 0)
            continue;
        
        // This batch of keys needs more examination, see which keys are pressed
        const uint32_t keyIdx1 = keyIdx + 0;
        const uint32_t keyIdx2 = keyIdx + 1;
        const uint32_t keyIdx3 = keyIdx + 2;
        const uint32_t keyIdx4 = keyIdx + 3;
        const uint32_t keyIdx5 = keyIdx + 4;
        const uint32_t keyIdx6 = keyIdx + 5;
        const uint32_t keyIdx7 = keyIdx + 6;
        const uint32_t keyIdx8 = keyIdx + 7;

        const bool bKey1Pressed = (pKbState[keyIdx1] > 0);
        const bool bKey2Pressed = (pKbState[keyIdx2] > 0);
        const bool bKey3Pressed = (pKbState[keyIdx3] > 0);
        const bool bKey4Pressed = (pKbState[keyIdx4] > 0);
        const bool bKey5Pressed = (pKbState[keyIdx5] > 0);
        const bool bKey6Pressed = (pKbState[keyIdx6] > 0);
        const bool bKey7Pressed = (pKbState[keyIdx7] > 0);
        const bool bKey8Pressed = (pKbState[keyIdx8] > 0);

        if (bKey1Pressed) { gKeyboardKeysPressed.push_back((SDL_Scancode) keyIdx1); }
        if (bKey2Pressed) { gKeyboardKeysPressed.push_back((SDL_Scancode) keyIdx2); }
        if (bKey3Pressed) { gKeyboardKeysPressed.push_back((SDL_Scancode) keyIdx3); }
        if (bKey4Pressed) { gKeyboardKeysPressed.push_back((SDL_Scancode) keyIdx4); }
        if (bKey5Pressed) { gKeyboardKeysPressed.push_back((SDL_Scancode) keyIdx5); }
        if (bKey6Pressed) { gKeyboardKeysPressed.push_back((SDL_Scancode) keyIdx6); }
        if (bKey7Pressed) { gKeyboardKeysPressed.push_back((SDL_Scancode) keyIdx7); }
        if (bKey8Pressed) { gKeyboardKeysPressed.push_back((SDL_Scancode) keyIdx8); }
    }

    // Process the remaining keyboard keys
    for (uint32_t keyIdx = numKeys8; keyIdx < numKeys; ++keyIdx) {
        const bool bKeyPressed = (pKbState[keyIdx] > 0);

        if (bKeyPressed) {
            gKeyboardKeysPressed.push_back(keyIdx);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Utility functions for querying if a value is in a vector and removing it
//----------------------------------------------------------------------------------------------------------------------
template <class T>
static inline bool vectorContainsValue(const std::vector<T>& vec, const T val) noexcept {
    const auto endIter = vec.end();
    const auto iter = std::find(vec.begin(), endIter, val);
    return (iter != endIter);
}

template <class T>
static inline void removeValueFromVector(const T val, std::vector<T>& vec) noexcept {
    auto endIter = vec.end();
    auto iter = std::find(vec.begin(), vec.end(), val);

    while (iter != endIter) {
        iter = vec.erase(iter);
        endIter = vec.end();
        iter = std::find(iter, endIter, val);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Convert an SDL axis value to a -1 to + 1 range float.
//----------------------------------------------------------------------------------------------------------------------
static float sdlAxisValueToFloat(const int16_t axis) noexcept {
    if (axis >= 0) {
        return (float) axis / 32767.0f;
    } else {
        return (float) axis / 32768.0f;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Close the currently open game controller (if any).
// Also clears up any related inputs.
//----------------------------------------------------------------------------------------------------------------------
static void closeCurrentGameController() noexcept {
    std::memset(gControllerInputs, 0, sizeof(gControllerInputs));
    gControllerInputsJustPressed.clear();
    gControllerInputsJustReleased.clear();

    if (gpGameController) {
        SDL_GameControllerClose(gpGameController);
        gpGameController = nullptr;
    }

    gpJoystick = nullptr;
    gJoystickId = {};
}

//----------------------------------------------------------------------------------------------------------------------
// Rescans for the SDL game controllers to use: just uses the first available game controller.
// This may choose wrong in a multi-gamepad situation but the user can always disconnect one to clarify what is wanted.
// Most computer users would probably only want one gamepad at a time anyway?
//----------------------------------------------------------------------------------------------------------------------
static void rescanGameControllers() noexcept {
    // If we already have a gamepad then just re-check that it is still connected
    if (gpGameController) {
        if (!SDL_GameControllerGetAttached(gpGameController)) {
            closeCurrentGameController();
        }
    }

    // See if there are any joysticks connected.
    // Note: a return of < 0 means an error, which we will ignore:
    const int numJoysticks = SDL_NumJoysticks();

    for (int joyIdx = 0; joyIdx < numJoysticks; ++joyIdx) {
        // If we find a valid game controller then try to open it.
        // If we succeed then our work is done!
        if (SDL_IsGameController(joyIdx)) {           
            gpGameController = SDL_GameControllerOpen(joyIdx);

            if (gpGameController) {
                gpJoystick = SDL_GameControllerGetJoystick(gpGameController);
                gJoystickId = SDL_JoystickInstanceID(gpJoystick);
                break;
            }       
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Handle events sent by SDL (keypresses and such)
//----------------------------------------------------------------------------------------------------------------------
static void handleSdlEvents() noexcept {
    SDL_Event sdlEvent;

    while (SDL_PollEvent(&sdlEvent) != 0) {
        switch (sdlEvent.type) {
            case SDL_QUIT:
                gbIsQuitRequested = true;
                break;

            case SDL_KEYDOWN: {
                const uint16_t scancode = (uint16_t) sdlEvent.key.keysym.scancode;

                if (scancode < NUM_KEYBOARD_KEYS) {
                    removeValueFromVector(scancode, gKeyboardKeysJustReleased); // Prevent contradictions!
                    gKeyboardKeysJustPressed.push_back(scancode);
                }
            }   break;

            case SDL_KEYUP: {
                const uint16_t scancode = (uint16_t) sdlEvent.key.keysym.scancode;

                if (scancode < NUM_KEYBOARD_KEYS) {
                    removeValueFromVector(scancode, gKeyboardKeysJustPressed);  // Prevent contradictions!
                    gKeyboardKeysJustReleased.push_back(scancode);
                }
            }   break;

            case SDL_CONTROLLERAXISMOTION: {
                if (sdlEvent.cbutton.which == gJoystickId) {
                    const ControllerInput input = sdlControllerAxisToInput(sdlEvent.caxis.axis);

                    if (input != ControllerInput::INVALID) {
                        const float pressedThreshold = Config::gGamepadAnalogToDigitalThreshold;
                        const uint8_t inputIdx = (uint8_t) input;

                        // See if there is a change in the 'pressed' status
                        const bool bPrevPressed = (std::abs(gControllerInputs[inputIdx]) >= pressedThreshold);
                        const float inputF = sdlAxisValueToFloat(sdlEvent.caxis.value);
                        const float inputFAbs = std::abs(inputF);
                        const bool bNowPressed = (inputFAbs >= pressedThreshold);
                        const bool bPassedDeadZone  = (inputFAbs >= Config::gGamepadDeadZone);

                        // Update input value
                        gControllerInputs[inputIdx] = (bPassedDeadZone) ? inputF : 0.0f;

                        // Generate events for the analog input
                        if (bPrevPressed != bNowPressed) {
                            if (bNowPressed) {
                                gControllerInputsJustPressed.push_back(input);
                                gControllerInputsPressed.push_back(input);
                                removeValueFromVector(input, gControllerInputsJustReleased);    // Prevent contradictions!
                            } else {
                                gControllerInputsJustReleased.push_back(input);
                                removeValueFromVector(input, gControllerInputsPressed);
                                removeValueFromVector(input, gControllerInputsJustPressed);     // Prevent contradictions!
                            }
                        }
                    }
                }
            }   break;

            case SDL_CONTROLLERBUTTONDOWN: {
                if (sdlEvent.cbutton.which == gJoystickId) {
                    const ControllerInput input = sdlControllerButtonToInput(sdlEvent.cbutton.button);

                    if (input != ControllerInput::INVALID) {
                        gControllerInputsJustPressed.push_back(input);
                        gControllerInputsPressed.push_back(input);
                        removeValueFromVector(input, gControllerInputsJustReleased);    // Prevent contradictions!
                        gControllerInputs[(uint8_t) input] = 1.0f;
                    }
                }
            }   break;

            case SDL_CONTROLLERBUTTONUP: {
                if (sdlEvent.cbutton.which == gJoystickId) {
                    const ControllerInput input = sdlControllerButtonToInput(sdlEvent.cbutton.button);

                    if (input != ControllerInput::INVALID) {
                        gControllerInputsJustReleased.push_back(input);
                        removeValueFromVector(input, gControllerInputsJustPressed);     // Prevent contradictions!
                        removeValueFromVector(input, gControllerInputsPressed);
                        gControllerInputs[(uint8_t) input] = 0.0f;
                    }
                }
            }   break;

            case SDL_JOYDEVICEADDED:
            case SDL_JOYDEVICEREMOVED:
            case SDL_CONTROLLERDEVICEADDED:
            case SDL_CONTROLLERDEVICEREMOVED:
            case SDL_CONTROLLERDEVICEREMAPPED:
                rescanGameControllers();
                break;
        }
    }
}

void init() noexcept {
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        FATAL_ERROR("Failed to initialize the SDL joystick input subsystem!");
    }

    SDL_GameControllerEventState(SDL_ENABLE);   // Want game controller events

    gbIsQuitRequested = false;
    gpKeyboardState = SDL_GetKeyboardState(&gNumKeyboardStateKeys);
    gKeyboardKeysJustPressed.reserve(16);
    gKeyboardKeysJustReleased.reserve(16);
    gControllerInputsJustPressed.reserve(16);
    gControllerInputsJustReleased.reserve(16);

    rescanGameControllers();
}

void shutdown() noexcept {
    closeCurrentGameController();

    gControllerInputsJustReleased.clear();
    gControllerInputsJustReleased.shrink_to_fit();
    gControllerInputsJustPressed.clear();
    gControllerInputsJustPressed.shrink_to_fit();

    gKeyboardKeysJustReleased.clear();
    gKeyboardKeysJustReleased.shrink_to_fit();
    gKeyboardKeysJustPressed.clear();
    gKeyboardKeysJustPressed.shrink_to_fit();
    gKeyboardKeysPressed.clear();
    gKeyboardKeysPressed.shrink_to_fit();

    gpKeyboardState = nullptr;
    gbIsQuitRequested = false;

    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

void update() noexcept {    
    consumeEvents();                    // Released/pressed events are now cleared
    handleSdlEvents();                  // Process events that SDL is sending to us
    updatePressedKeyboardKeys();        // Update polled key state
}

void consumeEvents() noexcept {
    gKeyboardKeysJustPressed.clear();
    gKeyboardKeysJustReleased.clear();
    gControllerInputsJustPressed.clear();
    gControllerInputsJustReleased.clear();
}

bool isQuitRequested() noexcept {
    return gbIsQuitRequested;
}

void requestQuit() noexcept {
    gbIsQuitRequested = true;
}

bool areAnyKeysOrButtonsPressed() noexcept {
    // Check keyboard for input
    if (!gKeyboardKeysPressed.empty())
        return true;
    
    // Check game controller for any input
    if (gpGameController) {
        if (!gControllerInputsPressed.empty())
            return true;
    
        const float deadZone = Config::gGamepadDeadZone;

        for (float inputValue : gControllerInputs) {
            if (std::abs(inputValue) >= deadZone)
                return true;
        }
    }

    return false;
}

const std::vector<uint16_t>& getKeyboardKeysPressed() noexcept {
    return gKeyboardKeysPressed;
}

const std::vector<uint16_t>& getKeyboardKeysJustPressed() noexcept {
    return gKeyboardKeysJustPressed;
}

const std::vector<uint16_t>& getKeyboardKeysJustReleased() noexcept {
    return gKeyboardKeysJustReleased;
}

bool isKeyboardKeyPressed(const uint16_t key) noexcept {
    return vectorContainsValue(gKeyboardKeysPressed, key);
}

bool isKeyboardKeyJustPressed(const uint16_t key) noexcept {
    return vectorContainsValue(gKeyboardKeysJustPressed, key);
}

bool isKeyboardKeyReleased(const uint16_t key) noexcept {
    return (!isKeyboardKeyPressed(key));
}

bool isKeyboardKeyJustReleased(const uint16_t key) noexcept {
    return vectorContainsValue(gKeyboardKeysJustReleased, key);
}

const std::vector<ControllerInput>& getControllerInputsPressed() noexcept {
    return gControllerInputsPressed;
}

const std::vector<ControllerInput>& getControllerInputsJustPressed() noexcept {
    return gControllerInputsJustPressed;
}

const std::vector<ControllerInput>& getControllerInputsJustReleased() noexcept {
    return gControllerInputsJustReleased;
}

bool isControllerInputPressed(const ControllerInput input) noexcept {
    return vectorContainsValue(gControllerInputsPressed, input);
}

bool isControllerInputJustPressed(const ControllerInput input) noexcept {
    return vectorContainsValue(gControllerInputsJustPressed, input);
}

bool isControllerInputJustReleased(const ControllerInput input) noexcept {
    return vectorContainsValue(gControllerInputsJustReleased, input);
}

float getControllerInputValue(const ControllerInput input) noexcept {
    const uint8_t inputIdx = (uint8_t) input;
    return (inputIdx < NUM_CONTROLLER_INPUTS) ? gControllerInputs[inputIdx] : 0.0f;
}

END_NAMESPACE(Input)
