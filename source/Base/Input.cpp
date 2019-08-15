#include "Input.h"
#include <algorithm>
#include <vector>

BEGIN_NAMESPACE(Input)

bool                        gbQuitRequested;
const Uint8*                gpKeyboardState;
int                         gNumKeyboardStateKeys;
std::vector<SDL_Scancode>   gKeysJustPressed;
std::vector<SDL_Scancode>   gKeysJustReleased;

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
    gbQuitRequested = false;
    gpKeyboardState = SDL_GetKeyboardState(&gNumKeyboardStateKeys);
    gKeysJustPressed.reserve(16);
    gKeysJustReleased.reserve(16);
}

void shutdown() noexcept {
    consumeEvents();
    gKeysJustReleased.shrink_to_fit();
    gKeysJustPressed.shrink_to_fit();
    gNumKeyboardStateKeys = 0;
    gpKeyboardState = nullptr;
    gbQuitRequested = false;
}

void update() noexcept {
    consumeEvents();
    SDL_Event sdlEvent;

    while (SDL_PollEvent(&sdlEvent) != 0) {
        switch (sdlEvent.type) {
            case SDL_QUIT:
                gbQuitRequested = true;
                break;
            
            case SDL_KEYDOWN: {
                const SDL_Scancode scancode = sdlEvent.key.keysym.scancode;
                removeValueFromVector(scancode, gKeysJustReleased); // Prevent contradictions!
                gKeysJustPressed.push_back(scancode);                 
            }   break;

            case SDL_KEYUP: {
                const SDL_Scancode scancode = sdlEvent.key.keysym.scancode;
                removeValueFromVector(scancode, gKeysJustPressed);  // Prevent contradictions!
                gKeysJustReleased.push_back(scancode);  
            }   break;
        }
    }
}

void consumeEvents() noexcept {
    gKeysJustPressed.clear();
    gKeysJustReleased.clear();
}

bool quitRequested() noexcept {
    return gbQuitRequested;
}

bool isKeyPressed(const SDL_Scancode key) noexcept {
    return (key < gNumKeyboardStateKeys) ? (gpKeyboardState[key] != 0) : false;
}

bool isKeyJustPressed(const SDL_Scancode key) noexcept {
    return vectorContainsValue(gKeysJustPressed, key);
}

bool isKeyReleased(const SDL_Scancode key) noexcept {
    return (!isKeyPressed(key));
}

bool isKeyJustReleased(const SDL_Scancode key) noexcept {
    return vectorContainsValue(gKeysJustReleased, key);
}

END_NAMESPACE(Input)
