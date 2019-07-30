#include "Renderer_Internal.h"

#include "Base/Endian.h"
#include "Base/FMath.h"
#include "Base/Tables.h"
#include "Blit.h"
#include "Game/Data.h"
#include "Textures.h"
#include "Video.h"

BEGIN_NAMESPACE(Renderer)

static uint32_t gSpanStart[MAXSCREENHEIGHT];

//----------------------------------------------------------------------------------------------------------------------
// Draws a horizontal span of the floor
//----------------------------------------------------------------------------------------------------------------------
static void drawFloorColumn(
    const uint32_t colY,
    const uint32_t colX,
    const uint32_t numPixels,
    const float texXFrac,
    const float texYFrac,
    const float texXStep,
    const float texYStep,
    const float lightMultiplier,
    const ImageData& texData
) noexcept {
    Blit::blitColumn<
        Blit::BCF_HORZ_COLUMN |     // Column is horizontal rather than vertical
        Blit::BCF_ROW_MAJOR_IMG |   // Floor textures are stored in row major format (unlike sprites and walls)
        Blit::BCF_STEP_X |
        Blit::BCF_STEP_Y |
        Blit::BCF_H_WRAP_64 |       // Floor textures are always 64x64!
        Blit::BCF_V_WRAP_64 |
        Blit::BCF_COLOR_MULT_RGB
    >(
        texData,
        texXFrac,
        texYFrac,
        0.0f,
        Video::gFrameBuffer,
        Video::SCREEN_WIDTH,
        Video::SCREEN_HEIGHT,
        colX + gScreenXOffset,
        colY + gScreenYOffset,
        numPixels,
        texXStep,
        texYStep,
        lightMultiplier,
        lightMultiplier,
        lightMultiplier
    );
}

//----------------------------------------------------------------------------------------------------------------------
// This is the basic function to draw horizontal lines quickly 
//----------------------------------------------------------------------------------------------------------------------
static void mapPlane(
    const uint32_t x2,
    const uint32_t y,
    const float planeY,
    const float planeHeight,
    const float baseXScale,
    const float baseYScale,
    const LightParams& lightParams,
    const ImageData& texData
) noexcept {
    // Start x, distance to floor column, length and angle
    const uint32_t x1 = gSpanStart[y];
    const float distance = gYSlope[y] * planeHeight;    // Get the offset for the plane height
    const float length = gDistScale[x1] * distance;
    const float angle = -getViewAngleForX(x1) + FMath::doomAngleToRadians<float>(gViewAngleBAM);

    // xfrac, yfrac, xstep, ystep
    const float xfrac = std::cos(angle) * length + FMath::doomFixed16ToFloat<float>(gViewXFrac);
    const float yfrac = planeY - std::sin(angle) * length;
    const float xstep = distance * baseXScale;
    const float ystep = distance * baseYScale;

    // Figure out the light multiplier to use and then draw the floor column
    const float lightMul = lightParams.getLightMulForDist(distance);
    drawFloorColumn(y, x1, x2 - x1, xfrac, yfrac, xstep, ystep, lightMul, texData);
}

//----------------------------------------------------------------------------------------------------------------------
// Draw a plane by scanning the open records. 
// The open records are an array of top and bottom Y's for a graphical plane.
// I traverse the array to find out the horizontal spans I need to draw; this is a bottleneck routine.
//----------------------------------------------------------------------------------------------------------------------
void drawVisPlane(
    visplane_t& plane,
    const float planeY,
    const float baseXScale,
    const float baseYScale
) noexcept {
    // Basic plane info
    const Texture* const pTex = getFlatAnimTexture((uint32_t) plane.picHandle);
    const ImageData& texData = pTex->data;
    const float planeHeight = std::abs(plane.height);
    const LightParams lightParams = getLightParams(plane.planeLight);
    
    // Start the draw loop
    const int32_t stop = plane.maxX + 1;    // Maximum x coord
    int32_t x = plane.minX;                 // Starting x

    ScreenYPair prevCol = { MAXSCREENHEIGHT - 1, 0 };       // Set posts to stop drawing
    ScreenYPair* const pPlaneCols = plane.cols;             // Init the pointer to the open Y's
    ASSERT(stop < C_ARRAY_SIZE(plane.cols));

    do {
        // Fetch the NEW top and bottom
        ASSERT((uint32_t) x < C_ARRAY_SIZE(plane.cols));
        const ScreenYPair newCol = pPlaneCols[x];         
        
        if (prevCol == newCol)
            continue;
        
        uint32_t prevTopY = prevCol.ty;         // Previous and dest Y coords for top and bottom line
        uint32_t prevBottomY = prevCol.by;
        uint32_t newTopY = newCol.ty;
        uint32_t newBottomY = newCol.by;
        
        // For lines on the top, check if the entry is going down            
        if (prevTopY < newTopY && prevTopY <= prevBottomY) {        // Valid?
            uint32_t count = prevBottomY + 1;                       // Convert to <
            if (newTopY < count) {                                  // Use the lower
                count = newTopY;                                    // This is smaller
            }

            do {
                // Draw to this x
                mapPlane(
                    x,
                    prevTopY,
                    planeY,
                    planeHeight,
                    baseXScale,
                    baseYScale,
                    lightParams,
                    texData
                );
            } while (++prevTopY < count);   // Keep counting
        }

        if (newTopY < prevTopY && newTopY <= newBottomY) {
            uint32_t count = newBottomY + 1;
            if (prevTopY < count) {
                count = prevTopY;
            }

            // Mark the starting x's
            do {
                ASSERT(newTopY < C_ARRAY_SIZE(gSpanStart));
                gSpanStart[newTopY] = x;
            } while (++newTopY < count);
        }
        
        if (prevBottomY > newBottomY && prevBottomY >= prevTopY) {
            int32_t count = (int32_t) prevTopY - 1;
            if (count < (int32_t) newBottomY) {
                count = newBottomY;
            }

            do {
                // Draw to this x
                mapPlane(
                    x,
                    prevBottomY,
                    planeY,
                    planeHeight,
                    baseXScale,
                    baseYScale,
                    lightParams,
                    texData
                );
            } while ((int32_t) --prevBottomY > count);
        }

        if (newBottomY > prevBottomY && newBottomY >= newTopY) {
            int32_t count = (int32_t) newTopY - 1;
            if (count < (int32_t) prevBottomY) {
                count = prevBottomY;
            }

            // Mark the starting x's
            do {
                ASSERT(newBottomY < C_ARRAY_SIZE(gSpanStart));
                gSpanStart[newBottomY] = x;
            } while ((int32_t) --newBottomY > count);
        }

        prevCol = newCol;
    } while (++x <= stop);
}

