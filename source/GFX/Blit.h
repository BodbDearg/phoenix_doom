#pragma once

#include "Base/Fixed.h"
#include "Base/Macros.h"
#include <algorithm>
#include <cmath>

// Set to '1' to enable heavy duty bounds checking on blitting.
// This can slow down stuff a lot, but can be useful to sanity check blit code.
#define BLIT_ASSERT_ENABLED 1

// Blit assertion macro
#if BLIT_ASSERT_ENABLED == 1
    #define BLIT_ASSERT(Check) ASSERT(Check)
#else
    #define BLIT_ASSERT()
#endif

namespace Blit {
    //------------------------------------------------------------------------------------------------------------------
    // Flags for when blitting a column.
    //------------------------------------------------------------------------------------------------------------------
    static constexpr uint32_t BCF_HORZ_COLUMN       = 0x00000001;       // Instead of blitting to a vertical column of pixels, blit to a horizontal column.
    static constexpr uint32_t BCF_ROW_MAJOR_IMG     = 0x00000002;       // The source image is stored in row major format instead of column major (default).
    static constexpr uint32_t BCF_STEP_X            = 0x00000004;       // Step the texture coordinate in the X direction for each pixel blitted.
    static constexpr uint32_t BCF_STEP_Y            = 0x00000008;       // Step the texture coordinate in the Y direction for each pixel blitted.
    static constexpr uint32_t BCF_ALPHA_TEST        = 0x00000010;       // Check the alpha of source image pixels and do not blit if alpha '0'.
    static constexpr uint32_t BCF_COLOR_MULT_RGB    = 0x00000020;       // Multiply the RGB values of the source image by the given 16.16 fixed point color.
    static constexpr uint32_t BCF_COLOR_MULT_A      = 0x00000040;       // Multiply the alpha value of the source image by the given 16.16 fixed point color.
    static constexpr uint32_t BCF_ALPHA_BLEND       = 0x00000080;       // Alpha blend by the alpha value of the source image. Note that you need use color multiply in order to get values other than '0' or '1' due to 1 bit alpha!
    static constexpr uint32_t BCF_H_WRAP_WRAP       = 0x00000100;       // Horizontal texture wrap mode: wrap. The source 'x' texture coord wraps around.
    static constexpr uint32_t BCF_H_WRAP_CLAMP      = 0x00000200;       // Horizontal texture wrap mode: clamp. The source 'x' texture coord is clamped.
    static constexpr uint32_t BCF_H_WRAP_DISCARD    = 0x00000400;       // Horizontal texture wrap mode: discard. Do not render anything when we go beyond the bounds of the texture.
    static constexpr uint32_t BCF_V_WRAP_WRAP       = 0x00000800;       // Vertical texture wrap mode: wrap. The source 'x' texture coord wraps around.
    static constexpr uint32_t BCF_V_WRAP_CLAMP      = 0x00001000;       // Vertical texture wrap mode: clamp. The source 'x' texture coord is clamped.
    static constexpr uint32_t BCF_V_WRAP_DISCARD    = 0x00002000;       // Vertical texture wrap mode: discard. Do not render anything when we go beyond the bounds of the texture.
    static constexpr uint32_t BCF_H_CLIP            = 0x00004000;       // If set then clip the column to the given bounds horizontally
    static constexpr uint32_t BCF_V_CLIP            = 0x00008000;       // If set then clip the column to the given bounds vertically

    //----------------------------------------------------------------------------------------------------------------------
    // Utility that determines how much to step (in texels) per pixel to render the entire of the given
    // texture dimension in the given render area dimension (both in pixels).
    //----------------------------------------------------------------------------------------------------------------------
    inline constexpr Fixed calcTexelStep(const uint32_t textureSize, const uint32_t renderSize) noexcept {
        if (textureSize <= 1 || renderSize <= 1)
            return 0;

        // The way the math works here helps to ensure that the last texel chosen for the given render size is pretty much
        // always the last texel according to the given texture dimension:
        const int32_t numPixelSteps = (int32_t) renderSize - 1;
        const Fixed step = fixed16Div(
            intToFixed16(textureSize) - 1,  // N.B: never let it reach 'textureSize' (i.e out of bounds) - keep *just* below!
            intToFixed16(numPixelSteps)
        );

        return step;
    }

