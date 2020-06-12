#include "Lights.h"

#include "Base/Random.h"
#include "Game/Tick.h"
#include "MapData.h"
#include "Specials.h"

// Struct for light flashers
struct lightflash_t {
    sector_t* sector;       // Sector to affect
    uint32_t count;         // Timer
    uint32_t maxlight;      // Light level for bright flash
    uint32_t minlight;      // Light level for ambient light
    uint32_t maxtime;       // Time to hold bright light level
    uint32_t mintime;       // Time to hold ambient light level
};

// Struct for strobe lights
struct strobe_t {
    sector_t* sector;       // Sector to affect
    uint32_t count;         // Timer
    uint32_t maxlight;      // Light level for bright flash
    uint32_t minlight;      // Light level for ambient light
    uint32_t brighttime;    // Time to hold bright light
    uint32_t darktime;      // Time to hold ambient light
};

// Struct for glowing room
struct glow_t {
    sector_t* sector;       // Sector to affect
    uint32_t minlight;      // Minimum light
    uint32_t maxlight;      // Maximum light
    int32_t direction;      // Direction of light change
};

//------------------------------------------------------------------------------------------------------------------------------------------
// Think logic for lighting effect of a dim occasional flash.
// (Used to pretend you have a defective flourecent bulb)
//------------------------------------------------------------------------------------------------------------------------------------------
static void T_LightFlash(lightflash_t& flash) noexcept {
    if (flash.count > 1) {
        --flash.count;
        return;
    }

    if (flash.sector->lightlevel == flash.maxlight) {                           // Bright already?
        flash.sector->lightlevel = flash.minlight;                              // Go dim
        flash.count = convertPcTicks(Random::nextU8() & flash.mintime) + 1;     // Time effect
    } else {
        flash.sector->lightlevel = flash.maxlight;                              // Set bright
        flash.count = convertPcTicks(Random::nextU8() & flash.maxtime) + 1;     // Time
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Create a light flasher.
//------------------------------------------------------------------------------------------------------------------------------------------
void P_SpawnLightFlash(sector_t& sector) noexcept {
    sector.special = 0;     // Nothing special about it during gameplay

    lightflash_t& flash = *(lightflash_t*) AddThinker((ThinkerFunc) T_LightFlash, sizeof(lightflash_t));

    flash.sector = &sector;                                                     // Sector to affect
    flash.maxlight = sector.lightlevel;                                         // Use existing light as max
    flash.minlight = P_FindMinSurroundingLight(sector, sector.lightlevel);
    flash.maxtime = 64;                                                         // Time to hold light (N.B: PC ticks!)
    flash.mintime = 7;                                                          // N.B: PC ticks!
    flash.count = Random::nextU32(convertPcTicks(flash.maxtime)) + 1;           // Init timer
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Think logic for strobe flash lights
//------------------------------------------------------------------------------------------------------------------------------------------
static void T_StrobeFlash(strobe_t& flash) noexcept {
    if (flash.count > 1) {      // Time up?
        --flash.count;          // Count down
        return;                 // Exit
    }

    if (flash.sector->lightlevel == flash.minlight) {   // Already dim?
        flash.sector->lightlevel = flash.maxlight;      // Make it bright
        flash.count = flash.brighttime;                 // Reset timer
    } else {
        flash.sector->lightlevel = flash.minlight;      // Make it dim
        flash.count = flash.darktime;                   // Reset timer
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Create a strobe light thinker
//------------------------------------------------------------------------------------------------------------------------------------------
void P_SpawnStrobeFlash(sector_t& sector, const uint32_t fastOrSlow, const bool bInSync) noexcept {
    strobe_t& flash = AddThinker(T_StrobeFlash);

    flash.sector = &sector;                 // Set the thinker
    flash.darktime = fastOrSlow;            // Save the time delay
    flash.brighttime = STROBEBRIGHT;        // Time for bright light
    flash.maxlight = sector.lightlevel;     // Maximum light level
    flash.minlight = P_FindMinSurroundingLight(sector, sector.lightlevel);

    if (flash.minlight == flash.maxlight) {     // No differance in light?
        flash.minlight = 0;                     // Pitch black then
    }

    sector.special = 0;     // Nothing special about it during gameplay

    if (!bInSync) {
        flash.count = Random::nextU32(7) + 1;   // Start at a random time
    } else {
        flash.count = 1;    // Start at a fixed time
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Start strobing lights (usually from a trigger)
//------------------------------------------------------------------------------------------------------------------------------------------
void EV_StartLightStrobing(line_t& line) noexcept {
    uint32_t secnum = UINT32_MAX;

    while ((secnum = P_FindSectorFromLineTag(line, secnum)) != UINT32_MAX) {
        sector_t& sec = gpSectors[secnum];
        if (!sec.specialdata) {                         // Something here?
            P_SpawnStrobeFlash(sec,SLOWDARK, false);    // Start a flash
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Turn line's tag lights off
//------------------------------------------------------------------------------------------------------------------------------------------
void EV_TurnTagLightsOff(line_t& line) noexcept {
    const uint32_t tag = line.tag;
    sector_t* pSector = gpSectors;
    uint32_t sectorsLeft = gNumSectors;

    do {
        if (pSector->tag == tag) {
            uint32_t min = pSector->lightlevel;         // Lowest light level found: start with the current light level
            uint32_t linesLeft = pSector->linecount;
            line_t** ppLines = pSector->lines;

            do {
                sector_t* const pJoinedSector = getNextSector(*ppLines[0], *pSector);
                if (pJoinedSector) {
                    if (pJoinedSector->lightlevel < min) {
                        min = pJoinedSector->lightlevel;
                    }
                }
                ++ppLines;
            } while (--linesLeft);          // All done?

            pSector->lightlevel = min;      // Get the lowest light level
        }
        ++pSector;
    } while (--sectorsLeft);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Turn line's tag lights on
//------------------------------------------------------------------------------------------------------------------------------------------
void EV_LightTurnOn(line_t& line, const uint32_t bright) noexcept {
    const uint32_t tag = line.tag;
    sector_t* pSector = gpSectors;
    uint32_t sectorsLeft = gNumSectors;

    do {
        if (pSector->tag == tag) {
            // bright = 0 means to search for highest light level surrounding sector
            uint32_t lightLevel = bright;

            if (lightLevel == 0) {
                uint32_t linesLeft = pSector->linecount;
                line_t** ppLines = pSector->lines;

                do {
                    sector_t* pJoinedSector = getNextSector(*ppLines[0], *pSector);
                    if (pJoinedSector) {
                        if (pJoinedSector->lightlevel > lightLevel) {
                            lightLevel = pJoinedSector->lightlevel;
                        }
                    }
                    ++ppLines;
                } while (--linesLeft);
            }

            pSector->lightlevel = lightLevel;
        }
        ++pSector;
    } while (--sectorsLeft);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Glowing light thinker function
//------------------------------------------------------------------------------------------------------------------------------------------
static void T_Glow(glow_t* pGlow) noexcept {
    switch(pGlow->direction) {
        case -1:    // DOWN
            pGlow->sector->lightlevel -= GLOWSPEED;
            if ((pGlow->sector->lightlevel & 0x8000) || pGlow->sector->lightlevel <= pGlow->minlight) {
                pGlow->sector->lightlevel = pGlow->minlight;
                pGlow->direction = 1;
            }
            break;

        case 1:     // UP
            pGlow->sector->lightlevel += GLOWSPEED;
            if (pGlow->sector->lightlevel >= pGlow->maxlight) {
                pGlow->sector->lightlevel = pGlow->maxlight;
                pGlow->direction = -1;
            }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Spawn glowing light
//------------------------------------------------------------------------------------------------------------------------------------------
void P_SpawnGlowingLight(sector_t& sector) noexcept {
    glow_t& g = *(glow_t*) AddThinker((ThinkerFunc) T_Glow, sizeof(glow_t));

    g.sector = &sector;
    g.minlight = P_FindMinSurroundingLight(sector, sector.lightlevel);
    g.maxlight = sector.lightlevel;
    g.direction = -1;                   // Darken
    sector.special = 0;                 // Nothing special here
}
