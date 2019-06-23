#include "AudioOutputDevice.h"

#include "AudioSystem.h"
#include "Macros.h"
#include <algorithm>
#include <cmath>
#include <SDL.h>

AudioOutputDevice::AudioOutputDevice() noexcept
    : mbIsInitialized(false)
    , mbDidInitSDL(false)
    , mAudioDeviceId(0)
    , mSampleRate(0)
    , mAudioSystems()
{
}

AudioOutputDevice::~AudioOutputDevice() noexcept {
    shutdown();
}

bool AudioOutputDevice::init() noexcept {
    ASSERT(!mbIsInitialized);
    mbIsInitialized = true;

    // Firstly initialize SDL
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        shutdown();
        return false;
    }

    mbDidInitSDL = true;

    // Open the audio stream
    SDL_AudioSpec requestedAudioFmt = {};
    requestedAudioFmt.freq = 48000;
    requestedAudioFmt.format = AUDIO_F32;
    requestedAudioFmt.channels = 2;
    requestedAudioFmt.samples = 2048;
    requestedAudioFmt.callback = audioCallback;
    requestedAudioFmt.userdata = this;

    SDL_AudioSpec obtainedAudioFmt = {};
    mAudioDeviceId = SDL_OpenAudioDevice(
        nullptr,
        0,                                  // 0 = not recording, no capture
        &requestedAudioFmt,
        &obtainedAudioFmt,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE    // Only allowing frequency to change
    );

    if (mAudioDeviceId == 0) {
        shutdown();
        return false;
    }

    // All good if we got to here - unpause the device and save the sample rate!
    mSampleRate = obtainedAudioFmt.freq;
    SDL_PauseAudioDevice(mAudioDeviceId, 0);
    return true;
}

void AudioOutputDevice::shutdown() noexcept {
    ASSERT_LOG(mAudioSystems.empty(), "All audio systems should be deregistered by the time the output device is shut down!");

    if (!mbIsInitialized)
        return;
    
    mbIsInitialized = false;

    if (mAudioDeviceId != 0) {
        SDL_PauseAudioDevice(mAudioDeviceId, 1);
        SDL_CloseAudioDevice(mAudioDeviceId);
        mAudioDeviceId = 0;
    }
    
    if (mbDidInitSDL) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        mbDidInitSDL = false;
    }
}

void AudioOutputDevice::registerAudioSystem(AudioSystem& system) noexcept {
    ASSERT(mbIsInitialized);
    ASSERT_LOG(
        std::find(mAudioSystems.begin(), mAudioSystems.end(), &system) == mAudioSystems.end(),
        "System must not already be registered!"
    );

    AudioDeviceLock lockAudioDev(*this);
    mAudioSystems.push_back(&system);
}

void AudioOutputDevice::unregisterAudioSystem(AudioSystem& system) noexcept {
    ASSERT(mbIsInitialized);

    // N.B: Safe to do searching outside of the lock so long as we don't unregister from the audio thread.
    // That should never be happening!
    const auto iter = std::find(mAudioSystems.begin(), mAudioSystems.end(), &system);
    
    if (iter != mAudioSystems.end()) {
        AudioDeviceLock lockAudioDev(*this);
        mAudioSystems.erase(iter);
    }
}

void AudioOutputDevice::lockAudioDevice() noexcept {
    ASSERT(mbIsInitialized);
    SDL_LockAudioDevice(mAudioDeviceId);
}

void AudioOutputDevice::unlockAudioDevice() noexcept {
    ASSERT(mbIsInitialized);
    SDL_UnlockAudioDevice(mAudioDeviceId);
}

void AudioOutputDevice::audioCallback(
    void* pUserData,
    uint8_t* pBuffer,
    int32_t bufferSize
) noexcept {
    ASSERT(pUserData);
    ASSERT(pBuffer > 0);
    ASSERT(bufferSize > 0);

    // Figure out how many samples
    const uint32_t numChannelSamples = bufferSize / sizeof(float);
    const uint32_t numSamples = numChannelSamples / 2;

    // Zero all sample data initially
    std::memset(pBuffer, 0, (uint32_t) bufferSize);

    // Add the contribution of all non paused systems to the output
    float* const pOutput = reinterpret_cast<float*>(pBuffer);
    AudioOutputDevice& device = *reinterpret_cast<AudioOutputDevice*>(pUserData);

    for (AudioSystem* pSystem : device.mAudioSystems) {
        if (!pSystem->isPaused()) {
            pSystem->mixAudio(pOutput, numSamples);
        }
    }
}