    //------------------------------------------------------------------------------------------------------------------
    // Wrap (or not) an X coordinate to the given width for the given blit column flags.
    // Note: only actually does something with certain wrapping modes!
    //------------------------------------------------------------------------------------------------------------------
    template <uint32_t BC_FLAGS>
    inline constexpr uint32_t wrapXCoord(int32_t x, [[maybe_unused]] const uint32_t width) noexcept {
        BLIT_ASSERT(width > 0);

        if constexpr ((BC_FLAGS & (BCF_H_WRAP_WRAP | BCF_H_WRAP_CLAMP)) == 0) {
            // No wrapping therefore return the input as-is.
            // However do assert that we are in bounds!
            BLIT_ASSERT(x >= 0);
            BLIT_ASSERT((uint32_t) x < width);
            return (uint32_t) x;
        }
        else {
            if constexpr ((BC_FLAGS & BCF_H_WRAP_WRAP) != 0) {
                x = (int32_t)((uint32_t) x % width);
            }

            if constexpr ((BC_FLAGS & BCF_H_WRAP_CLAMP) != 0) {
                x = (x >= 0) ? ((x < (int32_t) width) ? x : (int32_t) width - 1) : 0;
            }

            return (uint32_t) x;
        }
    }

    //------------------------------------------------------------------------------------------------------------------
    // Wrap (or not) an Y coordinate to the given height for the given blit column flags.
    // Note: only actually does something with certain wrapping modes!
    //------------------------------------------------------------------------------------------------------------------
    template <uint32_t BC_FLAGS>
    inline constexpr uint32_t wrapYCoord(int32_t y, [[maybe_unused]] const uint32_t height) noexcept {
        BLIT_ASSERT(height > 0);

        if constexpr ((BC_FLAGS & (BCF_V_WRAP_WRAP | BCF_V_WRAP_CLAMP)) == 0) {
            // No wrapping therefore return the input as-is.
            // However do assert that we are in bounds!
            BLIT_ASSERT((uint32_t) y >= 0);
            BLIT_ASSERT((uint32_t) y < height);
            return (uint32_t) y;
        }
        else {
            if constexpr ((BC_FLAGS & BCF_V_WRAP_WRAP) != 0) {
                y = (int32_t)((uint32_t) y % height);
            }

            if constexpr ((BC_FLAGS & BCF_V_WRAP_CLAMP) != 0) {
                y = (y >= 0) ? ((y < (int32_t) height) ? y : (int32_t) height - 1) : 0;
            }

            return (uint32_t) y;
        }
    }

