#pragma once

#include <cstdint>

//--------------------------------------------------------------------------------------------------
// Manages a single audio output device.
// Mixes together streams from multiple audio systems.
//--------------------------------------------------------------------------------------------------
class AudioOutputDevice {
public:
    AudioOutputDevice() noexcept;
    ~AudioOutputDevice() noexcept;
    
    bool init() noexcept;
    void shutdown() noexcept;
    inline bool isInitialized() const noexcept { return mbIsInitialized; }

private:
    static void audioCallback(
        void* pUserData,
        uint8_t* pBuffer,
        int32_t bufferSize
    ) noexcept;

    bool        mbIsInitialized;
    bool        mbDidInitSDL;
    uint32_t    mAudioDeviceId;
};
