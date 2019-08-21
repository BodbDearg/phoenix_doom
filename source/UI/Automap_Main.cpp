#include "Automap_Main.h"

#include "Base/Input.h"
#include "Base/Tables.h"
#include "Burger.h"
#include "Game/Data.h"
#include "Game/Tick.h"
#include "GFX/Blit.h"
#include "GFX/Video.h"
#include "Map/MapData.h"
#include "Things/MapObj.h"
#include "ThreeDO.h"
#include <cstring>

static constexpr Fixed STEPVALUE    = (2 << FRACBITS);      // Speed to move around in the map (Fixed) For non-follow mode
static constexpr Fixed MAXSCALES    = 0x10000;              // Maximum scale factor (Largest)
static constexpr Fixed MINSCALES    = 0x00800;              // Minimum scale factor (Smallest)
static constexpr Fixed ZOOMIN       = 66816;                // Fixed 1.02 * FRACUNIT
static constexpr Fixed ZOOMOUT      = 64222;                // Fixed (1/1.02) * FRACUNIT

// RGBA5551 colors
static constexpr uint16_t COLOR_BLACK       = 0x0001u;
static constexpr uint16_t COLOR_BROWN       = 0x4102u;
static constexpr uint16_t COLOR_BLUE        = 0x001Eu;
static constexpr uint16_t COLOR_RED         = 0x6800u;
static constexpr uint16_t COLOR_YELLOW      = 0x7BC0u;
static constexpr uint16_t COLOR_GREEN       = 0x0380u;
static constexpr uint16_t COLOR_LILAC       = 0x6A9Eu;
static constexpr uint16_t COLOR_LIGHTGREY   = 0x6318u;

// Used to restore the view if the screen goes blank
static Fixed        gOldPlayerX;        // X coord of the player previously
static Fixed        gOldPlayerY;        // Y coord of the player previously
static Fixed        gOldScale;          // Previous scale value

static Fixed        gMapScale;          // Scaling constant for the automap
static uint32_t     gTrueOldButtons;    // Previous buttons for joypad downs
static bool         gFollowMode;        // Follow mode active if true
static bool         gShowAllThings;     // If true, show all objects
static bool         gShowAllLines;      // If true, show all lines

static constexpr Fixed NOSELENGTH = 0x200000;   // Player's triangle
static constexpr Fixed MOBJLENGTH = 0x100000;   // Object's triangle

typedef enum {      // Cheat enum
    ch_allmap,      // Show the map
    ch_things,      // Show the items
    ch_godmode,
    ch_idkfa,
    ch_levelaccess,
    ch_noclip,
    ch_maxcheats
} cheat_e;

// order should mirror cheat_e
static char CheatStrings[ch_maxcheats][10] = {
    {"SEEALLUAC"},      // Allmap cheat
    {"SEERUBBLE"},      // Things cheat
    {"URABADASS"},      // God mode
    {"ALABARACA"},      // All weapons
    {"SUCCEDALL"},      // Level access
};

#define CHEATLETTERS 9

// Masks for the joypad
static uint32_t codes[CHEATLETTERS] = {
    PadA,
    PadB,
    PadC,
    PadUp,
    PadDown,
    PadLeft,
    PadRight,
    PadLeftShift,
    PadRightShift
};

static char CheatLetter[CHEATLETTERS + 1] = { "ABCUDLRSE" };
static char CurrentCheat[9];    // Current cheat string

//--------------------------------------------------------------------------------------------------
// Multiply a map coord and a fixed point scale value and return the INTEGER result.
// I assume that the scale cannot exceed 1.0 and the map coord has no fractional part.
// This way I can use a 16 by 16 mul with 32 bit result (Single mul) to speed up the code.
//--------------------------------------------------------------------------------------------------
static inline int32_t MulByMapScale(const Fixed mapCoord) noexcept {
    return ((mapCoord >> FRACBITS) * gMapScale) >> FRACBITS;
}

//--------------------------------------------------------------------------------------------------
// Multiply two fixed point numbers but return an INTEGER result!
//--------------------------------------------------------------------------------------------------
static inline int IMFixMulGetInt(const Fixed a, const Fixed b) noexcept {
    return fixedMul(a,b) >> FRACBITS;
}

