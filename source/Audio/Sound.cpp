#include "Sound.h"

#include "Audio.h"
#include "Base/Tables.h"
#include "Game/Data.h"
#include "Map/MapUtil.h"
#include "Sounds.h"
#include "Things/MapObj.h"

constexpr Fixed     S_CLIPPING_DIST = 3600 * 0x10000;       // Clip sounds beyond this distance
constexpr Fixed     S_CLOSE_DIST    = 200 * 0x10000;        // Sounds at this distance or closer are full volume sounds
constexpr int32_t   S_ATTENUATOR    = (S_CLIPPING_DIST - S_CLOSE_DIST) >> FRACBITS;
constexpr Fixed     S_STEREO_SWING  = 96 * 0x10000;

//----------------------------------------------------------------------------------------------------------------------
// Clear the sound buffers and stop all sound
//----------------------------------------------------------------------------------------------------------------------
void S_Clear() noexcept {
    Audio::stopAllSounds();
}

//----------------------------------------------------------------------------------------------------------------------
// Start a new sound: uses the view origin to affect the stereo panning and volume.
//----------------------------------------------------------------------------------------------------------------------
uint32_t S_StartSound(const Fixed* const pOriginXY, const uint32_t soundId, const bool bStopOtherInstances) noexcept {
    if (soundId <= 0 || soundId >= NUMSFX)
        return UINT32_MAX;

    // Figure out the volume of the sound to play
    uint32_t leftVolume = 255;
    uint32_t rightVolume = 255;

    if (pOriginXY) {
        const mobj_t* const pListener = gPlayer.mo;

        if (pOriginXY != &pListener->x) {
            const Fixed dist = GetApproxDistance(pListener->x - pOriginXY[0], pListener->y - pOriginXY[1]);

            if (dist > S_CLIPPING_DIST) {   // Too far away?
                return UINT32_MAX;
            }

            angle_t angle = PointToAngle(pListener->x, pListener->y, pOriginXY[0], pOriginXY[1]);
            angle = angle - pListener->angle;
            angle >>= ANGLETOFINESHIFT;

            if (dist >= S_CLOSE_DIST) {
                const int32_t vol = (255 * ((S_CLIPPING_DIST - dist) >> FRACBITS)) / S_ATTENUATOR;

                if (vol <= 0) { // Too quiet?
                    return UINT32_MAX;
                }

                const int sep = 128 - (fixed16Mul(S_STEREO_SWING, gFineSine[angle]) >> FRACBITS);
                rightVolume = (sep * vol) >> 8;
                leftVolume = ((256 - sep) * vol) >> 8;
            }
        }
    }

    // Convert audio volume to 0.0-1.0 range
    const float leftVolumeF = (float) leftVolume / 255.0f;
    const float rightVolumeF = (float) rightVolume / 255.0f;

    if (bStopOtherInstances) {
        return Audio::playSound(soundId, leftVolumeF, rightVolumeF, true);
    } else {
        return Audio::playSound(soundId, leftVolumeF, rightVolumeF, false);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Start music
//----------------------------------------------------------------------------------------------------------------------
static constexpr uint8_t SONG_LOOKUP[] = {
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

static constexpr uint32_t NUM_SONG_LOOKUPS = C_ARRAY_SIZE(SONG_LOOKUP);

void S_StartSong(const uint32_t musicId) noexcept {
    if (musicId < NUM_SONG_LOOKUPS) {
        const uint32_t trackNum = SONG_LOOKUP[musicId];
        Audio::playMusic(trackNum);
    }
}

void S_StopSong() noexcept {
    Audio::stopMusic();
}
