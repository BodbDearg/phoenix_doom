#pragma once

#include "Angle.h"
#include "Fixed.h"
#include <cmath>

//----------------------------------------------------------------------------------------------------------------------
// Floating point math functions
//----------------------------------------------------------------------------------------------------------------------
namespace FMath {
    //------------------------------------------------------------------------------------------------------------------
    // Commonly used float angles (in radians)
    //------------------------------------------------------------------------------------------------------------------
    template <class T> static constexpr T ANGLE_180 = T(3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067);
    template <class T> static constexpr T ANGLE_360 = ANGLE_180<T> * T(2.0);
    template <class T> static constexpr T ANGLE_90  = ANGLE_180<T> / T(2.0);
    template <class T> static constexpr T ANGLE_45  = ANGLE_180<T> / T(4.0);
    template <class T> static constexpr T ANGLE_30  = ANGLE_180<T> / T(6.0);
    template <class T> static constexpr T ANGLE_15  = ANGLE_180<T> / T(12.0);
    template <class T> static constexpr T ANGLE_10  = ANGLE_180<T> / T(18.0);
    template <class T> static constexpr T ANGLE_5   = ANGLE_180<T> / T(36.0);
    template <class T> static constexpr T ANGLE_1   = ANGLE_180<T> / T(180.0);

    //------------------------------------------------------------------------------------------------------------------
    // Convert a Doom binary angle to a float angle (in radians) and back again
    //------------------------------------------------------------------------------------------------------------------
    template <class T>
    inline constexpr T doomAngleToRadians(const angle_t angle) noexcept {
        const double normalized = (double) angle / (double(UINT32_MAX) + 1.0);
        const double twoPiRange = normalized * ANGLE_360<double>;
        return T(twoPiRange);
    }

    template <class T>
    inline angle_t radiansToDoomAngle(const T angle) noexcept {
        const double twoPiRange = std::fmod(double(angle), ANGLE_360<double>);
        const double normalized = twoPiRange / ANGLE_360<double>;
        const double intRange = normalized * (double(UINT32_MAX) + 1.0);
        return angle_t(intRange);
    }

    //------------------------------------------------------------------------------------------------------------------
    // Convert a Doom 16.16 fixed point number to float and back again
    //------------------------------------------------------------------------------------------------------------------
    template <class T>
    inline constexpr T doomFixed16ToFloat(const Fixed fixed) noexcept {
        return T((double) fixed * (1.0 / 65536.0));
    }

    template <class T>
    inline constexpr Fixed floatToDoomFixed16(const T value) noexcept {
        return Fixed((double) value * 65536.0);
    }

    //------------------------------------------------------------------------------------------------------------------
    // Convert a Doom 26.6 fixed point number (used for sector height values) to float and back again
    //------------------------------------------------------------------------------------------------------------------
    template <class T>
    inline constexpr T doomFixed6ToFloat(const Fixed fixed) noexcept {
        return T((double) fixed * (1.0 / 64.0));
    }

    template <class T>
    inline constexpr Fixed floatToDoomFixed6(const T value) noexcept {
        return Fixed((double) value * 64.0);
    }

    //------------------------------------------------------------------------------------------------------------------
    // Get the angle from one point to another in radians
    //------------------------------------------------------------------------------------------------------------------
    template <class T>
    static T angleFromPointToPoint(const T p1x, const T p1y, const T p2x, const T p2y) noexcept {
        const T dx = p2x - p1x;
        const T dy = p2y - p1y;
        return std::atan2(dy, dx);
    }

    //------------------------------------------------------------------------------------------------------------------
    // Linearly interpolate between two floats
    //------------------------------------------------------------------------------------------------------------------
    template <class T>
    static constexpr inline T lerp(const T a, const T b, const T t) noexcept {
        return a + (b - a) * t;
    }

    //------------------------------------------------------------------------------------------------------------------
    // Get the 3D distance between two points
    //------------------------------------------------------------------------------------------------------------------
    template <class T>
    static inline T distance3d(const T x1, const T y1, const T z1, const T x2, const T y2, const T z2) noexcept {
        const T dx = x2 - x1;
        const T dy = y2 - y1;
        const T dz = z2 - z1;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
}
