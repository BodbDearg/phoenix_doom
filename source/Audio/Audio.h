#pragma once

#include <cstdint>

//--------------------------------------------------------------------------------------------------
// Functionality for playing music and sound
//--------------------------------------------------------------------------------------------------

// Init and shutdown
void audioInit();
void audioLoadAllSounds();
void audioShutdown();

// Sounds
void audioPlaySound(const uint32_t num, float lVolume, float rVolume);
void audioStopAllSounds();
void audioPauseSound();
void audioResumeSound();

// Music
void audioPlayMusic(const uint32_t trackNum);
void audioStopMusic();
void audioPauseMusic();
void audioResumeMusic();

// Volume
static const uint32_t MAX_AUDIO_VOLUME = 15;

uint32_t audioGetMusicVolume();
void audioSetMusicVolume(const uint32_t volume);
uint32_t audioGetSoundVolume();
void audioSetSoundVolume(const uint32_t volume);
