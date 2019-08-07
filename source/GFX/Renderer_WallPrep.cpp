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
// Minimum depth at which we will begin clamping the position of the first pixel in a flat column.
// Restricting this adjustment to larger depths becase:
//  (1) It causes noticeable temporal aliasing when moving around in some cases, at close depths.
//  (2) Far depths are where we get the noticeable issues with flat textures overstepping their
//      bounds and wrapping erroneously. Things are far more accurate closer to the view, so there
//      is no need for this adjustment at near depths...
//----------------------------------------------------------------------------------------------------------------------
static constexpr float MIN_DEPTH_FOR_FLAT_PIXEL_CLAMP = 128.0f;

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

//----------------------------------------------------------------------------------------------------------------------
// Reverse the draw seg by swapping p1 and p2.
// Useful for fixing up back facing segs so we can deal with them in the same way as front facing segs.
//----------------------------------------------------------------------------------------------------------------------
static void swapDrawSegP1AndP2(DrawSeg& seg) noexcept {
    std::swap(seg.p1x,          seg.p2x);
    std::swap(seg.p1y,          seg.p2y);
    std::swap(seg.p1w,          seg.p2w);
    std::swap(seg.p1w_inv,      seg.p2w_inv);
    std::swap(seg.p1tz,         seg.p2tz);
    std::swap(seg.p1bz,         seg.p2bz);
    std::swap(seg.p1tz_back,    seg.p2tz_back);
    std::swap(seg.p1bz_back,    seg.p2bz_back);
    std::swap(seg.p1TexX,       seg.p2TexX);
    std::swap(seg.p1WorldX,     seg.p2WorldX);
    std::swap(seg.p1WorldY,     seg.p2WorldY);
}

//----------------------------------------------------------------------------------------------------------------------
// Populate vertex attributes for the given seg that are interpolated across the seg during rendering.
// These attributes are not affected by any transforms, but *ARE* clipped.
//----------------------------------------------------------------------------------------------------------------------
static void populateSegVertexAttribs(const seg_t& seg, DrawSeg& drawSeg) noexcept {
    // Set the texture 'X' coordinates for the seg
    {
        const float segdx = FMath::doomFixed16ToFloat<float>(seg.v2.x - seg.v1.x);
        const float segdy = FMath::doomFixed16ToFloat<float>(seg.v2.y - seg.v1.y);
        const float segLength = std::sqrtf(segdx * segdx + segdy * segdy);
        const float texXOffset = FMath::doomFixed16ToFloat<float>(seg.offset + seg.sidedef->textureoffset);

        drawSeg.p1TexX = texXOffset;
        drawSeg.p2TexX = texXOffset + segLength - 0.001f;   // Note: if a wall is 64 units and the texture is 64 units, never go to 64.0 (always stay to 63.99999 or something like that)
    }

    // Set the world x and y positions for the seg's two points
    drawSeg.p1WorldX = FMath::doomFixed16ToFloat<float>(seg.v1.x);
    drawSeg.p1WorldY = FMath::doomFixed16ToFloat<float>(seg.v1.y);
    drawSeg.p2WorldX = FMath::doomFixed16ToFloat<float>(seg.v2.x);
    drawSeg.p2WorldY = FMath::doomFixed16ToFloat<float>(seg.v2.y);
}

//----------------------------------------------------------------------------------------------------------------------
// Transforms the XY coordinates for the seg into view space
//----------------------------------------------------------------------------------------------------------------------
static void transformSegXYToViewSpace(const seg_t& inSeg, DrawSeg& outSeg) noexcept {
    // First convert from fixed point to floats
    outSeg.p1x = FMath::doomFixed16ToFloat<float>(inSeg.v1.x);
    outSeg.p1y = FMath::doomFixed16ToFloat<float>(inSeg.v1.y);
    outSeg.p2x = FMath::doomFixed16ToFloat<float>(inSeg.v2.x);
    outSeg.p2y = FMath::doomFixed16ToFloat<float>(inSeg.v2.y);

    // Transform the seg xy coords by the view position
    const float viewX = gViewX;
    const float viewY = gViewY;
    const float viewSin = gViewSin;
    const float viewCos = gViewCos;

    outSeg.p1x -= viewX;
    outSeg.p1y -= viewY;
    outSeg.p2x -= viewX;
    outSeg.p2y -= viewY;

    // Do rotation by view angle.
    // Rotation matrix formula from: https://en.wikipedia.org/wiki/Rotation_matrix
    const float p1xRot = viewCos * outSeg.p1x - viewSin * outSeg.p1y;
    const float p1yRot = viewSin * outSeg.p1x + viewCos * outSeg.p1y;
    const float p2xRot = viewCos * outSeg.p2x - viewSin * outSeg.p2y;
    const float p2yRot = viewSin * outSeg.p2x + viewCos * outSeg.p2y;

    outSeg.p1x = p1xRot;
    outSeg.p1y = p1yRot;
    outSeg.p2x = p2xRot;
    outSeg.p2y = p2yRot;
}

static bool isScreenSpaceSegBackFacing(const DrawSeg& seg) noexcept {
    // Front facing segs are always left to right when drawn, so if it's opposite way then it's a back facing seg!
    return (seg.p1x >= seg.p2x);
}

