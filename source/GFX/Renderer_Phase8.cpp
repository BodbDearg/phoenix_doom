#include "Renderer_Internal.h"

#include "Base/Endian.h"
#include "Base/Tables.h"
#include "Burger.h"
#include "Game/Data.h"
#include "Game/DoomRez.h"
#include "Game/Resources.h"
#include "Map/MapData.h"
#include "Things/Info.h"
#include "Things/MapObj.h"
#include "ThreeDO.h"

#define SCREENGUNY -40      // Y offset to center the player's weapon properly

BEGIN_NAMESPACE(Renderer)

/**********************************

    Using a point in space, determine if it is BEHIND a wall.
    Use a cross product to determine facing.
    
**********************************/

static uint32_t SegBehindPoint(viswall_t *ds,Fixed dx,Fixed dy)
{
    Fixed x1,y1;
    Fixed sdx,sdy;
    const seg_t *SegPtr = ds->SegPtr;
    
    x1 = SegPtr->v1.x;
    y1 = SegPtr->v1.y;
    
    sdx = SegPtr->v2.x-x1;
    sdy = SegPtr->v2.y-y1;
    
    dx -= x1;
    dy -= y1;
    
    sdx>>=FRACBITS;
    sdy>>=FRACBITS;
    dx>>=FRACBITS;
    dy>>=FRACBITS;
    
    dx*=sdy;
    sdx*=dy;
    if (sdx<dx) {
        return true;
    } 
    return false;
}

//--------------------------------------------------------------------------------------------------
// See if a sprite needs clipping and if so, then draw it clipped
//--------------------------------------------------------------------------------------------------
void drawVisSprite(const vissprite_t& visSprite) noexcept {
    viswall_t *ds;
    int x, r1, r2;
    int silhouette;
    int x1, x2;
    uint8_t* topsil,*bottomsil;
    uint32_t opening;
    int top, bottom;
    uint32_t scalefrac;
    uint32_t Clipped;

    x1 = visSprite.x1;        // Get the sprite's screen posts
    x2 = visSprite.x2;
    if (x1<0) {                 // These could be offscreen
        x1 = 0;
    }
    if (x2>=(int)gScreenWidth) {
        x2 = gScreenWidth-1;
    }
    scalefrac = visSprite.yscale;     // Get the Z scale    
    Clipped = false;                    // Assume I don't clip

    // scan drawsegs from end to start for obscuring segs
    // the first drawseg that has a greater scale is the clip seg
    ds = gLastWallCmd;
    do {
        --ds;           // Point to the next wall command
        
        // determine if the drawseg obscures the sprite
        if (ds->LeftX > x2 || ds->RightX < x1 ||
            ds->LargeScale <= scalefrac ||
            !(ds->WallActions&(AC_TOPSIL|AC_BOTTOMSIL|AC_SOLIDSIL)) ) {
            continue;           // doesn't cover sprite
        }

        if (ds->SmallScale<=scalefrac) {    // In range of the wall?
            if (SegBehindPoint(ds,visSprite.thing->x, visSprite.thing->y)) {
                continue;           // Wall seg is behind sprite
            }
        }
        if (!Clipped) {     // Never initialized?
            Clipped = true;
            x = x1;
            opening = gScreenHeight;
            do {
                gSprOpening[x] = opening;       // Init the clip table
            } while (++x<=x2);
        }
        r1 = ds->LeftX < x1 ? x1 : ds->LeftX;       // Get the clip bounds
        r2 = ds->RightX > x2 ? x2 : ds->RightX;

        // clip this piece of the sprite
        silhouette = ds->WallActions & (AC_TOPSIL|AC_BOTTOMSIL|AC_SOLIDSIL);
        x=r1;
        if (silhouette == AC_SOLIDSIL) {
            opening = gScreenHeight<<8;
            do {
                gSprOpening[x] = opening;       // Clip these to blanks
            } while (++x<=r2);
            continue;
        }
        
        topsil = ds->TopSil;
        bottomsil = ds->BottomSil;

        if (silhouette == AC_BOTTOMSIL) {   // bottom sil only
            do {
                opening = gSprOpening[x];
                if ( (opening&0xff) == gScreenHeight) {
                    gSprOpening[x] = (opening&0xff00) + bottomsil[x];
                }
            } while (++x<=r2);
        } else if (silhouette == AC_TOPSIL) {   // top sil only
            do {
                opening = gSprOpening[x];
                if ( !(opening&0xff00)) {
                    gSprOpening[x] = (topsil[x]<<8) + (opening&0xff);
                }
            } while (++x<=r2);
        } else if (silhouette == (AC_TOPSIL|AC_BOTTOMSIL) ) {   // both
            do {
                top = gSprOpening[x];
                bottom = top&0xff;
                top >>= 8;
                if (bottom == gScreenHeight) {
                    bottom = bottomsil[x];
                }
                if (!top) {
                    top = topsil[x];
                }
                gSprOpening[x] = (top<<8)+bottom;
            } while (++x<=r2);
        }
    } while (ds!=gVisWalls);
    
    // Now that I have created the clip regions, let's see if I need to do this    
    if (!Clipped) {                     // Do I have to clip at all?
        drawSpriteNoClip(visSprite);    // Draw it using no clipping at all */
        return;                         // Exit
    }
    
    // Check the Y bounds to see if the clip rect even touches the sprite    
    r1 = visSprite.y1;
    r2 = visSprite.y2;
    if (r1<0) {
        r1 = 0;     // Clip to screen coords
    }
    if (r2>=(int)gScreenHeight) {
        r2 = gScreenHeight;
    }
    x = x1;
    do {
        top = gSprOpening[x];
        if (top!=gScreenHeight) {        // Clipped?
            bottom = top&0xff;
            top >>=8;
            if (r1<top || r2>=bottom) { // Needs manual clipping!
                if (x!=x1) {        // Was any part visible?
                    drawSpriteClip(x1,x2, visSprite);  // Draw it and exit
                    return;
                }
                do {
                    top = gSprOpening[x];
                    if (top!=(gScreenHeight<<8)) {
                        bottom = top&0xff;
                        top >>=8;
                        if (r1<bottom && r2>=top) { // Is it even visible?
                            drawSpriteClip(x1,x2, visSprite);      // Draw it
                            return;
                        }
                    }
                } while (++x<=x2);
                return;     // It's not visible at all!!
            }
        }
    } while (++x<=x2);
    drawSpriteNoClip(visSprite);      // It still didn't need clipping!!
}

