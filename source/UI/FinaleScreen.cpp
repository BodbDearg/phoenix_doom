#include "FinaleScreen.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Tables.h"
#include "Game/Controls.h"
#include "Game/Data.h"
#include "Game/DoomDefines.h"
#include "Game/DoomRez.h"
#include "GFX/Blit.h"
#include "GFX/Sprites.h"
#include "GFX/Video.h"
#include "Things/Info.h"
#include "UIUtils.h"
#include <cstring>

enum final_e {
    fin_endtext,
    fin_charcast
};

// Names of all the critters
static constexpr const char* const CAST_NAMES[] = {
    "Zombieman",
    "Shotgun Guy",
    "Imp",
    "Demon",
    "Lost Soul",
    "Cacodemon",
    "Baron of Hell",
    "Our Hero"
};

// Pointer to the critter's base information
static constexpr mobjinfo_t* const CAST_ORDER[] = {
    &gMObjInfo[MT_POSSESSED],
    &gMObjInfo[MT_SHOTGUY],
    &gMObjInfo[MT_TROOP],
    &gMObjInfo[MT_SERGEANT],
    &gMObjInfo[MT_SKULL],
    &gMObjInfo[MT_HEAD],
    &gMObjInfo[MT_BRUISER],
    &gMObjInfo[MT_PLAYER]
};

static constexpr uint32_t CAST_COUNT    = C_ARRAY_SIZE(CAST_NAMES);
static constexpr uint32_t TEXTTIME      = (TICKSPERSEC / 10);           // Tics to display letters
static constexpr uint32_t STARTX        = 8;                            // Starting x and y
static constexpr uint32_t STARTY        = 8;

static const mobjinfo_t*    gCastInfo;          // Info for the current cast member
static uint32_t             gCastNum;           // Which cast member is being displayed
static uint32_t             gCastTics;          // Speed to animate the cast members
static const state_t*       gpCastState;        // Pointer to current state
static uint32_t             gCastFrames;        // Number of frames of animation played
static final_e              gStatus;            // State of the display?
static bool                 gCastAttacking;     // Currently attacking?
static bool                 gCastDeath;         // Playing the death scene?
static bool                 gCastOnMelee;       // Type of attack to play
static uint32_t             gTextIndex;         // Index to the opening text
static uint32_t             gTextDelay;         // Delay before next char

static constexpr const char* const gEndTextString =
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

