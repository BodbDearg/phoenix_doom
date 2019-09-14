#include "StatusBarUI.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Random.h"
#include "Game/Data.h"
#include "Game/DoomDefines.h"
#include "Game/DoomRez.h"
#include "GFX/CelImages.h"
#include "UIUtils.h"
#include <cstring>

static constexpr uint32_t FLASHDELAY = TICKSPERSEC / 4;     // # of tics delay (1/60 sec)
static constexpr uint32_t FLASHTIMES = 6;                   // # of times to flash new frag amount

static constexpr int32_t AMMOX    = 60;     // (42 Center) X,Y coord of the ammo
static constexpr int32_t AMMOY    = 174;
static constexpr int32_t HEALTHX  = 122;    // (93 Center) X,Y coord of health
static constexpr int32_t HEALTHY  = 174;
static constexpr int32_t KEYX     = 124;    // X coord of the key cards
static constexpr int32_t REDKEYY  = 163;    // Y coords of the key cards
static constexpr int32_t BLUKEYY  = 175;
static constexpr int32_t YELKEYY  = 187;
static constexpr int32_t FACEX    = 144;    // X,Y of the face
static constexpr int32_t FACEY    = 165;
static constexpr int32_t ARMORX   = 233;    // (204 Center) X,Y of the armor
static constexpr int32_t ARMORY   = 174;
static constexpr int32_t MAPX     = 290;    // X,Y Area level
static constexpr int32_t MAPY     = 174;

static constexpr uint32_t NUMMICROS     = 6;                    // Amount of micro-sized #'s (weapon armed)
static constexpr uint32_t GODFACE       = 40;                   // God mode face
static constexpr uint32_t DEADFACE      = 41;                   // Dead face
static constexpr uint32_t FIRSTSPLAT    = 42;                   // First gibbed face
static constexpr uint32_t GIBTIME       = (TICKSPERSEC/6);      // Time for gibbed animation

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

static constexpr int32_t CARD_X[NUMCARDS] = { KEYX, KEYX, KEYX, KEYX + 3, KEYX + 3, KEYX + 3 };
static constexpr int32_t CARD_Y[NUMCARDS] = { BLUKEYY, YELKEYY, REDKEYY, BLUKEYY, YELKEYY, REDKEYY };
static constexpr int32_t MICRO_NUMS_X[NUMMICROS] = { 237, 249, 261, 237, 249, 261 };
static constexpr int32_t MICRO_NUMS_Y[NUMMICROS] = { 175, 175, 175, 185, 185, 185 };

struct sbflash_t {
    uint32_t    delay;     // Time delay
    uint32_t    times;     // Number of times to flash
    bool        doDraw;    // True if I draw now
};

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

