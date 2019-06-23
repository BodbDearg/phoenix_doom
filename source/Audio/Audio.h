#pragma once

#include <stdint.h>

//--------------------------------------------------------------------------------------------------
// Functionality for playing music and sound
//--------------------------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

void audioInit();
void audioLoadAllSounds();
void audioShutdown();

void audioPlaySound(const uint32_t num, float lVolume, float rVolume);

#ifdef __cplusplus
}
#endif
