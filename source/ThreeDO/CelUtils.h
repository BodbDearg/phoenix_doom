#pragma once

#include "Base/Macros.h"
#include <cstddef>
#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Utility stuff relating to 3DO CEL files.
// Needed because a lot of resources and all sprites in 3DO Doom are in the native 3DO 'CEL' format.
//
// Notes:
//  (1) All data is ASSUMED to be in BIG ENDIAN format - these functions will NOT work otherwise.
//      BE was the native endian format of the 3DO and it's development environment (68K Mac), hence if you are dealing
//      with CELs or CCBs it is assumed the data is big endian.
//  (2) Most of these functions make tons of simplifying assumptions and shortcuts surrounding CEL files that hold true
//      for 3DO Doom, but which won't work on all 3DO CEL files in general. They also make modifications for 3DO Doom's
//      own specific way of way storing CEL data. If you want a more complete and robust reference for how to
//      decode/encode 3DO CEL files, check out various resources available on the net, including the GIMP CEL plugin:
//          https://github.com/ewhac/gimp-plugin-3docel
//  (3) 3DO Doom also makes some additional extensions (I think?) to the cel format to allow for arrays of sprites as
//      well as additional offsets to apply to the sprites when rendered. Whether or not these things are expected
//      depends on the what the code loading these assets expects, I don't think it's possible to determine just by
//      looking at the data... Hence loading code must modify the behavior of the loader based on what it expects,
//      via load flags.
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Definition for a 3DO Cel Control Block (CCB).
// This is used for draw operations with the 3DO hardware, and within CEL files on disk.
// Obtained this from the 3DO SDK headers.
//
// More info about it here:
//  https://github.com/trapexit/3DO-information/blob/master/software/sdk/3DO%20Online%20Developer%20Documentation/ppgfldr/smmfldr/cdmfldr/08CDM001.html
//----------------------------------------------------------------------------------------------------------------------
struct CelControlBlock {
	uint32_t    flags;
    uint32_t    nextPtr;        // N.B: can't use CelControlBlock* since that introduces 64-bit incompatibilities!
	uint32_t    sourcePtr;      // N.B: can't use void* since that introduces 64-bit incompatibilities!
	uint32_t    plutPtr;        // N.B: can't use void* since that introduces 64-bit incompatibilities!
	int32_t     xPos;
	int32_t     yPos;
	int32_t     hdx;
	int32_t     hdy;
	int32_t     vdx;
	int32_t     vdy;
	int32_t     hddx;
	int32_t     hddy;
	uint32_t    pixc;
	uint32_t    pre0;
	uint32_t    pre1;
	int32_t     width;
	int32_t     height;
};

//----------------------------------------------------------------------------------------------------------------------
// Holds info and data for a single CEL image.
// Note that the pixels field is expected to be allocated with c++ 'new'.
//----------------------------------------------------------------------------------------------------------------------
struct CelImage {
    uint16_t    width;          // Size of the image
    uint16_t    height;
    int16_t     offsetX;        // Offset to apply to the image when rendering
    int16_t     offsetY;
    uint16_t*   pPixels;        // Pointer to the ARGB1555 format pixels for the image

    CelImage() noexcept
        : width(0)
        , height(0)
        , offsetX(0)
        , offsetY(0)
        , pPixels(nullptr)
    {
    }

    inline void free() noexcept {
        delete[] pPixels;
        pPixels = nullptr;
    }
};

//----------------------------------------------------------------------------------------------------------------------
// Structure storing an array of Cel images
//----------------------------------------------------------------------------------------------------------------------
struct CelImageArray {
    uint32_t    numImages;      // Number of images in the set of images
    uint32_t    loadFlags;      // Flags the images were loaded with
    CelImage*   pImages;        // Pointer to the CEL images

    CelImageArray() noexcept
        : numImages(0)
        , loadFlags(0)
        , pImages(nullptr)
    {
    }

    inline CelImage& getImage(const uint32_t number) const noexcept {
        ASSERT(number < numImages);
        return pImages[number];
    }

    inline void free() noexcept {
        if (pImages) {
            uint32_t remaining = numImages;

            while (remaining > 0) {
                --remaining;
                pImages[remaining].free();
            }

            delete[] pImages;
            pImages = nullptr;
        }

        numImages = 0;
    }
};

//----------------------------------------------------------------------------------------------------------------------
// Flags specifying how resources containing cel images or a single cel image are to be interpreted on load.
//
//  MASKED      : The color 0x7FFF is to be interpreted as the 'transparent' color.
//  HAS_OFFSETS : If set then 2 extra 16-bit int offsets are present before the actual Cel image for each image
//                in the array. This is used for player sprites in 3DO Doom to specify their position on screen.
//----------------------------------------------------------------------------------------------------------------------
typedef uint32_t CelLoadFlags;

namespace CelLoadFlagBits {
    static constexpr CelLoadFlags NONE          = 0x00000000;
    static constexpr CelLoadFlags MASKED        = 0x00000001;
    static constexpr CelLoadFlags HAS_OFFSETS   = 0x00000002;
}

BEGIN_NAMESPACE(CelUtils)

//----------------------------------------------------------------------------------------------------------------------
// Determine the width and height from the given Cel Control Block structure
//----------------------------------------------------------------------------------------------------------------------
extern uint16_t getCCBWidth(const CelControlBlock& ccb) noexcept;
extern uint16_t getCCBHeight(const CelControlBlock& ccb) noexcept;

//----------------------------------------------------------------------------------------------------------------------
// Load a single CEL image from the given data.
//----------------------------------------------------------------------------------------------------------------------
bool loadCelImage(
    const std::byte* const pData,
    const uint32_t dataSize,
    const CelLoadFlags loadFlags,
    CelImage& imageOut
) noexcept;

//----------------------------------------------------------------------------------------------------------------------
// Load an array of CEL images from the given data.
// The given data is expected to start with an array of offsets to each image.
// Saves the images to the given object.
//----------------------------------------------------------------------------------------------------------------------
bool loadCelImages(
    const std::byte* const pData,
    const uint32_t dataSize,
    const CelLoadFlags loadFlags,
    CelImageArray& imagesOut
) noexcept;

END_NAMESPACE(CelUtils)
