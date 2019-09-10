#include "Setup.h"

#include "Base/Endian.h"
#include "Base/Mem.h"
#include "Base/Random.h"
#include "Base/Resource.h"
#include "Game/Data.h"
#include "Game/DoomMain.h"
#include "Game/DoomRez.h"
#include "Game/Resources.h"
#include "Game/Tick.h"
#include "GFX/Sprites.h"
#include "GFX/Textures.h"
#include "MapData.h"
#include "Specials.h"
#include "Switch.h"
#include "Things/MapObj.h"
#include "UI/UIUtils.h"
#include <cstring>

static constexpr uint32_t PRELOAD_TABLE[] = {
    rSPR_ZOMBIE,            // Zombiemen
    rSPR_SHOTGUY,           // Shotgun guys
    rSPR_IMP,               // Imps
    rSPR_DEMON,             // Demons
    rSPR_CACODEMON,         // Cacodemons
    rSPR_LOSTSOUL,          // Lost souls
    rSPR_BARON,             // Baron of Hell
    rSPR_OURHEROBDY,        // Our dead hero
    rSPR_BARREL,            // Exploding barrel
    rSPR_SHOTGUN,           // Shotgun on floor
    rSPR_CLIP,              // Clip of bullets
    rSPR_SHELLS,            // 4 shotgun shells
    rSPR_STIMPACK,          // Stimpack
    rSPR_MEDIKIT,           // Med-kit
    rSPR_GREENARMOR,        // Normal armor
    rSPR_BLUEARMOR,         // Mega armor
    rSPR_HEALTHBONUS,       // Health bonus
    rSPR_ARMORBONUS,        // Armor bonus
    rSPR_BLUD,              // Blood from bullet hit
    rSPR_PUFF,              // Gun sparks on wall
    UINT32_MAX
};

static line_t** gppLineArrayBuffer;     // Pointer to array of line_t pointers used by sectors

mapthing_t      gDeathmatchStarts[10];      // Deathmatch starts
mapthing_t*     gpDeathmatch;
mapthing_t      gPlayerStarts;              // Starting position for players
uint32_t        gSkyTextureNum;

