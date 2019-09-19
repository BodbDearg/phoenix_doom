#pragma once

#include "Base/Macros.h"
#include <cmath>

//----------------------------------------------------------------------------------------------------------------------
// Floating point math functions
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(FMath)

//----------------------------------------------------------------------------------------------------------------------
// Commonly used float angles (in radians)
//----------------------------------------------------------------------------------------------------------------------
template <class T> static constexpr T ANGLE_180 = T(3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067);
template <class T> static constexpr T ANGLE_360 = ANGLE_180<T> * T(2.0);
template <class T> static constexpr T ANGLE_90  = ANGLE_180<T> / T(2.0);
template <class T> static constexpr T ANGLE_45  = ANGLE_180<T> / T(4.0);
template <class T> static constexpr T ANGLE_30  = ANGLE_180<T> / T(6.0);
template <class T> static constexpr T ANGLE_15  = ANGLE_180<T> / T(12.0);
template <class T> static constexpr T ANGLE_10  = ANGLE_180<T> / T(18.0);
template <class T> static constexpr T ANGLE_5   = ANGLE_180<T> / T(36.0);
template <class T> static constexpr T ANGLE_1   = ANGLE_180<T> / T(180.0);

//----------------------------------------------------------------------------------------------------------------------
// Get the angle from one point to another in radians
//----------------------------------------------------------------------------------------------------------------------
template <class T>
inline T angleFromPointToPoint(const T p1x, const T p1y, const T p2x, const T p2y) noexcept {
    const T dx = p2x - p1x;
    const T dy = p2y - p1y;
    return std::atan2(dy, dx);
}

//----------------------------------------------------------------------------------------------------------------------
// Linearly interpolate between two floats
//----------------------------------------------------------------------------------------------------------------------
template <class T>
constexpr inline T lerp(const T a, const T b, const T t) noexcept {
    return a + (b - a) * t;
}

//----------------------------------------------------------------------------------------------------------------------
// Get the 3D distance between two points
//----------------------------------------------------------------------------------------------------------------------
template <class T>
inline T distance3d(const T x1, const T y1, const T z1, const T x2, const T y2, const T z2) noexcept {
    const T dx = x2 - x1;
    const T dy = y2 - y1;
    const T dz = z2 - z1;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

END_NAMESPACE(FMath)
