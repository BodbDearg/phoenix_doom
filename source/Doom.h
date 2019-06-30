#pragma once

#include "Burger.h"
#include "Sprites.h"
#include "States.h"

struct player_t;
struct subsector_t;

//----------------------------------------------------------------------------------------------------------------------
// Global typedefs
//----------------------------------------------------------------------------------------------------------------------
typedef int32_t     Fixed;      // 16.16 fixed point number
typedef uint32_t    angle_t;    // 32-bit BAM angle (uses the full 32-bits to represent 360)

/****************************

    Global defines

****************************/

#define FRACBITS 16             /* Number of fraction bits in Fixed */
#define FRACUNIT (1<<FRACBITS)  /* 1.0 in fixed point */
#define ANG45 0x20000000        /* 45 degrees in angle_t */
#define ANG90 0x40000000        /* 90 degrees in angle_t */
#define ANG180 0x80000000       /* 180 degrees in angle_t */
#define ANG270 0xC0000000       /* 270 degrees in angle_t */
#define FINEANGLES 8192         /* Size of the fineangle table */
#define FINEMASK (FINEANGLES-1) /* Rounding mask for table */
#define ANGLETOFINESHIFT 19     /* Convert angle_t to fineangle table */
#define ANGLETOSKYSHIFT 22      /* sky map is 256*128*4 maps */
#define MINZ (FRACUNIT*4)       /* Closest z allowed */
#define FIELDOFVIEW 2048        /* 90 degrees of view */
#define BFGCELLS 40             /* Number of energy units per blast */
#define MAXHEALTH 100           /* Normal health at start of game */
#define SKULLSPEED (40*FRACUNIT)    /* Speed of the skull to attack */
#define MAXVISSPRITES 128           /* Maximum number of visible sprites */
#define MAXVISPLANES 64 /* Maximum number of visible floor and ceiling textures */
#define MAXWALLCMDS 128 /* Maximum number of visible walls */
#define MAXOPENINGS MAXSCREENWIDTH*64   /* Space for sil tables */
#define MAXSCREENHEIGHT 160     /* Maximum height allowed */
#define MAXSCREENWIDTH 280      /* Maximum width allowed */
#define SLOPERANGE 2048         /* Number of entries in tantoangle table */
#define SLOPEBITS 11            /* Power of 2 for SLOPERANGE (2<<11) */
#define HEIGHTBITS 6            /* Number of bits for texture height */
#define FIXEDTOSCALE (FRACBITS-SCALEBITS)   /* Number of unused bits from fixed to HEIGHTBITS */
#define SCALEBITS 9             /* Number of bits for texture scale */
#define FIXEDTOHEIGHT (FRACBITS-HEIGHTBITS) /* Number of unused bits from fixed to SCALEBITS */
#define MAXINT ((int)0x7fffffff)    /* max pos 32-bit int */
#define MININT ((int)0x80000000)    /* max negative 32-bit integer */
#define ONFLOORZ MININT     /* Attach object to floor with this z */
#define ONCEILINGZ MAXINT   /* Attach object to ceiling with this z */
#define FLOATSPEED (8*FRACUNIT) /* Speed an object can float vertically */
#define GRAVITY (4*FRACUNIT)    /* Rate of fall */
#define MAXMOVE (16*FRACUNIT)   /* Maximum velocity */
#define VIEWHEIGHT (41*FRACUNIT)    /* Height to render from */
#define INVULNTICS (30*TICKSPERSEC) /* Time for invulnerability */
#define INVISTICS (60*TICKSPERSEC)  /* Time for invisibility */
#define INFRATICS (120*TICKSPERSEC) /* Time for I/R goggles */
#define IRONTICS (60*TICKSPERSEC)   /* Time for rad suit */
#define MAPBLOCKSHIFT (FRACBITS+7)  /* Shift value to convert Fixed to 128 pixel blocks */
#define PLAYERRADIUS (16<<FRACBITS) /* Radius of the player */
#define MAXRADIUS (32<<FRACBITS)    /* Largest radius of any critter */
#define USERANGE (70<<FRACBITS)     /* Range of pressing spacebar to activate something */
#define MELEERANGE (70<<FRACBITS)   /* Range of hand to hand combat */
#define MISSILERANGE (32*64<<FRACBITS)  /* Range of guns targeting */

/****************************

    Global Enums

****************************/

typedef enum {      /* Skill level settings */
    sk_baby,
    sk_easy,
    sk_medium,
    sk_hard,
    sk_nightmare
} skill_t;

typedef enum {      /* Current game state setting */
    ga_nothing,
    ga_died,
    ga_completed,
    ga_secretexit,
    ga_warped,
    ga_exitdemo
} gameaction_t;

