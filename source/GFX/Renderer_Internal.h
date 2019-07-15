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
    static constexpr uint32_t   MAXVISSPRITES           = 128;                      // Maximum number of visible sprites
    static constexpr uint32_t   MAXVISPLANES            = 64;                       // Maximum number of visible floor and ceiling textures
    static constexpr uint32_t   MAXWALLCMDS             = 128;                      // Maximum number of visible walls
    static constexpr uint32_t   MAXOPENINGS             = MAXSCREENWIDTH * 64;      // Space for sil tables
    static constexpr Fixed      MINZ                    = FRACUNIT * 4;             // Closest z allowed (clip sprites closer than this etc.)
    static constexpr uint32_t   FIELDOFVIEW             = 2048;                     // 90 degrees of view
    static constexpr uint32_t   ANGLETOSKYSHIFT         = 22;                       // sky map is 256*128*4 maps
    static constexpr uint8_t    MAX_WALL_LIGHT_VALUE    = 15;
    static constexpr uint8_t    MAX_FLOOR_LIGHT_VALUE   = 15;
    static constexpr uint8_t    MAX_SPRITE_LIGHT_VALUE  = 31;

    // Rendering constants
    static constexpr uint32_t   HEIGHTBITS              = 6;                        // Number of bits for texture height
    static constexpr uint32_t   FIXEDTOHEIGHT           = FRACBITS - HEIGHTBITS;    // Number of unused bits from fixed to SCALEBITS
    static constexpr uint32_t   SCALEBITS               = 9;                        // Number of bits for texture scale
    static constexpr uint32_t   FIXEDTOSCALE            = FRACBITS - SCALEBITS;     // Number of unused bits from fixed to HEIGHTBITS
    static constexpr uint32_t   LIGHTSCALESHIFT         = 3;
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

    // Describes a floor area to be drawn
    struct visplane_t {
        ColumnYBounds   cols[MAXSCREENWIDTH + 1];
        Fixed           height;                     // Height of the floor
        uint32_t        picHandle;                  // Texture handle
        uint32_t        planeLight;                 // Light override
        int32_t         minX;                       // Minimum x, max x
        int32_t         maxX;
    };

    // Describe a wall segment to be drawn
    struct viswall_t {        
        int32_t         leftX;              // Leftmost x screen coord
        int32_t         rightX;             // Rightmost inclusive x coordinates
        uint32_t        FloorPic;           // Picture handle to floor shape
        uint32_t        CeilingPic;         // Picture handle to ceiling shape
        uint32_t        WallActions;        // Actions to perform for draw

        int32_t         t_topheight;        // Describe the top texture
        int32_t         t_bottomheight;
        int32_t         t_texturemid;
        const Texture*  t_texture;          // Pointer to the top texture
    
        int32_t         b_topheight;        // Describe the bottom texture
        int32_t         b_bottomheight;
        int32_t         b_texturemid;
        const Texture*  b_texture;          // Pointer to the bottom texture
    
        int32_t         floorheight;
        int32_t         floornewheight;
        int32_t         ceilingheight;
        int32_t         ceilingnewheight;

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
    extern viswall_t        gVisWalls[MAXWALLCMDS];         // Visible wall array
    extern viswall_t*       gpEndVisWall;                   // End of the used viswalls range (also tells number of viswalls)
    extern visplane_t       gVisPlanes[MAXVISPLANES];       // Visible floor array
    extern visplane_t*      gpEndVisPlane;                  // End of the used visplanes range (also tells number of visplanes)
    extern vissprite_t      gVisSprites[MAXVISSPRITES];     // Visible sprite array
    extern vissprite_t*     gpEndVisSprite;                 // End of the used vissprites range (also tells the number of sprites)
    extern uint8_t          gOpenings[MAXOPENINGS];
    extern uint8_t*         gpEndOpening;
    extern Fixed            gViewX;                         // Camera x,y,z
    extern Fixed            gViewY;
    extern Fixed            gViewZ;
    extern angle_t          gViewAngle;                     // Camera angle
    extern Fixed            gViewCos;                       // Camera sine, cosine from angle
    extern Fixed            gViewSin;
    extern uint32_t         gExtraLight;                    // Bumped light from gun blasts
    extern angle_t          gClipAngle;                     // Leftmost clipping angle
    extern angle_t          gDoubleClipAngle;               // Doubled leftmost clipping angle
    extern uint32_t         gSprOpening[MAXSCREENWIDTH];    // clipped range

    //==================================================================================================================
    // Functions
    //==================================================================================================================

    void doBspTraversal() noexcept;
    void wallPrep(const int32_t leftX, const int32_t rightX, const seg_t& lineSeg, const angle_t lineAngle) noexcept;
    void drawAllLineSegs() noexcept;
    void drawAllVisPlanes() noexcept;
    void drawAllMapObjectSprites() noexcept;    
    void drawWeapons() noexcept;
    void doPostFx() noexcept;

    // TODO: FIXME - REMOVE FROM HERE
    void DrawSpriteCenter(uint32_t SpriteNum);

    //------------------------------------------------------------------------------------------------------------------
    // Utility: returns a fixed point multipler for the given texture light value (which is in 4.3 format)
    // This can be used to scale RGB values due to lighting.
    //------------------------------------------------------------------------------------------------------------------
    inline Fixed getLightMultiplier(const uint32_t lightValue, const uint32_t maxLightValue) noexcept {
        const Fixed maxLightValueFrac = intToFixed(maxLightValue);
        const Fixed textureLightFrac = lightValue << (FRACBITS - LIGHTSCALESHIFT);
        const Fixed lightMultiplier = fixedDiv(textureLightFrac, maxLightValueFrac);
        return (lightMultiplier > FRACUNIT) ? FRACUNIT : lightMultiplier;
    }
}
