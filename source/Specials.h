#pragma once

#include "Doom.h"

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

extern uint32_t NumFlatAnims;   // Number of flat anims
extern anim_t   FlatAnims[];    // Array of flat animations

void P_InitPicAnims();
side_t* getSide(sector_t* sec, uint32_t line, uint32_t side);
sector_t* getSector(sector_t* sec, uint32_t line, uint32_t side);
bool twoSided(sector_t* sec, uint32_t line);
sector_t* getNextSector(line_t* line, sector_t* sec);
Fixed P_FindLowestFloorSurrounding(sector_t* sec);
Fixed P_FindHighestFloorSurrounding(sector_t* sec);
Fixed P_FindNextHighestFloor(sector_t* sec, Fixed currentheight);
Fixed P_FindLowestCeilingSurrounding(sector_t* sec);
Fixed P_FindHighestCeilingSurrounding(sector_t* sec);
uint32_t P_FindSectorFromLineTag(line_t* line, uint32_t start);
uint32_t P_FindMinSurroundingLight(sector_t& sector, uint32_t max);
void P_CrossSpecialLine(line_t* line, mobj_t* thing);
void P_ShootSpecialLine(mobj_t* thing, line_t* line);
void PlayerInSpecialSector(player_t* player, sector_t* sector);
void P_UpdateSpecials();
void SpawnSpecials();
void PurgeLineSpecials();
