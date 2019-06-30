#include "Doom.h"

extern ammotype_t       WeaponAmmos[NUMWEAPONS];    // Ammo types for all weapons
extern uint32_t         maxammo[NUMAMMO];           // Max ammo for ammo types
extern uint32_t         PadAttack;                  // Joypad bit for attack
extern uint32_t         PadUse;                     // Joypad bit for use
extern uint32_t         PadSpeed;                   // Joypad bit for high speed
extern uint32_t         ControlType;                // Determine settings for PadAttack,Use,Speed
extern uint32_t         TotalGameTicks;             // Total number of ticks since game start
extern uint32_t         gElapsedTime;               // Ticks elapsed between frames
extern uint32_t         MaxLevel;                   // Highest level selectable in menu (1-23)
extern uint32_t*        DemoDataPtr;                // Running pointer to demo data
extern uint32_t*        DemoBuffer;                 // Pointer to demo data
extern uint32_t         JoyPadButtons;              // Current joypad
extern uint32_t         PrevJoyPadButtons;          // Previous joypad
extern uint32_t         NewJoyPadButtons;           // New joypad button downs
extern skill_t          StartSkill;                 // Default skill level
extern uint32_t         StartMap;                   // Default map start
extern const void*      BigNumFont;                 // Cached pointer to the big number font (rBIGNUMB)
extern uint32_t         TotalKillsInLevel;          // Number of monsters killed
extern uint32_t         ItemsFoundInLevel;          // Number of items found
extern uint32_t         SecretsFoundInLevel;        // Number of secrets discovered
extern uint32_t         tx_texturelight;            // Light value to pass to hardware
extern Fixed            lightsub;
extern Fixed            lightcoef;
extern Fixed            lightmin;
extern Fixed            lightmax;
extern player_t         players;                    // Current player stats
extern gameaction_t     gameaction;                 // Current game state
extern skill_t          gameskill;                  // Current skill level
extern uint32_t         gamemap;                    // Current game map #
extern uint32_t         nextmap;                    // The map to go to after the stats
extern uint32_t         ScreenSize;                 // Screen size to use
extern bool             LowDetail;                  // Use low detail mode
extern bool             DemoRecording;              // True if demo is being recorded
extern bool             DemoPlayback;               // True if demo is being played
extern bool             DoWipe;                     // True if I should do the DOOM wipe
extern uint32_t         validcount;                 // Increment every time a check is made (used for sort of unique ids)