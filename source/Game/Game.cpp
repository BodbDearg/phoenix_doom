#include "Game.h"

#include "Base/Input.h"
#include "Base/Mem.h"
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
#include <cstring>

static void loadSkyTexture() {
    const uint32_t skyTexNum = Textures::getCurrentSkyTexNum();
    Textures::loadWall(skyTexNum);
}

//---------------------------------------------------------------------------------------------------------------------
// Prepare to load a game level
//---------------------------------------------------------------------------------------------------------------------
void G_DoLoadLevel() {
    if (gPlayer.playerstate == PST_DEAD) {
        gPlayer.playerstate = PST_REBORN;   // Force rebirth
    }

    loadSkyTexture();
    SetupLevel(gGameMap);       // Load the level into memory
    gGameAction = ga_nothing;   // Game in progress
}

/**********************************

    Call when a player completes a level

**********************************/

void G_PlayerFinishLevel()
{
    player_t *p;        // Local pointer

    p = &gPlayer;
    memset(p->powers,0,sizeof(p->powers));  // Remove powers
    memset(p->cards,0,sizeof(p->cards));    // Remove keycards and skulls
    if (p->mo) {
        p->mo->flags &= ~MF_SHADOW;             // Allow me to be visible
    }
    p->extralight = 0;                      // cancel gun flashes
    p->fixedcolormap = 0;                   // cancel ir gogles
    p->damagecount = 0;                     // no palette changes
    p->bonuscount = 0;                      // cancel backpack
}

/**********************************

    Called after a player dies
    almost everything is cleared and initialized

**********************************/

void G_PlayerReborn()
{
    player_t*p = &gPlayer;      // Get local pointer
    memset(p,0,sizeof(*p));     // Zap the player

    p->usedown = p->attackdown = 0;                     // don't do anything immediately
    p->playerstate = PST_LIVE;                          // I live again!
    p->health = MAXHEALTH;                              // Restore health
    p->readyweapon = p->pendingweapon = wp_pistol;      // Reset weapon
    p->weaponowned[wp_fist] = true;                     // I have a fist
    p->weaponowned[wp_pistol] = true;                   // And a pistol
    p->ammo[am_clip] = 50;                              // Award 50 bullets
    uint32_t i = 0;
    do {
        p->maxammo[i] = gMaxAmmo[i]; // Reset ammo counts (No backpack)
    } while (++i<NUMAMMO);
}


/**********************************

    Player is reborn after death

**********************************/

void G_DoReborn()
{
    gGameAction = ga_died;   // Reload the level from scratch
}

/**********************************

    Set flag for normal level exit

**********************************/

void G_ExitLevel()
{
    gGameAction = ga_completed;
}

/**********************************

    Set flag for secret level exit

**********************************/

void G_SecretExitLevel()
{
    gGameAction = ga_secretexit;
}

/**********************************

    Init variables for a new game

**********************************/

void G_InitNew(skill_e skill, uint32_t map)
{
    Random::init();        // Reset the random number generator

    gGameMap = map;
    gGameSkill = skill;

// Force players to be initialized upon first level load

    gPlayer.playerstate = PST_REBORN;
    gPlayer.mo = 0; // For net consistancy checks

    gDemoRecording = false;      // No demo in progress
    gDemoPlayback = false;

    if (skill == sk_nightmare ) {       // Hack for really BAD monsters
        gStates[S_SARG_ATK1].Time = 2*4; // Speed up the demons
        gStates[S_SARG_ATK2].Time = 2*4;
        gStates[S_SARG_ATK3].Time = 2*4;
        gMObjInfo[MT_SERGEANT].Speed = 15;
        gMObjInfo[MT_SHADOWS].Speed = 15;
        gMObjInfo[MT_BRUISERSHOT].Speed = 40;    // Baron of hell
        gMObjInfo[MT_HEADSHOT].Speed = 40;       // Cacodemon
        gMObjInfo[MT_TROOPSHOT].Speed = 40;
    } else {
        gStates[S_SARG_ATK1].Time = 4*4;     // Set everyone back to normal
        gStates[S_SARG_ATK2].Time = 4*4;
        gStates[S_SARG_ATK3].Time = 4*4;
        gMObjInfo[MT_SERGEANT].Speed = 8;
        gMObjInfo[MT_SHADOWS].Speed = 8;
        gMObjInfo[MT_BRUISERSHOT].Speed = 30;
        gMObjInfo[MT_HEADSHOT].Speed = 20;
        gMObjInfo[MT_TROOPSHOT].Speed = 20;
    }
}

/**********************************

    The game should already have been initialized or loaded

**********************************/

void G_RunGame() noexcept {
    while (!Input::quitRequested()) {
        // Run a level until death or completion
        MiniLoop(P_Start, P_Stop, P_Ticker, P_Drawer);

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

/**********************************

    Play a demo using a pointer to the demo data

**********************************/

uint32_t G_PlayDemoPtr(uint32_t* demo)
{
    uint32_t exit;
    uint32_t skill,map;

    gDemoBuffer = demo;      // Save the demo buffer pointer
    skill = demo[0];        // Get the initial and map
    map = demo[1];
    gDemoDataPtr = &demo[2];     // Init the pointer
    G_InitNew((skill_e)skill,map);  // Init a game
    gDemoPlayback = true;    // I am playing back data
    exit = MiniLoop(P_Start, P_Stop, P_Ticker, P_Drawer);  // Execute game
    gDemoPlayback = false;   // End demo
    return exit;
}

/**********************************

    Record a demo
    Only used in testing.

**********************************/

void G_RecordDemo (void)
{
    uint32_t *Dest;

    Dest = (uint32_t*)MemAlloc(0x8000);       // Get memory for demo
    gDemoBuffer = Dest;         // Save the pointer
    Dest[0] = gStartSkill;      // Save the skill and level
    Dest[1] = gStartMap;
    gDemoDataPtr = Dest+2;
    G_InitNew(gStartSkill, gStartMap); // Begin a game
    gDemoRecording = true;       // Begin recording
    MiniLoop(P_Start,P_Stop,P_Ticker,P_Drawer); // Play it
    gDemoRecording = false;      // End recording
    for (;;) {                  // Stay forever
        G_PlayDemoPtr(gDemoBuffer);  // Play the demo
    }
}