typedef enum {      /* Current state of the player */
    PST_LIVE,       /* playing */
    PST_DEAD,       /* dead on the ground */
    PST_REBORN      /* ready to restart */
} playerstate_t;

typedef enum {      /* Sprites to draw on the playscreen */
    ps_weapon,      /* Currently selected weapon */
    ps_flash,       /* Weapon muzzle flash */
    NUMPSPRITES     /* Number of shapes for array sizing */
} psprnum_t;

typedef enum {      /* Item types for keycards or skulls */
    it_bluecard,
    it_yellowcard,
    it_redcard,
    it_blueskull,
    it_yellowskull,
    it_redskull,
    NUMCARDS        /* Number of keys for array sizing */
} card_t;

typedef enum {      /* Currently selected weapon */
    wp_fist,
    wp_pistol,
    wp_shotgun,
    wp_chaingun,
    wp_missile,
    wp_plasma,
    wp_bfg,
    wp_chainsaw,
    NUMWEAPONS,     /* Number of valid weapons for array sizing */
    wp_nochange     /* Inbetween weapons (Can't fire) */
} weapontype_t;

typedef enum {      /* Current ammo being used */
    am_clip,        /* pistol / chaingun */
    am_shell,       /* shotgun */
    am_cell,        /* BFG */
    am_misl,        /* missile launcher */
    NUMAMMO,        /* Number of valid ammo types for array sizing */
    am_noammo       /* chainsaw / fist */
} ammotype_t;

typedef enum {      /* Power index flags */
    pw_invulnerability, /* God mode */
    pw_strength,        /* Strength */
    pw_ironfeet,        /* Radiation suit */
    pw_allmap,          /* Mapping */
    pw_invisibility,    /* Can't see me! */
    NUMPOWERS           /* Number of powerups */
} powertype_t;

typedef enum {      /* Slopes for BSP tree */
    ST_HORIZONTAL,ST_VERTICAL,ST_POSITIVE,ST_NEGATIVE
} slopetype_t;

typedef enum {      /* Faces for our hero */
    f_none,         /* Normal state */
    f_faceleft,     /* turn face left */
    f_faceright,    /* turn face right */
    f_hurtbad,      /* surprised look when slammed hard */
    f_gotgat,       /* picked up a weapon smile */
    f_mowdown,      /* grimace while continuous firing */
    NUMSPCLFACES
} spclface_e;

// Basic direction list
enum dirtype_t : uint32_t {      
    DI_EAST,            // East etc...
    DI_NORTHEAST,
    DI_NORTH,
    DI_NORTHWEST,
    DI_WEST,
    DI_SOUTHWEST,
    DI_SOUTH,
    DI_SOUTHEAST,
    DI_NODIR            // No direction at all
};

typedef enum {      /* Enums for floor types */
    lowerFloor,         /* lower floor to highest surrounding floor */
    lowerFloorToLowest, /* lower floor to lowest surrounding floor */
    turboLower,         /* lower floor to highest surrounding floor VERY FAST */
    raiseFloor,         /* raise floor to lowest surrounding CEILING */
    raiseFloorToNearest,    /* raise floor to next highest surrounding floor */
    raiseToTexture,     /* raise floor to shortest height texture around it */
    lowerAndChange,     /* lower floor to lowest surrounding floor and change */
                        /* floorpic */
    raiseFloor24,
    raiseFloor24AndChange,
    raiseFloorCrush,
    donutRaise
} floor_e;

// Enums for moving sector results
typedef enum {
    moveok,
    crushed,
    pastdest
} result_e;

// Enums for ceiling types
enum ceiling_e {          
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    fastCrushAndRaise
};

// Enums for platform types
enum plattype_e {
    perpetualRaise,
    downWaitUpStay,
    raiseAndChange,
    raiseToNearestAndChange
};

// Enums for door types
enum vldoor_e {      
    normaldoor,
    close30ThenOpen,
    close,
    open,
    raiseIn5Mins
};

// bbox coordinates
enum {
    BOXTOP,
    BOXBOTTOM,
    BOXLEFT,
    BOXRIGHT,
    BOXCOUNT
};

/****************************

    Global structs

****************************/

typedef struct {        /* Struct for animating textures */
    Word LastPicNum;    /* Picture referance number */
    Word BasePic;       /* Base texture # */
    Word CurrentPic;    /* Current index */
} anim_t;

#define MTF_EASY 1
#define MTF_NORMAL 2
#define MTF_HARD 4
#define MTF_AMBUSH 8
#define MTF_DEATHMATCH 16

