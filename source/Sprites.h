#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------
// Represents the image to use for one angle of one frame in a sprite
//---------------------------------------------------------------------------------------------------------------------
typedef struct SpriteFrameAngle {
    uint16_t*   pTexture;       // The sprite texture to use for the frame
    uint16_t    width;          // Width of sprite texture
    uint16_t    height : 15;    // Height of sprite texture
    uint16_t    flipped : 1;    // If '1' then the frame is flipped horizontally when rendered
    int16_t     leftOffset;     // Where the first column of the sprite gets drawn, in pixels to the left of it's position.
    int16_t     topOffset;      // Where the first row of the sprite gets drawn, in pixels above it's position.    
} SpriteFrameAngle;

//---------------------------------------------------------------------------------------------------------------------
// Represents the images to use for all angles of all frames in a sprite
//---------------------------------------------------------------------------------------------------------------------
typedef struct SpriteFrame {
    SpriteFrameAngle    angles[8];
} SpriteFrame;

//---------------------------------------------------------------------------------------------------------------------
// Represents the all of the frames in a particular sprite
//---------------------------------------------------------------------------------------------------------------------
typedef struct Sprite {
    SpriteFrame*    pFrames;
    uint32_t        numFrames;
    uint32_t        resourceNum;
} Sprite;

void spritesInit();
void spritesShutdown();
void spritesFreeAll();

uint32_t getNumSprites();
uint32_t getFirstSpriteResourceNum();
uint32_t getEndSpriteResourceNum();     // N.B: 1 past the end index!

// N.B: Resource number MUST be within the range of resource numbers used for sprites!
// To check if valid, query the start and end sprite resource number.
Sprite* getSprite(const uint32_t resourceNum);
void loadSprite(const uint32_t resourceNum);
void freeSprite(const uint32_t resourceNum);

#ifdef __cplusplus
}
#endif
