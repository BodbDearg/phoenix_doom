#include "Data.h"

#include "Burger.h"

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

uint32_t        gPadAttack = PadA;
uint32_t        gPadUse = PadB;
uint32_t        gPadSpeed = PadC;
uint32_t        gControlType;
uint32_t        gTotalGameTicks;
uint32_t        gElapsedTime;
uint32_t        gMaxLevel;
uint32_t*       gDemoDataPtr;
uint32_t*       gDemoBuffer;
uint32_t        gJoyPadButtons;
uint32_t        gPrevJoyPadButtons;
uint32_t        gNewJoyPadButtons;
skill_e         gStartSkill;
uint32_t        gStartMap;
const void*     gBigNumFont;
uint32_t        gTotalKillsInLevel;
uint32_t        gItemsFoundInLevel;
uint32_t        gSecretsFoundInLevel;
uint32_t        gTxTextureLight;
Fixed           gLightSub;
Fixed           gLightCoef;
Fixed           gLightMin;
Fixed           gLightMax;
player_t        gPlayers;
gameaction_e    gGameAction;
skill_e         gGameSkill;
uint32_t        gGameMap;
uint32_t        gNextMap;
uint32_t        gScreenSize;
bool            gDemoRecording;
bool            gDemoPlayback;
bool            gDoWipe;
uint32_t        gValidCount;