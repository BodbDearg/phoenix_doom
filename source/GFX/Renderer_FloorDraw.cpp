#include "Renderer_Internal.h"

#include "Base/Tables.h"
#include "Blit.h"
#include "Textures.h"
#include "Video.h"

BEGIN_NAMESPACE(Renderer)

//----------------------------------------------------------------------------------------------------------------------
// What type of flat is being drawn
//----------------------------------------------------------------------------------------------------------------------
enum class DrawFlatMode {
    FLOOR,
    CEILING
};

//----------------------------------------------------------------------------------------------------------------------
// Does the intersection of a ray against a floor or ceiling plane
//----------------------------------------------------------------------------------------------------------------------
template <DrawFlatMode MODE>
static inline void doRayFlatPlaneIntersection(
    const float flatPlaneZ,
    const float rayOriginX,
    const float rayOriginY,
    const float rayOriginZ,
    const float rayDirX,
    const float rayDirY,
    const float rayDirZ,
    float& intersectX,
    float& intersectY,
    float& intersectZ
) noexcept {
    // Calculate first: AXd + BYd + CZd
    // I.E - The dot product of the plane normal (0, 0, 1) or (0, 0, -1) with the ray direction:
    float divisor;
    
    if constexpr (MODE == DrawFlatMode::FLOOR) {
        divisor = rayDirZ;
    } else {
        divisor = -rayDirZ;
    }

    // Calculate: AX0 + BY0 + CZ0 + D
    // I.E - The dot product of the ray origin with the plane normal (0, 0, 1) or (0, 0, -1) plus
    // the distance of the plane from the origin along it's normal:
    float dividend;
            
    if constexpr (MODE == DrawFlatMode::FLOOR) {
        dividend = rayOriginZ - flatPlaneZ;
    } else {
        dividend = -rayOriginZ + flatPlaneZ;
    }

    // Compute the ray intersection time: -(AX0 + BY0 + CZ0 + D) / (AXd + BYd + CZd)
    const float intersectT = -dividend / divisor;

    // Using the intersect time, compute the world intersect point
    intersectX = rayOriginX + rayDirX * intersectT;
    intersectY = rayOriginY + rayDirY * intersectT;
    intersectZ = rayOriginZ + rayDirZ * intersectT;    
}

