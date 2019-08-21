#include "AudioSystem.h"

#include "AudioDataMgr.h"
#include "AudioOutputDevice.h"

AudioSystem::AudioSystem() noexcept
    : mbIsInitialized(false)
    , mbIsPaused(false)
    , mpAudioOutputDevice(nullptr)
    , mpAudioDataMgr(nullptr)
    , mMasterVolume(DEFAULT_MASTER_VOLUME)
    , mVoices()
    , mFreeVoices()
{
}

AudioSystem::~AudioSystem() noexcept {
    shutdown();
}

void AudioSystem::init(AudioOutputDevice& device, AudioDataMgr& dataMgr, const uint32_t maxVoices) noexcept {
    // Sanity check expected state
    ASSERT(!mbIsInitialized);
    ASSERT(!mbIsPaused);
    ASSERT(mMasterVolume == DEFAULT_MASTER_VOLUME);
    ASSERT(mVoices.empty());
    ASSERT(mFreeVoices.empty());

    // Sanity check input
    ASSERT(device.isInitialized());
    ASSERT(maxVoices >= 1);

    // Lock the device
    AudioDeviceLock lockAudioDevice(device);

    // Initialize: note that we add the first voice indexes to the free list LAST so they are used FIRST
    mbIsInitialized = true;
    mpAudioOutputDevice = &device;
    mpAudioDataMgr = &dataMgr;
    mVoices.resize(maxVoices);
    mFreeVoices.reserve(maxVoices);

    for (uint32_t voiceIdx = maxVoices; voiceIdx > 0;) {
        --voiceIdx;
        mFreeVoices.push_back(voiceIdx);
    }

    // Register with the output device as an audio system
    device.registerAudioSystem(*this);
}

void AudioSystem::shutdown() noexcept {
    if (mpAudioOutputDevice) {
        mpAudioOutputDevice->unregisterAudioSystem(*this);
        mpAudioOutputDevice->lockAudioDevice();
    }

    mFreeVoices.clear();
    mVoices.clear();
    mMasterVolume = 1.0f;
    mpAudioDataMgr = nullptr;
    mbIsPaused = false;
    mbIsInitialized = false;

    if (mpAudioOutputDevice) {
        mpAudioOutputDevice->unlockAudioDevice();
    }

    mpAudioOutputDevice = nullptr;
}

AudioVoice AudioSystem::getVoiceState(const VoiceIdx voiceIdx) const noexcept {
    ASSERT(mbIsInitialized);
    ASSERT(voiceIdx < getNumVoices());
    return mVoices[voiceIdx];
}

void AudioSystem::setVoiceState(const VoiceIdx voiceIdx, const AudioVoice& state) noexcept {
    ASSERT(mbIsInitialized);
    ASSERT(voiceIdx < getNumVoices());

    AudioVoice& curState = mVoices[voiceIdx];
    AudioDeviceLock lockAudioDevice(*mpAudioOutputDevice);

    // See if there is a change in the active state for the voice
    const bool bActive = (curState.state != AudioVoice::State::STOPPED);
    const bool bWillBeActive = (state.state != AudioVoice::State::STOPPED);

    if (bActive != bWillBeActive) {
        if (!bWillBeActive) {
            mFreeVoices.push_back(voiceIdx);
        }
        else {
            removeFromFreeVoiceList(voiceIdx);
        }
    }

    // Update the state of the sound
    curState = state;
}

void AudioSystem::setMasterVolume(const float volume) noexcept {
    ASSERT(mbIsInitialized);
    AudioDeviceLock lockAudioDevice(*mpAudioOutputDevice);
    mMasterVolume = volume;
}

void AudioSystem::pause(const bool pause) noexcept {
    ASSERT(mbIsInitialized);

    // N.B: safe to check paused state outside of lock since we will never modify this from the audio thread
    if (mbIsPaused != pause) {
        AudioDeviceLock lockAudioDevice(*mpAudioOutputDevice);
        mbIsPaused = pause;
    }
}

