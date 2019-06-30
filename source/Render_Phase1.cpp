#include "CelUtils.h"

#include "Data.h"
#include "DoomRez.h"
#include "Info.h"
#include "MapData.h"
#include "MapObj.h"
#include "MapUtil.h"
#include "MathUtils.h"
#include "Render_Main.h"
#include "Resources.h"
#include "Tables.h"

static constexpr uint32_t MAXSEGS = 32;     // Maximum number of segs to scan

/**********************************

    By traversing the BSP tree, I will create a viswall_t array describing
    all walls are are visible to the computer screen, they may be projected off
    the left and right sides but this is to allow for scaling of the textures
    properly for clipping.
    
    All backface walls are removed by casting two angles to the end points and seeing
    if the differance in the angles creates a negative number (Reversed).
    The viswall_t record will contain the leftmost angle in unmodified 3 Space, the
    clipped screen left x and right x and the line segment that needs to be rendered.
    
    I also create all the sprite records (Unsorted) so that they can be merged
    with the rendering system to handle clipping.
    
**********************************/

typedef struct {
    int LeftX;      /* Left side of post */
    int RightX;     /* Right side of post */
} cliprange_t;

Word SpriteTotal;       /* Total number of sprites to render */
Word *SortedSprites;    /* Pointer to array of words of sprites to render */
static Word SortBuffer[MAXVISSPRITES*2];

static seg_t *curline;          /* Current line segment being processed */
static angle_t lineangle1;      /* Angle to leftmost side of wall segment */

static Word checkcoord[9][4] = {
{BOXRIGHT,BOXTOP,BOXLEFT,BOXBOTTOM},        /* Above,Left */
{BOXRIGHT,BOXTOP,BOXLEFT,BOXTOP},           /* Above,Center */
{BOXRIGHT,BOXBOTTOM,BOXLEFT,BOXTOP},        /* Above,Right */
{BOXLEFT,BOXTOP,BOXLEFT,BOXBOTTOM},         /* Center,Left */
{-1,0,0,0},         /* Center,Center */
{BOXRIGHT,BOXBOTTOM,BOXRIGHT,BOXTOP},       /* Center,Right */
{BOXLEFT,BOXTOP,BOXRIGHT,BOXBOTTOM},        /* Below,Left */
{BOXLEFT,BOXBOTTOM,BOXRIGHT,BOXBOTTOM},     /* Below,Center */
{BOXLEFT,BOXBOTTOM,BOXRIGHT,BOXTOP} };      /* Below,Right */

static cliprange_t solidsegs[MAXSEGS];      /* List of valid ranges to scan through */
static cliprange_t *newend;     /* Pointer to the first free entry */

/**********************************

    I will now find and try to display all objects and sprites in the 3D view. 
    I throw out any sprites that are off the screen to the left or right. 
    I don't check top to bottom.
    
**********************************/

static void SortAllSprites(void)
{
    vissprite_t *VisPtr;
    Word i;
    Word *LocalPtr;
    
    VisPtr = vissprites;
    SpriteTotal = vissprite_p - VisPtr;     /* How many sprites to draw? */
    if (SpriteTotal) {      /* Any sprites to speak of? */
        LocalPtr = SortBuffer;  /* Init buffer pointer */
        i = 0;
        do {
            *LocalPtr++ = (VisPtr->yscale<<7)+i;    /* Create array of indexs */
            ++VisPtr;
        } while (++i<SpriteTotal);  /* All done? */
        SortedSprites = SortWords(SortBuffer,&SortBuffer[MAXVISSPRITES],SpriteTotal);       /* Sort the sprites */
    }
}

//-------------------------------------------------------------------------------------------------
// Get the sprite angle (0-7) to render a thing with
//-------------------------------------------------------------------------------------------------
static uint8_t getThingSpriteAngleForViewpoint(const Fixed viewpointX, const Fixed viewpointY, const mobj_t* const pThing) {
    angle_t ang = PointToAngle(viewx, viewy, pThing->x, pThing->y);         // Get angle to thing
    ang -= pThing->angle;                                                   // Adjust for which way the thing is facing
    const uint8_t angleIdx = (ang + (angle_t)((ANG45 / 2) * 9U)) >> 29;     // Compute and return angle index (0-7)
    return angleIdx;
}