//----------------------------------------------------------------------------------------------------------------------
// Process the timer for a shape flash
//----------------------------------------------------------------------------------------------------------------------
static void CycleFlash(sbflash_t& flash) noexcept {
    if (flash.delay) {              // Active?
        if (flash.delay > 1) {      // Still time?
            --flash.delay;          // Remove the time
        } else {
            if (--flash.times == 0) {               // Can I still go?
                flash.delay = 0;
                flash.doDraw = false;               // Force off
            } else {
                flash.delay = FLASHDELAY;           // Reset the time
                flash.doDraw ^= true;               // Toggle the draw flag
                if (flash.doDraw) {                 // If on, play sound
                    S_StartSound(0, sfx_itemup);
                }
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Locate and load all needed graphics for the status bar.
//----------------------------------------------------------------------------------------------------------------------
void ST_Start() noexcept {
    gpSBObj = &CelImages::loadImages(rSBARSHP, CelLoadFlagBits::MASKED);    // Status bar shapes
    gpFaces = &CelImages::loadImages(rFACES, CelLoadFlagBits::MASKED);      // Load all the face frames
    gpStatusBarShape = &CelImages::loadImage(rSTBAR);                       // Load the status bar

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

//----------------------------------------------------------------------------------------------------------------------
// Called during the think logic phase to prepare to draw the status bar
//----------------------------------------------------------------------------------------------------------------------
void ST_Ticker() noexcept {
    // Animate face
    --gFaceTics;                                // Count down
    if (gFaceTics & 0x8000) {                   // Negative?
        gFaceTics = Random::nextU32(15)*4;      // New random value
        gNewFace = Random::nextU32(2);          // Which face 0-2
    }

    // Draw special face?
    if (gStBar.specialFace) {
        gNewFace = SPECIAL_FACE_SPRITE[gStBar.specialFace];     // Get the face shape
        gFaceTics = TICKSPERSEC;                                // 1 second
        gStBar.specialFace = f_none;                            // Ack the shape
    }

    // Did we get gibbed?
    if (gStBar.gotgibbed && !gGibDraw) {    // In progress?
        gGibDraw = true;                    // In progress...
        gGibFrame = 0;
        gGibDelay = GIBTIME;
        gStBar.gotgibbed = false;
    }

    // Tried to open a CARD or SKULL door?
    {
        sbflash_t* pFlash = gFlashCards;

        for (uint32_t i = 0; i < NUMCARDS; ++i) {
            if (gStBar.tryopen[i]) {                // Did the user ask to flash the card?
                gStBar.tryopen[i] = false;          // Ack the flag
                pFlash->delay = FLASHDELAY;
                pFlash->times = FLASHTIMES + 1;
                pFlash->doDraw = false;
            }

            CycleFlash(*pFlash);    // Handle the ticker
            ++pFlash;
        }
    }
}

// TODO: REIMPLEMENT FPS COUNT
#define VBLTIMER 0

#if VBLTIMER
#define TIMERSIZE 16
static Word Frames;
static Word CountVBL[TIMERSIZE];
#endif

//----------------------------------------------------------------------------------------------------------------------
// Draw the status bar
//----------------------------------------------------------------------------------------------------------------------
void ST_Drawer() noexcept {
    UIUtils::drawUISprite(0, 160, *gpStatusBarShape);

    // TODO: REIMPLEMENT FPS COUNT
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

    // Draw ammo amount
    player_t& p = gPlayer;

    if (p.readyweapon != wp_nochange) {                                                  // No weapon?
        const uint32_t ammoType = gWeaponAmmos[p.readyweapon];
        if (ammoType != am_noammo) {                                                    // No ammo needed?
            const uint32_t ammoAmount = p.ammo[ammoType];
            UIUtils::printNumber(AMMOX, AMMOY, ammoAmount, UIUtils::PNFLAGS_RIGHT);     // Draw ammo
        }
    }

    // Draw health armor and level number
    UIUtils::printNumber(HEALTHX, HEALTHY, p.health, UIUtils::PNFLAGS_RIGHT|UIUtils::PNFLAGS_PERCENT);
    UIUtils::printNumber(ARMORX, ARMORY, p.armorpoints, UIUtils::PNFLAGS_RIGHT|UIUtils::PNFLAGS_PERCENT);
    UIUtils::printNumber(MAPX, MAPY, gGameMap, UIUtils::PNFLAGS_CENTER);

    // Cards & skulls: draw if flashing or owned
    {
        sbflash_t* pFlash = gFlashCards;

        for (uint32_t i = 0; i < NUMCARDS; ++i) {
            if (p.cards[i] || pFlash->doDraw) {
                UIUtils::drawUISprite(CARD_X[i], CARD_Y[i], gpSBObj->getImage(sb_card_b + i));
            }
            ++pFlash;
        }
    }

    // Weapons
    for (uint32_t i = 0; i < NUMMICROS; ++i) {
        if (p.weaponowned[i + 1]) {
            UIUtils::drawUISprite(MICRO_NUMS_X[i], MICRO_NUMS_Y[i], gpSBObj->getImage(sb_micro + i));
        }
    }

    // Face change
    uint32_t faceImgNum;

    if ((p.AutomapFlags & AF_GODMODE) || p.powers[pw_invulnerability]) {      // In god mode?
        faceImgNum = GODFACE;       // God mode
        gGibFrame = 0;
    } else if (gGibDraw) {          // Got gibbed
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

        faceImgNum = FIRSTSPLAT + gGibFrame;
    } 
    else if (p.health == 0) {
        faceImgNum = FIRSTSPLAT + gGibFrame;    // Dead man
    } 
    else {
        gGibFrame = 0;
        const uint32_t health = p.health;       // Get the health
        if (health >= 80) {                     // Div by 20 without a real divide (Saves time)
            faceImgNum = 0;
        } else if (health >= 60) {
            faceImgNum = 8;
        } else if (health >= 40) {
            faceImgNum = 16;
        } else if (health >= 20) {
            faceImgNum = 24;
        } else {
            faceImgNum = 32;                    // Bad shape...
        }
        faceImgNum = faceImgNum + gNewFace;     // Get shape requested
    }

    UIUtils::drawUISprite(FACEX, FACEY, gpFaces->getImage(faceImgNum));     // Dead man
}
