#pragma once

#include "Base/Fixed.h"
#include "Base/Macros.h"
#include "ImageData.h"

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
    static constexpr uint32_t BCF_STEP_ANY = (
        BCF_STEP_X |
        BCF_STEP_Y
    );
    static constexpr uint32_t BCF_ALPHA_TEST        = 0x00000010;       // Check the alpha of source image pixels and do not blit if alpha '0'.
    static constexpr uint32_t BCF_COLOR_MULT_RGB    = 0x00000020;       // Multiply the RGB values of the source image by the given 16.16 fixed point color.
    static constexpr uint32_t BCF_COLOR_MULT_A      = 0x00000040;       // Multiply the alpha value of the source image by the given 16.16 fixed point color.
    static constexpr uint32_t BCF_ALPHA_BLEND       = 0x00000080;       // Alpha blend by the alpha value of the source image. Note that you need use color multiply in order to get values other than '0' or '1' due to 1 bit alpha!
    static constexpr uint32_t BCF_H_WRAP_WRAP       = 0x00000400;       // Horizontal texture wrap mode: wrap. The source 'x' texture coord wraps around.
    static constexpr uint32_t BCF_H_WRAP_CLAMP      = 0x00000800;       // Horizontal texture wrap mode: clamp. The source 'x' texture coord is clamped.
    static constexpr uint32_t BCF_H_WRAP_64         = 0x00001000;       // Horizontal texture wrap mode: wrap. (faster/specialized version, texture *MUST* be 64 units wide)
    static constexpr uint32_t BCF_H_WRAP_128        = 0x00002000;       // Horizontal texture wrap mode: wrap. (faster/specialized version, texture *MUST* be 128 units wide)
    static constexpr uint32_t BCF_H_WRAP_256        = 0x00004000;       // Horizontal texture wrap mode: wrap. (faster/specialized version, texture *MUST* be 256 units wide)
    static constexpr uint32_t BCF_H_WRAP_ANY = (
        BCF_H_WRAP_WRAP |
        BCF_H_WRAP_CLAMP |
        BCF_H_WRAP_64 |
        BCF_H_WRAP_128 |
        BCF_H_WRAP_256
    );
    static constexpr uint32_t BCF_V_WRAP_WRAP       = 0x00008000;       // Vertical texture wrap mode: wrap. The source 'x' texture coord wraps around.
    static constexpr uint32_t BCF_V_WRAP_CLAMP      = 0x00010000;       // Vertical texture wrap mode: clamp. The source 'x' texture coord is clamped.
    static constexpr uint32_t BCF_V_WRAP_64         = 0x00020000;       // Vertical texture wrap mode: wrap. (faster/specialized version, texture *MUST* be 64 units wide)
    static constexpr uint32_t BCF_V_WRAP_128        = 0x00040000;       // Vertical texture wrap mode: wrap. (faster/specialized version, texture *MUST* be 128 units wide)
    static constexpr uint32_t BCF_V_WRAP_256        = 0x00080000;       // Vertical texture wrap mode: wrap. (faster/specialized version, texture *MUST* be 256 units wide)
    static constexpr uint32_t BCF_V_WRAP_ANY = (
        BCF_V_WRAP_WRAP |
        BCF_V_WRAP_CLAMP |
        BCF_V_WRAP_64 |
        BCF_V_WRAP_128 |
        BCF_V_WRAP_256
    );

    //------------------------------------------------------------------------------------------------------------------
    // Wrap (or not) an X coordinate to the given width for the given blit column flags.
    //------------------------------------------------------------------------------------------------------------------
    template <uint32_t BC_FLAGS>
    inline uint32_t wrapXCoord(int32_t x, [[maybe_unused]] const uint32_t width) noexcept {
        BLIT_ASSERT(width > 0);

        if constexpr ((BC_FLAGS & BCF_H_WRAP_ANY) == 0) {
            // No wrapping therefore return the input as-is.
            // However do assert that we are in bounds!
            BLIT_ASSERT(x >= 0);
            BLIT_ASSERT((uint32_t) x < width);
            return (uint32_t) x;
        }
        else {
            if constexpr (BC_FLAGS & BCF_H_WRAP_WRAP) {
                x = (int32_t)((uint32_t) x % width);
            }

            if constexpr (BC_FLAGS & BCF_H_WRAP_CLAMP) {
                x = (x >= 0) ? ((x < width) ? x : width - 1) : 0;
            }

            if constexpr (BC_FLAGS & BCF_H_WRAP_64) {
                x &= (int32_t) 0x3F;
            }

            if constexpr (BC_FLAGS & BCF_H_WRAP_128) {
                x &= (int32_t) 0x7F;
            }

            if constexpr (BC_FLAGS & BCF_H_WRAP_256) {
                x &= (int32_t) 0xFF;
            }

            return (uint32_t) x;
        }
    }

    //------------------------------------------------------------------------------------------------------------------
    // Wrap (or not) an Y coordinate to the given height for the given blit column flags.
    //------------------------------------------------------------------------------------------------------------------
    template <uint32_t BC_FLAGS>
    inline uint32_t wrapYCoord(int32_t y, [[maybe_unused]] const uint32_t height) noexcept {
        BLIT_ASSERT(height > 0);

        if constexpr ((BC_FLAGS & BCF_V_WRAP_ANY) == 0) {
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
                y = (y >= 0) ? ((y < height) ? y : height - 1) : 0;
            }

            if constexpr ((BC_FLAGS & BCF_V_WRAP_64) != 0) {
                y &= (int32_t) 0x3F;
            }

            if constexpr ((BC_FLAGS & BCF_V_WRAP_128) != 0) {
                y &= (int32_t) 0x7F;
            }

            if constexpr ((BC_FLAGS & BCF_V_WRAP_256) != 0) {
                y &= (int32_t) 0xFF;
            }

            return (uint32_t) y;
        }
    }

    //------------------------------------------------------------------------------------------------------------------
    // Blits a column of pixels from the given source image in RGBA5551 format to the destination in RGBA8888 format.
    // Optionally, alpha testing and blending can be applied.
    //
    // Notes:
    //  (1) Destination pixels are written with alpha 0.
    //      We do not care about alpha in the destination.
    //  (2) Negative RGBA multipliers are *NOT* supported (considered undefined behavior).
    //  (3) If no wrap mode is specified then the blit *MUST* be in bounds for the source image for all pixels blitted.
    //      It is assumed that the callee has ensured this is the case.
    //  (4) Depending on flags, some input variables may be unused.
    //      It is hoped that in release builds these things will simply be optimized away and carry no overhead.
    //  (5) The output area blitted to *MUST* be in bounds.
    //      It is assumed the callee has already done all required clipping prior to calling this function.
    //  (6) Contrary to the input image, the output image *MUST* always be in row major format.
    //------------------------------------------------------------------------------------------------------------------
    template <uint32_t BC_FLAGS>
    inline void blitColumn(
        const ImageData& srcImg,                        // Input texture and dimensions
        const Fixed srcX,                               // Where to start blitting from in the input texture: x
        const Fixed srcY,                               // Where to start blitting from in the input texture: y
        uint32_t* const pDstPixels,                     // Output image pixels
        const uint32_t dstW,                            // Output image width
        [[maybe_unused]] const uint32_t dstH,           // Output image height
        const uint32_t dstX,                            // Where to start blitting to in output image: x
        const uint32_t dstY,                            // Where to start blitting to in output image: y
        const uint32_t dstCount,                        // How many pixels to blit to the output image (in positive x or y direction)
        [[maybe_unused]] const Fixed srcXStep,          // Texture coordinate x step
        [[maybe_unused]] const Fixed srcYStep,          // Texture coordinate y step
        [[maybe_unused]] const Fixed r = FRACUNIT,      // 16.16 color multiply value: red
        [[maybe_unused]] const Fixed g = FRACUNIT,      // 16.16 color multiply value: green
        [[maybe_unused]] const Fixed b = FRACUNIT,      // 16.16 color multiply value: blue
        [[maybe_unused]] const Fixed a = FRACUNIT       // 16.16 color multiply value: alpha
    ) noexcept {
        // Basic sanity checks
        BLIT_ASSERT(srcImg.pPixels);
        BLIT_ASSERT(srcImg.width > 0);
        BLIT_ASSERT(srcImg.height > 0);
        BLIT_ASSERT(r >= 0);
        BLIT_ASSERT(g >= 0);
        BLIT_ASSERT(b >= 0);
        BLIT_ASSERT(a >= 0);

        // Corner case: if multiplying by alpha and alpha test is enabled with no blend then
        // just avoid drawing the entire column if the multiply value is < 1.0!
        if constexpr (((BC_FLAGS & BCF_ALPHA_TEST) != 0) && ((BC_FLAGS & BCF_COLOR_MULT_A) != 0)) {
            if (a < FRACUNIT)
                return;
        }
        
        // Stuff these into locals to hint to the compiler that they will not change
        const uint32_t srcW = srcImg.width;
        const uint32_t srcH = srcImg.height;
        const uint16_t* const pSrcPixels = (const uint16_t*) srcImg.pPixels;

        // Where to start and stop outputing to and the pointer step to make on each output pixel
        BLIT_ASSERT(dstX < dstW);
        BLIT_ASSERT(dstY < dstH);
        uint32_t* pDstPixel = pDstPixels + dstY * dstW + dstX;
        uint32_t* pEndDstPixel;
        uint32_t dstStep;

        if constexpr (BC_FLAGS & BCF_HORZ_COLUMN) {
            BLIT_ASSERT(dstX + dstCount <= dstW);
            pEndDstPixel = pDstPixel + dstCount;
            dstStep = 1;
        }
        else {
            BLIT_ASSERT(dstY + dstCount <= dstH);
            pEndDstPixel = pDstPixel + dstCount * dstW;
            dstStep = dstW;
        }

        // Where to start reading from the in the source image; optimize for certain scenarios based on the input flags...
        // For example if the image is in column major format (the default) and we are only stepping in the y direction
        // then we can just index into a column rather than doing full two dimensional addressing.
        #if BLIT_ASSERT_ENABLED == 1
            if constexpr ((BC_FLAGS & BCF_H_WRAP_ANY) == 0) {
                BLIT_ASSERT(fixedToInt(srcX) >= 0);
                BLIT_ASSERT((uint32_t) fixedToInt(srcX) < srcW);
            }

            if constexpr ((BC_FLAGS & BCF_V_WRAP_ANY) == 0) {
                BLIT_ASSERT(fixedToInt(srcY) >= 0);
                BLIT_ASSERT((uint32_t) fixedToInt(srcY) < srcH);
            }
        #endif

        [[maybe_unused]] const uint16_t* pSrcRowOrCol;
        
        constexpr bool IS_SRC_COL_MAJOR = ((BC_FLAGS & BCF_ROW_MAJOR_IMG) == 0);
        constexpr bool IS_SRC_ROW_MAJOR = (!IS_SRC_COL_MAJOR);
        constexpr bool USE_SRC_ROW_INDEXING = (IS_SRC_ROW_MAJOR && ((BC_FLAGS & BCF_STEP_ANY) == BCF_STEP_X));
        constexpr bool USE_SRC_COL_INDEXING = (IS_SRC_COL_MAJOR && ((BC_FLAGS & BCF_STEP_ANY) == BCF_STEP_Y));

        if constexpr (USE_SRC_ROW_INDEXING) {
            // Stepping in x direction in a row major image.
            // Will index into a row instead of doing 2d indexing.
            pSrcRowOrCol = pSrcPixels + wrapYCoord<BC_FLAGS>(fixedToInt(srcY), srcH) * srcW;
        }
        else if constexpr (USE_SRC_COL_INDEXING) {
            // Stepping in y direction in a column major image.
            // Will index into a column instead of doing 2d indexing.
            pSrcRowOrCol = pSrcPixels + wrapXCoord<BC_FLAGS>(fixedToInt(srcX), srcW) * srcH;
        }

        // Main pixel blitting loop
        [[maybe_unused]] Fixed curSrcX = srcX;
        [[maybe_unused]] Fixed curSrcY = srcY;

        while (pDstPixel < pEndDstPixel) {
            // Get the source pixel (RGBA5551 format)
            uint16_t srcPixelRGBA5551;

            if constexpr (USE_SRC_ROW_INDEXING) {
                const uint32_t curSrcXInt = wrapXCoord<BC_FLAGS>(fixedToInt(curSrcX), srcW);
                srcPixelRGBA5551 = pSrcRowOrCol[curSrcXInt];
            }
            else if constexpr (USE_SRC_COL_INDEXING) {
                const uint32_t curSrcYInt = wrapYCoord<BC_FLAGS>(fixedToInt(curSrcY), srcH);
                srcPixelRGBA5551 = pSrcRowOrCol[curSrcYInt];
            }
            else {
                const uint32_t curSrcXInt = wrapXCoord<BC_FLAGS>(fixedToInt(curSrcX), srcW);
                const uint32_t curSrcYInt = wrapYCoord<BC_FLAGS>(fixedToInt(curSrcY), srcH);

                if constexpr (IS_SRC_COL_MAJOR) {
                    srcPixelRGBA5551 = pSrcPixels[curSrcXInt * srcH + curSrcYInt];
                }
                else {
                    srcPixelRGBA5551 = pSrcPixels[curSrcYInt * srcW + curSrcXInt];
                }
            }
            
            // Extract RGBA components.
            // In the case of RGB, shift such that the maximum value is 255 instead of 31.
            const uint16_t texR = (srcPixelRGBA5551 & uint16_t(0b1111100000000000)) >> 8;
            const uint16_t texG = (srcPixelRGBA5551 & uint16_t(0b0000011111000000)) >> 3;
            const uint16_t texB = (srcPixelRGBA5551 & uint16_t(0b0000000000111110)) << 2;
            [[maybe_unused]] const uint16_t texA = srcPixelRGBA5551 & 0x01;

            do {
                // Do the alpha test if enabled
                if constexpr ((BC_FLAGS & BCF_ALPHA_TEST) != 0) {
                    if (texA == 0)
                        break;
                }

                // Get the texture colors in 16.16 fixed point format.
                // Note that if we are not doing any color multiply these conversions would be redundant, but I'm guessing
                // that the compiler would be smart enough to optimize out the useless shifts in those cases (hopefully)!
                Fixed rFrac = intToFixed((int32_t) texR);
                Fixed gFrac = intToFixed((int32_t) texG);
                Fixed bFrac = intToFixed((int32_t) texB);
                Fixed aFrac = intToFixed((int32_t) texA);

                if constexpr ((BC_FLAGS & BCF_COLOR_MULT_RGB) != 0) {
                    rFrac = std::min(fixedMul(rFrac, r), 255 * FRACUNIT);
                    gFrac = std::min(fixedMul(gFrac, g), 255 * FRACUNIT);
                    bFrac = std::min(fixedMul(bFrac, b), 255 * FRACUNIT);
                }

                if constexpr ((BC_FLAGS & BCF_COLOR_MULT_A) != 0) {
                    aFrac = std::min(fixedMul(aFrac, a), FRACUNIT);
                }

                // Do alpha blending with the destination pixel if enabled
                if constexpr ((BC_FLAGS & BCF_ALPHA_BLEND) != 0) {
                    // Read the destination pixel RGB and convert to 16.16 fixed
                    const uint16_t dstPixelRGBA5551 = *pDstPixel;

                    const uint16_t dstR = (srcPixelRGBA5551 >> 11) << 3;
                    const uint16_t dstG = (srcPixelRGBA5551 >> 6) << 3;
                    const uint16_t dstB = (srcPixelRGBA5551 >> 1) << 3;

                    const Fixed dstRFrac = intToFixed(dstR);
                    const Fixed dstGFrac = intToFixed(dstG);
                    const Fixed dstBFrac = intToFixed(dstB);
                    
                    // Source and destination blend factors
                    const Fixed srcFactor = srcAFrac;
                    const Fixed dstFactor = FRACUNIT - aFrac;

                    // Do the blend
                    rFrac = fixedMul(rFrac, srcFactor) + fixedMul(dstRFrac, dstFactor);
                    gFrac = fixedMul(gFrac, srcFactor) + fixedMul(dstGFrac, dstFactor);
                    bFrac = fixedMul(bFrac, srcFactor) + fixedMul(dstBFrac, dstFactor);
                }

                // Write out the pixel value
                *pDstPixel = (
                    (uint32_t(uint8_t(fixedToInt(rFrac)) << 24)) |
                    (uint32_t(uint8_t(fixedToInt(gFrac)) << 16)) |
                    (uint32_t(uint8_t(fixedToInt(bFrac)) << 8))
                );
            } while (false);    // Dummy loop to allow break out due to alpha test cull!

            // Stepping in the source and destination
            if constexpr ((BC_FLAGS & BCF_STEP_X) != 0) {
                curSrcX += srcXStep;
            }

            if constexpr ((BC_FLAGS & BCF_STEP_Y) != 0) {
                curSrcY += srcYStep;
            }

            pDstPixel += dstStep;
        }
    }
}
