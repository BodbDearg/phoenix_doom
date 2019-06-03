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

seg_t *segs;                /* Pointer to array of loaded segs */
subsector_t *subsectors;    /* Pointer to array of loaded subsectors */
node_t *FirstBSPNode;   /* First BSP entry */
Word numlines;      /* Number of lines loaded */
line_t *lines;      /* Pointer to array of loaded lines */
line_t ***BlockMapLines;    /* Pointer to line lists based on blockmap */
Word BlockMapWidth,BlockMapHeight;  /* Size of blockmap in blocks */
Fixed BlockMapOrgX,BlockMapOrgY;    /* Origin of block map */
mobj_t **BlockLinkPtr;      /* Starting link for thing chains */
Byte *RejectMatrix;         /* For fast sight rejection */
mapthing_t deathmatchstarts[10],*deathmatch_p;  /* Deathmatch starts */
mapthing_t playerstarts;    /* Starting position for players */

/**********************************

    Load in all the line definitions.
    Requires vertexes,sectors and sides to be loaded.
    I also calculate the line's slope for quick processing by line
    slope comparisons.
    I also calculate the line's bounding box in Fixed pixels.

**********************************/

typedef struct {
    Word v1,v2;         /* Indexes to the vertex table */
    Word flags;         /* Line flags */
    Word special;       /* Special event type */
    Word tag;           /* ID tag for external trigger */
    Word sidenum[2];    /* sidenum[1] will be -1 if one sided */
} maplinedef_t;

static void LoadLineDefs(Word lump)
{
    Word i;
    maplinedef_t *mld;
    line_t *ld;

    mld = (maplinedef_t *)loadResourceData(lump);  /* Load in the data */
    numlines = ((Word*)mld)[0];         /* Get the number of lines in the struct array */   
    i = numlines*sizeof(line_t);        /* Get the size of the dest buffer */
    ld = (line_t *)MemAlloc(i);         /* Get the memory for the lines array */
    lines = ld;                         /* Save the lines */
    memset(ld,0,i);                     /* Blank out the buffer */
    mld = (maplinedef_t *)&((Word *)mld)[1];    /* Index to the first record of the struct array */
    i = numlines;           
    do {
        Fixed dx,dy;
        ld->flags = mld->flags;             /* Copy the flags */
        ld->special = mld->special;         /* Copy the special type */
        ld->tag = mld->tag;                 /* Copy the external tag ID trigger */
        ld->v1 = gpVertexes[mld->v1];       /* Copy the end points to the line */
        ld->v2 = gpVertexes[mld->v2];
        dx = ld->v2.x - ld->v1.x;           /* Get the delta offset (Line vector) */
        dy = ld->v2.y - ld->v1.y;
        
        /* What type of line is this? */
        
        if (!dx) {              /* No x motion? */
            ld->slopetype = ST_VERTICAL;        /* Vertical line only */
        } else if (!dy) {       /* No y motion? */
            ld->slopetype = ST_HORIZONTAL;      /* Horizontal line only */
        } else {
            if ((dy^dx) >= 0) { /* Is this a positive or negative slope */
                ld->slopetype = ST_POSITIVE;    /* Like signs, positive slope */
            } else {
                ld->slopetype = ST_NEGATIVE;    /* Unlike signs, negative slope */
            }
        }
        
        /* Create the bounding box */
        
        if (dx>=0) {            /* V2>=V1? */
            ld->bbox[BOXLEFT] = ld->v1.x;       /* Leftmost x */
            ld->bbox[BOXRIGHT] = ld->v2.x;      /* Rightmost x */
        } else {
            ld->bbox[BOXLEFT] = ld->v2.x;
            ld->bbox[BOXRIGHT] = ld->v1.x;
        }
        if (dy>=0) {
            ld->bbox[BOXBOTTOM] = ld->v1.y;     /* Bottommost y */
            ld->bbox[BOXTOP] = ld->v2.y;        /* Topmost y */
        } else {
            ld->bbox[BOXBOTTOM] = ld->v2.y;
            ld->bbox[BOXTOP] = ld->v1.y;
        }
        
        /* Copy the side numbers and sector pointers */
        ld->SidePtr[0] = &gpSides[mld->sidenum[0]];         /* Get the side number */
        ld->frontsector = ld->SidePtr[0]->sector;           /* All lines have a front side */
        if (mld->sidenum[1] != -1) {                        /* But maybe not a back side */
            ld->SidePtr[1] = &gpSides[mld->sidenum[1]];
            ld->backsector = ld->SidePtr[1]->sector;        /* Get the sector pointed to */
        }
        ++ld;           /* Next indexes */
        ++mld;
    } while (--i);
    freeResource(lump);     /* Release the resource */
}

