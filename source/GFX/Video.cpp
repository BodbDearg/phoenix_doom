#include "Video.h"

#include "Base/Macros.h"
#include "Base/Mem.h"
#include <SDL.h>

// TODO: REMOVE
#include <algorithm>
#include "Renderer.h"

static SDL_Window*     gWindow;
static SDL_Renderer*   gRenderer;
static SDL_Texture*    gFramebufferTexture;

uint32_t* Video::gpFrameBuffer;
uint32_t* Video::gpSavedFrameBuffer;

static void lockFramebufferTexture() noexcept {
    int pitch = 0;
    if (SDL_LockTexture(gFramebufferTexture, nullptr, reinterpret_cast<void**>(&Video::gpFrameBuffer), &pitch) != 0) {
        FATAL_ERROR("Failed to lock the framebuffer texture for writing!");
    }
}

static void unlockFramebufferTexture() noexcept {
    SDL_UnlockTexture(gFramebufferTexture);
}

void Video::init() noexcept {
    // Initialize SDL subsystems
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        FATAL_ERROR("Unable to initialize SDL!");
    }

    // TODO: TEMP
    uint32_t PRESENT_MAGNIFY;

    #if HACK_TEST_HIGH_RES_RENDERING
        PRESENT_MAGNIFY = std::max(1440u / (200u * HACK_TEST_HIGH_RENDER_SCALE), 1u);
    #else
        PRESENT_MAGNIFY = 6;
    #endif

    // Create the window
    Uint32 windowCreateFlags = 0;

    #ifndef __MACOSX__
        windowCreateFlags |= SDL_WINDOW_OPENGL;
    #endif

    gWindow = SDL_CreateWindow(
        "PhoenixDoom",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * PRESENT_MAGNIFY,
        SCREEN_HEIGHT * PRESENT_MAGNIFY,
        windowCreateFlags
    );

    if (!gWindow) {
        FATAL_ERROR("Unable to create a window!");
    }

    // Create the renderer and framebuffer texture
    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!gRenderer) {
        FATAL_ERROR("Failed to create renderer!");
    }

    gFramebufferTexture = SDL_CreateTexture(
        gRenderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    if (!gFramebufferTexture) {
        FATAL_ERROR("Failed to create a framebuffer texture!");
    }

    // Immediately lock the framebuffer texture for updating
    lockFramebufferTexture();

    // This can be used to take a screenshot for the screen wipe effect
    gpSavedFrameBuffer = new uint32_t[SCREEN_WIDTH * SCREEN_HEIGHT];
}

void Video::shutdown() noexcept {
    delete[] gpSavedFrameBuffer;
    gpSavedFrameBuffer = nullptr;
    gpFrameBuffer = nullptr;
    SDL_DestroyRenderer(gRenderer);
    gRenderer = nullptr;
    SDL_DestroyWindow(gWindow);
    gWindow = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void Video::debugClear() noexcept {
    ASSERT(gpFrameBuffer);

    // Clear the framebuffer to pink to spot rendering gaps
    #if ASSERTS_ENABLED
        const uint32_t pinkU32 = 0x00FF00FF;
        uint32_t* pPixel = gpFrameBuffer;
        uint32_t* const pEndPixel = gpFrameBuffer + (SCREEN_WIDTH * SCREEN_HEIGHT);

        while (pPixel < pEndPixel) {
            *pPixel = pinkU32;
            ++pPixel;
        }
    #endif
}

SDL_Window* Video::getWindow() noexcept {
    return gWindow;
}

void Video::saveFrameBuffer() noexcept {
    ASSERT(gpSavedFrameBuffer);
    ASSERT(gpFrameBuffer);

    // Note: the output framebuffer is saved in column major format.
    // This allows us to do screen wipes more efficiently due to better cache usage.
    const uint32_t screenWidth = SCREEN_WIDTH;
    const uint32_t screenWidth8 = (screenWidth / 8) * 8;
    const uint32_t screenHeight = SCREEN_HEIGHT;

    for (uint32_t y = 0; y < screenHeight; ++y) {
        const uint32_t* const pSrcRow = &gpFrameBuffer[y * screenWidth];
        uint32_t* const pDstRow = &gpSavedFrameBuffer[y];

        // Copy 8 pixels at a time, then mop up the rest
        for (uint32_t x = 0; x < screenWidth8; x += 8) {
            const uint32_t x1 = x + 0;
            const uint32_t x2 = x + 1;
            const uint32_t x3 = x + 2;
            const uint32_t x4 = x + 3;
            const uint32_t x5 = x + 4;
            const uint32_t x6 = x + 5;
            const uint32_t x7 = x + 6;
            const uint32_t x8 = x + 7;

            const uint32_t pix1 = pSrcRow[x1];
            const uint32_t pix2 = pSrcRow[x2];
            const uint32_t pix3 = pSrcRow[x3];
            const uint32_t pix4 = pSrcRow[x4];
            const uint32_t pix5 = pSrcRow[x5];
            const uint32_t pix6 = pSrcRow[x6];
            const uint32_t pix7 = pSrcRow[x7];
            const uint32_t pix8 = pSrcRow[x8];

            pDstRow[x1 * screenHeight] = pix1;
            pDstRow[x2 * screenHeight] = pix2;
            pDstRow[x3 * screenHeight] = pix3;
            pDstRow[x4 * screenHeight] = pix4;
            pDstRow[x5 * screenHeight] = pix5;
            pDstRow[x6 * screenHeight] = pix6;
            pDstRow[x7 * screenHeight] = pix7;
            pDstRow[x8 * screenHeight] = pix8;
        }

        for (uint32_t x = screenWidth8; x < screenWidth; x++) {
            const uint32_t pix = pSrcRow[x];
            pDstRow[x * screenHeight] = pix;
        }
    }
}

void Video::present() noexcept {
    unlockFramebufferTexture();
    SDL_RenderCopy(gRenderer, gFramebufferTexture, NULL, NULL);
    SDL_RenderPresent(gRenderer);
    lockFramebufferTexture();
}

void Video::endFrame(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    if (bSaveFrameBuffer) {
        saveFrameBuffer();
    }

    if (bPresent) {
        present();
    }
}
