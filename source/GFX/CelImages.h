#pragma once

#include "Base/Macros.h"
#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Provides access to images and sprites loaded from the game resource file which are in the native 3DO 'Cel' format.
//
// 3DO Doom also makes some additional extensions (I think?) to the cel format to allow for arrays of sprites as well
// as additional offsets to apply to the sprites when rendered. Whether or not these things are expected depends on the
// what the code loading these assets expects, I don't think it's possible to determine just by looking data...
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Holds info and data for a single CEL image
//----------------------------------------------------------------------------------------------------------------------
struct CelImage {
    uint16_t    width;          // Size of the image
    uint16_t    height;
    int16_t     offsetX;        // Offset to apply to the image when rendering
    int16_t     offsetY;
    uint16_t*   pPixels;        // Pointer to the ARGB1555 format pixels for the image
};

//----------------------------------------------------------------------------------------------------------------------
// Structure storing an array of Cel images
//----------------------------------------------------------------------------------------------------------------------
struct CelImageArray {
    uint32_t    numImages;      // Number of images in the set of images
    uint32_t    loadFlags;      // Flags the images were loaded with
    CelImage*   pImages;        // Pointer to the CEL images

    inline CelImage& getImage(const uint32_t number) const noexcept {
        ASSERT(number < numImages);
        return pImages[number];
    }
};

BEGIN_NAMESPACE(CelImages)

//----------------------------------------------------------------------------------------------------------------------
// Flags specifying how resources containing cel images or a single cel image are to be interpreted on load.
//
//  MASKED      : The color 0x7FFF is to be interpreted as the 'transparent' color.
//  HAS_OFFSETS : If set then 2 extra 16-bit int offsets are present before the actual Cel image for each image
//                in the array. This is used for player sprites in 3DO Doom to specify their position on screen.
//----------------------------------------------------------------------------------------------------------------------
typedef uint32_t LoadFlags;

namespace LoadFlagBits {
    static constexpr LoadFlags NONE         = 0x00000000;
    static constexpr LoadFlags MASKED       = 0x00000001;
    static constexpr LoadFlags HAS_OFFSETS  = 0x00000002;
}

void init() noexcept;
void shutdown() noexcept;
void freeAll() noexcept;

const CelImageArray* getImages(const uint32_t resourceNum) noexcept;
const CelImage* getImage(const uint32_t resourceNum) noexcept;

//----------------------------------------------------------------------------------------------------------------------
// Notes:
//  (1) Whether or not the resource is interpreted as an image array depends on whether the
//      'loadImage' or 'loadImages' function is called.
//  (2) If the cel image/images resource is already loaded then nothing will be done.
//----------------------------------------------------------------------------------------------------------------------
const CelImageArray& loadImages(const uint32_t resourceNum, const LoadFlags loadFlags = LoadFlagBits::NONE) noexcept;
const CelImage& loadImage(const uint32_t resourceNum, const LoadFlags loadFlags = LoadFlagBits::NONE) noexcept;

void freeImages(const uint32_t resourceNum) noexcept;
void releaseImages(const uint32_t resourceNum) noexcept;

END_NAMESPACE(CelImages)