AudioSystem::VoiceIdx AudioSystem::play(
    const uint32_t audioDataHandle,
    const bool bLooped,
    const float lVolume,
    const float rVolume,
    const bool bStopOtherInstances
) noexcept {
    ASSERT(mbIsInitialized);

    // Playback fails if there are no more voices or the audio data is not loaded.
    //
    // N.B: can check if resource is loaded outside of the audio device lock since
    // we should never be unloading audio from the audio thread...
    if (!mpAudioDataMgr->isHandleLoaded(audioDataHandle)) {
        return INVALID_VOICE_IDX;
    }

    AudioDeviceLock lockAudioDevice(*mpAudioOutputDevice);

    // If specified, stop other instances of this sound
    if (bStopOtherInstances) {
        const uint32_t numVoices = (uint32_t) mVoices.size();

        for (uint32_t voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx) {
            AudioVoice& voice = mVoices[voiceIdx];

            if (voice.state != AudioVoice::State::STOPPED) {
                if (voice.audioDataHandle == audioDataHandle) {
                    voice.state = AudioVoice::State::STOPPED;
                    mFreeVoices.push_back(voiceIdx);
                }
            }
        }
    }

    // Abort if there are no more free voices
    if (mFreeVoices.empty()) {
        return INVALID_VOICE_IDX;
    }

    // Consume a voice and play
    const VoiceIdx voiceIdx = mFreeVoices.back();
    mFreeVoices.pop_back();

    AudioVoice& voice = mVoices[voiceIdx];
    voice.state = AudioVoice::State::PLAYING;
    voice.bIsLooped = bLooped;
    voice.curSampleFrac = 0;
    voice.curSample = 0;
    voice.audioDataHandle = audioDataHandle;
    voice.lVolume = lVolume;
    voice.rVolume = rVolume;

    // Return the voice playing
    return voiceIdx;
}

uint32_t AudioSystem::getNumVoicesWithAudioData(const uint32_t audioDataHandle) noexcept {
    uint32_t numVoicesMatching = 0;
    AudioDeviceLock lockAudioDevice(*mpAudioOutputDevice);
    
    for (const AudioVoice& voice : mVoices) {
        if (voice.audioDataHandle == audioDataHandle) {
            if (voice.state != AudioVoice::State::STOPPED) {
                ++numVoicesMatching;
            }
        }
    }

    return numVoicesMatching;
}

void AudioSystem::stopAllVoices() noexcept {
    ASSERT(mbIsInitialized);

    const uint32_t numVoices = (uint32_t) mVoices.size();
    AudioDeviceLock lockAudioDevice(*mpAudioOutputDevice);

    for (uint32_t voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx) {
        AudioVoice& voice = mVoices[voiceIdx];

        if (voice.state != AudioVoice::State::STOPPED) {
            voice.state = AudioVoice::State::STOPPED;
            mFreeVoices.push_back(voiceIdx);
        }
    }
}

void AudioSystem::stopVoice(const VoiceIdx voiceIdx) noexcept {
    ASSERT(mbIsInitialized);
    ASSERT(voiceIdx < getNumVoices());

    AudioVoice& voice = mVoices[voiceIdx];
    AudioDeviceLock lockAudioDevice(*mpAudioOutputDevice);

    if (voice.state != AudioVoice::State::STOPPED) {
        voice.state = AudioVoice::State::STOPPED;
        mFreeVoices.push_back(voiceIdx);
    }
}

void AudioSystem::stopVoicesWithAudioData(const uint32_t audioDataHandle) noexcept {
    ASSERT(mbIsInitialized);

    const uint32_t numVoices = (uint32_t) mVoices.size();
    AudioDeviceLock lockAudioDevice(*mpAudioOutputDevice);

    for (uint32_t voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx) {
        AudioVoice& voice = mVoices[voiceIdx];

        if (voice.audioDataHandle == audioDataHandle) {
            if (voice.state != AudioVoice::State::STOPPED) {
                voice.state = AudioVoice::State::STOPPED;
                mFreeVoices.push_back(voiceIdx);
            }
        }
    }
}

