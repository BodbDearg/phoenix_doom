#include "Renderer_Internal.h"

#include "Base/Endian.h"
#include "Base/Tables.h"
#include "Burger.h"
#include "CelImages.h"
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
    // Get the image to draw for this weapon
    const state_t& playerSpriteState = *psp.StatePtr;
    const uint32_t resourceNum = playerSpriteState.SpriteFrame >> FF_SPRITESHIFT;
    const CelImageArray& weaponImgs = CelImages::loadImages(
        resourceNum,
        CelImages::LoadFlagBits::MASKED | CelImages::LoadFlagBits::HAS_OFFSETS
    );
    const CelImage& img = weaponImgs.getImage(playerSpriteState.SpriteFrame & FF_FRAMEMASK);
    
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

    // FIXME: DC: need to do gun scaling etc. here
    int32_t x = img.offsetX;
    int32_t y = img.offsetY;
    x = ((psp.WeaponX + x ) * (int32_t) gGunXScale) >> 20;
    y = ((psp.WeaponY + SCREENGUNY + y) * (int32_t) gGunYScale) >> 16;
    x += gScreenXOffset;
    y += gScreenYOffset + 2;            // Add 2 pixels to cover up the hole in the bottom
    DrawShape(x, y, img);               // Draw the weapon's shape

    CelImages::releaseImages(resourceNum);
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
        DrawShape(0, 0, CelImages::loadImage(borderRezNum, CelImages::LoadFlagBits::MASKED));      
        CelImages::releaseImages(borderRezNum);
    }
}

END_NAMESPACE(Renderer)
