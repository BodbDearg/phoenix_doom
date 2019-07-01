#pragma once

#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Fixed point defines: Doom uses 16.16 signed fixed point numbers throughout a lot of the game.
//----------------------------------------------------------------------------------------------------------------------

typedef int32_t Fixed;      // Typedef for a 16.16 fixed point number

static constexpr uint32_t   FRACBITS    = 16;                   // Number of fraction bits in Fixed
static constexpr Fixed      FRACUNIT    = 1 << FRACBITS;        // 1.0 in fixed point
static constexpr Fixed      FIXED_MIN   = INT32_MIN;            // Min and max value for a 16.16 fixed point number
static constexpr Fixed      FIXED_MAX   = INT32_MAX;

//----------------------------------------------------------------------------------------------------------------------
// Angle related defines: Doom uses binary angle measurement, where the entire range of a 32-bit unsigned integer is
// used to represent 0-360 degrees. This format also naturally wraps around.
//----------------------------------------------------------------------------------------------------------------------

typedef uint32_t angle_t;       // 32-bit BAM angle (uses the full 32-bits to represent 360 degrees)

static constexpr angle_t ANG45  = 0x20000000u;      // 45 degrees in angle_t
static constexpr angle_t ANG90  = 0x40000000u;      // 90 degrees in angle_t
static constexpr angle_t ANG180 = 0x80000000u;      // 180 degrees in angle_t
static constexpr angle_t ANG270 = 0xC0000000u;      // 270 degrees in angle_t

//----------------------------------------------------------------------------------------------------------------------
// Global defines
//----------------------------------------------------------------------------------------------------------------------

// View related
static constexpr uint32_t   MAXSCREENHEIGHT = 160;                  // Maximum height allowed
static constexpr uint32_t   MAXSCREENWIDTH  = 280;                  // Maximum width allowed
static constexpr Fixed      VIEWHEIGHT      = 41 * FRACUNIT;        // Height to render from

// Gameplay/simulation related constants
static constexpr uint32_t   TICKSPERSEC     = 60;                   // The game timebase (ticks per second)
static constexpr uint32_t   MAPBLOCKSHIFT   = FRACBITS + 7;         // Shift value to convert Fixed to 128 pixel blocks
static constexpr Fixed      ONFLOORZ        = FIXED_MIN;            // Attach object to floor with this z
static constexpr Fixed      ONCEILINGZ      = FIXED_MAX;            // Attach object to ceiling with this z
static constexpr Fixed      GRAVITY         = 4 * FRACUNIT;         // Rate of fall
static constexpr Fixed      MAXMOVE         = 16 * FRACUNIT;        // Maximum velocity
static constexpr Fixed      MAXRADIUS       = 32 * FRACUNIT;        // Largest radius of any critter
static constexpr Fixed      MELEERANGE      = 70 * FRACUNIT;        // Range of hand to hand combat
static constexpr Fixed      MISSILERANGE    = 32 * 64 * FRACUNIT;   // Range of guns targeting
static constexpr Fixed      FLOATSPEED      = 8 * FRACUNIT;         // Speed an object can float vertically
static constexpr Fixed      SKULLSPEED      = 40 * FRACUNIT;        // Speed of the skull to attack

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
