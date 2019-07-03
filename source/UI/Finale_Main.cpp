#include "Finale_Main.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Burger.h"
#include "Game/Data.h"
#include "Game/DoomDefines.h"
#include "Game/DoomRez.h"
#include "Intermission_Main.h"
#include "Things/Info.h"
#include "ThreeDO.h"

#define CASTCOUNT 8

typedef enum {
    fin_endtext,
    fin_charcast
} final_e;

static constexpr char* CAST_NAMES[] = {        // Names of all the critters 
    "Zombieman",
    "Shotgun Guy",
    "Imp",
    "Demon",
    "Lost Soul",
    "Cacodemon",
    "Baron of Hell",
    "Our Hero"
};

static constexpr mobjinfo_t* CAST_ORDER[] = {  // Pointer to the critter's base information 
    &gMObjInfo[MT_POSSESSED],
    &gMObjInfo[MT_SHOTGUY],
    &gMObjInfo[MT_TROOP],
    &gMObjInfo[MT_SERGEANT],
    &gMObjInfo[MT_SKULL],
    &gMObjInfo[MT_HEAD],
    &gMObjInfo[MT_BRUISER],
    &gMObjInfo[MT_PLAYER]
};

#define TEXTTIME (TICKSPERSEC/10)       // Tics to display letters 
#define STARTX 8                        // Starting x and y 
#define STARTY 8

static const mobjinfo_t*    gCastInfo;          // Info for the current cast member 
static uint32_t             gCastNum;           // Which cast member is being displayed 
static uint32_t             gCastTics;          // Speed to animate the cast members 
static const state_t*       gCastState;         // Pointer to current state 
static uint32_t             gCastFrames;        // Number of frames of animation played 
static final_e              gStatus;            // State of the display? 
static bool                 gCastAttacking;     // Currently attacking? 
static bool                 gCastDeath;         // Playing the death scene? 
static bool                 gCastOnMelee;       // Type of attack to play 
static uint32_t             gTextIndex;         // Index to the opening text 
static uint32_t             gTextDelay;         // Delay before next char 

static char gEndTextString[] =
    "     id software\n"
    "     salutes you!\n"
    "\n"
    "  the horrors of hell\n"
    "  could not kill you.\n"
    "  their most cunning\n"
    "  traps were no match\n"
    "  for you. you have\n"
    "  proven yourself the\n"
    "  best of all!\n"
    "\n"
    "  congratulations!";

/**********************************

    Print a string in a large font.
    NOTE : The strings must be lower case ONLY!

**********************************/

static void F_PrintString(uint32_t text_x, uint32_t text_y, char* string)
{
    uint32_t index;
    uint32_t val;

    index = 0;      // Index into the string 
    for (;;) {      // Stay forever 
        val = string[index];        // Get first char 
        if (!val || val=='\n') {    // End of line? 
            string[index] = 0;      // Mark end of line 
            PrintBigFont(text_x,text_y,string); // Draw the string without newline 
            if (!val) {         // Done now? 
                break;
            }
            string[index] = val;    // Restore the string 
            string+=index+1;        // Set the new start 
            index=-1;           // Reset the start index 
            text_y += 15;       // Next line down 
        }
        ++index;
    }
}

/**********************************

    Load all data for the finale

**********************************/

void F_Start(void)
{
    S_StartSong(Song_final);    // Play the end game music

    gStatus = fin_endtext;       // END TEXT PRINTS FIRST 
    gTextIndex = 0;              // At the beginning 
    gTextDelay = 0;              // Init the delay 
    gCastNum = 0;        // Start at the first monster 
    gCastInfo = CAST_ORDER[gCastNum];
    gCastState = gCastInfo->seestate;
    gCastTics = gCastState->Time;     // Init the time 
    gCastDeath = false;      // Not dead 
    gCastFrames = 0;         // No frames shown 
    gCastOnMelee = false;
    gCastAttacking = false;
}

/**********************************

    Release all memory for the finale

**********************************/

void F_Stop(void)
{
}

/**********************************

    Handle joypad info

**********************************/

