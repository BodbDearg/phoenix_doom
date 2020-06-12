#pragma once

#include "Base/Macros.h"
#include "Base/Fixed.h"

struct SDL_Window;

BEGIN_NAMESPACE(Video)

// Used to compute scale factors - the original screen resolution and 3d view size
static constexpr uint32_t REFERENCE_SCREEN_WIDTH = 320;
static constexpr uint32_t REFERENCE_SCREEN_HEIGHT = 200;

// The resolution that the game renders at.
// This is NOT the output resolution.
extern uint32_t gScreenWidth;
extern uint32_t gScreenHeight;

// This is the actual resolution of the window outputted to.
// In fullscreen mode this is the screen resolution.
extern uint32_t gVideoOutputWidth;
extern uint32_t gVideoOutputHeight;

// If true then the game is in fullscreen mode.
extern bool gbIsFullscreen;

// The 32-bit framebuffer to draw to and a saved old copy of it (for screen wipes).
// N.B: The saved framebuffer is in COLUMN MAJOR format for more efficient cache usage during wipes.
extern uint32_t* gpFrameBuffer;
extern uint32_t* gpSavedFrameBuffer;

// Create and destroy the display
void init() noexcept;
void shutdown() noexcept;

// Clear the screen to the specified RGB color   
void clearScreen(const uint8_t r, const uint8_t g, const uint8_t b) noexcept;

// Does a debug clear of the framebuffer to a known color - does nothing in release builds.
// Used to identify issues where parts of the screen are not being drawn to.
void debugClearScreen() noexcept;

// Get a handle to the underlying SDL window
SDL_Window* getWindow() noexcept;

// Saves a copy of the current framebuffer to the 'saved' framebuffer.
// This should be done prior to calling 'present'.
void saveFrameBuffer() noexcept;

// Presents the framebuffer to the screen
void present() noexcept;

// Helper that combines various end of frame operations.
// Saves the framebuffer if required and presents if required.
void endFrame(const bool bPresent, const bool bSaveFrameBuffer) noexcept;

//------------------------------------------------------------------------------------------------------------------------------------------
// Makes a framebuffer color from the given 16.16 fixed point RGB values.
// Saturates/clamps the values if they are out of range.
// The returned value is fully opaque.
//------------------------------------------------------------------------------------------------------------------------------------------
inline uint32_t fixedRgbToScreenCol(const Fixed rFrac, const Fixed gFrac, const Fixed bFrac) noexcept {
    const uint32_t rInt = (uint32_t)(rFrac >> (FRACBITS - 3));
    const uint32_t gInt = (uint32_t)(gFrac >> (FRACBITS - 3));
    const uint32_t bInt = (uint32_t)(bFrac >> (FRACBITS - 3));
    const uint32_t rClamp = (rInt > 0xFF) ? 0xFF : rInt;
    const uint32_t gClamp = (gInt > 0xFF) ? 0xFF : gInt;
    const uint32_t bClamp = (bInt > 0xFF) ? 0xFF : bInt;
    return ((rClamp << 24) | (gClamp << 16) | (bClamp << 8) | 0xFF);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Makes a framebuffer color from the given RGBA5551 color value.
// Note: the alpha component is ignored.
//------------------------------------------------------------------------------------------------------------------------------------------
inline uint32_t rgba5551ToScreenCol(const uint16_t color) noexcept {
    const uint32_t r5 = (color & 0b0111110000000000) >> 10;
    const uint32_t g5 = (color & 0b0000001111100000) >> 5;
    const uint32_t b5 = (color & 0b0000000000011111) >> 0;

    const uint32_t fbCol = (r5 << 19) | (g5 << 11) | (b5 << 3);
    return fbCol;
}

END_NAMESPACE(Video)