typedef struct {    /* Struct describing an object on the map */
    Fixed x,y;      /* X,Y position (Signed) */
    angle_t angle;  /* Angle facing */
    Word type;      /* Object type */
    Word ThingFlags;    /* mapthing flags */
} mapthing_t;

#define FF_FULLBRIGHT   0x00000100  /* Flag in thing->frame for full light */
#define FF_FRAMEMASK    0x000000FF  /* Use the rest */
#define FF_SPRITESHIFT  16          /* Bits to shift for group */

// Describes a 2D shape rendered to the screen
typedef struct {
    int x1,x2,y1,y2;                    // Clipped to screen edges column range
    Fixed xscale;                       // Scale factor
    Fixed yscale;                       // Y Scale factor
    const SpriteFrameAngle* pSprite;    // What sprite frame to actually render
    Word colormap;                      // 0x8000 = shadow draw,0x4000 flip, 0x3FFF color map
    const struct mobj_t *thing;         // Used for clipping...
} vissprite_t;

// Describes a floor texture
typedef struct {
    Word        open[MAXSCREENWIDTH+1];     // top<<8 | bottom
    Fixed       height;                     // Height of the floor
    uint32_t    PicHandle;                  // Texture handle
    Word        PlaneLight;                 // Light override
    int         minx, maxx;                 // Minimum x, max x
} visplane_t;

struct sector_t {       /* Describe a playfield sector (Polygon) */
    Fixed floorheight;  /* Floor height */
    Fixed ceilingheight;    /* Top and bottom height */
    Word FloorPic;      /* Floor texture # */
    Word CeilingPic;    /* If ceilingpic==-1, draw sky */
    Word lightlevel;    /* Light override */
    Word special;       /* Special event number */
    Word tag;           /* Event tag */
    Word soundtraversed;    /* 0 = untraversed, 1,2 = sndlines -1 */
    mobj_t *soundtarget;    /* thing that made a sound (or null) */

    Word blockbox[BOXCOUNT];    /* mapblock bounding box for height changes */
    Fixed SoundX,SoundY;            /* For any sounds played by the sector */
    Word validcount;    /* if == validcount, already checked */
    mobj_t *thinglist;  /* list of mobjs in sector */
    void *specialdata;  /* Thinker struct for reversable actions */
    Word linecount;     /* Number of lines in polygon */
    struct line_t **lines;  /* [linecount] size */
};

struct vector_t {        /* Vector struct */
    Fixed x,y;          /* X,Y start of line */
    Fixed dx,dy;        /* Distance traveled */
};

struct vertex_t {    /* Point in a map */
    Fixed x,y;      /* X and Y coord of dot */
};

typedef struct {    /* Data for a line side */
    Fixed textureoffset;    /* Column texture offset (X) */
    Fixed rowoffset;        /* Row texture offset (Y) */
    Word toptexture,bottomtexture,midtexture;   /* Wall textures */
    sector_t *sector;       /* Pointer to parent sector */
} side_t;

#define ML_BLOCKING     1       /* Line blocks all movement */
#define ML_BLOCKMONSTERS 2      /* Line blocks monster movement */  
#define ML_TWOSIDED     4       /* This line has two sides */
#define ML_DONTPEGTOP   8       /* Top texture is bottom anchored */
#define ML_DONTPEGBOTTOM 16     /* Bottom texture is bottom anchored */
#define ML_SECRET       32  /* don't map as two sided: IT'S A SECRET! */
#define ML_SOUNDBLOCK   64  /* don't let sound cross two of these */
#define ML_DONTDRAW     128 /* don't draw on the automap */
#define ML_MAPPED       256 /* set if allready drawn in automap */

typedef struct line_t {
    vertex_t v1,v2;     /* X,Ys for the line ends */
    Word flags;         /* Bit flags (ML_) for states */
    Word special;       /* Event number */
    Word tag;           /* Event tag number */
    side_t *SidePtr[2]; /* Sidenum[1] will be NULL if one sided */
    Fixed bbox[BOXCOUNT];   /* Bounding box for quick clipping */
    slopetype_t slopetype;  /* To aid move clipping */
    sector_t *frontsector;  /* Front line side sector */
    sector_t *backsector;   /* Back side sector (NULL if one sided) */
    Word validcount;        /* Prevent recursion flag */
    Word fineangle;         /* Index to get sine / cosine for sliding */
} line_t;

