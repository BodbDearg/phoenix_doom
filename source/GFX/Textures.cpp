#include "Textures.h"

#include "Base/Endian.h"
#include "Base/Macros.h"
#include "Game/Data.h"
#include "Game/DoomRez.h"
#include "Game/Resources.h"
#include <vector>

BEGIN_NAMESPACE(Textures)

struct TextureInfoHeader {
    uint32_t    numWallTextures;
    uint32_t    firstWallTexture;       // Resource number
    uint32_t    numFlatTextures;
    uint32_t    firstFlatTexture;       // Resource number
    
    void swapEndian() noexcept {
        byteSwapU32(numWallTextures);
        byteSwapU32(firstWallTexture);
        byteSwapU32(numFlatTextures);
        byteSwapU32(firstFlatTexture);
    }
};

struct TextureInfoEntry {
    uint32_t    width;
    uint32_t    height;
    uint32_t    _unused;
    
    void swapEndian() noexcept {
        byteSwapU32(width);
        byteSwapU32(height);
    }
};

static uint32_t                 gFirstWallTexResourceNum;
static uint32_t                 gFirstFlatTexResourceNum;
static std::vector<Texture>     gWallTextures;
static std::vector<Texture>     gFlatTextures;

//----------------------------------------------------------------------------------------------------------------------
// Decode a 3DO Doom wall texture to an RGBA5551 image for rendering.
//----------------------------------------------------------------------------------------------------------------------
static void decodeWallTextureImage(Texture& tex, const std::byte* const pBytes) noexcept {
    // I'm not sure how to handle odd sized images, might need to align to next nearest byte on a column end maybe...
    // I don't think this happens in reality however so I'll just assert this assumption for now:
    ASSERT((tex.data.width & 0x1) == 0);
    ASSERT((tex.data.height & 0x1) == 0);

    // The pixel lookup table is always at the start and consists of 16 ARGB1555 pixels (32 bytes).
    // Each pixel itself is encoded as 4-bit values (16 colors).
    const uint16_t* const pPLUT = reinterpret_cast<const uint16_t*>(pBytes);
    const uint8_t* const pSrcPixels = reinterpret_cast<const uint8_t*>(pBytes + 32);

    // Allocate the output iamge
    const uint32_t numPixels = tex.data.width * tex.data.height;
    tex.data.pPixels = reinterpret_cast<uint16_t*>(MemAlloc(numPixels * sizeof(uint16_t)));

    // Decode all the pixels, doing 2 at a time.
    // Each pixel is a 4-bit index into the pixel lookup table, and 2 pixels are packed into each byte.
    {
        const uint8_t* pCurSrcPixels = pSrcPixels;
        uint16_t* pCurDstPixels = tex.data.pPixels;
        uint16_t* const pEndDstPixels = tex.data.pPixels + numPixels;

        while (pCurDstPixels < pEndDstPixels) {
            // Read the color indexes for these two source pixels (4-bits each)
            const uint8_t colorIndexes = *pCurSrcPixels;
            const uint8_t color1Idx = colorIndexes >> 4;
            const uint8_t color2Idx = colorIndexes & uint8_t(0x0F);
            ++pCurSrcPixels;

            // Read and save the ARGB1555 pixels (note: need to correct endian too)
            const uint16_t color1ARGB = byteSwappedU16(pPLUT[color1Idx]);
            const uint16_t color2ARGB = byteSwappedU16(pPLUT[color2Idx]);

            pCurDstPixels[0] = color1ARGB;
            pCurDstPixels[1] = color2ARGB;

            pCurDstPixels += 2;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Decode a 3DO Doom flat texture to an RGBA5551 image for rendering.
//----------------------------------------------------------------------------------------------------------------------
static void decodeFlatTextureImage(Texture& tex, const std::byte* const pBytes) noexcept {
    // Flats should always be 64x64 in 3DO Doom!
    ASSERT(tex.data.width == 64);
    ASSERT(tex.data.height == 64);

    // The pixel lookup table is always at the start and consists of 32 ARGB1555 pixels (64 bytes).
    // Each pixel itself is encoded as 5-bit values (32 colors) in an 8-bit byte.
    const uint16_t* const pPLUT = reinterpret_cast<const uint16_t*>(pBytes);
    const uint8_t* const pSrcPixels = reinterpret_cast<const uint8_t*>(pBytes + 64);

    // Allocate the output image
    constexpr uint32_t numPixels = 64 * 64;
    tex.data.pPixels = reinterpret_cast<uint16_t*>(MemAlloc(numPixels * sizeof(uint16_t)));

    // Decode all the pixels, doing 8 at a time (to allow for more CPU pipelining)
    {
        const uint8_t* pCurSrcPixels = pSrcPixels;
        uint16_t* pCurDstPixels = tex.data.pPixels;
        uint16_t* const pEndDstPixels = tex.data.pPixels + numPixels;

        while (pCurDstPixels < pEndDstPixels) {
            // Read the color indexes for the 8 source pixels and ensure only 5-bits are used (32 colors max)
            const uint8_t color1Idx = pCurSrcPixels[0] & uint8_t(0x1F);
            const uint8_t color2Idx = pCurSrcPixels[1] & uint8_t(0x1F);
            const uint8_t color3Idx = pCurSrcPixels[2] & uint8_t(0x1F);
            const uint8_t color4Idx = pCurSrcPixels[3] & uint8_t(0x1F);
            const uint8_t color5Idx = pCurSrcPixels[4] & uint8_t(0x1F);
            const uint8_t color6Idx = pCurSrcPixels[5] & uint8_t(0x1F);
            const uint8_t color7Idx = pCurSrcPixels[6] & uint8_t(0x1F);
            const uint8_t color8Idx = pCurSrcPixels[7] & uint8_t(0x1F);
            pCurSrcPixels += 8;

            // Read and save the ARGB1555 pixels (note: need to correct endian too)
            const uint16_t color1ARGB = byteSwappedU16(pPLUT[color1Idx]);
            const uint16_t color2ARGB = byteSwappedU16(pPLUT[color2Idx]);
            const uint16_t color3ARGB = byteSwappedU16(pPLUT[color3Idx]);
            const uint16_t color4ARGB = byteSwappedU16(pPLUT[color4Idx]);
            const uint16_t color5ARGB = byteSwappedU16(pPLUT[color5Idx]);
            const uint16_t color6ARGB = byteSwappedU16(pPLUT[color6Idx]);
            const uint16_t color7ARGB = byteSwappedU16(pPLUT[color7Idx]);
            const uint16_t color8ARGB = byteSwappedU16(pPLUT[color8Idx]);

            pCurDstPixels[0] = color1ARGB;
            pCurDstPixels[1] = color2ARGB;
            pCurDstPixels[2] = color3ARGB;
            pCurDstPixels[3] = color4ARGB;
            pCurDstPixels[4] = color5ARGB;
            pCurDstPixels[5] = color6ARGB;
            pCurDstPixels[6] = color7ARGB;
            pCurDstPixels[7] = color8ARGB;

            pCurDstPixels += 8;
        }
    }
}

static void loadTexture(Texture& tex, uint32_t textureNum, const bool bIsWallTexture) noexcept {
    const std::byte* const pRawTexBytes = Resources::loadData(tex.resourceNum);

    if (bIsWallTexture) {
        decodeWallTextureImage(tex, pRawTexBytes);
    }
    else {
        decodeFlatTextureImage(tex, pRawTexBytes);
    }

    Resources::free(tex.resourceNum);       // Don't need the raw data anymore!
    tex.animTexNum = textureNum;            // Initially the texture is not animated to display another frame
}

static void freeTexture(Texture& tex) noexcept {
    MEM_FREE_AND_NULL(tex.data.pPixels);
}

static void freeTextures(std::vector<Texture>& textures) noexcept {
    for (Texture& texture : textures) {
        freeTexture(texture);
    }
}

static void clearTextures(std::vector<Texture>& textures) noexcept {
    freeTextures(textures);
    textures.clear();
}

void init() noexcept {
    ASSERT(gWallTextures.empty());
    ASSERT(gFlatTextures.empty());

    // Read the header for all the texture info.
    // Note that we do NOT byte swap the original resources the may be cached and reused multiple times.
    // If we byte swapped the originals then we might double swap back to big endian accidently...
    const std::byte* pData = (const std::byte*) Resources::loadData(rTEXTURE1);
    
    TextureInfoHeader header = (const TextureInfoHeader&) *pData;
    header.swapEndian();
    pData += sizeof(TextureInfoHeader);
    
    // Save global texture info and alloc memory for all the texture entries
    gFirstWallTexResourceNum = header.firstWallTexture;
    gFirstFlatTexResourceNum = header.firstFlatTexture;
    gWallTextures.resize(header.numWallTextures);
    gFlatTextures.resize(header.numFlatTextures);
    
    // Read the texture info for each wall texture
    {
        const uint32_t numWallTex = (uint32_t) gWallTextures.size();
        
        for (uint32_t wallTexNum = 0; wallTexNum < numWallTex; ++wallTexNum) {
            TextureInfoEntry info = (const TextureInfoEntry&) *pData;
            info.swapEndian();
            pData += sizeof(TextureInfoEntry);
            
            Texture& texture = gWallTextures[wallTexNum];
            texture.data.width = info.width;
            texture.data.height = info.height;
            texture.resourceNum = header.firstWallTexture + wallTexNum;
            texture.animTexNum = wallTexNum;    // Points to itself initallly (no anim)
        }
    }
    
    // Now done with this resource
    Resources::free(rTEXTURE1);
    
    // We don't have texture info for flats, all flats for 3DO are 64x64.
    // This was done orignally to help optimize the flat renderer, which was done in software on the 3DO's CPU.
    {
        const uint32_t numFlatTex = (uint32_t) gFlatTextures.size();
        
        for (uint32_t flatTexNum = 0; flatTexNum < numFlatTex; ++flatTexNum) {
            Texture& texture = gFlatTextures[flatTexNum];
            texture.data.width = 64;
            texture.data.height = 64;
            texture.resourceNum = header.firstFlatTexture + flatTexNum;
        }
    }
}

void shutdown() noexcept {
    clearTextures(gWallTextures);
    clearTextures(gFlatTextures);
    gFirstWallTexResourceNum = 0;
    gFirstFlatTexResourceNum = 0;
}

void freeAll() noexcept {
    freeTextures(gWallTextures);
    freeTextures(gFlatTextures);
}

uint32_t getNumWallTextures() noexcept {
    return (uint32_t) gWallTextures.size();
}

uint32_t getNumFlatTextures() noexcept {
    return (uint32_t) gFlatTextures.size();
}

uint32_t getSky1TexNum() noexcept {
    return (uint32_t) rSKY1 - gFirstWallTexResourceNum;
}

uint32_t getSky2TexNum() noexcept {
    return (uint32_t) rSKY2 - gFirstWallTexResourceNum;
}

uint32_t getSky3TexNum() noexcept {
    return (uint32_t) rSKY3 - gFirstWallTexResourceNum;
}

uint32_t getCurrentSkyTexNum() noexcept {
    if (gGameMap < 9 || gGameMap == 24) {
        return getSky1TexNum();
    } else if (gGameMap < 18) {
        return getSky2TexNum();
    } else {
        return getSky3TexNum();
    }
}

const Texture* getWall(const uint32_t num) noexcept {
    ASSERT(num < gWallTextures.size());
    return &gWallTextures[num];
}

const Texture* getFlat(const uint32_t num) noexcept {
    ASSERT(num < gFlatTextures.size());
    return &gFlatTextures[num];
}

void loadWall(const uint32_t num) noexcept {
    ASSERT(num < gWallTextures.size());
    loadTexture(gWallTextures[num], num, true);
}

void loadFlat(const uint32_t num) noexcept {
    ASSERT(num < gFlatTextures.size());
    loadTexture(gFlatTextures[num], num, false);
}

void freeWall(const uint32_t num) noexcept {
    ASSERT(num < gWallTextures.size());
    freeTexture(gWallTextures[num]);
}

void freeFlat(const uint32_t num) noexcept {
    ASSERT(num < gFlatTextures.size());
    freeTexture(gFlatTextures[num]);
}

void setWallAnimTexNum(const uint32_t num, const uint32_t animTexNum) noexcept {
    ASSERT(num < gWallTextures.size());
    ASSERT(animTexNum < gWallTextures.size());
    gWallTextures[num].animTexNum = animTexNum;
}

void setFlatAnimTexNum(const uint32_t num, const uint32_t animTexNum) noexcept {
    ASSERT(num < gFlatTextures.size());
    ASSERT(animTexNum < gFlatTextures.size());
    gFlatTextures[num].animTexNum = animTexNum;
}

const Texture* getWallAnim(const uint32_t num) noexcept {
    const Texture* const pOrigTexture = getWall(num);
    return getWall(pOrigTexture->animTexNum);
}

const Texture* getFlatAnim(const uint32_t num) noexcept {
    const Texture* const pOrigTexture = getFlat(num);
    return getFlat(pOrigTexture->animTexNum);
}

END_NAMESPACE(Textures)
