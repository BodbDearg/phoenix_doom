#pragma once

#include "Base/Macros.h"
#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Whether or not the host system is big endian or not:
// If this is not explicitly defined by the build setup, then just assume little endian.
// Eventually we can use C++20 features to detect this but for now this should work for most cases.
// Users on more exotic platforms (like the PPC 'Amiga X1000') may want to adjust however...
//----------------------------------------------------------------------------------------------------------------------
#ifndef BIG_ENDIAN
    #define BIG_ENDIAN 0
#endif

BEGIN_NAMESPACE(Endian)

//----------------------------------------------------------------------------------------------------------------------
// Utility functions for byte swapping big endian to host endian, which should be little endian in most cases...
// Needed because the 3DO data for doom is stored in big endian format. Both the dev machine (68K Mac) and the
// 3DO hardware itself were big endian, hence it made sense to store the data this way.
//----------------------------------------------------------------------------------------------------------------------
inline uint16_t bigToHost(const uint16_t num) {
    #if BIG_ENDIAN
        return num;
    #else
        return (
            (uint16_t)((num & 0x00FFU) << 8) |
            (uint16_t)((num & 0xFF00U) >> 8)
        );
    #endif
}

inline int16_t bigToHost(const int16_t num) {
    #if BIG_ENDIAN
        return num;
    #else
        return (int16_t) bigToHost((uint16_t) num);
    #endif
}

inline uint32_t bigToHost(const uint32_t num) {
    #if BIG_ENDIAN
        return num;
    #else
        return (
            ((num & (uint32_t) 0x000000FFU) << 24) |
            ((num & (uint32_t) 0x0000FF00U) << 8) |
            ((num & (uint32_t) 0x00FF0000U) >> 8) |
            ((num & (uint32_t) 0xFF000000U) >> 24)
        );
    #endif
}

inline int32_t bigToHost(const int32_t num) {
    #if BIG_ENDIAN
        return num;
    #else
        return (int32_t) bigToHost((uint32_t) num);
    #endif
}

template <class T>
inline void convertBigToHost(T& value) noexcept {
    #if !BIG_ENDIAN
        value = bigToHost(value);
    #endif
}

END_NAMESPACE(Endian)
