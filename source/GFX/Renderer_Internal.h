#pragma once

//----------------------------------------------------------------------------------------------------------------------
// Data structures and globals internal to the renderer.
// Nothing here is used by outside code.
//----------------------------------------------------------------------------------------------------------------------
#include "Base/Angle.h"
#include "Game/DoomDefines.h"
#include "Renderer.h"
#include <cstddef>
#include <vector>

struct mobj_t;
struct seg_t;
struct SpriteFrameAngle;
struct Texture;

namespace Renderer {
    //==================================================================================================================
    // Constants
    //==================================================================================================================

    // Limits
    static constexpr uint32_t   MAXVISSPRITES           = 128;                      // Maximum number of visible sprites
    static constexpr uint32_t   MAXVISPLANES            = 64;                       // Maximum number of visible floor and ceiling textures
    static constexpr uint32_t   MAXWALLCMDS             = 128;                      // Maximum number of visible walls
    static constexpr uint32_t   MAXOPENINGS             = MAXSCREENWIDTH * 64;      // Space for sil tables
    static constexpr Fixed      MINZ                    = FRACUNIT * 4;             // Closest z allowed (clip sprites closer than this etc.)
    static constexpr uint32_t   FIELDOFVIEW             = 2048;                     // 90 degrees of view
    static constexpr uint32_t   ANGLETOSKYSHIFT         = 22;                       // sky map is 256*128*4 maps
    static constexpr float      MAX_LIGHT_VALUE         = 255.0f;
    static constexpr float      MIN_LIGHT_MUL           = 0.020f;                   // Minimum allowed multiplier due to light

    // Rendering constants
    static constexpr uint32_t   HEIGHTBITS              = 6;                        // Number of bits for texture height
    static constexpr uint32_t   FIXEDTOHEIGHT           = FRACBITS - HEIGHTBITS;    // Number of unused bits from fixed to SCALEBITS
    static constexpr uint32_t   SCALEBITS               = 9;                        // Number of bits for texture scale
    static constexpr uint32_t   FIXEDTOSCALE            = FRACBITS - SCALEBITS;     // Number of unused bits from fixed to HEIGHTBITS
    static constexpr float      MIN_RENDER_SCALE        = 1 / 256.0f;
    static constexpr float      MAX_RENDER_SCALE        = 64.0f;

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

    // Describes the vertical bounds of a column on the screen
    struct ColumnYBounds {
        uint16_t topY;
        uint16_t bottomY;

        inline constexpr bool operator == (const ColumnYBounds& other) const noexcept {
            return (topY == other.topY && bottomY == other.bottomY);
        }

        inline constexpr bool isUndefined() const noexcept {
            return (*this == UNDEFINED());
        }

        inline constexpr bool isDefined() const noexcept {
            return (!isUndefined());
        }

        static inline constexpr ColumnYBounds UNDEFINED() noexcept {
            return ColumnYBounds{ UINT16_MAX, 0 };
        }
    };

    // Describes lighting params for an input light level
    struct LightParams {
        float   lightMin;       // Minimum light value allowed
        float   lightMax;       // Maximum light value allowed
        float   lightSub;       // Subtract this as part of the light diminishing calculations
        float   lightCoef;      // Controls the falloff for light diminishing

        // For these light parameters, gives a light multiplier that can be applied to textures etc.
        // after doing light diminishing effects. Requires the distance of the object from the camera.
        float getLightMulForDist(const float dist) const noexcept;
    };

    // Describes a floor area to be drawn
    struct visplane_t {
        ColumnYBounds   cols[MAXSCREENWIDTH + 1];
        float           height;                     // Height of the floor
        uint32_t        picHandle;                  // Texture handle
        uint32_t        planeLight;                 // Light override
        int32_t         minX;                       // Minimum x, max x
        int32_t         maxX;
    };

    //------------------------------------------------------------------------------------------------------------------
    // Describes a sparse 4x4 matrix designed for 3D projections.
    //
    //  (1) 'Z' in this matrix is DEPTH (unlike the Doom coord sys). 'Y' is also height here.
    //  (2) The matrix (M) is designed transform a single row/vector (V) using the following multiply order: VxM.
    //  (3) Except where otherwise stated, omitted elements are '0' and are not considered in calculations.
    //  (4) The omitted 'r3c3' element is '1' and IS considered in calculations.
    // 
    // Note that the calculations below are Loosely based on the GLM function 'perspectiveRH_ZO' but adjusted so that
    // Z positive goes INTO the screen and the Y coordinate in normalized device coords (NDC) increases as we go
    // DOWN the screen, similar to how most screen buffers are stored.
    //
    // Based on input parameters for view width & height, znear & far, and field of view respectively:
    //      w, h, zn, zf, fov
    //
    // The matrix is computed via the following method (note r3c3 is always omitted from storage):
    //      f = tan(fov * 0.5)
    //      a = w / h
    //
    //      r0c0 = 1 / (f * a)
    //      r1c1 = -1 / f
    //      r2c2 = -zf / (zn - zf)
    //      r2c3 = -(zn * zf) / (zf - zn)
    //      r3c3 = 1
    //
    // Effectively it looks like this when visualized (with implicit entries added):
    //      r0c0,   0,      0,      0,
    //      0,      r1c1,   0,      0,
    //      0,      0,      r2c2,   r2c3,
    //      0,      0,      0,      1
    //------------------------------------------------------------------------------------------------------------------
    struct ProjectionMatrix {
        float r0c0;
        float r1c1;
        float r2c2;
        float r2c3;
    };

