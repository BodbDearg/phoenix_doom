#include "Renderer_Internal.h"

#include "Base/Endian.h"
#include "Base/Tables.h"
#include "Burger.h"
#include "Game/Data.h"
#include "Game/DoomRez.h"
#include "Game/Resources.h"
#include "Things/Info.h"
#include "Things/MapObj.h"

BEGIN_NAMESPACE(Renderer)

static constexpr int32_t SCREENGUNY = -40;  // Y offset to center the player's weapon properly

//----------------------------------------------------------------------------------------------------------------------
// Draw a single weapon or muzzle flash on the screen
//----------------------------------------------------------------------------------------------------------------------
static void DrawAWeapon(const pspdef_t& psp, const bool bShadow) noexcept {
    // FIXME: DC: clean this up
    const state_t* const StatePtr = psp.StatePtr;                                               // Get the state struct pointer
    const uint32_t rezNum = StatePtr->SpriteFrame >> FF_SPRITESHIFT;                            // Get the file
    const uint16_t* input = (uint16_t*) loadResourceData(rezNum);                               // Get the main pointer
    input = (const uint16_t*) GetShapeIndexPtr(input, StatePtr->SpriteFrame & FF_FRAMEMASK);    // Pointer to the xy offset'd shape
    
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
    int32_t x = byteSwappedI16(input[0]);
    int32_t y = byteSwappedI16(input[1]);
    x = ((psp.WeaponX + x ) * (int32_t) gGunXScale) >> 20;
    y = ((psp.WeaponY + SCREENGUNY + y) * (int32_t) gGunYScale) >> 16;
    x += gScreenXOffset;
    y += gScreenYOffset + 2;                                    // Add 2 pixels to cover up the hole in the bottom
    DrawMShape(x, y, (const CelControlBlock*) &input[2]);       // Draw the weapon's shape
    releaseResource(rezNum);
}

//----------------------------------------------------------------------------------------------------------------------
// Draw the player's weapon in the foreground
//----------------------------------------------------------------------------------------------------------------------
void drawWeapons() noexcept {
    // Determine whether to draw the weapon partially invisible
    bool bShadow = false;
    if (gPlayers.mo->flags & MF_SHADOW) {
        const uint32_t powerTicksLeft = gPlayers.powers[pw_invisibility];               // Get flash time
        bShadow = (
            (powerTicksLeft >= (5 * TICKSPERSEC)) ||    // Is there a long time left for the power still?
            ((powerTicksLeft & 0x10) != 0)              // Allowed to show while flashing off?
        );
    }

    // Draw the sprites (if valid)
    {
        const pspdef_t* pSprite = gPlayers.psprites;                // Get the first sprite in the array 
        const pspdef_t* const pEndSprite = pSprite + NUMPSPRITES;

        while (pSprite < pEndSprite) {
            if (pSprite->StatePtr) {                // Valid state record?
                DrawAWeapon(*pSprite, bShadow);     // Draw the weapon
            }
            
            ++pSprite;
        }
    }

    // Draw the border
    {
        const uint32_t borderRezNum = gScreenSize + rBACKGROUNDMASK;
        DrawMShape(0,0, (const CelControlBlock*) loadResourceData(borderRezNum));      
        releaseResource(borderRezNum);
    }
}

END_NAMESPACE(Renderer)
