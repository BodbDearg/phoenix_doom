#include "Base/Tables.h"
#include "Game/Data.h"
#include "Render.h"
#include "Things/MapObj.h"
#include "ThreeDO.h"

viswall_t       viswalls[MAXWALLCMDS];
viswall_t*      lastwallcmd;
visplane_t      visplanes[MAXVISPLANES];
visplane_t*     lastvisplane;
vissprite_t     vissprites[MAXVISSPRITES];
vissprite_t*    vissprite_p;
uint8_t         openings[MAXOPENINGS];
uint8_t*        lastopening;
Fixed           viewx;
Fixed           viewy;
Fixed           viewz;
angle_t         viewangle;
Fixed           viewcos;
Fixed           viewsin;
uint32_t        extralight;
angle_t         clipangle;
angle_t         doubleclipangle;

//--------------------------------------------------------------------------------------------------
// Init the math tables for the refresh system
//--------------------------------------------------------------------------------------------------
void R_Init() noexcept {
    R_InitData();                       // Init the data (Via loading or calculations)
    clipangle = xtoviewangle[0];        // Get the left clip angle from viewport
    doubleclipangle = clipangle * 2;    // Precalc angle * 2
}

//--------------------------------------------------------------------------------------------------
// Setup variables for a 3D refresh based on the current camera location and angle
//--------------------------------------------------------------------------------------------------
void R_Setup() noexcept {
    // Set up globals for new frame
    const player_t& player = players;       // Which player is the camera attached?
    const mobj_t& mapObj = *player.mo;
    viewx = mapObj.x;                       // Get the position of the player
    viewy = mapObj.y;
    viewz = player.viewz;                   // Get the height of the player
    viewangle = mapObj.angle;               // Get the angle of the player

    {
        const uint32_t angleIdx = viewangle >> ANGLETOFINESHIFT;    // Precalc angle index
        viewsin = finesine[angleIdx];                               // Get the base sine value
        viewcos = finecosine[angleIdx];                             // Get the base cosine value
    }

    extralight = player.extralight << 6;    // Init the extra lighting value

    lastvisplane = visplanes + 1;   // visplanes[0] is left empty
    lastwallcmd = viswalls;         // No walls added yet
    vissprite_p = vissprites;       // No sprites added yet
    lastopening = openings;         // No openings found
}

//--------------------------------------------------------------------------------------------------
// Render the 3D view
//--------------------------------------------------------------------------------------------------
void R_RenderPlayerView() noexcept {
    R_Setup();          // Init variables based on camera angle
    BSP();              // Traverse the BSP tree for possible walls to render
    SegCommands();      // Draw all everything Z Sorted
    DrawColors();       // Draw color overlay if needed
    DrawWeapons();      // Draw the weapons on top of the screen
}
