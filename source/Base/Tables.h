#pragma once

#include "Angle.h"
#include "Fixed.h"

// Table related defines
static constexpr uint32_t FINEANGLES        = 8192;             // Size of the fineangle table
static constexpr uint32_t FINEMASK          = FINEANGLES - 1;   // Rounding mask for table
static constexpr uint32_t ANGLETOFINESHIFT  = 19;               // Convert angle_t to fineangle table
static constexpr uint32_t SLOPERANGE        = 2048;             // Number of entries in tantoangle table
static constexpr uint32_t SLOPEBITS         = 11;               // Power of 2 for SLOPERANGE (2<<11)

// The table data and other stuff
extern Fixed        gFineTangent[4096];
extern Fixed*       gFineCosine;
extern Fixed        gFineSine[10240];
extern angle_t      gTanToAngle[2049];
extern uint32_t     gIDivTable[8192];                   // 1.0 / 0-5500 for recipocal muls
extern uint32_t     gCenterX;                           // Center view center X coord (integer)
extern uint32_t     gCenterY;                           // Center view center Y coord (integer)
extern uint32_t     g3dViewWidth;                       // Width of the 3d view
extern uint32_t     g3dViewHeight;                      // Height of the 3d view
extern uint32_t     g3dViewXOffset;                     // The left X coord for the 3d view on the screen
extern uint32_t     g3dViewYOffset;                     // The top y coord for the 3d view on the screen
extern float        gScaleFactor;                       // Ratio of current resolution over original 320x200 resolution
extern float        gInvScaleFactor;                    // Reciprocal ratio of current resolution over original 320x200 resolution
extern float        gGunXScale;                         // Scale factor for player's weapon for X
extern float        gGunYScale;                         // Scale factor for player's weapon for Y
extern float        gLightMins[256];                    // Minimum light factors
extern float        gLightSubs[256];                    // Light subtraction
extern float        gLightCoefs[256];                   // Light coeffecient
