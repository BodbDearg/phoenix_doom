#include "MapData.h"

#include "doomrez.h"
#include "Endian.h"
#include "Macros.h"
#include "Resources.h"
#include <vector>

// On-disk versions of various map data structures.
// These differ in many (all?) cases to the runtime versions.
struct MapSector {
    Fixed       floorHeight;
    Fixed       ceilingHeight;
    uint32_t    floorPic;           // Floor flat number
    uint32_t    ceilingPic;         // Ceiling flat number
    uint32_t    lightLevel;
    uint32_t    special;            // Special flags
    uint32_t    tag;                // Tag ID
};

struct MapSide {
    Fixed       textureOffset;
    Fixed       rowOffset;
    uint32_t    topTexture;
    uint32_t    bottomTexture;
    uint32_t    midTexture;
    uint32_t    sector;
};

struct MapLine {
    uint32_t    v1;             // Vertex table index
    uint32_t    v2;             // Vertex table index
    uint32_t    flags;          // Line flags
    uint32_t    special;        // Special event type
    uint32_t    tag;            // ID tag for external trigger
    uint32_t    sideNum[2];     // sideNum[1] will be -1 if one sided
};

struct MapLineSeg {
    uint32_t    v1;         // Vertex index: 1
    uint32_t    v2;         // Vertex index: 2
    angle_t     angle;      // Angle of the line segment
    Fixed       offset;     // Texture offset
    uint32_t    lineDef;    // Line definition
    uint32_t    side;       // Side of the line segment
};

struct MapSubSector {
    uint32_t    numLines;       // Number of line segments
    uint32_t    firstLine;      // Note: line segs are stored sequentially
};

#define NF_SUBSECTOR 0x8000

struct MapNode {
    Fixed x;                // Partitioning vector
    Fixed y;
    Fixed dx;
    Fixed dy;
    Fixed bbox[2][4];       // Bounding box for each child
    uint32_t children[2];   // if NF_SUBSECTOR it's a subsector index else node index
};

// Internal data arrays
static std::vector<vertex_t>        gVertexes;
static std::vector<sector_t>        gSectors;
static std::vector<side_t>          gSides;
static std::vector<line_t>          gLines;
static std::vector<seg_t>           gLineSegs;
static std::vector<subsector_t>     gSubSectors;
static std::vector<node_t>          gNodes;
static uint32_t                     gLoadedRejectMatrixResourceNum;
static std::vector<line_t*>         gBlockMapLines;
static std::vector<line_t**>        gBlockMapLineLists;
static std::vector<mobj_t*>         gBlockMapThingLists;

static void loadVertexes(const uint32_t lumpResourceNum) noexcept {
    const Resource* const pResource = loadResource(lumpResourceNum);
    
    const uint32_t numVerts = pResource->size / sizeof(vertex_t);
    gNumVertexes = numVerts;
    gVertexes.resize(numVerts);
    gpVertexes = gVertexes.data();
    
    const vertex_t* pSrcVert = (const vertex_t*) pResource->pData;
    const vertex_t* const pEndSrcVerts = (const vertex_t*) pResource->pData + numVerts;
    vertex_t* pDstVert = gVertexes.data();
    
    while (pSrcVert < pEndSrcVerts) {
        pDstVert->x = byteSwappedI32(pSrcVert->x);
        pDstVert->y = byteSwappedI32(pSrcVert->y);
        ++pSrcVert;
        ++pDstVert;
    }
    
    freeResource(lumpResourceNum);
}

