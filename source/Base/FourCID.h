#pragma once

#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Represents a four character identifier used in various file formats to identify the file type, data chunk types etc.
// The identifier is always stored in reading order, from left to right - irrespective of machine endianess.
//----------------------------------------------------------------------------------------------------------------------
struct FourCID {
    union {
        uint8_t     idChars[4];
        uint32_t    idBits;         // So four chars can be compared at a time
    };
    
    FourCID() noexcept = default;
    FourCID(const FourCID& other) noexcept = default;

    inline constexpr FourCID(
        const uint8_t c1,
        const uint8_t c2,
        const uint8_t c3,
        const uint8_t c4
    ) noexcept 
        : idChars{ c1, c2, c3, c4 }
    {
    }

    inline constexpr FourCID(const char* const pStr) noexcept
        : idBits(FourCID::make(pStr).idBits)
    {
    }

    static inline constexpr FourCID make(const char* const pStr) noexcept {
        uint8_t c1 = 0;
        uint8_t c2 = 0;
        uint8_t c3 = 0;
        uint8_t c4 = 0;

        if (pStr[0]) {
            c1 = (uint8_t) pStr[0];

            if (pStr[1]) {
                c2 = (uint8_t) pStr[1];

                if (pStr[2]) {
                    c3 = (uint8_t) pStr[2];
                    c4 = (uint8_t) pStr[3];     // Don't care if this is NUL!
                }
            }
        }

        return FourCID(c1, c2, c3, c4);
    }

    inline constexpr bool operator == (const FourCID& other) const noexcept {
        return (idBits == other.idBits);
    }

    inline constexpr bool operator != (const FourCID& other) const noexcept {
        return (idBits != other.idBits);
    }
};