//-------------------------------------------------------------------------------------------------
// I have a possible sprite record, transform to 2D coords and then see if it is clipped.
// If the sprite is not clipped then it is prepared for rendering.
//-------------------------------------------------------------------------------------------------
static void PrepMObj(const mobj_t* const pThing) {
    // This is a HACK, so I don't draw the player for the 3DO version
    if (pThing->player) {
        return;
    }

    // Don't draw the sprite if we have hit the maximum limit
    vissprite_t* vis = vissprite_p;
    if (vis >= &vissprites[MAXVISSPRITES]) {
        return;
    }

    // Transform the origin point
    Fixed Trx = pThing->x - viewx;              // Get the point in 3 Space
    Fixed Try = pThing->y - viewy;
    Fixed Trz = sfixedMul16_16(Trx, viewcos);   // Rotate around the camera
    Trz += sfixedMul16_16(Try, viewsin);        // Add together

    // Ignore sprite if too close to the camera (too large)
    if (Trz < MINZ) {
        return;
    }
    
    // Calc the 3Space x coord
    Trx = sfixedMul16_16(Trx, viewsin);
    Trx -= sfixedMul16_16(Try, viewcos);
    
    // Ignore sprite if greater than 45 degrees off the side
    if (Trx > (Trz << 2) || Trx < -(Trz << 2)) {
        return;
    }

    // Figure out what sprite, frame and frame angle we want
    state_t* const pStatePtr = pThing->state;
    const uint32_t spriteResourceNum = pStatePtr->SpriteFrame >> FF_SPRITESHIFT;
    const uint32_t spriteFrameNum = pStatePtr->SpriteFrame & FF_FRAMEMASK;
    const uint8_t spriteAngle = getThingSpriteAngleForViewpoint(viewx, viewy, pThing);

    // Load the current sprite for the thing and the info for the actual sprite to use
    const Sprite* const pSprite = loadSprite(spriteResourceNum);
    ASSERT(spriteFrameNum < pSprite->numFrames);

    const SpriteFrame* const pSpriteFrame = &pSprite->pFrames[spriteFrameNum];
    const SpriteFrameAngle* const pSpriteFrameAngle = &pSpriteFrame->angles[spriteAngle];

    // Store information in a vissprite.
    // I also will clip to screen coords.
    const Fixed xScale = sfixedDiv16_16(CenterX << FRACBITS, Trz);  // Get the scale factor
    vis->xscale = xScale;                                           // Save it
    Trx -= (Fixed) pSpriteFrameAngle->leftOffset << FRACBITS;       // Adjust the x to the sprite's x
    int x1 = (sfixedMul16_16(Trx, xScale) >> FRACBITS) + CenterX;   // Scale to screen coords

    if (x1 > (int) ScreenWidth) {
        return;     // Off the right side, don't draw!
    }

    int x2 = sfixedMul16_16(pSpriteFrameAngle->width, xScale) + x1;

    if (x2 <= 0) {
        return;     // Off the left side, don't draw!
    }
    
    // Get light level
    const Fixed yScale = sfixedMul16_16(xScale, Stretch);   // Adjust for aspect ratio
    vis->yscale = yScale;
    vis->pSprite = pSpriteFrameAngle;
    vis->x1 = x1;                                           // Save the edge coords
    vis->x2 = x2;
    vis->thing = pThing;

    if (pThing->flags & MF_SHADOW) {                        // Draw a shadow...
        x1 = 0x8000U;
    } else if (pStatePtr->SpriteFrame & FF_FULLBRIGHT) {
        x1 = 255;                                           // full bright
    } else {
        x1 = pThing->subsector->sector->lightlevel;         // + extralight;

        if ((Word) x1 >= 256) {
            x1 = 255;                                       // Use maximum
        }
    }

    if (pSpriteFrameAngle->flipped != 0) {  // Reverse the shape?
        x1 |= 0x4000;
    }

    vis->colormap = x1;                                             // Save the light value
    Trz = pThing->z - viewz;
    Trz += (((Fixed) pSpriteFrameAngle->topOffset) << FRACBITS);    // Height offset

    // Determine screen top and bottom Y for the sprite
    const Fixed topY = (CenterY << FRACBITS) - sfixedMul16_16(Trz, yScale);
    const Fixed botY = topY + sfixedMul16_16(pSpriteFrameAngle->height << FRACBITS, yScale);
    vis->y1 = topY >> FRACBITS;
    vis->y2 = botY >> FRACBITS;

    // Check if vertically offscreen, if not use the sprite record
    if (vis->y2 >= 0 || vis->y1 < (int) ScreenHeight) {     
        vissprite_p = vis + 1;
    }
}

