#include "Renderer_Internal.h"

#include "Base/FMath.h"
#include "Base/Tables.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "Textures.h"

// TODO: TEST
#include "Blit.h"
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

static bool isScreenSpaceSegBackFacing(const DrawSeg& seg) noexcept {
    return (seg.coords.p1x >= seg.coords.p2x);
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
    seg.coords.p1y = gProjMatrix.r2c2 * y1Orig + gProjMatrix.r2c3;
    seg.coords.p2y = gProjMatrix.r2c2 * y2Orig + gProjMatrix.r2c3;
    seg.coords.p1w = y1Orig;    // Note: r3c2 is an implicit 1.0 - hence we just do this!
    seg.coords.p2w = y2Orig;    // Note: r3c2 is an implicit 1.0 - hence we just do this!
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
    // The left plane in normalized device coords (NDC) is at z = -1, hence in clipspace it is at -w.
    // The distance to the clip plane can be computed by the dot product against the following vector:
    //  (0, 0, 1, 1)
    //
    // Using this, compute the signed distance of the seg points to the plane and see if we should reject
    // due to both points being outside the clip plane or leave unmodified due to both being on the inside:
    //
    const float p1y = seg.coords.p1y;
    const float p2y = seg.coords.p2y;
    const float p1w = seg.coords.p1w;
    const float p2w = seg.coords.p2w;

    const float p1ClipPlaneSDist = p1y + p1w;
    const float p2ClipPlaneSDist = p2y + p2w;
    const bool p1InFront = (p1ClipPlaneSDist > 0);
    const bool p2InFront = (p2ClipPlaneSDist > 0);

    if (p1InFront == p2InFront) {
        return p1InFront;
    }

    // We need to clip: compute the time where the intersection with the clip plane would occur
    const float p1ClipPlaneUDist = std::abs(p1ClipPlaneSDist);
    const float p2ClipPlaneUDist = std::abs(p2ClipPlaneSDist);
    const float t = p1ClipPlaneUDist / (p1ClipPlaneUDist + p2ClipPlaneUDist);

    // Compute the new x and y and texture u for the intersection point via linear interpolation
    const float p1x = seg.coords.p1x;
    const float p2x = seg.coords.p2x;
    const float p1TexX = seg.p1TexX;
    const float p2TexX = seg.p2TexX;

    const float newX = p1x + (p2x - p1x) * t;
    const float newY = p1y + (p2y - p1y) * t;
    const float newTexX = p1TexX + (p2TexX - p1TexX) * t;

    // Save the result of the point we want to move.
    // Note that we set 'w' to '-y' to ensure that 'y' ends up as '-1' in NDC:
    if (p1InFront) {
        seg.coords.p2x = newX;
        seg.coords.p2y = newY;
        seg.coords.p2w = -newY;
        seg.p2TexX = newTexX;
    }
    else {
        seg.coords.p1x = newX;
        seg.coords.p1y = newY;
        seg.coords.p1w = -newY;
        seg.p1TexX = newTexX;
    }

    return true;
}

static bool clipSegAgainstLeftPlane(DrawSeg& seg) noexcept {
    // The left plane in normalized device coords (NDC) is at x = -1, hence in clipspace it is at -w.
    // The distance to the clip plane can be computed by the dot product against the following vector:
    //  (1, 0, 0, 1)
    //
    // Using this, compute the signed distance of the seg points to the plane and see if we should reject
    // due to both points being outside the clip plane or leave unmodified due to both being on the inside:
    //
    const float p1x = seg.coords.p1x;
    const float p2x = seg.coords.p2x;
    const float p1w = seg.coords.p1w;
    const float p2w = seg.coords.p2w;

    const float p1ClipPlaneSDist = p1x + p1w;
    const float p2ClipPlaneSDist = p2x + p2w;
    const bool p1InFront = (p1ClipPlaneSDist > 0);
    const bool p2InFront = (p2ClipPlaneSDist > 0);

    if (p1InFront == p2InFront) {
        return p1InFront;
    }

    // We need to clip: compute the time where the intersection with the clip plane would occur
    const float p1ClipPlaneUDist = std::abs(p1ClipPlaneSDist);
    const float p2ClipPlaneUDist = std::abs(p2ClipPlaneSDist);
    const float t = p1ClipPlaneUDist / (p1ClipPlaneUDist + p2ClipPlaneUDist);

    // Compute the new x and y and texture u for the intersection point via linear interpolation
    const float p1y = seg.coords.p1y;
    const float p2y = seg.coords.p2y;
    const float p1TexX = seg.p1TexX;
    const float p2TexX = seg.p2TexX;

    const float newX = p1x + (p2x - p1x) * t;
    const float newY = p1y + (p2y - p1y) * t;
    const float newTexX = p1TexX + (p2TexX - p1TexX) * t;

    // Save the result of the point we want to move.
    // Note that we set 'w' to '-x' to ensure that 'x' ends up as '-1' in NDC:
    if (p1InFront) {
        seg.coords.p2x = newX;
        seg.coords.p2y = newY;
        seg.coords.p2w = -newX;
        seg.p2TexX = newTexX;
    }
    else {
        seg.coords.p1x = newX;
        seg.coords.p1y = newY;
        seg.coords.p1w = -newX;
        seg.p1TexX = newTexX;
    }

    return true;
}

