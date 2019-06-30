#include "Audio/Audio.h"
#include "Data.h"
#include "MapObj.h"
#include "MapUtil.h"
#include "MathUtils.h"
#include "Player.h"
#include "Sounds.h"
#include "Tables.h"

#define S_CLIPPING_DIST (3600 * 0x10000)    // Clip sounds beyond this distance
#define S_CLOSE_DIST (200 * 0x10000)        // Sounds at this distance or closer are full volume sounds

#define S_ATTENUATOR ((S_CLIPPING_DIST - S_CLOSE_DIST) >> FRACBITS)
#define S_STEREO_SWING (96 * 0x10000)

//--------------------------------------------------------------------------------------------------
// Clear the sound buffers and stop all sound
//--------------------------------------------------------------------------------------------------
void S_Clear() {
    audioStopAllSounds();
}

//--------------------------------------------------------------------------------------------------
// Start a new sound.
// Uses the view origin to affect the stereo panning and volume.
//--------------------------------------------------------------------------------------------------
void S_StartSound(const Fixed* const pOriginXY, const uint32_t soundId) {
    if (soundId <= 0 || soundId >= NUMSFX)
        return;
    
    // Figure out the volume of the sound to play
    uint32_t leftVolume = 255;
    uint32_t rightVolume = 255;
    
    if (pOriginXY) {
        const mobj_t* const pListener = players.mo;

        if (pOriginXY != &pListener->x) {
            const Fixed dist = GetApproxDistance(pListener->x - pOriginXY[0], pListener->y - pOriginXY[1]);
            
            if (dist > S_CLIPPING_DIST)     // Too far away?
                return;
            
            angle_t angle = PointToAngle(pListener->x, pListener->y, pOriginXY[0], pOriginXY[1]);
            angle = angle - pListener->angle;
            angle >>= ANGLETOFINESHIFT;

            if (dist >= S_CLOSE_DIST) {
                const int vol = (255 * ((S_CLIPPING_DIST - dist) >> FRACBITS)) / S_ATTENUATOR;

                if (vol <= 0)   // Too quiet?
                    return;
                
                const int sep = 128 - (sfixedMul16_16(S_STEREO_SWING, finesine[angle]) >> FRACBITS);
                rightVolume = (sep * vol) >> 8;
                leftVolume = ((256 - sep) * vol) >> 8;
            }
        }
    }

    // Convert audio volume to 0.0-1.0 range
    const float leftVolumeF = (float) leftVolume / 255.0f;
    const float rightVolumeF = (float) rightVolume / 255.0f;

    // If the mysterious '0x8000U' flag is set then all previous instances of the sound are to be stopped
    if (soundId & (uint32_t) 0x8000U) {
        audioPlaySound(soundId & (uint32_t) 0x7FFFU, leftVolumeF, rightVolumeF);
    }
    else {
        audioPlaySound(soundId, leftVolumeF, rightVolumeF);
    }
}

//--------------------------------------------------------------------------------------------------
// Start music
//--------------------------------------------------------------------------------------------------
static const uint8_t SONG_LOOKUP[] = {
    0,      // -- NO SONG --
    11,     // Intro
    12,     // Final
    3,      // Bunny
    5,      // Intermission
    5,      // Map 01
    6,      // Map 02
    7,      // Map 03
    8,      // Map 04
    9,      // Map 05
    10,     // Map 06
    11,     // Map 07
    12,     // Map 08
    13,     // Map 09
    14,     // Map 10
    15,     // Map 11
    5,      // Map 12
    6,      // Map 13
    7,      // Map 14
    8,      // Map 15
    9,      // Map 16
    10,     // Map 17
    11,     // Map 18
    13,     // Map 19
    14,     // Map 20
    15,     // Map 21
    10,     // Map 22
    12,     // Map 23
    29,     // Map 24
    1,      // .. ?
    1,      // .. ?
    1,      // .. ?
    1,      // .. ?
    1,      // .. ?
    1,      // .. ?
    1       // .. ?
};

void S_StartSong(const uint8_t musicId) {
    const uint32_t trackNum = SONG_LOOKUP[musicId];    
    audioPlayMusic(trackNum);
}

void S_StopSong() {
    audioStopMusic();
}
