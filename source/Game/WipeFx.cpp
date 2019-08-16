#include "WipeFx.h"

#include "Base/Input.h"
#include "Base/Random.h"
#include "Base/Tables.h"
#include "GFX/Video.h"
#include "TickCounter.h"
#include <algorithm>
#include <memory>
#include <thread>
#include <vector>

BEGIN_NAMESPACE(WipeFx)

//----------------------------------------------------------------------------------------------------------------------
// Wipe speed settings.
// Note that these are scaled according to the resolution scale factor too, so we are not too slow at high resolution.
//----------------------------------------------------------------------------------------------------------------------
static constexpr float WIPE_SPEED_PRE_INTRO = 0.75f;    // Note: this was 0.5 in the original 3DO version, speeding it up slightly!
static constexpr float WIPE_SPEED_INTRO_MULT = 2.0f;
static constexpr float WIPE_SPEED_INTRO_ADD = WIPE_SPEED_PRE_INTRO;
static constexpr float WIPE_SPEED_CONSTANT = WIPE_SPEED_PRE_INTRO * 8.0f;

//----------------------------------------------------------------------------------------------------------------------
// Generates the values that randomize the wipe.
// These create the jagged look of the wipe.
//----------------------------------------------------------------------------------------------------------------------
static std::unique_ptr<float[]> generateYDeltaTable() noexcept {
    const uint32_t screenWidth = Video::SCREEN_WIDTH;
    const float scaleFactor = gScaleFactor;
    const uint32_t numRepeatedCols = std::max((uint32_t) gScaleFactor, 1u);     // Repeat same deltas according to the scale factor multiplier, so the wipe looks more pixelated
    const float minDelta = -16.0f * scaleFactor;
    const float maxDelta = 0.0f;

    std::unique_ptr<float[]> deltas(new float[screenWidth]);

    float delta = Random::nextFloat() * -16.0f * scaleFactor;

    for (uint32_t x = 0; x < numRepeatedCols; ++x) {
        deltas[x] = delta;
    }

    for (uint32_t x = numRepeatedCols; x < screenWidth; x += numRepeatedCols) {
        delta += (float)(Random::nextFloat() * 2.0f - 1.0f) * scaleFactor;  // Add -1, 0 or 1
        delta = std::min(delta, maxDelta);  // Too high?
        delta = std::max(delta, minDelta);  // Too low?

        uint32_t xCur = x;
        uint32_t xEnd = std::min(x + numRepeatedCols, screenWidth);
        
        while (xCur < xEnd) {
            deltas[xCur] = delta;
            ++xCur;
        }
    }

    return deltas;
}

//----------------------------------------------------------------------------------------------------------------------
// Does an update of the wipe.
// Returns true if it is time to exit the wipe.
//----------------------------------------------------------------------------------------------------------------------
static bool tickWipe(float* const pYDeltas) noexcept {
    const uint32_t screenWidth = Video::SCREEN_WIDTH;
    const uint32_t screenHeight = Video::SCREEN_HEIGHT;
    
    const float screenHeightF = (float) screenHeight;
    const float scaleFactor = gScaleFactor;

    const float introDeltaThreshold = 16.0f * gScaleFactor;
    const float scaledWipeSpeedIntroAdd = WIPE_SPEED_INTRO_ADD * scaleFactor;
    const float scaledWipeSpeedConstant = WIPE_SPEED_CONSTANT * scaleFactor;

    bool bWipeDone = true;

    for (uint32_t x = 0; x < screenWidth; ++x) {
        float delta = pYDeltas[x];

        // Is the line finished?
        // If not then the overall wipe is not done:
        if (delta < screenHeightF) {
            bWipeDone = false;
            
            if (delta < 0.0f) {
                // Slight delay
                delta += WIPE_SPEED_PRE_INTRO * scaleFactor;
            }
            else if (delta < introDeltaThreshold) {
                // Double the wipe: wipe starts off very fast
                delta *= WIPE_SPEED_INTRO_MULT;
                delta += scaledWipeSpeedIntroAdd;
            } else {
                // Constant speed wipe once it's gone a bit down
                delta += scaledWipeSpeedConstant;
            }

            delta = std::min(delta, screenHeightF);
            pYDeltas[x] = delta;
        }
    }

    return bWipeDone;
}

