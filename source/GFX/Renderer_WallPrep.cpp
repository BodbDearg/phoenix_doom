#include "Renderer_Internal.h"

#include "Base/Tables.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "Textures.h"

BEGIN_NAMESPACE(Renderer)

//----------------------------------------------------------------------------------------------------------------------
// Check all the visible walls and fill in all the "Blanks" such as texture pointers and sky hack variables.
// When finished all the viswall records are filled in.
//----------------------------------------------------------------------------------------------------------------------
static sector_t gEmptySector = { 
    0,                  // Floor height
    0,                  // Ceiling height
    (uint32_t) -2,      // Floor pic
    (uint32_t) -2,      // Ceiling pic
    (uint32_t) -2       // Light level
};

//----------------------------------------------------------------------------------------------------------------------
// Get the distance from the view x,y from a point in 2D space.
// The normal formula for doing this is 'dist = sqrt(x*x + y*y)' but that can be slow to do. 
// Instead I first get the angle of the point and then rotate it so that it is directly ahead.
// The resulting point then is the distance...
//----------------------------------------------------------------------------------------------------------------------
static Fixed point2dToDist(const Fixed px, const Fixed py) noexcept {    
    Fixed x = std::abs(px - gViewX);    // Get the absolute value point offset from the camera 
    Fixed y = std::abs(py - gViewY);
    
    if (y > x) {
        const Fixed temp = x;
        x = y;
        y = temp;
    }

    const angle_t angle = SlopeAngle(y, x) >> ANGLETOFINESHIFT;     // x = denominator
    x = (x >> (FRACBITS - 3)) * gFineCosine[angle];                 // Rotate the x
    x += (y >> (FRACBITS - 3)) * gFineSine[angle];                  // Rotate the y and add it
    x >>= 3;                                                        // Convert to fixed (I added 3 extra bits of precision)
    return x;                                                       // This is the true distance
}

//----------------------------------------------------------------------------------------------------------------------
// Returns the texture mapping scale for the current line at the given angle.
// Note: 'rwDistance' must be calculated first.
//----------------------------------------------------------------------------------------------------------------------
static Fixed scaleFromGlobalAngle(const Fixed rwDistance, const angle_t angleA, const angle_t angleB) noexcept {
    // Both sines are always positive
    const Fixed* const pSineTable = &gFineSine[ANG90 >> ANGLETOFINESHIFT];

    Fixed den = pSineTable[angleA >> ANGLETOFINESHIFT];
    Fixed num = pSineTable[angleB >> ANGLETOFINESHIFT];    
    num = fixedMul(gStretchWidth, num);
    den = fixedMul(rwDistance, den);

    if (den > num >> 16) {
        num = fixedDiv(num, den);       // Place scale in numerator
        if (num < 64 * FRACUNIT) {
            if (num >= 256) {
                return num;
            }
            return 256;     // Minimum scale value
        }
    }
    return 64 * FRACUNIT;   // Maximum scale value
}