uint32_t F_Ticker(void)
{
    uint32_t Temp;
// Check for press a key to kill actor 

    if (gStatus == fin_endtext) {        // Am I printing text? 
        if (gNewJoyPadButtons&(PadA|PadB|PadC) && (gTotalGameTicks >= (3*TICKSPERSEC))) {
            gStatus = fin_charcast;      // Continue to the second state 
            S_StartSound(0, gCastInfo->seesound); // Ohhh.. 
        }
        return ga_nothing;      // Don't exit 
    }

    if (!gCastDeath) {           // Not dead? 
        if (gNewJoyPadButtons&(PadA|PadB|PadC)) {   // go into death frame 
            Temp = gCastInfo->deathsound;            // Get the sound when the actor is killed 
            if (Temp) {
                S_StartSound(0,Temp);
            }
            gCastDeath = true;       // Death state 
            gCastState = gCastInfo->deathstate;
            gCastTics = gCastState->Time;
            gCastFrames = 0;
            gCastAttacking = false;
        }
    }

// Advance state 

    if (gCastTics>gElapsedTime) {
        gCastTics-=gElapsedTime;
        return ga_nothing;      // Not time to change state yet 
    }

    if (gCastState->Time == -1 || !gCastState->nextstate) {
        // switch from deathstate to next monster 
        ++gCastNum;
        if (gCastNum>=CASTCOUNT) {
            gCastNum = 0;
        }
        gCastDeath = false;
        gCastInfo = CAST_ORDER[gCastNum];
        Temp = gCastInfo->seesound;
        if (Temp) {
            S_StartSound (0,Temp);
        }
        gCastState = gCastInfo->seestate;
        gCastFrames = 0;
    } else {    // just advance to next state in animation 
        if (gCastState == &gStates[S_PLAY_ATK1]) {
            goto stopattack;    // Oh, gross hack!
        }
        gCastState = gCastState->nextstate;
        ++gCastFrames;

// sound hacks.... 

        {
            uint32_t st;
            st = gCastState - gStates;

            switch (st) {
            case S_POSS_ATK2:
                Temp = sfx_pistol;
                break;
            case S_SPOS_ATK2:
                Temp = sfx_shotgn;
                break;
            case S_TROO_ATK3:
                Temp = sfx_claw;
                break;
            case S_SARG_ATK2:
                Temp = sfx_sgtatk;
                break;
            case S_BOSS_ATK2:
            case S_HEAD_ATK2:
                Temp = sfx_firsht;
                break;
            case S_SKULL_ATK2:
                Temp = sfx_sklatk;
                break;
            default:
                Temp = 0;
            }
            if (Temp) {
                S_StartSound(0,Temp);
            }
        }
    }

    if (gCastFrames == 12) {     // go into attack frame 
        gCastAttacking = true;
        if (gCastOnMelee) {
            gCastState=gCastInfo->meleestate;
        } else {
            gCastState=gCastInfo->missilestate;
        }
        gCastOnMelee ^= true;        // Toggle the melee state 
        if (!gCastState) {
            if (gCastOnMelee) {
                gCastState=gCastInfo->meleestate;
            } else {
                gCastState=gCastInfo->missilestate;
            }
        }
    }

    if (gCastAttacking) {
        if (gCastFrames == 24
            || gCastState == gCastInfo->seestate) {
stopattack:
            gCastAttacking = false;
            gCastFrames = 0;
            gCastState = gCastInfo->seestate;
        }
    }

    gCastTics = gCastState->Time;     // Get the next time 
    if (gCastTics == -1) {
        gCastTics = (TICKSPERSEC/4);     // 1 second timer 
    }
    return ga_nothing;      // finale never exits 
}

/**********************************

    Draw the frame for the finale

**********************************/

void F_Drawer(void)
{
    DrawRezShape(0,0,rBACKGRNDBROWN);       // Draw the background 
    
    if (gStatus==fin_endtext) {
        uint32_t Temp;
        Temp = gEndTextString[gTextIndex];        // Get the final char 
        gEndTextString[gTextIndex] = 0;           // End the string here 
        F_PrintString(STARTX,STARTY,gEndTextString); // Print the string 
        gEndTextString[gTextIndex] = Temp;    // Restore the string 
        gTextDelay+=gElapsedTime;     // How much time has gone by? 
        if (gTextDelay>=TEXTTIME) {
            gTextDelay -= TEXTTIME;      // Adjust the time 
            if (Temp) {     // Already at the end? 
                ++gTextIndex;
            }
        } 
    } else {
        PrintBigFontCenter(160,20, CAST_NAMES[gCastNum]);  // Print the name 
        DrawSpriteCenter(gCastState->SpriteFrame);       // Draw the sprite 
    }
    UpdateAndPageFlip(true);        // Show the frame 
}
