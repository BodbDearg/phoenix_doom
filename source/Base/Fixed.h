#pragma once

#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Engine fixed point defines:
// Doom uses 16.16 signed fixed point numbers (in twos complement format) throughout a lot of the game.
// In some places the format is different however, but generally when you see 'Fixed' it means 16.16.
//----------------------------------------------------------------------------------------------------------------------

// Typedef for a fixed point number - mainly for code readability.
// IMPORTANT: does *NOT* have to be 16.16, even though in most places throughout the game it will be!
typedef int32_t Fixed;

static constexpr uint32_t   FRACBITS    = 16;                   // Number of fraction bits *USUALLY* in a Fixed (16.16 format)
static constexpr Fixed      FRACUNIT    = 1 << FRACBITS;        // 1.0 in a 16.16 format fixed point number
static constexpr Fixed      FRACMASK    = int32_t(0x0000FFFF);  // Masks out the fractional bits in a 16.16 number
static constexpr Fixed      FRACMIN     = INT32_MIN;            // Min and max value for a 16.16 fixed point number
static constexpr Fixed      FRACMAX     = INT32_MAX;

//----------------------------------------------------------------------------------------------------------------------
// Multiply and divide Doom format fixed point numbers (in 16.16 format).
// On a 32-bit CPU this would have been much trickier (due to overflow) but on a modern 64-bit system we can simply
// use native 64-bit types to do this very quickly.
//----------------------------------------------------------------------------------------------------------------------
static inline constexpr Fixed fixed16Mul(const Fixed num1, const Fixed num2) noexcept {
    const int64_t result64 = (int64_t) num1 * (int64_t) num2;
    return (Fixed) (result64 >> 16);
}

static inline constexpr Fixed fixed16Div(const Fixed num1, const Fixed num2) noexcept {
    const int64_t result64 = (((int64_t) num1) << 16) / (int64_t) num2;
    return (Fixed) result64;
}

//----------------------------------------------------------------------------------------------------------------------
// Conversions to and from Doom format fixed point numbers (in 16.16 format) to 32-bit integers
//----------------------------------------------------------------------------------------------------------------------
static inline constexpr Fixed intToFixed16(const int32_t num) noexcept {
    return num << 16;
}

static inline constexpr int32_t fixed16ToInt(const Fixed num) noexcept {
    return num >> 16;
}

//------------------------------------------------------------------------------------------------------------------
// Conversions to and from Doom format fixed point numbers (16.16 and 26.6) to floats
//------------------------------------------------------------------------------------------------------------------
inline constexpr float fixed16ToFloat(const Fixed fixed) noexcept {
    return float((double) fixed * (1.0 / 65536.0));
}

inline constexpr float fixed6ToFloat(const Fixed fixed) noexcept {
    return float((double) fixed * (1.0 / 64.0));
}

inline constexpr Fixed floatToFixed16(const float value) noexcept {
    return Fixed((double) value * 65536.0);
}

inline constexpr Fixed floatToFixed6(const float value) noexcept {
    return Fixed((double) value * 64.0);
}

//----------------------------------------------------------------------------------------------------------------------
// Yields a reciprocal of the given Doom format fixed point 16.16 number
//----------------------------------------------------------------------------------------------------------------------
static inline constexpr Fixed Fixed16Invert(const Fixed num) noexcept {
    return fixed16Div(FRACUNIT, num);
}