//--------------------------------------------------------------------------------------------------
// Init all the variables for the automap system Called during P_Start when the game is initally loaded.
// If I need any permanent art, load it now.
//--------------------------------------------------------------------------------------------------
void AM_Start() noexcept {
    gMapScale = (FRACUNIT / 16);            // Default map scale factor (0.00625)
    gShowAllThings = false;                 // Turn off the cheat
    gShowAllLines = false;                  // Turn off the cheat
    gFollowMode = true;                     // Follow the player
    gPlayer.AutomapFlags &= ~AF_ACTIVE;     // Automap off
    gTrueOldButtons = gJoyPadButtons;       // Get the current state

    memset((char*) CurrentCheat, 0, sizeof(CurrentCheat));
}

//--------------------------------------------------------------------------------------------------
// Check for cheat codes for automap fun stuff!
// Pass to me all the new joypad downs.
//--------------------------------------------------------------------------------------------------
static cheat_e AM_CheckCheat(uint32_t NewButtons) noexcept {
    // FIXME: DC - TEMP
    cheat_e cheat = ch_maxcheats;

    if (Input::isKeyJustPressed(SDL_SCANCODE_F1)) {
        cheat = cheat_e::ch_godmode;
    }
    else if (Input::isKeyJustPressed(SDL_SCANCODE_F2)) {
        cheat = cheat_e::ch_idkfa;
    }
    else if (Input::isKeyJustPressed(SDL_SCANCODE_F3)) {
        cheat = cheat_e::ch_noclip;
    }

    return cheat;

    // FIMXE: DC: Replace/remove
    #if 0
    Word c;
    char *SourcePtr;
    // Convert the button press to a cheat char
    c = 0;
    do {
        if (NewButtons & codes[c]) {        // Key press?

            // Shift the entire string over 1 char
            {
                char*EndPtr;
                SourcePtr = CurrentCheat;
                EndPtr = SourcePtr+8;
                do {
                    SourcePtr[0] = SourcePtr[1];
                    ++SourcePtr;
                } while (SourcePtr!=EndPtr);
                EndPtr[0] = CheatLetter[c];     // Set the new end char
            }
            // Does the string match a cheat sequence?
            {
                Word i;
                i = 0;
                SourcePtr = &CheatStrings[0][0];    // Main string pointer
                do {
                    if (!memcmp((char *)CurrentCheat,(char *)SourcePtr,9)) {    // Match the strings
                        memset((char *)CurrentCheat,0,sizeof(CurrentCheat));    // Zap the string
                        S_StartSound(0,sfx_rxplod);
                        return (cheat_e)i;      // I got a CHEAT!!
                    }
                    SourcePtr+=9;
                } while (++i<ch_maxcheats - 1);
            }
        }
    } while (++c<CHEATLETTERS);     // All scanned?
    return (cheat_e)-1;     // No cheat found this time
    #endif
}

//--------------------------------------------------------------------------------------------------
// Draw a pixel on the screen but clip it to the visible area.
// I assume a coordinate system where 0,0 is at the upper left corner of the screen.
//--------------------------------------------------------------------------------------------------
static void ClipPixel(const uint32_t x, const uint32_t y, const uint16_t color) noexcept {
    if (x < 320 && y < 160) {   // On the screen?
        const uint32_t color32 = Video::rgba5551ToScreenCol(color);
        Video::gpFrameBuffer[y * Video::SCREEN_WIDTH + x] = color32;
    }
}

