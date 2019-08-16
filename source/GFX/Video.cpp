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

void Video::saveFrameBuffer() noexcept {
    ASSERT(gpSavedFrameBuffer);
    ASSERT(gpFrameBuffer);
    std::memcpy(gpSavedFrameBuffer, gpFrameBuffer, sizeof(uint32_t) * SCREEN_WIDTH * SCREEN_HEIGHT);
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