static void loadSectors(const uint32_t lumpResourceNum) noexcept {
    // Load the sectors resource
    const Resource* const pResource = loadResource(lumpResourceNum);
    const std::byte* const pResourceData = (const std::byte*) pResource->pData;
    
    // Get the number of sectors first (first u32)
    const uint32_t numSectors = byteSwappedU32(((const uint32_t*) pResourceData)[0]);
    gNumSectors = numSectors;
    
    // Get the source sector data and alloc room for the runtime sectors
    const MapSector* pSrcSector = (const MapSector*)(pResourceData + sizeof(uint32_t));
    const MapSector* const pEndSrcSector = pSrcSector + numSectors;
    
    gSectors.clear();
    gSectors.resize(numSectors);
    gpSectors = gSectors.data();
    
    // Create all sectors
    sector_t* pDstSector = gSectors.data();
    
    while (pSrcSector < pEndSrcSector) {
        pDstSector->floorheight = byteSwappedI32(pSrcSector->floorHeight);
        pDstSector->ceilingheight = byteSwappedI32(pSrcSector->ceilingHeight);
        pDstSector->FloorPic = byteSwappedU32(pSrcSector->floorPic);
        pDstSector->CeilingPic = byteSwappedU32(pSrcSector->ceilingPic);
        pDstSector->lightlevel = byteSwappedU32(pSrcSector->lightLevel);
        pDstSector->special = byteSwappedU32(pSrcSector->special);
        pDstSector->tag  = byteSwappedU32(pSrcSector->tag);
        
        ++pSrcSector;
        ++pDstSector;
    }
    
    // Don't need this anymore
    freeResource(lumpResourceNum);
}

static void loadSides(const uint32_t lumpResourceNum) noexcept {
    // Load the side defs resource
    ASSERT_LOG(gSectors.size() > 0, "Sectors must be loaded first!");
    const Resource* const pResource = loadResource(lumpResourceNum);
    const std::byte* const pResourceData = (const std::byte*) pResource->pData;
    
    // Get the number of side defs first (first u32)
    const uint32_t numSides = byteSwappedU32(((const uint32_t*) pResourceData)[0]);
    gNumSides = numSides;
    
    // Get the source side def data and alloc room for the runtime equivalent
    const MapSide* pSrcSide = (const MapSide*)(pResourceData + sizeof(uint32_t));
    const MapSide* const pEndSrcSide = pSrcSide + numSides;
    
    gSides.clear();
    gSides.resize(numSides);
    gpSides = gSides.data();
    
    // Create all sides
    side_t* pDstSide = gSides.data();
    
    while (pSrcSide < pEndSrcSide) {
        pDstSide->textureoffset = byteSwappedI32(pSrcSide->textureOffset);
        pDstSide->rowoffset = byteSwappedI32(pSrcSide->rowOffset);
        pDstSide->toptexture = byteSwappedU32(pSrcSide->topTexture);
        pDstSide->bottomtexture = byteSwappedU32(pSrcSide->bottomTexture);
        pDstSide->midtexture = byteSwappedU32(pSrcSide->midTexture);
        
        const uint32_t sectorNum = byteSwappedU32(pSrcSide->sector);
        pDstSide->sector = &gSectors[sectorNum];
        
        ++pSrcSide;
        ++pDstSide;
    }
    
    // Don't need this anymore
    freeResource(lumpResourceNum);
}