typedef struct seg_t {      /* Structure for a line segment */
    vertex_t v1,v2;         /* Source and dest points */
    angle_t angle;          /* Angle of the vector */
    Fixed offset;           /* Extra shape offset */
    side_t *sidedef;        /* Pointer to the connected side */
    line_t *linedef;        /* Pointer to the connected line */
    sector_t *frontsector;  /* Sector on the front side */
    sector_t *backsector;   /* NULL for one sided lines */
} seg_t;

typedef struct subsector_t {    /* Subsector structure */
    sector_t *sector;           /* Pointer to parent sector */
    Word numsublines;           /* Number of subsector lines */
    seg_t *firstline;           /* Pointer to the first line */
} subsector_t;

typedef struct {
    vector_t Line;      /* BSP partition line */
    Fixed bbox[2][BOXCOUNT]; /* Bounding box for each child */
    void *Children[2];      /* If low bit is set then it's a subsector */
} node_t;

//---------------------------------------------------------------------------------------------------------------------
// DC: BSP nodes use the lowest bit of their child pointers to indicate whether or not the child
// pointed to is a subsector or just another BSP node. These 3 functions help with checking for
// the prescence of this flag and adding/removing it from a node child pointer.
//---------------------------------------------------------------------------------------------------------------------
static inline bool isNodeChildASubSector(const void* const pPtr) {
    return ((((uintptr_t) pPtr) & 1) != 0);
}

static inline const void* markNodeChildAsSubSector(const void* const pPtr) {
    // Set the lowest bit to mark the node's child pointer as a subsector
    return (const void*)(((uintptr_t) pPtr) | 1);
}

static inline const void* getActualNodeChildPtr(const void* const pPtr) {
    // Remove the lowest bit to get the actual child pointer for a node.
    // May be set in order to indicate that the child is a subsector:
    const uintptr_t mask = ~((uintptr_t) 1);
    return (const void*)(((uintptr_t) pPtr) & mask);
}

#define AC_ADDFLOOR 1
#define AC_ADDCEILING 2
#define AC_TOPTEXTURE 4
#define AC_BOTTOMTEXTURE 8
#define AC_NEWCEILING 16
#define AC_NEWFLOOR 32
#define AC_ADDSKY 64
#define AC_TOPSIL 128
#define AC_BOTTOMSIL 256
#define AC_SOLIDSIL 512

/* Describe a wall segment to be drawn */
typedef struct {        
    uint32_t    LeftX;          /* Leftmost x screen coord */
    uint32_t    RightX;         /* Rightmost inclusive x coordinates */
    uint32_t    FloorPic;       /* Picture handle to floor shape */
    uint32_t    CeilingPic;     /* Picture handle to ceiling shape */
    uint32_t    WallActions;    /* Actions to perform for draw */

    int t_topheight;                        /* Describe the top texture */
    int t_bottomheight;
    int t_texturemid;
    const struct Texture* t_texture;        /* Pointer to the top texture */
    
    int b_topheight;                        /* Describe the bottom texture */
    int b_bottomheight;
    int b_texturemid;
    const struct Texture* b_texture;        /* Pointer to the bottom texture */
    
    int floorheight;
    int floornewheight;
    int ceilingheight;
    int ceilingnewheight;

    Fixed       LeftScale;      /* LeftX Scale */
    Fixed       RightScale;     /* RightX scale */
    Fixed       SmallScale;
    Fixed       LargeScale;
    Byte*       TopSil;         /* YClips for the top line */
    Byte*       BottomSil;      /* YClips for the bottom line */
    Fixed       ScaleStep;      /* Scale step factor */
    angle_t     CenterAngle;    /* Center angle */
    Fixed       offset;         /* Offset to the texture */
    Word        distance;
    Word        seglightlevel;
    seg_t*      SegPtr;         /* Pointer to line segment for clipping */  
} viswall_t;

/* Describe data on the status bar */
typedef struct {        
    spclface_e  specialFace;        /* Which type of special face to make */
    bool        gotgibbed;          /* Got gibbed */
    bool        tryopen[NUMCARDS];  /* Tried to open a card or skull door */
} stbar_t;

// In Sound.c
extern void S_Clear();
extern void S_StartSound(const Fixed* const pOriginXY, const uint32_t soundId);
extern void S_StartSong(const uint8_t musicId);
extern void S_StopSong();

// In DMain.c
extern void AddToBox(Fixed *box,Fixed x,Fixed y);

extern Word MiniLoop(
    void (*start)(),
    void (*stop)(),
    Word (*ticker)(),
    void (*drawer)()
);

extern void D_DoomMain(void);

/* In Inter.c */
extern void TouchSpecialThing(mobj_t *special,mobj_t *toucher);
extern void DamageMObj(mobj_t *target,mobj_t *inflictor,mobj_t *source,Word damage);

