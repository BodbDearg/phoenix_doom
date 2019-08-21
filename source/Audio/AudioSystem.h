#pragma once

#include "AudioVoice.h"
#include <cstdint>
#include <vector>

class AudioDataMgr;
class AudioOutputDevice;
struct AudioData;

//--------------------------------------------------------------------------------------------------
// Manages a collection of playing audio voices.
// Also has a master volume and pause setting for the system.
//--------------------------------------------------------------------------------------------------
class AudioSystem {
public:
    // Default master volume
    static constexpr float DEFAULT_MASTER_VOLUME = 1.0f;

    // Voice index type and voice index returned to indicate an invalid voice
    typedef uint32_t VoiceIdx;
    static constexpr VoiceIdx INVALID_VOICE_IDX = UINT32_MAX;

    AudioSystem() noexcept;
    ~AudioSystem() noexcept;

    //----------------------------------------------------------------------------------------------
    // Notes:
    //  (1) The given data manager MUST remain valid for the lifetime of the audio system.
    //  (2) The given device MUST remain valid for the lifetime of the audio system.
    //----------------------------------------------------------------------------------------------
    void init(AudioOutputDevice& device, AudioDataMgr& dataMgr, const uint32_t maxVoices = 32) noexcept;
    void shutdown() noexcept;
    inline bool isInitialized() const noexcept { return mbIsInitialized; }

    inline AudioOutputDevice* getAudioOutputDevice() const { return mpAudioOutputDevice; }
    inline AudioDataMgr* getAudioDataMgr() const { return mpAudioDataMgr; }

    //----------------------------------------------------------------------------------------------
    // Get or set the state of a particular voice and query the number of voices
    //----------------------------------------------------------------------------------------------
    inline uint32_t getNumVoices() const { return (uint32_t) mVoices.size(); }
    AudioVoice getVoiceState(const VoiceIdx voiceIdx) const noexcept;
    void setVoiceState(const VoiceIdx voiceIdx, const AudioVoice& state) noexcept;

    //----------------------------------------------------------------------------------------------
    // Get or set the master volume for the system
    //----------------------------------------------------------------------------------------------
    inline float getMasterVolume() const { return mMasterVolume; }
    void setMasterVolume(const float volume) noexcept;

    //----------------------------------------------------------------------------------------------
    // Pause or unpause the entire system and query if paused
    //----------------------------------------------------------------------------------------------
    inline bool isPaused() const noexcept { return mbIsPaused; }
    void pause(const bool pause) noexcept;

    //----------------------------------------------------------------------------------------------
    // Try to play a particular audio piece with the given handle.
    // Returns the index of the voice allocated to the audio, or 'INVALID_VOICE_IDX' on failure.
    // Optionally, you can specify to stop other instances of the same sound.
    //----------------------------------------------------------------------------------------------
    VoiceIdx play(
        const uint32_t audioDataHandle,
        const bool bLooped = false,
        const float lVolume = 1.0f,
        const float rVolume = 1.0f,
        const bool bStopOtherInstances = false
    ) noexcept;

    //----------------------------------------------------------------------------------------------
    // Tell how many non stopped voices have the given audio data
    //----------------------------------------------------------------------------------------------
    uint32_t getNumVoicesWithAudioData(const uint32_t audioDataHandle) noexcept;

    //----------------------------------------------------------------------------------------------
    // Stop all voices in the system, a particular voice or voices with a particular sound
    //----------------------------------------------------------------------------------------------
    void stopAllVoices() noexcept;
    void stopVoice(const VoiceIdx voiceIdx) noexcept;
    void stopVoicesWithAudioData(const uint32_t audioDataHandle) noexcept;

    //----------------------------------------------------------------------------------------------
    // Called by the audio output device on the audio thread.
    // Should *NEVER* be called on any other thread as it already assumes the audio device is locked!
    //
    // This function should mix in (add) the requested number of audio samples in 2 channel stero
    // at the sample rate that the current audio device uses. The left/right stereo data should also
    // be interleaved for each sample point.
    //----------------------------------------------------------------------------------------------
    void mixAudio(float* const pSamples, const uint32_t numSamples) noexcept;

private:
    void removeFromFreeVoiceList(const VoiceIdx voiceIdx) noexcept;

    void mixVoiceAudio(
        AudioVoice& voice,
        const AudioData& audioData,
        float* const pSamples,
        const uint32_t numSamples
    ) noexcept;

    bool                        mbIsInitialized;
    bool                        mbIsPaused;
    AudioOutputDevice*          mpAudioOutputDevice;
    AudioDataMgr*               mpAudioDataMgr;
    float                       mMasterVolume;
    std::vector<AudioVoice>     mVoices;
    std::vector<uint32_t>       mFreeVoices;
};
