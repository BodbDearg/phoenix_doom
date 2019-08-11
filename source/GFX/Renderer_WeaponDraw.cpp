#include "Renderer_Internal.h"

#include "Base/Tables.h"
#include "Blit.h"
#include "Burger.h"
#include "CelImages.h"
#include "Game/Data.h"
#include "Game/DoomRez.h"
#include "Map/MapData.h"
#include "Things/Info.h"
#include "Things/MapObj.h"
#include "Video.h"

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

    // Figure out what light level to draw the gun at
    float lightMul;

    if ((playerSpriteState.SpriteFrame & FF_FULLBRIGHT) != 0) {
        lightMul = 1.0f;
    } else {
        const LightParams& lightParams = getLightParams(gPlayers.mo->subsector->sector->lightlevel + gExtraLight);
        lightMul = lightParams.getLightMulForDist(0.0f);
    }

    // Decide where to draw the gun sprite part
    float gunX = (float)(img.offsetX + psp.WeaponX);
    float gunY = (float)(img.offsetY + psp.WeaponY + SCREENGUNY);
    gunX *= gGunXScale;
    gunY *= gGunYScale;
    
    // Draw the gun sprite part.
    // If the player has invisibility then draw using alpha blending:
    if (bShadow) {
        Blit::blitSprite<
            Blit::BCF_ALPHA_TEST |
            Blit::BCF_ALPHA_BLEND |
            Blit::BCF_COLOR_MULT_RGB |
            Blit::BCF_COLOR_MULT_A |
            Blit::BCF_H_CLIP |
            Blit::BCF_V_CLIP
        >(
            img.pPixels,
            img.width,
            img.height,
            0.0f,
            0.0f,
            (float) img.width,
            (float) img.height,
            Video::gFrameBuffer + (gScreenYOffset * Video::SCREEN_WIDTH) + gScreenXOffset,
            gScreenWidth,
            gScreenHeight,
            Video::SCREEN_WIDTH,
            (float) gunX,
            (float) gunY,
            (float) img.width * gGunXScale,
            (float) img.height * gGunYScale,
            MF_SHADOW_COLOR_MULT,
            MF_SHADOW_COLOR_MULT,
            MF_SHADOW_COLOR_MULT,
            MF_SHADOW_ALPHA
        );
    }
    else {
        Blit::blitSprite<
            Blit::BCF_ALPHA_TEST |
            Blit::BCF_COLOR_MULT_RGB |
            Blit::BCF_H_CLIP |
            Blit::BCF_V_CLIP
        >(
            img.pPixels,
            img.width,
            img.height,
            0.0f,
            0.0f,
            (float) img.width,
            (float) img.height,
            Video::gFrameBuffer + (gScreenYOffset * Video::SCREEN_WIDTH) + gScreenXOffset,
            gScreenWidth,
            gScreenHeight,
            Video::SCREEN_WIDTH,
            (float) gunX,
            (float) gunY,
            (float) img.width * gGunXScale,
            (float) img.height * gGunYScale,
            lightMul,
            lightMul,
            lightMul
        );
    }

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
