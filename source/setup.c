#include "doom.h"
#include "MapData.h"
#include "Mem.h"
#include "Resources.h"
#include "Textures.h"
#include <intmath.h>
#include <string.h>

/* lump order in a map wad */
enum {
    ML_THINGS,ML_LINEDEFS,ML_SIDEDEFS,ML_VERTEXES,ML_SEGS,
    ML_SSECTORS,ML_SECTORS,ML_NODES,ML_REJECT,ML_BLOCKMAP,
    ML_TOTAL
};

static Word LoadedLevel;            /* Resource number of the loaded level */
static line_t **LineArrayBuffer;    /* Pointer to array of line_t pointers used by sectors */

static Word PreLoadTable[] = {
    rSPR_ZOMBIE,            /* Zombiemen */
    rSPR_SHOTGUY,           /* Shotgun guys */
    rSPR_IMP,               /* Imps */
    rSPR_DEMON,             /* Demons */
    rSPR_CACODEMON,         /* Cacodemons */
    rSPR_LOSTSOUL,          /* Lost souls */
    rSPR_BARON,             /* Baron of Hell */
    rSPR_OURHEROBDY,        /* Our dead hero */
    rSPR_BARREL,            /* Exploding barrel */
    rSPR_SHOTGUN,           /* Shotgun on floor */
    rSPR_CLIP,              /* Clip of bullets */
    rSPR_SHELLS,            /* 4 shotgun shells */
    rSPR_STIMPACK,          /* Stimpack */
    rSPR_MEDIKIT,           /* Med-kit */
    rSPR_GREENARMOR,        /* Normal armor */
    rSPR_BLUEARMOR,         /* Mega armor */
    rSPR_HEALTHBONUS,       /* Health bonus */
    rSPR_ARMORBONUS,        /* Armor bonus */
    rSPR_BLUD,              /* Blood from bullet hit */
    rSPR_PUFF,              /* Gun sparks on wall */
    -1
};

Byte *RejectMatrix;         /* For fast sight rejection */
mapthing_t deathmatchstarts[10],*deathmatch_p;  /* Deathmatch starts */
mapthing_t playerstarts;    /* Starting position for players */

/**********************************

    Builds sector line lists and subsector sector numbers
    Finds block bounding boxes for sectors

**********************************/
static void GroupLines(void)
{
    line_t **linebuffer;    /* Pointer to linebuffer array */
    Word total;             /* Number of entries needed for linebuffer array */
    line_t *li;             /* Pointer to a work line record */
    Word i,j;
    sector_t *sector;       /* Work sector pointer */
    Fixed block;            /* Clipped bounding box value */
    Fixed bbox[4];

    /* count number of lines in each sector */
    li = gpLines;       /* Init pointer to line array */
    total = 0;          /* How many line pointers are needed for sector line array */
    i = gNumLines;      /* How many lines to process */
    do {
        li->frontsector->linecount++;                               /* Inc the front sector's line count */
        if (li->backsector && li->backsector != li->frontsector) {  /* Two sided line? */
            li->backsector->linecount++;                            /* Add the back side referance */
            ++total;                                                /* Inc count */
        }
        ++total;                                                    /* Inc for the front */
        ++li;                                                       /* Next line down */
    } while (--i);

    /* Build line tables for each sector */
    linebuffer = (line_t**)MemAlloc(total * sizeof(line_t*));
    
    LineArrayBuffer = linebuffer;   /* Save in global for later disposal */
    sector = gpSectors;             /* Init the sector pointer */
    i = gNumSectors;                /* Get the sector count */
    
    do {
        bbox[BOXTOP] = bbox[BOXRIGHT] = MININT;     /* Invalidate the rect */
        bbox[BOXBOTTOM] = bbox[BOXLEFT] = MAXINT;
        sector->lines = linebuffer;                 /* Get the current list entry */
        li = gpLines;                               /* Init the line array pointer */
        j = gNumLines;
        
        do {
            if (li->frontsector == sector || li->backsector == sector) {
                linebuffer[0] = li;                 /* Add the pointer to the entry list */
                ++linebuffer;                       /* Add to the count */
                AddToBox(bbox,li->v1.x,li->v1.y);   /* Adjust the bounding box */
                AddToBox(bbox,li->v2.x,li->v2.y);   /* Both points */
            }
            ++li;
        } while (--j);      /* All done? */

        /* Set the sound origin to the center of the bounding box */
        sector->SoundX = (bbox[BOXRIGHT]+bbox[BOXLEFT])/2;      /* Get average */
        sector->SoundY = (bbox[BOXTOP]+bbox[BOXBOTTOM])/2;      /* This is SIGNED! */

        /* Adjust bounding box to map blocks and clip to unsigned values */
        block = (bbox[BOXTOP] - gBlockMapOriginY + MAXRADIUS) >> MAPBLOCKSHIFT;
        ++block;
        block = (block > (int) gBlockMapHeight) ? gBlockMapHeight : block;
        sector->blockbox[BOXTOP] = block;       // Save the topmost point

        block = (bbox[BOXBOTTOM] - gBlockMapOriginY - MAXRADIUS) >> MAPBLOCKSHIFT;
        block = (block < 0) ? 0 : block;
        sector->blockbox[BOXBOTTOM] = block;    // Save the bottommost point

        block = (bbox[BOXRIGHT] - gBlockMapOriginX + MAXRADIUS) >> MAPBLOCKSHIFT;
        ++block;
        block = (block > (int) gBlockMapWidth) ? gBlockMapWidth : block;
        sector->blockbox[BOXRIGHT] = block;     // Save the rightmost point

        block = (bbox[BOXLEFT] - gBlockMapOriginX - MAXRADIUS) >> MAPBLOCKSHIFT;
        block = (block < 0) ? 0 : block;
        sector->blockbox[BOXLEFT] = block;      // Save the leftmost point
        ++sector;
    } while (--i);
}

