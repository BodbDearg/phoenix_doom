#pragma once

#include "Base/Mem.h"
#include <type_traits>

//------------------------------------------------------------------------------------------------------------------------------------------
// Simple struct describing the data for an image in RGBA5551 format, stored either column major or row major layout.
// Since all images & textures in 3DO Doom are ultimately described in terms of a series of RGBA5551 colors, we can use
// this format as the single uniform format throughout the code.
//------------------------------------------------------------------------------------------------------------------------------------------
struct ImageData {
    uint32_t    width;
    uint32_t    height;
    uint16_t*   pPixels;
};

//------------------------------------------------------------------------------------------------------------------------------------------
// Utility functions for dealing with 'ImageData'
//------------------------------------------------------------------------------------------------------------------------------------------
namespace ImageDataUtils {
    // The maximum integer value for RGBA components in an 'ImageData' pixel
    constexpr uint8_t COLOR_R_MAX = 31;
    constexpr uint8_t COLOR_G_MAX = 31;
    constexpr uint8_t COLOR_B_MAX = 31;
    constexpr uint8_t COLOR_A_MAX = 1;

    // Decode an image data pixel to integer RGB values.
    // The decoded RGB components have a range from 0-31.
    template <class T>
    inline constexpr void decodePixelToRGB(const uint16_t pixel, T& r, T& g, T& b) noexcept {
        static_assert(std::is_integral_v<T>);
        r = T(pixel >> 11);
        g = T((pixel >> 6) & uint16_t(0x1F));
        b = T((pixel >> 1) & uint16_t(0x1F));
    }

    // Same as above, but the result includes alpha also.
    // The alpha value is from 0-1.
    template <class T>
    inline constexpr void decodePixelToRGB(const uint16_t pixel, T& r, T& g, T& b, T& a) noexcept {
        static_assert(std::is_integral_v<T>);
        r = T(pixel >> 11);
        g = T((pixel >> 6) & uint16_t(0x1F));
        b = T((pixel >> 1) & uint16_t(0x1F));
        a = T(pixel & uint16_t(0x01));
    }
}
