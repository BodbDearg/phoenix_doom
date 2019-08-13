#include "StatusBar_Main.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Random.h"
#include "Burger.h"
#include "Game/Data.h"
#include "Game/DoomDefines.h"
#include "Game/DoomRez.h"
#include "Game/Resources.h"
#include "GFX/CelImages.h"
#include "Intermission_Main.h"
#include <cstring>

/**********************************

    Local enums and data

**********************************/

#define FLASHDELAY (TICKSPERSEC/4)  // # of tics delay (1/60 sec) 
#define FLASHTIMES 6    // # of times to flash new frag amount 

#define AMMOX 60        // (42 Center) X,Y coord of the ammo 
#define AMMOY 174

#define HEALTHX 122     // (93 Center) X,Y coord of health 
#define HEALTHY 174

#define KEYX 124        // X coord of the key cards 
#define REDKEYY 163     // Y coords of the key cards 
#define BLUKEYY 175
#define YELKEYY 187

#define FACEX 144       // X,Y of the face 
#define FACEY 165

#define ARMORX 233      // (204 Center) X,Y of the armor 
#define ARMORY 174

#define MAPX 290    // X,Y Area level 
#define MAPY 174

#define NUMMICROS 6         // Amount of micro-sized #'s (weapon armed) 
#define GODFACE 40          // God mode face 
#define DEADFACE 41         // Dead face 
#define FIRSTSPLAT 42       // First gibbed face 
#define GIBTIME (TICKSPERSEC/6)         // Time for gibbed animation 

// Shape group rSBARSHP
enum {
    sb_card_b,      // Blue card 
    sb_card_y,      // Yellow card 
    sb_card_r,      // Red card 
    sb_skul_b,      // Blue skull 
    sb_skul_y,      // Yellow skull 
    sb_skul_r,      // Red skull 
    sb_micro        // The 6 small weapons 
};

// Indexs for face array
enum {
    sbf_lookfwd,    // Look ahead 
    sbf_lookrgt,    // Look right 
    sbf_looklft,    // Look left 
    sbf_facelft,    // Facing left 
    sbf_facergt,    // Facing right 
    sbf_ouch,       // In pain 
    sbf_gotgat,     // Got some goodies 
    sbf_mowdown     // Mowing down opponents 
};

// Special face to shape # 
static constexpr uint32_t SPECIAL_FACE_SPRITE[NUMSPCLFACES] = {
    sbf_lookfwd,
    sbf_facelft,
    sbf_facergt,
    sbf_ouch,
    sbf_gotgat,
    sbf_mowdown
};

static constexpr uint32_t CARD_X[NUMCARDS] = { KEYX, KEYX, KEYX, KEYX + 3, KEYX + 3, KEYX + 3 };
static constexpr uint32_t CARD_Y[NUMCARDS] = { BLUKEYY, YELKEYY, REDKEYY, BLUKEYY, YELKEYY, REDKEYY };
static constexpr uint32_t MICRO_NUMS_X[NUMMICROS] = { 237, 249, 261, 237, 249, 261 };
static constexpr uint32_t MICRO_NUMS_Y[NUMMICROS] = { 175, 175, 175, 185, 185, 185 };

typedef struct {
    uint32_t    delay;     // Time delay 
    uint32_t    times;     // Number of times to flash 
    bool        doDraw;    // True if I draw now 
} sbflash_t;

stbar_t gStBar;      // Current state of the status bar 

static const CelImage*          gpStatusBarShape;           // Handle to current status bar shape 
static const CelImageArray*     gpSBObj;                    // Cached handle to the status bar sub shapes 
static const CelImageArray*     gpFaces;                    // Cached handle to the faces 
static uint32_t                 gFaceTics;                  // Time before animating the face 
static uint32_t                 gNewFace;                   // Which normal face to show
static sbflash_t                gFlashCards[NUMCARDS];      // Info for flashing cards & Skulls 
static bool                     gGibDraw;                   // Got gibbed? 
static uint32_t                 gGibFrame;                  // Which gib frame 
static uint32_t                 gGibDelay;                  // Delay for gibbing 

/**********************************

    Process the timer for a shape flash

**********************************/