/**********************************

    Given a sector pointer, and if I hadn't already rendered the sprites,
    make valid sprites for the sprite list.
    
**********************************/

static void SpritePrep(sector_t *se)
{
    mobj_t *thing;
    if (se->validcount != validcount) {     /* Has this been processed? */
        se->validcount = validcount;    /* Mark it */           
        thing = se->thinglist;      /* Init the thing list */
        if (thing) {                /* Traverse the linked list */
            do {
                PrepMObj(thing);        /* Draw the object if ok... */
                thing = thing->snext;   /* Next? */
            } while (thing);
        }
    }
}

/**********************************

    Store the data describing a wall section that needs to be drawn
    
**********************************/

static void StoreWallRange(Word LeftX,Word RightX)
{
    WallPrep(LeftX,RightX,curline,lineangle1);  /* Create the wall data */
}

/**********************************

    Clips a wall segment and adds it to the solid wall
    segment list for masking.
    
**********************************/

static void ClipSolidWallSegment(int LeftX,int RightX)
{
    cliprange_t *next;
    cliprange_t *start;
    cliprange_t *next2;
    int Temp;

/* Find the first range that touches the range (adjacent pixels are touching) */

    start = solidsegs;  /* Init start table */
    Temp = LeftX-1;
    if (start->RightX < Temp) { /* Loop down */
        do {
            ++start;        /* Next entry */
        } while (start->RightX < Temp);
    }

    if (LeftX < start->LeftX) {     /* Clipped on the left? */
        if (RightX < start->LeftX-1) {  /* post is entirely visible, so insert a new clippost */
            StoreWallRange(LeftX,RightX);       /* Draw the wall */
            next = newend;
            newend = next+1;        /* Save the new last entry */
            if (next != start) {        /* Copy the current entry over */
                do {
                    --next;     /* Move back one */
                    next[1] = next[0];  /* Copy the struct */
                } while (next!=start);
            }
            start->LeftX = LeftX;   /* Insert the new record */
            start->RightX = RightX;
            return;         /* Exit now */
        }

      /* Oh oh, there is a wall in front, clip me */
      
        StoreWallRange(LeftX,start->LeftX-1);   /* I am clipped on the right */
        start->LeftX = LeftX;       /* Adjust the clip size to a new left edge */
    }

    if (RightX <= start->RightX) {  /* Is the rest obscured? */
        return;         /* Yep, exit now */
    }

    /* Start has the first post group that needs to be removed */
    
    next = start;
    next2 = next+1;
    if (RightX >= next2->LeftX-1) {
        do {
        /* there is a fragment between two posts */
            StoreWallRange(next->RightX+1,next2->LeftX-1);
            next=next2;
            if (RightX <= next2->RightX) {  /* bottom is contained in next */
                start->RightX = next2->RightX;  /* adjust the clip size */
                goto crunch;        /* Don't store the final fragment */
            }
            ++next2;
        } while (RightX >= next2->LeftX-1);
    }
    StoreWallRange(next->RightX+1,RightX);      /* Save the final fragment */
    start->RightX = RightX;     /* Adjust the clip size (Clipped on the left) */

/* remove start+1 to next from the clip list, */
/* because start now covers their area */

crunch:
    if (next != start) {    /* Do I need to remove any? */
        if (next != newend) {   /* remove a post */
            do {
                ++next;
                ++start;
                start[0] = next[0];     /* Copy the struct */
            } while (next!=newend);
        }
        newend = start+1;       /* All disposed! */
    }
}

