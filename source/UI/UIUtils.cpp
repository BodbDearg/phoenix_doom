#include "UIUtils.h"

#include "Base/Tables.h"
#include "Game/Data.h"
#include "Game/DoomRez.h"
#include "GFX/Blit.h"
#include "GFX/CelImages.h"
#include "GFX/Video.h"
#include <cstring>
#include <string>

BEGIN_NAMESPACE(UIUtils)

//----------------------------------------------------------------------------------------------------------------------
// Print a string using the large font.
// I only load the large ASCII font if it is needed.
//----------------------------------------------------------------------------------------------------------------------
void printBigFont(const int32_t x, const int32_t y, const char* const pStr) noexcept {
    // Get the first char and abort if we are already at the end of the string
    char c = pStr[0];

    if (c == 0) {
        return;
    }

    // Loop through the string
    const CelImageArray* gpUCharx = nullptr;     // Assume ASCII font is NOT loaded
    const char* pCurChar = pStr;
    int32_t curX = x;

    do {
        ++pCurChar;                                         // Place here so "continue" will work
        int32_t y2 = y;                                     // Assume normal y coord
        const CelImageArray* gpCurrent = gpBigNumFont;      // Assume numeric font

        if (c >= '0' && c <= '9') {
            c -= '0';
        } else if (c == '%') {  // Percent
            c = 10;
        } else if (c == '-') {  // Minus
            c = 11;
        } else {
            gpCurrent = gpUCharx;                   // Assume I use the ASCII set

            if (c >= 'A' && c <= 'Z') {             // Upper case?
                c -= 'A';
            } else if (c >= 'a' && c <= 'z') {
                c -= 'a' - 26;                      // Index to lower case text
                y2 += 3;
            } else if (c == '.') {                  // Period
                c = 52;
                y2 += 3;
            } else if (c == '!') {                  // Exclaimation point
                c = 53;
                y2 += 3;
            } else {                                // Hmmm, not supported!
                curX += 6;                          // Fake space
                continue;
            }
        }

        // Do I need the ASCII set? Make sure I have the text font.
        if (!gpCurrent) {
            gpUCharx = &CelImages::loadImages(rCHARSET, CelLoadFlagBits::MASKED);
            gpCurrent = gpUCharx;
        }

        const CelImage& shape = gpCurrent->getImage(c);     // Get the shape pointer
        UIUtils::drawUISprite(curX, y2, shape);             // Draw the char
        curX += shape.width + 1;                            // Get the width to tab

    } while ((c = pCurChar[0]) != 0);   // Next index

    if (gpUCharx) {                             // Did I load the ASCII font?
        CelImages::releaseImages(rCHARSET);     // Release the ASCII font
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Return the width of an ASCII string in pixels using the large font.
//----------------------------------------------------------------------------------------------------------------------
uint32_t getBigStringWidth(const char* const pStr) noexcept {
    // Get the first char and abort if we are already at the end of the string
    char c = pStr[0];

    if (c == 0) {
        return 0;
    }
    
    // Loop through the string
    const CelImageArray* gpUCharx = nullptr;    // Only load in the ASCII set if I really need it
    uint32_t width = 0;
    const char* pCurChar = pStr;

    do {
        ++pCurChar;
        const CelImageArray* gpCurrent = gpBigNumFont;   // Assume numeric font

        if (c >= '0' && c <='9') {
            c -= '0';
        } else if (c == '%') {  // Percent
            c = 10;
        } else if (c == '-') {  // Minus
            c = 11;
        } else {
            gpCurrent = gpUCharx;                   // Assume I use the ASCII set

            if (c >= 'A' && c <= 'Z') {             // Upper case?
                c -= 'A';
            } else if (c >= 'a' && c <= 'z') {
                c -= 'a' - 26;                      // Index to lower case text
            } else if (c == '.') {                  // Period
                c = 52;
            } else if (c == '!') {                  // Exclaimation point
                c = 53;
            } else {                                // Hmmm, not supported!
                width += 6;                         // Fake space
                continue;
            }
        }

        // Do I need the ASCII set? Make sure I have the text font.
        if (!gpCurrent) {
            gpUCharx = &CelImages::loadImages(rCHARSET, CelLoadFlagBits::MASKED);
            gpCurrent = gpUCharx;
        }

        const CelImage& shape = gpCurrent->getImage(c);     // Get the shape pointer
        width += shape.width + 1;                           // Get the width to tab

    } while ((c = (uint8_t) pCurChar[0]) != 0);   // Next index

    if (gpUCharx) {                             // Did I load the ASCII font?
        CelImages::releaseImages(rCHARSET);     // Release the ASCII font
    }

    return width;
}

//----------------------------------------------------------------------------------------------------------------------
// Draws a number, this number may be appended with a percent sign and or centered upon the x coord.
// I use flags PNPercent and PNCenter.
//----------------------------------------------------------------------------------------------------------------------
void printNumber(const int32_t x, const int32_t y, const uint32_t value, const uint32_t flags) noexcept {
    char buffer[40];
    std::snprintf(buffer, C_ARRAY_SIZE(buffer), "%u", value);

    // Append a percent sign?
    if ((flags & PNFLAGS_PERCENT) != 0) {
        std::strcat(buffer, "%");
    }

    // Center it?
    if ((flags & PNFLAGS_CENTER) != 0) {
        printBigFontCenter(x, y, buffer);
        return;
    }

    // Handle right justification and print
    int32_t displayXPos = x;

    if ((flags & PNFLAGS_RIGHT) != 0) { 
        displayXPos -= getBigStringWidth(buffer);
    }

    printBigFont(displayXPos, y, buffer);
}

//----------------------------------------------------------------------------------------------------------------------
// Print an ascii string centered on the x coord
//----------------------------------------------------------------------------------------------------------------------
void printBigFontCenter(const int32_t x, const int32_t y, const char* const pStr) noexcept {
    const int32_t w = getBigStringWidth(pStr) / 2;
    printBigFont(x - w, y, pStr);
}

void drawUISprite(const int32_t x, const int32_t y, const CelImage& image) noexcept {
    const float xScaled = (float) x * gScaleFactor;
    const float yScaled = (float) y * gScaleFactor;
    const float wScaled = (float) image.width * gScaleFactor;
    const float hScaled = (float) image.height * gScaleFactor;

    Blit::blitSprite<
        Blit::BCF_ALPHA_TEST |
        Blit::BCF_H_CLIP |
        Blit::BCF_V_CLIP
    >(
        image.pPixels,
        image.width,
        image.height,
        0.0f,
        0.0f,
        (float) image.width,
        (float) image.height,
        Video::gpFrameBuffer,
        Video::gScreenWidth,
        Video::gScreenHeight,
        Video::gScreenWidth,
        xScaled,
        yScaled,
        wScaled,
        hScaled
    );
}

void drawUISprite(const int32_t x, const int32_t y, const uint32_t resourceNum) noexcept {
    const CelImage& img = CelImages::loadImage(resourceNum, CelLoadFlagBits::NONE);
    drawUISprite(x, y, img);
    CelImages::releaseImages(resourceNum);
}

void drawMaskedUISprite(const int32_t x, const int32_t y, const uint32_t resourceNum) noexcept {
    const CelImage& img = CelImages::loadImage(resourceNum, CelLoadFlagBits::MASKED);
    drawUISprite(x, y, img);
    CelImages::releaseImages(resourceNum);
}

void drawPlaque(const uint32_t resourceNum) noexcept  {
    const CelImage& img = CelImages::loadImage(resourceNum);
    drawUISprite(160 - img.width / 2, 80, img);
    CelImages::releaseImages(resourceNum);
}

void drawPerformanceCounter(const int32_t x, const int32_t y) noexcept {
    if (gPerfCounterMode == PerfCounterMode::FPS) {
        if (gPerfCounterAverageUSec > 0) {
            const double frameTimeSeconds = gPerfCounterAverageUSec / 1000000.0;
            const double fps = 1.0f / frameTimeSeconds;
            std::string fpsString = std::to_string(fps) + std::string(" FPS");
            printBigFont(x, y, fpsString.c_str());
        }
    }
    else if (gPerfCounterMode == PerfCounterMode::USEC) {
        std::string usecString = std::to_string(gPerfCounterAverageUSec) + std::string(" USEC");
        printBigFont(x, y, usecString.c_str());
    }
}

END_NAMESPACE(UIUtils)
