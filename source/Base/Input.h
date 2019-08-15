#include "Base/Macros.h"
#include <SDL.h>

//----------------------------------------------------------------------------------------------------------------------
// Helper that manages user events and inputs received from the SDL library
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(Input)

// Initialize and shutdown input handling
void init() noexcept;
void shutdown() noexcept;

// Generates input events like key down etc.
// Should be called once per frame.
void update() noexcept;

// Discards input events
void consumeEvents() noexcept;

// Returns true if the user requested that the app be quit (via close button)
bool quitRequested() noexcept;

// Query input state and whether something is just pressed or released
bool isKeyPressed(const SDL_Scancode key) noexcept;
bool isKeyJustPressed(const SDL_Scancode key) noexcept;
bool isKeyReleased(const SDL_Scancode key) noexcept;
bool isKeyJustReleased(const SDL_Scancode key) noexcept;

END_NAMESPACE(Input)
