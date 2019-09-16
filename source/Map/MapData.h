#pragma once

#include "Base/Angle.h"
#include "Base/Macros.h"
#include "Game/DoomDefines.h"

struct line_t;
struct mobj_t;

// Point in a map (2d coords)
struct vertex_t {
    Fixed x;
    Fixed y;
};

// The same as 'vertex_t', except in floating point format
struct vertexf_t {
    float x;
    float y;
};

// Vector struct
struct vector_t {
    Fixed x;        // X,Y start of line
    Fixed y;
    Fixed dx;       // Distance traveled (delta)
    Fixed dy;
};

// Describe a playfield sector (Polygon)
struct sector_t {
    NON_ASSIGNABLE_STRUCT(sector_t)

    Fixed       floorheight;            // Floor height
    Fixed       ceilingheight;          // Top and bottom height
    uint32_t    FloorPic;               // Floor texture #
    uint32_t    CeilingPic;             // If ceilingpic==-1, draw sky
    uint32_t    lightlevel;             // Light override
    uint32_t    special;                // Special event number
    uint32_t    tag;                    // Event tag
    uint32_t    soundtraversed;         // 0 = untraversed, 1,2 = sndlines -1
    mobj_t*     soundtarget;            // thing that made a sound (or null)
    uint32_t    blockbox[BOXCOUNT];     // mapblock bounding box for height changes
    Fixed       SoundX;                 // For any sounds played by the sector
    Fixed       SoundY;                 // For any sounds played by the sector
    uint32_t    validcount;             // if == validcount, already checked
    mobj_t*     thinglist;              // list of mobjs in sector
    void*       specialdata;            // Thinker struct for reversable actions
    uint32_t    linecount;              // Number of lines in polygon
    line_t**    lines;                  // [linecount] size
};

// Data for a line side
struct side_t {
    NON_ASSIGNABLE_STRUCT(side_t)

    float       texXOffset;         // Column texture offset (X)
    float       texYOffset;         // Row texture offset (Y)
    uint32_t    toptexture;         // Wall textures
    uint32_t    bottomtexture;
    uint32_t    midtexture;
    sector_t*   sector;             // Pointer to parent sector
};

// Slope type for a line
enum slopetype_e {
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE
};

// Data for a line
struct line_t {
    NON_ASSIGNABLE_STRUCT(line_t)

    vertex_t        v1;                     // X,Ys for the line ends
    vertex_t        v2;
    vertexf_t       v1f;                    // The same in float format
    vertexf_t       v2f;
    uint32_t        flags;                  // Bit flags (ML_) for states
    uint32_t        special;                // Event number
    uint32_t        tag;                    // Event tag number
    side_t*         SidePtr[2];             // Sidenum[1] will be NULL if one sided
    Fixed           bbox[BOXCOUNT];         // Bounding box for quick clipping
    slopetype_e     slopetype;              // To aid move clipping
    sector_t*       frontsector;            // Front line side sector
    sector_t*       backsector;             // Back side sector (NULL if one sided)
    uint32_t        fineangle;              // Index to get sine / cosine for sliding

    // Intrusive/non-property fields
    uint32_t    validCount;             // Keeps track of whether the line has been visited during certain operations
    float       v1DrawDepth;            // Depth of v1 and v2 when drawn
    float       v2DrawDepth;
    uint8_t     drawnSideIndex;         // Which side of the line is being rendered
    bool        bIsInFrontOfSprite;     // Used during sprite clipping: stores if the line is considered in front of the sprite
};

// Flags that can be applied to a line
static constexpr uint32_t ML_BLOCKING       = 0x1;      // Line blocks all movement
static constexpr uint32_t ML_BLOCKMONSTERS  = 0x2;      // Line blocks monster movement
static constexpr uint32_t ML_TWOSIDED       = 0x4;      // This line has two sides
static constexpr uint32_t ML_DONTPEGTOP     = 0x8;      // Top texture is bottom anchored
static constexpr uint32_t ML_DONTPEGBOTTOM  = 0x10;     // Bottom texture is bottom anchored
static constexpr uint32_t ML_SECRET         = 0x20;     // Don't map as two sided: IT'S A SECRET!
static constexpr uint32_t ML_SOUNDBLOCK     = 0x40;     // don't let sound cross two of these
static constexpr uint32_t ML_DONTDRAW       = 0x80;     // don't draw on the automap
static constexpr uint32_t ML_MAPPED         = 0x100;    // set if allready drawn in automap

