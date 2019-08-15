#pragma once

#include "Base/Fixed.h"

// TODO: REMOVE
#include "Renderer.h"

namespace Video {
    // DC: FIXME: DO NOT HARDCODE LIKE THIS!
    // TODO: REMOVE
    #if HACK_TEST_HIGH_RES_RENDERING
        static constexpr uint32_t SCREEN_WIDTH = 320 * HACK_TEST_HIGH_RENDER_SCALE;
        static constexpr uint32_t SCREEN_HEIGHT = 200 * HACK_TEST_HIGH_RENDER_SCALE;
    #else
        static constexpr uint32_t SCREEN_WIDTH = 320;
        static constexpr uint32_t SCREEN_HEIGHT = 200;  // Note: the actual 3DO vertical res was 240 but 40 px was not drawn to (black letterbox)
    #endif

    // The 32-bit framebuffer to draw to and a saved old copy of it (for screen wipes)
    extern uint32_t* gpFrameBuffer;
    extern uint32_t* gpSavedFrameBuffer;

    // Create and destroy the display
    void init() noexcept;
    void shutdown() noexcept;

    // Does a debug clear of the framebuffer to a known color - does nothing in release builds.
    // Used to identify issues where parts of the screen are not being drawn to.
    void debugClear() noexcept;

    // Presents the framebuffer to the screen
    void present(const bool bSaveFramebuffer) noexcept;

    //------------------------------------------------------------------------------------------------------------------
    // Makes a framebuffer color from the given 16.16 fixed point RGB values.
    // Saturates/clamps the values if they are out of range.
    // The returned value is fully opaque.
    //------------------------------------------------------------------------------------------------------------------
    inline uint32_t fixedRgbToScreenCol(const Fixed rFrac, const Fixed gFrac, const Fixed bFrac) noexcept {
        const uint16_t rInt = rFrac >> (FRACBITS - 3);
        const uint16_t gInt = gFrac >> (FRACBITS - 3);
        const uint16_t bInt = bFrac >> (FRACBITS - 3);
        const uint32_t rClamp = (rInt > 0xFF) ? 0xFF : rInt;
        const uint32_t gClamp = (gInt > 0xFF) ? 0xFF : gInt;
        const uint32_t bClamp = (bInt > 0xFF) ? 0xFF : bInt;
        return ((rClamp << 24) | (gClamp << 16) | (bClamp << 8) | 0xFF);
    }

    //------------------------------------------------------------------------------------------------------------------
    // Makes a framebuffer color from the given RGBA5551 color value.
    // Note: the alpha component is ignored.
    //------------------------------------------------------------------------------------------------------------------
    inline uint32_t rgba5551ToScreenCol(const uint16_t color) noexcept {
        const uint32_t r5 = (color & 0b0111110000000000) >> 10;
        const uint32_t g5 = (color & 0b0000001111100000) >> 5;
        const uint32_t b5 = (color & 0b0000000000011111) >> 0;

        const uint32_t fbCol = (r5 << 19) | (g5 << 11) | (b5 << 3);
        return fbCol;
    }
}
