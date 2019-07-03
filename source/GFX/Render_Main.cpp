#include "Base/Tables.h"
#include "Game/Data.h"
#include "Render.h"
#include "Things/MapObj.h"
#include "ThreeDO.h"

viswall_t       gVisWalls[MAXWALLCMDS];
viswall_t*      gLastWallCmd;
visplane_t      gVisPlanes[MAXVISPLANES];
visplane_t*     gLastVisPlane;
vissprite_t     gVisSprites[MAXVISSPRITES];
vissprite_t*    gpVisSprite;
uint8_t         gOpenings[MAXOPENINGS];
uint8_t*        gLastOpening;
Fixed           gViewX;
Fixed           gViewY;
Fixed           gViewZ;
angle_t         gViewAngle;
Fixed           gViewCos;
Fixed           gViewSin;
uint32_t        gExtraLight;
angle_t         gClipAngle;
angle_t         gDoubleClipAngle;

//--------------------------------------------------------------------------------------------------
// Init the math tables for the refresh system
//--------------------------------------------------------------------------------------------------
void R_Init() noexcept {
    R_InitData();                           // Init the data (Via loading or calculations)
    gClipAngle = gXToViewAngle[0];          // Get the left clip angle from viewport
    gDoubleClipAngle = gClipAngle * 2;      // Precalc angle * 2
}

//--------------------------------------------------------------------------------------------------
// Setup variables for a 3D refresh based on the current camera location and angle
//--------------------------------------------------------------------------------------------------
void R_Setup() noexcept {
    // Set up globals for new frame
    const player_t& player = gPlayers;      // Which player is the camera attached?
    const mobj_t& mapObj = *player.mo;
    gViewX = mapObj.x;                      // Get the position of the player
    gViewY = mapObj.y;
    gViewZ = player.viewz;                  // Get the height of the player
    gViewAngle = mapObj.angle;              // Get the angle of the player

    {
        const uint32_t angleIdx = gViewAngle >> ANGLETOFINESHIFT;   // Precalc angle index
        gViewSin = gFineSine[angleIdx];                             // Get the base sine value
        gViewCos = gFineCosine[angleIdx];                           // Get the base cosine value
    }

    gExtraLight = player.extralight << 6;   // Init the extra lighting value

    gLastVisPlane = gVisPlanes + 1;     // visplanes[0] is left empty
    gLastWallCmd = gVisWalls;           // No walls added yet
    gpVisSprite = gVisSprites;          // No sprites added yet
    gLastOpening = gOpenings;           // No openings found
}

//--------------------------------------------------------------------------------------------------
// Render the 3D view
//--------------------------------------------------------------------------------------------------
void R_RenderPlayerView() noexcept {
    R_Setup();          // Init variables based on camera angle
    bsp();              // Traverse the BSP tree for possible walls to render
    SegCommands();      // Draw all everything Z Sorted
    DrawColors();       // Draw color overlay if needed
    DrawWeapons();      // Draw the weapons on top of the screen
}
