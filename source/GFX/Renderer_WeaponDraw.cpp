#include "Renderer_Internal.h"

#include "Base/Endian.h"
#include "Base/Tables.h"
#include "Burger.h"
#include "Game/Data.h"
#include "Game/DoomRez.h"
#include "Game/Resources.h"
#include "Things/Info.h"
#include "Things/MapObj.h"
#include "Things/Player.h"

BEGIN_NAMESPACE(Renderer)

#define SCREENGUNY -40      // Y offset to center the player's weapon properly

/**********************************

    Draw a single weapon or muzzle flash on the screen

**********************************/

static void DrawAWeapon(pspdef_t *psp,uint32_t Shadow)
{
    const state_t* const StatePtr = psp->StatePtr;                                      // Get the state struct pointer
    const uint32_t RezNum = StatePtr->SpriteFrame >> FF_SPRITESHIFT;                        // Get the file
    const uint16_t* Input = (uint16_t*)loadResourceData(RezNum);                        // Get the main pointer
    Input = (uint16_t*) GetShapeIndexPtr(Input,StatePtr->SpriteFrame & FF_FRAMEMASK);   // Pointer to the xy offset'd shape
    
    // FIXME: DC: Reimplement/replace
    #if 0
        ((LongWord *)Input)[7] = GunXScale;     // Set the scale factor
        ((LongWord *)Input)[10] = GunYScale;
        if (Shadow) {
            ((LongWord *)Input)[13] = 0x9C81;   // Set the shadow bits
        } else {
            ((LongWord *)Input)[13] = 0x1F00;   // Normal PMode
            #if 0
            if (StatePtr->SpriteFrame & FF_FULLBRIGHT) {
                Color = 255;            // Full bright
            } else {                    // Ambient light
                Color = players.mo->subsector->sector->lightlevel;
            }
            #endif
        }
    #endif

    // TODO: DC: Find a better place for these endian conversions
    int x = byteSwappedI16(Input[0]);
    int y = byteSwappedI16(Input[1]);
    x = ((psp->WeaponX + x ) * (int) gGunXScale) >> 20;
    y = ((psp->WeaponY + SCREENGUNY + y) * (int) gGunYScale) >> 16;
    x += gScreenXOffset;
    y += gScreenYOffset + 2;         // Add 2 pixels to cover up the hole in the bottom
    DrawMShape(x, y, (const CelControlBlock*) &Input[2]);    // Draw the weapon's shape
    releaseResource(RezNum);
}

/**********************************

    Draw the player's weapon in the foreground

**********************************/

void DrawWeapons(void)
{
    uint32_t i;
    uint32_t Shadow;        // Flag for shadowing
    pspdef_t *psp;
    
    psp = gPlayers.psprites; // Get the first sprite in the array 
    Shadow = false;         // Assume no shadow draw mode
    if (gPlayers.mo->flags & MF_SHADOW) {    // Could be active?
        i = gPlayers.powers[pw_invisibility];    // Get flash time
        if (i>=(5*TICKSPERSEC) || i&0x10) { // On a long time or if flashing...
            Shadow = true;      // Draw as a shadow right now
        }
    }
    i = 0;      // Init counter
    do {
        if (psp->StatePtr) {        // Valid state record?
            DrawAWeapon(psp,Shadow);    // Draw the weapon
        }
        ++psp;      // Next...
    } while (++i<NUMPSPRITES);  // All done?
    
    i = gScreenSize+rBACKGROUNDMASK;         // Get the resource needed

    DrawMShape(0,0, (const CelControlBlock*) loadResourceData(i));      // Draw the border
    releaseResource(i);                                                 // Release the resource
}

END_NAMESPACE(Renderer)
