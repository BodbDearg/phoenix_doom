#include "Renderer_Internal.h"

#include "Base/Tables.h"
#include "Game/Data.h"
#include "Video.h"
#include <algorithm>

BEGIN_NAMESPACE(Renderer)

static void doInvulnerabilityEffect() noexcept {
    // The invunerability effect in 3DO Doom was a simple bit inverse.
    // The 3DO game did not use the palette switching technique that the PC version did because there was no palette...
    const uint32_t viewH = g3dViewHeight;
    const uint32_t screenH = Video::gScreenWidth;

    for (uint32_t y = 0; y <= viewH; ++y) {
        // Process this row of pixels and invert RGB values
        uint32_t* pPixel = &Video::gpFrameBuffer[g3dViewXOffset + (g3dViewYOffset + y) * screenH];
        uint32_t* const pEndPixel = pPixel + g3dViewWidth;

        while (pPixel < pEndPixel) {
            const uint32_t color = *pPixel;
            *pPixel = (~color) & 0x00FFFFFFu;
            ++pPixel;
        }
    }
}

static void doTintEffect(const uint32_t r5, const uint32_t g5, const uint32_t b5) noexcept {
    // If there is no effect do nothing
    if (r5 == 0 && g5 == 0 && b5 == 0)
        return;

    // Create a fixed point multiplier for RGB values
    constexpr Fixed COL5_MAX_FRAC = intToFixed16(31);
    constexpr Fixed COL8_MAX_FRAC = intToFixed16(255);
    constexpr Fixed EFFECT_STRENGHT = intToFixed16(2);

    const Fixed rMul = FRACUNIT + fixed16Mul(fixed16Div(intToFixed16(r5), COL5_MAX_FRAC), EFFECT_STRENGHT);
    const Fixed gMul = FRACUNIT + fixed16Mul(fixed16Div(intToFixed16(g5), COL5_MAX_FRAC), EFFECT_STRENGHT);
    const Fixed bMul = FRACUNIT + fixed16Mul(fixed16Div(intToFixed16(b5), COL5_MAX_FRAC), EFFECT_STRENGHT);

    // Modulate all of the RGB values in the framebuffer
    const uint32_t screenH = g3dViewHeight;

    for (uint32_t y = 0; y <= screenH; ++y) {
        // Process this row of pixels
        uint32_t* pPixel = &Video::gpFrameBuffer[g3dViewXOffset + (g3dViewYOffset + y) * Video::gScreenWidth];
        uint32_t* const pEndPixel = pPixel + g3dViewWidth;

        while (pPixel < pEndPixel) {
            // Get the old colors from 0-1
            const uint32_t color = *pPixel;
            const Fixed oldRFrac = intToFixed16((color >> 16) & 0xFFu);
            const Fixed oldGFrac = intToFixed16((color >> 8) & 0xFFu);
            const Fixed oldBFrac = intToFixed16(color & 0xFFu);

            // Modulate and clamp
            const Fixed rFrac = std::min(fixed16Mul(oldRFrac, rMul), 255 * FRACUNIT);
            const Fixed gFrac = std::min(fixed16Mul(oldGFrac, gMul), 255 * FRACUNIT);
            const Fixed bFrac = std::min(fixed16Mul(oldBFrac, bMul), 255 * FRACUNIT);

            // Make a framebuffer color
            const uint32_t finalColor = (
                ((uint32_t) fixed16ToInt(rFrac) << 16) |
                ((uint32_t) fixed16ToInt(gFrac) << 8) |
                ((uint32_t) fixed16ToInt(bFrac))
            );

            *pPixel = finalColor;
            ++pPixel;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Does post processing fx on the entire 3D view
//----------------------------------------------------------------------------------------------------------------------
void doPostFx() noexcept {
    const player_t& player = gPlayer;

    // See if we are to do the invulnerability effect.
    // If this effect is in place then do that exclusively and nothing else:
    const uint32_t invunTicksLeft = player.powers[pw_invulnerability];
    const bool bDoInvunFx = (
        (invunTicksLeft > TICKSPERSEC * 4) ||   // Full strength?
        (invunTicksLeft & 0x10)                 // Flashing?
    );

    if (bDoInvunFx) {
        doInvulnerabilityEffect();
        return;
    }

    // Do color effects due to other powerups and pain/item-pickup
    const uint32_t pickupFx = player.bonuscount / 2;

    uint32_t redFx = player.damagecount + pickupFx;
    uint32_t greenFx = pickupFx;
    uint32_t blueFx = 0;

    const uint32_t radSuitTicksLeft = player.powers[pw_ironfeet];
    const uint32_t beserkTicksLeft = player.powers[pw_strength];

    const bool bDoRadSuitFx = (
        (radSuitTicksLeft > TICKSPERSEC * 4) ||   // Full strength?
        (radSuitTicksLeft & 0x10)                 // Flashing?
    );

    if (bDoRadSuitFx) {
        greenFx = 15;
    }

    if (beserkTicksLeft > 0 && beserkTicksLeft < 255) {
        uint32_t color = 255 - beserkTicksLeft;
        color >>= 4;
        redFx += color;     // Feeling good!
    }

    if (player.cheatFxTicksLeft > 0) {
        blueFx += player.cheatFxTicksLeft;
    }

    redFx = std::min(redFx, 31u);
    greenFx = std::min(greenFx, 31u);
    blueFx = std::min(blueFx, 31u);

    doTintEffect(redFx, greenFx, blueFx);
}

END_NAMESPACE(Renderer)
