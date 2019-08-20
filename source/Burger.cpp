#include "Burger.h"

#include "Base/Input.h"
#include "GFX/Blit.h"
#include "GFX/Video.h"

uint32_t ReadJoyButtons(uint32_t PadNum) noexcept {
    // DC: FIXME: TEMP
    uint32_t buttons = 0;

    if (Input::isKeyPressed(SDL_SCANCODE_UP)) {
        buttons |= PadUp;
    }

    if (Input::isKeyPressed(SDL_SCANCODE_DOWN)) {
        buttons |= PadDown;
    }

    if (Input::isKeyPressed(SDL_SCANCODE_LEFT)) {
        buttons |= PadLeft;
    }

    if (Input::isKeyPressed(SDL_SCANCODE_RIGHT)) {
        buttons |= PadRight;
    }

    if (Input::isKeyPressed(SDL_SCANCODE_A)) {
        buttons |= PadA;
    }

    if (Input::isKeyPressed(SDL_SCANCODE_S)) {
        buttons |= PadB;
    }

    if (Input::isKeyPressed(SDL_SCANCODE_D)) {
        buttons |= PadC;
    }

    if (Input::isKeyPressed(SDL_SCANCODE_Z)) {
        buttons |= PadX;
    }

    if (Input::isKeyPressed(SDL_SCANCODE_X)) {
        buttons |= PadStart;
    }

    if (Input::isKeyPressed(SDL_SCANCODE_Q)) {
        buttons |= PadLeftShift;
    }

    if (Input::isKeyPressed(SDL_SCANCODE_E)) {
        buttons |= PadRightShift;
    }

    #if ENABLE_DEBUG_CAMERA_Z_MOVEMENT
        if (Input::isKeyPressed(SDL_SCANCODE_PAGEUP)) {
            Renderer::gDebugCameraZOffset += 1.0f;
        }

        if (Input::isKeyPressed(SDL_SCANCODE_PAGEDOWN)) {
            Renderer::gDebugCameraZOffset -= 1.0f;
        }
    #endif

    return buttons;
}

/****************************************

    Prints an unsigned long number.
    Also prints lead spaces or zeros if needed

****************************************/

static uint32_t gTensTable[] = {
1,              // Table to quickly div by 10
10,
100,
1000,
10000,
100000,
1000000,
10000000,
100000000,
1000000000
};

void LongWordToAscii(uint32_t Val, char* AsciiPtr) noexcept
{
    uint32_t Index;      // Index to TensTable
    uint32_t BigNum;    // Temp for TensTable value
    uint32_t Letter;        // ASCII char
    uint32_t Printing;      // Flag for printing

    Index = 10;      // 10 digits to process
    Printing = false;   // Not printing yet
    do {
        --Index;        // Dec index
        BigNum = gTensTable[Index];  // Get div value in local
        Letter = '0';            // Init ASCII value
        while (Val>=BigNum) {    // Can I divide?
            Val-=BigNum;        // Remove value
            ++Letter;            // Inc ASCII value
        }
        if (Printing || Letter!='0' || !Index) {    // Already printing?
            Printing = true;        // Force future printing
            AsciiPtr[0] = Letter;       // Also must print on last char
            ++AsciiPtr;
        }
    } while (Index);        // Any more left?
    AsciiPtr[0] = 0;        // Terminate the string
}

/**********************************

    Save a file to NVRAM or disk

**********************************/

uint32_t SaveAFile(const char* FileName, void *data, uint32_t dataSize) noexcept
{
    // DC: FIXME: reimplement/replace
    return -1;
}

/**********************************

    This code is functionally equivalent to the Burgerlib
    version except that it is using the cached CCB system.

**********************************/
void DrawARect(const uint32_t x, const uint32_t y, const uint32_t width, const uint32_t height, const uint16_t color) noexcept {

    const uint32_t color32 = Video::rgba5551ToScreenCol(color);

    // Clip the rect bounds
    if (x >= Video::SCREEN_WIDTH || y >= Video::SCREEN_HEIGHT)
        return;

    const uint32_t xEnd = std::min(x + width, Video::SCREEN_WIDTH);
    const uint32_t yEnd = std::min(y + height, Video::SCREEN_HEIGHT);

    // Fill the color
    uint32_t* pRow = Video::gpFrameBuffer + x + (y * Video::SCREEN_WIDTH);

    for (uint32_t yCur = y; yCur < yEnd; ++yCur) {
        uint32_t* pPixel = pRow;

        for (uint32_t xCur = x; xCur < xEnd; ++xCur) {
            *pPixel = color32;
            ++pPixel;
        }

        pRow += Video::SCREEN_WIDTH;
    }
}