    //------------------------------------------------------------------------------------------------------------------
    // Blits a column of pixels from the given source image in either ARGB1555 or ARGB8888 format to the destination
    // pixel buffer which is in XRGB8888 format and ROW MAJOR. Optionally, alpha testing and blending can be applied.
    //
    // Notes:
    //  (1) Destination pixels are written with alpha 0.
    //      We do not care about alpha in the destination.
    //  (2) Negative ARGB multipliers are *NOT* supported (considered undefined behavior).
    //  (3) If no wrap mode is specified then the blit *MUST* be in bounds for the source image for all pixels blitted.
    //      It is assumed that the callee has ensured this is the case.
    //  (4) Depending on flags, some input variables may be unused.
    //      It is hoped that in release builds these things will simply be optimized away and carry no overhead.
    //  (5) The output area blitted to *MUST* be in bounds, unless clipping is used.
    //      It is assumed the callee has already done all required clipping prior to calling this function.
    //  (6) Contrary to the input image, the output image *MUST* always be in row major format.
    //------------------------------------------------------------------------------------------------------------------
    template <uint32_t BC_FLAGS, class SrcPixelT>
    inline void blitColumn(
        const SrcPixelT* const pSrcPixels,                  // Pixel data for source image
        const uint32_t srcW,                                // Width of source image
        const uint32_t srcH,                                // Height of source image
        float srcX,                                         // Where to start blitting from in the input texture: x
        float srcY,                                         // Where to start blitting from in the input texture: y
        float srcXSubPixelAdjustment,                       // An adjustment applied to the first stepping of the src X value - used for sub-pixel stability adjustments to prevent 'shaking' and 'wiggle'
        float srcYSubPixelAdjustment,                       // An adjustment applied to the first stepping of the src Y value - used for sub-pixel stability adjustments to prevent 'shaking' and 'wiggle'
        uint32_t* const pDstPixels,                         // Output image pixels, this must point to the the TOP LEFT pixel of the output image
        [[maybe_unused]] const uint32_t dstW,               // Output image width
        [[maybe_unused]] const uint32_t dstH,               // Output image height
        [[maybe_unused]] const uint32_t dstPixelsPitch,     // The number of pixels that must be skipped to go onto a new row in the output image
        int32_t dstX,                                       // Where to start blitting to in output image: x
        int32_t dstY,                                       // Where to start blitting to in output image: y
        uint32_t dstCount,                                  // How many pixels to blit to the output image (in positive x or y direction)
        [[maybe_unused]] const float srcXStep,              // Texture coordinate x step
        [[maybe_unused]] const float srcYStep,              // Texture coordinate y step
        [[maybe_unused]] const float rMul = 1.0f,           // Color multiply value: red
        [[maybe_unused]] const float gMul = 1.0f,           // Color multiply value: green
        [[maybe_unused]] const float bMul = 1.0f,           // Color multiply value: blue
        [[maybe_unused]] const float aMul = 1.0f            // Color multiply value: alpha
    ) noexcept {
        // Basic sanity checks
        static_assert(
            (std::is_same_v<SrcPixelT, uint16_t> || std::is_same_v<SrcPixelT, uint32_t>),
            "Source pixel buffer must be either usigned ARGB1555 (16-bit) or ARGB8888 (32-bit)!"
        );

        BLIT_ASSERT(pSrcPixels);
        BLIT_ASSERT(srcW > 0);
        BLIT_ASSERT(srcH > 0);
        BLIT_ASSERT(pDstPixels);
        BLIT_ASSERT(dstW > 0);
        BLIT_ASSERT(dstH > 0);
        BLIT_ASSERT(dstPixelsPitch > 0);
        BLIT_ASSERT(rMul >= 0.0f);
        BLIT_ASSERT(gMul >= 0.0f);
        BLIT_ASSERT(bMul >= 0.0f);
        BLIT_ASSERT(aMul >= 0.0f);

        // Turn these into bools for readability
        constexpr bool IS_VERT_COLUMN       = ((BC_FLAGS & BCF_HORZ_COLUMN) == 0);
        constexpr bool IS_HORZ_COLUMN       = ((BC_FLAGS & BCF_HORZ_COLUMN) != 0);
        constexpr bool IS_SRC_COL_MAJOR     = ((BC_FLAGS & BCF_ROW_MAJOR_IMG) == 0);
        constexpr bool IS_SRC_ROW_MAJOR     = ((BC_FLAGS & BCF_ROW_MAJOR_IMG) != 0);

        constexpr bool DO_X_STEP            = ((BC_FLAGS & BCF_STEP_X) != 0);
        constexpr bool DO_Y_STEP            = ((BC_FLAGS & BCF_STEP_Y) != 0);
        constexpr bool DO_ANY_STEP          = (DO_X_STEP | DO_Y_STEP);

        constexpr bool DO_ALPHA_TEST        = ((BC_FLAGS & BCF_ALPHA_TEST) != 0);
        constexpr bool DO_COLOR_MULT_RGB    = ((BC_FLAGS & BCF_COLOR_MULT_RGB) != 0);
        constexpr bool DO_COLOR_MULT_A      = ((BC_FLAGS & BCF_COLOR_MULT_A) != 0);
        constexpr bool DO_ALPHA_BLEND       = ((BC_FLAGS & BCF_ALPHA_BLEND) != 0);
        constexpr bool NEED_ALPHA_CHANNEL   = (DO_ALPHA_TEST | DO_COLOR_MULT_A | DO_ALPHA_BLEND);

        constexpr bool DO_H_WRAP_WRAP       = ((BC_FLAGS & BCF_H_WRAP_WRAP) != 0);
        constexpr bool DO_H_WRAP_CLAMP      = ((BC_FLAGS & BCF_H_WRAP_CLAMP) != 0);
        constexpr bool DO_H_WRAP_DISCARD    = ((BC_FLAGS & BCF_H_WRAP_DISCARD) != 0);
        constexpr bool DO_ANY_H_WRAP        = (DO_H_WRAP_WRAP | DO_H_WRAP_CLAMP | DO_H_WRAP_DISCARD);

        constexpr bool DO_V_WRAP_WRAP       = ((BC_FLAGS & BCF_V_WRAP_WRAP) != 0);
        constexpr bool DO_V_WRAP_CLAMP      = ((BC_FLAGS & BCF_V_WRAP_CLAMP) != 0);
        constexpr bool DO_V_WRAP_DISCARD    = ((BC_FLAGS & BCF_V_WRAP_DISCARD) != 0);
        constexpr bool DO_ANY_V_WRAP        = (DO_V_WRAP_WRAP | DO_V_WRAP_CLAMP | DO_V_WRAP_DISCARD);

        constexpr bool DO_H_CLIP            = ((BC_FLAGS & BCF_H_CLIP) != 0);
        constexpr bool DO_V_CLIP            = ((BC_FLAGS & BCF_V_CLIP) != 0);
        constexpr bool DO_ANY_CLIP          = (DO_H_CLIP | DO_V_CLIP);

        // Corner case: if we multiply to alpha zero then just discard everything
        if constexpr (DO_ALPHA_TEST && DO_COLOR_MULT_A) {
            if (aMul <= 0.0f)
                return;
        }

        // Clip: reject an entire vertical or horizontal column if out of bounds
        if constexpr (IS_VERT_COLUMN && DO_H_CLIP) {
            if ((uint32_t) dstX >= dstW)    // Note: cheat and do a < 0 check in one operation by casting to unsigned and checking in this way!
                return;
        }

        if constexpr (IS_HORZ_COLUMN && DO_V_CLIP) {
            if ((uint32_t) dstY >= dstH)    // Note: cheat and do a < 0 check in one operation by casting to unsigned and checking in this way!
                return;
        }

        // Clip: a vertical column against the top of the draw area
        if constexpr (IS_VERT_COLUMN && DO_V_CLIP) {
            const int32_t numPixelsOutOfBounds = -dstY;

            if (numPixelsOutOfBounds > 0) {
                if (numPixelsOutOfBounds >= (int32_t) dstCount)     // Completely offscreen?
                    return;

                dstY = 0;
                srcY = srcY + srcYStep * numPixelsOutOfBounds + srcYSubPixelAdjustment;
                srcYSubPixelAdjustment = 0;
                dstCount -= numPixelsOutOfBounds;
            }
        }

        // Clip: a vertical column against the bottom of the draw area
        if constexpr (IS_VERT_COLUMN && DO_V_CLIP) {
            const int32_t endY = dstY + dstCount;

            if (endY > (int32_t) dstH) {
                const int32_t numPixelsOutOfBounds = endY - dstH;
                dstCount -= numPixelsOutOfBounds;
            }
        }

        // Clip: a horizontal column against the left of the draw area
        if constexpr (IS_HORZ_COLUMN && DO_H_CLIP) {
            const int32_t numPixelsOutOfBounds = -dstX;

            if (numPixelsOutOfBounds > 0) {
                if (numPixelsOutOfBounds >= (int32_t) dstCount)     // Completely offscreen?
                    return;

                dstX = 0;
                srcX = srcX + srcXStep * numPixelsOutOfBounds + srcXSubPixelAdjustment;
                srcXSubPixelAdjustment = 0;
                dstCount -= numPixelsOutOfBounds;
            }
        }

        // Clip: a horizontal column against the right of the draw area
        if constexpr (IS_HORZ_COLUMN && DO_H_CLIP) {
            const int32_t endX = dstX + dstCount;

            if (endX > (int32_t) dstW) {
                const int32_t numPixelsOutOfBounds = endX - dstW;
                dstCount -= numPixelsOutOfBounds;
            }
        }

        // Wrap discard checks.
        // See if we can just discard the entire column:
        if constexpr (DO_H_WRAP_DISCARD && (!DO_X_STEP)) {
            if ((uint32_t) srcX >= srcW)    // Note: cheat and do a < 0 check in one operation by casting to unsigned and checking in this way!
                return;
        }

        if constexpr (DO_V_WRAP_DISCARD && (!DO_Y_STEP)) {
            if ((uint32_t) srcY >= srcH)    // Note: cheat and do a < 0 check in one operation by casting to unsigned and checking in this way!
                return;
        }

        // Where to start and stop outputing to and the pointer step to make on each output pixel
        BLIT_ASSERT(dstX >= 0 && dstX < (int32_t) dstW);
        BLIT_ASSERT(dstY >= 0 && dstY < (int32_t) dstH);

        uint32_t* const pFirstDstPixel = pDstPixels + (uintptr_t) dstY * dstPixelsPitch + dstX;
        uint32_t* pDstPixel = pFirstDstPixel;
        uint32_t* pEndDstPixel;

        if constexpr (IS_HORZ_COLUMN) {
            BLIT_ASSERT(dstX + dstCount <= dstW);
            pEndDstPixel = pDstPixel + dstCount;
        } else {
            static_assert(IS_VERT_COLUMN);
            BLIT_ASSERT(dstY + dstCount <= dstH);
            pEndDstPixel = pDstPixel + (uintptr_t) dstCount * dstPixelsPitch;
        }

        // Where to start reading from the in the source image; optimize for certain scenarios based on the input flags...
        // For example if the image is in column major format (the default) and we are only stepping in the y direction
        // then we can just index into a column rather than doing full two dimensional addressing.
        #if BLIT_ASSERT_ENABLED == 1
            if constexpr (!DO_ANY_H_WRAP) {
                BLIT_ASSERT((int32_t) srcX >= 0);
                BLIT_ASSERT((int32_t) srcX < (int32_t) srcW);
            }

            if constexpr (!DO_ANY_V_WRAP) {
                BLIT_ASSERT((int32_t) srcY >= 0);
                BLIT_ASSERT((int32_t) srcY < (int32_t) srcH);
            }
        #endif

        [[maybe_unused]] const SrcPixelT* pSrcRowOrCol;

        constexpr bool USE_SRC_ROW_INDEXING = (IS_SRC_ROW_MAJOR && DO_X_STEP && (!DO_Y_STEP));
        constexpr bool USE_SRC_COL_INDEXING = (IS_SRC_COL_MAJOR && DO_Y_STEP && (!DO_X_STEP));

        if constexpr (USE_SRC_ROW_INDEXING) {
            // Stepping in x direction in a row major image.
            // Will index into a row instead of doing 2d indexing.
            pSrcRowOrCol = pSrcPixels + (uintptr_t) wrapYCoord<BC_FLAGS>((int32_t) srcY, srcH) * srcW;
        }
        else if constexpr (USE_SRC_COL_INDEXING) {
            // Stepping in y direction in a column major image.
            // Will index into a column instead of doing 2d indexing.
            pSrcRowOrCol = pSrcPixels + (uintptr_t) wrapXCoord<BC_FLAGS>((int32_t) srcX, srcW) * srcH;
        }

        // Main pixel blitting loop
        [[maybe_unused]] uint32_t curSrcXInt = (uint32_t) srcX;             // Note: unsigned allows us to test < 0 at the same time as >= texture dimension!
        [[maybe_unused]] uint32_t curSrcYInt = (uint32_t) srcY;             // Note: unsigned allows us to test < 0 at the same time as >= texture dimension!
        [[maybe_unused]] float nextSrcX = srcX + srcXSubPixelAdjustment;    // Note: the adjusment is applied AFTER the first pixel
        [[maybe_unused]] float nextSrcY = srcY + srcYSubPixelAdjustment;    // Note: the adjusment is applied AFTER the first pixel
        [[maybe_unused]] bool bDidHWrapDiscardClamp = false;
        [[maybe_unused]] bool bDidVWrapDiscardClamp = false;

        while (pDstPixel < pEndDstPixel) {
            do {
                // Do discard wrapping.
                // Note that we allow ONE out of bounds coordiante provided that it has an integer texture coordinate DIFFERENT to the last.
                // This ensures that if we stop at the edges then we always show the last row or column of the texture for borders.
                if constexpr (DO_H_WRAP_DISCARD && DO_X_STEP) {
                    if (curSrcXInt >= srcW) {
                        if (bDidHWrapDiscardClamp)
                            break;

                        curSrcXInt = wrapXCoord<BCF_H_WRAP_CLAMP>((int32_t) curSrcXInt, srcW);
                        bDidHWrapDiscardClamp = true;
                        const uint32_t prevSrcXInt = (uint32_t)(nextSrcX - srcXStep);

                        if (prevSrcXInt == curSrcXInt)
                            break;
                    }
                }

                if constexpr (DO_V_WRAP_DISCARD && DO_Y_STEP) {
                    if (curSrcYInt >= srcH) {
                        if (bDidVWrapDiscardClamp)
                            break;

                        curSrcYInt = wrapYCoord<BCF_V_WRAP_CLAMP>((int32_t) curSrcYInt, srcH);
                        bDidVWrapDiscardClamp = true;
                        const uint32_t prevSrcYInt = (uint32_t)(nextSrcY - srcYStep);

                        if (prevSrcYInt == curSrcYInt)
                            break;
                    }
                }

                // Get the source pixel (ARGB1555 or ARGB8888 format)
                SrcPixelT srcPixel;

                if constexpr (USE_SRC_ROW_INDEXING) {
                    curSrcXInt = wrapXCoord<BC_FLAGS>((int32_t) curSrcXInt, srcW);
                    srcPixel = pSrcRowOrCol[curSrcXInt];
                }
                else if constexpr (USE_SRC_COL_INDEXING) {
                    curSrcYInt = wrapYCoord<BC_FLAGS>((int32_t) curSrcYInt, srcH);
                    srcPixel = pSrcRowOrCol[curSrcYInt];
                }
                else {
                    curSrcXInt = wrapXCoord<BC_FLAGS>((int32_t) curSrcXInt, srcW);
                    curSrcYInt = wrapYCoord<BC_FLAGS>((int32_t) curSrcYInt, srcH);

                    if constexpr (IS_SRC_COL_MAJOR) {
                        srcPixel = pSrcPixels[curSrcXInt * srcH + curSrcYInt];
                    } else {
                        srcPixel = pSrcPixels[curSrcYInt * srcW + curSrcXInt];
                    }
                }

                // Extract RGBA components.
                // In the case of ARGB1555 also shift such that the maximum value is 255 instead of 31.
                // Exception: leave alpha as '1' since that saves us a division when converting to a 0-1 range.
                [[maybe_unused]] uint8_t texA;

                if constexpr (NEED_ALPHA_CHANNEL) {
                    if constexpr (std::is_same_v<SrcPixelT, uint16_t>) {
                        texA = (uint8_t)((srcPixel & uint16_t(0b1000000000000000)) >> 15);
                    } else {
                        texA = (uint8_t)(srcPixel >> 24);
                    }
                }

                uint8_t texR;
                uint8_t texG;
                uint8_t texB;

                if constexpr (std::is_same_v<SrcPixelT, uint16_t>) {
                    texR = (uint8_t)((srcPixel & uint16_t(0b0111110000000000)) >> 7);
                    texG = (uint8_t)((srcPixel & uint16_t(0b0000001111100000)) >> 2);
                    texB = (uint8_t)((srcPixel & uint16_t(0b0000000000011111)) << 3);
                } else {
                    texR = (uint8_t)((srcPixel & 0x00FF0000u) >> 16);
                    texG = (uint8_t)((srcPixel & 0x0000FF00u) >> 8);
                    texB = (uint8_t)(srcPixel & 0x000000FFu);
                }

                // Get the texture colors in 0-255 float format, and alpha in a 0-1 format.
                // Note that if we are not doing any color multiply these conversions would be redundant, but I'm guessing
                // that the compiler would be smart enough to optimize out the useless operations in those cases (hopefully)!
                [[maybe_unused]] float a;

                if constexpr (NEED_ALPHA_CHANNEL) {
                    // N.B - in the case of ARGB1555 alpha is already in a 0-1 range, so just a cast is needed!
                    if constexpr (std::is_same_v<SrcPixelT, uint16_t>) {
                        a = (float) texA; 
                    } else {
                        a = (float) texA / 255.0f;
                    }
                }

                float r = (float) texR;
                float g = (float) texG;
                float b = (float) texB;

                if constexpr (DO_COLOR_MULT_RGB) {
                    r = std::min(r * rMul, 255.0f);
                    g = std::min(g * gMul, 255.0f);
                    b = std::min(b * bMul, 255.0f);
                }

                if constexpr (DO_COLOR_MULT_A) {
                    a = std::min(a * aMul, 1.0f);
                }

                // Do the alpha test if enabled
                if constexpr (DO_ALPHA_TEST) {
                    if (texA <= 0.0f)
                        break;
                }

                // Do alpha blending with the destination pixel if enabled
                if constexpr (DO_ALPHA_BLEND) {
                    // Read the destination pixel RGBA and convert to 0-255 float
                    const uint32_t dstPixelXRGB8888 = *pDstPixel;
                    const float dstR = (float)((uint8_t)(dstPixelXRGB8888 >> 16));
                    const float dstG = (float)((uint8_t)(dstPixelXRGB8888 >> 8));
                    const float dstB = (float)((uint8_t)(dstPixelXRGB8888));

                    // Source and destination blend factors
                    const float srcFactor = a;
                    const float dstFactor = 1.0f - a;

                    // Do the blend
                    r = r * srcFactor + dstR * dstFactor;
                    g = g * srcFactor + dstG * dstFactor;
                    b = b * srcFactor + dstB * dstFactor;
                }

                // Write out the pixel value
                *pDstPixel = (
                    (uint32_t(r) << 16) |
                    (uint32_t(g) << 8) |
                    (uint32_t(b))
                );
            } while (false);    // Dummy loop to allow break out due to alpha test cull, or wrap discard cull!

            // Stepping in the source and destination
            if constexpr (DO_X_STEP) {
                nextSrcX += srcXStep;
                curSrcXInt = (uint32_t) nextSrcX;
            }

            if constexpr (DO_Y_STEP) {
                nextSrcY += srcYStep;
                curSrcYInt = (uint32_t) nextSrcY;
            }

            if constexpr (IS_HORZ_COLUMN) {
                pDstPixel += 1;
            } else {
                static_assert(IS_VERT_COLUMN);
                pDstPixel += dstPixelsPitch;
            }
        }
    }

