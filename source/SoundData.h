#pragma once

#include "Macros.h"
#include <cstddef>
#include <cstdint>

//--------------------------------------------------------------------------------------------------
// Structure holding data and information about that data for a sound
//--------------------------------------------------------------------------------------------------
struct SoundData {
    std::byte*  pBuffer;        // Contains all the samples for the sound. Note: each sample should be byte aligned!
    uint32_t    bufferSize;     // Actual size in bytes of the sound byte buffer
    uint32_t    numSamples;     // Number of samples in the data
    uint32_t    sampleRate;     // 44,100 etc.
    uint16_t    numChannels;    // Should be: '1' or '2', note that the data for each channel is interleaved for each sample.
    uint16_t    bitDepth;       // Should be: '8' or '16' only.

    inline SoundData() noexcept
        : pBuffer(nullptr)
        , bufferSize(0)
        , numSamples(0)
        , sampleRate(0)
        , numChannels(0)
        , bitDepth(0)
    {
    }

    //----------------------------------------------------------------------------------------------
    // Allocates the buffer for the sound
    //----------------------------------------------------------------------------------------------
    void allocBuffer(const uint32_t size) noexcept {
        ASSERT(!pBuffer);
        ASSERT(size > 0);

        pBuffer = new std::byte[size];
        bufferSize = size;
    }
    
    //----------------------------------------------------------------------------------------------
    // Frees the buffer for the sound
    //----------------------------------------------------------------------------------------------
    void freeBuffer() noexcept {
        delete[] pBuffer;
        pBuffer = nullptr;
        bufferSize = 0;
    }

    //----------------------------------------------------------------------------------------------
    // Frees the sound buffer and resets all fields
    //----------------------------------------------------------------------------------------------
    void clear() noexcept {
        freeBuffer();
        
        numSamples = 0;
        sampleRate = 0;
        numChannels = 0;
        bitDepth = 0;
    }
};