void drawAllVisPlanes() noexcept {
    const float viewAngle = FMath::doomAngleToRadians<float>(gViewAngleBAM);
    const float wallScaleAngle = viewAngle - FMath::ANGLE_90<float>;        // Left to right mapping

    const float baseXScale = +std::cos(wallScaleAngle) / ((float) gScreenWidth * 0.5f);
    const float baseYScale = -std::sin(wallScaleAngle) / ((float) gScreenWidth * 0.5f);
    const float planeY = FMath::doomFixed16ToFloat<float>(-gViewYFrac);     // Get the Y coord for camera

    visplane_t* pPlane = gVisPlanes + 1;
    visplane_t* const pEndPlane = gpEndVisPlane;
    
    while (pPlane < pEndPlane) {
        drawVisPlane(*pPlane, planeY, baseXScale, baseYScale);
        ++pPlane;
    }
}

void drawAllFloorFragments() noexcept {
    // The view position
    const float viewX = gViewX;
    const float viewY = gViewY;
    const float viewZ = gViewZ;

    // Get a view direction and a perpendicular to it
    const float viewDirX = std::cos(gViewAngle);
    const float viewDirY = std::sin(gViewAngle);
    const float viewPerpX = viewDirY;
    const float viewPerpY = -viewDirX;

    // Near plane width and height
    const float nearPlaneW = Z_NEAR * std::tan(FOV * 0.5f) * 2.0f;
    const float halfNearPlaneW = nearPlaneW * 0.5f;
    float nearPlaneH = nearPlaneW / VIEW_ASPECT_RATIO;
    const float halfNearPlaneH = nearPlaneH * 0.5f;
    
    // Compute the two worldspace end points of the view near plane
    const float viewNearP1x = viewX + viewDirX * Z_NEAR - halfNearPlaneW * viewPerpX;
    const float viewNearP1y = viewY + viewDirY * Z_NEAR - halfNearPlaneW * viewPerpY;
    const float viewNearP2x = viewX + viewDirX * Z_NEAR + halfNearPlaneW * viewPerpX;
    const float viewNearP2y = viewY + viewDirY * Z_NEAR + halfNearPlaneW * viewPerpY;

    // Compute the step in world coordinates for each view colum
    const float viewNearColumnStepX = (viewNearP2x - viewNearP1x) / ((float) gScreenWidth - 1.0f);
    const float viewNearColumnStepY = (viewNearP2y - viewNearP1y) / ((float) gScreenWidth - 1.0f);

    for (const FlatFragment& flatFrag : gFloorFragments) {
        // Compute the x,y and z coordinate at which the bottom of screen column lies in world space.
        // This yields a position along the near plane:
        float nearX = viewNearP1x + (float) flatFrag.x * viewNearColumnStepX;
        float nearY = viewNearP1y + (float) flatFrag.x * viewNearColumnStepY;
        float nearZ = viewZ - halfNearPlaneH;

        // Compute the intersection of a ray going from the view point & through the near plane point, 
        // against the plane that we are drawing. This will become the new 'near' plane point and will
        // determine where to access in the flat texture:
        {
            // This is the ray direction
            const float rayDirX = nearX - viewX;
            const float rayDirY = nearY - viewY;
            const float rayDirZ = nearZ - viewZ;

            // Calculate first: AXd + BYd + CZd
            // I.E - The dot product of the plane normal (0, 0, 1) with the ray direction:
            const float divisor = rayDirZ;

            // Calculate: AX0 + BY0 + CZ0 + D
            // I.E - The dot product of the ray origin with the plane normal (0, 0, 1) plus
            // the distance of the plane from the origin along it's normal:
            const float planeOffset = flatFrag.endWorldZ;
            const float dividend = viewZ - planeOffset;

            // Compute the ray intersection time: -(AX0 + BY0 + CZ0 + D) / (AXd + BYd + CZd)
            const float intersectT = -dividend / divisor;

            // Using the intersect time, compute the world intersect point
            nearX = viewX + rayDirX * intersectT;
            nearY = viewY + rayDirY * intersectT;
            nearZ = viewZ + rayDirZ * intersectT;
        }

        // Get the depth at which the first pixel in the floor plane column lies
        float nearDepth;

        {
            const float dx = nearX - viewX;
            const float dy = nearY - viewY;
            const float dz = nearZ - viewZ;
            nearDepth = std::sqrtf(dx * dx + dy * dy + dz * dz);
        }

        // Get the distance of the flat fragment to the view (far depth)
        const float farX = flatFrag.endWorldX;
        const float farY = flatFrag.endWorldY;
        const float farZ = flatFrag.endWorldZ;

        float farDepth;

        {
            const float dx = farX - viewX;
            const float dy = farY - viewY;
            const float dz = farZ - viewZ;
            farDepth = std::sqrtf(dx * dx + dy * dy + dz * dz);
        }

        // Figure out the inverse depths and inverse depth step.
        // Pretend the floor column is unclipped for the purposes of stepping.
        const float invNearDepth = 1.0f / nearDepth;
        const float invFarDepth = 1.0f / farDepth;
        const float numStepsUnclipped = (float) gScreenHeight - flatFrag.y;
        const float divNumSteps = (numStepsUnclipped > 1.0f) ? 1.0f / (numStepsUnclipped - 1.0f) : 0.0f;
        const float invDepthStep = (invNearDepth - invFarDepth) * divNumSteps;

        // Blit
        Blit::blitColumn<
            Blit::BCF_STEP_X |
            Blit::BCF_STEP_Y |
            Blit::BCF_H_WRAP_64 |
            Blit::BCF_V_WRAP_64 |
            Blit::BCF_COLOR_MULT_RGB |
            Blit::BCF_PERSP_CORRECT
        >(
            *flatFrag.pImageData,
            farX * invFarDepth,     // N.B: Must adjust for perspective correct interpolation!
            farY * invFarDepth,     // N.B: Must adjust for perspective correct interpolation!
            0.0f,
            Video::gFrameBuffer,
            Video::SCREEN_WIDTH,
            Video::SCREEN_HEIGHT,
            flatFrag.x + gScreenXOffset,
            flatFrag.y + gScreenYOffset,
            flatFrag.height,
            (nearX * invNearDepth - farX * invFarDepth) * divNumSteps,
            (nearY * invNearDepth - farY * invFarDepth) * divNumSteps,
            1.0f,   // TODO
            1.0f,   // TODO
            1.0f,   // TODO
            1.0f,
            invFarDepth,
            invDepthStep
        );
    }
}

