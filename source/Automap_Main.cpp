#include "doom.h"
#include "MapData.h"
#include "Video.h"
#include "MathUtils.h"
#include <cstring>

// FIXME: TEMP!
#include <SDL.h>

#define STEPVALUE   (2 << FRACBITS)   // Speed to move around in the map (Fixed) For non-follow mode
#define MAXSCALES   0x10000           // Maximum scale factor (Largest)
#define MINSCALES   0x00800           // Minimum scale factor (Smallest)
#define ZOOMIN      66816L            // Fixed 1.02 * FRACUNIT
#define ZOOMOUT     64222L            // Fixed (1/1.02) * FRACUNIT

// Used to restore the view if the screen goes blank
static Fixed    OldPlayerX;         // X coord of the player previously
static Fixed    OldPlayerY;         // Y coord of the player previously
static Fixed    OldScale;           // Previous scale value

static Fixed    MapScale;           // Scaling constant for the automap
static Word     TrueOldButtons;     // Previous buttons for joypad downs
static bool     FollowMode;         // Follow mode active if true
static bool     ShowAllThings;      // If true, show all objects
static bool     ShowAllLines;       // If true, show all lines

#define NOSELENGTH 0x200000     // Player's triangle
#define MOBJLENGTH 0x100000     // Object's triangle

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
static Word codes[CHEATLETTERS] = {
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

//--------------------------------------------------------------------------------------------------// Multiply a map coord and a fixed point scale value and return the INTEGER result. 
// I assume that the scale cannot exceed 1.0 and the map coord has no fractional part.
// This way I can use a 16 by 16 mul with 32 bit result (Single mul) to speed up the code.
//--------------------------------------------------------------------------------------------------
static inline int32_t MulByMapScale(const Fixed mapCoord) noexcept {
    return ((mapCoord >> FRACBITS) * MapScale) >> FRACBITS;
}

//--------------------------------------------------------------------------------------------------
// Multiply two fixed point numbers but return an INTEGER result!
//--------------------------------------------------------------------------------------------------
static inline int IMFixMulGetInt(const Fixed a, const Fixed b) noexcept {
    return sfixedMul16_16(a,b) >> FRACBITS;
}

//--------------------------------------------------------------------------------------------------
// Init all the variables for the automap system Called during P_Start when the game is initally loaded.
// If I need any permanent art, load it now.
//--------------------------------------------------------------------------------------------------
void AM_Start() noexcept {
    MapScale = (FRACUNIT / 16);             // Default map scale factor (0.00625)
    ShowAllThings = false;                  // Turn off the cheat
    ShowAllLines = false;                   // Turn off the cheat
    FollowMode = true;                      // Follow the player
    players.AutomapFlags &= ~AF_ACTIVE;     // Automap off
    TrueOldButtons = JoyPadButtons;         // Get the current state

    memset((char*) CurrentCheat, 0, sizeof(CurrentCheat));
}

/**********************************

    Check for cheat codes for automap fun stuff!
    Pass to me all the new joypad downs.

**********************************/

static cheat_e AM_CheckCheat(Word NewButtons) noexcept {
    Word c;
    char *SourcePtr;

    // FIXME: temp cheats - DC
    SDL_PumpEvents();
    static Uint8 oldKBState[SDL_NUM_SCANCODES] = {};
    const Uint8* kbState = SDL_GetKeyboardState(NULL);
    #define WAS_JUST_RELEASED(Key) ((kbState[(Key)] == SDL_RELEASED) && (oldKBState[(Key)] == SDL_PRESSED))

    cheat_e cheat = ch_maxcheats;

    if (WAS_JUST_RELEASED(SDL_SCANCODE_F1)) {
        cheat = cheat_e::ch_godmode;
    }
    else if (WAS_JUST_RELEASED(SDL_SCANCODE_F2)) {
        cheat = cheat_e::ch_idkfa;
    }
    else if (WAS_JUST_RELEASED(SDL_SCANCODE_F3)) {
        cheat = cheat_e::ch_noclip;
    }

    std::memcpy(oldKBState, kbState, sizeof(Uint8) * SDL_NUM_SCANCODES);

    // FIMXE: DC: Replace/remove
    #if 0
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

    return cheat;
}

//--------------------------------------------------------------------------------------------------
// Draw a pixel on the screen but clip it to the visible area.
// I assume a coordinate system where 0,0 is at the upper left corner of the screen.
//--------------------------------------------------------------------------------------------------
static void ClipPixel(const uint32_t x, const uint32_t y, const uint16_t color) noexcept {
    if (x < 320 && y < 160) {   // On the screen?
        const uint32_t color32 = Video::rgba5551ToScreenCol(color);
        Video::gFrameBuffer[y * Video::SCREEN_WIDTH + x] = color32;
    }
}

/**********************************

    Draw a line in the automap, I use a classic Bresanhem line
    algorithm and call a routine to clip to the visible bounds.

    All x and y's assume a coordinate system where 0,0 is the CENTER
    of the visible screen!

    Even though all the variables are cast as unsigned, they
    are truly signed. I do this to test for out of bounds with one
    compare (x>=0 && x<320) is now just (x<320). Neat eh?

**********************************/

static void DrawLine(Word x1,Word y1,Word x2,Word y2,Word color) noexcept {
    Word DeltaX,DeltaY;     // X and Y motion
    Word Delta;             // Fractional step
    int XStep,YStep;        // Step to the left or right

    x1+=160;        // Move the x coords to the CENTER of the screen
    x2+=160;
    y1=80-y1;       // Vertically flip and then CENTER the y
    y2=80-y2;
    ClipPixel(x1,y1,color);     // Draw the initial pixel

    DeltaX = x2-x1;         // Get the distance to travel
    DeltaY = y2-y1;
    if (!DeltaX && !DeltaY) {       // No line here?
        return;             // Exit now
    }
    XStep = 1;              // Assume positive step
    YStep = 1;
    if (DeltaX&0x8000) {    // Going left?
        DeltaX = -DeltaX;   // Make positive
        XStep = -1;         // Step to the left
    }
    if (DeltaY&0x8000) {    // Going up?
        DeltaY = -DeltaY;   // Make positive
        YStep = -1;
    }
    Delta = 0;              // Init the fraction
    if (DeltaX<DeltaY) {    // Is the Y larger? (Step in Y)
        do {                // Yes, make the Y step every pixel
            y1+=YStep;      // Step the Y pixel
            Delta+=DeltaX;  // Add the X fraction
            if (Delta>=DeltaY) {    // Time to step the X?
                x1+=XStep;          // Step the X
                Delta-=DeltaY;      // Adjust the fraction
            }
            ClipPixel(x1,y1,color); // Draw a pixel
        } while (y1!=y2);           // At the bottom?
        return;             // Exit
    }
    do {            // Nope, make the X step every pixel
        x1+=XStep;      // Step the X coord
        Delta+=DeltaY;  // Adjust the Y fraction
        if (Delta>=DeltaX) {    // Time to step Y?
            y1+=YStep;      // Step the Y coord
            Delta-=DeltaX;  // Adjust the fraction
        }
        ClipPixel(x1,y1,color);     // Draw a pixel
    } while (x1!=x2);       // At the bottom?
}

/**********************************

    Called by P_PlayerThink before any other player processing

    Button bits can be eaten by clearing them in JoyPadButtons

    Since I am making joypad events disappear and I want to track 
    joypad downs, I need to cache the UNFILTERED JoyPadButtons and pass through
    a filtered NewPadButtons and JoyPadButtons 
    
**********************************/
void AM_Control(player_t *player) noexcept
{
    Word buttons;   // Buttons
    Word NewButtons;    // Button down events

    buttons = JoyPadButtons;        // Get the joypad
    NewButtons = (TrueOldButtons^buttons)&buttons;  // Get the UNFILTERED joypad
    TrueOldButtons = buttons;   // Set the previous joypad UNFILTERED

    if (NewButtons & (PadUse|PadStart)) {   // Shift down event?
        if ((buttons&((PadUse|PadStart)))==((PadUse|PadStart))) {
            if (!(player->AutomapFlags & AF_OPTIONSACTIVE)) {   // Can't toggle in option mode!
                player->AutomapFlags ^= AF_ACTIVE;  // Swap the automap state if both held
            }
        }
    }

    if ( !(player->AutomapFlags & AF_ACTIVE) )  {   // Is the automap is off?
        return;     // Exit now!
    }

    buttons &= ~PadX;           // Don't allow option screen to come up

    switch (AM_CheckCheat(NewButtons)) {        // Check cheat events
    case ch_allmap:
        ShowAllLines ^= true;   // Toggle lines
        break;
    case ch_things:
        ShowAllThings ^= true;  // Toggle things
        break;
    case ch_godmode:
        player->health = 100;
        player->mo->MObjHealth = 100;
        player->AutomapFlags ^= AF_GODMODE; // Toggle god mode
        break;
    case ch_noclip:
        // DC: reimplementing noclip cheat
        player->mo->flags ^= MF_NOCLIP;
        player->mo->flags ^= MF_SOLID;
        player->AutomapFlags ^= AF_NOCLIP; 
        break;
    case ch_idkfa:
        {
        Word i;
        Word j;
        i = 0;
        j = true;
        do {
            if (i==3) {     // 0-2 are keys, 3-5 are skulls
                j=false;
            }
            player->cards[i] = j;       // Award all keycards
        } while (++i<NUMCARDS);
        player->armorpoints = 200;      // Full armor
        player->armortype = 2;          // Mega armor
        i = 0;
        do {
            player->weaponowned[i] = true;  // Give all weapons
        } while (++i<NUMWEAPONS);
        i = 0;
        do {
            player->ammo[i] = player->maxammo[i] = 500; // Full ammo!
        } while (++i<NUMAMMO);
        }
        break;
    case ch_levelaccess:
        MaxLevel = 23;
        WritePrefsFile();
        break;
    case ch_maxcheats:
        break;
    }

    if (FollowMode) {       // Test here to make SURE I get called at least once
        mobj_t *mo;
        mo = player->mo;
        player->automapx = mo->x;   // Mark the automap position
        player->automapy = mo->y;
    }
    OldPlayerX = player->automapx;      // Mark the previous position
    OldPlayerY = player->automapy;
    OldScale = MapScale;                // And scale value

    if (NewButtons&PadX) {      // Enable/disable follow mode
        FollowMode^=true;       // Toggle the mode
    }

    // If follow mode if off, then I intercept the joypad motion to
    // move the map anywhere on the screen

    if (!FollowMode) {      // Not being followed?
        Fixed step;         // Multiplier for joypad motion
        step = STEPVALUE*gElapsedTime;           // Mul by integer
        step = sfixedDiv16_16(step, MapScale);  // Adjust for scale factor
        if (buttons & PadRight) {
            player->automapx+=step;     // Step to the right
        }
        if (buttons & PadLeft) {
            player->automapx-=step;     // Step to the left
        }
        if (buttons & PadUp) {
            player->automapy+=step;     // Step up
        }
        if (buttons & PadDown) {
            player->automapy-=step;     // Step down
        }

        // Scaling is tricky, I cannot use a simple multiply to adjust
        // the scale factor to the timebase since the formula is
        // ZOOM to the ElapsedTime power times scale.

        if (buttons & PadRightShift) {
            NewButtons = 0;         // Init the count
            do {
                MapScale=sfixedMul16_16(MapScale, ZOOMOUT);     // Perform the scale
                if (MapScale<MINSCALES) {       // Too small?
                    MapScale = MINSCALES;       // Set to smallest allowable
                    break;      // Leave now!
                }
            } while (++NewButtons<gElapsedTime);     // All done?
        }
        if (buttons & PadLeftShift) {
            NewButtons = 0;         // Init the count
            do {
                MapScale=sfixedMul16_16(MapScale, ZOOMIN);      // Perform the scale
                if (MapScale>=MAXSCALES) {          // Too large?
                    MapScale = MAXSCALES;           // Set to maximum
                    break;
                }
            } while (++NewButtons<gElapsedTime); // All done?
        }
        // Eat the direction keys if not in follow mode
        buttons &= ~(PadUp|PadLeft|PadRight|PadDown|PadRightShift|PadLeftShift);
    }
    JoyPadButtons = buttons;        // Save the filtered joypad value
    NewJoyPadButtons = (PrevJoyPadButtons^buttons)&buttons; // Filter the joydowns
}

/**********************************

    Draws the current frame to workingscreen

**********************************/
void AM_Drawer() noexcept {
    player_t *p;        // Pointer to current player record
    line_t *line;       // Pointer to the list of lines that make a map
    Word drawn;         // How many lines drawn?
    Word i;             // Temp
    Word color;         // Color to draw line in
    Fixed ox,oy;        // X and Y to draw the map from
    int x1,y1;          // Source line coords
    int x2,y2;          // Dest line coords

    DrawARect(0,0,320,160,BLACK);       // Blank out the screen

    p = &players;           // Get pointer to the player
    ox = p->automapx;       // Get the x and y to draw from
    oy = p->automapy;
    line = gpLines;         // Init the list pointer to the line segment array
    drawn = 0;              // Init the count of drawn lines
    i = gNumLines;          // Init the line count
    do {
        if (ShowAllLines ||                     // Cheat?
            p->powers[pw_allmap] ||             // Automap enabled?
            ((line->flags & ML_MAPPED) &&       // If not mapped or don't draw
            !(line->flags & ML_DONTDRAW)) )  {

            x1 = MulByMapScale(line->v1.x-ox);  // Scale it
            y1 = MulByMapScale(line->v1.y-oy);
            x2 = MulByMapScale(line->v2.x-ox);  // Scale it
            y2 = MulByMapScale(line->v2.y-oy);
            if ((y1>80 && y2>80) || (y1<-80 && y2<-80) ||
                (x1>160 && x2>160) || (x1<-160 && x2<-160)) {
                goto Skip;      // Is the line clipped?
            }

            // Figure out color
            if ((ShowAllLines ||
                p->powers[pw_allmap]) &&    // If compmap && !Mapped yet
                !(line->flags & ML_MAPPED)) {
                color = LIGHTGREY;
            } else if (!(line->flags & ML_TWOSIDED)) {  // One-sided line
                color = RED;
            } else if (line->special == 97 ||       // Teleport line
                line->special == 39) {
                color = GREEN;
            } else if (line->flags & ML_SECRET) {   // Secret room?
                color = RED;
            } else if (line->special) {
                color = BLUE;           // SPECIAL LINE
            } else if (line->frontsector->floorheight !=
                line->backsector->floorheight) {
                color = YELLOW;
            } else {
                color = BROWN;
            }
            DrawLine(x1,y1,x2,y2,color);        // Draw the line
            ++drawn;                            // A line is drawn
        }
Skip:
        ++line;
    } while (--i);

    // Draw the position of the player

    {
        Fixed NoseScale;        // Scale factor for triangle size
        mobj_t *mo;
        angle_t angle;          // Angle of view
        int nx3,ny3;            // Other points for the triangle
        
        // Get the size of the triangle into a cached local
        NoseScale = sfixedMul16_16(NOSELENGTH, MapScale);
        mo = players.mo;

        x1 = MulByMapScale(mo->x-ox);                           // Get the screen
        y1 = MulByMapScale(mo->y-oy);                           // coords
        angle = mo->angle>>ANGLETOFINESHIFT;                    // Get angle
        nx3 = IMFixMulGetInt(finecosine[angle],NoseScale)+x1;
        ny3 = IMFixMulGetInt(finesine[angle],NoseScale)+y1;

        angle = (angle-((ANG90+ANG45)>>ANGLETOFINESHIFT))&FINEMASK;
        x2 = IMFixMulGetInt(finecosine[angle],NoseScale)+x1;
        y2 = IMFixMulGetInt(finesine[angle],NoseScale)+y1;

        angle = (angle+(((ANG90+ANG45)>>ANGLETOFINESHIFT)*2))&FINEMASK;
        x1 = IMFixMulGetInt(finecosine[angle],NoseScale)+x1;
        y1 = IMFixMulGetInt(finesine[angle],NoseScale)+y1;

        DrawLine(x1,y1,x2,y2,GREEN);    // Draw the triangle
        DrawLine(x2,y2,nx3,ny3,GREEN);
        DrawLine(nx3,ny3,x1,y1,GREEN);  
    }

    // SHOW ALL MAP THINGS (CHEAT)
    if (ShowAllThings) {
        int ObjScale;
        mobj_t *mo;
        mobj_t *pmo;
        Fixed nx3;
        
        ObjScale = MulByMapScale(MOBJLENGTH);   // Get the triangle size
        mo = mobjhead.next;
        pmo = p->mo;
        while (mo != &mobjhead) {       // Wrapped around?
            if (mo != pmo) {            // Not the player?
                x1 = MulByMapScale(mo->x-ox);
                y1 = MulByMapScale(mo->y-oy);
                
                x2 = x1-ObjScale;       // Create the triangle
                y2 = y1+ObjScale;
                nx3 = x1+ObjScale;
                y1 = y1-ObjScale;

                DrawLine(x1,y1,x2,y2,LILAC);    // Draw the triangle
                DrawLine(x2,y2,nx3,y2,LILAC);
                DrawLine(nx3,y2,x1,y1,LILAC);
            }
            mo = mo->next;      // Next item?
        }
    }

    // If less than 5 lines drawn, move to last position!
    if (drawn < 5) {
        p->automapx = OldPlayerX;       // Restore the x,y
        p->automapy = OldPlayerY;
        MapScale = OldScale;            // Restore scale factor as well...
    }
}
