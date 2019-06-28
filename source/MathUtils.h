#pragma once

#include <stdint.h>

//--------------------------------------------------------------------------------------------------
// Multiply and divide 16.16 fixed point numbers
//--------------------------------------------------------------------------------------------------
static inline constexpr int32_t sfixedMul16_16(const int32_t num1, const int32_t num2) noexcept {
    const int64_t result64 = (int64_t) num1 * (int64_t) num2;
    return (int32_t) (result64 >> 16);
}

static inline constexpr int32_t sfixedDiv16_16(const int32_t num1, const int32_t num2) noexcept {
    const int64_t result64 = (((int64_t) num1) << 16) / (int64_t) num2;
    return (int32_t) result64;
}

//--------------------------------------------------------------------------------------------------
// Converstions from 'float' and 'int32_t' to 16.16 fixed point
//--------------------------------------------------------------------------------------------------
static inline constexpr int32_t int32ToSFixed16_16(const int32_t num) noexcept {
    return num << 16;
}

static inline constexpr int32_t sfixed16_16ToInt32(const int32_t num) noexcept {
    return num >> 16;
}

static inline constexpr int32_t floatToSFixed16_16(const float num) noexcept {
    const double doubleVal = (double) num * 65536.0;
    return (int32_t) doubleVal;
}

static inline constexpr float sfixed16_16ToFloat(const int32_t num) noexcept {
    const double doubleVal = (double) num;
    return (float) (doubleVal / 65536.0);
}

//--------------------------------------------------------------------------------------------------
// A utility to convert a tick count from PC Doom's original 35Hz timebase to the 60Hz timebase 
// used by this version of doom. Tries to round so the answer is as close as possible.
//--------------------------------------------------------------------------------------------------
static inline constexpr uint32_t convertPcTicks(const uint32_t ticks35Hz) noexcept {
    // Get the tick count in 31.1 fixed point format by multiplying by 60/35 (in 31.1 format)
    const uint32_t tickCountFixed = ((ticks35Hz * uint32_t(60)) << 2) / (uint32_t(35) << 1);
    // Return the tick count and round up if the fractional answer is .5
    return (tickCountFixed & 1) ? (tickCountFixed >> 1) + 1 : (tickCountFixed >> 1);
}

//--------------------------------------------------------------------------------------------------
// Convert an integer speed defined in the PC 35Hz timebase to the 60Hz timebase used by
// used by this version of doom. Tries to round so the answer is as close as possible.
//--------------------------------------------------------------------------------------------------
static inline constexpr uint32_t convertPcUint32Speed(const uint32_t speed35Hz) noexcept {
    // Get the tick count in 31.1 fixed point format by multiplying by 35/60 (in 31.1 format)
    const uint32_t speedFixed = ((speed35Hz * uint32_t(35)) << 2) / (uint32_t(60) << 1);
    // Return the speed and round up if the fractional answer is .5
    return (speedFixed & 1) ? (speedFixed >> 1) + 1 : (speedFixed >> 1);
}