static void loadLines(const uint32_t lumpResourceNum) noexcept {
    // Load the line defs resource
    ASSERT_LOG(gSides.size() > 0, "Sides must be loaded first!");
    const Resource* const pResource = loadResource(lumpResourceNum);
    const std::byte* const pResourceData = (const std::byte*) pResource->pData;
    
    // Get the number of line defs first (first u32)
    const uint32_t numLines = byteSwappedU32(((const uint32_t*) pResourceData)[0]);
    gNumLines = numLines;
    
    // Get the source line def data and alloc room for the runtime lines
    const MapLine* pSrcLine = (const MapLine*)(pResourceData + sizeof(uint32_t));
    const MapLine* const pEndSrcLine = pSrcLine + numLines;
    
    gLines.clear();
    gLines.resize(numLines);
    gpLines = gLines.data();
    
    // Create all lines
    line_t* pDstLine = gLines.data();
    
    while (pSrcLine < pEndSrcLine) {
        pDstLine->flags = byteSwappedU32(pSrcLine->flags);
        pDstLine->special = byteSwappedU32(pSrcLine->special);
        pDstLine->tag = byteSwappedU32(pSrcLine->tag);
        
        // Copy the end points to the line
        const uint32_t v1Idx = byteSwappedU32(pSrcLine->v1);
        const uint32_t v2Idx = byteSwappedU32(pSrcLine->v2);
        pDstLine->v1 = gVertexes[v1Idx];
        pDstLine->v2 = gVertexes[v2Idx];
        
        // Get the delta offset (Line vector)
        const Fixed dx = pDstLine->v2.x - pDstLine->v1.x;
        const Fixed dy = pDstLine->v2.y - pDstLine->v1.y;
        
        // What type of line is this?
        if (!dx) {                                      // No x motion?
            pDstLine->slopetype = ST_VERTICAL;          // Vertical line only
        } else if (!dy) {                               // No y motion?
            pDstLine->slopetype = ST_HORIZONTAL;        // Horizontal line only
        } else {
            if ((dy ^ dx) >= 0) {                       // Is this a positive or negative slope
                pDstLine->slopetype = ST_POSITIVE;      // Like signs, positive slope
            } else {
                pDstLine->slopetype = ST_NEGATIVE;      // Unlike signs, negative slope
            }
        }
        
        // Create the bounding box
        if (dx >= 0) {                                      // V2 >= V1?
            pDstLine->bbox[BOXLEFT] = pDstLine->v1.x;       // Leftmost x
            pDstLine->bbox[BOXRIGHT] = pDstLine->v2.x;      // Rightmost x
        } else {
            pDstLine->bbox[BOXLEFT] = pDstLine->v2.x;
            pDstLine->bbox[BOXRIGHT] = pDstLine->v1.x;
        }
        
        if (dy>=0) {
            pDstLine->bbox[BOXBOTTOM] = pDstLine->v1.y;     // Bottommost y
            pDstLine->bbox[BOXTOP] = pDstLine->v2.y;        // Topmost y
        } else {
            pDstLine->bbox[BOXBOTTOM] = pDstLine->v2.y;
            pDstLine->bbox[BOXTOP] = pDstLine->v1.y;
        }
        
        // Copy the side numbers and sector pointers.
        // Note: all lines have a front side, but not necessarily a back side!
        const uint32_t sideNum1 = byteSwappedU32(pSrcLine->sideNum[0]);
        const uint32_t sideNum2 = byteSwappedU32(pSrcLine->sideNum[1]);
        
        pDstLine->SidePtr[0] = &gSides[sideNum1];
        pDstLine->frontsector = pDstLine->SidePtr[0]->sector;
        
        if (sideNum2 != -1) {
            // Line has a back side also
            pDstLine->SidePtr[1] = &gSides[sideNum2];
            pDstLine->backsector = pDstLine->SidePtr[1]->sector;
        }
        
        ++pSrcLine;
        ++pDstLine;
    }
    
    // Don't need this anymore
    freeResource(lumpResourceNum);
}

static void loadLineSegs(const uint32_t lumpResourceNum) noexcept {
    // Load the line segs resource
    ASSERT_LOG(gVertexes.size() > 0, "Vertices must be loaded first!");
    ASSERT_LOG(gLines.size() > 0, "Lines must be loaded first!");
    const Resource* const pResource = loadResource(lumpResourceNum);
    const std::byte* const pResourceData = (const std::byte*) pResource->pData;
    
    // Get the number of line segments first (first u32)
    const uint32_t numLineSegs = byteSwappedU32(((const uint32_t*) pResourceData)[0]);
    gNumLineSegs = numLineSegs;
    
    // Get the source line seg data and alloc room for the runtime equivalent
    const MapLineSeg* pSrcLineSeg = (const MapLineSeg*)(pResourceData + sizeof(uint32_t));
    const MapLineSeg* const pEndSrcLineSeg = pSrcLineSeg + numLineSegs;
    
    gLineSegs.clear();
    gLineSegs.resize(numLineSegs);
    gpLineSegs = gLineSegs.data();
    
    seg_t* pDstLineSeg = gLineSegs.data();
    
    while (pSrcLineSeg < pEndSrcLineSeg) {
        pDstLineSeg->v1 = gVertexes[byteSwappedU32(pSrcLineSeg->v1)];
        pDstLineSeg->v2 = gVertexes[byteSwappedU32(pSrcLineSeg->v2)];
        pDstLineSeg->angle = byteSwappedU32(pSrcLineSeg->angle);
        pDstLineSeg->offset = byteSwappedI32(pSrcLineSeg->offset);
        
        line_t* const pLine = &gLines[byteSwappedU32(pSrcLineSeg->lineDef)];
        pDstLineSeg->linedef = pLine;
        
        const uint32_t side = byteSwappedU32(pSrcLineSeg->side);
        pDstLineSeg->sidedef = pLine->SidePtr[side];
        pDstLineSeg->frontsector = pDstLineSeg->sidedef->sector;
        
        if (pLine->flags & ML_TWOSIDED) {
            // Store a reference to the joining sector as well for segs on a two sided line
            pDstLineSeg->backsector = pLine->SidePtr[side ^ 1]->sector;
        }
        
        // Init the finea ngle on the line
        if (pLine->v1.x == pDstLineSeg->v1.x && pLine->v1.y == pDstLineSeg->v1.y) {
            pLine->fineangle = pDstLineSeg->angle >> ANGLETOFINESHIFT;  // This is a point only
        }
    
        ++pSrcLineSeg;
        ++pDstLineSeg;
    }
    
    // Don't need this anymore
    freeResource(lumpResourceNum);
}

