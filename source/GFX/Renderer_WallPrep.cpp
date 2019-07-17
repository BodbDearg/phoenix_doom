#include "Renderer_Internal.h"

#include "Base/FMath.h"
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
// Get the distance from the view xy to the given point
//----------------------------------------------------------------------------------------------------------------------
static float viewToPointDist(const Fixed px, const Fixed py) noexcept {    
    const float dx = FMath::doomFixed16ToFloat<float>(px - gViewX);
    const float dy = FMath::doomFixed16ToFloat<float>(py - gViewY);
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
        (offsetAngle + gViewAngle) - normalAngle
    );
    float scale2 = scale;

    if (wall.rightX > wall.leftX) {
        offsetAngle = gXToViewAngle[wall.rightX];
        scale2 = scaleFromGlobalAngle(
            rwDistance,
            offsetAngle,
            (offsetAngle + gViewAngle) - normalAngle
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
        wall.CenterAngle = gViewAngle - normalAngle;
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
    const float f_floorheight = FMath::doomFixed16ToFloat<float>(frontSector.floorheight - gViewZ);
    const float f_ceilingheight = FMath::doomFixed16ToFloat<float>(frontSector.ceilingheight - gViewZ);
    
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
    const float b_floorheight = FMath::doomFixed16ToFloat<float>(pBackSector->floorheight - gViewZ);
    const float b_ceilingheight = FMath::doomFixed16ToFloat<float>(pBackSector->ceilingheight - gViewZ);

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

END_NAMESPACE(Renderer)
