#include "Audio.h"

#include "AudioDataMgr.h"
#include "AudioOutputDevice.h"
#include "AudioSystem.h"
#include "sounds.h"

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
static uint32_t gMusicVolume = MAX_AUDIO_VOLUME;
static uint32_t gSoundVolume = MAX_AUDIO_VOLUME;
static uint32_t gPlayingMusicTrackNum = UINT32_MAX;

extern "C" {

void audioInit() {   
    if (!gAudioOutputDevice.init()) {
        FATAL_ERROR("Unable to initialize an audio output device!");
    }

    gSoundAudioSystem.init(gAudioOutputDevice, gAudioDataMgr, MAX_SOUND_VOICES);
    gMusicAudioSystem.init(gAudioOutputDevice, gAudioDataMgr, 1);
    
    // Insure initial volume is set with the audio system
    audioSetMusicVolume(gMusicVolume);
    audioSetSoundVolume(gSoundVolume);
}

void audioLoadAllSounds() {    
    const uint32_t numSounds = NUMSFX;
    gSoundAudioDataHandles[0] = AudioDataMgr::INVALID_HANDLE;    // The 'none' sound

    for (uint32_t soundNum = 1; soundNum < NUMSFX; ++soundNum) {
        char fileName[128];
        std::snprintf(fileName, sizeof(fileName), "Sounds/Sound%02d.aiff", int(soundNum));
        gSoundAudioDataHandles[soundNum] = gAudioDataMgr.loadFile(fileName);
    }
}

void audioShutdown() {
    gMusicAudioSystem.shutdown();
    gSoundAudioSystem.shutdown();

    gAudioDataMgr.unloadAll();
    gAudioOutputDevice.shutdown();

    for (AudioDataMgr::Handle& handle : gSoundAudioDataHandles) {
        handle = AudioDataMgr::INVALID_HANDLE;
    }

    gMusicAudioDataHandle = AudioDataMgr::INVALID_HANDLE;
}

void audioPlaySound(const uint32_t num, float lVolume, float rVolume) {
    ASSERT(num < NUMSFX);
    const AudioDataMgr::Handle soundHandle = gSoundAudioDataHandles[num];
    gSoundAudioSystem.play(soundHandle, false, lVolume, rVolume);
}

void audioPauseSound() {
    gSoundAudioSystem.pause(true);
}

void audioResumeSound() {
    gSoundAudioSystem.pause(false);
}

void audioPlayMusic(const uint32_t trackNum) {
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

void audioStopMusic() {
    gMusicAudioSystem.stopAllVoices();
    gPlayingMusicTrackNum = UINT32_MAX;
}

void audioPauseMusic() {
    gMusicAudioSystem.pause(true);
}

void audioResumeMusic() {
    gMusicAudioSystem.pause(false);
}

uint32_t audioGetMusicVolume() {
    return gMusicVolume;
}

void audioSetMusicVolume(const uint32_t volume) {
    gMusicVolume = (volume > MAX_AUDIO_VOLUME) ? MAX_AUDIO_VOLUME : volume;

    if (gMusicAudioSystem.isInitialized()) {
        gMusicAudioSystem.setMasterVolume((float) gMusicVolume / (float) MAX_AUDIO_VOLUME);
    }
}

uint32_t audioGetSoundVolume() {
    return gSoundVolume;
}

void audioSetSoundVolume(const uint32_t volume) {
    gSoundVolume = (volume > MAX_AUDIO_VOLUME) ? MAX_AUDIO_VOLUME : volume;

    if (gSoundAudioSystem.isInitialized()) {
        gSoundAudioSystem.setMasterVolume((float) gSoundVolume / (float) MAX_AUDIO_VOLUME);
    }
}

}