    //------------------------------------------------------------------------------------------------------------------
    // Blits a portion of a sprite in either ARGB1555 or ARGB8888 format to the destination in XRGB8888 format.
    // Optionally, alpha testing and blending can be applied.
    //
    // Notes:
    //  (1) See the 'blitColumn' documentation for most of the details on how blitting works.
    //  (2) The wrapping mode used for this operation is always DISCARD; wrapping or clamp is NOT allowed.
    //  (3) The input image is assummed ROW MAJOR, you CANNOT use column major images.
    //  (4) For sprite blitting the following blit column flags are NOT allowed, since they are controlled
    //      by the sprite blit routine:
    //          (a) Whether to output to a horizontal column or not.
    //          (b) Whether the input image is row major (it is always assumed to be this)
    //          (c) Whether to step in the x and y directions of the source texture.
    //          (d) Whether to allow wrapping.
    //------------------------------------------------------------------------------------------------------------------
    template <uint32_t BC_FLAGS, class SrcPixelT>
    inline void blitSprite(
        const SrcPixelT* const pSrcPixels,              // Pixel data for source image
        const uint32_t srcPixelsW,                      // Width of source image
        const uint32_t srcPixelsH,                      // Height of source image
        const float srcX,                               // Where to start blitting from in the input texture: x & y
        const float srcY,
        const float srcW,                               // Size of the area to blit from the input texture: width & height
        const float srcH,
        uint32_t* const pDstPixels,                     // Output image pixels, this must point to the the TOP LEFT pixel of the output image
        const uint32_t dstPixelsW,                      // Output image width
        const uint32_t dstPixelsH,                      // Output image height
        const uint32_t dstPixelsPitch,                  // The number of pixels that must be skipped to go onto a new row in the output image
        const float dstX,                               // Where to start blitting to in output image: x & y
        const float dstY,
        const float dstW,                               // Size of the area to blit to in the output texture: width & height
        const float dstH,
        [[maybe_unused]] const float rMul = 1.0f,       // Color multiply value: red
        [[maybe_unused]] const float gMul = 1.0f,       // Color multiply value: green
        [[maybe_unused]] const float bMul = 1.0f,       // Color multiply value: blue
        [[maybe_unused]] const float aMul = 1.0f        // Color multiply value: alpha
    ) noexcept {
        // These blit column flags are not allowed to be set
        static_assert((BC_FLAGS & BCF_HORZ_COLUMN) == 0);
        static_assert((BC_FLAGS & BCF_ROW_MAJOR_IMG) == 0);
        static_assert((BC_FLAGS & BCF_STEP_X) == 0);
        static_assert((BC_FLAGS & BCF_STEP_Y) == 0);
        static_assert((BC_FLAGS & BCF_H_WRAP_WRAP) == 0);
        static_assert((BC_FLAGS & BCF_H_WRAP_CLAMP) == 0);
        static_assert((BC_FLAGS & BCF_H_WRAP_DISCARD) == 0);
        static_assert((BC_FLAGS & BCF_V_WRAP_WRAP) == 0);
        static_assert((BC_FLAGS & BCF_V_WRAP_CLAMP) == 0);
        static_assert((BC_FLAGS & BCF_V_WRAP_DISCARD) == 0);

        // Sanity checks
        BLIT_ASSERT(pSrcPixels);
        BLIT_ASSERT(srcPixelsW > 0);
        BLIT_ASSERT(srcPixelsH > 0);
        BLIT_ASSERT(srcW >= 0.0f);
        BLIT_ASSERT(srcH >= 0.0f);
        BLIT_ASSERT(pDstPixels);
        BLIT_ASSERT(dstPixelsW > 0);
        BLIT_ASSERT(dstPixelsH > 0);
        BLIT_ASSERT(dstPixelsPitch > 0);

        // Ignore if blitting to a zero sized area
        if (dstW <= 0.0f || dstH <= 0.0f) {
            BLIT_ASSERT(dstW == 0.0f);
            BLIT_ASSERT(dstH == 0.0f);
            return;
        }

        // Figure out the start and end pixel in the destination and number of rows and columns
        const uint32_t dstXi = (uint32_t) dstX;
        const uint32_t dstXiend = (uint32_t)(dstX + std::ceil(dstW));
        const uint32_t dstYi = (uint32_t) dstY;
        const uint32_t dstYiend = (uint32_t)(dstY + std::ceil(dstH));

        const uint32_t dstXCount = (dstXiend - dstXi) + 1;
        const uint32_t dstYCount = (dstYiend - dstYi) + 1;

        // Figure out the x and y step.
        //
        // Note: adding a small amount to source width and height here as a hack to combat imprecision
        // and missing pixels for certain UI scalings from the base resolution of 320x200.
        const float srcXStep = (dstXCount > 0) ? (srcW + 0.01f) / (float) dstW : 0.0f;
        const float srcYStep = (dstYCount > 0) ? (srcH + 0.01f) / (float) dstH : 0.0f;

        // Blit each row of the image
        for (uint32_t rowNum = 0; rowNum < dstYCount; ++rowNum) {
            blitColumn<
                BC_FLAGS |
                BCF_HORZ_COLUMN |
                BCF_ROW_MAJOR_IMG |
                BCF_STEP_X |
                BCF_H_WRAP_DISCARD |
                BCF_V_WRAP_DISCARD
            >(
                pSrcPixels,
                srcPixelsW,
                srcPixelsH,
                srcX,
                srcY + srcYStep * (float) rowNum,
                0.0f,
                0.0f,
                pDstPixels,
                dstPixelsW,
                dstPixelsH,
                dstPixelsPitch,
                dstXi,
                dstYi + rowNum,
                dstXCount,
                srcXStep,
                0.0f,
                rMul,
                gMul,
                bMul,
                aMul
            );
        }
    }