//------------------------------------------------------------------------------------------------------------------------------------------
// Draw the current frame for the specified actor centered in the middle of the screen for the finale
//------------------------------------------------------------------------------------------------------------------------------------------
static void DrawActorCentered(const state_t& actorState) noexcept {
    // Determine the sprite frame to be drawn
    uint32_t spriteResourceNum;
    uint32_t spriteFrameNum;
    bool bIsSpriteFullBright;
    state_t::decomposeSpriteFrameFieldComponents(
        actorState.SpriteFrame,
        spriteResourceNum,
        spriteFrameNum,
        bIsSpriteFullBright
    );

    // Grab the sprite frame to be drawn
    const Sprite* const pSprite = Sprites::load(spriteResourceNum);
    ASSERT(spriteFrameNum < pSprite->numFrames);
    const SpriteFrame& spriteFrame = pSprite->pFrames[spriteFrameNum];
    const SpriteFrameAngle& spriteAngle = spriteFrame.angles[0];

    // Figure out the position and size of the sprite to center it on the screen at this scale
    constexpr float SPRITE_SCALE = 2.0f;
    constexpr float SPRITE_Y_OFFSET = 80.0f;

    const float screenCenterX = Video::REFERENCE_SCREEN_WIDTH * 0.5f * gScaleFactor;
    const float screenCenterY = Video::REFERENCE_SCREEN_HEIGHT * 0.5f * gScaleFactor;
    const float w = spriteAngle.width * gScaleFactor * SPRITE_SCALE;
    const float h = spriteAngle.height * gScaleFactor * SPRITE_SCALE;
    const float lx = screenCenterX - (float) spriteAngle.leftOffset * SPRITE_SCALE * gScaleFactor;
    const float ty = screenCenterY - (float) spriteAngle.topOffset * SPRITE_SCALE * gScaleFactor + SPRITE_Y_OFFSET * gScaleFactor;

    const int32_t lxInt = (int32_t) lx;
    const int32_t tyInt = (int32_t) ty;
    const int32_t rxInt = (int32_t)(lx + w + 1);    // Do 1 extra column to ensure we capture sprite borders!
    const int32_t byInt = (int32_t)(ty + h + 1);    // Do 1 extra row to ensure we capture sprite borders!
    const int32_t wInt = (rxInt - lxInt) + 1;
    const int32_t hInt = (byInt - tyInt) + 1;

    // Figure out x and y texcoord stepping
    const float texXStep = (wInt > 1) ? (float) spriteAngle.width / (float)(wInt - 1) : 0.0f;
    const float texYStep = (hInt > 1) ? (float) spriteAngle.height / (float)(hInt - 1) : 0.0f;

    for (int32_t xInt = lxInt; xInt <= rxInt; ++xInt) {
        Blit::blitColumn<
            Blit::BCF_STEP_Y |
            Blit::BCF_ALPHA_TEST |
            Blit::BCF_H_WRAP_DISCARD |
            Blit::BCF_V_WRAP_DISCARD |
            Blit::BCF_H_CLIP |
            Blit::BCF_V_CLIP
        >
        (
            spriteAngle.pTexture,
            spriteAngle.width,
            spriteAngle.height,
            (float)(xInt - lx) * texXStep,
            0.0f,
            0.0f,
            0.0f,
            Video::gpFrameBuffer,
            Video::gScreenWidth,
            Video::gScreenHeight,
            Video::gScreenWidth,
            xInt,
            tyInt,
            (uint32_t) hInt,
            0.0f,
            texYStep
        );
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Print a portion of a string in a large font.
// NOTE : The strings must be lower case ONLY!
//------------------------------------------------------------------------------------------------------------------------------------------
static void F_PrintString(
    const int32_t textX,
    const int32_t textY,
    const char* const str,
    const char* const strEnd
) noexcept {
    int32_t curTextY = textY;
    const char* curSubStr = str;
    const char* curSubStrEnd = str;

    auto outputSubStr = [](
        const char* const beg,
        const char* const end,
        const int32_t x,
        const int32_t y
    ) noexcept {
        const uint32_t numChars = (uint32_t)(end - beg);
        char subStrChars[64];

        if (numChars > 0) {
            ASSERT(numChars < C_ARRAY_SIZE(subStrChars));
            std::strncpy(subStrChars, beg, numChars);
            subStrChars[numChars] = 0;
            UIUtils::printBigFont(x, y, subStrChars);
        }
    };

    while (curSubStrEnd < strEnd) {
        // Have we reached the end of the line? If so output and move down:
        if (curSubStrEnd[0] == '\n') {
            outputSubStr(curSubStr, curSubStrEnd, textX, curTextY);
            curTextY += 15;
            curSubStr = curSubStrEnd + 1;
            curSubStrEnd = curSubStr;
        } else {
            ++curSubStrEnd;
        }
    }

    outputSubStr(curSubStr, curSubStrEnd, textX, curTextY);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Load all data for the finale
//------------------------------------------------------------------------------------------------------------------------------------------
void F_Start() noexcept {
    S_StartSong(Song_final);                // Play the end game music

    gStatus = fin_endtext;                  // END TEXT PRINTS FIRST
    gTextIndex = 0;                         // At the beginning
    gTextDelay = 0;                         // Init the delay
    gCastNum = 0;                           // Start at the first monster
    gCastInfo = CAST_ORDER[gCastNum];
    gpCastState = gCastInfo->seestate;
    gCastTics = gpCastState->Time;          // Init the time
    gCastDeath = false;                     // Not dead
    gCastFrames = 0;                        // No frames shown
    gCastOnMelee = false;
    gCastAttacking = false;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Release all memory for the finale
//------------------------------------------------------------------------------------------------------------------------------------------
void F_Stop() noexcept {
    // Nothing to do...
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Handle joypad input etc.
//------------------------------------------------------------------------------------------------------------------------------------------
gameaction_e F_Ticker() noexcept {
    ASSERT(gpCastState);

    // Check for press a key to kill actor
    if (gStatus == fin_endtext) {   // Am I printing text?
        if (MENU_ACTION(OK) && (gTotalGameTicks >= 3 * TICKSPERSEC)) {
            gStatus = fin_charcast;                         // Continue to the second state
            S_StartSound(nullptr, gCastInfo->seesound);     // Ohhh..
        }
        return ga_nothing;  // Don't exit
    }

    // If not dead then maybe go into death frame if the right inputs are pressed
    if (!gCastDeath) {
        if (MENU_ACTION(OK)) {
            // Enter death state
            S_StartSound(nullptr, gCastInfo->deathsound);
            gCastDeath = true;
            gpCastState = gCastInfo->deathstate;
            gCastTics = gpCastState->Time;
            gCastFrames = 0;
            gCastAttacking = false;
        }
    }

    // Allow exit back to the main menu if the 'abort' key is pressed and if we're not showing the end text
    if (gStatus != fin_endtext) {
        if (MENU_ACTION_ENDED(BACK)) {
            return ga_quit;
        }
    }

    // Advance state: if there is no transition to do then we can end here
    if (gCastTics > 1) {
        --gCastTics;
        return ga_nothing;  // Not time to change state yet
    }

    if (gpCastState->Time == UINT32_MAX || (!gpCastState->nextstate)) {
        // Switch from deathstate to next monster
        ++gCastNum;
        if (gCastNum >= CAST_COUNT) {
            gCastNum = 0;
        }
        gCastDeath = false;
        gCastInfo = CAST_ORDER[gCastNum];
        S_StartSound(nullptr, gCastInfo->seesound);
        gpCastState = gCastInfo->seestate;
        gCastFrames = 0;
    } else {
        // Just advance to next state in animation
        if (gpCastState == &gStates[S_PLAY_ATK1]) {
            goto stopattack;    // Oh, gross hack!
        }

        gpCastState = gpCastState->nextstate;
        ++gCastFrames;

        // Sound hacks....
        {
            uint32_t soundToPlay = 0;
            const uint32_t stateNum = (uint32_t)(gpCastState - gStates);

            switch (stateNum) {
                case S_POSS_ATK2:   soundToPlay = sfx_pistol;   break;
                case S_SPOS_ATK2:   soundToPlay = sfx_shotgn;   break;
                case S_TROO_ATK3:   soundToPlay = sfx_claw;     break;
                case S_SARG_ATK2:   soundToPlay = sfx_sgtatk;   break;
                case S_BOSS_ATK2:   soundToPlay = sfx_firsht;   break;
                case S_HEAD_ATK2:   soundToPlay = sfx_firsht;   break;
                case S_SKULL_ATK2:  soundToPlay = sfx_sklatk;   break;
                default:            soundToPlay = 0;            break;
            }

            S_StartSound(nullptr, soundToPlay);
        }
    }

    if (gCastFrames == 12) {
        // Go into attack frame
        gCastAttacking = true;
        if (gCastOnMelee) {
            gpCastState = gCastInfo->meleestate;
        } else {
            gpCastState = gCastInfo->missilestate;
        }
        gCastOnMelee ^= true;   // Toggle the melee state
        if (!gpCastState) {
            if (gCastOnMelee) {
                gpCastState = gCastInfo->meleestate;
            } else {
                gpCastState = gCastInfo->missilestate;
            }
        }
    }

    if (gCastAttacking) {
        if (gCastFrames == 24 || gpCastState == gCastInfo->seestate) {
        stopattack:
            gCastAttacking = false;
            gCastFrames = 0;
            gpCastState = gCastInfo->seestate;
        }
    }

    ASSERT(gpCastState);
    gCastTics = gpCastState->Time;          // Get the next time

    if (gCastTics == UINT32_MAX) {
        gCastTics = (TICKSPERSEC / 4);      // 1 second timer
    }

    return ga_nothing;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Draw the frame for the finale
//------------------------------------------------------------------------------------------------------------------------------------------
void F_Drawer(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    Video::debugClearScreen();
    UIUtils::drawUISprite(0, 0, rBACKGRNDBROWN);    // Draw the background

    if (gStatus == fin_endtext) {
        // Print the string portion
        F_PrintString(
            STARTX,
            STARTY,
            gEndTextString,
            gEndTextString + gTextIndex
        );

        // Advance to the next character if that is possible
        ++gTextDelay;

        if (gTextDelay >= TEXTTIME) {
            gTextDelay -= TEXTTIME;                     // Adjust the time
            if (gEndTextString[gTextIndex] != 0) {      // Already at the end?
                ++gTextIndex;
            }
        }
    } else {
        DrawActorCentered(*gpCastState);                                // Draw the sprite
        UIUtils::printBigFontCenter(160, 20, CAST_NAMES[gCastNum]);     // Print the name
    }

    UIUtils::drawPerformanceCounter(0, 0);
    Video::endFrame(bPresent, bSaveFrameBuffer);
}
