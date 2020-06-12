#pragma once

#include "ThreeDO/CelUtils.h"

//------------------------------------------------------------------------------------------------------------------------------------------
// Provides access to images and sprites loaded from the game resource file which are in the native 3DO 'Cel' format.
//------------------------------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(CelImages)

void init() noexcept;
void shutdown() noexcept;
void freeAll() noexcept;

const CelImageArray* getImages(const uint32_t resourceNum) noexcept;
const CelImage* getImage(const uint32_t resourceNum) noexcept;

//------------------------------------------------------------------------------------------------------------------------------------------
// Notes:
//  (1) Whether or not the resource is interpreted as an image array depends on whether the
//      'loadImage' or 'loadImages' function is called.
//  (2) If the cel image/images resource is already loaded then nothing will be done.
//------------------------------------------------------------------------------------------------------------------------------------------
const CelImageArray& loadImages(const uint32_t resourceNum, const CelLoadFlags loadFlags = CelLoadFlagBits::NONE) noexcept;
const CelImage& loadImage(const uint32_t resourceNum, const CelLoadFlags loadFlags = CelLoadFlagBits::NONE) noexcept;

void freeImages(const uint32_t resourceNum) noexcept;
void releaseImages(const uint32_t resourceNum) noexcept;

END_NAMESPACE(CelImages)
