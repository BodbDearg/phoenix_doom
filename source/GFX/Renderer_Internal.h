#pragma once

//----------------------------------------------------------------------------------------------------------------------
// Data structures and globals internal to the renderer.
// Nothing here is used by outside code.
//----------------------------------------------------------------------------------------------------------------------

#include "Base/Angle.h"
#include "Game/DoomDefines.h"
#include "Renderer.h"
#include <cstddef>

struct mobj_t;
struct seg_t;
struct SpriteFrameAngle;
struct Texture;

namespace Renderer {
    //==================================================================================================================
    // Constants
    //==================================================================================================================

    // Limits
    static constexpr uint32_t   MAXVISSPRITES       = 128;                      // Maximum number of visible sprites
    static constexpr uint32_t   MAXVISPLANES        = 64;                       // Maximum number of visible floor and ceiling textures
    static constexpr uint32_t   MAXWALLCMDS         = 128;                      // Maximum number of visible walls
    static constexpr uint32_t   MAXOPENINGS         = MAXSCREENWIDTH * 64;      // Space for sil tables
    static constexpr Fixed      MINZ                = FRACUNIT * 4;             // Closest z allowed (clip sprites closer than this etc.)
    static constexpr uint32_t   FIELDOFVIEW         = 2048;                     // 90 degrees of view

    // Rendering constants
    static constexpr uint32_t   HEIGHTBITS          = 6;                        // Number of bits for texture height
    static constexpr uint32_t   FIXEDTOHEIGHT       = FRACBITS - HEIGHTBITS;    // Number of unused bits from fixed to SCALEBITS
    static constexpr uint32_t   SCALEBITS           = 9;                        // Number of bits for texture scale
    static constexpr uint32_t   FIXEDTOSCALE        = FRACBITS - SCALEBITS;     // Number of unused bits from fixed to HEIGHTBITS
    static constexpr uint32_t   ANGLETOSKYSHIFT     = 22;                       // sky map is 256*128*4 maps

    //==================================================================================================================
    // Data structures
    //==================================================================================================================

    // Describes a 2D shape rendered to the screen
    struct vissprite_t {
        int32_t                     x1;             // Clipped to screen edges column range
        int32_t                     x2;
        int32_t                     y1;
        int32_t                     y2;
        Fixed                       xscale;         // Scale factor
        Fixed                       yscale;         // Y Scale factor
        const SpriteFrameAngle*     pSprite;        // What sprite frame to actually render
        uint32_t                    colormap;       // 0x8000 = shadow draw,0x4000 flip, 0x3FFF color map
        const mobj_t*               thing;          // Used for clipping...
    };

    // Describes a floor area to be drawn
    struct visplane_t {
        uint32_t    open[MAXSCREENWIDTH + 1];   // top<<8 | bottom
        Fixed       height;                     // Height of the floor
        uint32_t    PicHandle;                  // Texture handle
        uint32_t    PlaneLight;                 // Light override
        int32_t     minx;                       // Minimum x, max x
        int32_t     maxx;
    };

    // Describe a wall segment to be drawn
    struct viswall_t {        
        uint32_t    LeftX;          // Leftmost x screen coord
        uint32_t    RightX;         // Rightmost inclusive x coordinates
        uint32_t    FloorPic;       // Picture handle to floor shape
        uint32_t    CeilingPic;     // Picture handle to ceiling shape
        uint32_t    WallActions;    // Actions to perform for draw

        int32_t         t_topheight;        // Describe the top texture
        int32_t         t_bottomheight;
        int32_t         t_texturemid;
        const Texture*  t_texture;          // Pointer to the top texture
    
        int32_t         b_topheight;        // Describe the bottom texture
        int32_t         b_bottomheight;
        int32_t         b_texturemid;
        const Texture*  b_texture;          // Pointer to the bottom texture
    
        int32_t     floorheight;
        int32_t     floornewheight;
        int32_t     ceilingheight;
        int32_t     ceilingnewheight;

        Fixed       LeftScale;              // LeftX Scale
        Fixed       RightScale;             // RightX scale
        Fixed       SmallScale;
        Fixed       LargeScale;
        uint8_t*    TopSil;                 // YClips for the top line
        uint8_t*    BottomSil;              // YClips for the bottom line
        Fixed       ScaleStep;              // Scale step factor
        angle_t     CenterAngle;            // Center angle
        Fixed       offset;                 // Offset to the texture
        uint32_t    distance;
        uint32_t    seglightlevel;
        seg_t*      SegPtr;                 // Pointer to line segment for clipping  
    };