/**********************************

    Load in the block map
    I need to have the lines preloaded before I can process the blockmap
    The data has 4 longwords at the beginning which will have an array of offsets
    to a series of line #'s. These numbers will be turned into pointers into the line
    array for quick compares to lines.
    
**********************************/

static void LoadBlockMap(Word lump)
{
    Word Count;
    Word Entries;
    Byte *MyLumpPtr;
    void **BlockHandle;
    LongWord *StartIndex;

    const Resource* pBlockResource = loadResource(lump);
    BlockHandle = pBlockResource->pData;    /* Load the data */
    
    MyLumpPtr = (Byte*) BlockHandle;
    BlockMapOrgX = ((Word *)MyLumpPtr)[0];      /* Get the orgx and y */
    BlockMapOrgY = ((Word *)MyLumpPtr)[1];
    BlockMapWidth = ((Word *)MyLumpPtr)[2];     /* Get the map size */
    BlockMapHeight = ((Word *)MyLumpPtr)[3];

    Entries = BlockMapWidth*BlockMapHeight; /* How many entries are there? */
    
/* Convert the loaded block map table into a huge array of block entries */

    Count = Entries;            /* Init the longword count */
    StartIndex = &((LongWord *)MyLumpPtr)[4];   /* Index to the offsets! */
    BlockMapLines = (line_t ***)StartIndex;     /* Save in global */
    do {
        StartIndex[0] = (LongWord)&MyLumpPtr[StartIndex[0]];    /* Convert to pointer */
        ++StartIndex;       /* Next offset entry */
    } while (--Count);      /* All done? */ 

    /* Convert the lists appended to the array into pointers to lines */
    Count = pBlockResource->size / 4;      /* How much memory is needed? (Longs) */
    
    Count -= (Entries+4);       /* Remove the header count */
    do {
        if (StartIndex[0]!=-1) {    /* End of a list? */
            StartIndex[0] = (LongWord)&lines[StartIndex[0]];    /* Get the line pointer */
        } else {
            StartIndex[0] = 0;  /* Insert a null pointer */
        }
        ++StartIndex;   /* Next entry */
    } while (--Count);
    
    /* Clear out mobj chains */
    Count = sizeof(*BlockLinkPtr)*Entries;      /* Get memory */
    BlockLinkPtr = (mobj_t**) MemAlloc(Count);  /* Allocate memory */
    memset(BlockLinkPtr,0,Count);               /* Clear it out */
}

/**********************************

    Load the line segments structs for rendering
    Requires vertexes,sides and lines to be preloaded

**********************************/

typedef struct {
    Word v1,v2;         /* Index to the vertexs */
    angle_t angle;      /* Angle of the line segment */
    Fixed offset;       /* Texture offset */
    Word linedef;       /* Line definition */
    Word side;          /* Side of the line segment */
} mapseg_t;

static void LoadSegs(Word lump)
{
    Word i;
    mapseg_t *ml;
    seg_t *li;
    Word numsegs;

    ml = (mapseg_t *)loadResourceData(lump);    /* Load in the map data */
    numsegs = ((Word*)ml)[0];                   /* Get the count */
    i = numsegs*sizeof(seg_t);                  /* Get the memory size */
    li = (seg_t*)MemAlloc(i);                   /* Allocate it */
    segs = li;                                  /* Save pointer to global */
    memset(li,0,i);                             /* Clear it out */

    ml = (mapseg_t *)&((Word *)ml)[1];  /* Init pointer to first record */
    i = 0;
    do {
        line_t *ldef;
        Word side;
        
        li->v1 = gpVertexes[ml->v1];            /* Get the line points */
        li->v2 = gpVertexes[ml->v2];
        li->angle = ml->angle;                  /* Set the angle of the line */
        li->offset = ml->offset;                /* Get the texture offset */
        ldef = &lines[ml->linedef];             /* Get the line pointer */
        li->linedef = ldef;
        side = ml->side;                        /* Get the side number */
        li->sidedef = ldef->SidePtr[side];      /* Grab the side pointer */
        li->frontsector = li->sidedef->sector;  /* Get the front sector */
        
        if (ldef->flags & ML_TWOSIDED) {                        /* Two sided? */
            li->backsector = ldef->SidePtr[side^1]->sector;     /* Mark the back sector */
        }
        
        if (ldef->v1.x == li->v1.x && ldef->v1.y == li->v1.y) {     /* Init the fineangle */
            ldef->fineangle = li->angle>>ANGLETOFINESHIFT;          /* This is a point only */
        }
        
        ++li;   /* Next entry */
        ++ml;   /* Next resource entry */
    } while (++i<numsegs);
    
    freeResource(lump);    /* Release the resource */
}

