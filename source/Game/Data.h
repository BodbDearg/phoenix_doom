#pragma once

#include "Things/Player.h"

//----------------------------------------------------------------------------------------------------------------------
// Machine independent data for DOOM!
//----------------------------------------------------------------------------------------------------------------------

struct CelImageArray;

// Current game state setting
enum gameaction_e : uint8_t {
    ga_nothing,
    ga_died,
    ga_completed,
    ga_secretexit,
    ga_warped,
    ga_exitdemo,
    ga_quit
};

// Skill level settings
enum skill_e : uint8_t {
    sk_baby,
    sk_easy,
    sk_medium,
    sk_hard,
    sk_nightmare
};

extern ammotype_e               gWeaponAmmos[NUMWEAPONS];       // Ammo types for all weapons
extern uint32_t                 gMaxAmmo[NUMAMMO];              // Max ammo for ammo types
extern uint32_t                 gTotalGameTicks;                // Total number of ticks since game start
extern uint32_t                 gMaxLevel;                      // Highest level selectable in menu (1-23)
extern skill_e                  gStartSkill;                    // Default skill level
extern uint32_t                 gStartMap;                      // Default map start
extern const CelImageArray*     gpBigNumFont;                   // Cached pointer to the big number font (rBIGNUMB)
extern uint32_t                 gTotalKillsInLevel;             // Number of monsters killed
extern uint32_t                 gItemsFoundInLevel;             // Number of items found
extern uint32_t                 gSecretsFoundInLevel;           // Number of secrets discovered
extern player_t                 gPlayer;                        // The game player
extern gameaction_e             gGameAction;                    // Current game state
extern skill_e                  gGameSkill;                     // Current skill level
extern uint32_t                 gGameMap;                       // Current game map #
extern uint32_t                 gNextMap;                       // The map to go to after the stats
extern uint32_t                 gScreenSize;                    // Screen size to use
extern bool                     gDoWipe;                        // True if I should do the DOOM wipe
extern uint32_t                 gValidCount;                    // Increment every time a check is made (used for sort of unique ids)
