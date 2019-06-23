#pragma once

#include <cstdint>

//--------------------------------------------------------------------------------------------------
// Used to keep track of a playing piece of audio
//--------------------------------------------------------------------------------------------------
struct AudioVoice {
    enum class State : uint8_t {
        STOPPED,    // Voice is not in use
        PLAYING,
        PAUSED
    };

    State       state;
    bool        bIsLooped;
    uint16_t    curSampleFrac;      // Fractional position in between samples (up to 65535/65536)
    uint32_t    curSample;          // Whole sample position in the source audio
    uint32_t    audioDataHandle;    // Handle to audio data from the data manager
    float       volume;
    float       pan;                // 0 = center, -1 left, +1 right
};