/**********************************

    Load in all the subsectors
    Requires segs, sectors, sides

**********************************/

typedef struct {        /* Loaded map subsectors */
    Word numlines;      /* Number of line segments */
    Word firstline;     /* Segs are stored sequentially */
} mapsubsector_t;

static void LoadSubsectors(Word lump)
{
    Word numsubsectors;
    Word i;
    mapsubsector_t *ms;
    subsector_t *ss;

    ms = (mapsubsector_t *)loadResourceData(lump);  /* Get the map data */
    numsubsectors = ((Word*)ms)[0];                 /* Get the subsector count */
    i = numsubsectors*sizeof(subsector_t);          /* Calc needed buffer */
    ss = (subsector_t *)MemAlloc(i);                /* Get the memory */
    subsectors=ss;                                  /* Save in global */
    ms = (mapsubsector_t *)&((Word *)ms)[1];        /* Index to first entry */
    i = numsubsectors;
    
    do {
        seg_t *seg;
        ss->numsublines = ms->numlines;     /* Number of lines in the sub sectors */
        seg = &segs[ms->firstline];         /* Get the first line segment pointer */
        ss->firstline = seg;                /* Save it */
        ss->sector = seg->sidedef->sector;  /* Get the parent sector */
        ++ss;                               /* Index to the next entry */
        ++ms;
    } while (--i);
    
    freeResource(lump);
}

/**********************************

    Load in the BSP tree and convert the indexs into pointers
    to either the node list or the subsector array.
    I require that the subsectors are loaded in.

**********************************/

#define NF_SUBSECTOR 0x8000

typedef struct {
    Fixed x,y,dx,dy;    /* Partition vector */
    Fixed bbox[2][4];   /* Bounding box for each child */
    LongWord children[2];   /* if NF_SUBSECTOR it's a subsector index else node index */
} mapnode_t;

static void LoadNodes(Word lump)
{
    Word numnodes;      /* Number of BSP nodes */
    Word i,j,k;
    mapnode_t *mn;
    node_t *no; 
    node_t *nodes;

    mn = (mapnode_t *)loadResourceData(lump);   /* Get the data */
    numnodes = ((Word*)mn)[0];                  /* How many nodes to process */
    mn = (mapnode_t *)&((Word *)mn)[1];
    no = (node_t *)mn;
    nodes = no;
    i = numnodes;           /* Get the node count */
    do {
        j = 0;
        do {
            k = (Word)mn->children[j];  /* Get the child offset */
            if (k&NF_SUBSECTOR) {       /* Subsector? */
                k&=(~NF_SUBSECTOR);     /* Clear the flag */
                no->Children[j] = (void *)((LongWord)&subsectors[k]|1);
            } else {    
                no->Children[j] = &nodes[k];    /* It's a node offset */
            }
        } while (++j<2);        /* Both branches done? */
        ++no;       /* Next index */
        ++mn;
    } while (--i);
    FirstBSPNode = no-1;    /* The last node is the first entry into the tree */
}

/**********************************

    Builds sector line lists and subsector sector numbers
    Finds block bounding boxes for sectors

**********************************/

