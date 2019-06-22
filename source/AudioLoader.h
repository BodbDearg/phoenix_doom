#pragma once

#include <cstddef>
#include <cstdint>

struct AudioData;

namespace AudioLoader {
    //----------------------------------------------------------------------------------------------
    // Loads an audio file from the specified file path and saves the loaded data to the given object.
    // Supported sound file formats are:
    //      (1) AIFF
    //      (2) AIFF-C
    //----------------------------------------------------------------------------------------------
    bool loadFromFile(const char* const filePath, AudioData& audioData) noexcept;

    //----------------------------------------------------------------------------------------------
    // Same as 'loadFromFile' but loads the audio from a buffer instead
    //----------------------------------------------------------------------------------------------
    bool loadFromBuffer(const std::byte* const pBuffer, const uint32_t bufferSize, AudioData& audioData) noexcept;
};
