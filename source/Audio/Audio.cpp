#include "Audio.h"

#include "AudioDataMgr.h"
#include "AudioOutputDevice.h"
#include "AudioSystem.h"
#include "sounds.h"

static constexpr uint32_t MAX_SOUND_VOICES = 32;

static AudioOutputDevice        gAudioOutputDevice;
static AudioDataMgr             gAudioDataMgr;
static AudioSystem              gSoundAudioSystem;
static AudioDataMgr::Handle     gSoundHandles[NUMSFX];

extern "C" {

void audioInit() {   
    if (!gAudioOutputDevice.init()) {
        FATAL_ERROR("Unable to initialize an audio output device!");
    }

    gSoundAudioSystem.init(gAudioOutputDevice, gAudioDataMgr, MAX_SOUND_VOICES);
}

void audioLoadAllSounds() {    
    const uint32_t numSounds = NUMSFX;
    gSoundHandles[0] = AudioDataMgr::INVALID_HANDLE;    // The 'none' sound

    for (uint32_t soundNum = 1; soundNum < NUMSFX; ++soundNum) {
        char fileName[128];
        std::snprintf(fileName, sizeof(fileName), "Sounds/Sound%02d.aiff", int(soundNum));
        gSoundHandles[soundNum] = gAudioDataMgr.loadFile(fileName);
    }
}

void audioShutdown() {
    gSoundAudioSystem.shutdown();
    gAudioDataMgr.unloadAll();
    gAudioOutputDevice.shutdown();

    for (AudioDataMgr::Handle& handle : gSoundHandles) {
        handle = AudioDataMgr::INVALID_HANDLE;
    }
}

void audioPlaySound(const uint32_t num, float lVolume, float rVolume) {
    ASSERT(num < NUMSFX);
    const AudioDataMgr::Handle soundHandle = gSoundHandles[num];
    gSoundAudioSystem.play(soundHandle, false, lVolume, rVolume);
}

}
