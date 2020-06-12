#include "Switch.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Ceiling.h"
#include "Doors.h"
#include "Floor.h"
#include "Game/DoomRez.h"
#include "Game/Game.h"
#include "Game/Tick.h"
#include "MapData.h"
#include "Platforms.h"
#include "Things/MapObj.h"

static constexpr uint32_t BUTTONTIME = TICKSPERSEC;     // 1 second

// Positions for the button
enum bwhere_e {      
    top,        // Top button
    middle,     // Middle button
    bottom      // Bottom button
};

// Struct to describe a switch
struct button_t {
    line_t*     line;       // Line that the button is on
    uint32_t    btexture;   // Texture #
    uint32_t    btimer;     // Time before switching back
    bwhere_e    where;      // Vertical position of the button
};

// Before, After
const uint32_t gSwitchList[] = {
    rSW1BRN1 - rT_START,
    rSW2BRN1 - rT_START,
    rSW1GARG - rT_START,
    rSW2GARG - rT_START,
    rSW1GSTON - rT_START,
    rSW2GSTON - rT_START,
    rSW1HOT - rT_START,
    rSW2HOT - rT_START,
    rSW1STAR - rT_START,
    rSW2STAR - rT_START,
    rSW1WOOD - rT_START,
    rSW2WOOD - rT_START
};

uint32_t gNumSwitches;

