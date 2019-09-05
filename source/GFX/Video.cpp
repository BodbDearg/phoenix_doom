#include "Video.h"

#include "Base/Macros.h"
#include "Base/Mem.h"
#include "Game/Config.h"
#include <algorithm>
#include <cmath>
#include <SDL.h>

BEGIN_NAMESPACE(Video)

static SDL_Window*     gWindow;
static SDL_Renderer*   gRenderer;
static SDL_Texture*    gFramebufferTexture;

uint32_t    gScreenWidth;
uint32_t    gScreenHeight;
uint32_t    gVideoOutputWidth;
uint32_t    gVideoOutputHeight;
SDL_Rect    gOutputRect;
bool        gbIsFullscreen;
uint32_t*   gpFrameBuffer;
uint32_t*   gpSavedFrameBuffer;

static void lockFramebufferTexture() noexcept {
    int pitch = 0;
    if (SDL_LockTexture(gFramebufferTexture, nullptr, reinterpret_cast<void**>(&Video::gpFrameBuffer), &pitch) != 0) {
        FATAL_ERROR("Failed to lock the framebuffer texture for writing!");
    }
}

static void unlockFramebufferTexture() noexcept {
    SDL_UnlockTexture(gFramebufferTexture);
}

static void determineTargetVideoMode() noexcept {
    // Fullscreen mode and game render resolution
    gbIsFullscreen = Config::gbFullscreen;

    if (Config::gRenderScale <= 0) {
        FATAL_ERROR_F("Invalid render scale '%u'!", Config::gRenderScale);
    }

    gScreenWidth = Config::gRenderScale * REFERENCE_SCREEN_WIDTH;
    gScreenHeight = Config::gRenderScale * REFERENCE_SCREEN_HEIGHT;

    // Get the current screen resolution.
    // Note: MAY not be correct for multiple monitors, but a user can specify manually in those cases.
    SDL_DisplayMode screenMode = {};

    if (SDL_GetCurrentDisplayMode(0, &screenMode) != 0) {
        FATAL_ERROR_F("Failed to determine current screen video mode!");
    }

    // Determine output width and height.
    //
    // Cases:
    //  (1) If output size is specified manually then just use that.
    //  (2) If using 'auto' resolution and fullscreen mode, then use the screen resolution as the output size.
    //  (3) If using 'auto' resolution in windowed mode, determine the output size from the closest fit of
    //      the rendered resolution we can do to the screen resolution, while using integer-only scaling.
    //
    if (Config::gOutputResolutionW > 0) {
        gVideoOutputWidth = Config::gOutputResolutionW;
    } else {
        if (gbIsFullscreen) {
            gVideoOutputWidth = (uint32_t) screenMode.w; 
        } else {
            gVideoOutputWidth = std::max(screenMode.w / gScreenWidth, 1u) * gScreenWidth;
        }
    }

    if (Config::gOutputResolutionH > 0) {
        gVideoOutputHeight = Config::gOutputResolutionH;
    } else {
        if (gbIsFullscreen) {
            gVideoOutputHeight = (uint32_t) screenMode.h;
        } else {
            gVideoOutputHeight = std::max(screenMode.h / gScreenHeight, 1u) * gScreenHeight;
        }
    }

    const bool bInvalidOutputSize = (
        (gVideoOutputWidth < gScreenWidth) ||
        (gVideoOutputHeight < gScreenHeight)
    );

    if (bInvalidOutputSize) {
        FATAL_ERROR_F(
            "Size of rendered game image (%u x %u) exceeds output area size (%u x %u)! "
            "The game only supports outputting the rendered images to an equal sized or larger area, "
            "downscaling is not supported!",
            gScreenWidth,
            gScreenHeight,
            gVideoOutputWidth,
            gVideoOutputHeight
        );
    }

    // Determine the width and height of the output rect.
    // Obey integer scaling and aspect correct rules.
    // Center the output also, if there is space leftover...
    float outputScaleX = (float) gVideoOutputWidth / (float) gScreenWidth;
    float outputScaleY = (float) gVideoOutputHeight / (float) gScreenHeight;

    if (Config::gbAspectCorrectOutputScaling) {
        const float minScale = std::min(outputScaleX, outputScaleY);
        outputScaleX = minScale;
        outputScaleY = minScale;
    }

    if (Config::gbIntegerOutputScaling) {
        outputScaleX = std::trunc(outputScaleX);
        outputScaleY = std::trunc(outputScaleY);
    }

    
    if (!gbIsFullscreen) {
        // Trim the window sizes in windowed mode if auto size is enabled
        if (Config::gOutputResolutionW <= 0) {
            gVideoOutputWidth = (int)((float) gScreenWidth * outputScaleX);
        }

        if (Config::gOutputResolutionH <= 0) {
            gVideoOutputHeight = (int)((float) gScreenHeight * outputScaleY);
        }
    }

    gOutputRect.w = (int)((float) gScreenWidth * outputScaleX);
    gOutputRect.h = (int)((float) gScreenHeight * outputScaleY);
    gOutputRect.x = (gVideoOutputWidth - gOutputRect.w) / 2;
    gOutputRect.y = (gVideoOutputHeight - gOutputRect.h) / 2;
}