/**********************************

    Clips a wall segment but does not add it to the solid wall
    segment list for masking.
    
**********************************/

static void ClipPassWallSegment(int LeftX,int RightX)
{
    cliprange_t *ClipPtr;
    cliprange_t *NextClipPtr;
    int Temp;

/* find the first range that touches the range (adjacent pixels are touching) */

    ClipPtr = solidsegs;
    Temp = LeftX-1;         /* Leftmost edge I can ignore */
    if (ClipPtr->RightX < Temp) {   /* Skip over non-touching posts */
        do {
            ++ClipPtr;      /* Next index */    
        } while (ClipPtr->RightX < Temp);
    }

    if (LeftX < ClipPtr->LeftX) {   /* Is the left side visible? */
        if (RightX < ClipPtr->LeftX-1) {    /* Post is entirely visible (above start) */
            StoreWallRange(LeftX,RightX);   /* Store the range! */
            return;                 /* Exit now! */
        }
        StoreWallRange(LeftX,ClipPtr->LeftX-1); /* Oh oh, I clipped on the right! */
    }
    
    /* At this point, I know that some leftmost pixels are hidden. */

    if (RightX <= ClipPtr->RightX) {        /* All are hidden? */
        return;         /* Don't draw. */
    }
    NextClipPtr = ClipPtr+1;        /* Next index */
    if (RightX >= NextClipPtr->LeftX-1) {   /* Now draw all fragments behind solid posts */
        do {
            StoreWallRange(ClipPtr->RightX+1,NextClipPtr->LeftX-1);
            if (RightX <= NextClipPtr->RightX) {    /* Is the rest hidden? */
                return;
            }
            ClipPtr = NextClipPtr;  /* Next index */
            ++NextClipPtr;          /* Inc running pointer */
        } while (RightX >= NextClipPtr->LeftX-1);
    }
    StoreWallRange(ClipPtr->RightX+1,RightX);       /* Draw the final fragment */
}

/**********************************

    Clips the given segment and adds any visible pieces to the line list
    I also add to the solid wall list so that I can rule out BSP sections quickly.
    
**********************************/

static void AddLine(seg_t *line,sector_t *FrontSector)
{
    angle_t angle1,angle2,span,tspan;
    sector_t *backsector;

    angle1 = PointToAngle(viewx,viewy,line->v1.x,line->v1.y);   /* Calc the angle for the left edge */
    angle2 = PointToAngle(viewx,viewy,line->v2.x,line->v2.y);   /* Now the right edge */

    span = angle1 - angle2;     /* Get the line span */
    if (span >= ANG180) {       /* Backwards? */
        return;     /* Don't handle backwards lines */
    }
    lineangle1 = angle1;        /* Store the leftmost angle for StoreWallRange */
    angle1 -= viewangle;        /* Adjust the angle for viewangle */
    angle2 -= viewangle;

    tspan = angle1+clipangle;   /* Adjust the center x of 0 */
    if (tspan > doubleclipangle) {  /* Possibly off the left side? */
        tspan -= doubleclipangle;   /* See if it's visible */
        if (tspan >= span) {    /* Off the left? */
            return; /* Remove it */
        }
        angle1 = clipangle; /* Clip the left edge */
    }
    tspan = clipangle - angle2;     /* Get the right edge adjustment */
    if (tspan > doubleclipangle) {  /* Possibly off the right side? */
        tspan -= doubleclipangle;
        if (tspan >= span) {        /* Off the right? */
            return;         /* Off the right side */
        }
        angle2 = -(int)clipangle;       /* Clip the right side */
    }

/* The seg is in the view range, but not necessarily visible */
/* It may be a line for specials or imbedded floor line */

    angle1 = (angle1+ANG90)>>(ANGLETOFINESHIFT+1);      /* Convert angles to table indexs */
    angle2 = (angle2+ANG90)>>(ANGLETOFINESHIFT+1);
    angle1 = viewangletox[angle1];      /* Get the screen x left */
    angle2 = viewangletox[angle2];      /* Screen x right */
    if (angle1 >= angle2) {
        return;             /* This is too small to bother with or invalid */
    }
    --angle2;                   /* Make the right side inclusive */
    backsector = line->backsector;  /* Get the back sector */
    curline = line;         /* Save the line record */

    if (!backsector ||  /* Single sided line? */
        backsector->ceilingheight <= FrontSector->floorheight ||    /* Closed door? */
        backsector->floorheight >= FrontSector->ceilingheight) {
        ClipSolidWallSegment(angle1,angle2);        /* Make a SOLID wall */
        return;
    }

    if (backsector->ceilingheight != FrontSector->ceilingheight ||  /* Normal window */
        backsector->floorheight != FrontSector->floorheight ||
        backsector->CeilingPic != FrontSector->CeilingPic ||        /* Different texture */
        backsector->FloorPic != FrontSector->FloorPic ||            /* Floor texture */
        backsector->lightlevel != FrontSector->lightlevel ||        /* Differant light? */
        line->sidedef->midtexture) {            /* Center wall texture? */
        ClipPassWallSegment(angle1,angle2);     /* Render but allow walls behind it */
    }
}