//----------------------------------------------------------------------------------------------------------------------
// Draw one vertical column of a flat.
// 
// Unlike the original version of 3DO Doom (and PC Doom) I do not bother with visplanes, or converting vertical floor
// columns into horizontal floor columns. These days it seems to make sense to lean more on the fast arithmetic 
// performance of the CPU instead of trawling through memory (slow) trying to match up visplanes and convert vertical
// columns into horizontal ones.
//----------------------------------------------------------------------------------------------------------------------
template <DrawFlatMode MODE>
static inline void drawFlatColumn(const FlatFragment flatFrag) noexcept {
    // Cache some useful values
    const float viewX = gViewX;
    const float viewY = gViewY;
    const float viewZ = gViewZ;
    const float flatPlaneZ = flatFrag.worldZ;
    const float nearPlaneTz = gNearPlaneTz;
    const float nearPlaneZStep = gNearPlaneZStepPerViewColPixel;

    const LightParams& lightParams = getLightParams(flatFrag.sectorLightLevel);
    const uint16_t* const pSrcPixels = flatFrag.pImageData->pPixels;

    // The x and y coordinate in world space of the screen column being drawn.
    // Note: take the horizontal center position of the pixel to improve accuracy, hence + 0.5 here:
    const float nearPlaneX = gNearPlaneP1x + ((float) flatFrag.x + 0.5f) * gNearPlaneXStepPerViewCol;
    const float nearPlaneY = gNearPlaneP1y + ((float) flatFrag.x + 0.5f) * gNearPlaneYStepPerViewCol;

    // Compute the xz direction of the ray going from the view through this screen column.
    // Since this is constant per screen column pixel, we only need to do this once here:
    const float rayDirX = nearPlaneX - viewX;
    const float rayDirY = nearPlaneY - viewY;

    // Where to start outputting to and where we end outputting.
    // Note that floors rendered in a top to bottom direction, while ceilings are bottom to top:
    int32_t curDstY;
    int32_t endDstY;

    if constexpr (MODE == DrawFlatMode::FLOOR) {
        curDstY = (int32_t)(flatFrag.y);
        endDstY = (int32_t)(flatFrag.y + flatFrag.height);
    } else {
        curDstY = (int32_t)(flatFrag.y + flatFrag.height - 1);
        endDstY = (int32_t)(flatFrag.y - 1);
    }

    const uint32_t startScreenX = gScreenXOffset + flatFrag.x;
    const uint32_t startScreenY = gScreenYOffset + (uint32_t) curDstY;

    // This is where we store the intersection of a ray going from the view point to the flat plane.
    // The ray passes through whatever near plane pixel on the screen we are rendering and we update
    // the ray and location of intersection for whatever pixel we are drawing...
    float intersectX;
    float intersectY;
    float intersectZ;

    // If clamp was specified for the first pixel then use the world position of where the column starts to figure out
    // the texture coordinate for the first column pixel, otherwise do a ray/plane intersection like we do in the column loop.
    // The clamp is used to prevent over-runs of textures that are sensitive to repeating, such as 64x64 teleporters.
    BLIT_ASSERT(flatFrag.depth >= 0.0f);

    if (flatFrag.bClampFirstPixel) {
        intersectX = flatFrag.worldX;
        intersectY = flatFrag.worldY;
        intersectZ = flatFrag.worldZ;
    } else {
        const float nearPlaneZ = nearPlaneTz + nearPlaneZStep * ((float) curDstY + 0.5f);   // Note: take the center position of the pixel to improve accuracy, hence + 0.5 here!
        const float rayDirZ = nearPlaneZ - viewZ;

        doRayFlatPlaneIntersection<MODE>(
            flatPlaneZ,
            viewX,
            viewY,
            viewZ,
            rayDirX,
            rayDirY,
            rayDirZ,
            intersectX,
            intersectY,
            intersectZ
        );
    }

    // Draw the column!
    uint32_t* pDstPixel = Video::gFrameBuffer + (startScreenY * Video::SCREEN_WIDTH) + startScreenX;

    while (true) {
        // Are we done?
        if constexpr (MODE == DrawFlatMode::FLOOR) {
            if (curDstY >= endDstY)
                break;
        } else {
            if (curDstY <= endDstY)
                break;
        }

        // Get the source pixel (ARGB1555 format).
        // Note that the flat texture is always expected to be 64x64, hence we can wraparound with a simple bitwise AND:
        const uint32_t curSrcXInt = (uint32_t) intersectX & 63;
        const uint32_t curSrcYInt = (uint32_t) intersectY & 63;
        const uint16_t srcPixelARGB1555 = pSrcPixels[curSrcYInt * 64 + curSrcXInt];
        
        // Extract RGB components and shift such that the maximum value is 255 instead of 31.
        const uint16_t texR = (srcPixelARGB1555 & uint16_t(0b0111110000000000)) >> 7;
        const uint16_t texG = (srcPixelARGB1555 & uint16_t(0b0000001111100000)) >> 2;
        const uint16_t texB = (srcPixelARGB1555 & uint16_t(0b0000000000011111)) << 3;

        // Get the distance to the view point and light multiplier for that distance
        const float distToView = FMath::distance3d(intersectX, intersectY, intersectZ, viewX, viewY, viewZ);
        const float lightMul = lightParams.getLightMulForDist(distToView);

        // Get the texture colors in 0-255 float format.
        // Note that if we are not doing any color multiply these conversions would be redundant, but I'm guessing
        // that the compiler would be smart enough to optimize out the useless operations in those cases (hopefully)!
        const float r = std::min((float) texR * lightMul, 255.0f);
        const float g = std::min((float) texG * lightMul, 255.0f);
        const float b = std::min((float) texB * lightMul, 255.0f);

        // Write out the pixel value
        *pDstPixel = (
            (uint32_t(r) << 24) |
            (uint32_t(g) << 16) |
            (uint32_t(b) << 8)
        );

        // Move onto the next pixel
        if constexpr (MODE == DrawFlatMode::FLOOR) {
            ++curDstY;
            pDstPixel += Video::SCREEN_WIDTH;
        } else {
            --curDstY;
            pDstPixel -= Video::SCREEN_WIDTH;
        }

        // Compute the ray/plane intersection for the upcoming pixel to get its texture coordinate
        {
            // Note: take the vertical center position of the pixel to improve accuracy, hence + 0.5 here!
            const float nearPlaneZ = nearPlaneTz + nearPlaneZStep * ((float) curDstY + 0.5f);
            const float rayDirZ = nearPlaneZ - viewZ;

            doRayFlatPlaneIntersection<MODE>(
                flatPlaneZ,
                viewX,
                viewY,
                viewZ,
                rayDirX,
                rayDirY,
                rayDirZ,
                intersectX,
                intersectY,
                intersectZ
            );
        }
    }
}

void drawAllFloorFragments() noexcept {
    for (const FlatFragment& flatFrag : gFloorFragments) {
        drawFlatColumn<DrawFlatMode::FLOOR>(flatFrag);
    }
}

void drawAllCeilingFragments() noexcept {
    for (const FlatFragment& flatFrag : gCeilFragments) {
        drawFlatColumn<DrawFlatMode::CEILING>(flatFrag);
    }
}

END_NAMESPACE(Renderer)
