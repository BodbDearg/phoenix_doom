#pragma once

#include "Base/Fixed.h"

struct line_t;
struct mobj_t;
struct player_t;
struct sector_t;
struct side_t;

// Struct for animating textures
struct anim_t {
    uint32_t LastPicNum;    // Picture referance number
    uint32_t BasePic;       // Base texture #
    uint32_t CurrentPic;    // Current index
};

extern uint32_t gNumFlatAnims;      // Number of flat anims
extern anim_t   gFlatAnims[];       // Array of flat animations

void P_InitPicAnims() noexcept;
side_t& getSide(sector_t& sec, const uint32_t line, const uint32_t side) noexcept;
sector_t& getSector(sector_t& sec, const uint32_t line, const uint32_t side) noexcept;
bool twoSided(sector_t& sec, const uint32_t line) noexcept;
sector_t* getNextSector(line_t& line, sector_t& sec) noexcept;
Fixed P_FindLowestFloorSurrounding(sector_t& sec) noexcept;
Fixed P_FindHighestFloorSurrounding(sector_t& sec) noexcept;
Fixed P_FindNextHighestFloor(sector_t& sec, const Fixed currentheight) noexcept;
Fixed P_FindLowestCeilingSurrounding(sector_t& sec) noexcept;
Fixed P_FindHighestCeilingSurrounding(sector_t& sec) noexcept;
uint32_t P_FindSectorFromLineTag(line_t& line, const uint32_t start) noexcept;
uint32_t P_FindMinSurroundingLight(sector_t& sector, uint32_t max) noexcept;
void P_CrossSpecialLine(line_t& line, mobj_t& thing) noexcept;
void P_ShootSpecialLine(mobj_t& thing, line_t& line) noexcept;
void PlayerInSpecialSector(player_t& player, sector_t& sector) noexcept;
void P_UpdateSpecials() noexcept;
void SpawnSpecials() noexcept;
void PurgeLineSpecials() noexcept;