//----------------------------------------------------------------------------------------------------------------------
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//----------------------------------------------------------------------------------------------------------------------
static void GroupLines() noexcept {
    // Count number of lines in each sector (and thus the number of line pointers needed)
    uint32_t totalLines = 0;
    
    for (uint32_t i = gNumLines; i > 0;) {
        line_t& line = gpLines[--i];
        line.frontsector->linecount++;                                  // Inc the front sector's line count
        if (line.backsector && line.backsector != line.frontsector) {   // Two sided line?
            line.backsector->linecount++;                               // Add the back side referance
            ++totalLines;                                               // Inc count
        }
        ++totalLines;                                                   // Inc for the front
    }

    // Build line tables for each sector
    line_t** const ppLineBuffer = (line_t**) MemAlloc(totalLines * sizeof(line_t*));
    gppLineArrayBuffer = ppLineBuffer;      // Save in global for later disposal
    line_t** ppCurLines = ppLineBuffer;
    
    for (uint32_t i = gNumSectors; i > 0;) {
        // Invalidate the rect initally
        Fixed bbox[4];
        bbox[BOXTOP] = bbox[BOXRIGHT] = FRACMIN;        
        bbox[BOXBOTTOM] = bbox[BOXLEFT] = FRACMAX;

        // Get the current list entry
        sector_t& sector = gpSectors[--i];
        sector.lines = ppCurLines;
        
        for (uint32_t j = gNumLines; j > 0;) {
            line_t& line = gpLines[--j];
            if (line.frontsector == &sector || line.backsector == &sector) {
                ppCurLines[0] = &line;                  // Add the pointer to the entry list
                ++ppCurLines;                           // Add to the count
                AddToBox(bbox, line.v1.x, line.v1.y);   // Adjust the bounding box
                AddToBox(bbox, line.v2.x, line.v2.y);   // Both points
            }
        }

        // Set the sound origin to the center of the bounding box
        sector.SoundX = (bbox[BOXRIGHT] + bbox[BOXLEFT]) / 2;   // Get average
        sector.SoundY = (bbox[BOXTOP] + bbox[BOXBOTTOM]) / 2;   // This is SIGNED!

        // Adjust bounding box to map blocks and clip to unsigned values
        Fixed block = (bbox[BOXTOP] - gBlockMapOriginY + MAXRADIUS) >> MAPBLOCKSHIFT;
        ++block;
        block = (block > (int) gBlockMapHeight) ? gBlockMapHeight : block;
        sector.blockbox[BOXTOP] = block;        // Save the topmost point

        block = (bbox[BOXBOTTOM] - gBlockMapOriginY - MAXRADIUS) >> MAPBLOCKSHIFT;
        block = (block < 0) ? 0 : block;
        sector.blockbox[BOXBOTTOM] = block;     // Save the bottommost point

        block = (bbox[BOXRIGHT] - gBlockMapOriginX + MAXRADIUS) >> MAPBLOCKSHIFT;
        ++block;
        block = (block > (int) gBlockMapWidth) ? gBlockMapWidth : block;
        sector.blockbox[BOXRIGHT] = block;      // Save the rightmost point

        block = (bbox[BOXLEFT] - gBlockMapOriginX - MAXRADIUS) >> MAPBLOCKSHIFT;
        block = (block < 0) ? 0 : block;
        sector.blockbox[BOXLEFT] = block;       // Save the leftmost point
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Spawn items and critters
//----------------------------------------------------------------------------------------------------------------------
static void LoadThings(const uint32_t lumpResourceNum) {
    // Load the things resource
    const Resource* const pResource = Resources::load(lumpResourceNum);
    const std::byte* const pResourceData = pResource->pData;

    // Get the number of things first (first u32)
    const uint32_t numThings = Endian::bigToHost(((const uint32_t*) pResourceData)[0]);

    // Get the range of things and spawn each one
    const mapthing_t* pSrcThing = (const mapthing_t*)(pResourceData + sizeof(uint32_t));
    const mapthing_t* const pEndSrcThing = pSrcThing + numThings;

    while (pSrcThing < pEndSrcThing) {
        // N.B: we must correct endianess before spawning due to big endian source data!
        mapthing_t thing;
        thing.x = Endian::bigToHost(pSrcThing->x);
        thing.y = Endian::bigToHost(pSrcThing->y);
        thing.angle = Endian::bigToHost(pSrcThing->angle);
        thing.type = Endian::bigToHost(pSrcThing->type);
        thing.ThingFlags = Endian::bigToHost(pSrcThing->ThingFlags);

        // Spawn and move on
        SpawnMapThing(thing);
        ++pSrcThing;
    }

    // Done with this list
    Resources::free(lumpResourceNum);
}

//----------------------------------------------------------------------------------------------------------------------
// Draw the word "Loading" on the screen
//----------------------------------------------------------------------------------------------------------------------
static void LoadingPlaque() noexcept {
    UIUtils::drawPlaque(rLOADING);
}

//----------------------------------------------------------------------------------------------------------------------
// Preload all the wall and flat shapes
//----------------------------------------------------------------------------------------------------------------------
static void PreloadWalls() noexcept {
    const uint32_t numWallTex = Textures::getNumWallTextures();
    const uint32_t numFlatTex = Textures::getNumFlatTextures();
    const uint32_t numLoadTexFlags = (numWallTex > numFlatTex) ? numWallTex : numFlatTex;

    // This array holds which textures (and flats, later) to load
    bool* const bLoadTexFlags = reinterpret_cast<bool*>(MemAlloc(numLoadTexFlags * sizeof(bool)));
    memset(bLoadTexFlags, 0, numLoadTexFlags * sizeof(bool));

    // Scan all textures used by sidedefs and mark them for loading
    {
        const side_t* pSide = gpSides;
        const side_t* const pEndSide = pSide + gNumSides;

        while (pSide < pEndSide) {
            if (pSide->toptexture < numWallTex) {
                bLoadTexFlags[pSide->toptexture] = true;
            }

            if (pSide->midtexture < numWallTex) {
                bLoadTexFlags[pSide->midtexture] = true;
            }

            if (pSide->bottomtexture < numWallTex) {
                bLoadTexFlags[pSide->bottomtexture] = true;
            }

            ++pSide;
        }
    }

    // Now scan the walls for switches; mark for loading the alternate switch state texture:
    for (uint32_t switchNum = 0; switchNum < gNumSwitches; ++switchNum) {
        if (bLoadTexFlags[gSwitchList[switchNum]]) {            // Found a switch?
            bLoadTexFlags[gSwitchList[switchNum ^ 1]] = true;   // Load the alternate texture for the switch
        }
    }

    // Now load in the wall textures that were marked for loading
    for (uint32_t texNum = 0; texNum < numWallTex; ++texNum) {
        if (bLoadTexFlags[texNum]) {
            Textures::loadWall(texNum);
        }
    }

    // Load in the sky texture (that is a wall too).
    // Expect the sky texture number to be determined at this point!
    ASSERT(gSkyTextureNum > 0);
    Textures::loadWall(gSkyTextureNum);

    // Reset the portion of the flags we will use for flats.
    // Then scan all flats for what textures we need to load:
    memset(bLoadTexFlags, 0, numFlatTex * sizeof(bool));

    // Now scan for the flat textures used in all sectors and mark them for loading
    {
        const sector_t* pSector = gpSectors;
        const sector_t* const pEndSector = gpSectors + gNumSectors;

        while (pSector < pEndSector) {
            bLoadTexFlags[pSector->FloorPic] = true;

            if (pSector->CeilingPic < numFlatTex) {
                bLoadTexFlags[pSector->CeilingPic] = true;
            }

            ++pSector;
        }
    }

    // Mark for loading the other frames for any animated flats that we will load
    {
        const anim_t* pAnim = gFlatAnims;
        const anim_t* const pEndAnim = gFlatAnims + gNumFlatAnims;

        while (pAnim < pEndAnim) {
            const uint32_t lastAnimTexNum = pAnim->LastPicNum;

            if (bLoadTexFlags[lastAnimTexNum]) {
                // Animated flat is loading: load all other frames
                for (uint32_t texNum = pAnim->BasePic; texNum < lastAnimTexNum; ++texNum) {
                    bLoadTexFlags[texNum] = true;
                }
            }

            ++pAnim;
        }
    }

    // Now load all of the flat textures we marked for loading
    for (uint32_t texNum = 0; texNum < numFlatTex; ++texNum) {
        if (bLoadTexFlags[texNum]) {
            Textures::loadFlat(texNum);
        }
    }

    // Preloading other resources that were marked for preloading in the fixed preload table
    {
        uint32_t tableIdx = 0;

        while (PRELOAD_TABLE[tableIdx] != -1) {
            Resources::loadData(PRELOAD_TABLE[tableIdx]);
            Resources::release(PRELOAD_TABLE[tableIdx]);
            ++tableIdx;
        }
    }

    // Cleanup
    MemFree(bLoadTexFlags);
}

//----------------------------------------------------------------------------------------------------------------------
// Set the sky texture number for the map
//----------------------------------------------------------------------------------------------------------------------
static void setSkyTextureNum() noexcept {
    if (gGameMap < 9 || gGameMap == 24) {
        gSkyTextureNum = (uint32_t) rSKY1 - Textures::getFirstWallTexResourceNum();
    } else if (gGameMap < 18) {
        gSkyTextureNum = (uint32_t) rSKY2 - Textures::getFirstWallTexResourceNum();
    } else {
        gSkyTextureNum = (uint32_t) rSKY3 - Textures::getFirstWallTexResourceNum();
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Load and prepare the game level
//----------------------------------------------------------------------------------------------------------------------
void SetupLevel(uint32_t map) noexcept {
    Random::init();         // Reset the random number generator
    LoadingPlaque();        // Display "Loading"

    gTotalKillsInLevel = gItemsFoundInLevel = gSecretsFoundInLevel = 0;

    player_t* p = &gPlayer;
    p->killcount = 0;           // Nothing killed
    p->secretcount = 0;         // No secrets found
    p->itemcount = 0;           // No items found

    InitThinkers();         // Zap the think logics
    mapDataInit(map);       // Loads all map geometry, bsp, reject matrix etc. (everything except things)
    GroupLines();           // Final last minute data arranging

    gpDeathmatch = gDeathmatchStarts;

    LoadThings(getMapStartLump(map) + ML_THINGS);   // Spawn all the items
    SpawnSpecials();                                // Spawn all sector specials
    setSkyTextureNum();                             // Figure out which sky to use
    PreloadWalls();                                 // Load all the wall textures and sprites (also does sky texture)
    gbGamePaused = false;                           // Game in progress
}

//----------------------------------------------------------------------------------------------------------------------
// Dispose of all memory allocated by loading a level
//----------------------------------------------------------------------------------------------------------------------
void ReleaseMapMemory() noexcept {
    mapDataShutdown();
    MEM_FREE_AND_NULL(gppLineArrayBuffer);
    Textures::freeAll();
    Sprites::freeAll();
    InitThinkers();         // Dispose of all remaining memory
}

//----------------------------------------------------------------------------------------------------------------------
// Init the machine independant code
//----------------------------------------------------------------------------------------------------------------------
void P_Init() noexcept {
    P_InitSwitchList();     // Init the switch picture lookup list
    P_InitPicAnims();       // Init the picture animation scripts
}