void drawAllCeilingFragments() noexcept {
    const float viewX = gViewX;
    const float viewY = gViewY;

    for (const FlatFragment& flatFrag : gCeilFragments) {
        // Get the Z distance away at which the flat fragment will meet the view plane.
        // Note that this assumes a vertical FOV of 90 degrees!
        const float clipDistFromView = std::fabsf(flatFrag.endWorldZ - gViewZ);

        // Get the distance of the flat fragment to the view
        const float fragEndX = flatFrag.endWorldX;
        const float fragEndY = flatFrag.endWorldY;
        const float viewToEndX = fragEndX - viewX;
        const float viewToEndY = fragEndY - viewY;
        const float endToViewDist = std::sqrtf(viewToEndX * viewToEndX + viewToEndY * viewToEndY);

        // Figure out the world X and Y coordinate for the flat where it intersects the view
        const float clipT = clipDistFromView / endToViewDist;
        const float clipWorldX = viewX + viewToEndX * clipT;
        const float clipWorldY = viewY + viewToEndY * clipT;

        Blit::blitColumn<
            Blit::BCF_STEP_X |
            Blit::BCF_STEP_Y |
            Blit::BCF_H_WRAP_64 |
            Blit::BCF_V_WRAP_64 |
            Blit::BCF_COLOR_MULT_RGB
        >(
            *flatFrag.pImageData,
            flatFrag.endWorldX,
            flatFrag.endWorldY,
            0.0f,
            Video::gFrameBuffer,
            Video::SCREEN_WIDTH,
            Video::SCREEN_HEIGHT,
            flatFrag.x + gScreenXOffset,
            flatFrag.y + gScreenYOffset,
            flatFrag.height,
            viewToEndX / flatFrag.height,   // TODO: X STEP
            viewToEndY / flatFrag.height,   // TODO: Y STEP
            1.0f,   // TODO
            1.0f,   // TODO
            1.0f    // TODO
        );
    }
}

END_NAMESPACE(Renderer)
