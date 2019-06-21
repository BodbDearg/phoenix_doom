#pragma once

#include <cstddef>
#include <cstdint>

struct SoundData;

namespace SoundLoader {
    //----------------------------------------------------------------------------------------------
    // Loads a sound from the specified file path and saves the loaded data to the given object.
    // Supported sound file formats are:
    //      (1) AIFF
    //      (2) AIFF-C
    //----------------------------------------------------------------------------------------------
    bool loadFromFile(const char* const filePath, SoundData& soundData) noexcept;

    //----------------------------------------------------------------------------------------------
    // Same as 'loadFromFile' but loads the sound from a buffer instead
    //----------------------------------------------------------------------------------------------
    bool loadFromBuffer(const std::byte* const pBuffer, const uint32_t bufferSize, SoundData& soundData) noexcept;
};
