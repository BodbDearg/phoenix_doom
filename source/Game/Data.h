#pragma once

#include "Things/Player.h"

//----------------------------------------------------------------------------------------------------------------------
// Machine independent data for DOOM!
//----------------------------------------------------------------------------------------------------------------------

// Current game state setting
enum gameaction_e : uint8_t {      
    ga_nothing,
    ga_died,
    ga_completed,
    ga_secretexit,
    ga_warped,
    ga_exitdemo
};

// Skill level settings
enum skill_e : uint8_t {
    sk_baby,
    sk_easy,
    sk_medium,
    sk_hard,
    sk_nightmare
};

extern ammotype_e       gWeaponAmmos[NUMWEAPONS];   // Ammo types for all weapons
extern uint32_t         gMaxAmmo[NUMAMMO];          // Max ammo for ammo types
extern uint32_t         gPadAttack;                 // Joypad bit for attack
extern uint32_t         gPadUse;                    // Joypad bit for use
extern uint32_t         gPadSpeed;                  // Joypad bit for high speed
extern uint32_t         gControlType;               // Determine settings for PadAttack,Use,Speed
extern uint32_t         gTotalGameTicks;            // Total number of ticks since game start
extern uint32_t         gElapsedTime;               // Ticks elapsed between frames
extern uint32_t         gMaxLevel;                  // Highest level selectable in menu (1-23)
extern uint32_t*        gDemoDataPtr;               // Running pointer to demo data
extern uint32_t*        gDemoBuffer;                // Pointer to demo data
extern uint32_t         gJoyPadButtons;             // Current joypad
extern uint32_t         gPrevJoyPadButtons;         // Previous joypad
extern uint32_t         gNewJoyPadButtons;          // New joypad button downs
extern skill_e          gStartSkill;                // Default skill level
extern uint32_t         gStartMap;                  // Default map start
extern const void*      gBigNumFont;                // Cached pointer to the big number font (rBIGNUMB)
extern uint32_t         gTotalKillsInLevel;         // Number of monsters killed
extern uint32_t         gItemsFoundInLevel;         // Number of items found
extern uint32_t         gSecretsFoundInLevel;       // Number of secrets discovered
extern uint32_t         gTxTextureLight;            // Light value to pass to hardware
extern Fixed            gLightSub;
extern Fixed            gLightCoef;
extern Fixed            gLightMin;
extern Fixed            gLightMax;
extern player_t         gPlayers;                   // Current player stats
extern gameaction_e     gGameAction;                // Current game state
extern skill_e          gGameSkill;                 // Current skill level
extern uint32_t         gGameMap;                   // Current game map #
extern uint32_t         gNextMap;                   // The map to go to after the stats
extern uint32_t         gScreenSize;                // Screen size to use
extern bool             gLowDetail;                 // Use low detail mode
extern bool             gDemoRecording;             // True if demo is being recorded
extern bool             gDemoPlayback;              // True if demo is being played
extern bool             gDoWipe;                    // True if I should do the DOOM wipe
extern uint32_t         gValidCount;                // Increment every time a check is made (used for sort of unique ids)
