#include "AudioOutputDevice.h"

#include "Macros.h"
#include <cmath>
#include <SDL.h>

AudioOutputDevice::AudioOutputDevice() noexcept
    : mbIsInitialized(false)
    , mbDidInitSDL(false)
    , mAudioDeviceId(0)
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

    // All good if we got to here - unpause the device!
    SDL_PauseAudioDevice(mAudioDeviceId, 0);
    return true;
}

void AudioOutputDevice::shutdown() noexcept {
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

void AudioOutputDevice::audioCallback(
    void* pUserData,
    uint8_t* pBuffer,
    int32_t bufferSize
) noexcept {
    // Figure out how many samples
    const uint32_t numChannelSamples = bufferSize / sizeof(float);
    const uint32_t numSamples = numChannelSamples / 2;

    // TEMPORARY TEST: a simple sawtooth wave
    float* pOutput = reinterpret_cast<float*>(pBuffer);
    float cur = 0.0f;

    for (uint32_t i = 0; i < numSamples; ++i) {        
        pOutput[0] = cur;
        pOutput[1] = cur;

        cur += 0.001f;

        if (cur > 0.25f) {
            cur = -0.25f + std::fmodf(cur, 0.25f);
        }

        pOutput += 2;
    }
}
