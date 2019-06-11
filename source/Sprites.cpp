#include "Sprites.h"

#include "doomrez.h"
#include "Mem.h"
#include <algorithm>
#include <vector>

static std::vector<Sprite>  gSprites;
static std::vector<void*>   gTmpPtrList;

//--------------------------------------------------------------------------------------------------
// Get the sprite for a particular resource number.
// The resource number MUST be that for a sprite.
//--------------------------------------------------------------------------------------------------
static Sprite& getSpriteForResourceNum(const uint32_t resourceNum) noexcept {
    ASSERT(resourceNum >= getFirstSpriteResourceNum());
    ASSERT(resourceNum < getEndSpriteResourceNum());

    const uint32_t spriteIndex = resourceNum - getFirstSpriteResourceNum();
    return gSprites[spriteIndex];
}

//--------------------------------------------------------------------------------------------------
// Frees the texture data associated with a sprite
//--------------------------------------------------------------------------------------------------
static void freeSprite(Sprite& sprite) noexcept {
    // Gather up all the texture data pointers that need to be freed
    ASSERT(gTmpPtrList.empty());

    {
        SpriteFrame* pCurSpriteFrame = sprite.pFrames;
        SpriteFrame* const pEndSpriteFrame = sprite.pFrames + sprite.numFrames;

        void* pPrevTexture = nullptr;   // Used to help avoid some dupes in the list

        while (pCurSpriteFrame < pEndSpriteFrame) {
            for (SpriteFrameAngle& angle : pCurSpriteFrame->angles) {
                void* const pAngleTexture = angle.pTexture;

                if (pAngleTexture && pAngleTexture != pPrevTexture) {
                    gTmpPtrList.push_back(pAngleTexture);
                    pPrevTexture = pAngleTexture;
                }
            }

            ++pCurSpriteFrame;
        }
    }

    // Sort the pointers list so we can deduplicate
    std::sort(gTmpPtrList.begin(), gTmpPtrList.end());

    // Now free all the pointers and cleanup the temp list
    {
        void* pPrevFreedTexture = nullptr;

        for (void* pTexture : gTmpPtrList) {
            if (pTexture != pPrevFreedTexture) {
                MemFree(pTexture);
                pPrevFreedTexture = pTexture;
            }
        }

        gTmpPtrList.clear();
    }

    // Finally reset all sprite fields
    sprite = {};
}

#ifdef __cplusplus
extern "C" {
#endif

void spritesInit() {
    gSprites.resize(getNumSprites());
}

void spritesShutdown() {
    spritesFreeAll();
    gSprites.clear();
}

void spritesFreeAll() {
    for (Sprite& sprite : gSprites) {
        freeSprite(sprite);
    }
}

uint32_t getNumSprites() {
    return uint32_t(rLASTSPRITE - rFIRSTSPRITE);
}

uint32_t getFirstSpriteResourceNum() {
    return uint32_t(rFIRSTSPRITE);
}

uint32_t getEndSpriteResourceNum() {
    return uint32_t(rLASTSPRITE);
}

Sprite* getSprite(const uint32_t resourceNum) {
    Sprite& sprite = getSpriteForResourceNum(resourceNum);
    return &sprite;
}

void loadSprite(const uint32_t resourceNum) {
    // TODO...
}

void freeSprite(const uint32_t resourceNum) {
    Sprite& sprite = getSpriteForResourceNum(resourceNum);
    freeSprite(sprite);
}

#ifdef __cplusplus
}
#endif
