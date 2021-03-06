#include "Sprites.h"

#include "Base/Endian.h"
#include "Base/Mem.h"
#include "Base/Resource.h"
#include "Game/DoomRez.h"
#include "Game/Resources.h"
#include "ThreeDO/CelUtils.h"
#include <algorithm>
#include <map>
#include <vector>

BEGIN_NAMESPACE(Sprites)

static std::vector<Sprite> gSprites;

//------------------------------------------------------------------------------------------------------------------------------------------
// Header for sprite data.
//
// This was known as a 'patch' previously in the 3DO source, but differed greatly to the PC version
// of the same data structure since it does not include the sprite size. The sprite size is already
// encoded in the CEL image data hence that would have been redundant...
//
// Note: The actual data for the sprite image follows this data structure.
//------------------------------------------------------------------------------------------------------------------------------------------
struct SpriteImageHeader {
    int16_t     leftOffset;     // Where the first column of the sprite gets drawn, in pixels to the left of it's position.
    int16_t     topOffset;      // Where the first row of the sprite gets drawn, in pixels above it's position.
};

//------------------------------------------------------------------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------------------------------------------------------------------

// Flag encoded in the offset to sprite frame data:
// If set then the particular angle of a sprite frame should be rendered flipped.
// This is used to save memory on certain animated sprites.
static constexpr uint32_t SPR_OFFSET_FLAG_FLIP = uint32_t(0x80000000);\

// Flag encoded in the offset to sprite frame data:
// If set the sprite frame data starts with 8 uint32_t offsets to the actual data for each direction.
// Otherwise only a single direction of data (and no offsets) are defined ahead in the data.
static constexpr uint32_t SPR_OFFSET_FLAG_ROTATED = uint32_t(0x40000000);

// Bit mask to remove sprite offsets flags
static constexpr uint32_t REMOVE_SPR_OFFSET_FLAGS_MASK = uint32_t(0x3FFFFFFF);

