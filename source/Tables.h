#pragma once

#include "Doom.h"

extern Fixed        finetangent[4096];
extern Fixed*       finecosine;
extern Fixed        finesine[10240];
extern angle_t      tantoangle[2049];
extern angle_t      xtoviewangle[MAXSCREENWIDTH + 1];
extern int32_t      viewangletox[FINEANGLES / 4];
extern uint32_t     yslope[MAXSCREENHEIGHT];            // 6.10 frac
extern uint32_t     distscale[MAXSCREENWIDTH];          // 1.15 frac
extern uint32_t     IDivTable[8192];                    // 1.0 / 0-5500 for recipocal muls   
extern uint32_t     CenterX;                            // Center X coord in fixed point
extern uint32_t     CenterY;                            // Center Y coord in fixed point
extern uint32_t     ScreenWidth;                        // Width of the view screen
extern uint32_t     ScreenHeight;                       // Height of the view screen
extern Fixed        Stretch;                            // Stretch factor
extern Fixed        StretchWidth;                       // Stretch factor * ScreenWidth
extern uint32_t     ScreenXOffset;                      // True X coord for projected screen
extern uint32_t     ScreenYOffset;                      // True Y coord for projected screen
extern uint32_t     GunXScale;                          // Scale factor for player's weapon for X
extern uint32_t     GunYScale;                          // Scale factor for player's weapon for Y
extern Fixed        lightmins[256];                     // Minimum light factors
extern Fixed        lightsubs[256];                     // Light subtraction
extern Fixed        lightcoefs[256];                    // Light coeffecient
extern Fixed        planelightcoef[256];                // Plane light coeffecient