void init() noexcept {
    // Initialize SDL subsystems
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        FATAL_ERROR("Unable to initialize SDL!");
    }

    // Determine the video mode to use
    determineTargetVideoMode();

    // Create the window
    Uint32 windowCreateFlags = (gbIsFullscreen) ? SDL_WINDOW_FULLSCREEN : 0;

    #ifndef __MACOSX__
        windowCreateFlags |= SDL_WINDOW_OPENGL;
    #endif

    gWindow = SDL_CreateWindow(
        "PhoenixDoom",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        gVideoOutputWidth,
        gVideoOutputHeight,
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
        gScreenWidth,
        gScreenHeight
    );

    if (!gFramebufferTexture) {
        FATAL_ERROR("Failed to create a framebuffer texture!");
    }

    // Clear the renderer to black
    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 0);
    SDL_RenderClear(gRenderer);

    // Immediately lock the framebuffer texture for updating
    lockFramebufferTexture();

    // This can be used to take a screenshot for the screen wipe effect
    gpSavedFrameBuffer = new uint32_t[(size_t) gScreenWidth * gScreenHeight];
}

void shutdown() noexcept {
    delete[] gpSavedFrameBuffer;
    gpSavedFrameBuffer = nullptr;
    gpFrameBuffer = nullptr;
    SDL_DestroyRenderer(gRenderer);
    gRenderer = nullptr;
    SDL_DestroyWindow(gWindow);
    gWindow = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    gScreenWidth = 0;
    gScreenHeight = 0;
    gVideoOutputWidth = 0;
    gVideoOutputHeight = 0;
    gOutputRect = {};
    gbIsFullscreen = false;
}

void clearScreen(const uint8_t r, const uint8_t g, const uint8_t b) noexcept {
    ASSERT(gpFrameBuffer);    

    // Cache these for the loop
    uint32_t* const pFramebuffer = gpFrameBuffer;
    const uint32_t clearColorXRGB8888 = ((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) b;

    // Clear 16 pixels at a time at first
    const uint32_t endPixelIdx = gScreenWidth * gScreenHeight;
    const uint32_t endPixelIdx16 = endPixelIdx & 0xFFFFFFF0u;
    uint32_t pixelIdx = 0;
    
    while (pixelIdx < endPixelIdx16) {
        pFramebuffer[pixelIdx + 0 ] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 1 ] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 2 ] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 3 ] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 4 ] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 5 ] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 6 ] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 7 ] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 8 ] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 9 ] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 10] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 11] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 12] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 13] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 14] = clearColorXRGB8888;
        pFramebuffer[pixelIdx + 15] = clearColorXRGB8888;
        pixelIdx += 16;
    }

    // Clear any remaining pixels
    while (pixelIdx < endPixelIdx) {
        pFramebuffer[pixelIdx] = clearColorXRGB8888;
        ++pixelIdx;
    }
}

void debugClearScreen() noexcept {
    // Clear the framebuffer to pink to spot rendering gaps
    #if ASSERTS_ENABLED
        clearScreen(255, 0, 255);
    #endif
}

SDL_Window* getWindow() noexcept {
    return gWindow;
}

void saveFrameBuffer() noexcept {
    ASSERT(gpSavedFrameBuffer);
    ASSERT(gpFrameBuffer);

    // Note: the output framebuffer is saved in column major format.
    // This allows us to do screen wipes more efficiently due to better cache usage.
    const uint32_t screenWidth = gScreenWidth;
    const uint32_t screenWidth8 = (screenWidth / 8) * 8;
    const uint32_t screenHeight = gScreenHeight;

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

void present() noexcept {
    unlockFramebufferTexture();
    SDL_RenderCopy(gRenderer, gFramebufferTexture, NULL, &gOutputRect);
    SDL_RenderPresent(gRenderer);
    lockFramebufferTexture();
}

void endFrame(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    if (bSaveFrameBuffer) {
        saveFrameBuffer();
    }

    if (bPresent) {
        present();
    }
}

END_NAMESPACE(Video)
