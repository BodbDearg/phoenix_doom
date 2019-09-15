#include "Data.h"

ammotype_e gWeaponAmmos[NUMWEAPONS] = {
    am_noammo,      // Fists
    am_clip,        // Pistol
    am_shell,       // Shotgun
    am_clip,        // Chain gun
    am_misl,        // Rocket launcher
    am_cell,        // Plasma rifle
    am_cell,        // BFG 9000
    am_noammo       // Chainsaw
};

uint32_t gMaxAmmo[NUMAMMO] = {
    200,
    50,
    300,
    50
};

uint32_t                gTotalGameTicks;
uint32_t                gMaxLevel;
skill_e                 gStartSkill;
uint32_t                gStartMap;
const CelImageArray*    gpBigNumFont;
uint32_t                gTotalKillsInLevel;
uint32_t                gItemsFoundInLevel;
uint32_t                gSecretsFoundInLevel;
player_t                gPlayer;
gameaction_e            gGameAction;
skill_e                 gGameSkill;
uint32_t                gGameMap;
uint32_t                gNextMap;
uint32_t                gScreenSize;
bool                    gAlwaysRun;
bool                    gDoWipe;
uint32_t                gValidCount;
PerfCounterMode         gPerfCounterMode = PerfCounterMode::NONE;
uint64_t                gPerfCounterAverageUSec;
uint64_t                gPerfCounterRunningTotal;
uint64_t                gPerfCounterTicksDone;
