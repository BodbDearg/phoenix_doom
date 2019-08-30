#include "IntroMovies.h"

#include "Base/FileUtils.h"
#include "Base/Finally.h"
#include "Base/Input.h"
#include "Base/Tables.h"
#include "Burger.h"
#include "Game/Data.h"
#include "Game/DoomMain.h"
#include "GFX/Blit.h"
#include "GFX/Video.h"
#include "ThreeDO/CelUtils.h"

BEGIN_NAMESPACE(IntroLogos)

enum class LogoState {
    FADE_IN,
    HOLD,
    FADE_OUT
};

static CelImage     gLogoImg            = {};
static float        gLogoFadeInTime     = 1.0f;
static float        gLogoHoldTime       = 1.0f;
static float        gLogoFadeOutTime    = 1.0f;
static LogoState    gLogoCurState       = LogoState::FADE_IN;
static float        gLogoCurStateTime   = 0.0f;

static void loadLogo(const char* path) noexcept {
    // Try to load the logo file into memory
    std::byte* pLogoFileData = nullptr;
    size_t logoFileSize = 0;

    auto cleanupMovieFileData = finally([&]() {
        delete[] pLogoFileData;
    });

    if (!FileUtils::getContentsOfFile(path, pLogoFileData, logoFileSize))
        return;

    // Load the logo CEL from that
    if (!CelUtils::loadCelImage(pLogoFileData, (uint32_t) logoFileSize, CelLoadFlagBits::NONE, gLogoImg)) {
        gLogoImg.free();
    }
}

static void on3doLogoStarting() noexcept {
    gDoWipe = false;
    gLogoFadeInTime = 0.0f;
    gLogoHoldTime = 3.0f;
    gLogoFadeOutTime = 1.0f;
    gLogoCurState = LogoState::HOLD;

    loadLogo("3do.logo.cel");
}

static void onIdLogoStarting() noexcept {
    gDoWipe = false;
    gLogoFadeInTime = 1.0f;
    gLogoHoldTime = 2.0f;
    gLogoFadeOutTime = 1.0f;
    gLogoCurState = LogoState::FADE_IN;

    loadLogo("IDLogo.cel");
}

static void onLogoStopping() noexcept {
    gLogoImg.free();
}

static gameaction_e updateLogo() noexcept {
    // If there is no logo then we are done
    if (!gLogoImg.pPixels)
        return ga_completed;

    // Skip pressed?
    const uint32_t buttonsJustReleased = (~gJoyPadButtons) & gPrevJoyPadButtons;

    if ((buttonsJustReleased & (PadA|PadB|PadC|PadD)) != 0)
        return ga_exitdemo;

    // Advance logo state
    gLogoCurStateTime += SECS_PER_TICK;

    if (gLogoCurState == LogoState::FADE_IN) {
        if (gLogoCurStateTime >= gLogoFadeInTime) {
            gLogoCurStateTime = 0;
            gLogoCurState = LogoState::HOLD;
        }
    } 
    else if (gLogoCurState == LogoState::HOLD) {
        if (gLogoCurStateTime >= gLogoHoldTime) {
            gLogoCurStateTime = 0;
            gLogoCurState = LogoState::FADE_OUT;
        }
    }
    else {
        if (gLogoCurStateTime >= gLogoFadeOutTime) {
            return ga_completed;
        }
    }

    return ga_nothing;
}

static void drawLogo(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    if (!gLogoImg.pPixels)
        return;

    // Figure out the logo brightness multiplier
    float brightnessMul = 1.0f;

    if (gLogoCurState == LogoState::FADE_IN) {
        brightnessMul = std::min(gLogoCurStateTime / gLogoFadeInTime, 1.0f);
    }
    else if (gLogoCurState == LogoState::FADE_OUT) {
        brightnessMul = 1.0f - std::min(gLogoCurStateTime / gLogoFadeOutTime, 1.0f);
    }

    // Figure out positition for the logo (in the original 320x200 resolution)
    const int32_t logoX = ((int32_t) Renderer::REFERENCE_SCREEN_WIDTH - (int32_t) gLogoImg.width) / 2;
    const int32_t logoY = ((int32_t) Renderer::REFERENCE_SCREEN_HEIGHT - (int32_t) gLogoImg.height) / 2;

    // Now figure out the scaled position and size of the logo
    const float xScaled = (float) logoX * gScaleFactor;
    const float yScaled = (float) logoY * gScaleFactor;
    const float wScaled = (float) gLogoImg.width * gScaleFactor;
    const float hScaled = (float) gLogoImg.height * gScaleFactor;

    // Draw logo
    Blit::blitSprite<
        Blit::BCF_H_CLIP |
        Blit::BCF_V_CLIP |
        Blit::BCF_COLOR_MULT_RGB
    >(
        gLogoImg.pPixels,
        gLogoImg.width,
        gLogoImg.height,
        0.0f,
        0.0f,
        (float) gLogoImg.width,
        (float) gLogoImg.height,
        Video::gpFrameBuffer,
        Video::SCREEN_WIDTH,
        Video::SCREEN_HEIGHT,
        Video::SCREEN_WIDTH,
        xScaled,
        yScaled,
        wScaled,
        hScaled,
        brightnessMul,
        brightnessMul,
        brightnessMul
    );

    Video::endFrame(bPresent, bSaveFrameBuffer);
}

void run() noexcept {
    if (Input::isQuitRequested())
        return;

    if (MiniLoop(on3doLogoStarting, onLogoStopping, updateLogo, drawLogo) == ga_quit)
        return;
    
    MiniLoop(onIdLogoStarting, onLogoStopping, updateLogo, drawLogo);
}

END_NAMESPACE(IntroLogos)
