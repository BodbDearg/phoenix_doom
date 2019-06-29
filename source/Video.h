#pragma once

#include <cstdint>

namespace Video {
    // DC: FIXME: DO NOT HARDCODE LIKE THIS!
    static constexpr uint32_t SCREEN_WIDTH = 320;
    static constexpr uint32_t SCREEN_HEIGHT = 200;  // Note: the actual 3DO vertical res was 240 but 40 px was not drawn to (black letterbox)

    // The 32-bit framebuffer to draw to
    extern uint32_t* gFrameBuffer;

    // Create and destroy the display
    void init() noexcept;
    void shutdown() noexcept;

    // Presents the framebuffer to the screen
    void present(const bool bAllowDebugClear) noexcept;
}