/**********************************

    Spawn items and critters

**********************************/
static void LoadThings(Word lump)
{
    Word i;
    mapthing_t *mt;
    
    mt = (mapthing_t *)loadResourceData(lump); /* Load the thing list */
    i = ((Word*)mt)[0];         /* Get the count */
    mt = (mapthing_t *)&((Word *)mt)[1];    /* Point to the first entry */
    do {
        SpawnMapThing(mt);  /* Spawn the thing */
        ++mt;       /* Next item */
    } while (--i);  /* More? */
    freeResource(lump);    /* Release the list */
}

//---------------------------------------------------------------------------------------------------------------------
// Draw the word "Loading" on the screen
//---------------------------------------------------------------------------------------------------------------------
static void LoadingPlaque() {
    DrawPlaque(rLOADING);
}

//---------------------------------------------------------------------------------------------------------------------
// Preload all the wall and flat shapes
//---------------------------------------------------------------------------------------------------------------------
static void PreloadWalls() {
    const uint32_t numWallTex = getNumWallTextures();
    const uint32_t numFlatTex = getNumFlatTextures();
    const uint32_t numLoadTexFlags = (numWallTex > numFlatTex) ? numWallTex : numFlatTex;
    
    // This array holds which textures (and flats, later) to load
    bool* const bLoadTexFlags = MemAlloc(numLoadTexFlags * sizeof(bool));
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
    for (uint32_t switchNum = 0; switchNum < NumSwitches; ++switchNum) {
        if (bLoadTexFlags[SwitchList[switchNum]]) {         // Found a switch?
            bLoadTexFlags[SwitchList[switchNum^1]] = true;  // Load the alternate texture for the switch
        }
    }
    
    // Now load in the wall textures that were marked for loading
    for (uint32_t texNum = 0; texNum < numWallTex; ++texNum) {
        if (bLoadTexFlags[texNum]) {
            loadWallTexture(texNum);
        }
    }
    
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
        const anim_t* pAnim = FlatAnims;
        const anim_t* const pEndAnim = FlatAnims + NumFlatAnims;
        
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
            loadFlatTexture(texNum);
        }
    }
    
    // Preloading other resources that were marked for preloading in the fixed preload table
    {
        uint32_t tableIdx = 0;
        
        while (PreLoadTable[tableIdx] != -1) {
            loadResourceData(PreLoadTable[tableIdx]);
            releaseResource(PreLoadTable[tableIdx]);
            ++tableIdx;
        }
    }
    
    // Cleanup
    MemFree(bLoadTexFlags);
}

//---------------------------------------------------------------------------------------------------------------------
// Load and prepare the game level
//---------------------------------------------------------------------------------------------------------------------
void SetupLevel(Word map) {
    Randomize();            /* Reset the random number generator */
    LoadingPlaque();        /* Display "Loading" */

    // DC: TODO: Remove
    #if 0
        PurgeHandles(0);    /* Purge memory */
        CompactHandles();   /* Pack remaining memory */
    #endif
    
    TotalKillsInLevel = ItemsFoundInLevel = SecretsFoundInLevel = 0;
    
    player_t* p = &players;
    p->killcount = 0;       /* Nothing killed */
    p->secretcount = 0;     /* No secrets found */
    p->itemcount = 0;       /* No items found */
    
    InitThinkers();         /* Zap the think logics */
    
    Word lumpnum = ((map-1)*ML_TOTAL)+rMAP01;    /* Get the map number */
    LoadedLevel = lumpnum;      /* Save the loaded resource number */
    
    mapDataInit(map);

    /* Note: most of this ordering is important */
    RejectMatrix = (Byte *)loadResourceData(lumpnum+ML_REJECT);     /* Get the reject matrix */
    
    GroupLines();                       /* Final last minute data arranging */
    deathmatch_p = deathmatchstarts;
    LoadThings(lumpnum+ML_THINGS);      /* Spawn all the items */
    SpawnSpecials();                    /* Spawn all sector specials */
    PreloadWalls();                     /* Load all the wall textures and sprites */
    gamepaused = false;                 /* Game in progress */
}

//---------------------------------------------------------------------------------------------------------------------
// Dispose of all memory allocated by loading a level
//---------------------------------------------------------------------------------------------------------------------
void ReleaseMapMemory() {
    mapDataShutdown();
    
    freeResource(LoadedLevel + ML_REJECT);      // Release the quick reject matrix
    MEM_FREE_AND_NULL(LineArrayBuffer);
    RejectMatrix = 0;
    
    texturesReleaseAll();
    InitThinkers();         // Dispose of all remaining memory
}

//---------------------------------------------------------------------------------------------------------------------
// Init the machine independant code
//---------------------------------------------------------------------------------------------------------------------
void P_Init() {
    P_InitSwitchList();     // Init the switch picture lookup list
    P_InitPicAnims();       // Init the picture animation scripts
}