void AudioSystem::mixAudio(float* const pSamples, const uint32_t numSamples) noexcept {
    ASSERT(mbIsInitialized);
    ASSERT(pSamples);
    ASSERT(numSamples > 0);

    const uint32_t numVoices = getNumVoices();

    for (uint32_t voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx) {
        // Skip the voice if it is not active
        AudioVoice& voice = mVoices[voiceIdx];

        if (voice.state != AudioVoice::State::PLAYING)
            continue;

        // Skip and stop the sound if the audio data is not valid
        const AudioData* const pAudioData = mpAudioDataMgr->getHandleData(voice.audioDataHandle);

        if (!pAudioData) {
            voice.state = AudioVoice::State::STOPPED;
            mFreeVoices.push_back(voiceIdx);
            continue;
        }

        // Mix in the voice audio
        mixVoiceAudio(voice, *pAudioData, pSamples, numSamples);

        // If the voice is done playing now then add it to the free list
        if (voice.state == AudioVoice::State::STOPPED) {
            mFreeVoices.push_back(voiceIdx);
        }
    }
}

void AudioSystem::removeFromFreeVoiceList(const VoiceIdx voiceIdx) noexcept {
    // N.B: This call assumes all locking of the audio device has already been done externally
    const uint32_t numFreeVoices = (uint32_t) mFreeVoices.size();
    uint32_t freeListIdx = UINT32_MAX;

    for (uint32_t i = 0; i < numFreeVoices; ++i) {
        if (mFreeVoices[i] == voiceIdx) {
            freeListIdx = i;
            break;
        }
    }

    if (freeListIdx < numFreeVoices) {
        // Remove by swapping the back voice into place and popping
        mFreeVoices[freeListIdx] = mFreeVoices.back();
        mFreeVoices.pop_back();
    }
}