    //------------------------------------------------------------------------------------------------------------------
    // Blits a single color to the destination in XRGB8888 format.
    // Alpha blending is also applied if alpha is < 1.
    // Intended to be used for clearing a portion of the screen, or for screen tints.
    //
    // Notes:
    //  (1) Clipping to the specified output area size is also automatically done.
    //------------------------------------------------------------------------------------------------------------------
    inline void blitRect(
        uint32_t* const pDstPixels,         // Output image pixels, this must point to the the TOP LEFT pixel of the output image
        const uint32_t dstPixelsW,          // Output image width
        const uint32_t dstPixelsH,          // Output image height
        const uint32_t dstPixelsPitch,      // The number of pixels that must be skipped to go onto a new row in the output image
        float dstX,                         // Where to start blitting to in output image: x & y
        float dstY,
        float dstW,                         // Size of the area to blit to in the output texture: width & height
        float dstH,
        const float r,                      // Color: red
        const float g,                      // Color: green
        const float b,                      // Color: blue
        const float a                       // Color: alpha
    ) noexcept {
        // Sanity checks
        BLIT_ASSERT(pDstPixels);
        BLIT_ASSERT(dstPixelsW > 0);
        BLIT_ASSERT(dstPixelsH > 0);
        BLIT_ASSERT(dstPixelsPitch > 0);

        // Early out if completely invisible
        if (a <= 0.0f) {
            return;
        }

        // Ignore if blitting to a zero sized area
        if (dstW <= 0.0f || dstH <= 0.0f) {
            BLIT_ASSERT(dstW == 0.0f);
            BLIT_ASSERT(dstH == 0.0f);
            return;
        }

        // Figure out the start and end pixel in the destination
        int32_t dstXi = (int32_t) dstX;
        int32_t dstXiend = (int32_t) std::ceil(dstX + dstW);
        int32_t dstYi = (int32_t) dstY;
        int32_t dstYiend = (int32_t) std::ceil(dstY + dstH);

        // Do clipping against the specified region and reject if out of bounds
        dstXi = std::max(dstXi, 0);
        dstYi = std::max(dstYi, 0);
        dstXiend = std::min(dstXiend, (int32_t) dstPixelsW - 1);
        dstYiend = std::min(dstYiend, (int32_t) dstPixelsH - 1);

        if ((dstXi > dstXiend) || (dstYi > dstYiend)) {
            return;
        }

        // Do either a straight color fill or blend with the destination
        if (a >= 1.0f) {
            // No alpha blending, just fill with the color
            const uint32_t colorXRGB8888 = (
                (((uint32_t) std::min(std::max(r * 255.0f, 0.0f), 255.0f)) << 16) |
                (((uint32_t) std::min(std::max(g * 255.0f, 0.0f), 255.0f)) << 8) |
                ((uint32_t) std::min(std::max(b * 255.0f, 0.0f), 255.0f))
            );

            for (int32_t y = dstYi; y <= dstYiend; ++y) {
                uint32_t* const pDstRow = pDstPixels + (uintptr_t) y * dstPixelsPitch;

                for (int32_t x = dstXi; x <= dstXiend; ++x) {
                    uint32_t& dstPixel = pDstRow[x];
                    dstPixel = colorXRGB8888;
                }
            }
        } else {
            // Alpha blending, have to blend with the destination color.
            // Pre-multiply the given color to convert to a 0-255 range:
            const float srcR = r * 255.0f;
            const float srcG = g * 255.0f;
            const float srcB = b * 255.0f;

            for (int32_t y = dstYi; y <= dstYiend; ++y) {
                uint32_t* const pDstRow = pDstPixels + (uintptr_t) y * dstPixelsPitch;

                for (int32_t x = dstXi; x <= dstXiend; ++x) {
                    uint32_t& dstPixel = pDstRow[x];
                    
                    // Read the destination pixel RGBA and convert to 0-255 float
                    const uint32_t dstPixelXRGB8888 = dstPixel;
                    const float dstR = (float)((uint8_t)(dstPixelXRGB8888 >> 16));
                    const float dstG = (float)((uint8_t)(dstPixelXRGB8888 >> 8));
                    const float dstB = (float)((uint8_t)(dstPixelXRGB8888));

                    // Source and destination blend factors
                    const float srcFactor = a;
                    const float dstFactor = 1.0f - a;

                    // Do the blend
                    const float rt = srcR * srcFactor + dstR * dstFactor;
                    const float gt = srcG * srcFactor + dstG * dstFactor;
                    const float bt = srcB * srcFactor + dstB * dstFactor;

                    // Convert back to XRGB8888 and save
                    const uint32_t colorXRGB8888 = (
                        (((uint32_t) std::min(std::max(rt, 0.0f), 255.0f)) << 16) |
                        (((uint32_t) std::min(std::max(gt, 0.0f), 255.0f)) << 8) |
                        ((uint32_t) std::min(std::max(bt, 0.0f), 255.0f))
                    );

                    dstPixel = colorXRGB8888;
                }
            }            
        }
    }
}