//--------------------------------------------------------------------------------------------------
// Draw a line in the automap, I use a classic Bresanhem line algorithm and call a routine to clip
// to the visible bounds. All x and y's assume a coordinate system where 0,0 is the CENTER of the
// visible screen!
//
// Even though all the variables are cast as unsigned, they are truly signed. I do this to test
// for out of bounds with one compare (x>=0 && x<320) is now just (x<320). Neat eh?
//--------------------------------------------------------------------------------------------------
static void DrawLine(
    const uint32_t x1,
    const uint32_t y1,
    const uint32_t x2,
    const uint32_t y2,
    const uint16_t color
) noexcept {
    // Coord adjustment:
    // (1) Move the x coords to the CENTER of the screen
    // (2) Vertically flip and then CENTER the y
    int32_t x = x1 + 160;
    const int32_t xEnd = x2 + 160;
    int32_t y = 80 - y1;
    const int32_t yEnd = 80 - y2;

    // Draw the initial pixel
    ClipPixel(x, y, color);

    // Get the distance to travel
    uint32_t deltaX = xEnd - x;
    uint32_t deltaY = yEnd - y;

    if (deltaX == 0 && deltaY == 0) {   // No line here?
        return;                         // Exit now
    }

    int32_t xStep = 1;          // Assume positive step
    int32_t yStep = 1;

    if (deltaX & 0x80000000) {                      // Going left?
        deltaX = (uint32_t) -(int32_t) deltaX;      // Make positive
        xStep = -1;                                 // Step to the left
    }

    if (deltaY & 0x80000000) {                      // Going up?
        deltaY = (uint32_t) -(int32_t) deltaY;      // Make positive
        yStep = -1;                                 // Step upwards
    }

    uint32_t delta = 0;     // Init the fractional step

    if (deltaX < deltaY) {              // Is the Y larger? (Step in Y)
        do {                            // Yes, make the Y step every pixel
            y += yStep;                 // Step the Y pixel
            delta += deltaX;            // Add the X fraction
            if (delta >= deltaY) {      // Time to step the X?
                x += xStep;             // Step the X
                delta -= deltaY;        // Adjust the fraction
            }
            ClipPixel(x, y, color);     // Draw a pixel
        } while (y != yEnd);            // At the bottom?
        return;                         // Exit
    }

    do {                            // Nope, make the X step every pixel
        x += xStep;                 // Step the X coord
        delta += deltaY;            // Adjust the Y fraction
        if (delta >= deltaX) {      // Time to step Y?
            y += yStep;             // Step the Y coord
            delta -= deltaX;        // Adjust the fraction
        }
        ClipPixel(x, y, color);     // Draw a pixel
    } while (x != xEnd);            // At the bottom?
}

