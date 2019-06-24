#include "Audio.h"

#include "AudioDataMgr.h"
#include "AudioOutputDevice.h"
#include "AudioSystem.h"
#include "sounds.h"

static constexpr uint32_t MAX_SOUND_VOICES = 32;

static AudioOutputDevice        gAudioOutputDevice;
static AudioDataMgr             gAudioDataMgr;
static AudioSystem              gSoundAudioSystem;
static AudioSystem              gMusicAudioSystem;
static AudioDataMgr::Handle     gSoundAudioDataHandles[NUMSFX];
static AudioDataMgr::Handle     gMusicAudioDataHandle = AudioDataMgr::INVALID_HANDLE;
static uint32_t                 gPlayingMusicTrackNum = UINT32_MAX;

extern "C" {

void audioInit() {   
    if (!gAudioOutputDevice.init()) {
        FATAL_ERROR("Unable to initialize an audio output device!");
    }

    gSoundAudioSystem.init(gAudioOutputDevice, gAudioDataMgr, MAX_SOUND_VOICES);
    gMusicAudioSystem.init(gAudioOutputDevice, gAudioDataMgr, 1);
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
    const AudioDataMgr::Handle songAudioDataHandle = gAudioDataMgr.loadFile(fileName);

    // Stop the old one and play the new one
    gMusicAudioSystem.stopAllVoices();
    gMusicAudioSystem.play(songAudioDataHandle, true);  // N.B: assuming it will play successfully always!

    // Unload the old song and make a note of the new one
    if (gMusicAudioDataHandle != AudioDataMgr::INVALID_HANDLE) {
        gAudioDataMgr.unloadHandle(gMusicAudioDataHandle);
    }

    gMusicAudioDataHandle = songAudioDataHandle;
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

}