/**********************************

    Given a subsector pointer, pass all walls to the
    rendering engine. Also pass all the sprites.
    
**********************************/

static void Subsector(const subsector_t* sub)
{
    Word count;
    seg_t *line;
    sector_t *CurrentSector;
    
    CurrentSector = sub->sector;    /* Get the front sector */  
    SpritePrep(CurrentSector);          /* Prepare sprites for rendering */
    count = sub->numsublines;   /* Number of line to process */
    line = sub->firstline;      /* Get pointer to the first line */
    do {
        AddLine(line,CurrentSector);        /* Render each line */
        ++line;             /* Inc the line pointer */
    } while (--count);      /* All done? */
}

/**********************************

    Check if any part of the BSP bounding box is touching the view arc. 
    Also project the width of the box to screen coords to see if it is too
    small to even bother with.
    If I should process this box then return TRUE.
    
**********************************/

static Word CheckBBox(const Fixed *bspcoord)
{
    angle_t angle1,angle2;  /* Left and right angles for view */

/* Find the corners of the box that define the edges from current viewpoint */
    
    {       /* Use BoxPtr */
    Word *BoxPtr;           /* Pointer to bspcoord offset table */
    BoxPtr = &checkcoord[0][0];     /* Init to the base of the table (Above) */
    if (viewy < bspcoord[BOXTOP]) { /* Off the top? */
        BoxPtr+=12;                 /* Index to center */
        if (viewy <= bspcoord[BOXBOTTOM]) { /* Off the bottom? */
            BoxPtr += 12;           /* Index to below */
        }
    }

    if (viewx > bspcoord[BOXLEFT]) {    /* Check if off the left edge */
        BoxPtr+=4;                  /* Center x */
        if (viewx >= bspcoord[BOXRIGHT]) {  /* Is it off the right? */
            BoxPtr+=4;
        }
    }
    if (BoxPtr[0]==-1) {        /* Center node? */
        return true;    /* I am in the center of the box, process it!! */
    }
    
    
/* I now have in 3 Space the endpoints of the BSP box, now project it to the screen */
/* and see if it is either off the screen or too small to even care about */

    angle1 = PointToAngle(viewx,viewy,bspcoord[BoxPtr[0]],bspcoord[BoxPtr[1]]) - viewangle; /* What is the projected angle? */
    angle2 = PointToAngle(viewx,viewy,bspcoord[BoxPtr[2]],bspcoord[BoxPtr[3]]) - viewangle; /* Now the rightmost angle */
    }       /* End use of BoxPtr */

    {       /* Use span and tspan */
    angle_t span;       /* Degrees of span for the view */
    angle_t tspan;      /* Temp */

    span = angle1 - angle2; /* What is the span of the angle? */
    if (span >= ANG180) {   /* Whoa... I must be sitting on the line or it's in my face! */
        return true;    /* Process this one... */
    }
    
    /* angle1 must be treated as signed, so to see if it is either >-clipangle and < clipangle */
    /* I add clipangle to the angle to adjust the 0 center and compare to clipangle * 2 */
    
    tspan = angle1+clipangle;
    if (tspan > doubleclipangle) {  /* Possibly off the left edge */
        tspan -= doubleclipangle;
        if (tspan >= span) {        /* Off the left side? */
            return false;   /* Don't bother, it's off the left side */
        }
        angle1 = clipangle; /* Clip the left edge */
    }
    
    tspan = clipangle - angle2;     /* Move from a zero base of "clipangle" */
    if (tspan > doubleclipangle) {  /* Possible off the right edge */
        tspan -= doubleclipangle;
        if (tspan >= span) {    /* The entire span is off the right edge? */
            return false;           /* Too far right! */
        }
        angle2 = -(int)clipangle;   /* Clip the right edge angle */
    }

/* See if any part of the contained area could be visible */

    angle1 = (angle1+ANG90)>>(ANGLETOFINESHIFT+1);  /* Rotate 90 degrees and make table index */
    angle2 = (angle2+ANG90)>>(ANGLETOFINESHIFT+1);
    } /* End use of span and tspan */
    
    angle1 = viewangletox[angle1];      /* Get the screen coords */
    angle2 = viewangletox[angle2];
    if (angle1 == angle2) {             /* Is the run too small? */
        return false;               /* Don't bother rendering it then */
    }
    --angle2;
    {   /* Use start */
    
    cliprange_t *SolidPtr;  /* Pointer to range */
    SolidPtr = solidsegs;   /* Index to the solid walls */
    if (SolidPtr->RightX < (int)angle2) {       /* Scan through the sorted list */
        do {
            ++SolidPtr;     /* Next entry */
        } while (SolidPtr->RightX< (int)angle2);
    }
    if ((int)angle1 >= SolidPtr->LeftX && (int)angle2 <= SolidPtr->RightX) {
        return false;   /* This block is behind a solid wall! */
    }
    return true;        /* Process me! */
    }   /* End use of start */
}