//--------------------------------------------------------------------------------------------------
// Called by P_PlayerThink before any other player processing.
// Button bits can be eaten by clearing them in JoyPadButtons.
//
// Since I am making joypad events disappear and I want to track joypad downs, I need to cache
// the UNFILTERED JoyPadButtons and pass through a filtered NewPadButtons and JoyPadButtons.
//--------------------------------------------------------------------------------------------------
void AM_Control(player_t& player) noexcept {
    uint32_t buttons = gJoyPadButtons;                              // Get the joypad
    uint32_t NewButtons = (gTrueOldButtons ^ buttons) & buttons;    // Get the UNFILTERED joypad
    gTrueOldButtons = buttons;                                      // Set the previous joypad UNFILTERED

    if (NewButtons & (gPadUse|PadStart)) {                              // Shift down event?
        if ((buttons & (gPadUse|PadStart)) == (gPadUse|PadStart)) {
            if (!(player.AutomapFlags & AF_OPTIONSACTIVE)) {            // Can't toggle in option mode!
                player.AutomapFlags ^= AF_ACTIVE;                       // Swap the automap state if both held
            }
        }
    }

    if (!(player.AutomapFlags & AF_ACTIVE)) {   // Is the automap is off?
        return;                                 // Exit now!
    }

    buttons &= ~PadX;   // Don't allow option screen to come up

    // Check cheat events
    switch (AM_CheckCheat(NewButtons)) {
        case ch_allmap:
            gShowAllLines ^= true;      // Toggle lines
            break;

        case ch_things:
            gShowAllThings ^= true;     // Toggle things
            break;

        case ch_godmode:
            player.health = 100;
            player.mo->MObjHealth = 100;
            player.AutomapFlags ^= AF_GODMODE;      // Toggle god mode
            break;

        case ch_noclip:
            // DC: reimplementing noclip cheat
            player.mo->flags ^= MF_NOCLIP;
            player.mo->flags ^= MF_SOLID;
            player.AutomapFlags ^= AF_NOCLIP;
            break;

        case ch_idkfa: {
            uint32_t i = 0;
            uint32_t j = true;

            do {
                if (i == 3) {               // 0-2 are keys, 3-5 are skulls
                    j = false;
                }
                player.cards[i] = j;        // Award all keycards
            } while (++i < NUMCARDS);

            player.armorpoints = 200;       // Full armor
            player.armortype = 2;           // Mega armor
            i = 0;

            do {
                player.weaponowned[i] = true;   // Give all weapons
            } while (++i<NUMWEAPONS);

            i = 0;

            do {
                player.ammo[i] = player.maxammo[i] = 500;   // Full ammo!
            } while (++i<NUMAMMO);
        }   break;

        case ch_levelaccess:
            gMaxLevel = 23;
            WritePrefsFile();
            break;

        case ch_maxcheats:
            break;
    }

    if (gFollowMode) {                          // Test here to make SURE I get called at least once
        mobj_t* const pMapObj = player.mo;
        player.automapx = pMapObj->x;           // Mark the automap position
        player.automapy = pMapObj->y;
    }

    gOldPlayerX = player.automapx;      // Mark the previous position
    gOldPlayerY = player.automapy;
    gOldScale = gMapScale;               // And scale value

    if (NewButtons & PadX) {    // Enable/disable follow mode
        gFollowMode ^= true;    // Toggle the mode
    }

    // If follow mode if off, then I intercept the joypad motion to move the map anywhere on the screen.
    if (!gFollowMode) {
        Fixed step = STEPVALUE;                 // Multiplier for joypad motion: mul by integer
        step = fixedDiv(step, gMapScale);       // Adjust for scale factor

        if (buttons & PadRight) {
            player.automapx += step;    // Step to the right
        }

        if (buttons & PadLeft) {
            player.automapx -= step;    // Step to the left
        }

        if (buttons & PadUp) {
            player.automapy += step;    // Step up
        }

        if (buttons & PadDown) {
            player.automapy -= step;    // Step down
        }

        // Scaling is tricky, I cannot use a simple multiply to adjust the scale factor to the
        // timebase since the formula is ZOOM to the ElapsedTime power times scale.
        if (buttons & PadRightShift) {
            NewButtons = 0;                                     // Init the count
            do {
                gMapScale = fixedMul(gMapScale, ZOOMOUT);       // Perform the scale
                if (gMapScale < MINSCALES) {                    // Too small?
                    gMapScale = MINSCALES;                      // Set to smallest allowable
                    break;                                      // Leave now!
                }
            } while (++NewButtons < 1);                         // All done?
        }

        if (buttons & PadLeftShift) {
            NewButtons = 0;         // Init the count
            do {
                gMapScale = fixedMul(gMapScale, ZOOMIN);        // Perform the scale
                if (gMapScale >= MAXSCALES) {                   // Too large?
                    gMapScale = MAXSCALES;                      // Set to maximum
                    break;
                }
            } while (++NewButtons < 1);                         // All done?
        }

        // Eat the direction keys if not in follow mode
        buttons &= ~(PadUp|PadLeft|PadRight|PadDown|PadRightShift|PadLeftShift);
    }

    gJoyPadButtons = buttons;                                       // Save the filtered joypad value
    gNewJoyPadButtons = (gPrevJoyPadButtons ^ buttons) & buttons;   // Filter the joydowns
}