static bool clipSegAgainstRightPlane(DrawSeg& seg) noexcept {
    // The left plane in normalized device coords (NDC) is at x = 1, hence in clipspace it is at w.
    // The distance to the clip plane can be computed by the dot product against the following vector:
    //  (-1, 0, 0, 1)
    //
    // Using this, compute the signed distance of the seg points to the plane and see if we should reject
    // due to both points being outside the clip plane or leave unmodified due to both being on the inside:
    //
    const float p1x = seg.coords.p1x;
    const float p2x = seg.coords.p2x;
    const float p1w = seg.coords.p1w;
    const float p2w = seg.coords.p2w;

    const float p1ClipPlaneSDist = -p1x + p1w;
    const float p2ClipPlaneSDist = -p2x + p2w;
    const bool p1InFront = (p1ClipPlaneSDist > 0);
    const bool p2InFront = (p2ClipPlaneSDist > 0);

    if (p1InFront == p2InFront) {
        return p1InFront;
    }

    // We need to clip: compute the time where the intersection with the clip plane would occur
    const float p1ClipPlaneUDist = std::abs(p1ClipPlaneSDist);
    const float p2ClipPlaneUDist = std::abs(p2ClipPlaneSDist);
    const float t = p1ClipPlaneUDist / (p1ClipPlaneUDist + p2ClipPlaneUDist);

    // Compute the new x and y and texture u for the intersection point via linear interpolation
    const float p1y = seg.coords.p1y;
    const float p2y = seg.coords.p2y;
    const float p1TexX = seg.p1TexX;
    const float p2TexX = seg.p2TexX;

    const float newX = p1x + (p2x - p1x) * t;
    const float newY = p1y + (p2y - p1y) * t;
    const float newTexX = p1TexX + (p2TexX - p1TexX) * t;

    // Save the result of the point we want to move.
    // Note that we set 'w' to 'x' to ensure that 'x' ends up as '+1' in NDC:
    if (p1InFront) {
        seg.coords.p2x = newX;
        seg.coords.p2y = newY;
        seg.coords.p2w = newX;
        seg.p2TexX = newTexX;
    }
    else {
        seg.coords.p1x = newX;
        seg.coords.p1y = newY;
        seg.coords.p1w = newX;
        seg.p1TexX = newTexX;
    }

    return true;
}

