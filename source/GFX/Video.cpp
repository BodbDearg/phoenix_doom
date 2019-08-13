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

uint32_t* Video::gFrameBuffer;

void Video::init() noexcept {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        FATAL_ERROR("Unable to initialize SDL!");
    }
    
    Uint32 windowCreateFlags = 0;
    
    #ifndef __MACOSX__
        windowCreateFlags |= SDL_WINDOW_OPENGL;
    #endif

    // TODO: TEMP
    uint32_t PRESENT_MAGNIFY;

    #if HACK_TEST_HIGH_RES_RENDERING
        PRESENT_MAGNIFY = std::max(1440u / (200u * HACK_TEST_HIGH_RENDER_SCALE), 1u);
    #else
        PRESENT_MAGNIFY = 6;
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

    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    const uint32_t framebufferSize = SCREEN_WIDTH * SCREEN_WIDTH * sizeof(uint32_t);
    gFrameBuffer = (uint32_t*) MemAlloc(framebufferSize);
    
    if (!gRenderer) {
        FATAL_ERROR("Failed to create renderer!");
    }
    
    int textureAccessMode = SDL_TEXTUREACCESS_STREAMING;
    
    #ifdef __MACOSX__
        // Streaming doesn't work for the Metal backend currently!
        textureAccessMode = SDL_TEXTUREACCESS_STATIC;
    #endif

    gFramebufferTexture = SDL_CreateTexture(
        gRenderer,
        SDL_PIXELFORMAT_RGBA8888,
        textureAccessMode,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    if (!gFramebufferTexture) {
        FATAL_ERROR("Failed to create a framebuffer texture!");
    }
}

void Video::shutdown() noexcept {
    MemFree(gFrameBuffer);
    gFrameBuffer = 0;
    SDL_DestroyRenderer(gRenderer);
    gRenderer = 0;
    SDL_DestroyWindow(gWindow);
    gWindow = 0;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void Video::debugClear() noexcept {
    // Clear the framebuffer to pink to spot rendering gaps
    #if ASSERTS_ENABLED
        const uint32_t pinkU32 = 0xFF00FFFF;
        uint32_t* pPixel = gFrameBuffer;
        uint32_t* const pEndPixel = gFrameBuffer + (SCREEN_WIDTH * SCREEN_HEIGHT);

        while (pPixel < pEndPixel) {
            *pPixel = pinkU32;
            ++pPixel;
        }
    #endif    
}

void Video::present() noexcept {
    SDL_UpdateTexture(gFramebufferTexture, NULL, gFrameBuffer, SCREEN_WIDTH * sizeof(uint32_t));    
    SDL_RenderCopy(gRenderer, gFramebufferTexture, NULL, NULL);
    SDL_RenderPresent(gRenderer);
}
