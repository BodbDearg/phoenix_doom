#pragma once

#include <cstdint>
#include <vector>

class AudioSystem;

//------------------------------------------------------------------------------------------------------------------------------------------
// Manages a single audio output device.
// Mixes together streams from multiple audio systems.
//------------------------------------------------------------------------------------------------------------------------------------------
class AudioOutputDevice {
public:
    AudioOutputDevice() noexcept;
    ~AudioOutputDevice() noexcept;

    bool init() noexcept;
    void shutdown() noexcept;
    inline bool isInitialized() const noexcept { return mbIsInitialized; }

    // Gives the sample rate of the audio device in Hz (samples per second)
    inline uint32_t getSampleRate() const noexcept { return mSampleRate; }

    // Register and unregister an audio system
    void registerAudioSystem(AudioSystem& system) noexcept;
    void unregisterAudioSystem(AudioSystem& system) noexcept;

    //------------------------------------------------------------------------------------------------------------------
    // Locks and unlocks the audio device.
    // When the lock is held it is guaranteed that no audio callback will be in progress.
    //
    // Lock should be done whenever we are doing anything that might affect playing audio, such as
    // modifying a voice or setting the volume or pausing a music system. Otherwise we might run
    // into data race issues with the audio callback thread...
    //
    // Use the RAII lock helper to assist with using these.
    //------------------------------------------------------------------------------------------------------------------
    void lockAudioDevice() noexcept;
    void unlockAudioDevice() noexcept;

private:
    static void audioCallback(
        void* pUserData,
        uint8_t* pBuffer,
        int32_t bufferSize
    ) noexcept;

    bool                        mbIsInitialized;
    bool                        mbDidInitSDL;
    uint32_t                    mAudioDeviceId;
    uint32_t                    mSampleRate;
    std::vector<AudioSystem*>   mAudioSystems;
};

//------------------------------------------------------------------------------------------------------------------------------------------
// Helper for locking and unlocking the audio device
//------------------------------------------------------------------------------------------------------------------------------------------
class AudioDeviceLock {
public:
    inline AudioDeviceLock(AudioOutputDevice& device) noexcept : mDevice(device) {
        device.lockAudioDevice();
    }

    inline ~AudioDeviceLock() noexcept {
        mDevice.unlockAudioDevice();
    }

private:
    AudioOutputDevice& mDevice;
};
