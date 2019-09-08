#include "Game.h"

#include "Base/Input.h"
#include "Base/Random.h"
#include "Data.h"
#include "DoomMain.h"
#include "GFX/Textures.h"
#include "Map/Setup.h"
#include "Things/Info.h"
#include "Things/MapObj.h"
#include "ThreeDO.h"
#include "Tick.h"
#include "UI/Finale_Main.h"
#include "UI/Intermission_Main.h"

//---------------------------------------------------------------------------------------------------------------------
// Prepare to load a game level
//---------------------------------------------------------------------------------------------------------------------
void G_DoLoadLevel() noexcept {
    if (gPlayer.playerstate == PST_DEAD) {
        gPlayer.playerstate = PST_REBORN;   // Force rebirth
    }

    SetupLevel(gGameMap);       // Load the level into memory
    gGameAction = ga_nothing;   // Game in progress
}

//---------------------------------------------------------------------------------------------------------------------
// Call when a player completes a level
//---------------------------------------------------------------------------------------------------------------------
void G_PlayerFinishLevel() noexcept {
    player_t& player = gPlayer;
    std::memset(player.powers, 0, sizeof(player.powers));       // Remove powers
    std::memset(player.cards, 0, sizeof(player.cards));         // Remove keycards and skulls
    
    if (player.mo) {
        player.mo->flags &= ~MF_SHADOW;     // Allow me to be visible
    }

    player.extralight = 0;          // cancel gun flashes
    player.fixedcolormap = 0;       // cancel ir gogles
    player.damagecount = 0;         // no palette changes
    player.bonuscount = 0;          // cancel backpack
}

//---------------------------------------------------------------------------------------------------------------------
// Called after a player dies, almost everything is cleared and initialized
//---------------------------------------------------------------------------------------------------------------------
void G_PlayerReborn() noexcept {
    player_t& player = gPlayer;
    std::memset(&player, 0, sizeof(player));    // Zap the player

    player.attackdown = 0;                                      // Don't do anything immediately
    player.playerstate = PST_LIVE;                              // I live again!
    player.health = MAXHEALTH;                                  // Restore health
    player.readyweapon = player.pendingweapon = wp_pistol;      // Reset weapon
    player.weaponowned[wp_fist] = true;                         // I have a fist
    player.weaponowned[wp_pistol] = true;                       // And a pistol
    player.ammo[am_clip] = 50;                                  // Award 50 bullets

    // Reset ammo counts (No backpack)
    for (uint32_t i = 0; i < NUMAMMO; ++i) {
        player.maxammo[i] = gMaxAmmo[i];
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Player is reborn after death
//---------------------------------------------------------------------------------------------------------------------
void G_DoReborn() noexcept {
    gGameAction = ga_died;  // Reload the level from scratch
}

//---------------------------------------------------------------------------------------------------------------------
// Set flag for normal level exit
//---------------------------------------------------------------------------------------------------------------------
void G_ExitLevel() noexcept {
    gGameAction = ga_completed;
}

//---------------------------------------------------------------------------------------------------------------------
// Set flag for secret level exit
//---------------------------------------------------------------------------------------------------------------------
void G_SecretExitLevel() noexcept {
    gGameAction = ga_secretexit;
}

//---------------------------------------------------------------------------------------------------------------------
// Init variables for a new game
//---------------------------------------------------------------------------------------------------------------------
void G_InitNew(const skill_e skill, const uint32_t map) noexcept {
    Random::init();     // Reset the random number generator

    gGameMap = map;
    gGameSkill = skill;
    gPlayer.playerstate = PST_REBORN;   // Force players to be initialized upon first level load
    gPlayer.mo = nullptr;               // For net consistency checks

    if (skill == sk_nightmare) {                // Hack for really BAD monsters
        gStates[S_SARG_ATK1].Time = 2 * 4;      // Speed up the demons
        gStates[S_SARG_ATK2].Time = 2 * 4;
        gStates[S_SARG_ATK3].Time = 2 * 4;
        gMObjInfo[MT_SERGEANT].Speed = 15;
        gMObjInfo[MT_SHADOWS].Speed = 15;
        gMObjInfo[MT_BRUISERSHOT].Speed = 40;   // Baron of hell
        gMObjInfo[MT_HEADSHOT].Speed = 40;      // Cacodemon
        gMObjInfo[MT_TROOPSHOT].Speed = 40;
    } else {
        gStates[S_SARG_ATK1].Time = 4 * 4;      // Set everyone back to normal
        gStates[S_SARG_ATK2].Time = 4 * 4;
        gStates[S_SARG_ATK3].Time = 4 * 4;
        gMObjInfo[MT_SERGEANT].Speed = 8;
        gMObjInfo[MT_SHADOWS].Speed = 8;
        gMObjInfo[MT_BRUISERSHOT].Speed = 30;
        gMObjInfo[MT_HEADSHOT].Speed = 20;
        gMObjInfo[MT_TROOPSHOT].Speed = 20;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// The game should already have been initialized or loaded
//----------------------------------------------------------------------------------------------------------------------
void G_RunGame() noexcept {
    while (!Input::isQuitRequested()) {
        // Run a level until death or completion
        MiniLoop(P_Start, P_Stop, P_Ticker, P_Drawer);

        // If quitting to the main menu was requested then don't go any further
        if (gGameAction == ga_quit)     
            return;
        
        // Take away cards and stuff
        G_PlayerFinishLevel();

        if ((gGameAction == ga_died) ||     // Died, so restart the level
            (gGameAction == ga_warped)      // Skip intermission
        ) {
            continue;
        }

        // Decide which level to go to next
        if (gGameAction == ga_secretexit) {
             gNextMap = 24;  // Go to the secret level
        } else {
            switch (gGameMap) {
                // Secret level?
                case 24:
                    gNextMap = 4;
                    break;

                // Final level! Note: don't add secret level to prefs.
                case 23:
                    gNextMap = 23;
                    break;

                default:
                    gNextMap = gGameMap + 1;
                    break;
            }

            if (gNextMap > gMaxLevel) {
                gMaxLevel = gNextMap;   // Save the prefs file
                WritePrefsFile();
            }
        }

        // Run a stats intermission
        MiniLoop(IN_Start, IN_Stop, IN_Ticker, IN_Drawer);

        // Run the finale if needed and exit
        if (gGameMap == 23) {
            MiniLoop(F_Start, F_Stop, F_Ticker, F_Drawer);
            return;
        }

        gGameMap = gNextMap;
    }
}
