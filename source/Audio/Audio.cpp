#include "Audio.h"

#include "AudioDataMgr.h"
#include "AudioOutputDevice.h"
#include "AudioSystem.h"
#include "Sounds.h"

BEGIN_NAMESPACE(Audio)

static constexpr uint32_t MAX_SOUND_VOICES = 32;

// Audio device, audio data manager & sound systems
static AudioOutputDevice    gAudioOutputDevice;
static AudioDataMgr         gAudioDataMgr;
static AudioSystem          gSoundAudioSystem;
static AudioSystem          gMusicAudioSystem;

// Loaded sound and music
static AudioDataMgr::Handle gSoundAudioDataHandles[NUMSFX];
static AudioDataMgr::Handle gMusicAudioDataHandle = AudioDataMgr::INVALID_HANDLE;

// Other audio state
static uint32_t gMusicVolume = MAX_VOLUME;
static uint32_t gSoundVolume = MAX_VOLUME;
static uint32_t gPlayingMusicTrackNum = UINT32_MAX;

void init() noexcept {
    if (!gAudioOutputDevice.init()) {
        FATAL_ERROR("Unable to initialize an audio output device!");
    }

    gSoundAudioSystem.init(gAudioOutputDevice, gAudioDataMgr, MAX_SOUND_VOICES);
    gMusicAudioSystem.init(gAudioOutputDevice, gAudioDataMgr, 1);

    // Insure initial volume is set with the audio system
    setMusicVolume(gMusicVolume);
    setSoundVolume(gSoundVolume);
}

void loadAllSounds() noexcept {
    gSoundAudioDataHandles[0] = AudioDataMgr::INVALID_HANDLE;    // The 'none' sound

    for (uint32_t soundNum = 1; soundNum < NUMSFX; ++soundNum) {
        char fileName[128];
        std::snprintf(fileName, sizeof(fileName), "Sounds/Sound%02d.aiff", int(soundNum));
        gSoundAudioDataHandles[soundNum] = gAudioDataMgr.loadFile(fileName);
    }
}

void shutdown() noexcept {
    gMusicAudioSystem.shutdown();
    gSoundAudioSystem.shutdown();

    gAudioDataMgr.unloadAll();
    gAudioOutputDevice.shutdown();

    for (AudioDataMgr::Handle& handle : gSoundAudioDataHandles) {
        handle = AudioDataMgr::INVALID_HANDLE;
    }

    gMusicAudioDataHandle = AudioDataMgr::INVALID_HANDLE;
}

uint32_t playSound(
    const uint32_t num,
    const float lVolume,
    const float rVolume,
    const bool bStopOtherInstances
) noexcept {
    ASSERT(num < NUMSFX);
    const AudioDataMgr::Handle soundHandle = gSoundAudioDataHandles[num];
    return gSoundAudioSystem.play(soundHandle, false, lVolume, rVolume, bStopOtherInstances);
}

bool isSoundPlaying(const uint32_t num) noexcept {
    ASSERT(num < NUMSFX);
    const AudioDataMgr::Handle soundHandle = gSoundAudioDataHandles[num];
    return (gSoundAudioSystem.getNumVoicesWithAudioData(soundHandle) > 0);
}

void stopVoice(const uint32_t voiceIdx) noexcept {
    if (voiceIdx < gSoundAudioSystem.getNumVoices()) {
        gSoundAudioSystem.stopVoice(voiceIdx);
    }
}

void stopAllInstancesOfSound(const uint32_t num) noexcept {
    ASSERT(num < NUMSFX);
    const AudioDataMgr::Handle soundHandle = gSoundAudioDataHandles[num];
    gSoundAudioSystem.stopVoicesWithAudioData(soundHandle);
}

void stopAllSounds() noexcept {
    gSoundAudioSystem.stopAllVoices();
}

void pauseAllSounds() noexcept {
    gSoundAudioSystem.pause(true);
}

void resumeAllSounds() noexcept {
    gSoundAudioSystem.pause(false);
}

void playMusic(const uint32_t trackNum) noexcept {
    // If we are already playing this then don't need to do anything
    if (gPlayingMusicTrackNum == trackNum)
        return;

    // Load the song
    char fileName[128];
    std::snprintf(fileName, sizeof(fileName), "Music/Song%d", int(trackNum));

    const AudioDataMgr::Handle oldMusicHandle = gMusicAudioDataHandle;
    const AudioDataMgr::Handle newMusicHandle = gAudioDataMgr.loadFile(fileName);

    // Stop the old one and play the new one
    gMusicAudioSystem.stopAllVoices();
    gMusicAudioSystem.play(newMusicHandle, true);   // N.B: assuming it will play successfully always!

    // Unload the old song (if it's still loaded)
    if (oldMusicHandle != AudioDataMgr::INVALID_HANDLE) {
        if (oldMusicHandle != newMusicHandle) {
            gAudioDataMgr.unloadHandle(oldMusicHandle);
        }
    }

    // Remember what is playing
    gMusicAudioDataHandle = newMusicHandle;
    gPlayingMusicTrackNum = trackNum;
}

void stopMusic() noexcept {
    gMusicAudioSystem.stopAllVoices();
    gPlayingMusicTrackNum = UINT32_MAX;
}

void pauseMusic() noexcept {
    gMusicAudioSystem.pause(true);
}

void resumeMusic() noexcept {
    gMusicAudioSystem.pause(false);
}

uint32_t getMusicVolume() noexcept {
    return gMusicVolume;
}

void setMusicVolume(const uint32_t volume) noexcept {
    gMusicVolume = (volume > MAX_VOLUME) ? MAX_VOLUME : volume;

    if (gMusicAudioSystem.isInitialized()) {
        gMusicAudioSystem.setMasterVolume((float) gMusicVolume / (float) MAX_VOLUME);
    }
}

uint32_t getSoundVolume() noexcept {
    return gSoundVolume;
}

void setSoundVolume(const uint32_t volume) noexcept {
    gSoundVolume = (volume > MAX_VOLUME) ? MAX_VOLUME : volume;

    if (gSoundAudioSystem.isInitialized()) {
        gSoundAudioSystem.setMasterVolume((float) gSoundVolume / (float) MAX_VOLUME);
    }
}

AudioDataMgr& getAudioDataMgr() noexcept {
    return gAudioDataMgr;
}

AudioSystem& getSoundAudioSystem() noexcept {
    return gSoundAudioSystem;
}

AudioSystem& getMusicAudioSystem() noexcept {
    return gMusicAudioSystem;
}

END_NAMESPACE(Audio)