    //------------------------------------------------------------------------------------------------------------------
    // Describes 3D coordiantes for line segment to be drawn.
    //
    // Note:
    //  (1) Coords are such that xy is the ground plane and z represents height. (Doom coord system)
    //  (2) Before perspective projection x and y give the 2d position on the map for the line segment.
    //  (3) After persective projection y is a 'depth' value into the screen.
    //  (4) 1/w represents the value scale at a point.
    //      It can also be used to help perform perspective correct interpolation.
    //------------------------------------------------------------------------------------------------------------------
    struct DrawSegCoords {
        float p1x;              // 1st wall point: xyw and 1/w (scale)
        float p1y;
        float p1w;
        float p1w_inv;
        
        float p2x;              // 2nd wall point: xyw and 1/w (scale)
        float p2y;
        float p2w;
        float p2w_inv;
        
        float p1tz;             // 1st wall point: top and bottom z
        float p1bz;
        float p1tz_back;        // 1st wall point: top and bottom z for the back/joining sector
        float p1bz_back;

        float p2tz;             // 2nd wall point: top and bottom z
        float p2bz;
        float p2tz_back;        // 2nd wall point: top and bottom z for the back/joining sector
        float p2bz_back;
    };

    //------------------------------------------------------------------------------------------------------------------
    // Describes a wall segment to be drawn
    //------------------------------------------------------------------------------------------------------------------
    struct DrawSeg {
        DrawSegCoords       coords;
        float               p1TexU;             // X texture coordinate for p1 and p2
        float               p2TexU;
        float               texOffsetV;         // Vertical offset to apply to the texture when drawing
        const Texture*      texture_top;        // Top and bottom texture
        const Texture*      texture_bottom;
        const Texture*      texture_floor;      // Floor and ceiling texture. Note: draw sky if no ceiling!
        const Texture*      texture_ceil;
    };

    // Describe a wall segment to be drawn
    struct viswall_t {        
        int32_t         leftX;              // Leftmost x screen coord
        int32_t         rightX;             // Rightmost inclusive x coordinates
        uint32_t        FloorPic;           // Picture handle to floor shape
        uint32_t        CeilingPic;         // Picture handle to ceiling shape
        uint32_t        WallActions;        // Actions to perform for draw

        float           t_topheight;        // Describe the top texture
        float           t_bottomheight;
        float           t_texturemid;
        const Texture*  t_texture;          // Pointer to the top texture
    
        float           b_topheight;        // Describe the bottom texture
        float           b_bottomheight;
        float           b_texturemid;
        const Texture*  b_texture;          // Pointer to the bottom texture
        
        float           floorheight;
        float           floornewheight;
        float           ceilingheight;
        float           ceilingnewheight;

        float           LeftScale;              // LeftX Scale
        float           RightScale;             // RightX scale
        Fixed           SmallScale;
        Fixed           LargeScale;
        uint8_t*        TopSil;                 // YClips for the top line
        uint8_t*        BottomSil;              // YClips for the bottom line
        float           ScaleStep;              // Scale step factor
        angle_t         CenterAngle;            // Center angle
        float           offset;                 // Offset to the texture
        float           distance;
        uint32_t        seglightlevel;
        const seg_t*    SegPtr;                 // Pointer to line segment for clipping  
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
    // Globals shared throughout the renderer - defined in Renderer.cpp
    //==================================================================================================================
    extern viswall_t                gVisWalls[MAXWALLCMDS];         // Visible wall array
    extern viswall_t*               gpEndVisWall;                   // End of the used viswalls range (also tells number of viswalls)
    extern visplane_t               gVisPlanes[MAXVISPLANES];       // Visible floor array
    extern visplane_t*              gpEndVisPlane;                  // End of the used visplanes range (also tells number of visplanes)
    extern vissprite_t              gVisSprites[MAXVISSPRITES];     // Visible sprite array
    extern vissprite_t*             gpEndVisSprite;                 // End of the used vissprites range (also tells the number of sprites)
    extern uint8_t                  gOpenings[MAXOPENINGS];
    extern uint8_t*                 gpEndOpening;
    extern std::vector<DrawSeg>     gDrawSegs;
    extern Fixed                    gViewXFrac;                     // Camera x,y,z
    extern Fixed                    gViewYFrac;
    extern Fixed                    gViewZFrac;
    extern float                    gViewX;                         // Camera x,y,z
    extern float                    gViewY;
    extern float                    gViewZ;
    extern angle_t                  gViewAngleBAM;                  // Camera angle
    extern float                    gViewAngle;                     // Camera angle
    extern Fixed                    gViewCosFrac;                   // Camera sine, cosine from angle
    extern Fixed                    gViewSinFrac;
    extern float                    gViewCos;                       // Camera sine, cosine from angle
    extern float                    gViewSin;
    extern ProjectionMatrix         gProjMatrix;                    // 3D projection matrix
    extern uint32_t                 gExtraLight;                    // Bumped light from gun blasts
    extern angle_t                  gClipAngleBAM;                  // Leftmost clipping angle
    extern angle_t                  gDoubleClipAngleBAM;            // Doubled leftmost clipping angle
    extern uint32_t                 gSprOpening[MAXSCREENWIDTH];    // clipped range

    //==================================================================================================================
    // Functions
    //==================================================================================================================

    void doBspTraversal() noexcept;
    void addSegToFrame(const seg_t& seg) noexcept;
    void wallPrep(const int32_t leftX, const int32_t rightX, const seg_t& lineSeg, const angle_t lineAngle) noexcept;
    void drawAllLineSegs() noexcept;
    void drawAllVisPlanes() noexcept;
    void drawAllMapObjectSprites() noexcept;    
    void drawWeapons() noexcept;
    void doPostFx() noexcept;

    // TODO: FIXME - REMOVE FROM HERE
    void DrawSpriteCenter(uint32_t SpriteNum);

    // Get light parameters for a floor or wall at the given light level
    LightParams getLightParams(const uint32_t sectorLightLevel) noexcept;
}
