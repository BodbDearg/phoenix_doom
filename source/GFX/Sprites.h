#pragma once

#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Provides access to Doom format sprites from the game's resource file.
//
// When I say 'Doom format' I mean that they are in a format specific to this game and very much distinct from the
// generalized 3DO format ('CEL') files used elsewhere in the game. Note: the sprites used here are also NOT in the same
// format as the original PC version of Doom, all of this has been tailored to the specifics of the 3DO port.
//----------------------------------------------------------------------------------------------------------------------

// The number of sprite angles in Doom (45 degree angle increments)
static constexpr uint32_t NUM_SPRITE_DIRECTIONS = 8;

//----------------------------------------------------------------------------------------------------------------------
// Represents the image to use for one angle of one frame in a sprite
//----------------------------------------------------------------------------------------------------------------------
struct SpriteFrameAngle {
    uint16_t*   pTexture;       // The sprite texture to use for the frame. This texture is in RGBA5551 format and COLUMN MAJOR.
    uint16_t    width;          // Width of sprite texture
    uint16_t    height : 15;    // Height of sprite texture
    uint16_t    flipped : 1;    // If '1' then the frame is flipped horizontally when rendered
    int16_t     leftOffset;     // Where the first column of the sprite gets drawn, in pixels to the left of it's position.
    int16_t     topOffset;      // Where the first row of the sprite gets drawn, in pixels above it's position.    
};

//----------------------------------------------------------------------------------------------------------------------
// Represents the images to use for all angles of all frames in a sprite
//----------------------------------------------------------------------------------------------------------------------
struct SpriteFrame {
    SpriteFrameAngle    angles[NUM_SPRITE_DIRECTIONS];
};

//----------------------------------------------------------------------------------------------------------------------
// Represents the all of the frames in a particular sprite
//----------------------------------------------------------------------------------------------------------------------
struct Sprite {
    SpriteFrame*    pFrames;
    uint32_t        numFrames;
    uint32_t        resourceNum;
};

void spritesInit();
void spritesShutdown();
void spritesFreeAll();

uint32_t getNumSprites();
uint32_t getFirstSpriteResourceNum();
uint32_t getEndSpriteResourceNum();     // N.B: 1 past the end index!

//----------------------------------------------------------------------------------------------------------------------
// Notes:
//  (1) With 'load' sprites are only loaded if not already loaded.
//  (2) Resource number given MUST be within the range of resource numbers used for sprites!
//      To check if valid, query the start and end sprite resource number.
//----------------------------------------------------------------------------------------------------------------------
const Sprite* getSprite(const uint32_t resourceNum);
const Sprite* loadSprite(const uint32_t resourceNum);
void freeSprite(const uint32_t resourceNum);
