#pragma once

#include "Base/Macros.h"
#include <cstdint>

class AudioDataMgr;
class AudioSystem;

//----------------------------------------------------------------------------------------------------------------------
// Functionality for playing music and sound
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(Audio)

// Init and shutdown
void init() noexcept;
void loadAllSounds() noexcept;
void shutdown() noexcept;

// Sounds
uint32_t playSound(
    const uint32_t num,
    const float lVolume,
    const float rVolume,
    const bool bStopOtherInstances = false
) noexcept;

bool isSoundPlaying(const uint32_t num) noexcept;
void stopVoice(const uint32_t voiceIdx) noexcept;
void stopAllInstancesOfSound(const uint32_t num) noexcept;
void stopAllSounds() noexcept;
void pauseAllSounds() noexcept;
void resumeAllSounds() noexcept;

// Music
void playMusic(const uint32_t trackNum) noexcept;
void stopMusic() noexcept;
void pauseMusic() noexcept;
void resumeMusic() noexcept;

// Volume
static const uint32_t MAX_VOLUME = 15;

uint32_t getMusicVolume() noexcept;
void setMusicVolume(const uint32_t volume) noexcept;
uint32_t getSoundVolume() noexcept;
void setSoundVolume(const uint32_t volume) noexcept;

// Low level access
AudioDataMgr& getAudioDataMgr() noexcept;
AudioSystem& getSoundAudioSystem() noexcept;
AudioSystem& getMusicAudioSystem() noexcept;

END_NAMESPACE(Audio)
