#include "UIUtils.h"

#include "Base/Tables.h"
#include "GFX/Blit.h"
#include "GFX/CelImages.h"
#include "GFX/Video.h"

BEGIN_NAMESPACE(UIUtils)

void drawUISprite(const int32_t x, const int32_t y, const CelImage& image) noexcept {
    const float xScaled = (float) x * gScaleFactor;
    const float yScaled = (float) y * gScaleFactor;
    const float wScaled = (float) image.width * gScaleFactor;
    const float hScaled = (float) image.height * gScaleFactor;

    Blit::blitSprite<
        Blit::BCF_ALPHA_TEST |
        Blit::BCF_H_CLIP |
        Blit::BCF_V_CLIP
    >(
        image.pPixels,
        image.width,
        image.height,
        0.0f,
        0.0f,
        (float) image.width,
        (float) image.height,
        Video::gpFrameBuffer,
        Video::gScreenWidth,
        Video::gScreenHeight,
        Video::gScreenWidth,
        xScaled,
        yScaled,
        wScaled,
        hScaled
    );
}

void drawUISprite(const int32_t x, const int32_t y, const uint32_t resourceNum) noexcept {
    const CelImage& img = CelImages::loadImage(resourceNum, CelLoadFlagBits::NONE);
    drawUISprite(0, 0, img);
    CelImages::releaseImages(resourceNum);
}

void drawMaskedUISprite(const int32_t x, const int32_t y, const uint32_t resourceNum) noexcept {
    const CelImage& img = CelImages::loadImage(resourceNum, CelLoadFlagBits::MASKED);
    drawUISprite(0, 0, img);
    CelImages::releaseImages(resourceNum);
}

void drawPlaque(const uint32_t resourceNum) noexcept  {
    const CelImage& img = CelImages::loadImage(resourceNum);
    drawUISprite(160 - img.width / 2, 80, img);
    CelImages::releaseImages(resourceNum);
}

END_NAMESPACE(UIUtils)