static void loadSubSectors(const uint32_t lumpResourceNum) noexcept {
    // Load the sub sectors resource
    ASSERT_LOG(gLineSegs.size() > 0, "Line segments must be loaded first!");
    const Resource* const pResource = loadResource(lumpResourceNum);
    const std::byte* const pResourceData = (const std::byte*) pResource->pData;
    
    // Get the number of sub sectors first (first u32)
    const uint32_t numSubSectors = byteSwappedU32(((const uint32_t*) pResourceData)[0]);
    gNumSubSectors = numSubSectors;
    
    // Get the source sub sector data and alloc room for the runtime equivalent
    const MapSubSector* pSrcSubSector = (const MapSubSector*)(pResourceData + sizeof(uint32_t));
    const MapSubSector* const pEndSrcSubSector = pSrcSubSector + numSubSectors;
    
    gSubSectors.clear();
    gSubSectors.resize(numSubSectors);
    gpSubSectors = gSubSectors.data();
    
    subsector_t* pDstSubSector = gSubSectors.data();
    
    while (pSrcSubSector < pEndSrcSubSector) {
        pDstSubSector->numsublines = byteSwappedU32(pSrcSubSector->numLines);
        
        seg_t* const pLineSeg = &gLineSegs[byteSwappedU32(pSrcSubSector->firstLine)];
        pDstSubSector->firstline = pLineSeg;
        pDstSubSector->sector = pLineSeg->sidedef->sector;
    
        ++pSrcSubSector;
        ++pDstSubSector;
    }
    
    // Don't need this anymore
    freeResource(lumpResourceNum);
}

static void loadNodes(const uint32_t lumpResourceNum) noexcept {
    // Load the nodes resource
    ASSERT_LOG(gSubSectors.size() > 0, "Sub sectors must be loaded first!");
    const Resource* const pResource = loadResource(lumpResourceNum);
    const std::byte* const pResourceData = (const std::byte*) pResource->pData;
    
    // Get the number of nodes first (first u32)
    const uint32_t numNodes = byteSwappedU32(((const uint32_t*) pResourceData)[0]);
    gNumNodes = numNodes;
    
    // Get the source node data and alloc room for the runtime equivalent
    const MapNode* pSrcNode = (const MapNode*)(pResourceData + sizeof(uint32_t));
    const MapNode* const pEndSrcNode = pSrcNode + numNodes;
    
    gNodes.clear();
    gNodes.resize(numNodes);
    gpNodes = gNodes.data();
    
    node_t* pDstNode = gNodes.data();
    
    while (pSrcNode < pEndSrcNode) {
        pDstNode->Line.x = byteSwappedI32(pSrcNode->x);
        pDstNode->Line.y = byteSwappedI32(pSrcNode->y);
        pDstNode->Line.dx = byteSwappedI32(pSrcNode->dx);
        pDstNode->Line.dy = byteSwappedI32(pSrcNode->dy);
        
        for (uint32_t childNum = 0; childNum < 2; ++childNum) {
            pDstNode->bbox[childNum][0] = byteSwappedI32(pSrcNode->bbox[childNum][0]);
            pDstNode->bbox[childNum][1] = byteSwappedI32(pSrcNode->bbox[childNum][1]);
            pDstNode->bbox[childNum][2] = byteSwappedI32(pSrcNode->bbox[childNum][2]);
            pDstNode->bbox[childNum][3] = byteSwappedI32(pSrcNode->bbox[childNum][3]);
            
            // See if this child is a leaf node (subsector) or another node.
            // Unclear what happens here if node index is > 0x8000 - engine limitation on subsector count?
            const uint32_t childNodeOrSubSecIdx = byteSwappedU32(pSrcNode->children[childNum]);
            
            if (childNodeOrSubSecIdx & NF_SUBSECTOR) {
                // Child is a subsector (node is a leaf)
                const uint32_t subSectorIdx = childNodeOrSubSecIdx & (~NF_SUBSECTOR);
                
                // This seems odd, but the game uses the lowest bit of the child pointer to indicate that
                // the node is a leaf node, hence adding '1' here to the pointer address. Should be ok in
                // all cases because the bit will never be used, due to alignment...
                std::byte* pChildPtr = (std::byte*) &gSubSectors[subSectorIdx];
                pChildPtr += 1;
                pDstNode->Children[childNum] = pChildPtr;
            }
            else {
                // Child is another node
                pDstNode->Children[childNum] = &gNodes[childNodeOrSubSecIdx];
            }
        }
        
        ++pSrcNode;
        ++pDstNode;
    }
    
    // Don't need this anymore
    freeResource(lumpResourceNum);
}

