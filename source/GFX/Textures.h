#pragma once

#include "ImageData.h"

//---------------------------------------------------------------------------------------------------------------------
// Describes a texture for a wall or flat (floor)
//---------------------------------------------------------------------------------------------------------------------
struct Texture {
    ImageData   data;           // The image data for the texture
    uint32_t    resourceNum;    // What resource this came from
    uint32_t    animTexNum;     // Number of the texture to use in place of this one currently, if the texture is animated
};

void texturesInit() noexcept;
void texturesShutdown() noexcept;
void texturesFreeAll() noexcept;

uint32_t getNumWallTextures() noexcept;
uint32_t getNumFlatTextures() noexcept;

uint32_t getSky1TexNum() noexcept;
uint32_t getSky2TexNum() noexcept;
uint32_t getSky3TexNum() noexcept;
uint32_t getCurrentSkyTexNum() noexcept;

// N.B: Texture numbers must all be in range!
// If safety is required, check against the texture counts.
const Texture* getWallTexture(const uint32_t num) noexcept;
const Texture* getFlatTexture(const uint32_t num) noexcept;

void loadWallTexture(const uint32_t num) noexcept;
void loadFlatTexture(const uint32_t num) noexcept;
void freeWallTexture(const uint32_t num) noexcept;
void freeFlatTexture(const uint32_t num) noexcept;

void setWallAnimTexNum(const uint32_t num, const uint32_t animTexNum) noexcept;
void setFlatAnimTexNum(const uint32_t num, const uint32_t animTexNum) noexcept;

// Helpers that dereference 'animTexNum' for the specified texture number.
// They return the texture frame pointed to by 'animTexNum' within the specified texture.
const Texture* getWallAnimTexture(const uint32_t num) noexcept;
const Texture* getFlatAnimTexture(const uint32_t num) noexcept;
