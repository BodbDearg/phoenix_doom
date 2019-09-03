#include "Intermission_Main.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Game/Controls.h"
#include "Game/Data.h"
#include "Game/DoomDefines.h"
#include "Game/DoomRez.h"
#include "Game/Resources.h"
#include "GFX/CelImages.h"
#include "GFX/Video.h"
#include <cstring>

static constexpr int32_t KVALX      = 232;
static constexpr int32_t KVALY      = 70;
static constexpr int32_t IVALX      = 232;
static constexpr int32_t IVALY      = 100;
static constexpr int32_t SVALX      = 231;
static constexpr int32_t SVALY      = 130;
static constexpr int32_t INTERTIME  = TICKSPERSEC / 30;

// Intermission shape group
enum {
    KillShape,
    ItemsShape,
    SecretsShape
};

// Here are the names of each level
static constexpr const char* const MAP_NAMES[] = {
    "Hangar",
    "Nuclear Plant",
    "Toxin Refinery",
    "Command Control",
    "Phobos Lab",
    "Central Processing",
    "Computer Station",
    "Phobos Anomaly",
    "Deimos Anomaly",
    "Containment Area",
    "Refinery",
    "Deimos Lab",
    "Command Center",
    "Halls of the Damned",
    "Spawning Vats",
    "Tower of Babel",
    "Hell Keep",
    "Pandemonium",
    "House of Pain",
    "Unholy Cathedral",
    "Mt. Erebus",
    "Limbo",
    "Dis",
    "Military Base"
};

static constexpr const char* const FINISHED = "Finished";
static constexpr const char* const ENTERING = "Entering";

static uint32_t gKillPercent;        // Percent to attain
static uint32_t gItemPercent;
static uint32_t gSecretPercent;
static uint32_t gKillValue;         // Displayed percent value
static uint32_t gItemValue;
static uint32_t gSecretValue;
static uint32_t gINDelay;            // Delay before next inc
static uint32_t gBangCount;          // Delay for gunshot sound