// Structure for a line segment
struct seg_t {
    NON_ASSIGNABLE_STRUCT(seg_t)

    vertexf_t   v1;             // Source and dest points
    vertexf_t   v2;
    angle_t     angle;          // Angle of the vector
    float       texXOffset;     // Extra texture offset in the x direction
    side_t*     sidedef;        // Pointer to the connected side
    line_t*     linedef;        // Pointer to the connected line
    sector_t*   frontsector;    // Sector on the front side
    sector_t*   backsector;     // NULL for one sided lines
    float       lightMul;       // Wall light multiplier (used for fake contrast)

    uint8_t getLineSideIndex() const noexcept {
        return (linedef->SidePtr[0] == sidedef) ? 0 : 1;
    }
};

// Subsector structure
struct subsector_t {
    NON_ASSIGNABLE_STRUCT(subsector_t)

    sector_t*   sector;         // Pointer to parent sector
    uint32_t    numsublines;    // Number of subsector lines
    seg_t*      firstline;      // Pointer to the first line
};

// BSP partition line
struct node_t {
    NON_ASSIGNABLE_STRUCT(node_t)

    vector_t    Line;
    Fixed       bbox[2][BOXCOUNT];      // Bounding box for each child
    void*       Children[2];            // If low bit is set then it's a subsector
};

//---------------------------------------------------------------------------------------------------------------------
// DC: BSP nodes use the lowest bit of their child pointers to indicate whether or not the child
// pointed to is a subsector or just another BSP node. These 3 functions help with checking for
// the prescence of this flag and adding/removing it from a node child pointer.
//---------------------------------------------------------------------------------------------------------------------
static inline bool isBspNodeASubSector(const void* const pPtr) {
    return ((((uintptr_t) pPtr) & 1) != 0);
}

static inline const void* markBspNodeAsSubSector(const void* const pPtr) {
    // Set the lowest bit to mark the node's child pointer as a subsector
    return (const void*)(((uintptr_t) pPtr) | 1);
}

static inline const void* getActualBspNodePtr(const void* const pPtr) {
    // Remove the lowest bit to get the actual child pointer for a node.
    // May be set in order to indicate that the child is a subsector:
    const uintptr_t mask = ~((uintptr_t) 1);
    return (const void*)(((uintptr_t) pPtr) & mask);
}

// Pointers to global map data for ease of access
extern const vertex_t*      gpVertexes;
extern uint32_t             gNumVertexes;
extern sector_t*            gpSectors;
extern uint32_t             gNumSectors;
extern side_t*              gpSides;
extern uint32_t             gNumSides;
extern line_t*              gpLines;
extern uint32_t             gNumLines;
extern const seg_t*         gpLineSegs;
extern uint32_t             gNumLineSegs;
extern const subsector_t*   gpSubSectors;
extern uint32_t             gNumSubSectors;
extern const node_t*        gpBSPTreeRoot;
extern const uint8_t*       gpRejectMatrix;         // For fast sight rejection
extern line_t***            gpBlockMapLineLists;    // For each blockmap entry, a pointer to a list of line pointers (all lines in the block)
extern mobj_t**             gpBlockMapThingLists;   // For each blockmap entry, a pointer to the first thing in a linked list of things (all things in the block)
extern uint32_t             gBlockMapWidth;
extern uint32_t             gBlockMapHeight;
extern Fixed                gBlockMapOriginX;
extern Fixed                gBlockMapOriginY;

// Load all map data for the specified map and release it
void mapDataInit(const uint32_t mapNum);
void mapDataShutdown();