//------------------------------------------------------------------------------------------------------------------------------------------
// Think logic for a button
//------------------------------------------------------------------------------------------------------------------------------------------
static void T_Button(button_t& button) noexcept {
    ASSERT(button.line);
    ASSERT(button.line->SidePtr[0]);

    // Do buttons
    if (button.btimer > 1) {    // Time up?
        --button.btimer;        // Adjust timer
    } else {
        line_t& line = *button.line;
        side_t& side = *line.SidePtr[0];    // Get the side record
        switch (button.where) {
            case top:
                side.toptexture = button.btexture;
                break;
            case middle:
                side.midtexture = button.btexture;
                break;
            case bottom:
                side.bottomtexture = button.btexture;
                break;
        }
        S_StartSound(&line.frontsector->SoundX, sfx_swtchn);
        RemoveThinker(button);  // Unlink and free
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Init the switch list so that textures can swap depending on the state of a switch
//------------------------------------------------------------------------------------------------------------------------------------------
void P_InitSwitchList() noexcept {
    gNumSwitches = C_ARRAY_SIZE(gSwitchList);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Start a button counting down till it turns off.
//------------------------------------------------------------------------------------------------------------------------------------------
static void P_StartButton(line_t& line, const bwhere_e w, const uint32_t texture, const uint32_t time) noexcept {
    button_t& button = AddThinker(T_Button);
    button.line = &line;
    button.where = w;
    button.btexture = texture;
    button.btimer = time;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Function that changes wall texture.
// Tell it if switch is ok to use again (yes, if it's a button).
//------------------------------------------------------------------------------------------------------------------------------------------
void P_ChangeSwitchTexture(line_t& line, const bool bUseAgain) noexcept {
    if (!bUseAgain) {
        line.special = 0;
    }

    ASSERT(line.SidePtr[0]);
    side_t& side = *line.SidePtr[0];
    Fixed* const pSoundOrg = &line.frontsector->SoundX;

    const uint32_t texTop = side.toptexture;
    const uint32_t texMid = side.midtexture;
    const uint32_t texBot = side.bottomtexture;
    uint32_t sound = sfx_swtchn;

    if (line.special == 11) {   // Exit switch?
        sound = sfx_swtchx;
    }

    for (uint32_t i = 0; i < gNumSwitches; ++i) {
        if (gSwitchList[i] == texTop) {
            S_StartSound(pSoundOrg, sound);
            side.toptexture = gSwitchList[i ^ 1];
            if (bUseAgain) {
                P_StartButton(line, top, texTop, BUTTONTIME);
            }
            break;
        } else if (gSwitchList[i] == texMid) {
            S_StartSound(pSoundOrg, sound);
            side.midtexture = gSwitchList[i ^ 1];
            if (bUseAgain) {
                P_StartButton(line, middle, texMid, BUTTONTIME);
            }
            break;
        } else if (gSwitchList[i] == texBot) {
            S_StartSound(pSoundOrg, sound);
            side.bottomtexture = gSwitchList[i ^ 1];
            if (bUseAgain) {
                P_StartButton(line, bottom, texBot, BUTTONTIME);
            }
            break;
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Called when a thing uses a special line Only the front sides of lines are usable
//------------------------------------------------------------------------------------------------------------------------------------------
bool P_UseSpecialLine(mobj_t& thing, line_t& line) noexcept {
    // Switches that other things can activate
    if (!thing.player) {
        // Monsters never open secret doors
        if ((line.flags & ML_SECRET) != 0) {
            return false;
        }

        // Monsters are only allowed to open ordinary doors
        if (line.special != 1)
            return false;
    }

    // Do something
    switch (line.special) {
        //--------------------------------------------------------------------------------------------------------------
        // Normal doors
        //--------------------------------------------------------------------------------------------------------------
        case 1:     // Vertical Door
        case 31:    // Manual door open
        case 26:    // Blue Card Door Raise
        case 32:    // Blue Card door open
        case 99:    // Blue Skull Door Open
        case 106:   // Blue Skull Door Raise
        case 27:    // Yellow Card Door Raise
        case 34:    // Yellow Card door open
        case 105:   // Yellow Skull Door Open
        case 108:   // Yellow Skull Door Raise
        case 28:    // Red Card Door Raise
        case 33:    // Red Card door open
        case 100:   // Red Skull Door Open
        case 107:   // Red Skull Door Raise
            EV_VerticalDoor(line, thing);
            break;

        //--------------------------------------------------------------------------------------------------------------
        // Buttons
        //--------------------------------------------------------------------------------------------------------------
        case 42:    // Close Door
            if (EV_DoDoor(line, close)) {
                P_ChangeSwitchTexture(line, true);
            }
            break;
        case 43:    // Lower Ceiling to Floor
            if (EV_DoCeiling(line, lowerToFloor)) {
                P_ChangeSwitchTexture(line, true);
            }
            break;
        case 45:    // Lower Floor to Surrounding floor height
            if (EV_DoFloor(line, lowerFloor)) {
                P_ChangeSwitchTexture(line, true);
            }
            break;
        case 60:    // Lower Floor to Lowest
            if (EV_DoFloor(line, lowerFloorToLowest)) {
                P_ChangeSwitchTexture(line, true);
            }
            break;
        case 61:    // Open Door
            if (EV_DoDoor(line, open)) {
                P_ChangeSwitchTexture(line, true);
            }
            break;
        case 62:    // PlatDownWaitUpStay
            if (EV_DoPlat(line, downWaitUpStay,1)) {
                P_ChangeSwitchTexture(line, true);
            }
            break;
        case 63:    // Raise Door
            if (EV_DoDoor(line, normaldoor)) {
                P_ChangeSwitchTexture(line, true);
            }
            break;
        case 64:    // Raise Floor to ceiling
            if (EV_DoFloor(line,raiseFloor)) {
                P_ChangeSwitchTexture(line, true);
            }
            break;
        case 66:    // Raise Floor 24 and change texture
            if (EV_DoPlat(line, raiseAndChange, 24)) {
                P_ChangeSwitchTexture(line, true);
            }
            break;
        case 67:    // Raise Floor 32 and change texture
            if (EV_DoPlat(line, raiseAndChange, 32)) {
                P_ChangeSwitchTexture(line, true);
            }
            break;
        case 65:    // Raise Floor Crush
            if (EV_DoFloor(line, raiseFloorCrush)) {
                P_ChangeSwitchTexture(line, true);
            }
            break;
        case 68:    // Raise Plat to next highest floor and change texture
            if (EV_DoPlat(line, raiseToNearestAndChange, 0)) {
                P_ChangeSwitchTexture(line, true);
            }
            break;
        case 69:    // Raise Floor to next highest floor
            if (EV_DoFloor(line, raiseFloorToNearest)) {
                P_ChangeSwitchTexture(line, true);
            }
            break;
        case 70:    // Turbo Lower Floor
            if (EV_DoFloor(line, turboLower)) {
                P_ChangeSwitchTexture(line, true);
            }
            break;

        //--------------------------------------------------------------------------------------------------------------
        // Switches (One shot buttons)
        //--------------------------------------------------------------------------------------------------------------
        case 7: // Build Stairs
            if (EV_BuildStairs(line)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 9: // Change Donut
            if (EV_DoDonut(line)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 11:    // Exit level
            G_ExitLevel();
            P_ChangeSwitchTexture(line, false);
            break;
        case 14:    // Raise Floor 32 and change texture
            if (EV_DoPlat(line, raiseAndChange, 32)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 15:    // Raise Floor 24 and change texture
            if (EV_DoPlat(line, raiseAndChange, 24)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 18:    // Raise Floor to next highest floor
            if (EV_DoFloor(line, raiseFloorToNearest)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 20:    // Raise Plat next highest floor and change texture
            if (EV_DoPlat(line,raiseToNearestAndChange,0)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 21:    // PlatDownWaitUpStay
            if (EV_DoPlat(line, downWaitUpStay, 0)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 23:    // Lower Floor to Lowest
            if (EV_DoFloor(line, lowerFloorToLowest)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 29:    // Raise Door
            if (EV_DoDoor(line, normaldoor)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 41:    // Lower Ceiling to Floor
            if (EV_DoCeiling(line,lowerToFloor)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 71:    // Turbo Lower Floor
            if (EV_DoFloor(line, turboLower)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 49:    // Lower Ceiling And Crush
            if (EV_DoCeiling(line, lowerAndCrush)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 50:    // Close Door
            if (EV_DoDoor(line, close)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 51:    // Secret EXIT
            G_SecretExitLevel();
            P_ChangeSwitchTexture(line, false);
            break;
        case 55:    // Raise Floor Crush
            if (EV_DoFloor(line, raiseFloorCrush))
                P_ChangeSwitchTexture(line, false);
            break;
        case 101:   // Raise Floor
            if (EV_DoFloor(line, raiseFloor)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 102:   // Lower Floor to Surrounding floor height
            if (EV_DoFloor(line, lowerFloor)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
        case 103:   // Open Door
            if (EV_DoDoor(line, open)) {
                P_ChangeSwitchTexture(line, false);
            }
            break;
    }
    return true;
}
