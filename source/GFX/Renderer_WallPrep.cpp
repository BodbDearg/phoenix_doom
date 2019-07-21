#include "Renderer_Internal.h"

#include "Base/FMath.h"
#include "Base/Tables.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "Textures.h"

// TODO: TEST
#include "Video.h"

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
// Get the distance from the view xy to the given point
//----------------------------------------------------------------------------------------------------------------------
static float viewToPointDist(const Fixed px, const Fixed py) noexcept {    
    const float dx = FMath::doomFixed16ToFloat<float>(px - gViewXFrac);
    const float dy = FMath::doomFixed16ToFloat<float>(py - gViewYFrac);
    return std::sqrt(dx * dx + dy * dy);
}

//----------------------------------------------------------------------------------------------------------------------
// Returns the texture mapping scale for the current line at the given angle.
// Note: 'rwDistance' must be calculated first.
//----------------------------------------------------------------------------------------------------------------------
static float scaleFromGlobalAngle(const float rwDistance, const angle_t angleA, const angle_t angleB) noexcept {
    // Note: both sines are always positive
    const float angleARad = FMath::doomAngleToRadians<float>(angleA) + FMath::ANGLE_90<float>;
    const float angleBRad = FMath::doomAngleToRadians<float>(angleB) + FMath::ANGLE_90<float>;

    const float stretchWidth = FMath::doomFixed16ToFloat<float>(gStretchWidth);
    const float den = std::sin(angleARad) * rwDistance;
    const float num = std::sin(angleBRad) * stretchWidth;

    if (den != 0) {
        const float scale = num / den;
        return std::fmin(std::fmax(scale, MIN_RENDER_SCALE), MAX_RENDER_SCALE);
    }
    else {
        return MAX_RENDER_SCALE;
    }
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

    const float pointDistance = viewToPointDist(lineSeg.v1.x, lineSeg.v1.y);    // Distance to end wall point
    const float rwDistance = pointDistance * std::sinf(FMath::doomAngleToRadians<float>(ANG90 - offsetAngle));
    wall.distance = rwDistance;

    // Calc scales
    offsetAngle = gXToViewAngle[wall.leftX];
    const float scale = wall.LeftScale = scaleFromGlobalAngle(
        rwDistance,
        offsetAngle,
        (offsetAngle + gViewAngleBAM) - normalAngle
    );
    float scale2 = scale;

    if (wall.rightX > wall.leftX) {
        offsetAngle = gXToViewAngle[wall.rightX];
        scale2 = scaleFromGlobalAngle(
            rwDistance,
            offsetAngle,
            (offsetAngle + gViewAngleBAM) - normalAngle
        );
        wall.ScaleStep = (scale2 - scale) / (float)(wall.rightX - wall.leftX);
    }

    wall.RightScale = scale2;
    
    if (scale2 < scale) {
        wall.SmallScale = FMath::floatToDoomFixed16(scale2);
        wall.LargeScale = FMath::floatToDoomFixed16(scale);
    } else {
        wall.LargeScale = FMath::floatToDoomFixed16(scale2);
        wall.SmallScale = FMath::floatToDoomFixed16(scale);
    }
    
    if ((wall.WallActions & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE)) != 0) {
        offsetAngle = normalAngle - leftAngle;
        
        if (offsetAngle > ANG180) {
            offsetAngle = negateAngle(offsetAngle);     // Force unsigned
        }

        if (offsetAngle > ANG90) {
            offsetAngle = ANG90;    // Clip to maximum           
        }
        
        scale2 = pointDistance * std::sin(FMath::doomAngleToRadians<float>(offsetAngle));
        
        if (normalAngle - leftAngle < ANG180) {
            scale2 = -scale2;   // Reverse the texture anchor
        }
        
        wall.offset += scale2;
        wall.CenterAngle = gViewAngleBAM - normalAngle;
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

    // Front sector floor and ceiling height - viewz: adjust for camera z
    const float f_floorheight = FMath::doomFixed16ToFloat<float>(frontSector.floorheight - gViewZFrac);
    const float f_ceilingheight = FMath::doomFixed16ToFloat<float>(frontSector.ceilingheight - gViewZFrac);
    
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

    // Adjust for camera z
    const float b_floorheight = FMath::doomFixed16ToFloat<float>(pBackSector->floorheight - gViewZFrac);
    const float b_ceilingheight = FMath::doomFixed16ToFloat<float>(pBackSector->ceilingheight - gViewZFrac);

    // Add floors and ceilings if the wall needs one
    uint32_t actionbits = 0;

    if (f_floorheight < 0 && (                                      // Is the camera above the floor?
            (frontSector.FloorPic != pBackSector->FloorPic) ||      // Floor texture changed?
            (f_floorheight != b_floorheight) ||                     // Differant height?
            (f_lightlevel != b_lightlevel) ||                       // Differant light?
            (b_ceilingheight == b_floorheight)                      // No thickness line?
        )
    ) {
        curWall.floorheight = curWall.floornewheight = f_floorheight;
        actionbits = (AC_ADDFLOOR | AC_NEWFLOOR);   // Create floor
    }

    if ((f_ceilingpic != -1 || b_ceilingpic != -1) &&       // Normal ceiling?
        (f_ceilingheight > 0 || f_ceilingpic == -1) && (    // Camera below ceiling? Sky ceiling?
            (f_ceilingpic != b_ceilingpic) ||               // New ceiling image?
            (f_ceilingheight != b_ceilingheight) ||         // Differant ceiling height?
            (f_lightlevel != b_lightlevel) ||               // Differant ceiling light?
            (b_ceilingheight == b_floorheight)              // Thin dividing line?
        )
    ) {
        curWall.ceilingheight = curWall.ceilingnewheight = f_ceilingheight;
         
        if (f_ceilingpic == -1) {
            actionbits |= (AC_ADDSKY | AC_NEWCEILING);          // Add sky to the ceiling
        } else {
            actionbits |= (AC_ADDCEILING | AC_NEWCEILING);      // Add ceiling texture
        }
    }
    
    curWall.t_topheight = f_ceilingheight;      // Y coord of the top texture

    // Single sided line? (no back sector) They only have a center texture.
    if (pBackSector == &gEmptySector) {
        curWall.t_texture = getWallAnimTexture(sideDef.midtexture);
        float t_texturemid;
        
        if (lineFlags & ML_DONTPEGBOTTOM) {     
            t_texturemid = f_floorheight + (float) curWall.t_texture->data.height;      // Bottom of texture at bottom
        } else {
            t_texturemid = f_ceilingheight;     // Top of texture at top
        }
        
        t_texturemid += FMath::doomFixed16ToFloat<float>(sideDef.rowoffset);    // Add texture anchor offset

        curWall.t_texturemid = t_texturemid;            // Save the top texture anchor var
        curWall.t_bottomheight = f_floorheight;
        actionbits |= (AC_TOPTEXTURE | AC_SOLIDSIL);    // Draw the middle texture only
    } else {
        // Two sided lines are more tricky since I may be able to see through it.
        // Check if the bottom wall texture is visible?
        if (b_floorheight > f_floorheight) {
            // Draw the bottom texture
            curWall.b_texture = getWallAnimTexture(sideDef.bottomtexture);
            float b_texturemid;
            
            if (lineFlags & ML_DONTPEGBOTTOM) {
                b_texturemid = f_ceilingheight;     // bottom of texture at bottom
            } else {
                b_texturemid = b_floorheight;       // Top of texture at top
            }
            
            b_texturemid += FMath::doomFixed16ToFloat<float>(sideDef.rowoffset);    // Add the adjustment
            curWall.b_texturemid = b_texturemid;
            curWall.b_topheight = curWall.floornewheight = b_floorheight;
            curWall.b_bottomheight = f_floorheight;

            actionbits |= (AC_NEWFLOOR | AC_BOTTOMTEXTURE);     // Generate a floor and bottom wall texture
        }

        if (b_ceilingheight < f_ceilingheight && (f_ceilingpic != -1 || b_ceilingpic != -1)) {  // Ceiling wall without sky
            // Draw the top texture
            curWall.t_texture = getWallAnimTexture(sideDef.toptexture);
            float t_texturemid;
            
            if (lineFlags & ML_DONTPEGTOP) {
                t_texturemid = f_ceilingheight;     // Top of texture at top
            } else {
                t_texturemid = b_ceilingheight + (float) curWall.t_texture->data.height;
            }
            
            t_texturemid += FMath::doomFixed16ToFloat<float>(sideDef.rowoffset);    // Anchor the top texture
            curWall.t_texturemid = t_texturemid;                                    // Save the top texture anchor var
            curWall.t_bottomheight = curWall.ceilingnewheight = b_ceilingheight;

            actionbits |= (AC_NEWCEILING | AC_TOPTEXTURE);      // Generate the top texture
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
    
    curWall.seglightlevel = f_lightlevel;                                                       // Save the light level
    curWall.offset = FMath::doomFixed16ToFloat<float>(sideDef.textureoffset + lineSeg.offset);  // Texture anchor X
    latePrep(curWall, lineSeg, lineAngle);
}

static void transformSegXYToViewSpace(const seg_t& inSeg, DrawSeg& outSeg) noexcept {
    // First convert from fixed point to floats
    outSeg.coords.p1x = FMath::doomFixed16ToFloat<float>(inSeg.v1.x);
    outSeg.coords.p1y = FMath::doomFixed16ToFloat<float>(inSeg.v1.y);
    outSeg.coords.p2x = FMath::doomFixed16ToFloat<float>(inSeg.v2.x);
    outSeg.coords.p2y = FMath::doomFixed16ToFloat<float>(inSeg.v2.y);

    // Transform the seg xy coords by the view position
    const float viewX = gViewX;
    const float viewY = gViewY;
    const float viewSin = gViewSin;
    const float viewCos = gViewCos;

    outSeg.coords.p1x -= viewX;
    outSeg.coords.p1y -= viewY;
    outSeg.coords.p2x -= viewX;
    outSeg.coords.p2y -= viewY;

    // Do rotation by view angle.
    // Rotation matrix formula from: https://en.wikipedia.org/wiki/Rotation_matrix
    const float p1xRot = viewCos * outSeg.coords.p1x - viewSin * outSeg.coords.p1y;
    const float p1yRot = viewSin * outSeg.coords.p1x + viewCos * outSeg.coords.p1y;
    const float p2xRot = viewCos * outSeg.coords.p2x - viewSin * outSeg.coords.p2y;
    const float p2yRot = viewSin * outSeg.coords.p2x + viewCos * outSeg.coords.p2y;

    outSeg.coords.p1x = p1xRot;
    outSeg.coords.p1y = p1yRot;
    outSeg.coords.p2x = p2xRot;
    outSeg.coords.p2y = p2yRot;
}

static bool isViewSpaceSegBackFacing(const DrawSeg& seg) noexcept {
    // Get a 2D normal for the seg (un-normalized)
    const float segP1x = seg.coords.p1x;
    const float segP1y = seg.coords.p1y;
    const float segDirX = seg.coords.p2x - segP1x;
    const float segDirY = seg.coords.p2y - segP1y;
    const float segNormX = segDirY;
    const float segNormY = -segDirX;

    // Get the direction from the seg to the camera
    const float segToViewX = gViewX - segP1x;
    const float segToViewY = gViewY - segP1y;

    // Do a dot product against the seg normal to see what side we are on
    const float dot = segToViewX * segNormX + segToViewY * segNormY;
    return (dot < 0);
}

static void transformSegXYWToClipSpace(DrawSeg& seg) noexcept {
    // Notes:
    //  (1) We treat 'y' as if it were 'z' for the purposes of these calculations, since the
    //      projection matrix has 'z' as the depth value and not y (Doom coord sys). 
    //  (2) We assume that the seg always starts off with an implicit 'w' value of '1'.
    //
    const float y1Orig = seg.coords.p1y;
    const float y2Orig = seg.coords.p2y;

    seg.coords.p1x *= gProjMatrix.r0c0;
    seg.coords.p2x *= gProjMatrix.r0c0;
    seg.coords.p1y *= gProjMatrix.r2c2;
    seg.coords.p2y *= gProjMatrix.r2c2;
    seg.coords.p1w = y1Orig * gProjMatrix.r2c3 + 1.0f;
    seg.coords.p2w = y2Orig * gProjMatrix.r2c3 + 1.0f;
}

//----------------------------------------------------------------------------------------------------------------------
// Clipping functions:
//
// These largely follow the method described at: 
//  https://chaosinmotion.blog/2016/05/22/3d-clipping-in-homogeneous-coordinates/
//
// Notes:
//  (1) I treat Doom's 'y' coordinate as 'z' for all calculations.
//      In most academic and code works 'z' is actually depth so interpreting 'y' as this keeps things
//      more familiar and allows me to just plug the values into existing equations.
//  (2) For 'xc', 'yc', and 'zc' clipspace coordinates, the point will be inside the clip cube if the 
//      following comparisons against clipspace w ('wc') hold:
//          -wc <= xc && xc >= wc
//          -wc <= yc && yc >= wc
//          -wc <= zc && zc >= wc
//----------------------------------------------------------------------------------------------------------------------
static bool clipSegAgainstFrontPlane(DrawSeg& seg) noexcept {
    // The front plane in normalized device coords (NDC) is at z = -1 (back is z = 1)
    // Hence in clipspace it is at -w (back is at +w):
    const float p1y = seg.coords.p1y;
    const float p2y = seg.coords.p2y;
    const float p1w = seg.coords.p1w;
    const float p2w = seg.coords.p2w;

    const bool p1InFront = (p1y >= -p1w);
    const bool p2InFront = (p2y >= -p2w);

    // Easy case: cee if they are on the same side of the clip plane.
    // Reject if both are on the outside, or accept and leave unmodified if both are on the inside:
    if (p1InFront == p2InFront) {
        return p1InFront;
    }

    // We need to clip: start by computing the distance (using the dot product) of p1 and p2
    // to the 4D plane: (0, 0, 1, 1). Again, treat Doom's 'y' coordinate as 'z' here:
    const float p1Dist = p1y + p1w;
    const float p2Dist = p2y + p2w;

    // Compute the time where the intersection would occur
    const float p1ToP2Dist = p2Dist - p1Dist;
    const float t = p1Dist / p1ToP2Dist;

    // Compute the new x and y and texture u for the intersection point via linear interpolation
    const float p1x = seg.coords.p1x;
    const float p2x = seg.coords.p2x;
    const float p1u = seg.p1TexU;
    const float p2u = seg.p2TexU;

    const float newX = p1x + (p2x - p1x) * t;
    const float newY = p1y + (p2y - p1y) * t;
    const float newU = p1u + (p2u - p1u) * t;

    // Save the result of the point we want to move.
    // Note that we set 'w' to '-y' to ensure that 'y' ends up as '-1' in NDC:
    if (p1InFront) {
        seg.coords.p2x = newX;
        seg.coords.p2y = newY;        
        seg.coords.p2w = -seg.coords.p2y;
        seg.p2TexU = newU;
    }
    else {
        seg.coords.p1x = newX;
        seg.coords.p1y = newY;
        seg.coords.p1w = -seg.coords.p1y;
        seg.p1TexU = newU;
    }

    return true;
}

static bool clipSegAgainstLeftPlane(DrawSeg& seg) noexcept {
    // The left plane in normalized device coords (NDC) is at x = -1 (right is x = 1)
    // Hence in clipspace it is at -w (right is at +w):
    const float p1x = seg.coords.p1x;
    const float p2x = seg.coords.p2x;
    const float p1w = seg.coords.p1w;
    const float p2w = seg.coords.p2w;

    const bool p1InFront = (p1x >= -p1w);
    const bool p2InFront = (p2x >= -p2w);

    // Easy case: cee if they are on the same side of the clip plane.
    // Reject if both are on the outside, or accept and leave unmodified if both are on the inside:
    if (p1InFront == p2InFront) {
        return p1InFront;
    }

    // We need to clip: start by computing the distance (using the dot product) of
    // p1 and p2 to the 4D plane: (1, 0, 0, 1):
    const float p1Dist = p1x + p1w;
    const float p2Dist = p2x + p2w;

    // Compute the time where the intersection would occur
    const float p1ToP2Dist = p2Dist - p1Dist;
    const float t = p1Dist / p1ToP2Dist;

    // Compute the new x and y and texture u for the intersection point via linear interpolation
    const float p1y = seg.coords.p1y;
    const float p2y = seg.coords.p2y;
    const float p1u = seg.p1TexU;
    const float p2u = seg.p2TexU;

    const float newX = p1x + (p2x - p1x) * t;
    const float newY = p1y + (p2y - p1y) * t;
    const float newU = p1u + (p2u - p1u) * t;

    // Save the result of the point we want to move.
    // Note that we set 'w' to '-x' to ensure that 'x' ends up as '-1' in NDC:
    if (p1InFront) {
        seg.coords.p2x = newX;
        seg.coords.p2y = newY;
        seg.coords.p2w = -seg.coords.p2x;
        seg.p2TexU = newU;
    }
    else {
        seg.coords.p1x = newX;
        seg.coords.p1y = newY;
        seg.coords.p1w = -seg.coords.p1x;
        seg.p1TexU = newU;
    }

    return true;
}

static bool clipSegAgainstRightPlane(DrawSeg& seg) noexcept {
    // The right plane in normalized device coords (NDC) is at x = 1 (left is x = -1)
    // Hence in clipspace it is at +w (left is at -w):
    const float p1x = seg.coords.p1x;
    const float p2x = seg.coords.p2x;
    const float p1w = seg.coords.p1w;
    const float p2w = seg.coords.p2w;

    const bool p1InFront = (p1x <= p1w);
    const bool p2InFront = (p2x <= p2w);

    // Easy case: cee if they are on the same side of the clip plane.
    // Reject if both are on the outside, or accept and leave unmodified if both are on the inside:
    if (p1InFront == p2InFront) {
        return p1InFront;
    }

    // We need to clip: start by computing the distance (using the dot product) of
    // p1 and p2 to the 4D plane: (-1, 0, 0, 1):
    const float p1Dist = -p1x + p1w;
    const float p2Dist = -p2x + p2w;

    // Compute the time where the intersection would occur
    const float p1ToP2Dist = p2Dist - p1Dist;
    const float t = p1Dist / p1ToP2Dist;

    // Compute the new x and y and texture u for the intersection point via linear interpolation
    const float p1y = seg.coords.p1y;
    const float p2y = seg.coords.p2y;
    const float p1u = seg.p1TexU;
    const float p2u = seg.p2TexU;

    const float newX = p1x + (p2x - p1x) * t;
    const float newY = p1y + (p2y - p1y) * t;
    const float newU = p1u + (p2u - p1u) * t;

    // Save the result of the point we want to move.
    // Note that we set 'w' to 'x' to ensure that 'x' ends up as '+1' in NDC:
    if (p1InFront) {
        seg.coords.p2x = newX;
        seg.coords.p2y = newY;
        seg.coords.p2w = seg.coords.p2x;
        seg.p2TexU = newU;
    }
    else {
        seg.coords.p1x = newX;
        seg.coords.p1y = newY;
        seg.coords.p1w = seg.coords.p1x;
        seg.p1TexU = newU;
    }

    return true;
}

static void doPerspectiveDivisionForSeg(DrawSeg& seg) noexcept {
    // Compute the inverse of w for p1 and p2
    const float w1Inv = 1.0f / seg.coords.p1w;
    const float w2Inv = 1.0f / seg.coords.p2w;
    seg.coords.p1w_inv = w1Inv;
    seg.coords.p2w_inv = w2Inv;

    // Note: don't bother modifying 'w' - it's unused after perspective division
    seg.coords.p1x *= w1Inv;
    seg.coords.p1y *= w1Inv;
    seg.coords.p2x *= w2Inv;
    seg.coords.p2y *= w2Inv;

    seg.coords.p1tz *= w1Inv;
    seg.coords.p1bz *= w1Inv;
    seg.coords.p1tz_back *= w1Inv;
    seg.coords.p1bz_back *= w1Inv;

    seg.coords.p2tz *= w2Inv;
    seg.coords.p2bz *= w2Inv;
    seg.coords.p2tz_back *= w2Inv;
    seg.coords.p2bz_back *= w2Inv;
}

static void transformSegXZToScreenSpace(DrawSeg& seg) noexcept {
    const float screenW = (float) gScreenWidth;
    const float screenH = (float) gScreenHeight;

    // All coords are in the range -1 to +1 now.
    // Bring in the range 0-1 and then expand to screen width and height:
    seg.coords.p1x = (seg.coords.p1x * 0.5f + 0.5f) * screenW;
    seg.coords.p2x = (seg.coords.p2x * 0.5f + 0.5f) * screenW;

    seg.coords.p1tz = (seg.coords.p1tz * 0.5f + 0.5f) * screenH;
    seg.coords.p1bz = (seg.coords.p1bz * 0.5f + 0.5f) * screenH;
    seg.coords.p2tz = (seg.coords.p2tz * 0.5f + 0.5f) * screenH;
    seg.coords.p2bz = (seg.coords.p2bz * 0.5f + 0.5f) * screenH;
    
    seg.coords.p1tz_back = (seg.coords.p1tz_back * 0.5f + 0.5f) * screenH;
    seg.coords.p1bz_back = (seg.coords.p1bz_back * 0.5f + 0.5f) * screenH;
    seg.coords.p2tz_back = (seg.coords.p2tz_back * 0.5f + 0.5f) * screenH;
    seg.coords.p2bz_back = (seg.coords.p2bz_back * 0.5f + 0.5f) * screenH;
}

static void emitWallFragments(const DrawSeg& seg) noexcept {
    // TODO: TEST
    int32_t x1 = (int32_t) seg.coords.p1x;
    int32_t x2 = (int32_t) seg.coords.p2x;

    ASSERT(x1 >= 0);
    ASSERT(x1 < gScreenWidth);
    ASSERT(x2 >= 0);
    ASSERT(x2 < gScreenWidth);

    float y1t = seg.coords.p1tz;
    float y1b = seg.coords.p1bz;
    float y2t = seg.coords.p2tz;
    float y2b = seg.coords.p2bz;
    float w1 = seg.coords.p1w;
    float w1Inv = seg.coords.p1w_inv;
    float w2 = seg.coords.p2w;
    float w2Inv = seg.coords.p2w_inv;

    if (x2 < x1) {
        std::swap(x1, x2);
        std::swap(y1t, y2t);
        std::swap(y1b, y2b);
        std::swap(w1, w2);
        std::swap(w1Inv, w2Inv);
    }

    const uint32_t colCount = (x2 - x1) + 1;

    y1t *= w1Inv;
    y1b *= w1Inv;
    y2t *= w1Inv;
    y2b *= w1Inv;

    const float ytStep = (y2t - y1t) / (float) (colCount - 1);
    const float ybStep = (y2b - y1b) / (float) (colCount - 1);

    for (int32_t x = x1; x <= x2; ++x) {
        const float pixelNumF = (float) (x - x1);
        const float yt = y1t + ytStep * pixelNumF;
        const float yb = y1b + ybStep * pixelNumF;

        int32_t yti = (int32_t) yt;
        int32_t ybi = (int32_t) yb;

        if (yti < 0) {
            yti = 0;

            if (yti > ybi)
                continue;
        }

        if (ybi >= gScreenHeight) {
            ybi = gScreenHeight - 1;

            if (yti > ybi)
                continue;
        }

        for (int32_t y = yti; y <= ybi; ++y) {
            Video::gFrameBuffer[(gScreenYOffset + y) * Video::SCREEN_WIDTH + gScreenXOffset + x] = 0xFFFFFFFFu;
        }
    }
}

void addSegToFrame(const seg_t& seg) noexcept {
    // First transform the seg into viewspace
    DrawSeg drawSeg;
    transformSegXYToViewSpace(seg, drawSeg);

    // Discard any segs that are back facing
    if (isViewSpaceSegBackFacing(drawSeg))
        return;
    
    // Set the U coordinates for the seg.
    // Those will need to be adjusted accordingly if we clip:
    {
        const float segLength = std::sqrtf(
            (drawSeg.coords.p1x * drawSeg.coords.p1x) +
            (drawSeg.coords.p1y * drawSeg.coords.p1y)
        );
        const float uOffset = FMath::doomFixed16ToFloat<float>(seg.offset);
        drawSeg.p1TexU = uOffset;
        drawSeg.p2TexU = uOffset + segLength - 0.001f;      // Note: if a wall is 64 units and the texture is 64 units, never go to 64.0 (always stay to 63.99999 or something like that)
    }

    // Next transform to clip space and clip against the front and left + right planes
    transformSegXYWToClipSpace(drawSeg);

    if (!clipSegAgainstFrontPlane(drawSeg))
        return;
    
    if (!clipSegAgainstLeftPlane(drawSeg))
        return;
    
    if (!clipSegAgainstRightPlane(drawSeg))
        return;
    
    // Now that the seg is not rejected, fill in the height values
    {
        const float frontFloorY = FMath::doomFixed16ToFloat<float>(seg.frontsector->floorheight);
        const float frontCeilY = FMath::doomFixed16ToFloat<float>(seg.frontsector->ceilingheight);

        drawSeg.coords.p1tz = frontCeilY;
        drawSeg.coords.p1bz = frontFloorY;
        drawSeg.coords.p2tz = frontCeilY;
        drawSeg.coords.p2bz = frontFloorY;

        if (seg.backsector) {
            const float backFloorY = FMath::doomFixed16ToFloat<float>(seg.backsector->floorheight);
            const float backCeilY = FMath::doomFixed16ToFloat<float>(seg.backsector->ceilingheight);

            drawSeg.coords.p1tz_back = backCeilY;
            drawSeg.coords.p1bz_back = backFloorY;
            drawSeg.coords.p2tz_back = backCeilY;
            drawSeg.coords.p2bz_back = backFloorY;
        }
        else {
            drawSeg.coords.p1tz_back = 0.0f;
            drawSeg.coords.p1bz_back = 0.0f;
            drawSeg.coords.p2tz_back = 0.0f;
            drawSeg.coords.p2bz_back = 0.0f;
        }
    }

    // Do perspective division and transform the seg to screen space
    doPerspectiveDivisionForSeg(drawSeg);
    transformSegXZToScreenSpace(drawSeg);

    // Emit all wall fragments for the seg
    emitWallFragments(drawSeg);
}

END_NAMESPACE(Renderer)