//--------------------------------------------------------------------------------------------------
// Draws the current frame to workingscreen
//--------------------------------------------------------------------------------------------------
void AM_Drawer() noexcept {
    // Clear the screen black
    Blit::blitRect(
        Video::gpFrameBuffer,
        Video::SCREEN_WIDTH,
        Video::SCREEN_HEIGHT,
        Video::SCREEN_WIDTH,
        0.0f,
        0.0f,
        320 * gScaleFactor,
        160 * gScaleFactor,
        0.0f,
        0.0f,
        0.0f,
        1.0f
    );

    player_t* const pPlayer = &gPlayer;     // Get pointer to the player
    Fixed ox = pPlayer->automapx;           // Get the x and y to draw from
    Fixed oy = pPlayer->automapy;
    line_t* pLine = gpLines;                // Init the list pointer to the line segment array
    uint32_t drawn = 0;                     // Init the count of drawn lines
    uint32_t i = gNumLines;                 // Init the line count

    do {
        if (gShowAllLines ||                    // Cheat?
            pPlayer->powers[pw_allmap] || (     // Automap enabled?
                (pLine->flags & ML_MAPPED) &&   // If not mapped or don't draw
                !(pLine->flags & ML_DONTDRAW)
            )
         ) {
            const int32_t x1 = MulByMapScale(pLine->v1.x - ox);       // Source line coords: scale it
            const int32_t y1 = MulByMapScale(pLine->v1.y - oy);
            const int32_t x2 = MulByMapScale(pLine->v2.x - ox);       // Dest line coords: scale it
            const int32_t y2 = MulByMapScale(pLine->v2.y - oy);

            // Is the line clipped?
            if ((y1 > 80 && y2 > 80) ||
                (y1 < -80 && y2 < -80) ||
                (x1 > 160 && x2 > 160) ||
                (x1 < -160 && x2 < -160)
            ) {
                ++pLine;
                continue;
            }

            // Figure out color
            uint16_t color;

            if ((gShowAllLines ||
                pPlayer->powers[pw_allmap]) &&      // If compmap && !Mapped yet
                (!(pLine->flags & ML_MAPPED))
            ) {
                color = COLOR_LIGHTGREY;
            } else if (!(pLine->flags & ML_TWOSIDED)) {     // One-sided line
                color = COLOR_RED;
            } else if (pLine->special == 97 || pLine->special == 39) {  // Teleport line
                color = COLOR_GREEN;
            } else if (pLine->flags & ML_SECRET) {      // Secret room?
                color = COLOR_RED;
            } else if (pLine->special) {    // Special line
                color = COLOR_BLUE;
            } else if (pLine->frontsector->floorheight != pLine->backsector->floorheight) {     // Floor height change
                color = COLOR_YELLOW;
            } else {
                color = COLOR_BROWN;
            }

            DrawLine(x1, y1, x2, y2, color);    // Draw the line
            ++drawn;                            // A line is drawn
        }
        ++pLine;
    } while (--i);

    // Draw the position of the player
    {
        // Get the size of the triangle into a cached local
        const Fixed noseScale = fixedMul(NOSELENGTH, gMapScale);
        mobj_t* const pMapObj = gPlayer.mo;

        int32_t x1 = MulByMapScale(pMapObj->x - ox);            // Get the screen
        int32_t y1 = MulByMapScale(pMapObj->y - oy);            // coords
        angle_t angle = pMapObj->angle >> ANGLETOFINESHIFT;     // Get angle of view

        const int32_t nx3 = IMFixMulGetInt(gFineCosine[angle], noseScale) + x1;     // Other points for the triangle
        const int32_t ny3 = IMFixMulGetInt(gFineSine[angle], noseScale) + y1;

        angle = (angle - ((ANG90 + ANG45) >> ANGLETOFINESHIFT)) & FINEMASK;
        const int32_t x2 = IMFixMulGetInt(gFineCosine[angle], noseScale) + x1;
        const int32_t y2 = IMFixMulGetInt(gFineSine[angle], noseScale) + y1;

        angle = (angle + (((ANG90 + ANG45) >> ANGLETOFINESHIFT) * 2)) & FINEMASK;
        x1 = IMFixMulGetInt(gFineCosine[angle], noseScale)+x1;
        y1 = IMFixMulGetInt(gFineSine[angle], noseScale)+y1;

        DrawLine(x1, y1, x2, y2, COLOR_GREEN);      // Draw the triangle
        DrawLine(x2, y2, nx3, ny3, COLOR_GREEN);
        DrawLine(nx3, ny3, x1, y1, COLOR_GREEN);
    }

    // Show all map things (cheat)
    if (gShowAllThings) {
        const int32_t objScale = MulByMapScale(MOBJLENGTH);   // Get the triangle size
        mobj_t* pMapObj = gMObjHead.next;
        const mobj_t* const pPlayerMapObj = pPlayer->mo;

        while (pMapObj != &gMObjHead) {         // Wrapped around?
            if (pMapObj != pPlayerMapObj) {     // Not the player?
                const int32_t x1 = MulByMapScale(pMapObj->x-ox);
                int32_t y1 = MulByMapScale(pMapObj->y-oy);

                const int32_t x2 = x1 - objScale;       // Create the triangle
                const int32_t y2 = y1 + objScale;
                const int32_t nx3 = x1 + objScale;
                y1 = y1 - objScale;

                DrawLine(x1, y1, x2, y2, COLOR_LILAC);      // Draw the triangle
                DrawLine(x2, y2, nx3, y2, COLOR_LILAC);
                DrawLine(nx3, y2, x1, y1, COLOR_LILAC);
            }

            pMapObj = pMapObj->next;    // Next item?
        }
    }

    // If less than 5 lines drawn, move to last position!
    if (drawn < 5) {
        pPlayer->automapx = gOldPlayerX;      // Restore the x,y
        pPlayer->automapy = gOldPlayerY;
        gMapScale = gOldScale;          // Restore scale factor as well...
    }
}
