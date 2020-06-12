#pragma once

#include "Base/Macros.h"
#include "ImageData.h"

//------------------------------------------------------------------------------------------------------------------------------------------
// Module that provides access to Doom format textures, in the form of wall and flat textures.
// Note that wall textures are stored in a column major format, similar to sprites.
// Flat textures are row major however...
//------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------
// Describes a texture for a wall or flat (floor)
//------------------------------------------------------------------------------------------------------------------------------------------
struct Texture {
    ImageData   data;           // The image data for the texture
    uint32_t    resourceNum;    // What resource this came from
    uint32_t    animTexNum;     // Number of the texture to use in place of this one currently, if the texture is animated
};

BEGIN_NAMESPACE(Textures)

void init() noexcept;
void shutdown() noexcept;
void freeAll() noexcept;

uint32_t getNumWallTextures() noexcept;
uint32_t getNumFlatTextures() noexcept;
uint32_t getFirstWallTexResourceNum() noexcept;
uint32_t getFirstFlatTexResourceNum() noexcept;

// N.B: Texture numbers must all be in range!
// If safety is required, check against the texture counts.
const Texture* getWall(const uint32_t num) noexcept;
const Texture* getFlat(const uint32_t num) noexcept;

void loadWall(const uint32_t num) noexcept;
void loadFlat(const uint32_t num) noexcept;
void freeWall(const uint32_t num) noexcept;
void freeFlat(const uint32_t num) noexcept;

void setWallAnimTexNum(const uint32_t num, const uint32_t animTexNum) noexcept;
void setFlatAnimTexNum(const uint32_t num, const uint32_t animTexNum) noexcept;

// Helpers that dereference 'animTexNum' for the specified texture number.
// They return the texture frame pointed to by 'animTexNum' within the specified texture.
const Texture* getWallAnim(const uint32_t num) noexcept;
const Texture* getFlatAnim(const uint32_t num) noexcept;

END_NAMESPACE(Textures)