//----------------------------------------------------------------------------------------------------------------------
// Transforms the XY and W coordinates for the seg into clip space
//----------------------------------------------------------------------------------------------------------------------
static void transformSegXYWToClipSpace(DrawSeg& seg) noexcept {
    // Notes:
    //  (1) We treat 'y' as if it were 'z' for the purposes of these calculations, since the
    //      projection matrix has 'z' as the depth value and not y (Doom coord sys). 
    //  (2) We assume that the seg always starts off with an implicit 'w' value of '1'.
    const float y1Orig = seg.p1y;
    const float y2Orig = seg.p2y;

    seg.p1x *= gProjMatrix.r0c0;
    seg.p2x *= gProjMatrix.r0c0;
    seg.p1y = gProjMatrix.r2c2 * y1Orig + gProjMatrix.r2c3;
    seg.p2y = gProjMatrix.r2c2 * y2Orig + gProjMatrix.r2c3;
    seg.p1w = y1Orig;   // Note: r3c2 is an implicit 1.0 - hence we just do this!
    seg.p2w = y2Orig;   // Note: r3c2 is an implicit 1.0 - hence we just do this!
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
    const float p1y = seg.p1y;
    const float p2y = seg.p2y;
    const float p1w = seg.p1w;
    const float p2w = seg.p2w;

    const float p1ClipPlaneSDist = p1y + p1w;
    const float p2ClipPlaneSDist = p2y + p2w;
    const bool p1InFront = (p1ClipPlaneSDist >= 0);
    const bool p2InFront = (p2ClipPlaneSDist >= 0);

    if (p1InFront == p2InFront) {
        return p1InFront;
    }

    // We need to clip: compute the time where the intersection with the clip plane would occur
    const float p1ClipPlaneUDist = std::abs(p1ClipPlaneSDist);
    const float p2ClipPlaneUDist = std::abs(p2ClipPlaneSDist);
    const float t = p1ClipPlaneUDist / (p1ClipPlaneUDist + p2ClipPlaneUDist);

    // Compute the new x and y and vertex attribs for the intersection point via linear interpolation
    const float newX = FMath::lerp(seg.p1x, seg.p2x, t);
    const float newY = FMath::lerp(p1y, p2y, t);
    const float newTexX = FMath::lerp(seg.p1TexX, seg.p2TexX, t);
    const float newWorldX = FMath::lerp(seg.p1WorldX, seg.p2WorldX, t);
    const float newWorldY = FMath::lerp(seg.p1WorldY, seg.p2WorldY, t);

    // Save the result of the point we want to move.
    // Note that we set 'w' to '-y' to ensure that 'y' ends up as '-1' in NDC:
    if (p1InFront) {
        seg.p2x = newX;
        seg.p2y = newY;
        seg.p2w = -newY;
        seg.p2TexX = newTexX;
        seg.p2WorldX = newWorldX;
        seg.p2WorldY = newWorldY;
    }
    else {
        seg.p1x = newX;
        seg.p1y = newY;
        seg.p1w = -newY;
        seg.p1TexX = newTexX;
        seg.p1WorldX = newWorldX;
        seg.p1WorldY = newWorldY;
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
    const float p1x = seg.p1x;
    const float p2x = seg.p2x;
    const float p1w = seg.p1w;
    const float p2w = seg.p2w;

    const float p1ClipPlaneSDist = p1x + p1w;
    const float p2ClipPlaneSDist = p2x + p2w;
    const bool p1InFront = (p1ClipPlaneSDist >= 0);
    const bool p2InFront = (p2ClipPlaneSDist >= 0);

    if (p1InFront == p2InFront) {
        return p1InFront;
    }

    // We need to clip: compute the time where the intersection with the clip plane would occur
    const float p1ClipPlaneUDist = std::abs(p1ClipPlaneSDist);
    const float p2ClipPlaneUDist = std::abs(p2ClipPlaneSDist);
    const float t = p1ClipPlaneUDist / (p1ClipPlaneUDist + p2ClipPlaneUDist);

    // Compute the new x and y and vertex attribs for the intersection point via linear interpolation
    const float newX = FMath::lerp(p1x, p2x, t);
    const float newY = FMath::lerp(seg.p1y, seg.p2y, t);
    const float newTexX = FMath::lerp(seg.p1TexX, seg.p2TexX, t);
    const float newWorldX = FMath::lerp(seg.p1WorldX, seg.p2WorldX, t);
    const float newWorldY = FMath::lerp(seg.p1WorldY, seg.p2WorldY, t);

    // Save the result of the point we want to move.
    // Note that we set 'w' to '-x' to ensure that 'x' ends up as '-1' in NDC:
    if (p1InFront) {
        seg.p2x = newX;
        seg.p2y = newY;
        seg.p2w = -newX;
        seg.p2TexX = newTexX;
        seg.p2WorldX = newWorldX;
        seg.p2WorldY = newWorldY;
    }
    else {
        seg.p1x = newX;
        seg.p1y = newY;
        seg.p1w = -newX;
        seg.p1TexX = newTexX;
        seg.p1WorldX = newWorldX;
        seg.p1WorldY = newWorldY;
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
    const float p1x = seg.p1x;
    const float p2x = seg.p2x;
    const float p1w = seg.p1w;
    const float p2w = seg.p2w;

    const float p1ClipPlaneSDist = -p1x + p1w;
    const float p2ClipPlaneSDist = -p2x + p2w;
    const bool p1InFront = (p1ClipPlaneSDist >= 0);
    const bool p2InFront = (p2ClipPlaneSDist >= 0);

    if (p1InFront == p2InFront) {
        return p1InFront;
    }

    // We need to clip: compute the time where the intersection with the clip plane would occur
    const float p1ClipPlaneUDist = std::abs(p1ClipPlaneSDist);
    const float p2ClipPlaneUDist = std::abs(p2ClipPlaneSDist);
    const float t = p1ClipPlaneUDist / (p1ClipPlaneUDist + p2ClipPlaneUDist);

    // Compute the new x and y and vertex attribs for the intersection point via linear interpolation
    const float newX = FMath::lerp(p1x, p2x, t);
    const float newY = FMath::lerp(seg.p1y, seg.p2y, t);
    const float newTexX = FMath::lerp(seg.p1TexX, seg.p2TexX, t);
    const float newWorldX = FMath::lerp(seg.p1WorldX, seg.p2WorldX, t);
    const float newWorldY = FMath::lerp(seg.p1WorldY, seg.p2WorldY, t);

    // Save the result of the point we want to move.
    // Note that we set 'w' to 'x' to ensure that 'x' ends up as '+1' in NDC:
    if (p1InFront) {
        seg.p2x = newX;
        seg.p2y = newY;
        seg.p2w = newX;
        seg.p2TexX = newTexX;
        seg.p2WorldX = newWorldX;
        seg.p2WorldY = newWorldY;
    }
    else {
        seg.p1x = newX;
        seg.p1y = newY;
        seg.p1w = newX;
        seg.p1TexX = newTexX;
        seg.p1WorldX = newWorldX;
        seg.p1WorldY = newWorldY;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Add the clip space Z (height) values to the seg.
// We add these lazily after other clipping operations have succeeded.
//----------------------------------------------------------------------------------------------------------------------
static void addClipSpaceZValuesForSeg(DrawSeg& drawSeg, const seg_t& seg) noexcept {
    const float viewZ = gViewZ;

    const float frontFloorZ = FMath::doomFixed16ToFloat<float>(seg.frontsector->floorheight);
    const float frontCeilZ = FMath::doomFixed16ToFloat<float>(seg.frontsector->ceilingheight);    
    const float frontFloorViewZ = frontFloorZ - viewZ;
    const float frontCeilViewZ = frontCeilZ - viewZ;
    
    drawSeg.bEmitCeiling = (frontCeilViewZ > 0.0f);
    drawSeg.bEmitFloor = (frontFloorViewZ < 0.0f);

    drawSeg.p1tz = frontCeilViewZ * gProjMatrix.r1c1;
    drawSeg.p1bz = frontFloorViewZ * gProjMatrix.r1c1;
    drawSeg.p2tz = frontCeilViewZ * gProjMatrix.r1c1;
    drawSeg.p2bz = frontFloorViewZ * gProjMatrix.r1c1;

    if (seg.backsector) {
        const float backFloorZ = FMath::doomFixed16ToFloat<float>(seg.backsector->floorheight);
        const float backCeilZ = FMath::doomFixed16ToFloat<float>(seg.backsector->ceilingheight);
        const float backFloorViewZ = backFloorZ - viewZ;
        const float backCeilViewZ = backCeilZ - viewZ;

        drawSeg.p1tz_back = backCeilViewZ * gProjMatrix.r1c1;
        drawSeg.p1bz_back = backFloorViewZ * gProjMatrix.r1c1;
        drawSeg.p2tz_back = backCeilViewZ * gProjMatrix.r1c1;
        drawSeg.p2bz_back = backFloorViewZ * gProjMatrix.r1c1;        

        // Whether to emit upper and lower wall occluders
        const float clipFloorZ = std::fmax(frontFloorZ, backFloorZ);

        if (clipFloorZ < backCeilZ) {
            // Not a closed door, crusher etc.
            if (frontFloorZ < backFloorZ) {
                drawSeg.bEmitLowerOccluder = (viewZ <= backFloorZ);
                drawSeg.bLowerOccluderUsesBackZ = true;
            } 
            else if (frontFloorZ > backFloorZ) {
                drawSeg.bEmitLowerOccluder = (viewZ >= backFloorZ);
                drawSeg.bLowerOccluderUsesBackZ = false;
            } 
            else {
                drawSeg.bEmitLowerOccluder = false;
            }

            if (frontCeilZ < backCeilZ) {
                drawSeg.bEmitUpperOccluder = (viewZ <= backCeilZ);
                drawSeg.bUpperOccluderUsesBackZ = false;
            } 
            else if (frontCeilZ > backCeilZ) {
                drawSeg.bEmitUpperOccluder = (viewZ >= backCeilZ);
                drawSeg.bUpperOccluderUsesBackZ = true;
            } 
            else {
                drawSeg.bEmitUpperOccluder = false;
            }
        } else {
            // Closed door or crusher
            drawSeg.bEmitLowerOccluder = true;
            drawSeg.bLowerOccluderUsesBackZ = true;
            drawSeg.bEmitUpperOccluder = true;
            drawSeg.bUpperOccluderUsesBackZ = true;
        }
    } else {
        drawSeg.p1tz_back = 0.0f;
        drawSeg.p1bz_back = 0.0f;
        drawSeg.p2tz_back = 0.0f;
        drawSeg.p2bz_back = 0.0f;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Do the perspective division for the seg.
// This transforms the seg coordinates into normalized device coords.
//----------------------------------------------------------------------------------------------------------------------
static void doPerspectiveDivisionForSeg(DrawSeg& seg) noexcept {
    // Compute the inverse of w for p1 and p2
    const float w1Inv = 1.0f / seg.p1w;
    const float w2Inv = 1.0f / seg.p2w;
    seg.p1w_inv = w1Inv;
    seg.p2w_inv = w2Inv;

    // Note: don't bother modifying 'w' - it's unused after perspective division
    seg.p1x *= w1Inv;
    seg.p1y *= w1Inv;

    seg.p2x *= w2Inv;
    seg.p2y *= w2Inv;

    seg.p1tz *= w1Inv;
    seg.p1bz *= w1Inv;
    seg.p1tz_back *= w1Inv;
    seg.p1bz_back *= w1Inv;

    seg.p2tz *= w2Inv;
    seg.p2bz *= w2Inv;
    seg.p2tz_back *= w2Inv;
    seg.p2bz_back *= w2Inv;
}

//----------------------------------------------------------------------------------------------------------------------
// Transform the seg xz coordinates from normalized device coords into screen pixel coords.
//----------------------------------------------------------------------------------------------------------------------
static void transformSegXZToScreenSpace(DrawSeg& seg) noexcept {
    // Note: have to subtract a bit here because at 100% of the range we don't want to be >= screen width or height!
    const float screenW = (float) gScreenWidth - 0.5f;
    const float screenH = (float) gScreenHeight - 0.5f;

    // All coords are in the range -1 to +1 now.
    // Bring in the range 0-1 and then expand to screen width and height:
    seg.p1x = (seg.p1x * 0.5f + 0.5f) * screenW;
    seg.p2x = (seg.p2x * 0.5f + 0.5f) * screenW;

    seg.p1tz = (seg.p1tz * 0.5f + 0.5f) * screenH;
    seg.p1bz = (seg.p1bz * 0.5f + 0.5f) * screenH;
    seg.p2tz = (seg.p2tz * 0.5f + 0.5f) * screenH;
    seg.p2bz = (seg.p2bz * 0.5f + 0.5f) * screenH;
    
    seg.p1tz_back = (seg.p1tz_back * 0.5f + 0.5f) * screenH;
    seg.p1bz_back = (seg.p1bz_back * 0.5f + 0.5f) * screenH;
    seg.p2tz_back = (seg.p2tz_back * 0.5f + 0.5f) * screenH;
    seg.p2bz_back = (seg.p2bz_back * 0.5f + 0.5f) * screenH;
}

//----------------------------------------------------------------------------------------------------------------------
// Flags for what types of wall and floor/ceiling fragments to emit
//----------------------------------------------------------------------------------------------------------------------
typedef uint16_t FragEmitFlagsT;

namespace FragEmitFlags {
    static constexpr FragEmitFlagsT MID_WALL                = 0x0001;       // Emit a single sided wall
    static constexpr FragEmitFlagsT UPPER_WALL              = 0x0002;       // Emit an upper wall
    static constexpr FragEmitFlagsT LOWER_WALL              = 0x0004;       // Emit a lower wall
    static constexpr FragEmitFlagsT FLOOR                   = 0x0008;       // Emit a floor
    static constexpr FragEmitFlagsT CEILING                 = 0x0010;       // Emit a ceiling
    static constexpr FragEmitFlagsT SKY                     = 0x0020;       // Emit a sky column if possible
    static constexpr FragEmitFlagsT MID_WALL_OCCLUDER       = 0x0040;       // Emit a single sided wall occluder entry
    static constexpr FragEmitFlagsT UPPER_WALL_OCCLUDER     = 0x0080;       // Emit an upper wall occluder entry
    static constexpr FragEmitFlagsT LOWER_WALL_OCCLUDER     = 0x0100;       // Emit a lower wall occluder entry
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
    const float depth,
    SegClip& clipBounds,    
    const LightParams& lightParams,
    const float segLightMul,
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
        if (curZt > curZb)
            return;
        
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
        curZb = std::nextafter((float) clipBounds.bottom, -1.0f);
        zbInt = (int32_t) curZb;
        const float pixelsOffscreen = zb - curZb;
        curTexBy -= texYStep * pixelsOffscreen;
        
        // If the clipped size is now invalid then skip
        if (curZt > curZb)
            return;
    }

    // Sanity checks
    BLIT_ASSERT(x <= (int32_t) gScreenWidth);
    BLIT_ASSERT(ztInt <= zbInt);
    BLIT_ASSERT(ztInt >= 0 && ztInt < (int32_t) gScreenHeight);
    BLIT_ASSERT(zbInt >= 0 && zbInt < (int32_t) gScreenHeight);

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
        frag.lightMul = lightParams.getLightMulForDist(depth) * segLightMul;
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
}

//----------------------------------------------------------------------------------------------------------------------
// Clips and emits a floor or ceiling column
//----------------------------------------------------------------------------------------------------------------------
template <FragEmitFlagsT FLAGS>
static void clipAndEmitFlatColumn(
    const uint32_t x,
    const float zt,
    const float zb,
    SegClip& clipBounds,
    const float depth,
    const float worldX,
    const float worldY,
    const float worldZ,
    const bool bClampFirstColumnPixel,
    const uint8_t sectorLightLevel,
    const ImageData& texture
) noexcept {
    static_assert(FLAGS == FragEmitFlags::FLOOR || FLAGS == FragEmitFlags::CEILING);
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
        if (curZt > curZb)
            return;
    }

    if (zbInt >= clipBounds.bottom) {
        // Offscreen at the bottom - clip:
        curZb = (float) clipBounds.bottom - 1.0f;
        zbInt = (int32_t) curZb;
        
        // If the clipped size is now invalid then skip
        if (curZt > curZb)
            return;
    }

    // Emit the floor or ceiling fragment
    const int32_t columnHeight = zbInt - ztInt + 1;

    {
        FlatFragment frag;
        frag.x = x;
        frag.y = ztInt;
        frag.height = columnHeight;
        frag.sectorLightLevel = sectorLightLevel;
        frag.bClampFirstPixel = bClampFirstColumnPixel;
        frag.depth = depth;
        frag.worldX = worldX;
        frag.worldY = worldY;
        frag.worldZ = worldZ;
        frag.pImageData = &texture;

        if constexpr (FLAGS == FragEmitFlags::FLOOR) {    
            gFloorFragments.push_back(frag);
        } else {
            static_assert(FLAGS == FragEmitFlags::CEILING);
            gCeilFragments.push_back(frag);
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
        if (ztInt - 1 >= clipBounds.top) {
            clipBounds = SegClip{ clipBounds.top, (int16_t) ztInt };
        } else {
            clipBounds = SegClip{ 0, 0 };
            gNumFullSegCols++;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// What type of occluder we are emitting to occlude sprites.
//----------------------------------------------------------------------------------------------------------------------
enum class EmitOccluderMode {
    TOP,        // Occlude at the given screen coordinate and above
    BOTTOM      // Occlude at the given screen coordinate and below
};

//----------------------------------------------------------------------------------------------------------------------
// Emits an occluder column that occludes sprites.
// Either the top or bottom can be occluded.
//----------------------------------------------------------------------------------------------------------------------
template <EmitOccluderMode MODE>
static void emitOccluderColumn(
    const uint32_t x,
    const int32_t screenYCoord,
    const float depth
) noexcept {
    static_assert(MODE == EmitOccluderMode::TOP || MODE == EmitOccluderMode::BOTTOM);
    ASSERT(x < gScreenWidth);

    // Ignore the request if the bound is already offscreen
    if constexpr (MODE == EmitOccluderMode::TOP) {
        if (screenYCoord < 0)
            return;
    } else {
        if (screenYCoord >= (int32_t) gScreenHeight)
            return;
    }

    // Determine if we need a new occluder column
    OccludingColumns& occludingCols = gOccludingCols[x];
    const uint32_t numOccludingCols = occludingCols.count;
    const uint32_t curOccluderIdx = numOccludingCols - 1;

    if (numOccludingCols <= 0) {
        // No occluders for this column yet: need a new occluding columns bounds entry
        ASSERT_LOG(numOccludingCols < OccludingColumns::MAX_ENTRIES, "Too many occluders for column!");

        if (numOccludingCols >= OccludingColumns::MAX_ENTRIES)
            return;
        
        ++occludingCols.count;
        occludingCols.depths[0] = depth;
        OccludingColumns::Bounds& bounds = occludingCols.bounds[0];

        if constexpr (MODE == EmitOccluderMode::TOP) {
            bounds.top = (int16_t) screenYCoord;
            bounds.bottom = gScreenHeight;
        } else {
            
            bounds.top = -1;
            bounds.bottom = (int16_t) screenYCoord;
        }
    }
    else if (occludingCols.depths[curOccluderIdx] < depth) {
        // Closer occluders at this column.
        // Only emit a new occluder if it would decrease the number of visible pixels.
        const OccludingColumns::Bounds prevBounds = occludingCols.bounds[curOccluderIdx];
        const int32_t numRowsVisible = std::max((int32_t) prevBounds.bottom - (int32_t) prevBounds.top - 1, 0);
        
        // Figure out the new bounds and new number of rows visible.
        // Only emit the occluder which is deeper in if it occludes more:
        const int16_t newBound = (int16_t) screenYCoord;
        bool bEmitOccluder;

        if constexpr (MODE == EmitOccluderMode::TOP) {
            const int32_t newNumRowsVisible = std::max((int32_t) prevBounds.bottom - (int32_t) newBound - 1, 0);
            bEmitOccluder = (newNumRowsVisible < numRowsVisible);
        } else {
            const int32_t newNumRowsVisible = std::max((int32_t) newBound - (int32_t) prevBounds.top - 1, 0);
            bEmitOccluder = (newNumRowsVisible < numRowsVisible);
        }

        if (bEmitOccluder) {
            ASSERT_LOG(numOccludingCols < OccludingColumns::MAX_ENTRIES, "Too many occluders for column!");

            if (numOccludingCols >= OccludingColumns::MAX_ENTRIES)
                return;
            
            ++occludingCols.count;
            occludingCols.depths[numOccludingCols] = depth;
            OccludingColumns::Bounds& bounds = occludingCols.bounds[numOccludingCols];

            if constexpr (MODE == EmitOccluderMode::TOP) {
                bounds.top = newBound;
                bounds.bottom = prevBounds.bottom;
            } else {
                bounds.top = prevBounds.top;
                bounds.bottom = newBound;
            }
        }
    } else {
        // Re-use an existing column bounds entry if at the same depth.
        //
        // Note that if the depth of the existing column is GREATER then pretend the occluder request is
        // at a greater depth and only allow it to extend the clipped area.
        //
        // Normally this should *NOT* happen because we render front to back, but there appears to be some
        // rare cases where this does not occur for some strange reason, maybe due to the imperfect nature
        // of the BSP splits and lower accuracy of fixed point numbers?
        //
        OccludingColumns::Bounds& bounds = occludingCols.bounds[curOccluderIdx];

        if constexpr (MODE == EmitOccluderMode::TOP) {
            bounds.top = std::max((int16_t) screenYCoord, bounds.top);
        } else {
            bounds.bottom = std::min((int16_t) screenYCoord, bounds.bottom);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Emits wall and potentially floor/ceiling fragments for each column of the draw seg (for later rendering).
// Also potentially emits occluders if specified.
//----------------------------------------------------------------------------------------------------------------------
template <FragEmitFlagsT FLAGS>
static void emitDrawSegColumns(const DrawSeg& drawSeg, const seg_t seg) noexcept {
    //------------------------------------------------------------------------------------------------------------------
    // Some setup logic
    //------------------------------------------------------------------------------------------------------------------

    // For readability's sake:
    constexpr bool EMIT_MID_WALL                = ((FLAGS & FragEmitFlags::MID_WALL) != 0);
    constexpr bool EMIT_UPPER_WALL              = ((FLAGS & FragEmitFlags::UPPER_WALL) != 0);
    constexpr bool EMIT_LOWER_WALL              = ((FLAGS & FragEmitFlags::LOWER_WALL) != 0);
    constexpr bool EMIT_ANY_WALL                = (EMIT_MID_WALL || EMIT_UPPER_WALL || EMIT_LOWER_WALL);
    constexpr bool EMIT_FLOOR                   = ((FLAGS & FragEmitFlags::FLOOR) != 0);
    constexpr bool EMIT_CEILING                 = ((FLAGS & FragEmitFlags::CEILING) != 0);
    constexpr bool EMIT_ANY_FLAT                = (EMIT_FLOOR || EMIT_CEILING);
    constexpr bool EMIT_SKY                     = ((FLAGS & FragEmitFlags::SKY) != 0);
    constexpr bool EMIT_MID_WALL_OCCLUDER       = ((FLAGS & FragEmitFlags::MID_WALL_OCCLUDER) != 0);
    constexpr bool EMIT_UPPER_WALL_OCCLUDER     = ((FLAGS & FragEmitFlags::UPPER_WALL_OCCLUDER) != 0);
    constexpr bool EMIT_LOWER_WALL_OCCLUDER     = ((FLAGS & FragEmitFlags::LOWER_WALL_OCCLUDER) != 0);

    // Sanity checks, expect p1x to be < p2x - should be ensured externally!
    ASSERT(drawSeg.p1x <= drawSeg.p2x);
    
    // Get integer range of the wall
    const int32_t x1 = (int32_t) drawSeg.p1x;
    const int32_t x2 = (int32_t) drawSeg.p2x;

    // Sanity checks: x values should be clipped to be within screen range!
    // x1 should also be >= x2!
    ASSERT(x1 >= 0);
    ASSERT(x1 < (int32_t) gScreenWidth);
    ASSERT(x2 >= 0);
    ASSERT(x2 < (int32_t) gScreenWidth);
    ASSERT(x1 <= x2);
    
    //-----------------------------------------------------------------------------------------------------------------
    // Figure out whether we should clamp the first pixel of each floor or ceiling column.
    // This is done to prevent flat textures from stepping past where they should be at far depths and shallow view angles.
    //-----------------------------------------------------------------------------------------------------------------
    const sector_t& frontSector = *seg.frontsector;

    [[maybe_unused]] bool bCanClampFirstFloorColumnPixel;
    [[maybe_unused]] bool bCanClampFirstCeilingColumnPixel;

    if constexpr (EMIT_ANY_FLAT) {
        if (seg.backsector) {
            // Only clamp when the floor height or texture changes!
            // If adjacent subsectors use the same texture and are at the same height then we
            // want texturing to be as contiguous as possible, so don't do clamping in those cases.            
            const sector_t& backSector = *seg.backsector;

            if constexpr (EMIT_FLOOR) {
                bCanClampFirstFloorColumnPixel = (
                    (frontSector.floorheight != backSector.floorheight) ||
                    (frontSector.FloorPic != backSector.FloorPic)
                );
            }

            if constexpr (EMIT_CEILING) {
                bCanClampFirstCeilingColumnPixel = (
                    (frontSector.ceilingheight != backSector.ceilingheight) ||
                    (frontSector.CeilingPic != backSector.CeilingPic)
                );
            }
        } else {
            // Never step past the bounds when the column starts at a fully solid wall
            if constexpr (EMIT_FLOOR) {
                bCanClampFirstFloorColumnPixel = true;
            }

            if constexpr (EMIT_CEILING) {
                bCanClampFirstCeilingColumnPixel = true;
            }
        }
    }

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
        p1UpperTz = drawSeg.p1tz;
        p2UpperTz = drawSeg.p2tz;
    }

    if constexpr (EMIT_UPPER_WALL || EMIT_UPPER_WALL_OCCLUDER) {
        p1UpperBz = drawSeg.p1tz_back;
        p2UpperBz = drawSeg.p2tz_back;
    }

    if constexpr (EMIT_LOWER_WALL || EMIT_LOWER_WALL_OCCLUDER) {
        p1LowerTz = drawSeg.p1bz_back;
        p2LowerTz = drawSeg.p2bz_back;
    }

    if constexpr (EMIT_MID_WALL || EMIT_LOWER_WALL || EMIT_FLOOR) {
        p1LowerBz = drawSeg.p1bz;
        p2LowerBz = drawSeg.p2bz;
    }

    //------------------------------------------------------------------------------------------------------------------
    // Figure out effective sector light level and light params for that light level
    //------------------------------------------------------------------------------------------------------------------
    uint32_t sectorLightLevel = frontSector.lightlevel;

    if (sectorLightLevel < 240) {
        sectorLightLevel += gExtraLight;
    }

    sectorLightLevel = std::min(sectorLightLevel, 255u);
    const LightParams lightParams = getLightParams(sectorLightLevel);

    //------------------------------------------------------------------------------------------------------------------
    // Grab any required textures and the sector light level
    //------------------------------------------------------------------------------------------------------------------
    const side_t& sideDef = *seg.sidedef;

    [[maybe_unused]] const Texture* pMidTex;
    [[maybe_unused]] const Texture* pUpperTex;
    [[maybe_unused]] const Texture* pLowerTex;
    [[maybe_unused]] const Texture* pFloorTex;
    [[maybe_unused]] const Texture* pCeilingTex;

    if constexpr (EMIT_MID_WALL) {
        pMidTex = getWallTexture(sideDef.midtexture);    
    }

    if constexpr (EMIT_UPPER_WALL) {
        pUpperTex = getWallTexture(sideDef.toptexture);
    }

    if constexpr (EMIT_LOWER_WALL) {
        pLowerTex = getWallTexture(sideDef.bottomtexture);
    }

    if constexpr (EMIT_FLOOR) {
        pFloorTex = getFlatAnimTexture(frontSector.FloorPic);
    }

    if constexpr (EMIT_CEILING || EMIT_SKY) {
        if (seg.frontsector->CeilingPic != SKY_CEILING_PIC) {
            pCeilingTex = getFlatAnimTexture(frontSector.CeilingPic);
        } else {
            pCeilingTex = nullptr;
        }
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
            const float texH = (float) pMidTex->data.height;
            const float texAnchor = bBottomTexUnpegged ? frontFloorZ + texH : frontCeilingZ;
            midTexTy = texAnchor + sideDefRowOffset - frontCeilingZ;

            if (midTexTy < 0) {
                midTexTy += texH;       // DC: This is required for correct vertical alignment in some cases
            }
        }

        if constexpr (EMIT_UPPER_WALL) {
            const float texH = (float) pUpperTex->data.height;
            const float texAnchor = bTopTexUnpegged ? frontCeilingZ : backCeilingZ + texH;
            upperTexTy = texAnchor + sideDefRowOffset - frontCeilingZ;

            if (upperTexTy < 0) {
                upperTexTy += texH;     // DC: This is required for correct vertical alignment in some cases
            }
        } 

        if constexpr (EMIT_LOWER_WALL) {
            const float texH = (float) pLowerTex->data.height;
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

    if constexpr (EMIT_MID_WALL || EMIT_UPPER_WALL || EMIT_CEILING || EMIT_SKY) {
        upperWorldTz = FMath::doomFixed16ToFloat<float>(seg.frontsector->ceilingheight);
    }

    if constexpr (EMIT_UPPER_WALL || EMIT_UPPER_WALL_OCCLUDER) {
        upperWorldBz = FMath::doomFixed16ToFloat<float>(seg.backsector->ceilingheight);
    }

    if constexpr (EMIT_LOWER_WALL || EMIT_LOWER_WALL_OCCLUDER) {
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
    const float xRange = (drawSeg.p2x - drawSeg.p1x);
    const float xRangeDivider =  1.0f / xRange;

    // 1/w (sort of inverse depth)
    const float p1InvW = 1.0f / drawSeg.p1w;
    const float p2InvW = 1.0f / drawSeg.p2w;
    const float invWStep = (p2InvW - p1InvW) * xRangeDivider;

    // Normalized depth
    const float p1y = drawSeg.p1y * p1InvW;
    const float p2y = drawSeg.p2y * p2InvW;
    const float yStep = (p2y - p1y) * xRangeDivider;

    // Vertex attributes that are interpolated across the span (walls only)
    [[maybe_unused]] float p1TexX;
    [[maybe_unused]] float p2TexX;
    [[maybe_unused]] float texXStep;

    if constexpr (EMIT_ANY_WALL) {
        p1TexX = drawSeg.p1TexX * p1InvW;
        p2TexX = drawSeg.p2TexX * p2InvW;
        texXStep = (p2TexX - p1TexX) * xRangeDivider;
    }

    // Vertex attributes that are interpolated across the span (floors)
    [[maybe_unused]] float p1WorldX;
    [[maybe_unused]] float p1WorldY;
    [[maybe_unused]] float p2WorldX;
    [[maybe_unused]] float p2WorldY;
    [[maybe_unused]] float worldXStep;
    [[maybe_unused]] float worldYStep;

    if constexpr (EMIT_ANY_FLAT) {
        p1WorldX = drawSeg.p1WorldX * p1InvW;
        p1WorldY = drawSeg.p1WorldY * p1InvW;
        p2WorldX = drawSeg.p2WorldX * p2InvW;
        p2WorldY = drawSeg.p2WorldY * p2InvW;
        worldXStep = (p2WorldX - p1WorldX) * xRangeDivider;
        worldYStep = (p2WorldY - p1WorldY) * xRangeDivider;
    }

    // Bottom and top Z stepping values for walls and floors
    [[maybe_unused]] float upperTzStep;
    [[maybe_unused]] float upperBzStep;
    [[maybe_unused]] float lowerTzStep;
    [[maybe_unused]] float lowerBzStep;

    if constexpr (EMIT_MID_WALL || EMIT_UPPER_WALL || EMIT_CEILING) {
        upperTzStep = (p2UpperTz - p1UpperTz) * xRangeDivider;
    }

    if constexpr (EMIT_UPPER_WALL || EMIT_UPPER_WALL_OCCLUDER) {
        upperBzStep = (p2UpperBz - p1UpperBz) * xRangeDivider;
    }

    if constexpr (EMIT_LOWER_WALL || EMIT_LOWER_WALL_OCCLUDER) {
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
    float nextXStepCount = -(drawSeg.p1x - (float) x1);     // Adjustement for sub-pixel pos to prevent wiggle

    //------------------------------------------------------------------------------------------------------------------
    // Caching some useful stuff
    //------------------------------------------------------------------------------------------------------------------
    const float viewH = (float) gScreenHeight;

    [[maybe_unused]] bool bEmitFloor;
    [[maybe_unused]] bool bEmitCeiling;

    if constexpr (EMIT_FLOOR) {
        bEmitFloor = drawSeg.bEmitFloor;
    }

    if constexpr (EMIT_CEILING) {
        bEmitCeiling = drawSeg.bEmitCeiling;
    }

    [[maybe_unused]] bool bEmitLowerWallOccluder;
    [[maybe_unused]] bool bEmitUpperWallOccluder;
    [[maybe_unused]] bool bLowerWallOccluderUsesBackZ;
    [[maybe_unused]] bool bUpperWallOccluderUsesBackZ;

    if constexpr (EMIT_LOWER_WALL_OCCLUDER) {
        bEmitLowerWallOccluder = drawSeg.bEmitLowerOccluder;
        bLowerWallOccluderUsesBackZ = drawSeg.bLowerOccluderUsesBackZ;
    }

    if constexpr (EMIT_UPPER_WALL_OCCLUDER) {
        bEmitUpperWallOccluder = drawSeg.bEmitUpperOccluder;
        bUpperWallOccluderUsesBackZ = drawSeg.bUpperOccluderUsesBackZ;
    }

    //------------------------------------------------------------------------------------------------------------------
    // Emit each column
    //------------------------------------------------------------------------------------------------------------------
    for (int32_t x = x1; x <= x2; ++x) {
        //--------------------------------------------------------------------------------------------------------------
        // Grab the clip bounds for this column.
        // If it is fully filled (top >= bottom) then just skip over it and go to the next step
        //--------------------------------------------------------------------------------------------------------------
        SegClip& clipBounds = gSegClip[x];

        if (clipBounds.top >= clipBounds.bottom) {
            nextXStepCount += 1.0f;
            curXStepCount = nextXStepCount;
            continue;
        }

        //--------------------------------------------------------------------------------------------------------------
        // Do stepping for this column and figure out these quantities following through simple linear interpolation.
        // Do perspective correct adjustments where required.
        //--------------------------------------------------------------------------------------------------------------

        // 1/w and w (depth)
        const float wInv = (x < x2) ? p1InvW + invWStep * curXStepCount : p2InvW;
        const float w = 1.0f / wInv;
        const float depth = w;

        // X texture coordinate (for walls)
        [[maybe_unused]] float texX;
        
        if constexpr (EMIT_ANY_WALL) {
            texX = (p1TexX + texXStep * curXStepCount) * w;
        }

        // World X and Y (for floors)
        [[maybe_unused]] float worldX;
        [[maybe_unused]] float worldY;
        
        if constexpr (EMIT_ANY_FLAT) {
            worldX = (p1WorldX + worldXStep * curXStepCount) * w;
            worldY = (p1WorldY + worldYStep * curXStepCount) * w;
        }

        // Bottom and top Z values for walls and floors
        [[maybe_unused]] float upperTz;
        [[maybe_unused]] float upperBz;
        [[maybe_unused]] float lowerTz;
        [[maybe_unused]] float lowerBz;

        if constexpr (EMIT_MID_WALL || EMIT_UPPER_WALL || EMIT_CEILING) {
            upperTz = p1UpperTz + upperTzStep * curXStepCount;
        }

        if constexpr (EMIT_UPPER_WALL || EMIT_UPPER_WALL_OCCLUDER) {
            upperBz = p1UpperBz + upperBzStep * curXStepCount;
        }

        if constexpr (EMIT_LOWER_WALL || EMIT_LOWER_WALL_OCCLUDER) {
            lowerTz = p1LowerTz + lowerTzStep * curXStepCount;
        }

        if constexpr (EMIT_MID_WALL || EMIT_LOWER_WALL || EMIT_FLOOR) {
            lowerBz = p1LowerBz + lowerBzStep * curXStepCount;
        }

        //--------------------------------------------------------------------------------------------------------------
        // Move onto the next pixel for future steps
        //--------------------------------------------------------------------------------------------------------------
        nextXStepCount += 1.0f;
        curXStepCount = nextXStepCount;

        //--------------------------------------------------------------------------------------------------------------
        // Emit all fragments and occluding columns
        //--------------------------------------------------------------------------------------------------------------
        if constexpr (EMIT_FLOOR) {
            if (bEmitFloor) {
                const bool bClampFirstColPixel = (
                    bCanClampFirstFloorColumnPixel &&
                    (depth >= MIN_DEPTH_FOR_FLAT_PIXEL_CLAMP)
                );

                clipAndEmitFlatColumn<FragEmitFlags::FLOOR>(
                    x,
                    lowerBz,
                    viewH,
                    clipBounds,
                    depth,
                    worldX,
                    worldY,
                    lowerWorldBz,
                    bClampFirstColPixel,
                    sectorLightLevel,
                    pFloorTex->data
                );
            }
        }

        if constexpr (EMIT_CEILING) {           
            if (bEmitCeiling && pCeilingTex) {
                const bool bClampFirstColPixel = (
                    bCanClampFirstCeilingColumnPixel &&
                    (depth >= MIN_DEPTH_FOR_FLAT_PIXEL_CLAMP)
                );

                clipAndEmitFlatColumn<FragEmitFlags::CEILING>(
                    x,
                    0.0f,
                    upperTz,
                    clipBounds,
                    depth,
                    worldX,
                    worldY,
                    upperWorldTz,
                    bClampFirstColPixel,
                    sectorLightLevel,
                    pCeilingTex->data
                );
            }
        }

        if constexpr (EMIT_SKY) {
            if ((!pCeilingTex) && (upperTz > 0)) {
                SkyFragment skyFrag;
                skyFrag.x = x;
                skyFrag.height = (uint16_t) std::ceilf(upperTz);

                gSkyFragments.push_back(skyFrag);
            }
        }

        if constexpr (EMIT_MID_WALL) {
            clipAndEmitWallColumn<FragEmitFlags::MID_WALL>(
                x,
                upperTz,
                lowerBz,
                texX,
                midTexTy,
                midTexBy,
                depth,
                clipBounds,
                lightParams,
                seg.lightMul,
                pMidTex->data
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
                depth,
                clipBounds,
                lightParams,
                seg.lightMul,
                pLowerTex->data
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
                depth,
                clipBounds,
                lightParams,
                seg.lightMul,
                pUpperTex->data
            );
        }

        if constexpr (EMIT_MID_WALL_OCCLUDER) {
            // A solid wall will gobble up the entire screen and occlude everything!
            emitOccluderColumn<EmitOccluderMode::TOP>(x, gScreenHeight, depth);
        } else {
            // Even if it's not asked for, if we find the column at this pixel is now
            // fully occluded then mark that as the case with an occluder column:
            if (clipBounds.top >= clipBounds.bottom) {
                emitOccluderColumn<EmitOccluderMode::TOP>(x, gScreenHeight, depth);
                continue;
            }
        }

        if constexpr (EMIT_LOWER_WALL_OCCLUDER) {
            if (bEmitLowerWallOccluder) {
                const float z = (bLowerWallOccluderUsesBackZ) ? lowerTz : lowerBz;
                emitOccluderColumn<EmitOccluderMode::BOTTOM>(x, (int32_t) z, depth);
            }
        }

        if constexpr (EMIT_UPPER_WALL_OCCLUDER) {
            if (bEmitUpperWallOccluder) {
                const float z = (bUpperWallOccluderUsesBackZ) ? upperBz : upperTz;
                emitOccluderColumn<EmitOccluderMode::TOP>(x, (int32_t) z, depth);
            }
        }
    }
}

void addSegToFrame(const seg_t& seg) noexcept {
    // First transform the seg into viewspace and populate vertex attributes
    DrawSeg drawSeg;
    populateSegVertexAttribs(seg, drawSeg);
    transformSegXYToViewSpace(seg, drawSeg);
    
    // Next transform to clip space and clip against the front and left + right planes
    transformSegXYWToClipSpace(drawSeg);
    
    if (!clipSegAgainstFrontPlane(drawSeg))
        return;

    if (!clipSegAgainstLeftPlane(drawSeg))
        return;
    
    if (!clipSegAgainstRightPlane(drawSeg))
        return;
    
    // Now that the seg is not rejected fill in the height values.
    // This function and also determines if we can draw the ceiling/floor and emit occluders based on those z values.
    addClipSpaceZValuesForSeg(drawSeg, seg);

    // Do perspective division and transform the seg to screen space
    doPerspectiveDivisionForSeg(drawSeg);
    transformSegXZToScreenSpace(drawSeg);
    
    // Determine if the seg is back facing and cull if it is
    if (isScreenSpaceSegBackFacing(drawSeg))
        return;

    // Emit all wall and floor fragments for the seg
    if (!seg.backsector) {
        // Fully solid wall with no lower or upper parts
        emitDrawSegColumns<
            FragEmitFlags::MID_WALL |
            FragEmitFlags::MID_WALL_OCCLUDER |
            FragEmitFlags::FLOOR |
            FragEmitFlags::CEILING |
            FragEmitFlags::SKY
        >(drawSeg, seg);
    } else {
        // Two sided seg that may have upper and lower parts to draw.
        // Note: need to ignore upper walls if back sector has a sky ceiling
        if (seg.backsector->CeilingPic != SKY_CEILING_PIC) {
            emitDrawSegColumns<
                FragEmitFlags::LOWER_WALL |
                FragEmitFlags::UPPER_WALL |
                FragEmitFlags::LOWER_WALL_OCCLUDER |
                FragEmitFlags::UPPER_WALL_OCCLUDER |
                FragEmitFlags::FLOOR |
                FragEmitFlags::CEILING |
                FragEmitFlags::SKY
            >(drawSeg, seg);
        } else {
            emitDrawSegColumns<
                FragEmitFlags::LOWER_WALL |
                FragEmitFlags::LOWER_WALL_OCCLUDER |
                FragEmitFlags::UPPER_WALL_OCCLUDER |
                FragEmitFlags::FLOOR |
                FragEmitFlags::CEILING
            >(drawSeg, seg);
        }
    }

    // Grab the flags for the seg's linedef and mark it as seen (since it's visible)
    line_t& lineDef = *seg.linedef;
    lineDef.flags |= ML_MAPPED;
}

END_NAMESPACE(Renderer)
