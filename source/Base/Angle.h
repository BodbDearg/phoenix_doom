#pragma once

#include "FMath.h"
#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Engine angle type: Doom uses binary angle measurement, where the entire range of a 32-bit unsigned integer is used to
// represent 0-360 degrees. This format also naturally wraps around.
//----------------------------------------------------------------------------------------------------------------------

typedef uint32_t angle_t;       // 32-bit BAM angle (uses the full 32-bits to represent 360 degrees)

static constexpr angle_t ANG45  = 0x20000000u;      // 45 degrees in angle_t
static constexpr angle_t ANG90  = 0x40000000u;      // 90 degrees in angle_t
static constexpr angle_t ANG180 = 0x80000000u;      // 180 degrees in angle_t
static constexpr angle_t ANG270 = 0xC0000000u;      // 270 degrees in angle_t

inline constexpr angle_t negateAngle(const angle_t a) noexcept {
    return (angle_t) -(int32_t) a;
}

//----------------------------------------------------------------------------------------------------------------------
// Convert a Doom binary angle to a float angle (in radians) and back again
//----------------------------------------------------------------------------------------------------------------------
inline constexpr float bamAngleToRadians(const angle_t angle) noexcept {
    const double normalized = (double) angle / (double(UINT32_MAX) + 1.0);
    const double twoPiRange = normalized * FMath::ANGLE_360<double>;
    return float(twoPiRange);
}

inline angle_t radiansToBamAngle(const float angle) noexcept {
    const double twoPiRange = std::fmod(double(angle), FMath::ANGLE_360<double>);
    const double normalized = twoPiRange / FMath::ANGLE_360<double>;
    const double intRange = normalized * (double(UINT32_MAX) + 1.0);
    return angle_t(intRange);
}
