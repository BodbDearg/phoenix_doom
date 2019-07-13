#pragma once

#include "Angle.h"
#include <cmath>

namespace FloatMath {
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
    // Convert a Doom binary angle to a float angle (in radians)
    //------------------------------------------------------------------------------------------------------------------
    template <class T>
    inline T doomAngleToRadians(const angle_t angle) noexcept {
        const double normalized = (double) angle / (double(UINT32_MAX) + 1.0);
        const double twoPiRange = normalized * ANGLE_360<double>;
        return T(twoPiRange);
    }
    
    //------------------------------------------------------------------------------------------------------------------
    // Convert a float angle (in radians) to a Doom binary angle
    //------------------------------------------------------------------------------------------------------------------
    template <class T>
    inline angle_t radiansToDoomAngle(const T angle) noexcept {
        const double twoPiRange = std::fmod(double(angle), ANGLE_360<double>);
        const double normalized = twoPiRange / ANGLE_360<double>;
        const double intRange = normalized * (double(UINT32_MAX) + 1.0);
        return angle_t(intRange);
    }
}
