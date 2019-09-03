#pragma once

#include "Base/Angle.h"
#include "Base/Fixed.h"
#include "PlayerSprites.h"

struct sector_t;

// Player constants
static constexpr Fixed      PLAYERRADIUS    = 16 * FRACUNIT;    // Radius of the player
static constexpr Fixed      USERANGE        = 70 * FRACUNIT;    // Range of pressing spacebar to activate something
static constexpr uint32_t   MAXHEALTH       = 100;              // Normal health at start of game

// Player automap flags
static constexpr uint32_t AF_ACTIVE         = 0x01;     // Automap active
static constexpr uint32_t AF_FOLLOW         = 0x02;     // Follow mode on
static constexpr uint32_t AF_ALLLINES       = 0x04;     // All lines cheat
static constexpr uint32_t AF_ALLMOBJ        = 0x08;     // All objects cheat
static constexpr uint32_t AF_NOCLIP         = 0x10;     // Can walk through walls
static constexpr uint32_t AF_GODMODE        = 0x20;     // No one can hurt me!
static constexpr uint32_t AF_OPTIONSACTIVE  = 0x80;     // Options screen running
static constexpr uint32_t AF_FREE_CAM       = 0x100;    // Follow mode deactivated?

// Current state of the player
enum playerstate_e {
    PST_LIVE,       // Playing
    PST_DEAD,       // Dead on the ground
    PST_REBORN      // Ready to restart
};

// Current ammo type being used
enum ammotype_e : uint8_t {
    am_clip,        // Pistol / chaingun
    am_shell,       // shotgun
    am_cell,        // BFG
    am_misl,        // Missile launcher
    NUMAMMO,        // Number of valid ammo types for array sizing
    am_noammo       // Chainsaw / fist
};

// Power index flags
enum powertype_e {
    pw_invulnerability,     // God mode
    pw_strength,            // Strength
    pw_ironfeet,            // Radiation suit
    pw_allmap,              // Mapping
    pw_invisibility,        // Can't see me!
    NUMPOWERS               // Number of powerups
};

// Item types for keycards or skulls
enum card_e {
    it_bluecard,
    it_yellowcard,
    it_redcard,
    it_blueskull,
    it_yellowskull,
    it_redskull,
    NUMCARDS            // Number of keys for array sizing
};

// Currently selected weapon
enum weapontype_e : uint8_t {
    wp_fist,
    wp_pistol,
    wp_shotgun,
    wp_chaingun,
    wp_missile,
    wp_plasma,
    wp_bfg,
    wp_chainsaw,
    NUMWEAPONS,     // Number of valid weapons for array sizing
    wp_nochange     // Inbetween weapons (Can't fire)
};

// Player's current game state
struct player_t {
    NON_ASSIGNABLE_STRUCT(player_t)

    mobj_t*         mo;                         // Pointer to sprite object
    uint32_t        health;                     // only used between levels, mo->health is used during levels
    uint32_t        armorpoints;                // Amount of armor
    uint32_t        armortype;                  // armor type is 0-2
    uint32_t        ammo[NUMAMMO];              // Current ammo
    uint32_t        maxammo[NUMAMMO];           // Maximum ammo
    uint32_t        killcount;                  // Number of critters killed
    uint32_t        itemcount;                  // Number of items gathered
    uint32_t        secretcount;                // Number of secret sectors touched
    const char*     message;                    // Hint messages
    mobj_t*         attacker;                   // Who did damage (NULL for floors)
    uint32_t        extralight;                 // so gun flashes light up areas
    uint32_t        fixedcolormap;              // can be set to REDCOLORMAP, etc
    uint32_t        colormap;                   // 0-3 for which color to draw player
    pspdef_t        psprites[NUMPSPRITES];      // view sprites (gun, etc)
    sector_t*       lastsoundsector;            // Pointer to sector where I last made a sound
    Fixed           forwardmove;                // Motion ahead (- for reverse)
    Fixed           sidemove;                   // Motion to the side (- for left)
    Fixed           viewz;                      // focal origin above r.z
    Fixed           viewheight;                 // base height above floor for viewz
    Fixed           deltaviewheight;            // squat speed
    Fixed           bob;                        // bounded/scaled total momentum
    Fixed           automapx;                   // X coord on the auto map
    Fixed           automapy;                   // Y coord on the auto map
    angle_t         angleturn;                  // Speed of turning
    uint32_t        turnheld;                   // For accelerative turning
    uint32_t        damagecount;                // Redness factor
    uint32_t        bonuscount;                 // Goldness factor
    uint32_t        powers[NUMPOWERS];          // invinc and invis are tic counters
    uint32_t        AutomapFlags;               // Bit flags for options and automap
    weapontype_e    readyweapon;                // Weapon being used
    weapontype_e    pendingweapon;              // wp_nochange if not changing
    playerstate_e   playerstate;                // Alive/dead...
    bool            cards[NUMCARDS];            // Keycards held
    bool            backpack;                   // Got the backpack?
    uint32_t        attackdown;                 // Held the attack key if > 0
    bool            weaponowned[NUMWEAPONS];    // Do I own these weapons?
    bool            refire;                     // refired shots are less accurate

    inline bool isAutomapActive() const noexcept {
        return ((AutomapFlags & AF_ACTIVE) != 0);
    }

    inline bool isAutomapFollowModeActive() const noexcept {
        return ((AutomapFlags & AF_FREE_CAM) == 0);
    }

    inline bool isOptionsMenuActive() const noexcept {
        return ((AutomapFlags & AF_OPTIONSACTIVE) != 0);
    }
};