//----------------------------------------------------------------------------------------------------------------------
// Does the drawing for the wipe
//----------------------------------------------------------------------------------------------------------------------
static void drawWipe(
    const uint32_t* const gpOldImg,
    const uint32_t* const gpNewImg,
    const float* const pYDeltas
) noexcept {
    const uint32_t screenWidth = Video::SCREEN_WIDTH;
    const uint32_t screenHeight = Video::SCREEN_HEIGHT;
    uint32_t* const pDstPixels = Video::gpFrameBuffer;

    for (uint32_t x = 0; x < screenWidth; ++x) {
        uint32_t* const pDstPixelsCol = pDstPixels + x;

        //--------------------------------------------------------------------------------------------------------------
        // Add in the new image pixels at the top: do 8 pixel batches at a time, then the remainder pixels
        //--------------------------------------------------------------------------------------------------------------
        const uint32_t yDelta = std::min((uint32_t) std::max(pYDeltas[x], 0.0f), screenHeight);
        const uint32_t yDelta8 = (yDelta / 8) * 8;
        const uint32_t* const gpNewImgCol = gpNewImg + x;

        for (uint32_t y = 0; y < yDelta8; y += 8) {
            const uint32_t idx1 = y * screenWidth;
            const uint32_t idx2 = (y + 1) * screenWidth;
            const uint32_t idx3 = (y + 2) * screenWidth;
            const uint32_t idx4 = (y + 3) * screenWidth;
            const uint32_t idx5 = (y + 4) * screenWidth;
            const uint32_t idx6 = (y + 5) * screenWidth;
            const uint32_t idx7 = (y + 6) * screenWidth;
            const uint32_t idx8 = (y + 7) * screenWidth;

            pDstPixelsCol[idx1] = gpNewImgCol[idx1];
            pDstPixelsCol[idx2] = gpNewImgCol[idx2];
            pDstPixelsCol[idx3] = gpNewImgCol[idx3];
            pDstPixelsCol[idx4] = gpNewImgCol[idx4];
            pDstPixelsCol[idx5] = gpNewImgCol[idx5];
            pDstPixelsCol[idx6] = gpNewImgCol[idx6];
            pDstPixelsCol[idx7] = gpNewImgCol[idx7];
            pDstPixelsCol[idx8] = gpNewImgCol[idx8];
        }

        for (uint32_t y = yDelta8; y < yDelta; ++y) {
            const uint32_t idx = y * screenWidth;
            pDstPixelsCol[idx] = gpNewImgCol[idx];
        }

        //--------------------------------------------------------------------------------------------------------------
        // Add in the old image pixels: do 8 pixel batches at a time, then the remainder pixels
        //--------------------------------------------------------------------------------------------------------------
        const uint32_t numPixelsRemaining = screenHeight - yDelta;
        const uint32_t screenHeight8 = yDelta + (numPixelsRemaining / 8) * 8;
        const uint32_t* const gpOldImgCol = gpOldImg + x;

        for (uint32_t y = yDelta; y < screenHeight8; y += 8) {
            const uint32_t oldPixelBaseY = y - yDelta;
            const uint32_t oldPixel1 = gpOldImgCol[oldPixelBaseY * screenWidth];
            const uint32_t oldPixel2 = gpOldImgCol[(oldPixelBaseY + 1) * screenWidth];
            const uint32_t oldPixel3 = gpOldImgCol[(oldPixelBaseY + 2) * screenWidth];
            const uint32_t oldPixel4 = gpOldImgCol[(oldPixelBaseY + 3) * screenWidth];
            const uint32_t oldPixel5 = gpOldImgCol[(oldPixelBaseY + 4) * screenWidth];
            const uint32_t oldPixel6 = gpOldImgCol[(oldPixelBaseY + 5) * screenWidth];
            const uint32_t oldPixel7 = gpOldImgCol[(oldPixelBaseY + 6) * screenWidth];
            const uint32_t oldPixel8 = gpOldImgCol[(oldPixelBaseY + 7) * screenWidth];

            pDstPixelsCol[y * screenWidth] = oldPixel1;
            pDstPixelsCol[(y + 1) * screenWidth] = oldPixel2;
            pDstPixelsCol[(y + 2) * screenWidth] = oldPixel3;
            pDstPixelsCol[(y + 3) * screenWidth] = oldPixel4;
            pDstPixelsCol[(y + 4) * screenWidth] = oldPixel5;
            pDstPixelsCol[(y + 5) * screenWidth] = oldPixel6;
            pDstPixelsCol[(y + 6) * screenWidth] = oldPixel7;
            pDstPixelsCol[(y + 7) * screenWidth] = oldPixel8;
        }

        for (uint32_t y = screenHeight8; y < screenHeight; ++y) {
            const uint32_t oldPixel = gpOldImgCol[(y - yDelta) * screenWidth];
            pDstPixelsCol[y * screenWidth] = oldPixel;
        }
    }
}

void doWipe(const GameLoopDrawFunc drawFunc) noexcept {
    // Firstly make a copy of the current saved framebuffer
    const uint32_t numFramebufferPixels = Video::SCREEN_WIDTH * Video::SCREEN_HEIGHT;
    std::unique_ptr<uint32_t[]> oldFramebuffer(new uint32_t[numFramebufferPixels]);
    std::memcpy(oldFramebuffer.get(), Video::gpSavedFrameBuffer, sizeof(uint32_t) * numFramebufferPixels);

    // Render with the drawer and save it to the saved framebuffer.
    // Ensure that the drawer does NOT present:
    drawFunc(false, true);

    // Generate the Y delta table that randomizes the wipe
    std::unique_ptr<float[]> yDeltas = generateYDeltaTable();

    // Initialize ticking and continue until the wipe is done
    TickCounter::init();
    bool bWipeDone = false;

    while (!bWipeDone) {
        // Is it time do a wipe frame yet?
        uint32_t ticksLeftToSimulate = TickCounter::update();

        if (ticksLeftToSimulate <= 0) {
            std::this_thread::yield();
            continue;
        }

        // Update input and if a quit was requested then exit immediately
        Input::update();

        if (Input::quitRequested()) {
            break;
        }

        while (ticksLeftToSimulate > 0 && (!bWipeDone)) {
            bWipeDone = tickWipe(yDeltas.get());
            --ticksLeftToSimulate;
        }

        // Draw the wipe and present
        drawWipe(oldFramebuffer.get(), Video::gpSavedFrameBuffer, yDeltas.get());
        Video::present();
    }

    // Cleanup
    Input::consumeEvents();     // No lingering keypresses
    TickCounter::shutdown();
}

END_NAMESPACE(WipeFx)