//------------------------------------------------------------------------------------------------------------------------------------------
// Get the sprite for a particular resource number.
// The resource number MUST be that for a sprite.
//------------------------------------------------------------------------------------------------------------------------------------------
static Sprite& getSpriteForResourceNum(const uint32_t resourceNum) noexcept {
    ASSERT(resourceNum >= getFirstSpriteResourceNum());
    ASSERT(resourceNum < getEndSpriteResourceNum());

    const uint32_t spriteIndex = resourceNum - getFirstSpriteResourceNum();
    return gSprites[spriteIndex];
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Frees the texture data associated with a sprite
//------------------------------------------------------------------------------------------------------------------------------------------
static void freeSprite(Sprite& sprite) noexcept {
    // Gather up all the texture data pointers that need to be freed
    std::vector<uint16_t*> tmpTexturePtrList;

    {
        SpriteFrame* pCurSpriteFrame = sprite.pFrames;
        SpriteFrame* const pEndSpriteFrame = sprite.pFrames + sprite.numFrames;

        uint16_t* pPrevTexture = nullptr;   // Used to help avoid some dupes in the list

        while (pCurSpriteFrame < pEndSpriteFrame) {
            for (SpriteFrameAngle& angle : pCurSpriteFrame->angles) {
                uint16_t* const pAngleTexture = angle.pTexture;

                if (pAngleTexture && pAngleTexture != pPrevTexture) {
                    tmpTexturePtrList.push_back(pAngleTexture);
                    pPrevTexture = pAngleTexture;
                }
            }

            ++pCurSpriteFrame;
        }
    }

    // Sort the pointers list so we can deduplicate
    std::sort(tmpTexturePtrList.begin(), tmpTexturePtrList.end());

    // Now free all the pointers and cleanup the temp list
    {
        uint16_t* pPrevFreedTexture = nullptr;

        for (uint16_t* pTexture : tmpTexturePtrList) {
            if (pTexture != pPrevFreedTexture) {
                MemFree(pTexture);
                pPrevFreedTexture = pTexture;
            }
        }

        tmpTexturePtrList.clear();
    }

    // Release the frame list and clear all sprite fields
    delete[] sprite.pFrames;
    sprite = {};
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Reads the data for a sprite frame header
//------------------------------------------------------------------------------------------------------------------------------------------
static SpriteImageHeader readSpriteFrameHeader(const std::byte* const pData) noexcept {
    SpriteImageHeader header = *((const SpriteImageHeader*)pData);
    Endian::convertBigToHost(header.leftOffset);
    Endian::convertBigToHost(header.topOffset);
    return header;
}

void init() noexcept {
    ASSERT(gSprites.empty());
    gSprites.resize(getNumSprites());
}

void shutdown() noexcept {
    freeAll();
    gSprites.clear();
}

void freeAll() noexcept {
    for (Sprite& sprite : gSprites) {
        freeSprite(sprite);
    }
}

uint32_t getNumSprites() noexcept {
    return uint32_t(rLASTSPRITE - rFIRSTSPRITE);
}

uint32_t getFirstSpriteResourceNum() noexcept {
    return uint32_t(rFIRSTSPRITE);
}

uint32_t getEndSpriteResourceNum() noexcept {
    return uint32_t(rLASTSPRITE);
}

const Sprite* get(const uint32_t resourceNum) noexcept {
    Sprite& sprite = getSpriteForResourceNum(resourceNum);
    return &sprite;
}

const Sprite* load(const uint32_t resourceNum) noexcept {
    // Just give back the sprite if it is already loaded
    Sprite& sprite = getSpriteForResourceNum(resourceNum);
    const bool bIsSpriteLoaded = (sprite.pFrames != nullptr);

    if (bIsSpriteLoaded) {
        return &sprite;
    }

    // Otherwise load the raw sprite data and then determine the number of sprite frames defined for this resource by
    // reading the offset to the data for the first sprite frame. This offset tells us the size of the 'uint32_t' frame
    // offsets array at the start of the data, and thus the number of frames:
    const Resource* const pSpriteResouce = Resources::load(resourceNum);
    const uint32_t spriteDataSize = pSpriteResouce->size;
    const std::byte* const pSpriteData = (const std::byte*) pSpriteResouce->pData;
    const uint32_t* const pFrameOffsets = (const uint32_t*) pSpriteData;

    const uint32_t firstFrameOffset = Endian::bigToHost(pFrameOffsets[0]);
    ASSERT_LOG(firstFrameOffset % sizeof(uint32_t) == 0, "Expect offset to be in multiples of 'uint32_t'!");
    const uint32_t numFrames = (firstFrameOffset & REMOVE_SPR_OFFSET_FLAGS_MASK) / sizeof(uint32_t);

    // Fill in basic sprite info and alloc frames
    sprite.pFrames = new SpriteFrame[numFrames];
    sprite.numFrames = numFrames;
    sprite.resourceNum = resourceNum;

    // The code below only works if sizeof(uint32_t) <= sizeof(uintptr_t).
    // We use the pointers to image data to store offsets to image data initially...
    static_assert(sizeof(uint32_t) <= sizeof(uintptr_t));

    // Store what offsets in the data are requested to be decoded here and the actual decoded image.
    // Only want to load each unique images - some frames may use duplicate or flipped sprite data!
    struct DecodedImage {
        uint16_t*   pPixels;
        uint16_t    width;
        uint16_t    height;
        uint16_t    _unused[2];
    };

    std::map<uint32_t, DecodedImage> decodedImages;

    // Start reading the info for each frame and build up a list of what we need to decode
    for (uint32_t frameIdx = 0; frameIdx < numFrames; ++frameIdx) {
        SpriteFrame& frame = sprite.pFrames[frameIdx];

        const uint32_t frameOffsetWithFlags = Endian::bigToHost(pFrameOffsets[frameIdx]);
        const uint32_t frameOffset = frameOffsetWithFlags & REMOVE_SPR_OFFSET_FLAGS_MASK;

        if (frameOffsetWithFlags & SPR_OFFSET_FLAG_ROTATED) {
            // Sprite frame defines data for different angles
            const uint32_t* const pFrameAngleOffsets = (const uint32_t*)(pSpriteData + frameOffset);

            for (uint8_t angle = 0; angle < NUM_SPRITE_DIRECTIONS; ++angle) {
                // Read the header for this angle and fill in its info
                const uint32_t frameAngleOffsetWithFlags = frameOffset + Endian::bigToHost(pFrameAngleOffsets[angle]);
                const uint32_t frameAngleOffset = frameAngleOffsetWithFlags & REMOVE_SPR_OFFSET_FLAGS_MASK;
                const uint32_t imageDataOffset = frameAngleOffset + sizeof(SpriteImageHeader);

                SpriteImageHeader header = readSpriteFrameHeader(pSpriteData + frameAngleOffset);

                SpriteFrameAngle& frameAngle = frame.angles[angle];
                frameAngle.pTexture = (uint16_t*)(uintptr_t) imageDataOffset;   // Hold the desired image offset for now
                frameAngle.width = 0;                                           // Unknown until we load the image
                frameAngle.height = 0;                                          // Unknown until we load the image
                frameAngle.flipped = ((frameAngleOffsetWithFlags & SPR_OFFSET_FLAG_FLIP) != 0);
                frameAngle.leftOffset = header.leftOffset;
                frameAngle.topOffset = header.topOffset;

                // Ensure there is an entry making note to load this image
                decodedImages[imageDataOffset];
            }
        }
        else {
            // No data for different angles, the actual sprite data follows read the header and fill in its info
            const uint32_t imageDataOffset = frameOffset + sizeof(SpriteImageHeader);

            SpriteImageHeader header = readSpriteFrameHeader(pSpriteData + frameOffset);

            SpriteFrameAngle& frameAngle = frame.angles[0];
            frameAngle.pTexture = (uint16_t*)(uintptr_t) imageDataOffset;       // Hold the desired image offset for now
            frameAngle.width = 0;                                               // Unknown until we load the image
            frameAngle.height = 0;                                              // Unknown until we load the image
            frameAngle.flipped = ((frameOffsetWithFlags & SPR_OFFSET_FLAG_FLIP) != 0);
            frameAngle.leftOffset = header.leftOffset;
            frameAngle.topOffset = header.topOffset;

            // Copy the data for this angle to other angles (all angles are the same)
            static_assert(NUM_SPRITE_DIRECTIONS == 8, "This code only works for 8 directions!");

            frame.angles[1] = frame.angles[0];
            frame.angles[2] = frame.angles[0];
            frame.angles[3] = frame.angles[0];
            frame.angles[4] = frame.angles[0];
            frame.angles[5] = frame.angles[0];
            frame.angles[6] = frame.angles[0];
            frame.angles[7] = frame.angles[0];

            // Ensure there is an entry making note to load this image
            decodedImages[imageDataOffset];
        }
    }

    // Now start loading the data for all images
    for (auto iter = decodedImages.begin(), endIter = decodedImages.end(); iter != endIter; ++iter) {
        // Figure out the size of the data for the image to decode.
        // Either use the offset of the next image to determine this or the offset of the entire sprite data's end:
        const uint32_t imageDataOffset = iter->first;
        DecodedImage& decodedImage = iter->second;

        uint32_t imageDataSize;
        auto nextIter = iter;
        ++nextIter;

        if (nextIter != endIter) {
            const uint32_t nextImageOffset = nextIter->first;
            imageDataSize = nextImageOffset - imageDataOffset;
        } else {
            imageDataSize = spriteDataSize - imageDataOffset;
        }
        
        // Decode the sprite's image
        CelImage celImg;
        const bool bLoadedSpriteOk = CelUtils::loadRezFileCelImage(
            pSpriteData + imageDataOffset,
            imageDataSize,
            CelLoadFlagBits::NONE,
            celImg
        );

        if (!bLoadedSpriteOk) {
            FATAL_ERROR("Failed to load a sprite used by the game!");
        }

        decodedImage.pPixels = celImg.pPixels;
        decodedImage.width = celImg.width;
        decodedImage.height = celImg.height;
    }

    // Now once we have all the image data, fill in the actual texture info for all sprite frames
    for (uint32_t frameIdx = 0; frameIdx < numFrames; ++frameIdx) {
        SpriteFrame& frame = sprite.pFrames[frameIdx];

        for (SpriteFrameAngle& angle : frame.angles) {
            const uint32_t requestedImageOffset = (uint32_t)(uintptr_t) angle.pTexture;

            const DecodedImage& decodedImage = decodedImages.at(requestedImageOffset);
            ASSERT(decodedImage.width > 0);
            ASSERT(decodedImage.height > 0);

            // Note: Doom sprites are stored in COLUMN MAJOR format, so the width is actually the height and visa versa...
            // Swap them here to account for this!
            angle.pTexture = decodedImage.pPixels;
            angle.width = decodedImage.height;
            angle.height = decodedImage.width;
        }
    }

    // Finally return the newly loaded sprite
    return &sprite;
}

void free(const uint32_t resourceNum) noexcept {
    Sprite& sprite = getSpriteForResourceNum(resourceNum);
    freeSprite(sprite);
}

END_NAMESPACE(Sprites)