/* In Switch.c */
extern Word NumSwitches;        /* Number of switches * 2 */
extern Word SwitchList[];
extern void P_InitSwitchList(void);
extern void P_ChangeSwitchTexture(line_t *line, bool useAgain);
extern bool P_UseSpecialLine(mobj_t *thing, line_t *line);

/* In FMain.c */

extern void F_Start(void);
extern void F_Stop(void);
extern Word F_Ticker(void);
extern void F_Drawer(void);

/* In Ceilng.c */

extern bool EV_DoCeiling(line_t *line,ceiling_e type);
extern bool EV_CeilingCrushStop(line_t *line);
extern void ResetCeilings(void);

/* In Plats.c */

extern bool EV_DoPlat(line_t *line,plattype_e type,Word amount);
extern void EV_StopPlat(line_t *line);
extern void ResetPlats(void);

/* In Doors.c */

extern bool EV_DoDoor(line_t *line,vldoor_e type);
extern void EV_VerticalDoor(line_t *line,mobj_t *thing);
extern void P_SpawnDoorCloseIn30(sector_t *sec);
extern void P_SpawnDoorRaiseIn5Mins(sector_t *sec);

/* In Setup.c */
extern mapthing_t deathmatchstarts[10],*deathmatch_p;   /* Deathmatch starts */
extern mapthing_t playerstarts;                         /* Starting position for players */

extern void SetupLevel(Word map);
extern void ReleaseMapMemory(void);
extern void P_Init(void);

/* In Telept.c */
extern bool EV_Teleport(line_t *line,mobj_t *thing);

/* In RData.c */
extern void R_InitData(void);
extern void InitMathTables(void);

/* In Change.c */
extern Word ChangeSector(sector_t *sector,Word crunch);

/* In User.c */

extern void P_PlayerThink(player_t *player);

/* In Sight.c */
extern Word CheckSight(mobj_t *t1,mobj_t *t2);

/* In Base.c */

extern void P_RunMobjBase(void);

/* In OMain.c */

extern void O_Init(void);
extern void O_Control(player_t *player);
extern void O_Drawer(void);

/* In MMain.c */

extern void M_Start(void);
extern void M_Stop(void);
extern Word M_Ticker(void);
extern void M_Drawer(void);

/* In StMain.c */

extern stbar_t stbar;       /* Pass messages to the status bar */

extern void ST_Start(void);
extern void ST_Stop(void);
extern void ST_Ticker(void);
extern void ST_Drawer(void);

/* In Phase1.c */
extern Word SpriteTotal;        /* Total number of sprites to render */
extern Word *SortedSprites;     /* Pointer to array of words of sprites to render */
extern void BSP();

/* In Phase2.c */
extern void WallPrep(Word LeftX,Word RightX,seg_t *LineSeg,angle_t LineAngle);

/* In Phase6.c */
extern void SegCommands(void);

/* In Phase7.c */
extern Byte *PlaneSource;                           /* Pointer to floor shape */
extern Fixed planey;                                /* latched viewx / viewy for floor drawing */
extern Fixed basexscale,baseyscale;
extern void DrawVisPlane(visplane_t *PlanePtr);

/* In Phase8.c */
extern Word spropening[MAXSCREENWIDTH];     /* clipped range */
extern Word *SortWords(Word *Before,Word *After,Word Total);
extern void DrawVisSprite(const vissprite_t* const pVisSprite);
extern void DrawAllSprites(void);
extern void DrawWeapons(void);

/* In AppleIIgs.c, Threedo.c, IBM.c Etc... */

extern LongWord LastTics;       /* Time elapsed since last page flip */

extern void WritePrefsFile(void);
extern void ClearPrefsFile(void);
extern void ReadPrefsFile(void);
extern void UpdateAndPageFlip(const bool bAllowDebugClear);
extern void DrawPlaque(Word RezNum);
extern void DrawSkyLine(void);

extern void DrawWallColumn(
    const Word y,
    const Word Colnum,
    const Word ColY,
    const Word TexHeight,
    const Byte* const Source,
    const Word Run
);

extern void DrawFloorColumn(Word ds_y,Word ds_x1,Word Count,LongWord xfrac,
    LongWord yfrac,Fixed ds_xstep,Fixed ds_ystep);
extern void DrawSpriteNoClip(const vissprite_t* const pVisSprite);
extern void DrawSpriteClip(const Word x1, const Word x2, const vissprite_t* const pVisSprite);
extern void DrawSpriteCenter(Word SpriteNum);
extern void EnableHardwareClipping(void);
extern void DisableHardwareClipping(void);
extern void DrawColors(void);