void AudioSystem::mixVoiceAudio(
    AudioVoice& voice,
    const AudioData& audioData,
    float* const pSamples,
    const uint32_t numSamples
) noexcept {
    // Gathering some useful info
    const uint32_t inSampleRate = audioData.sampleRate;
    const uint32_t outSampleRate = mpAudioOutputDevice->getSampleRate();
    const uint32_t totalInSamples = audioData.numSamples;

    // Figure out how many input samples to step per output sample in 32.16 fixed point format
    uint64_t sampleStepFrac;

    {
        uint64_t inSampleRate32_16 = uint64_t(inSampleRate) << 16;
        uint64_t outSampleRate32_16 = uint64_t(outSampleRate) << 16;
        sampleStepFrac = (inSampleRate32_16 << 16) / outSampleRate32_16;
    }

    // Figure out the current sample location in the audio in 32.16 format
    uint64_t curSampleFrac = (uint64_t(voice.curSample) << 16) | uint64_t(voice.curSampleFrac);

    // Now, continue to sample audio
    float* pCurOutSample = pSamples;
    float* const pEndOutSample = pSamples + (numSamples * 2);

    while (pCurOutSample < pEndOutSample) {
        // See if we are over the end of the input.
        // If the sound is not looped then we are done playback, otherwise we wraparound.
        uint32_t curSample = uint32_t(curSampleFrac >> 16);

        if (curSample >= totalInSamples) {
            if (!voice.bIsLooped) {
                voice.state = AudioVoice::State::STOPPED;
                voice.curSample = audioData.numSamples;
                voice.curSampleFrac = 0;
                break;
            }

            curSample = curSample % totalInSamples;
        }

        // Get the next sample to interpolate with.
        // Will interpolate between the two sample values depending on fraction between samples.
        uint32_t nextSample = (uint16_t(curSampleFrac) != 0) ? curSample + 1 : curSample;

        if (nextSample >= totalInSamples) {
            nextSample = (voice.bIsLooped) ? 0 : totalInSamples - 1;
        }

        // Figure out interpolation value between samples
        const float sampleLerp = float(uint16_t(curSampleFrac)) / 65536.0f;

        // Read the actual samples
        float sample1L;
        float sample1R;
        float sample2L;
        float sample2R;

        if (audioData.numChannels == 1) {
            if (audioData.bitDepth == 8) {
                const int8_t rawSample1 = ((const int8_t*) audioData.pBuffer)[curSample];
                const int8_t rawSample2 = ((const int8_t*) audioData.pBuffer)[nextSample];
                sample1L = float(rawSample1) / float(INT8_MAX);
                sample2L = float(rawSample2) / float(INT8_MAX);
                sample1R = sample1L;
                sample2R = sample2L;
            }
            else {
                ASSERT(audioData.bitDepth == 16);

                const int16_t rawSample1 = ((const int16_t*) audioData.pBuffer)[curSample];
                const int16_t rawSample2 = ((const int16_t*) audioData.pBuffer)[nextSample];
                sample1L = float(rawSample1) / float(INT16_MAX);
                sample2L = float(rawSample2) / float(INT16_MAX);
                sample1R = sample1L;
                sample2R = sample2L;
            }
        }
        else {
            ASSERT(audioData.numChannels == 2);

            if (audioData.bitDepth == 8) {
                const int8_t rawSample1L = ((const int8_t*) audioData.pBuffer)[curSample * 2 + 0];
                const int8_t rawSample1R = ((const int8_t*) audioData.pBuffer)[curSample * 2 + 1];
                const int8_t rawSample2L = ((const int8_t*) audioData.pBuffer)[nextSample * 2 + 0];
                const int8_t rawSample2R = ((const int8_t*) audioData.pBuffer)[nextSample * 2 + 1];
                sample1L = float(rawSample1L) / float(INT8_MAX);
                sample1R = float(rawSample1R) / float(INT8_MAX);
                sample2L = float(rawSample2L) / float(INT8_MAX);
                sample2R = float(rawSample2R) / float(INT8_MAX);
            }
            else {
                ASSERT(audioData.bitDepth == 16);

                const int16_t rawSample1L = ((const int16_t*) audioData.pBuffer)[curSample * 2 + 0];
                const int16_t rawSample1R = ((const int16_t*) audioData.pBuffer)[curSample * 2 + 1];
                const int16_t rawSample2L = ((const int16_t*) audioData.pBuffer)[nextSample * 2 + 0];
                const int16_t rawSample2R = ((const int16_t*) audioData.pBuffer)[nextSample * 2 + 1];
                sample1L = float(rawSample1L) / float(INT16_MAX);
                sample1R = float(rawSample1R) / float(INT16_MAX);
                sample2L = float(rawSample2L) / float(INT16_MAX);
                sample2R = float(rawSample2R) / float(INT16_MAX);
            }
        }

        // Interpolate and save the samples, modulating by volume and panning
        const float sampleL = (1.0f - sampleLerp) * sample1L + sampleLerp * sample2L;
        const float sampleR = (1.0f - sampleLerp) * sample1R + sampleLerp * sample2R;
        const float finalSampleL = sampleL * voice.lVolume;
        const float finalSampleR = sampleR * voice.rVolume;

        pCurOutSample[0] += finalSampleL * mMasterVolume;
        pCurOutSample[1] += finalSampleR * mMasterVolume;

        // Move along in the input and output sound
        curSampleFrac += sampleStepFrac;
        pCurOutSample += 2;
    }

    // At the end of this bout of playback check if we are stopped.
    // If not stopped then we can save the current position
    {
        uint32_t curSample = (uint32_t)(curSampleFrac >> 16);

        if (curSample >= totalInSamples) {
            if (!voice.bIsLooped) {
                voice.state = AudioVoice::State::STOPPED;
                voice.curSample = audioData.numSamples;
                voice.curSampleFrac = 0;
            }
            else {
                curSample = curSample % totalInSamples;
                voice.curSample = curSample;
                voice.curSampleFrac = (uint16_t) curSampleFrac;
            }
        }
        else {
            voice.curSample = curSample;
            voice.curSampleFrac = (uint16_t) curSampleFrac;
        }
    }
}