    // Wall 'action' flags for a vis wall
    static constexpr uint32_t AC_ADDFLOOR       = 0x1;
    static constexpr uint32_t AC_ADDCEILING     = 0x2;
    static constexpr uint32_t AC_TOPTEXTURE     = 0x4;
    static constexpr uint32_t AC_BOTTOMTEXTURE  = 0x8;
    static constexpr uint32_t AC_NEWCEILING     = 0x10;
    static constexpr uint32_t AC_NEWFLOOR       = 0x20;
    static constexpr uint32_t AC_ADDSKY         = 0x40;
    static constexpr uint32_t AC_TOPSIL         = 0x80;
    static constexpr uint32_t AC_BOTTOMSIL      = 0x100;
    static constexpr uint32_t AC_SOLIDSIL       = 0x200;

    //==================================================================================================================
    // Globals
    //==================================================================================================================
    extern viswall_t        gVisWalls[MAXWALLCMDS];         // Visible wall array
    extern viswall_t*       gLastWallCmd;                   // Pointer to free wall entry
    extern visplane_t       gVisPlanes[MAXVISPLANES];       // Visible floor array
    extern visplane_t*      gLastVisPlane;                  // Pointer to free floor entry
    extern vissprite_t      gVisSprites[MAXVISSPRITES];     // Visible sprite array
    extern vissprite_t*     gpVisSprite;                    // Pointer to free sprite entry
    extern uint8_t          gOpenings[MAXOPENINGS];
    extern uint8_t*         gLastOpening;
    extern Fixed            gViewX;                         // Camera x,y,z
    extern Fixed            gViewY;
    extern Fixed            gViewZ;
    extern angle_t          gViewAngle;                     // Camera angle
    extern Fixed            gViewCos;                       // Camera sine, cosine from angle
    extern Fixed            gViewSin;
    extern uint32_t         gExtraLight;                    // Bumped light from gun blasts
    extern angle_t          gClipAngle;                     // Leftmost clipping angle
    extern angle_t          gDoubleClipAngle;               // Doubled leftmost clipping angle
    extern uint32_t         gSpriteTotal;                   // Total number of sprites to render
    extern uint32_t*        gSortedSprites;                 // Pointer to array of words of sprites to render
    extern std::byte*       gPlaneSource;                   // Pointer to floor shape
    extern Fixed            gPlaneY;                        // Latched viewx / viewy for floor drawing
    extern Fixed            gBaseXScale;
    extern Fixed            gBaseYScale;
    extern uint32_t         gSprOpening[MAXSCREENWIDTH];    // clipped range
    extern uint32_t         gTexX;                          // Screen x coord being drawn
    extern int32_t          gTexScale;                      // True scale value 0-0x7FFF

    //==================================================================================================================
    // Functions
    //==================================================================================================================
    void bsp() noexcept;
    void WallPrep(uint32_t LeftX, uint32_t RightX, seg_t* LineSeg, angle_t LineAngle);
    void SegCommands();
    void DrawVisPlane(visplane_t* PlanePtr);
    uint32_t* SortWords(uint32_t* Before, uint32_t* After, uint32_t Total);
    void DrawVisSprite(const vissprite_t* const pVisSprite);
    void DrawWeapons();

    // TODO: tidy all this up
    void DrawSkyLine();
    void DrawWallColumn(
        const uint32_t y,
        const uint32_t Colnum,
        const uint32_t ColY,
        const uint32_t TexHeight,
        const std::byte* const Source,
        const uint32_t Run
    );

    void DrawFloorColumn(
        uint32_t ds_y,
        uint32_t ds_x1,
        uint32_t Count,
        uint32_t xfrac,
        uint32_t yfrac,
        Fixed ds_xstep,
        Fixed ds_ystep
    );

    void DrawSpriteNoClip(const vissprite_t* const pVisSprite);
    void DrawSpriteClip(const uint32_t x1, const uint32_t x2, const vissprite_t* const pVisSprite);
    void DrawSpriteCenter(uint32_t SpriteNum);
    void DrawColors();

    // Get the distance from the view x,y from a point in 2D space
    Fixed point2dToDist(const Fixed px, const Fixed py) noexcept;
}