static void CycleFlash(sbflash_t *FlashPtr)
{
    if (FlashPtr->delay) {              // Active? 
        if (FlashPtr->delay > 1) {      // Still time? 
            --FlashPtr->delay;          // Remove the time 
        } else {
            if (!--FlashPtr->times) {           // Can I still go? 
                FlashPtr->delay = 0;
                FlashPtr->doDraw = false;       // Force off 
            } else {
                FlashPtr->delay = FLASHDELAY;       // Reset the time 
                FlashPtr->doDraw ^= true;           // Toggle the draw flag 
                if (FlashPtr->doDraw) {             // If on, play sound 
                    S_StartSound(0,sfx_itemup);
                }
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Locate and load all needed graphics for the status bar.
//----------------------------------------------------------------------------------------------------------------------
void ST_Start() noexcept {
    gpSBObj = &CelImages::loadImages(rSBARSHP, CelImages::LoadFlagBits::MASKED);        // Status bar shapes
    gpFaces = &CelImages::loadImages(rFACES, CelImages::LoadFlagBits::MASKED);          // Load all the face frames    
    gpStatusBarShape = &CelImages::loadImage(rSTBAR);                                   // Load the status bar

    memset(&gStBar, 0, sizeof(gStBar));                 // Reset the status bar
    gFaceTics = 0;                                      // Reset the face tic count
    gGibDraw = false;                                   // Don't draw gibbed head sequence
    memset(&gFlashCards, 0, sizeof(gFlashCards));
}

//----------------------------------------------------------------------------------------------------------------------
// Release resources allocated by the status bar
//----------------------------------------------------------------------------------------------------------------------
void ST_Stop() noexcept {
    CelImages::releaseImages(rSBARSHP); 
    CelImages::releaseImages(rFACES);
    CelImages::releaseImages(rSTBAR);
}

/**********************************

    Called during the think logic phase to prepare
    to draw the status bar.

**********************************/

void ST_Ticker()
{
    uint32_t ind;
    sbflash_t *FlashPtr;

    // Animate face 
    --gFaceTics;                                // Count down 
    if (gFaceTics & 0x8000) {                   // Negative? 
        gFaceTics = Random::nextU32(15)*4;      // New random value 
        gNewFace = Random::nextU32(2);          // Which face 0-2 
    }

    // Draw special face?
    if (gStBar.specialFace) {
        gNewFace = SPECIAL_FACE_SPRITE[gStBar.specialFace];    // Get the face shape 
        gFaceTics = TICKSPERSEC;     // 1 second 
        gStBar.specialFace = f_none; // Ack the shape 
    }

    // Did we get gibbed?
    if (gStBar.gotgibbed && !gGibDraw) {  // In progress? 
        gGibDraw = true;     // In progress... 
        gGibFrame = 0;
        gGibDelay = GIBTIME;
        gStBar.gotgibbed = false;
    }

    // Tried to open a CARD or SKULL door?
    ind = 0;
    FlashPtr = gFlashCards;
    do {    // Check for initalization 
        if (gStBar.tryopen[ind]) {       // Did the user ask to flash the card? 
            gStBar.tryopen[ind] = false; // Ack the flag 
            FlashPtr->delay = FLASHDELAY;
            FlashPtr->times = FLASHTIMES+1;
            FlashPtr->doDraw = false;
        }
        CycleFlash(FlashPtr);       // Handle the ticker 
        ++FlashPtr;
    } while (++ind<NUMCARDS);       // All scanned? 
}

/**********************************

    Draw the status bar

**********************************/

#define VBLTIMER 0

#if VBLTIMER
#define TIMERSIZE 16
static Word Frames;
static Word CountVBL[TIMERSIZE];
#endif

void ST_Drawer()
{
    uint32_t i;
    uint32_t ind;
    player_t *p;
    sbflash_t *FlashPtr;

    DrawShape(0, 160, *gpStatusBarShape);   // Draw the status bar 

#if VBLTIMER
    ++Frames;
    if (Frames>=TIMERSIZE) {
        Frames=0;
    }
    CountVBL[Frames]=ElapsedTime;
    ind = 0;
    i = 0;
    do {
        ind+=CountVBL[i];
    } while (++i<TIMERSIZE);
    ind = (60*TIMERSIZE)/ind;

    PrintNumber(100,80,ind,0);
#endif
    
    p = &gPlayers;

    // Ammo 

    if (p->readyweapon != wp_nochange) {        // No weapon? 
        ind = gWeaponAmmos[p->readyweapon];
        if (ind != am_noammo) {     // No ammo needed? 
            i = p->ammo[ind];
            PrintNumber(AMMOX,AMMOY,i,PNFLAGS_RIGHT);     // Draw ammo 
        }
    }
    PrintNumber(HEALTHX,HEALTHY,p->health,PNFLAGS_RIGHT|PNFLAGS_PERCENT);   // Draw the health 
    PrintNumber(ARMORX,ARMORY,p->armorpoints,PNFLAGS_RIGHT|PNFLAGS_PERCENT);    // Draw armor 
    PrintNumber(MAPX,MAPY,gGameMap,PNFLAGS_CENTER);        // Draw AREA 

    // Cards & skulls
    ind = 0;
    FlashPtr = gFlashCards;
    do {
        if (p->cards[ind] || FlashPtr->doDraw) {    // Flashing? 
            DrawShape(CARD_X[ind], CARD_Y[ind], gpSBObj->getImage(sb_card_b + ind));
        }
        ++FlashPtr;
    } while (++ind<NUMCARDS);

    // Weapons
    ind = 0;
    do {
        if (p->weaponowned[ind+1]) {
            DrawShape(MICRO_NUMS_X[ind], MICRO_NUMS_Y[ind], gpSBObj->getImage(sb_micro+ind));
        }
    } while (++ind<NUMMICROS);
    
    // Face change
    if ((p->AutomapFlags & AF_GODMODE) || p->powers[pw_invulnerability]) {      // In god mode? 
        i = GODFACE;        // God mode 
        gGibFrame = 0;
    } else if (gGibDraw) {       // Got gibbed 
        if (gGibDelay > 0) {
            --gGibDelay;
        }
        else {
            ++gGibFrame;
            gGibDelay = GIBTIME;
            if (gGibFrame >= 7) {        // All frames shown? 
                gGibDraw = false;        // Shut it off 
            }
        }

        i = FIRSTSPLAT+gGibFrame;
    } else if (!p->health) {
        i = FIRSTSPLAT+gGibFrame;    // Dead man 
    } else {
        gGibFrame = 0;
        i = p->health;      // Get the health 
        if (i>=80) {    // Div by 20 without a real divide (Saves time) 
            i = 0;
        } else if (i>=60) {
            i = 8;
        } else if (i>=40) {
            i = 16;
        } else if (i>=20) {
            i = 24;
        } else {
            i = 32; // Bad shape... 
        }
        i = i+gNewFace;  // Get shape requested 
    }
    
    DrawShape(FACEX, FACEY, gpFaces->getImage(i));  // Dead man 
}