//----------------------------------------------------------------------------------------------------------------------
// Calculate the wall scaling constants
//----------------------------------------------------------------------------------------------------------------------
static void latePrep(viswall_t& wall, const seg_t& lineSeg, const angle_t leftAngle) noexcept {
    // Calculate normalangle and rwDistance for scale calculation and texture mapping
    const angle_t normalAngle = lineSeg.angle + ANG90;  // Angle to wall
    angle_t offsetAngle = (normalAngle - leftAngle);

    if ((int32_t) offsetAngle < 0) {
        offsetAngle = negateAngle(offsetAngle);
    }

    if (offsetAngle > ANG90) {
        offsetAngle = ANG90;
    }

    const Fixed pointDistance = point2dToDist(lineSeg.v1.x, lineSeg.v1.y);  // Distance to end wall point
    const Fixed rwDistance = fixedMul(
        pointDistance,
        gFineSine[(ANG90 - offsetAngle) >> ANGLETOFINESHIFT]
    );
    wall.distance = rwDistance;

    // Calc scales
    offsetAngle = gXToViewAngle[wall.leftX];
    const Fixed scaleFrac = wall.LeftScale = scaleFromGlobalAngle(
        rwDistance,
        offsetAngle,
        (offsetAngle + gViewAngle) - normalAngle
    );
    Fixed scale2 = scaleFrac;

    if (wall.rightX > wall.leftX) {
        offsetAngle = gXToViewAngle[wall.rightX];
        scale2 = scaleFromGlobalAngle(
            rwDistance,
            offsetAngle,
            (offsetAngle + gViewAngle) - normalAngle
        );
        wall.ScaleStep = (int32_t)(scale2 - scaleFrac) / (int32_t)(wall.rightX - wall.leftX);
    }

    wall.RightScale = scale2;
    
    if (scale2 < scaleFrac) {
        wall.SmallScale = scale2;
        wall.LargeScale = scaleFrac;
    } else {
        wall.LargeScale = scale2;
        wall.SmallScale = scaleFrac;
    }
    
    if ((wall.WallActions & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE)) != 0) {
        offsetAngle = normalAngle - leftAngle;
        
        if (offsetAngle > ANG180) {
            offsetAngle = negateAngle(offsetAngle);     // Force unsigned
        }

        if (offsetAngle > ANG90) {
            offsetAngle = ANG90;    // Clip to maximum           
        }
        
        scale2 = fixedMul(pointDistance, gFineSine[offsetAngle >> ANGLETOFINESHIFT]);
        
        if (normalAngle - leftAngle < ANG180) {
            scale2 = -scale2;   // Reverse the texture anchor
        }
        
        wall.offset += scale2;
        wall.CenterAngle = ANG90 + gViewAngle - normalAngle;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Calculate the wall scaling constants
//----------------------------------------------------------------------------------------------------------------------
void wallPrep(
    const int32_t leftX,
    const int32_t rightX,
    const seg_t& lineSeg,
    const angle_t lineAngle
) noexcept {
    viswall_t& curWall = *gpEndVisWall;     // Get the first wall pointer
    ++gpEndVisWall;                         // Inc my pointer
    curWall.leftX = leftX;                  // Set the edges of the visible wall
    curWall.rightX = rightX;                // Right is inclusive!
    curWall.SegPtr = &lineSeg;              // For clipping

    line_t& lineDef = *lineSeg.linedef;
    const uint32_t lineFlags = lineDef.flags;
    lineDef.flags = lineFlags | ML_MAPPED;      // Mark as seen...    
    
    side_t& sideDef = *lineSeg.sidedef;                                     // Get the line side
    sector_t& frontSector = *lineSeg.frontsector;                           // Get the front sector
    const uint32_t f_ceilingpic = frontSector.CeilingPic;                   // Front sector ceiling image #
    uint32_t f_lightlevel = frontSector.lightlevel;
    const Fixed f_floorheight = frontSector.floorheight - gViewZ;           // Front sector floor height - viewz: adjust for camera z
    const Fixed f_ceilingheight = frontSector.ceilingheight - gViewZ;       // Front sector ceiling height - viewz
    
    // Set the floor and ceiling shape handles & look up animated texture numbers.
    // Note that ceiling might NOT exist!
    curWall.FloorPic = frontSector.FloorPic;
    
    if (f_ceilingpic == -1) {
        curWall.CeilingPic = 0;
    } else {
        curWall.CeilingPic = f_ceilingpic;
    }
    
    const sector_t* pBackSector = lineSeg.backsector;   // Get the back sector
    if (!pBackSector) {                                 // Invalid?
        pBackSector = &gEmptySector;
    }
    
    const uint32_t b_ceilingpic = pBackSector->CeilingPic;                  // Get backsector data into locals
    const uint32_t b_lightlevel = pBackSector->lightlevel;                  // Back sector light level
    const Fixed b_floorheight = pBackSector->floorheight - gViewZ;          // Adjust for camera z
    const Fixed b_ceilingheight = pBackSector->ceilingheight - gViewZ;
    uint32_t actionbits = 0;                                                // Reset vars for future storage
    
    // Add floors and ceilings if the wall needs one
    if (f_floorheight < 0 && (                                      // Is the camera above the floor?
            (frontSector.FloorPic != pBackSector->FloorPic) ||      // Floor texture changed?
            (f_floorheight != b_floorheight) ||                     // Differant height?
            (f_lightlevel != b_lightlevel) ||                       // Differant light?
            (b_ceilingheight == b_floorheight)                      // No thickness line?
        )
    ) {
        curWall.floorheight = curWall.floornewheight = f_floorheight>>FIXEDTOHEIGHT;
        actionbits = (AC_ADDFLOOR|AC_NEWFLOOR);     // Create floor
    }

    if ((f_ceilingpic != -1 || b_ceilingpic != -1) &&       // Normal ceiling?
        (f_ceilingheight > 0 || f_ceilingpic == -1) && (    // Camera below ceiling? Sky ceiling?
            (f_ceilingpic != b_ceilingpic) ||               // New ceiling image?
            (f_ceilingheight != b_ceilingheight) ||         // Differant ceiling height?
            (f_lightlevel != b_lightlevel) ||               // Differant ceiling light?
            (b_ceilingheight == b_floorheight)              // Thin dividing line?
        )
    ) {
        curWall.ceilingheight = curWall.ceilingnewheight = f_ceilingheight >>FIXEDTOHEIGHT;
        
        if (f_ceilingpic == -1) {
            actionbits |= AC_ADDSKY | AC_NEWCEILING;        // Add sky to the ceiling
        } else {
            actionbits |= AC_ADDCEILING | AC_NEWCEILING;    // Add ceiling texture
        }
    }
    
    curWall.t_topheight = f_ceilingheight>>FIXEDTOHEIGHT;   // Y coord of the top texture

    // Single sided line? (no back sector) They only have a center texture.
    if (pBackSector == &gEmptySector) {
        curWall.t_texture = getWallAnimTexture(sideDef.midtexture);
        int32_t t_texturemid;
        
        if (lineFlags & ML_DONTPEGBOTTOM) {     
            t_texturemid = f_floorheight + (curWall.t_texture->data.height << FRACBITS);    // Bottom of texture at bottom
        } else {
            t_texturemid = f_ceilingheight;     // Top of texture at top
        }
        
        t_texturemid += sideDef.rowoffset;                          // Add texture anchor offset
        curWall.t_texturemid = t_texturemid;                        // Save the top texture anchor var
        curWall.t_bottomheight = f_floorheight>>FIXEDTOHEIGHT;
        actionbits |= (AC_TOPTEXTURE | AC_SOLIDSIL);                // Draw the middle texture only
    } else {
        // Two sided lines are more tricky since I may be able to see through it.
        // Check if the bottom wall texture is visible?
        if (b_floorheight > f_floorheight) {
            // Draw the bottom texture
            curWall.b_texture = getWallAnimTexture(sideDef.bottomtexture);
            int32_t b_texturemid;
            
            if (lineFlags & ML_DONTPEGBOTTOM) {
                b_texturemid = f_ceilingheight;     // bottom of texture at bottom
            } else {
                b_texturemid = b_floorheight;       // Top of texture at top
            }
            
            b_texturemid += sideDef.rowoffset;      // Add the adjustment
            curWall.b_texturemid = b_texturemid;
            curWall.b_topheight = curWall.floornewheight = b_floorheight>>FIXEDTOHEIGHT;
            curWall.b_bottomheight = f_floorheight>>FIXEDTOHEIGHT;

            actionbits |= AC_NEWFLOOR|AC_BOTTOMTEXTURE;     // Generate a floor and bottom wall texture
        }

        if (b_ceilingheight < f_ceilingheight && (f_ceilingpic != -1 || b_ceilingpic != -1)) {  // Ceiling wall without sky
            // Draw the top texture
            curWall.t_texture = getWallAnimTexture(sideDef.toptexture);
            int32_t t_texturemid;
            
            if (lineFlags & ML_DONTPEGTOP) {
                t_texturemid = f_ceilingheight;     // Top of texture at top
            } else {
                t_texturemid = b_ceilingheight + (curWall.t_texture->data.height << FRACBITS);
            }
            
            t_texturemid += sideDef.rowoffset;      // Anchor the top texture
            curWall.t_texturemid = t_texturemid;    // Save the top texture anchor var
            curWall.t_bottomheight = curWall.ceilingnewheight = b_ceilingheight>>FIXEDTOHEIGHT;

            actionbits |= AC_NEWCEILING | AC_TOPTEXTURE;    // Generate the top texture
        }
        
        // Check if this wall is solid (This is for sprite clipping)
        if (b_floorheight >= f_ceilingheight || b_ceilingheight <= f_floorheight) {
            actionbits |= AC_SOLIDSIL;      // This is solid (For sprite masking)
        } else {
            const int32_t width = (rightX - leftX + 1);     // Get width of opening
            
            if ((b_floorheight > 0 && b_floorheight > f_floorheight) ||
                (f_floorheight < 0 && f_floorheight > b_floorheight)
            ) {
                actionbits |= AC_BOTTOMSIL;     // There is a mask on the bottom
                curWall.BottomSil = gpEndOpening - leftX;
                gpEndOpening += width;
            }
            
            if (f_ceilingpic != -1 || b_ceilingpic != -1) {                             // Only if no sky
                if ((b_ceilingheight <= 0 && b_ceilingheight < f_ceilingheight) ||
                    (f_ceilingheight > 0 && b_ceilingheight > f_ceilingheight)          // Top sil?
                ) {
                    actionbits |= AC_TOPSIL;    // There is a mask on the bottom
                    curWall.TopSil = gpEndOpening - leftX;
                    gpEndOpening += width;
                }
            }
        }
    }
    
    curWall.WallActions = actionbits;       // Save the action bits    
    if (f_lightlevel < 240) {               // Get the light level
        f_lightlevel += gExtraLight;        // Add the light factor
        
        if (f_lightlevel > 240) {
            f_lightlevel = 240;
        }
    }
    
    curWall.seglightlevel = f_lightlevel;                       // Save the light level
    curWall.offset = sideDef.textureoffset + lineSeg.offset;    // Texture anchor X
    latePrep(curWall, lineSeg, lineAngle);
}

END_NAMESPACE(Renderer)
