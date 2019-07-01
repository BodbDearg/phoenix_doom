#include "Data.h"

ammotype_e WeaponAmmos[NUMWEAPONS] = {  
    am_noammo,      // Fists
    am_clip,        // Pistol
    am_shell,       // Shotgun
    am_clip,        // Chain gun
    am_misl,        // Rocket launcher
    am_cell,        // Plasma rifle
    am_cell,        // BFG 9000
    am_noammo       // Chainsaw
};

uint32_t maxammo[NUMAMMO] = { 
    200,
    50,
    300,
    50
};

uint32_t        PadAttack = PadA;
uint32_t        PadUse = PadB;
uint32_t        PadSpeed = PadC;
uint32_t        ControlType;
uint32_t        TotalGameTicks;
uint32_t        gElapsedTime;
uint32_t        MaxLevel;
uint32_t*       DemoDataPtr;
uint32_t*       DemoBuffer;
uint32_t        JoyPadButtons;
uint32_t        PrevJoyPadButtons;
uint32_t        NewJoyPadButtons;
skill_e         StartSkill;
uint32_t        StartMap;
const void*     BigNumFont;
uint32_t        TotalKillsInLevel;
uint32_t        ItemsFoundInLevel;
uint32_t        SecretsFoundInLevel;
uint32_t        tx_texturelight;
Fixed           lightsub;
Fixed           lightcoef;
Fixed           lightmin;
Fixed           lightmax;
player_t        players;
gameaction_e    gameaction;
skill_e         gameskill;
uint32_t        gamemap;
uint32_t        nextmap;
uint32_t        ScreenSize;
bool            DemoRecording;
bool            DemoPlayback;
bool            DoWipe;
uint32_t        validcount;
