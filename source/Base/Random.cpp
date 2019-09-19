#include "Random.h"

#include <random>

static std::mt19937 gRandom;

void Random::init() noexcept {
    std::random_device randomDevice;
    gRandom.seed(randomDevice());
}

void Random::init(const uint32_t seed) noexcept {
    gRandom.seed(seed);
}

bool Random::nextBool() noexcept {
    return (std::uniform_int_distribution<uint16_t>()(gRandom) & 1) != 0;
}

uint32_t Random::nextU32() noexcept {
    return std::uniform_int_distribution<uint32_t>()(gRandom);
}

uint32_t Random::nextU32(const uint32_t max) noexcept {
    return std::uniform_int_distribution<uint32_t>(0, max)(gRandom);
}

uint16_t Random::nextU16() noexcept {
    return std::uniform_int_distribution<uint16_t>()(gRandom);
}

uint16_t Random::nextU16(const uint16_t max) noexcept {
    return std::uniform_int_distribution<uint16_t>(0, max)(gRandom);
}

uint8_t Random::nextU8() noexcept {
    return (uint8_t) std::uniform_int_distribution<uint16_t>()(gRandom);
}

uint8_t Random::nextU8(const uint8_t max) noexcept {
    return (uint8_t) std::uniform_int_distribution<uint16_t>(0, max)(gRandom);
}

float Random::nextFloat() noexcept {
    return std::uniform_real_distribution<float>()(gRandom);
}
