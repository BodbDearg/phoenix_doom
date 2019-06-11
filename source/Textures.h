#pragma once

#include <stdint.h>

//---------------------------------------------------------------------------------------------------------------------
// Describes a texture for a wall or flat (floor)
//---------------------------------------------------------------------------------------------------------------------
typedef struct Texture {
    uint32_t    width;
    uint32_t    height;
    uint32_t    resourceNum;    // What resource this came from
    uint32_t    animTexNum;     // Number of the texture to use in place of this one currently, if the texture is animated
    void*       pData;
} Texture;

#ifdef __cplusplus
extern "C" {
#endif

void texturesInit();
void texturesShutdown();
void texturesFreeAll();

uint32_t getNumWallTextures();
uint32_t getNumFlatTextures();

uint32_t getSky1TexNum();
uint32_t getSky2TexNum();
uint32_t getSky3TexNum();
uint32_t getCurrentSkyTexNum();

// N.B: Texture numbers must all be in range!
// If safety is required, check against the texture counts.
const Texture* getWallTexture(const uint32_t num);
const Texture* getFlatTexture(const uint32_t num);

void loadWallTexture(const uint32_t num);
void loadFlatTexture(const uint32_t num);
void freeWallTexture(const uint32_t num);
void freeFlatTexture(const uint32_t num);

void setWallAnimTexNum(const uint32_t num, const uint32_t animTexNum);
void setFlatAnimTexNum(const uint32_t num, const uint32_t animTexNum);

// Helpers that dereference 'animTexNum' for the specified texture number.
// They return the texture frame pointed to by 'animTexNum' within the specified texture.
const Texture* getWallAnimTexture(const uint32_t num);
const Texture* getFlatAnimTexture(const uint32_t num);

#ifdef __cplusplus
}
#endif
