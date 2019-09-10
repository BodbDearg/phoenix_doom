#pragma once

#include "Base/Fixed.h"

//----------------------------------------------------------------------------------------------------------------------
// Global defines for Doom
//----------------------------------------------------------------------------------------------------------------------

// These affect the folder that save data and config preferences go into:
static constexpr const char* const SAVE_FILE_ORG = "com.codelobster";
static constexpr const char* const SAVE_FILE_PRODUCT = "phoenix_doom";

// View related
static constexpr uint32_t   MAXSCREENHEIGHT = 160;                  // Maximum height allowed
static constexpr uint32_t   MAXSCREENWIDTH  = 280;                  // Maximum width allowed
static constexpr Fixed      VIEWHEIGHT      = 41 * FRACUNIT;        // Height to render from

// Gameplay/simulation related constants
static constexpr uint32_t   TICKSPERSEC     = 60;                           // The game timebase (ticks per second)
static constexpr float      SECS_PER_TICK   = 1.0f / (float) TICKSPERSEC;   // The number of seconds per tick
static constexpr uint32_t   MAPBLOCKSHIFT   = FRACBITS + 7;                 // Shift value to convert Fixed to 128 pixel blocks
static constexpr Fixed      ONFLOORZ        = FRACMIN;                      // Attach object to floor with this z
static constexpr Fixed      ONCEILINGZ      = FRACMAX;                      // Attach object to ceiling with this z
static constexpr Fixed      GRAVITY         = (FRACUNIT * 35) / 60;         // Rate of fall (DC: bugfix - convert to 3DO timebase properly. Fall speed in 3DO version was too much originally!)
static constexpr Fixed      MAXMOVE         = 16 * FRACUNIT;                // Maximum velocity
static constexpr Fixed      MAXRADIUS       = 32 * FRACUNIT;                // Largest radius of any critter
static constexpr Fixed      MELEERANGE      = 70 * FRACUNIT;                // Range of hand to hand combat
static constexpr Fixed      MISSILERANGE    = 32 * 64 * FRACUNIT;           // Range of guns targeting
static constexpr Fixed      FLOATSPEED      = 8 * FRACUNIT;                 // Speed an object can float vertically
static constexpr Fixed      SKULLSPEED      = 40 * FRACUNIT;                // Speed of the skull to attack

// Graphics
static constexpr float  MF_SHADOW_ALPHA         = 0.5f;     // Alpha to render things with that have the 'MF_SHADOW' map thing flag applied
static constexpr float  MF_SHADOW_COLOR_MULT    = 0.1f;     // Color multiply to render things with that have the 'MF_SHADOW' map thing flag applied

// Misc
static constexpr uint32_t   SKY_CEILING_PIC = UINT32_MAX;           // Texture number that indicates a sky texture

//----------------------------------------------------------------------------------------------------------------------
// Globa enums
//----------------------------------------------------------------------------------------------------------------------

// Index for a bounding box coordinate
enum {
    BOXTOP,
    BOXBOTTOM,
    BOXLEFT,
    BOXRIGHT,
    BOXCOUNT
};

//----------------------------------------------------------------------------------------------------------------------
// A utility to convert a tick count from PC Doom's original 35Hz timebase to the timebase used by this game version.
// Tries to round so the answer is as close as possible.
//----------------------------------------------------------------------------------------------------------------------
static inline constexpr uint32_t convertPcTicks(const uint32_t ticks35Hz) noexcept {
    // Get the tick count in 31.1 fixed point format by multiplying by TICKSPERSEC/35 (in 31.1 format).
    // When returning the integer answer round up if the fractional part is '.5':
    const uint32_t tickCountFixed = ((ticks35Hz * uint32_t(TICKSPERSEC)) << 2) / (uint32_t(35) << 1);
    return (tickCountFixed & 1) ? (tickCountFixed >> 1) + 1 : (tickCountFixed >> 1);
}

//----------------------------------------------------------------------------------------------------------------------
// Convert an uint32 speed defined in the PC 35Hz timebase to the 60Hz timebase used by used by this game version.
// Tries to round so the answer is as close as possible.
//----------------------------------------------------------------------------------------------------------------------
static inline constexpr uint32_t convertPcUintSpeed(const uint32_t speed35Hz) noexcept {
    // Get the tick count in 31.1 fixed point format by multiplying by 35/TICKSPERSEC (in 31.1 format).
    // When returning the integer answer round up if the fractional part is '.5':
    const uint32_t speedFixed = ((speed35Hz * uint32_t(35)) << 2) / (uint32_t(TICKSPERSEC) << 1);
    return (speedFixed & 1) ? (speedFixed >> 1) + 1 : (speedFixed >> 1);
}