static void addClipSpaceZValuesForSeg(DrawSeg& drawSeg, const seg_t& seg) noexcept {
    const float viewZ = gViewZ;
    const float frontFloorZ = FMath::doomFixed16ToFloat<float>(seg.frontsector->floorheight);
    const float frontCeilZ = FMath::doomFixed16ToFloat<float>(seg.frontsector->ceilingheight);

    drawSeg.coords.p1tz = (frontCeilZ - viewZ) * gProjMatrix.r1c1;
    drawSeg.coords.p1bz = (frontFloorZ - viewZ) * gProjMatrix.r1c1;
    drawSeg.coords.p2tz = (frontCeilZ - viewZ) * gProjMatrix.r1c1;
    drawSeg.coords.p2bz = (frontFloorZ - viewZ) * gProjMatrix.r1c1;

    if (seg.backsector) {
        const float backFloorZ = FMath::doomFixed16ToFloat<float>(seg.backsector->floorheight);
        const float backCeilZ = FMath::doomFixed16ToFloat<float>(seg.backsector->ceilingheight);

        drawSeg.coords.p1tz_back = (backCeilZ - viewZ) * gProjMatrix.r1c1;
        drawSeg.coords.p1bz_back = (backFloorZ - viewZ) * gProjMatrix.r1c1;
        drawSeg.coords.p2tz_back = (backCeilZ - viewZ) * gProjMatrix.r1c1;
        drawSeg.coords.p2bz_back = (backFloorZ - viewZ) * gProjMatrix.r1c1;
    }
    else {
        drawSeg.coords.p1tz_back = 0.0f;
        drawSeg.coords.p1bz_back = 0.0f;
        drawSeg.coords.p2tz_back = 0.0f;
        drawSeg.coords.p2bz_back = 0.0f;
    }    
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
    // Note: have to -1 here because at 100% of the range we don't want to be >= screen width or height!
    const float screenW = (float) gScreenWidth - 1.0f;
    const float screenH = (float) gScreenHeight - 1.0f;

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

//----------------------------------------------------------------------------------------------------------------------
// Flags for what types of wall and floor/ceiling fragments to emit
//----------------------------------------------------------------------------------------------------------------------
typedef uint16_t FragEmitFlagsT;

namespace FragEmitFlags {
    static constexpr FragEmitFlagsT MID_WALL    = 0x0001;       // Emit a single sided wall
    static constexpr FragEmitFlagsT UPPER_WALL  = 0x0002;       // Emit an upper wall
    static constexpr FragEmitFlagsT LOWER_WALL  = 0x0004;       // Emit a lower wall
    static constexpr FragEmitFlagsT FLOOR       = 0x0008;       // Emit a floor
    static constexpr FragEmitFlagsT CEILING     = 0x0010;       // Emit a ceiling
}

//----------------------------------------------------------------------------------------------------------------------
// Clips and emits a wall column
//----------------------------------------------------------------------------------------------------------------------
template <FragEmitFlagsT FLAGS>
static void clipAndEmitWallColumn(
    const uint32_t x,
    const float zt,
    const float zb,
    const float texX,
    const float texTy,
    const float texBy,
    SegClip& clipBounds,
    const ImageData& texImage
) noexcept {
    ASSERT(x < gScreenWidth);

    // Only bother emitting wall fragments if the column height would be > 0
    if (zt >= zb)
        return;
    
    // This is how much to step the texture in the Y direction for this column
    const float texYStep = (texBy - texTy) / (zb - zt);

    // Clip the column against the top and bottom of the current seg clip bounds
    int32_t zbInt = (int32_t) zb;
    int32_t ztInt = (int32_t) zt;

    float curZt = zt;
    float curZb = zb;
    float curTexTy = texTy;
    float curTexBy = texBy;
    float texYSubPixelAdjustment;

    if (ztInt <= clipBounds.top) {
        // Offscreen at the top - clip:
        curZt = (float) clipBounds.top + 1.0f;
        ztInt = (int32_t) curZt;
        const float pixelsOffscreen = curZt - zt;
        curTexTy += texYStep * pixelsOffscreen;
        
        // If the clipped size is now invalid then skip
        if (curZt >= curZb) {
            return;
        }

        // Note: no sub adjustement when we clip, it's already done implicitly as part of clipping
        texYSubPixelAdjustment = 0.0f;
    }
    else {
        // Small adjustment to account for eventual integer rounding of the z coordinate. For more 'solid' and less
        // 'fuzzy' and temporally unstable texture mapping, we need to make adjustments based on the sub pixel y-position
        // of the column. If for example the true pixel Y position is 0.25 units above it's integer position then count
        // 0.25 pixels as already having been 'stepped' and adjust the texture coordinate accordingly:
        texYSubPixelAdjustment = -(curZt - std::trunc(curZt)) * texYStep;
    }

    if (zbInt >= clipBounds.bottom) {
        // Offscreen at the bottom - clip:
        curZb = (float) clipBounds.bottom - 1.0f;
        zbInt = (int32_t) curZb;
        const float pixelsOffscreen = zb - curZb;
        curTexBy -= texYStep * pixelsOffscreen;
        
        // If the clipped size is now invalid then skip
        if (curZt >= curZb) {
            return;
        }
    }

    // Emit the column fragment
    const int32_t columnHeight = zbInt - ztInt + 1;

    {
        WallFragment frag;
        frag.x = x;
        frag.y = (uint16_t) ztInt;
        frag.height = (uint16_t) columnHeight;
        frag.texcoordX = (uint16_t) texX;
        frag.texcoordY = curTexTy;
        frag.texcoordYSubPixelAdjust = texYSubPixelAdjustment;
        frag.texcoordYStep = texYStep;
        frag.lightMul = 1.0f;                   // TODO
        frag.pImageData = &texImage;

        gWallFragments.push_back(frag);
    }

    // Save new clip bounds and if the column is fully filled then mark it so by setting to all zeros (top >= bottom)
    if constexpr (FLAGS == FragEmitFlags::MID_WALL) {
        // Solid walls always gobble up the entire column, nothing renders behind them!
        clipBounds = SegClip{ 0, 0 };
        gNumFullSegCols++;
    } 
    else if constexpr (FLAGS == FragEmitFlags::UPPER_WALL) {                
        if (zbInt + 1 < clipBounds.bottom) {
            clipBounds = SegClip{ (int16_t) zbInt, clipBounds.bottom };
        } else {
            clipBounds = SegClip{ 0, 0 };
            gNumFullSegCols++;                    
        }
    }
    else {
        static_assert(FLAGS == FragEmitFlags::LOWER_WALL);

        if (ztInt - 1 > clipBounds.top) {
            clipBounds = SegClip{ clipBounds.top, (int16_t) ztInt };
        } else {
            clipBounds = SegClip{ 0, 0 };
            gNumFullSegCols++;
        }
    }

    gSegClip[x] = clipBounds;
}

//----------------------------------------------------------------------------------------------------------------------
// Clips and emits a floor or ceiling column
//----------------------------------------------------------------------------------------------------------------------
template <FragEmitFlagsT FLAGS>
static void clipAndEmitFlatColumn(
    const uint32_t x,
    const float zt,
    const float zb,
    SegClip& clipBounds
) noexcept {
    ASSERT(x < gScreenWidth);

    // Only bother emitting wall fragments if the column height would be > 0
    if (zt >= zb)
        return;
    
    // Clip the column against the top and bottom of the current seg clip bounds
    int32_t zbInt = (int32_t) zb;
    int32_t ztInt = (int32_t) zt;

    float curZt = zt;
    float curZb = zb;

    if (ztInt <= clipBounds.top) {
        // Offscreen at the top - clip:
        curZt = (float) clipBounds.top + 1.0f;
        ztInt = (int32_t) curZt;
        
        // If the clipped size is now invalid then skip
        if (curZt >= curZb) {
            return;
        }
    }

    if (zbInt >= clipBounds.bottom) {
        // Offscreen at the bottom - clip:
        curZb = (float) clipBounds.bottom - 1.0f;
        zbInt = (int32_t) curZb;
        
        // If the clipped size is now invalid then skip
        if (curZt >= curZb) {
            return;
        }
    }

    // Emit the floor or ceiling fragment
    const int32_t columnHeight = zbInt - ztInt + 1;

    {
        // TODO: TEMP
        for (int32_t y = ztInt; y <= zbInt; ++y) {
            uint32_t& pixel = Video::gFrameBuffer[(gScreenYOffset + y) * Video::SCREEN_WIDTH + x + gScreenXOffset];

            if constexpr (FLAGS == FragEmitFlags::CEILING) {    
                pixel = 0x3F1F1FFF;
            } else {
                pixel = 0x1F1F3FFF;
            }
        }
    }

    // Save new clip bounds and if the column is fully filled then mark it so by setting to all zeros (top >= bottom)
    if constexpr (FLAGS == FragEmitFlags::CEILING) {
        if (zbInt + 1 < clipBounds.bottom) {
            clipBounds = SegClip{ (int16_t) zbInt, clipBounds.bottom };
        } else {
            clipBounds = SegClip{ 0, 0 };
            gNumFullSegCols++;                    
        }
    }
    else {
        static_assert(FLAGS == FragEmitFlags::FLOOR);

        if (ztInt - 1 > clipBounds.top) {
            clipBounds = SegClip{ clipBounds.top, (int16_t) ztInt };
        } else {
            clipBounds = SegClip{ 0, 0 };
            gNumFullSegCols++;
        }
    }

    gSegClip[x] = clipBounds;
}

//----------------------------------------------------------------------------------------------------------------------
// Emits wall and floor/ceiling fragments for later rendering
//----------------------------------------------------------------------------------------------------------------------
template <FragEmitFlagsT FLAGS>
static void emitWallAndFlatFragments(const DrawSeg& drawSeg, const seg_t seg) noexcept {
    //-----------------------------------------------------------------------------------------------------------------
    // Some setup logic
    //-----------------------------------------------------------------------------------------------------------------

    // For readability's sake:
    constexpr bool EMIT_MID_WALL            = ((FLAGS & FragEmitFlags::MID_WALL) != 0);
    constexpr bool EMIT_UPPER_WALL          = ((FLAGS & FragEmitFlags::UPPER_WALL) != 0);
    constexpr bool EMIT_LOWER_WALL          = ((FLAGS & FragEmitFlags::LOWER_WALL) != 0);
    constexpr bool EMIT_ANY_WALL            = (EMIT_MID_WALL || EMIT_UPPER_WALL || EMIT_LOWER_WALL);
    constexpr bool EMIT_FLOOR               = ((FLAGS & FragEmitFlags::FLOOR) != 0);
    constexpr bool EMIT_CEILING             = ((FLAGS & FragEmitFlags::CEILING) != 0);
    constexpr bool EMIT_FLOOR_OR_CEILING    = EMIT_FLOOR || EMIT_CEILING;

    // Sanity checks, expect p1x to be < p2x - should be ensured externally!
    ASSERT(drawSeg.coords.p1x < drawSeg.coords.p2x);
    
    // Get integer range of the wall
    const int32_t x1 = (int32_t) drawSeg.coords.p1x;
    const int32_t x2 = (int32_t) drawSeg.coords.p2x;

    // Sanity checks: x values should be clipped to be within screen range!
    // x1 should also be >= x2!
    ASSERT(x1 >= 0);
    ASSERT(x1 < (int32_t) gScreenWidth);
    ASSERT(x2 >= 0);
    ASSERT(x2 < (int32_t) gScreenWidth);
    ASSERT(x1 <= x2);
    
    //------------------------------------------------------------------------------------------------------------------
    // Figure out top and bottom z for p1 and p2 for all wall and/or floor & ceiling pieces.
    // Depending on what flags are passed, some of these coordinates may be unused:
    //------------------------------------------------------------------------------------------------------------------
    [[maybe_unused]] float p1UpperTz;
    [[maybe_unused]] float p2UpperTz;
    [[maybe_unused]] float p1UpperBz;
    [[maybe_unused]] float p2UpperBz;
    [[maybe_unused]] float p1LowerTz;
    [[maybe_unused]] float p2LowerTz;
    [[maybe_unused]] float p1LowerBz;
    [[maybe_unused]] float p2LowerBz;

    if constexpr (EMIT_MID_WALL || EMIT_UPPER_WALL || EMIT_CEILING) {
        p1UpperTz = drawSeg.coords.p1tz;
        p2UpperTz = drawSeg.coords.p2tz;
    }

    if constexpr (EMIT_UPPER_WALL) {
        p1UpperBz = drawSeg.coords.p1tz_back;
        p2UpperBz = drawSeg.coords.p2tz_back;
    }

    if constexpr (EMIT_LOWER_WALL) {
        p1LowerTz = drawSeg.coords.p1bz_back;
        p2LowerTz = drawSeg.coords.p2bz_back;
    }

    if constexpr (EMIT_MID_WALL || EMIT_LOWER_WALL || EMIT_FLOOR) {
        p1LowerBz = drawSeg.coords.p1bz;
        p2LowerBz = drawSeg.coords.p2bz;
    }

    //------------------------------------------------------------------------------------------------------------------
    // Grab any required textures
    //------------------------------------------------------------------------------------------------------------------
    [[maybe_unused]] const Texture* pMidTexture;
    [[maybe_unused]] const Texture* pUpperTexture;
    [[maybe_unused]] const Texture* pLowerTexture;

    if constexpr (EMIT_MID_WALL) {
        pMidTexture = getWallTexture(seg.sidedef->midtexture);    
    }

    if constexpr (EMIT_UPPER_WALL) {
        pUpperTexture = getWallTexture(seg.sidedef->toptexture);
    }

    if constexpr (EMIT_LOWER_WALL) {
        pLowerTexture = getWallTexture(seg.sidedef->bottomtexture);
    }
    
    //------------------------------------------------------------------------------------------------------------------
    // Figure out the top 'Y' texture coordinate for all wall columns
    //------------------------------------------------------------------------------------------------------------------
    [[maybe_unused]] float midTexTy;
    [[maybe_unused]] float upperTexTy;
    [[maybe_unused]] float lowerTexTy;

    if constexpr (EMIT_ANY_WALL) {
        // Need the line flags, line def and side def for this
        line_t& lineDef = *seg.linedef;
        const side_t& sideDef = *seg.sidedef;
        const uint32_t lineFlags = lineDef.flags;

        // Populate these values lazily
        [[maybe_unused]] float frontFloorZ;
        [[maybe_unused]] float frontCeilingZ;
        [[maybe_unused]] float backFloorZ;
        [[maybe_unused]] float backCeilingZ;
        [[maybe_unused]] float sideDefRowOffset;
        [[maybe_unused]] bool bBottomTexUnpegged;
        [[maybe_unused]] bool bTopTexUnpegged;

        if constexpr (EMIT_ANY_WALL) {
            sideDefRowOffset = FMath::doomFixed16ToFloat<float>(sideDef.rowoffset);
        } 
        
        if constexpr (EMIT_MID_WALL) {
            frontFloorZ = FMath::doomFixed16ToFloat<float>(seg.frontsector->floorheight);
        }
        
        if constexpr (EMIT_ANY_WALL) {
            frontCeilingZ = FMath::doomFixed16ToFloat<float>(seg.frontsector->ceilingheight);
        }
        
        if constexpr (EMIT_LOWER_WALL) {
            backFloorZ = FMath::doomFixed16ToFloat<float>(seg.backsector->floorheight);
        }
        
        if constexpr (EMIT_UPPER_WALL) {
            backCeilingZ = FMath::doomFixed16ToFloat<float>(seg.backsector->ceilingheight);
        }
        
        if constexpr (EMIT_MID_WALL || EMIT_LOWER_WALL) {
            bBottomTexUnpegged = ((lineFlags & ML_DONTPEGBOTTOM) != 0);
        }

        if constexpr (EMIT_UPPER_WALL) {
            bTopTexUnpegged = ((lineFlags & ML_DONTPEGTOP) != 0);
        }

        // Figure out the texture anchor
        if constexpr (EMIT_MID_WALL) {
            const float texH = (float) pMidTexture->data.height;
            const float texAnchor = bBottomTexUnpegged ? frontFloorZ + texH : frontCeilingZ;
            midTexTy = texAnchor + sideDefRowOffset - frontCeilingZ;

            if (midTexTy < 0) {
                midTexTy += texH;       // DC: This is required for correct vertical alignment in some cases
            }
        }

        if constexpr (EMIT_UPPER_WALL) {
            const float texH = (float) pUpperTexture->data.height;
            const float texAnchor = bTopTexUnpegged ? frontCeilingZ : backCeilingZ + texH;
            upperTexTy = texAnchor + sideDefRowOffset - frontCeilingZ;

            if (upperTexTy < 0) {
                upperTexTy += texH;     // DC: This is required for correct vertical alignment in some cases
            }
        } 

        if constexpr (EMIT_LOWER_WALL) {
            const float texH = (float) pLowerTexture->data.height;
            const float texAnchor = bBottomTexUnpegged ? frontCeilingZ : backFloorZ;
            lowerTexTy = texAnchor + sideDefRowOffset - backFloorZ;

            if (lowerTexTy < 0) {
                lowerTexTy += texH;     // DC: This is required for correct vertical alignment in some cases
            }
        }
    }

    //------------------------------------------------------------------------------------------------------------------
    // Figure out the world top and bottom Y values for all the wall pieces.
    // From this we can figure out the bottom Y texture coordinate for the wall pieces.
    //------------------------------------------------------------------------------------------------------------------
    [[maybe_unused]] float upperWorldTz;
    [[maybe_unused]] float upperWorldBz;
    [[maybe_unused]] float lowerWorldTz;
    [[maybe_unused]] float lowerWorldBz;

    if constexpr (EMIT_MID_WALL || EMIT_UPPER_WALL || EMIT_CEILING) {
        upperWorldTz = FMath::doomFixed16ToFloat<float>(seg.frontsector->ceilingheight);
    }

    if constexpr (EMIT_UPPER_WALL) {
        upperWorldBz = FMath::doomFixed16ToFloat<float>(seg.backsector->ceilingheight);
    }

    if constexpr (EMIT_LOWER_WALL) {
        lowerWorldTz = FMath::doomFixed16ToFloat<float>(seg.backsector->floorheight);
    }

    if constexpr (EMIT_MID_WALL || EMIT_LOWER_WALL || EMIT_FLOOR) {
        lowerWorldBz = FMath::doomFixed16ToFloat<float>(seg.frontsector->floorheight);
    }

    //------------------------------------------------------------------------------------------------------------------
    // Figure out the bottom texture 'Y' values for each wall piece
    //------------------------------------------------------------------------------------------------------------------
    [[maybe_unused]] float midTexBy;
    [[maybe_unused]] float upperTexBy;
    [[maybe_unused]] float lowerTexBy;

    constexpr float BOTTOM_TEX_Y_ADJUST = -0.0001f;     // Note: move the coords up a little so we don't skip past the last pixel

    if constexpr (EMIT_MID_WALL) {
        const float worldH = upperWorldTz - lowerWorldBz;
        midTexBy = midTexTy + worldH + BOTTOM_TEX_Y_ADJUST;
    }

    if constexpr (EMIT_UPPER_WALL) {
        const float worldH = upperWorldTz - upperWorldBz;
        upperTexBy = upperTexTy + worldH + BOTTOM_TEX_Y_ADJUST;
    }

    if constexpr (EMIT_LOWER_WALL) {
        const float worldH = lowerWorldTz - lowerWorldBz;
        lowerTexBy = lowerTexTy + worldH + BOTTOM_TEX_Y_ADJUST;
    }
    
    //------------------------------------------------------------------------------------------------------------------
    // Figure out how what to start at and much to step for each x pixel for various quantities.
    // Note that all quantities except for 'z' values, and '1/w' must be first divided by 'w' prior to interpolation,
    // and then divided by interpolated '1/w' again to extract the perspective correct value at each pixel. 'z' does
    // not need to be interpolated because we will always have a straight line for the top and bottom edges and '1/w'
    // is also ok to interpolate because it is based on depth.
    //------------------------------------------------------------------------------------------------------------------
    const float xRange = (drawSeg.coords.p2x - drawSeg.coords.p1x);
    const float xRangeDivider =  1.0f / xRange;

    // 1/w (sort of inverse depth)
    const float p1InvW = 1.0f / drawSeg.coords.p1w;
    const float p2InvW = 1.0f / drawSeg.coords.p2w;
    const float invWStep = (p2InvW - p1InvW) * xRangeDivider;

    // Normalized depth
    const float p1y = drawSeg.coords.p1y * p1InvW;
    const float p2y = drawSeg.coords.p2y * p2InvW;
    const float yStep = (p2y - p1y) * xRangeDivider;

    // X texture coordinate (for walls)
    [[maybe_unused]] float p1TexX;
    [[maybe_unused]] float p2TexX;
    [[maybe_unused]] float texXStep;

    if constexpr (EMIT_ANY_WALL) {
        p1TexX = drawSeg.p1TexX * p1InvW;
        p2TexX = drawSeg.p2TexX * p2InvW;
        texXStep = (p2TexX - p1TexX) * xRangeDivider;
    }

    // Bottom and top Z stepping values for walls and floors
    [[maybe_unused]] float upperTzStep;
    [[maybe_unused]] float upperBzStep;
    [[maybe_unused]] float lowerTzStep;
    [[maybe_unused]] float lowerBzStep;

    if constexpr (EMIT_MID_WALL || EMIT_UPPER_WALL || EMIT_CEILING) {
        upperTzStep = (p2UpperTz - p1UpperTz) * xRangeDivider;
    }

    if constexpr (EMIT_UPPER_WALL) {
        upperBzStep = (p2UpperBz - p1UpperBz) * xRangeDivider;
    }

    if constexpr (EMIT_LOWER_WALL) {
        lowerTzStep = (p2LowerTz - p1LowerTz) * xRangeDivider;
    }

    if constexpr (EMIT_MID_WALL || EMIT_LOWER_WALL || EMIT_FLOOR) {
        lowerBzStep = (p2LowerBz - p1LowerBz) * xRangeDivider;
    }

    //------------------------------------------------------------------------------------------------------------------
    // The number of steps in the x direction we have made.
    // Increment on each column.
    //
    // Note: we also do a sub pixel adjustement based on the fractional x position of the pixel.
    // This helps ensure stability and prevents 'wiggle' as the camera moves about.
    // This is similar to the stability adjustment we do for the V texture coordinate on walls.
    //------------------------------------------------------------------------------------------------------------------
    float curXStepCount = 0.0f;
    float nextXStepCount = -(drawSeg.coords.p1x - (float) x1);      // Adjustement for sub-pixel pos to prevent wiggle

    //------------------------------------------------------------------------------------------------------------------
    // Emit each column
    //------------------------------------------------------------------------------------------------------------------
    const float viewH = (float) gScreenHeight;

    for (int32_t x = x1; x <= x2; ++x) {
        //--------------------------------------------------------------------------------------------------------------
        // Grab the clip bounds for this column.
        // If it is fully filled (top >= bottom) then just skip over it and go to the next step
        //--------------------------------------------------------------------------------------------------------------
        SegClip clipBounds = gSegClip[x];

        if (clipBounds.top >= clipBounds.bottom) {
            nextXStepCount += 1.0f;
            curXStepCount = nextXStepCount;
            continue;
        }

        //--------------------------------------------------------------------------------------------------------------
        // Do stepping for this column and figure out these quantities following through simple linear interpolation.
        // Do perspective correct adjustments where required.
        //--------------------------------------------------------------------------------------------------------------

        // 1/w and w
        const float wInv = p1InvW + invWStep * curXStepCount;
        const float w = 1.0f / wInv;

        // Normalized depth
        const float y = (p1y + yStep * curXStepCount) * w;

        // X texture coordinate (for walls)
        [[maybe_unused]] float texX;
        
        if constexpr (EMIT_ANY_WALL) {
            texX = (p1TexX + texXStep * curXStepCount) * w;
        }

        // Bottom and top Z values for walls and floors
        [[maybe_unused]] float upperTz;
        [[maybe_unused]] float upperBz;
        [[maybe_unused]] float lowerTz;
        [[maybe_unused]] float lowerBz;

        if constexpr (EMIT_MID_WALL || EMIT_UPPER_WALL || EMIT_CEILING) {
            upperTz = viewH - (p1UpperTz + upperTzStep * curXStepCount);
        }

        if constexpr (EMIT_UPPER_WALL) {
            upperBz = viewH - (p1UpperBz + upperBzStep * curXStepCount);
        }

        if constexpr (EMIT_LOWER_WALL) {
            lowerTz = viewH - (p1LowerTz + lowerTzStep * curXStepCount);
        }

        if constexpr (EMIT_MID_WALL || EMIT_LOWER_WALL || EMIT_FLOOR) {
            lowerBz = viewH - (p1LowerBz + lowerBzStep * curXStepCount);
        }

        //--------------------------------------------------------------------------------------------------------------
        // Move onto the next pixel for future steps
        //--------------------------------------------------------------------------------------------------------------
        nextXStepCount += 1.0f;
        curXStepCount = nextXStepCount;

        //--------------------------------------------------------------------------------------------------------------
        // Emit all fragments
        //--------------------------------------------------------------------------------------------------------------
        if constexpr (EMIT_CEILING) {
            clipAndEmitFlatColumn<FragEmitFlags::CEILING>(
                x,
                0.0f,
                upperTz,
                clipBounds
            );
        }

        if constexpr (EMIT_FLOOR) {
            clipAndEmitFlatColumn<FragEmitFlags::FLOOR>(
                x,
                lowerBz,
                viewH - 1.0f,
                clipBounds
            );
        }

        if constexpr (EMIT_MID_WALL) {
            clipAndEmitWallColumn<FragEmitFlags::MID_WALL>(
                x,
                upperTz,
                lowerBz,
                texX,
                midTexTy,
                midTexBy,
                clipBounds,
                pMidTexture->data
            );
        }

        if constexpr (EMIT_UPPER_WALL) {
            clipAndEmitWallColumn<FragEmitFlags::UPPER_WALL>(
                x,
                upperTz,
                upperBz,
                texX,
                upperTexTy,
                upperTexBy,
                clipBounds,
                pUpperTexture->data
            );
        }

        if constexpr (EMIT_LOWER_WALL) {
            clipAndEmitWallColumn<FragEmitFlags::LOWER_WALL>(
                x,
                lowerTz,
                lowerBz,
                texX,
                lowerTexTy,
                lowerTexBy,
                clipBounds,
                pLowerTexture->data
            );
        }
    }
}

void addSegToFrame(const seg_t& seg) noexcept {
    // First transform the seg into viewspace
    DrawSeg drawSeg;
    transformSegXYToViewSpace(seg, drawSeg);
    
    // Set the U coordinates for the seg.
    // Those will need to be adjusted accordingly if we clip:
    {
        const float segdx = FMath::doomFixed16ToFloat<float>(seg.v2.x - seg.v1.x);
        const float segdy = FMath::doomFixed16ToFloat<float>(seg.v2.y - seg.v1.y);
        const float segLength = std::sqrtf(segdx * segdx + segdy * segdy);
        const float texXOffset = FMath::doomFixed16ToFloat<float>(seg.offset + seg.sidedef->textureoffset);

        drawSeg.p1TexX = texXOffset;
        drawSeg.p2TexX = texXOffset + segLength - 0.001f;   // Note: if a wall is 64 units and the texture is 64 units, never go to 64.0 (always stay to 63.99999 or something like that)
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
    addClipSpaceZValuesForSeg(drawSeg, seg);

    // Do perspective division and transform the seg to screen space
    doPerspectiveDivisionForSeg(drawSeg);
    transformSegXZToScreenSpace(drawSeg);

    // If the seg is a zero sized or invalid range then abort
    if (drawSeg.coords.p1x >= drawSeg.coords.p2x)
        return;

    // Discard any segs that are back facing
    const bool bIsBackFacing = isScreenSpaceSegBackFacing(drawSeg);

    // Emit all wall and floor fragments for the seg
    bool bIsLineSeen = false;

    if (!seg.backsector) {
        // We only emit fragments for solid walls if NOT back facing
        if (!bIsBackFacing) {
            emitWallAndFlatFragments<
                FragEmitFlags::MID_WALL |
                FragEmitFlags::CEILING |
                FragEmitFlags::FLOOR
            >(drawSeg, seg);
        }
    } else {
        if (!bIsBackFacing) {
            emitWallAndFlatFragments<
                FragEmitFlags::UPPER_WALL |
                FragEmitFlags::LOWER_WALL |
                FragEmitFlags::CEILING |
                FragEmitFlags::FLOOR
            >(drawSeg, seg);
        } else {
            emitWallAndFlatFragments<
                FragEmitFlags::CEILING |
                FragEmitFlags::FLOOR
            >(drawSeg, seg);
        }
    }

    // Grab the flags for the seg's linedef and mark it as seen (if visible)
    if (bIsLineSeen) {
        line_t& lineDef = *seg.linedef;
        lineDef.flags |= ML_MAPPED;
    }
}

END_NAMESPACE(Renderer)