static void loadReject(const uint32_t lumpResourceNum) noexcept {
    // Note: this one is easy!
    gpRejectMatrix = (const Byte*) loadResourceData(lumpResourceNum);
    gLoadedRejectMatrixResourceNum = lumpResourceNum;
}

static void loadBlockMap(const uint32_t lumpResourceNum) noexcept {
    // Load the block map resource
    ASSERT_LOG(gLines.size() > 0, "Lines must be loaded first!");
    const Resource* const pResource = loadResource(lumpResourceNum);
    const std::byte* const pResourceData = (const std::byte*) pResource->pData;
    
    // Read the header info for the blockmap (first 4 32-bit integers)
    gBlockMapOriginX = byteSwappedI32(((const Fixed*) pResourceData)[0]);
    gBlockMapOriginY = byteSwappedI32(((const Fixed*) pResourceData)[1]);
    gBlockMapWidth = byteSwappedU32(((const uint32_t*) pResourceData)[2]);
    gBlockMapHeight = byteSwappedU32(((const uint32_t*) pResourceData)[3]);
    
    // The number of entries in the blockmap
    const uint32_t numBlockMapEntries = gBlockMapWidth * gBlockMapHeight;
    
    // This section of the blockmap gives the byte offset of first line number for each block within the blockmap data.
    // The list of lines for each block is terminated by UINT32_MAX.
    const uint32_t* const pBegLineListOffset = ((const uint32_t*) pResourceData) + 4;
    const uint32_t* const pEndLineListOffset = pBegLineListOffset + numBlockMapEntries;
    
    // Now figure out how many actual line list entries there are in the blockmap and where they
    // start and end in the resource data:
    const uint32_t numLineListEntries = (pResource->size / sizeof(uint32_t)) - 4 - numBlockMapEntries;
    const uint32_t* const pBegLineListEntry = pEndLineListOffset;
    const uint32_t* const pEndLineListEntry = pBegLineListEntry + numLineListEntries;
    
    // Now first of all read all of the line list entries.
    // These are simply a series of uint32_t line numbers, with UINT32_MAX meaning 'end of list':
    gBlockMapLines.resize(numLineListEntries);
    
    {
        const uint32_t* pCurLineListEntry = pBegLineListEntry;
        line_t** pDstLinePtr = gBlockMapLines.data();
        
        while (pCurLineListEntry < pEndLineListEntry) {
            const uint32_t lineNum = byteSwappedU32(*pCurLineListEntry);
            
            if (lineNum != UINT32_MAX) {
                ASSERT(lineNum < gNumLines);
                *pDstLinePtr = &gpLines[lineNum];
            }
            else {
                *pDstLinePtr = nullptr;
            }
            
            ++pCurLineListEntry;
            ++pDstLinePtr;
        }
    }
    
    // This tells where the line list entries start in the resource data, in multiples of 'uint32_t'
    const uint32_t lineListEntriesStartU32Idx = 4 + numBlockMapEntries;
    
    // Next read where the lines for each blockmap line list starts.
    // This is given in terms of an offset into the blockmap resource:
    gBlockMapLineLists.resize(numBlockMapEntries);
    gpBlockMapLineLists = gBlockMapLineLists.data();
    
    {
        const uint32_t* pCurLineListOffset = pBegLineListOffset;
        line_t*** pDstLineList = gBlockMapLineLists.data();
        
        while (pCurLineListOffset < pEndLineListOffset) {
            const uint32_t lineListByteOffset = byteSwappedU32(*pCurLineListOffset);
            const uint32_t lineListIdx = (lineListByteOffset / sizeof(uint32_t)) - lineListEntriesStartU32Idx;
            
            ASSERT(lineListIdx < gBlockMapLines.size());
            *pDstLineList = gBlockMapLines.data() + lineListIdx;
            
            ++pCurLineListOffset;
            ++pDstLineList;
        }
    }
    
    // Finally allocate room for the linked list of things for each blockmap entry
    gBlockMapThingLists.clear();
    gBlockMapThingLists.resize(numBlockMapEntries);
    gpBlockMapThingLists = gBlockMapThingLists.data();
    
    // Don't need this anymore
    freeResource(lumpResourceNum);
}