/**********************************

    Draw a single weapon or muzzle flash on the screen

**********************************/

static void DrawAWeapon(pspdef_t *psp,uint32_t Shadow)
{
    const state_t* const StatePtr = psp->StatePtr;                                      // Get the state struct pointer
    const uint32_t RezNum = StatePtr->SpriteFrame >> FF_SPRITESHIFT;                        // Get the file
    const uint16_t* Input = (uint16_t*)loadResourceData(RezNum);                        // Get the main pointer
    Input = (uint16_t*) GetShapeIndexPtr(Input,StatePtr->SpriteFrame & FF_FRAMEMASK);   // Pointer to the xy offset'd shape
    
    // FIXME: DC: Reimplement/replace
    #if 0
        ((LongWord *)Input)[7] = GunXScale;     // Set the scale factor
        ((LongWord *)Input)[10] = GunYScale;
        if (Shadow) {
            ((LongWord *)Input)[13] = 0x9C81;   // Set the shadow bits
        } else {
            ((LongWord *)Input)[13] = 0x1F00;   // Normal PMode
            #if 0
            if (StatePtr->SpriteFrame & FF_FULLBRIGHT) {
                Color = 255;            // Full bright
            } else {                    // Ambient light
                Color = players.mo->subsector->sector->lightlevel;
            }
            #endif
        }
    #endif

    // TODO: DC: Find a better place for these endian conversions
    int x = byteSwappedI16(Input[0]);
    int y = byteSwappedI16(Input[1]);
    x = ((psp->WeaponX + x ) * (int) gGunXScale) >> 20;
    y = ((psp->WeaponY + SCREENGUNY + y) * (int) gGunYScale) >> 16;
    x += gScreenXOffset;
    y += gScreenYOffset + 2;         // Add 2 pixels to cover up the hole in the bottom
    DrawMShape(x, y, (const CelControlBlock*) &Input[2]);    // Draw the weapon's shape
    releaseResource(RezNum);
}

/**********************************

    Draw the player's weapon in the foreground

**********************************/

void DrawWeapons(void)
{
    uint32_t i;
    uint32_t Shadow;        // Flag for shadowing
    pspdef_t *psp;
    
    psp = gPlayers.psprites; // Get the first sprite in the array 
    Shadow = false;         // Assume no shadow draw mode
    if (gPlayers.mo->flags & MF_SHADOW) {    // Could be active?
        i = gPlayers.powers[pw_invisibility];    // Get flash time
        if (i>=(5*TICKSPERSEC) || i&0x10) { // On a long time or if flashing...
            Shadow = true;      // Draw as a shadow right now
        }
    }
    i = 0;      // Init counter
    do {
        if (psp->StatePtr) {        // Valid state record?
            DrawAWeapon(psp,Shadow);    // Draw the weapon
        }
        ++psp;      // Next...
    } while (++i<NUMPSPRITES);  // All done?
    
    i = gScreenSize+rBACKGROUNDMASK;         // Get the resource needed

    DrawMShape(0,0, (const CelControlBlock*) loadResourceData(i));      // Draw the border
    releaseResource(i);                                                 // Release the resource
}

END_NAMESPACE(Renderer)
