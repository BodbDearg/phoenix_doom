#pragma once

#include <stdint.h>

//---------------------------------------------------------------------------------------------------------------------
// Constants
//---------------------------------------------------------------------------------------------------------------------
#define NUM_SPRITE_DIRECTIONS 8

//---------------------------------------------------------------------------------------------------------------------
// Represents the image to use for one angle of one frame in a sprite
//---------------------------------------------------------------------------------------------------------------------
typedef struct SpriteFrameAngle {
    uint16_t*   pTexture;       // The sprite texture to use for the frame. This texture is in RGBA5551 format and COLUMN MAJOR.
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
    SpriteFrameAngle    angles[NUM_SPRITE_DIRECTIONS];
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

//---------------------------------------------------------------------------------------------------------------------
// Notes:
//  (1) Sprites are only loaded if not already loaded.
//  (2) Resource number given MUST be within the range of resource numbers used for sprites!
//      To check if valid, query the start and end sprite resource number.
//---------------------------------------------------------------------------------------------------------------------
const Sprite* getSprite(const uint32_t resourceNum);
const Sprite* loadSprite(const uint32_t resourceNum);
void freeSprite(const uint32_t resourceNum);
