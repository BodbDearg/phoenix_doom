#include "Input.h"

#include <SDL.h>

BEGIN_NAMESPACE(Input)

bool                    gbIsQuitRequested;
const Uint8*            gpKeyboardState;
int                     gNumKeyboardStateKeys;
std::vector<uint16_t>   gKeyboardKeysPressed;
std::vector<uint16_t>   gKeyboardKeysJustPressed;
std::vector<uint16_t>   gKeyboardKeysJustReleased;

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

void init() noexcept {
    gbIsQuitRequested = false;
    gpKeyboardState = SDL_GetKeyboardState(&gNumKeyboardStateKeys);
    gKeyboardKeysJustPressed.reserve(16);
    gKeyboardKeysJustReleased.reserve(16);
}

void shutdown() noexcept {
    gKeyboardKeysPressed.clear();
    gKeyboardKeysPressed.shrink_to_fit();

    gKeyboardKeysJustReleased.clear();
    gKeyboardKeysJustReleased.shrink_to_fit();
    
    gKeyboardKeysJustPressed.clear();
    gKeyboardKeysJustPressed.shrink_to_fit();

    gpKeyboardState = nullptr;
    gbIsQuitRequested = false;
}

void update() noexcept {
    // Released/pressed events are now cleared
    consumeEvents();

    // Update polled key state
    updatePressedKeyboardKeys();

    // Poll for input events
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
        }
    }
}

void consumeEvents() noexcept {
    gKeyboardKeysJustPressed.clear();
    gKeyboardKeysJustReleased.clear();
}

bool isQuitRequested() noexcept {
    return gbIsQuitRequested;
}

bool areAnyKeysOrButtonsPressed() noexcept {
    return (!gKeyboardKeysPressed.empty());
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

END_NAMESPACE(Input)