static void GroupLines(void)
{
    line_t **linebuffer;    /* Pointer to linebuffer array */
    Word total;     /* Number of entries needed for linebuffer array */
    line_t *li;     /* Pointer to a work line record */
    Word i,j;
    sector_t *sector;   /* Work sector pointer */
    Fixed block;    /* Clipped bounding box value */
    Fixed bbox[4];

/* count number of lines in each sector */

    li = lines;     /* Init pointer to line array */
    total = 0;      /* How many line pointers are needed for sector line array */
    i = numlines;   /* How many lines to process */
    do {
        li->frontsector->linecount++;   /* Inc the front sector's line count */
        if (li->backsector && li->backsector != li->frontsector) {  /* Two sided line? */
            li->backsector->linecount++;    /* Add the back side referance */
            ++total;    /* Inc count */
        }
        ++total;    /* Inc for the front */
        ++li;       /* Next line down */
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
        li = lines;                                 /* Init the line array pointer */
        j = numlines;
        
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

        sector->SoundX = (bbox[BOXRIGHT]+bbox[BOXLEFT])/2;  /* Get average */
        sector->SoundY = (bbox[BOXTOP]+bbox[BOXBOTTOM])/2;  /* This is SIGNED! */

        /* Adjust bounding box to map blocks and clip to unsigned values */

        block = (bbox[BOXTOP]-BlockMapOrgY+MAXRADIUS)>>MAPBLOCKSHIFT;
        ++block;
        block = (block > (int)BlockMapHeight) ? BlockMapHeight : block;
        sector->blockbox[BOXTOP]=block;     /* Save the topmost point */

        block = (bbox[BOXBOTTOM]-BlockMapOrgY-MAXRADIUS)>>MAPBLOCKSHIFT;
        block = (block < 0) ? 0 : block;
        sector->blockbox[BOXBOTTOM]=block;  /* Save the bottommost point */

        block = (bbox[BOXRIGHT]-BlockMapOrgX+MAXRADIUS)>>MAPBLOCKSHIFT;
        ++block;
        block = (block > (int)BlockMapWidth) ? BlockMapWidth : block;
        sector->blockbox[BOXRIGHT]=block;   /* Save the rightmost point */

        block = (bbox[BOXLEFT]-BlockMapOrgX-MAXRADIUS)>>MAPBLOCKSHIFT;
        block = (block < 0) ? 0 : block;
        sector->blockbox[BOXLEFT]=block;    /* Save the leftmost point */
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

/**********************************

    Load and prepare the game level

**********************************/

void SetupLevel(Word map)
{
    Word lumpnum;
    player_t *p;

    Randomize();            /* Reset the random number generator */
    LoadingPlaque();        /* Display "Loading" */

    // DC: TODO: Remove
    #if 0
        PurgeHandles(0);    /* Purge memory */
        CompactHandles();   /* Pack remaining memory */
    #endif
    
    TotalKillsInLevel = ItemsFoundInLevel = SecretsFoundInLevel = 0;
    p = &players;
    p->killcount = 0;       /* Nothing killed */
    p->secretcount = 0;     /* No secrets found */
    p->itemcount = 0;       /* No items found */
    
    InitThinkers();         /* Zap the think logics */

    lumpnum = ((map-1)*ML_TOTAL)+rMAP01;    /* Get the map number */
    LoadedLevel = lumpnum;      /* Save the loaded resource number */
    
    mapDataInit(map);

    /* Note: most of this ordering is important */
    LoadLineDefs(lumpnum+ML_LINEDEFS);                              /* Needs vertexes,sectors and sides */
    LoadBlockMap(lumpnum+ML_BLOCKMAP);                              /* Needs lines */
    LoadSegs(lumpnum+ML_SEGS);                                      /* Needs vertexes,lines,sides */
    LoadSubsectors(lumpnum+ML_SSECTORS);                            /* Needs sectors and segs and sides */
    LoadNodes(lumpnum+ML_NODES);                                    /* Needs subsectors */
    RejectMatrix = (Byte *)loadResourceData(lumpnum+ML_REJECT);     /* Get the reject matrix */
    
    GroupLines();                       /* Final last minute data arranging */
    deathmatch_p = deathmatchstarts;
    LoadThings(lumpnum+ML_THINGS);      /* Spawn all the items */
    SpawnSpecials();                    /* Spawn all sector specials */
    PreloadWalls();                     /* Load all the wall textures and sprites */

/* if deathmatch, randomly spawn the active players */

    gamepaused = false;     /* Game in progress */
}

//---------------------------------------------------------------------------------------------------------------------
// Dispose of all memory allocated by loading a level
//---------------------------------------------------------------------------------------------------------------------
void ReleaseMapMemory() {
    mapDataShutdown();
    
    MEM_FREE_AND_NULL(lines);                   // Dispose of the lines
    freeResource(LoadedLevel + ML_BLOCKMAP);    // Make sure it's discarded since I modified it
    MEM_FREE_AND_NULL(BlockLinkPtr);            // Discard the block map mobj linked list
    MEM_FREE_AND_NULL(segs);                    // Release the line segment memory
    MEM_FREE_AND_NULL(subsectors);              // Release the sub sectors
    freeResource(LoadedLevel + ML_NODES);       // Release the BSP tree
    freeResource(LoadedLevel + ML_REJECT);      // Release the quick reject matrix
    MEM_FREE_AND_NULL(LineArrayBuffer);
    
    lines = 0;
    BlockMapLines = 0;      // Force zero for resource
    BlockLinkPtr = 0;
    segs = 0;
    subsectors = 0;
    FirstBSPNode = 0;
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
