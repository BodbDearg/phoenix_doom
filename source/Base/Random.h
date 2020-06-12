#include <cstdint>

//------------------------------------------------------------------------------------------------------------------------------------------
// Random number generation utilities
//------------------------------------------------------------------------------------------------------------------------------------------
namespace Random {
    void init() noexcept;
    void init(const uint32_t seed) noexcept;

    bool nextBool() noexcept;

    uint32_t nextU32() noexcept;
    uint32_t nextU32(const uint32_t max) noexcept;

    uint16_t nextU16() noexcept;
    uint16_t nextU16(const uint16_t max) noexcept;

    uint8_t nextU8() noexcept;
    uint8_t nextU8(const uint8_t max) noexcept;

    // Gives a float between 0 and 1
    float nextFloat() noexcept;
}