//----------------------------------------------------------------------------------------------------------------------
// Print a string using the large font.
// I only load the large ASCII font if it is needed.
//----------------------------------------------------------------------------------------------------------------------
void PrintBigFont(const int32_t x, const int32_t y, const char* const pString) noexcept {
    // Get the first char and abort if we are already at the end of the string
    char c = pString[0];

    if (c == 0) {
        return;
    }

    // Loop through the string
    const CelImageArray* gpUCharx = nullptr;     // Assume ASCII font is NOT loaded
    const char* pCurChar = pString;
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
        Renderer::drawUISprite(curX, y2, shape);            // Draw the char
        curX += shape.width + 1;                            // Get the width to tab

    } while ((c = pCurChar[0]) != 0);   // Next index

    if (gpUCharx) {                             // Did I load the ASCII font?
        CelImages::releaseImages(rCHARSET);     // Release the ASCII font
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Return the width of an ASCII string in pixels using the large font.
//----------------------------------------------------------------------------------------------------------------------
uint32_t GetBigStringWidth(const char* const pString) noexcept {
    // Get the first char and abort if we are already at the end of the string
    char c = pString[0];

    if (c == 0) {
        return 0;
    }
    
    // Loop through the string
    const CelImageArray* gpUCharx = nullptr;    // Only load in the ASCII set if I really need it
    uint32_t width = 0;
    const char* pCurChar = pString;

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
void PrintNumber(const int32_t x, const int32_t y, const uint32_t value, const uint32_t flags) noexcept {
    char buffer[40];
    std::snprintf(buffer, C_ARRAY_SIZE(buffer), "%u", value);

    // Append a percent sign?
    if ((flags & PNFLAGS_PERCENT) != 0) {
        std::strcat(buffer, "%");
    }

    // Center it?
    if ((flags & PNFLAGS_CENTER) != 0) {
        PrintBigFontCenter(x, y, buffer);
        return;
    }

    // Handle right justification and print
    int32_t displayXPos = x;

    if (flags & PNFLAGS_RIGHT) { 
        displayXPos -= GetBigStringWidth(buffer);
    }

    PrintBigFont(displayXPos, y, buffer);
}

//----------------------------------------------------------------------------------------------------------------------
// Print an ascii string centered on the x coord
//----------------------------------------------------------------------------------------------------------------------
void PrintBigFontCenter(const int32_t x, const int32_t y, const char* const str) noexcept {
    const int32_t w = (GetBigStringWidth(str) / 2);
    PrintBigFont(x - w, y, str);
}

//----------------------------------------------------------------------------------------------------------------------
// Init the intermission data
//----------------------------------------------------------------------------------------------------------------------
void IN_Start() noexcept {
    gINDelay = 0;
    gBangCount = 0;
    gKillValue = gItemValue = gSecretValue = 0;             // All values shown are zero
    gKillPercent = gItemPercent = gSecretPercent = 100;     // Init in case of divide by zero

    if (gTotalKillsInLevel > 0) {
        gKillPercent = (gPlayer.killcount * 100) / gTotalKillsInLevel;
    }

    if (gItemsFoundInLevel > 0) {
        gItemPercent = (gPlayer.itemcount * 100) / gItemsFoundInLevel;
    }

    if (gSecretsFoundInLevel > 0) {
        gSecretPercent = (gPlayer.secretcount * 100) / gSecretsFoundInLevel;
    }

    S_StartSong(Song_intermission);     // Begin the music
}

//----------------------------------------------------------------------------------------------------------------------
// Exit the intermission
//----------------------------------------------------------------------------------------------------------------------
void IN_Stop() noexcept {
    S_StopSong();   // Kill the music
}

//----------------------------------------------------------------------------------------------------------------------
// Exit the intermission
//----------------------------------------------------------------------------------------------------------------------
gameaction_e IN_Ticker() noexcept {    
    if (gTotalGameTicks < TICKSPERSEC / 2) {    // Initial wait before I begin
        return ga_nothing;                      // Do nothing
    }

    // Pressed a button?
    if (MENU_ACTION_ENDED(OK) || MENU_ACTION_ENDED(BACK)) {
        gKillValue = gKillPercent;          // Set to maximums
        gItemValue = gItemPercent;
        gSecretValue = gSecretPercent;
        return ga_died;                     // Exit after drawing
    }

    ++gINDelay;
    
    if (gINDelay >= INTERTIME) {
        bool bBang = false;
        gINDelay -= INTERTIME;

        if (gKillValue < gKillPercent) {    // Is it too high?
            ++gKillValue;
            bBang = true;
        }

        if (gItemValue < gItemPercent) {
            ++gItemValue;
            bBang = true;
        }

        if (gSecretValue < gSecretPercent) {
            ++gSecretValue;
            bBang = true;
        }

        if (bBang) {
            ++gBangCount;
            if (gBangCount % 4 == 0) {
                S_StartSound(0, sfx_pistol);
            }
        }
    }

    return ga_nothing;  // Still here!
}

//----------------------------------------------------------------------------------------------------------------------
// Draw the intermission screen
//----------------------------------------------------------------------------------------------------------------------
void IN_Drawer(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    Video::debugClearScreen();
    Renderer::drawUISprite(0, 0, rBACKGRNDBROWN);   // Load and draw the skulls

    const CelImageArray& intermisShapes = CelImages::loadImages(rINTERMIS, CelLoadFlagBits::MASKED);

    PrintBigFontCenter(160, 10, MAP_NAMES[gGameMap - 1]);   // Print the current map name
    PrintBigFontCenter(160, 34, FINISHED);                  // Print "Finished"

    if (gNextMap != 23) {
        PrintBigFontCenter(160, 162, ENTERING);
        PrintBigFontCenter(160, 182, MAP_NAMES[gNextMap - 1]);
    }

    Renderer::drawUISprite(71, KVALY, intermisShapes.getImage(KillShape));   // Draw the shapes
    Renderer::drawUISprite(65, IVALY, intermisShapes.getImage(ItemsShape));
    Renderer::drawUISprite(27, SVALY, intermisShapes.getImage(SecretsShape));

    PrintNumber(KVALX, KVALY, gKillValue, PNFLAGS_PERCENT|PNFLAGS_RIGHT);   // Print the numbers
    PrintNumber(IVALX, IVALY, gItemValue, PNFLAGS_PERCENT|PNFLAGS_RIGHT);
    PrintNumber(SVALX, SVALY, gSecretValue, PNFLAGS_PERCENT|PNFLAGS_RIGHT);

    CelImages::releaseImages(rINTERMIS);
    Video::endFrame(bPresent, bSaveFrameBuffer);
}