#ifdef __cplusplus
extern "C" {
#endif

// External data pointers and information
const vertex_t*     gpVertexes;
uint32_t            gNumVertexes;
sector_t*           gpSectors;
uint32_t            gNumSectors;
side_t*             gpSides;
uint32_t            gNumSides;
line_t*             gpLines;
uint32_t            gNumLines;
const seg_t*        gpLineSegs;
uint32_t            gNumLineSegs;
const subsector_t*  gpSubSectors;
uint32_t            gNumSubSectors;
const node_t*       gpNodes;
uint32_t            gNumNodes;
const Byte*         gpRejectMatrix;
line_t***           gpBlockMapLineLists;
mobj_t**            gpBlockMapThingLists;
uint32_t            gBlockMapWidth;
uint32_t            gBlockMapHeight;
Fixed               gBlockMapOriginX;
Fixed               gBlockMapOriginY;

void mapDataInit(const uint32_t mapNum) {
    // Load all the map data.
    // N.B: must be done in this order due to data dependencies!
    const uint32_t mapStartLump = getMapStartLump(mapNum);
    
    loadVertexes(mapStartLump + ML_VERTEXES);
    loadSectors(mapStartLump + ML_SECTORS);
    loadSides(mapStartLump + ML_SIDEDEFS);
    loadLines(mapStartLump + ML_LINEDEFS);
    loadLineSegs(mapStartLump + ML_SEGS);
    loadSubSectors(mapStartLump + ML_SSECTORS);
    loadNodes(mapStartLump + ML_NODES);
    loadReject(mapStartLump + ML_REJECT);
    loadBlockMap(mapStartLump + ML_BLOCKMAP);
}

void mapDataShutdown() {
    gVertexes.clear();
    gpVertexes = nullptr;
    gNumVertexes = 0;
    
    gSectors.clear();
    gpSectors = nullptr;
    gNumSectors = 0;
    
    gSides.clear();
    gpSides = nullptr;
    gNumSides = 0;
    
    gLines.clear();
    gpLines = nullptr;
    gNumLines = 0;
    
    gLineSegs.clear();
    gpLineSegs = nullptr;
    gNumLineSegs = 0;
    
    gSubSectors.clear();
    gpSubSectors = nullptr;
    gNumSubSectors = 0;
    
    gNodes.clear();
    gpNodes = nullptr;
    gNumNodes = 0;
    
    if (gLoadedRejectMatrixResourceNum > 0) {
        freeResource(gLoadedRejectMatrixResourceNum);
        gLoadedRejectMatrixResourceNum = 0;
    }
    
    gpRejectMatrix = nullptr;
    
    gBlockMapLines.clear();
    gBlockMapLineLists.clear();
    gBlockMapThingLists.clear();
    gpBlockMapLineLists = nullptr;
    gpBlockMapThingLists = nullptr;
    gBlockMapWidth = 0;
    gBlockMapHeight = 0;
    gBlockMapOriginX = 0;
    gBlockMapOriginY = 0;
}

#ifdef __cplusplus
}
#endif