//---------------------------------------------------------------------------------------------------------------------
// Traverse the BSP tree starting from a tree node (Or sector) and recursively subdivide if needed.
// Use a cross product from the line cast from the viewxy to the bspxy and the bsp line itself.
//---------------------------------------------------------------------------------------------------------------------
static void RenderBSPNode(const node_t* pNode) {
    // Is this node actual pointing to a sub sector?
    if (isNodeChildASubSector(pNode)) {
        // Process the sub sector.
        // N.B: Need to fix up the pointer as well due to the lowest bit set as a flag!
        const subsector_t* pSubSector = (const subsector_t*) getActualNodeChildPtr(pNode);
        Subsector(pSubSector);
        return;
    }
    
    // Decide which side the view point is on
    uint32_t Side = PointOnVectorSide(viewx, viewy, &pNode->Line);  // Is this the front side?
    RenderBSPNode((node_t*) pNode->Children[Side]);                 // Process the side closer to me
    Side ^= 1;                                                      // Swap the side
    
    if (CheckBBox(pNode->bbox[Side])) {                     // Is the viewing rect on both sides?
        RenderBSPNode((node_t*) pNode->Children[Side]);     // Render the back side
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Find all walls that can be rendered in the current view plane. I make it handle the whole
// screen by placing fake posts on the farthest left and right sides in solidsegs 0 and 1.
//---------------------------------------------------------------------------------------------------------------------
void BSP() {
    ++validcount;                       // For sprite recursion
    solidsegs[0].LeftX = -0x4000;       // Fake leftmost post
    solidsegs[0].RightX = -1;
    solidsegs[1].LeftX = ScreenWidth;   // Fake rightmost post
    solidsegs[1].RightX = 0x4000;
    newend = solidsegs+2;               // Init the free memory pointer
    RenderBSPNode(gpBSPTreeRoot);       // Begin traversing the BSP tree for all walls in render range
    SortAllSprites();                   // Sort the sprites from front to back
}
