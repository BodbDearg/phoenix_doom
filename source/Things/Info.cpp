#include "Info.h"

#include "Audio/Sounds.h"
#include "Enemy.h"
#include "Game/DoomRez.h"
#include "MapObj.h"
#include "PlayerSprites.h"

//----------------------------------------------------------------------------------------------------------------------
// This data determines each state of the sprites in DOOM.
//
// The first value determines the shape GROUP that will be used to draw the sprite.
// The first also will determine which shape in the GROUP that will be drawn.
// The upper bit MAY be or'd with FF_FULLBRIGHT) so that the shape will be at maximum brightness at all times.
//
// The second value is the timer in ticks before the next state is executed.
// If it is -1 then it will NEVER change states. If 0 then I go the next state NOW and I don't care about
// values #1 and 2. I assume a time base of 60hz.
//
// The third value is a procedure pointer to execute when the state is entered.
// 
// The fourth value is the index to the next state when time runs out.
//
//----------------------------------------------------------------------------------------------------------------------

// Macro for making a 'sprite' number from a sprite list and sub frame
#define Spr(s, f) ((s << FF_SPRITESHIFT) | f)

// Some useful constants
static constexpr PspActionFunc      NO_PSP_ACTION   = nullptr;
static constexpr MObjActionFunc     NO_MOBJ_ACTION  = nullptr;
static constexpr uint32_t           TIME_INF        = UINT32_MAX;
static constexpr state_t*           NULL_STATE      = nullptr;

state_t gStates[NUMSTATES] = {
    state_t{Spr(rSPR_HEALTHBONUS,       0),                 TIME_INF,  NO_PSP_ACTION,   NULL_STATE                },  // S_NULL - Dummy entry
    state_t{Spr(rSPR_BIGFISTS,          0),                 0*4,       A_Light0,        NULL_STATE                },  // S_LIGHTDONE
    state_t{Spr(rSPR_BIGFISTS,          0),                 1*4,       A_WeaponReady,   &gStates[S_PUNCH]         },  // S_PUNCH
    state_t{Spr(rSPR_BIGFISTS,          0),                 1*4,       A_Lower,         &gStates[S_PUNCHDOWN]     },  // S_PUNCHDOWN
    state_t{Spr(rSPR_BIGFISTS,          0),                 1*4,       A_Raise,         &gStates[S_PUNCHUP]       },  // S_PUNCHUP
    state_t{Spr(rSPR_BIGFISTS,          1),                 2*4,       NO_PSP_ACTION,   &gStates[S_PUNCH2]        },  // S_PUNCH1
    state_t{Spr(rSPR_BIGFISTS,          2),                 2*4,       A_Punch,         &gStates[S_PUNCH3]        },  // S_PUNCH2
    state_t{Spr(rSPR_BIGFISTS,          3),                 2*4,       NO_PSP_ACTION,   &gStates[S_PUNCH4]        },  // S_PUNCH3
    state_t{Spr(rSPR_BIGFISTS,          2),                 2*4,       NO_PSP_ACTION,   &gStates[S_PUNCH5]        },  // S_PUNCH4
    state_t{Spr(rSPR_BIGFISTS,          1),                 2*4,       A_ReFire,        &gStates[S_PUNCH]         },  // S_PUNCH5
    state_t{Spr(rSPR_BIGPISTOL,         0),                 1*4,       A_WeaponReady,   &gStates[S_PISTOL]        },  // S_PISTOL
    state_t{Spr(rSPR_BIGPISTOL,         0),                 1*4,       A_Lower,         &gStates[S_PISTOLDOWN]    },  // S_PISTOLDOWN
    state_t{Spr(rSPR_BIGPISTOL,         0),                 1*4,       A_Raise,         &gStates[S_PISTOLUP]      },  // S_PISTOLUP
    state_t{Spr(rSPR_BIGPISTOL,         0),                 2*4,       NO_PSP_ACTION,   &gStates[S_PISTOL2]       },  // S_PISTOL1
    state_t{Spr(rSPR_BIGPISTOL,         1),                 3*4,       A_FirePistol,    &gStates[S_PISTOL3]       },  // S_PISTOL2
    state_t{Spr(rSPR_BIGPISTOL,         2),                 3*4,       NO_PSP_ACTION,   &gStates[S_PISTOL4]       },  // S_PISTOL3
    state_t{Spr(rSPR_BIGPISTOL,         1),                 2*4,       A_ReFire,        &gStates[S_PISTOL]        },  // S_PISTOL4
    state_t{Spr(rSPR_BIGPISTOL,         3|FF_FULLBRIGHT),   3*4,       A_Light1,        &gStates[S_LIGHTDONE]     },  // S_PISTOLFLASH
    state_t{Spr(rSPR_BIGSHOTGUN,        0),                 1*4,       A_WeaponReady,   &gStates[S_SGUN]          },  // S_SGUN
    state_t{Spr(rSPR_BIGSHOTGUN,        0),                 1*4,       A_Lower,         &gStates[S_SGUNDOWN]      },  // S_SGUNDOWN
    state_t{Spr(rSPR_BIGSHOTGUN,        0),                 1*4,       A_Raise,         &gStates[S_SGUNUP]        },  // S_SGUNUP
    state_t{Spr(rSPR_BIGSHOTGUN,        0),                 2*4,       NO_PSP_ACTION,   &gStates[S_SGUN2]         },  // S_SGUN1
    state_t{Spr(rSPR_BIGSHOTGUN,        0),                 2*4,       A_FireShotgun,   &gStates[S_SGUN3]         },  // S_SGUN2
    state_t{Spr(rSPR_BIGSHOTGUN,        1),                 3*4,       NO_PSP_ACTION,   &gStates[S_SGUN4]         },  // S_SGUN3
    state_t{Spr(rSPR_BIGSHOTGUN,        2),                 2*4,       NO_PSP_ACTION,   &gStates[S_SGUN5]         },  // S_SGUN4
    state_t{Spr(rSPR_BIGSHOTGUN,        3),                 2*4,       NO_PSP_ACTION,   &gStates[S_SGUN6]         },  // S_SGUN5
    state_t{Spr(rSPR_BIGSHOTGUN,        2),                 2*4,       NO_PSP_ACTION,   &gStates[S_SGUN7]         },  // S_SGUN6
    state_t{Spr(rSPR_BIGSHOTGUN,        1),                 2*4,       NO_PSP_ACTION,   &gStates[S_SGUN8]         },  // S_SGUN7
    state_t{Spr(rSPR_BIGSHOTGUN,        0),                 2*4,       NO_PSP_ACTION,   &gStates[S_SGUN9]         },  // S_SGUN8
    state_t{Spr(rSPR_BIGSHOTGUN,        0),                 3*4,       A_ReFire,        &gStates[S_SGUN]          },  // S_SGUN9
    state_t{Spr(rSPR_BIGSHOTGUN,        4|FF_FULLBRIGHT),   2*4,       A_Light1,        &gStates[S_SGUNFLASH2]    },  // S_SGUNFLASH1
    state_t{Spr(rSPR_BIGSHOTGUN,        5|FF_FULLBRIGHT),   1*4,       A_Light2,        &gStates[S_LIGHTDONE]     },  // S_SGUNFLASH2
    state_t{Spr(rSPR_BIGCHAINGUN,       0),                 1*4,       A_WeaponReady,   &gStates[S_CHAIN]         },  // S_CHAIN
    state_t{Spr(rSPR_BIGCHAINGUN,       0),                 1*4,       A_Lower,         &gStates[S_CHAINDOWN]     },  // S_CHAINDOWN
    state_t{Spr(rSPR_BIGCHAINGUN,       0),                 1*4,       A_Raise,         &gStates[S_CHAINUP]       },  // S_CHAINUP
    state_t{Spr(rSPR_BIGCHAINGUN,       0),                 2*4,       A_FireCGun,      &gStates[S_CHAIN2]        },  // S_CHAIN1
    state_t{Spr(rSPR_BIGCHAINGUN,       1),                 2*4,       A_FireCGun,      &gStates[S_CHAIN3]        },  // S_CHAIN2
    state_t{Spr(rSPR_BIGCHAINGUN,       1),                 0*4,       A_ReFire,        &gStates[S_CHAIN]         },  // S_CHAIN3
    state_t{Spr(rSPR_BIGCHAINGUN,       2|FF_FULLBRIGHT),   3*4,       A_Light1,        &gStates[S_LIGHTDONE]     },  // S_CHAINFLASH1
    state_t{Spr(rSPR_BIGCHAINGUN,       3|FF_FULLBRIGHT),   2*4,       A_Light2,        &gStates[S_LIGHTDONE]     },  // S_CHAINFLASH2
    state_t{Spr(rSPR_BIGROCKET,         0),                 1*4,       A_WeaponReady,   &gStates[S_MISSILE]       },  // S_MISSILE
    state_t{Spr(rSPR_BIGROCKET,         0),                 1*4,       A_Lower,         &gStates[S_MISSILEDOWN]   },  // S_MISSILEDOWN
    state_t{Spr(rSPR_BIGROCKET,         0),                 1*4,       A_Raise,         &gStates[S_MISSILEUP]     },  // S_MISSILEUP
    state_t{Spr(rSPR_BIGROCKET,         1),                 4*4,       A_GunFlash,      &gStates[S_MISSILE2]      },  // S_MISSILE1
    state_t{Spr(rSPR_BIGROCKET,         1),                 6*4,       A_FireMissile,   &gStates[S_MISSILE3]      },  // S_MISSILE2
    state_t{Spr(rSPR_BIGROCKET,         1),                 0*4,       A_ReFire,        &gStates[S_MISSILE]       },  // S_MISSILE3
    state_t{Spr(rSPR_BIGROCKET,         2|FF_FULLBRIGHT),   2*4,       A_Light1,        &gStates[S_MISSILEFLASH2] },  // S_MISSILEFLASH1
    state_t{Spr(rSPR_BIGROCKET,         3|FF_FULLBRIGHT),   2*4,       NO_PSP_ACTION,   &gStates[S_MISSILEFLASH3] },  // S_MISSILEFLASH2
    state_t{Spr(rSPR_BIGROCKET,         4|FF_FULLBRIGHT),   2*4,       A_Light2,        &gStates[S_MISSILEFLASH4] },  // S_MISSILEFLASH3
    state_t{Spr(rSPR_BIGROCKET,         5|FF_FULLBRIGHT),   2*4,       A_Light2,        &gStates[S_LIGHTDONE]     },  // S_MISSILEFLASH4
    state_t{Spr(rSPR_BIGCHAINSAW,       2),                 2*4,       A_WeaponReady,   &gStates[S_SAWB]          },  // S_SAW
    state_t{Spr(rSPR_BIGCHAINSAW,       3),                 2*4,       A_WeaponReady,   &gStates[S_SAW]           },  // S_SAWB
    state_t{Spr(rSPR_BIGCHAINSAW,       2),                 1*4,       A_Lower,         &gStates[S_SAWDOWN]       },  // S_SAWDOWN
    state_t{Spr(rSPR_BIGCHAINSAW,       2),                 1*4,       A_Raise,         &gStates[S_SAWUP]         },  // S_SAWUP
    state_t{Spr(rSPR_BIGCHAINSAW,       0),                 2*4,       A_Saw,           &gStates[S_SAW2]          },  // S_SAW1
    state_t{Spr(rSPR_BIGCHAINSAW,       1),                 2*4,       A_Saw,           &gStates[S_SAW3]          },  // S_SAW2
    state_t{Spr(rSPR_BIGCHAINSAW,       1),                 0*4,       A_ReFire,        &gStates[S_SAW]           },  // S_SAW3
    state_t{Spr(rSPR_BIGPLASMA,         0),                 1*4,       A_WeaponReady,   &gStates[S_PLASMA]        },  // S_PLASMA
    state_t{Spr(rSPR_BIGPLASMA,         0),                 1*4,       A_Lower,         &gStates[S_PLASMADOWN]    },  // S_PLASMADOWN
    state_t{Spr(rSPR_BIGPLASMA,         0),                 1*4,       A_Raise,         &gStates[S_PLASMAUP]      },  // S_PLASMAUP
    state_t{Spr(rSPR_BIGPLASMA,         0),                 2*4,       A_FirePlasma,    &gStates[S_PLASMA2]       },  // S_PLASMA1
    state_t{Spr(rSPR_BIGPLASMA,         1),                 10*4,      A_ReFire,        &gStates[S_PLASMA]        },  // S_PLASMA2
    state_t{Spr(rSPR_BIGPLASMA,         2|FF_FULLBRIGHT),   2*4,       A_Light1,        &gStates[S_LIGHTDONE]     },  // S_PLASMAFLASH1
    state_t{Spr(rSPR_BIGPLASMA,         3|FF_FULLBRIGHT),   2*4,       A_Light1,        &gStates[S_LIGHTDONE]     },  // S_PLASMAFLASH2
    state_t{Spr(rSPR_BIGBFG,            0),                 1*4,       A_WeaponReady,   &gStates[S_BFG]           },  // S_BFG
    state_t{Spr(rSPR_BIGBFG,            0),                 1*4,       A_Lower,         &gStates[S_BFGDOWN]       },  // S_BFGDOWN
    state_t{Spr(rSPR_BIGBFG,            0),                 1*4,       A_Raise,         &gStates[S_BFGUP]         },  // S_BFGUP
    state_t{Spr(rSPR_BIGBFG,            0),                 10*4,      A_BFGsound,      &gStates[S_BFG2]          },  // S_BFG1
    state_t{Spr(rSPR_BIGBFG,            1),                 5*4,       A_GunFlash,      &gStates[S_BFG3]          },  // S_BFG2
    state_t{Spr(rSPR_BIGBFG,            1),                 5*4,       A_FireBFG,       &gStates[S_BFG4]          },  // S_BFG3
    state_t{Spr(rSPR_BIGBFG,            1),                 10*4,      A_ReFire,        &gStates[S_BFG]           },  // S_BFG4
    state_t{Spr(rSPR_BIGBFG,            2|FF_FULLBRIGHT),   5*4,       A_Light1,        &gStates[S_BFGFLASH2]     },  // S_BFGFLASH1
    state_t{Spr(rSPR_BIGBFG,            3|FF_FULLBRIGHT),   3*4,       A_Light2,        &gStates[S_LIGHTDONE]     },  // S_BFGFLASH2
    state_t{Spr(rSPR_BLUD,              2),                 4*4,       NO_MOBJ_ACTION,  &gStates[S_BLOOD2]        },  // S_BLOOD1
    state_t{Spr(rSPR_BLUD,              1),                 4*4,       NO_MOBJ_ACTION,  &gStates[S_BLOOD3]        },  // S_BLOOD2
    state_t{Spr(rSPR_BLUD,              0),                 4*4,       NO_MOBJ_ACTION,  NULL_STATE                },  // S_BLOOD3
    state_t{Spr(rSPR_PUFF,              0|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_PUFF2]         },  // S_PUFF1
    state_t{Spr(rSPR_PUFF,              1),                 2*4,       NO_MOBJ_ACTION,  &gStates[S_PUFF3]         },  // S_PUFF2
    state_t{Spr(rSPR_PUFF,              2),                 2*4,       NO_MOBJ_ACTION,  &gStates[S_PUFF4]         },  // S_PUFF3
    state_t{Spr(rSPR_PUFF,              3),                 2*4,       NO_MOBJ_ACTION,  NULL_STATE                },  // S_PUFF4
    state_t{Spr(rSPR_IMP,               21|FF_FULLBRIGHT),  2*4,       NO_MOBJ_ACTION,  &gStates[S_TBALL2]        },  // S_TBALL1
    state_t{Spr(rSPR_IMP,               22|FF_FULLBRIGHT),  2*4,       NO_MOBJ_ACTION,  &gStates[S_TBALL1]        },  // S_TBALL2
    state_t{Spr(rSPR_IMP,               23|FF_FULLBRIGHT),  3*4,       NO_MOBJ_ACTION,  &gStates[S_TBALLX2]       },  // S_TBALLX1
    state_t{Spr(rSPR_IMP,               24|FF_FULLBRIGHT),  3*4,       NO_MOBJ_ACTION,  &gStates[S_TBALLX3]       },  // S_TBALLX2
    state_t{Spr(rSPR_IMP,               25|FF_FULLBRIGHT),  3*4,       NO_MOBJ_ACTION,  NULL_STATE                },  // S_TBALLX3
    state_t{Spr(rSPR_CACOBOLT,          0|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_RBALL2]        },  // S_RBALL1
    state_t{Spr(rSPR_CACOBOLT,          1|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_RBALL1]        },  // S_RBALL2
    state_t{Spr(rSPR_CACOBOLT,          2|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_RBALLX2]       },  // S_RBALLX1
    state_t{Spr(rSPR_CACOBOLT,          3|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_RBALLX3]       },  // S_RBALLX2
    state_t{Spr(rSPR_CACOBOLT,          4|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  NULL_STATE                },  // S_RBALLX3
    state_t{Spr(rSPR_BARONBLT,          0|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_BRBALL2]       },  // S_BRBALL1
    state_t{Spr(rSPR_BARONBLT,          1|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_BRBALL1]       },  // S_BRBALL2
    state_t{Spr(rSPR_BARONBLT,          2|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_BRBALLX2]      },  // S_BRBALLX1
    state_t{Spr(rSPR_BARONBLT,          3|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_BRBALLX3]      },  // S_BRBALLX2
    state_t{Spr(rSPR_BARONBLT,          4|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  NULL_STATE                },  // S_BRBALLX3
    state_t{Spr(rSPR_PLSS,              0|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PLASBALL2]     },  // S_PLASBALL
    state_t{Spr(rSPR_PLSS,              1|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PLASBALL]      },  // S_PLASBALL2
    state_t{Spr(rSPR_PLSE,              0|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_PLASEXP2]      },  // S_PLASEXP
    state_t{Spr(rSPR_PLSE,              1|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_PLASEXP3]      },  // S_PLASEXP2
    state_t{Spr(rSPR_PLSE,              2|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_PLASEXP4]      },  // S_PLASEXP3
    state_t{Spr(rSPR_PLSE,              3|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_PLASEXP5]      },  // S_PLASEXP4
    state_t{Spr(rSPR_PLSE,              4|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  NULL_STATE                },  // S_PLASEXP5
    state_t{Spr(rSPR_MISL,              0|FF_FULLBRIGHT),   1*4,       NO_MOBJ_ACTION,  &gStates[S_ROCKET]        },  // S_ROCKET
    state_t{Spr(rSPR_BFS1,              0|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_BFGSHOT2]      },  // S_BFGSHOT
    state_t{Spr(rSPR_BFS1,              1|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_BFGSHOT]       },  // S_BFGSHOT2
    state_t{Spr(rSPR_BFE1,              0|FF_FULLBRIGHT),   4*4,       NO_MOBJ_ACTION,  &gStates[S_BFGLAND2]      },  // S_BFGLAND
    state_t{Spr(rSPR_BFE1,              1|FF_FULLBRIGHT),   4*4,       NO_MOBJ_ACTION,  &gStates[S_BFGLAND3]      },  // S_BFGLAND2
    state_t{Spr(rSPR_BFE1,              2|FF_FULLBRIGHT),   4*4,       A_BFGSpray,      &gStates[S_BFGLAND4]      },  // S_BFGLAND3
    state_t{Spr(rSPR_BFE1,              3|FF_FULLBRIGHT),   4*4,       NO_MOBJ_ACTION,  &gStates[S_BFGLAND5]      },  // S_BFGLAND4
    state_t{Spr(rSPR_BFE1,              4|FF_FULLBRIGHT),   4*4,       NO_MOBJ_ACTION,  &gStates[S_BFGLAND6]      },  // S_BFGLAND5
    state_t{Spr(rSPR_BFE1,              5|FF_FULLBRIGHT),   4*4,       NO_MOBJ_ACTION,  NULL_STATE                },  // S_BFGLAND6
    state_t{Spr(rSPR_BFE2,              0|FF_FULLBRIGHT),   4*4,       NO_MOBJ_ACTION,  &gStates[S_BFGEXP2]       },  // S_BFGEXP
    state_t{Spr(rSPR_BFE2,              1|FF_FULLBRIGHT),   4*4,       NO_MOBJ_ACTION,  &gStates[S_BFGEXP3]       },  // S_BFGEXP2
    state_t{Spr(rSPR_BFE2,              2|FF_FULLBRIGHT),   4*4,       NO_MOBJ_ACTION,  &gStates[S_BFGEXP4]       },  // S_BFGEXP3
    state_t{Spr(rSPR_BFE2,              3|FF_FULLBRIGHT),   4*4,       NO_MOBJ_ACTION,  NULL_STATE                },  // S_BFGEXP4
    state_t{Spr(rSPR_MISL,              1|FF_FULLBRIGHT),   4*4,       A_Explode,       &gStates[S_EXPLODE2]      },  // S_EXPLODE1
    state_t{Spr(rSPR_MISL,              2|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_EXPLODE3]      },  // S_EXPLODE2
    state_t{Spr(rSPR_MISL,              3|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  NULL_STATE                },  // S_EXPLODE3
    state_t{Spr(rSPR_TELEFOG,           0|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_TFOG01]        },  // S_TFOG
    state_t{Spr(rSPR_TELEFOG,           1|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_TFOG02]        },  // S_TFOG01
    state_t{Spr(rSPR_TELEFOG,           0|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_TFOG2]         },  // S_TFOG02
    state_t{Spr(rSPR_TELEFOG,           1|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_TFOG3]         },  // S_TFOG2
    state_t{Spr(rSPR_TELEFOG,           2|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_TFOG4]         },  // S_TFOG3
    state_t{Spr(rSPR_TELEFOG,           3|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_TFOG5]         },  // S_TFOG4
    state_t{Spr(rSPR_TELEFOG,           4|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_TFOG6]         },  // S_TFOG5
    state_t{Spr(rSPR_TELEFOG,           5|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_TFOG7]         },  // S_TFOG6
    state_t{Spr(rSPR_TELEFOG,           6|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_TFOG8]         },  // S_TFOG7
    state_t{Spr(rSPR_TELEFOG,           7|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_TFOG9]         },  // S_TFOG8
    state_t{Spr(rSPR_TELEFOG,           8|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_TFOG10]        },  // S_TFOG9
    state_t{Spr(rSPR_TELEFOG,           9|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  NULL_STATE                },  // S_TFOG10
    state_t{Spr(rSPR_IFOG,              0|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_IFOG01]        },  // S_IFOG
    state_t{Spr(rSPR_IFOG,              1|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_IFOG02]        },  // S_IFOG01
    state_t{Spr(rSPR_IFOG,              0|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_IFOG2]         },  // S_IFOG02
    state_t{Spr(rSPR_IFOG,              1|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_IFOG3]         },  // S_IFOG2
    state_t{Spr(rSPR_IFOG,              2|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_IFOG4]         },  // S_IFOG3
    state_t{Spr(rSPR_IFOG,              3|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_IFOG5]         },  // S_IFOG4
    state_t{Spr(rSPR_IFOG,              4|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  NULL_STATE                },  // S_IFOG5
    state_t{Spr(rSPR_OURHERO,           0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_PLAY
    state_t{Spr(rSPR_OURHERO,           0),                 2*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_RUN2]     },  // S_PLAY_RUN1
    state_t{Spr(rSPR_OURHERO,           1),                 2*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_RUN3]     },  // S_PLAY_RUN2
    state_t{Spr(rSPR_OURHERO,           2),                 2*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_RUN4]     },  // S_PLAY_RUN3
    state_t{Spr(rSPR_OURHERO,           3),                 2*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_RUN1]     },  // S_PLAY_RUN4
    state_t{Spr(rSPR_OURHERO,           4),                 2*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_ATK2]     },  // S_PLAY_ATK1
    state_t{Spr(rSPR_OURHERO,           5|FF_FULLBRIGHT),   4*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY]          },  // S_PLAY_ATK2
    state_t{Spr(rSPR_OURHERO,           6),                 2*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_PAIN2]    },  // S_PLAY_PAIN
    state_t{Spr(rSPR_OURHERO,           6),                 2*4,       A_Pain,          &gStates[S_PLAY]          },  // S_PLAY_PAIN2
    state_t{Spr(rSPR_OURHERO,           7),                 5*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_DIE2]     },  // S_PLAY_DIE1
    state_t{Spr(rSPR_OURHERO,           8),                 5*4,       A_Scream,        &gStates[S_PLAY_DIE3]     },  // S_PLAY_DIE2
    state_t{Spr(rSPR_OURHERO,           9),                 5*4,       A_Fall,          &gStates[S_PLAY_DIE4]     },  // S_PLAY_DIE3
    state_t{Spr(rSPR_OURHERO,           10),                5*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_DIE5]     },  // S_PLAY_DIE4
    state_t{Spr(rSPR_OURHERO,           11),                5*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_DIE6]     },  // S_PLAY_DIE5
    state_t{Spr(rSPR_OURHERO,           12),                5*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_DIE7]     },  // S_PLAY_DIE6
    state_t{Spr(rSPR_OURHEROBDY,        0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_PLAY_DIE7
    state_t{Spr(rSPR_OURHERO,           13),                2*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_XDIE2]    },  // S_PLAY_XDIE1
    state_t{Spr(rSPR_OURHERO,           14),                2*4,       A_XScream,       &gStates[S_PLAY_XDIE3]    },  // S_PLAY_XDIE2
    state_t{Spr(rSPR_OURHERO,           15),                2*4,       A_Fall,          &gStates[S_PLAY_XDIE4]    },  // S_PLAY_XDIE3
    state_t{Spr(rSPR_OURHERO,           16),                2*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_XDIE5]    },  // S_PLAY_XDIE4
    state_t{Spr(rSPR_OURHERO,           17),                2*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_XDIE6]    },  // S_PLAY_XDIE5
    state_t{Spr(rSPR_OURHERO,           18),                2*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_XDIE7]    },  // S_PLAY_XDIE6
    state_t{Spr(rSPR_OURHERO,           19),                2*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_XDIE8]    },  // S_PLAY_XDIE7
    state_t{Spr(rSPR_OURHERO,           20),                2*4,       NO_MOBJ_ACTION,  &gStates[S_PLAY_XDIE9]    },  // S_PLAY_XDIE8
    state_t{Spr(rSPR_OURHEROBDY,        1),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_PLAY_XDIE9
    state_t{Spr(rSPR_ZOMBIE,            0),                 5*4,       A_Look,          &gStates[S_POSS_STND2]    },  // S_POSS_STND
    state_t{Spr(rSPR_ZOMBIE,            1),                 5*4,       A_Look,          &gStates[S_POSS_STND]     },  // S_POSS_STND2
    state_t{Spr(rSPR_ZOMBIE,            0),                 2*4,       A_Chase,         &gStates[S_POSS_RUN2]     },  // S_POSS_RUN1
    state_t{Spr(rSPR_ZOMBIE,            0),                 2*4,       A_Chase,         &gStates[S_POSS_RUN3]     },  // S_POSS_RUN2
    state_t{Spr(rSPR_ZOMBIE,            1),                 2*4,       A_Chase,         &gStates[S_POSS_RUN4]     },  // S_POSS_RUN3
    state_t{Spr(rSPR_ZOMBIE,            1),                 2*4,       A_Chase,         &gStates[S_POSS_RUN5]     },  // S_POSS_RUN4
    state_t{Spr(rSPR_ZOMBIE,            2),                 2*4,       A_Chase,         &gStates[S_POSS_RUN6]     },  // S_POSS_RUN5
    state_t{Spr(rSPR_ZOMBIE,            2),                 2*4,       A_Chase,         &gStates[S_POSS_RUN7]     },  // S_POSS_RUN6
    state_t{Spr(rSPR_ZOMBIE,            3),                 2*4,       A_Chase,         &gStates[S_POSS_RUN8]     },  // S_POSS_RUN7
    state_t{Spr(rSPR_ZOMBIE,            3),                 2*4,       A_Chase,         &gStates[S_POSS_RUN1]     },  // S_POSS_RUN8
    state_t{Spr(rSPR_ZOMBIE,            4),                 5*4,       A_FaceTarget,    &gStates[S_POSS_ATK2]     },  // S_POSS_ATK1
    state_t{Spr(rSPR_ZOMBIE,            5),                 4*4,       A_PosAttack,     &gStates[S_POSS_ATK3]     },  // S_POSS_ATK2
    state_t{Spr(rSPR_ZOMBIE,            4),                 4*4,       NO_MOBJ_ACTION,  &gStates[S_POSS_RUN1]     },  // S_POSS_ATK3
    state_t{Spr(rSPR_ZOMBIE,            6),                 1*4,       NO_MOBJ_ACTION,  &gStates[S_POSS_PAIN2]    },  // S_POSS_PAIN
    state_t{Spr(rSPR_ZOMBIE,            6),                 2*4,       A_Pain,          &gStates[S_POSS_RUN1]     },  // S_POSS_PAIN2
    state_t{Spr(rSPR_ZOMBIE,            7),                 2*4,       NO_MOBJ_ACTION,  &gStates[S_POSS_DIE2]     },  // S_POSS_DIE1
    state_t{Spr(rSPR_ZOMBIE,            8),                 3*4,       A_Scream,        &gStates[S_POSS_DIE3]     },  // S_POSS_DIE2
    state_t{Spr(rSPR_ZOMBIE,            9),                 2*4,       A_Fall,          &gStates[S_POSS_DIE4]     },  // S_POSS_DIE3
    state_t{Spr(rSPR_ZOMBIE,            10),                3*4,       NO_MOBJ_ACTION,  &gStates[S_POSS_DIE5]     },  // S_POSS_DIE4
    state_t{Spr(rSPR_ZOMBIEBODY,        0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_POSS_DIE5
    state_t{Spr(rSPR_ZOMBIE,            11),                2*4,       NO_MOBJ_ACTION,  &gStates[S_POSS_XDIE2]    },  // S_POSS_XDIE1
    state_t{Spr(rSPR_ZOMBIE,            12),                3*4,       A_XScream,       &gStates[S_POSS_XDIE3]    },  // S_POSS_XDIE2
    state_t{Spr(rSPR_ZOMBIE,            13),                2*4,       A_Fall,          &gStates[S_POSS_XDIE4]    },  // S_POSS_XDIE3
    state_t{Spr(rSPR_ZOMBIE,            14),                3*4,       NO_MOBJ_ACTION,  &gStates[S_POSS_XDIE5]    },  // S_POSS_XDIE4
    state_t{Spr(rSPR_ZOMBIE,            15),                2*4,       NO_MOBJ_ACTION,  &gStates[S_POSS_XDIE6]    },  // S_POSS_XDIE5
    state_t{Spr(rSPR_ZOMBIE,            16),                3*4,       NO_MOBJ_ACTION,  &gStates[S_POSS_XDIE7]    },  // S_POSS_XDIE6
    state_t{Spr(rSPR_ZOMBIE,            17),                2*4,       NO_MOBJ_ACTION,  &gStates[S_POSS_XDIE8]    },  // S_POSS_XDIE7
    state_t{Spr(rSPR_ZOMBIE,            18),                3*4,       NO_MOBJ_ACTION,  &gStates[S_POSS_XDIE9]    },  // S_POSS_XDIE8
    state_t{Spr(rSPR_ZOMBIEBODY,        1),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_POSS_XDIE9
    state_t{Spr(rSPR_SHOTGUY,           0),                 5*4,       A_Look,          &gStates[S_SPOS_STND2]    },  // S_SPOS_STND
    state_t{Spr(rSPR_SHOTGUY,           1),                 5*4,       A_Look,          &gStates[S_SPOS_STND]     },  // S_SPOS_STND2
    state_t{Spr(rSPR_SHOTGUY,           0),                 1*4,       A_Chase,         &gStates[S_SPOS_RUN2]     },  // S_SPOS_RUN1
    state_t{Spr(rSPR_SHOTGUY,           0),                 2*4,       A_Chase,         &gStates[S_SPOS_RUN3]     },  // S_SPOS_RUN2
    state_t{Spr(rSPR_SHOTGUY,           1),                 1*4,       A_Chase,         &gStates[S_SPOS_RUN4]     },  // S_SPOS_RUN3
    state_t{Spr(rSPR_SHOTGUY,           1),                 2*4,       A_Chase,         &gStates[S_SPOS_RUN5]     },  // S_SPOS_RUN4
    state_t{Spr(rSPR_SHOTGUY,           2),                 1*4,       A_Chase,         &gStates[S_SPOS_RUN6]     },  // S_SPOS_RUN5
    state_t{Spr(rSPR_SHOTGUY,           2),                 2*4,       A_Chase,         &gStates[S_SPOS_RUN7]     },  // S_SPOS_RUN6
    state_t{Spr(rSPR_SHOTGUY,           3),                 1*4,       A_Chase,         &gStates[S_SPOS_RUN8]     },  // S_SPOS_RUN7
    state_t{Spr(rSPR_SHOTGUY,           3),                 2*4,       A_Chase,         &gStates[S_SPOS_RUN1]     },  // S_SPOS_RUN8
    state_t{Spr(rSPR_SHOTGUY,           4),                 5*4,       A_FaceTarget,    &gStates[S_SPOS_ATK2]     },  // S_SPOS_ATK1
    state_t{Spr(rSPR_SHOTGUY,           5|FF_FULLBRIGHT),   5*4,       A_SPosAttack,    &gStates[S_SPOS_ATK3]     },  // S_SPOS_ATK2
    state_t{Spr(rSPR_SHOTGUY,           4),                 5*4,       NO_MOBJ_ACTION,  &gStates[S_SPOS_RUN1]     },  // S_SPOS_ATK3
    state_t{Spr(rSPR_SHOTGUY,           6),                 1*4,       NO_MOBJ_ACTION,  &gStates[S_SPOS_PAIN2]    },  // S_SPOS_PAIN
    state_t{Spr(rSPR_SHOTGUY,           6),                 2*4,       A_Pain,          &gStates[S_SPOS_RUN1]     },  // S_SPOS_PAIN2
    state_t{Spr(rSPR_SHOTGUY,           7),                 2*4,       NO_MOBJ_ACTION,  &gStates[S_SPOS_DIE2]     },  // S_SPOS_DIE1
    state_t{Spr(rSPR_SHOTGUY,           8),                 3*4,       A_Scream,        &gStates[S_SPOS_DIE3]     },  // S_SPOS_DIE2
    state_t{Spr(rSPR_SHOTGUY,           9),                 2*4,       A_Fall,          &gStates[S_SPOS_DIE4]     },  // S_SPOS_DIE3
    state_t{Spr(rSPR_SHOTGUY,           10),                3*4,       NO_MOBJ_ACTION,  &gStates[S_SPOS_DIE5]     },  // S_SPOS_DIE4
    state_t{Spr(rSPR_SHOTGUY,           11),                TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_SPOS_DIE5
    state_t{Spr(rSPR_SHOTGUY,           12),                2*4,       NO_MOBJ_ACTION,  &gStates[S_SPOS_XDIE2]    },  // S_SPOS_XDIE1
    state_t{Spr(rSPR_SHOTGUY,           13),                3*4,       A_XScream,       &gStates[S_SPOS_XDIE3]    },  // S_SPOS_XDIE2
    state_t{Spr(rSPR_SHOTGUY,           14),                2*4,       A_Fall,          &gStates[S_SPOS_XDIE4]    },  // S_SPOS_XDIE3
    state_t{Spr(rSPR_SHOTGUY,           15),                3*4,       NO_MOBJ_ACTION,  &gStates[S_SPOS_XDIE5]    },  // S_SPOS_XDIE4
    state_t{Spr(rSPR_SHOTGUY,           16),                2*4,       NO_MOBJ_ACTION,  &gStates[S_SPOS_XDIE6]    },  // S_SPOS_XDIE5
    state_t{Spr(rSPR_SHOTGUY,           17),                3*4,       NO_MOBJ_ACTION,  &gStates[S_SPOS_XDIE7]    },  // S_SPOS_XDIE6
    state_t{Spr(rSPR_SHOTGUY,           18),                2*4,       NO_MOBJ_ACTION,  &gStates[S_SPOS_XDIE8]    },  // S_SPOS_XDIE7
    state_t{Spr(rSPR_SHOTGUY,           19),                3*4,       NO_MOBJ_ACTION,  &gStates[S_SPOS_XDIE9]    },  // S_SPOS_XDIE8
    state_t{Spr(rSPR_SHOTGUY,           20),                TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_SPOS_XDIE9
    state_t{Spr(rSPR_IMP,               0),                 5*4,       A_Look,          &gStates[S_TROO_STND2]    },  // S_TROO_STND
    state_t{Spr(rSPR_IMP,               1),                 5*4,       A_Look,          &gStates[S_TROO_STND]     },  // S_TROO_STND2
    state_t{Spr(rSPR_IMP,               0),                 1*4,       A_Chase,         &gStates[S_TROO_RUN2]     },  // S_TROO_RUN1
    state_t{Spr(rSPR_IMP,               0),                 2*4,       A_Chase,         &gStates[S_TROO_RUN3]     },  // S_TROO_RUN2
    state_t{Spr(rSPR_IMP,               1),                 1*4,       A_Chase,         &gStates[S_TROO_RUN4]     },  // S_TROO_RUN3
    state_t{Spr(rSPR_IMP,               1),                 2*4,       A_Chase,         &gStates[S_TROO_RUN5]     },  // S_TROO_RUN4
    state_t{Spr(rSPR_IMP,               2),                 1*4,       A_Chase,         &gStates[S_TROO_RUN6]     },  // S_TROO_RUN5
    state_t{Spr(rSPR_IMP,               2),                 2*4,       A_Chase,         &gStates[S_TROO_RUN7]     },  // S_TROO_RUN6
    state_t{Spr(rSPR_IMP,               3),                 1*4,       A_Chase,         &gStates[S_TROO_RUN8]     },  // S_TROO_RUN7
    state_t{Spr(rSPR_IMP,               3),                 2*4,       A_Chase,         &gStates[S_TROO_RUN1]     },  // S_TROO_RUN8
    state_t{Spr(rSPR_IMP,               4),                 4*4,       A_FaceTarget,    &gStates[S_TROO_ATK2]     },  // S_TROO_ATK1
    state_t{Spr(rSPR_IMP,               5),                 4*4,       A_FaceTarget,    &gStates[S_TROO_ATK3]     },  // S_TROO_ATK2
    state_t{Spr(rSPR_IMP,               6),                 3*4,       A_TroopAttack,   &gStates[S_TROO_RUN1]     },  // S_TROO_ATK3
    state_t{Spr(rSPR_IMP,               7),                 1*4,       NO_MOBJ_ACTION,  &gStates[S_TROO_PAIN2]    },  // S_TROO_PAIN
    state_t{Spr(rSPR_IMP,               7),                 1*4,       A_Pain,          &gStates[S_TROO_RUN1]     },  // S_TROO_PAIN2
    state_t{Spr(rSPR_IMP,               8),                 4*4,       NO_MOBJ_ACTION,  &gStates[S_TROO_DIE2]     },  // S_TROO_DIE1
    state_t{Spr(rSPR_IMP,               9),                 4*4,       A_Scream,        &gStates[S_TROO_DIE3]     },  // S_TROO_DIE2
    state_t{Spr(rSPR_IMP,               10),                3*4,       NO_MOBJ_ACTION,  &gStates[S_TROO_DIE4]     },  // S_TROO_DIE3
    state_t{Spr(rSPR_IMP,               11),                3*4,       A_Fall,          &gStates[S_TROO_DIE5]     },  // S_TROO_DIE4
    state_t{Spr(rSPR_IMP,               12),                TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_TROO_DIE5
    state_t{Spr(rSPR_IMP,               13),                2*4,       NO_MOBJ_ACTION,  &gStates[S_TROO_XDIE2]    },  // S_TROO_XDIE1
    state_t{Spr(rSPR_IMP,               14),                3*4,       A_XScream,       &gStates[S_TROO_XDIE3]    },  // S_TROO_XDIE2
    state_t{Spr(rSPR_IMP,               15),                2*4,       NO_MOBJ_ACTION,  &gStates[S_TROO_XDIE4]    },  // S_TROO_XDIE3
    state_t{Spr(rSPR_IMP,               16),                3*4,       A_Fall,          &gStates[S_TROO_XDIE5]    },  // S_TROO_XDIE4
    state_t{Spr(rSPR_IMP,               17),                2*4,       NO_MOBJ_ACTION,  &gStates[S_TROO_XDIE6]    },  // S_TROO_XDIE5
    state_t{Spr(rSPR_IMP,               18),                3*4,       NO_MOBJ_ACTION,  &gStates[S_TROO_XDIE7]    },  // S_TROO_XDIE6
    state_t{Spr(rSPR_IMP,               19),                2*4,       NO_MOBJ_ACTION,  &gStates[S_TROO_XDIE8]    },  // S_TROO_XDIE7
    state_t{Spr(rSPR_IMP,               20),                TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_TROO_XDIE8
    state_t{Spr(rSPR_DEMON,             0),                 5*4,       A_Look,          &gStates[S_SARG_STND2]    },  // S_SARG_STND
    state_t{Spr(rSPR_DEMON,             1),                 5*4,       A_Look,          &gStates[S_SARG_STND]     },  // S_SARG_STND2
    state_t{Spr(rSPR_DEMON,             0),                 1*4,       A_Chase,         &gStates[S_SARG_RUN2]     },  // S_SARG_RUN1
    state_t{Spr(rSPR_DEMON,             0),                 1*4,       A_Chase,         &gStates[S_SARG_RUN3]     },  // S_SARG_RUN2
    state_t{Spr(rSPR_DEMON,             1),                 1*4,       A_Chase,         &gStates[S_SARG_RUN4]     },  // S_SARG_RUN3
    state_t{Spr(rSPR_DEMON,             1),                 1*4,       A_Chase,         &gStates[S_SARG_RUN5]     },  // S_SARG_RUN4
    state_t{Spr(rSPR_DEMON,             2),                 1*4,       A_Chase,         &gStates[S_SARG_RUN6]     },  // S_SARG_RUN5
    state_t{Spr(rSPR_DEMON,             2),                 1*4,       A_Chase,         &gStates[S_SARG_RUN7]     },  // S_SARG_RUN6
    state_t{Spr(rSPR_DEMON,             3),                 1*4,       A_Chase,         &gStates[S_SARG_RUN8]     },  // S_SARG_RUN7
    state_t{Spr(rSPR_DEMON,             3),                 1*4,       A_Chase,         &gStates[S_SARG_RUN1]     },  // S_SARG_RUN8
    state_t{Spr(rSPR_DEMON,             4),                 4*4,       A_FaceTarget,    &gStates[S_SARG_ATK2]     },  // S_SARG_ATK1
    state_t{Spr(rSPR_DEMON,             5),                 4*4,       A_FaceTarget,    &gStates[S_SARG_ATK3]     },  // S_SARG_ATK2
    state_t{Spr(rSPR_DEMON,             6),                 4*4,       A_SargAttack,    &gStates[S_SARG_RUN1]     },  // S_SARG_ATK3
    state_t{Spr(rSPR_DEMON,             7),                 1*4,       NO_MOBJ_ACTION,  &gStates[S_SARG_PAIN2]    },  // S_SARG_PAIN
    state_t{Spr(rSPR_DEMON,             7),                 1*4,       A_Pain,          &gStates[S_SARG_RUN1]     },  // S_SARG_PAIN2
    state_t{Spr(rSPR_DEMON,             8),                 4*4,       NO_MOBJ_ACTION,  &gStates[S_SARG_DIE2]     },  // S_SARG_DIE1
    state_t{Spr(rSPR_DEMON,             9),                 4*4,       A_Scream,        &gStates[S_SARG_DIE3]     },  // S_SARG_DIE2
    state_t{Spr(rSPR_DEMON,             10),                2*4,       NO_MOBJ_ACTION,  &gStates[S_SARG_DIE4]     },  // S_SARG_DIE3
    state_t{Spr(rSPR_DEMON,             11),                2*4,       A_Fall,          &gStates[S_SARG_DIE5]     },  // S_SARG_DIE4
    state_t{Spr(rSPR_DEMON,             12),                2*4,       NO_MOBJ_ACTION,  &gStates[S_SARG_DIE6]     },  // S_SARG_DIE5
    state_t{Spr(rSPR_DEMON,             13),                TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_SARG_DIE6
    state_t{Spr(rSPR_CACODEMON,         0),                 5*4,       A_Look,          &gStates[S_HEAD_STND]     },  // S_HEAD_STND
    state_t{Spr(rSPR_CACODEMON,         0),                 1*4,       A_Chase,         &gStates[S_HEAD_RUN1]     },  // S_HEAD_RUN1
    state_t{Spr(rSPR_CACODEMON,         1),                 2*4,       A_FaceTarget,    &gStates[S_HEAD_ATK2]     },  // S_HEAD_ATK1
    state_t{Spr(rSPR_CACODEMON,         2),                 2*4,       A_FaceTarget,    &gStates[S_HEAD_ATK3]     },  // S_HEAD_ATK2
    state_t{Spr(rSPR_CACODEMON,         3|FF_FULLBRIGHT),   3*4,       A_HeadAttack,    &gStates[S_HEAD_RUN1]     },  // S_HEAD_ATK3
    state_t{Spr(rSPR_CACODEMON,         4),                 1*4,       NO_MOBJ_ACTION,  &gStates[S_HEAD_PAIN2]    },  // S_HEAD_PAIN
    state_t{Spr(rSPR_CACODEMON,         4),                 2*4,       A_Pain,          &gStates[S_HEAD_PAIN3]    },  // S_HEAD_PAIN2
    state_t{Spr(rSPR_CACODEMON,         5),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_HEAD_RUN1]     },  // S_HEAD_PAIN3
    state_t{Spr(rSPR_CACODIE,           0),                 4*4,       NO_MOBJ_ACTION,  &gStates[S_HEAD_DIE2]     },  // S_HEAD_DIE1
    state_t{Spr(rSPR_CACODIE,           1),                 4*4,       A_Scream,        &gStates[S_HEAD_DIE3]     },  // S_HEAD_DIE2
    state_t{Spr(rSPR_CACODIE,           2),                 4*4,       NO_MOBJ_ACTION,  &gStates[S_HEAD_DIE4]     },  // S_HEAD_DIE3
    state_t{Spr(rSPR_CACODIE,           3),                 4*4,       NO_MOBJ_ACTION,  &gStates[S_HEAD_DIE5]     },  // S_HEAD_DIE4
    state_t{Spr(rSPR_CACODIE,           4),                 4*4,       A_Fall,          &gStates[S_HEAD_DIE6]     },  // S_HEAD_DIE5
    state_t{Spr(rSPR_CACOBDY,           0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_HEAD_DIE6
    state_t{Spr(rSPR_BARON,             0),                 5*4,       A_Look,          &gStates[S_BOSS_STND2]    },  // S_BOSS_STND
    state_t{Spr(rSPR_BARON,             1),                 5*4,       A_Look,          &gStates[S_BOSS_STND]     },  // S_BOSS_STND2
    state_t{Spr(rSPR_BARON,             0),                 1*4,       A_Chase,         &gStates[S_BOSS_RUN2]     },  // S_BOSS_RUN1
    state_t{Spr(rSPR_BARON,             0),                 2*4,       A_Chase,         &gStates[S_BOSS_RUN3]     },  // S_BOSS_RUN2
    state_t{Spr(rSPR_BARON,             1),                 1*4,       A_Chase,         &gStates[S_BOSS_RUN4]     },  // S_BOSS_RUN3
    state_t{Spr(rSPR_BARON,             1),                 2*4,       A_Chase,         &gStates[S_BOSS_RUN5]     },  // S_BOSS_RUN4
    state_t{Spr(rSPR_BARON,             2),                 1*4,       A_Chase,         &gStates[S_BOSS_RUN6]     },  // S_BOSS_RUN5
    state_t{Spr(rSPR_BARON,             2),                 2*4,       A_Chase,         &gStates[S_BOSS_RUN7]     },  // S_BOSS_RUN6
    state_t{Spr(rSPR_BARON,             3),                 1*4,       A_Chase,         &gStates[S_BOSS_RUN8]     },  // S_BOSS_RUN7
    state_t{Spr(rSPR_BARON,             3),                 2*4,       A_Chase,         &gStates[S_BOSS_RUN1]     },  // S_BOSS_RUN8
    state_t{Spr(rSPR_BARONATK,          0),                 4*4,       A_FaceTarget,    &gStates[S_BOSS_ATK2]     },  // S_BOSS_ATK1
    state_t{Spr(rSPR_BARONATK,          1),                 4*4,       A_FaceTarget,    &gStates[S_BOSS_ATK3]     },  // S_BOSS_ATK2
    state_t{Spr(rSPR_BARONATK,          2),                 4*4,       A_BruisAttack,   &gStates[S_BOSS_RUN1]     },  // S_BOSS_ATK3
    state_t{Spr(rSPR_BARONATK,          3),                 1*4,       NO_MOBJ_ACTION,  &gStates[S_BOSS_PAIN2]    },  // S_BOSS_PAIN
    state_t{Spr(rSPR_BARONATK,          3),                 1*4,       A_Pain,          &gStates[S_BOSS_RUN1]     },  // S_BOSS_PAIN2
    state_t{Spr(rSPR_BARONDIE,          0),                 4*4,       NO_MOBJ_ACTION,  &gStates[S_BOSS_DIE2]     },  // S_BOSS_DIE1
    state_t{Spr(rSPR_BARONDIE,          1),                 4*4,       A_Scream,        &gStates[S_BOSS_DIE3]     },  // S_BOSS_DIE2
    state_t{Spr(rSPR_BARONDIE,          2),                 4*4,       NO_MOBJ_ACTION,  &gStates[S_BOSS_DIE4]     },  // S_BOSS_DIE3
    state_t{Spr(rSPR_BARONDIE,          3),                 4*4,       A_Fall,          &gStates[S_BOSS_DIE5]     },  // S_BOSS_DIE4
    state_t{Spr(rSPR_BARONDIE,          4),                 4*4,       NO_MOBJ_ACTION,  &gStates[S_BOSS_DIE6]     },  // S_BOSS_DIE5
    state_t{Spr(rSPR_BARONDIE,          5),                 4*4,       NO_MOBJ_ACTION,  &gStates[S_BOSS_DIE7]     },  // S_BOSS_DIE6
    state_t{Spr(rSPR_BARONBDY,          0),                 TIME_INF,  A_BossDeath,     NULL_STATE                },  // S_BOSS_DIE7
    state_t{Spr(rSPR_LOSTSOUL,          0|FF_FULLBRIGHT),   5*4,       A_Look,          &gStates[S_SKULL_STND2]   },  // S_SKULL_STND
    state_t{Spr(rSPR_LOSTSOUL,          1|FF_FULLBRIGHT),   5*4,       A_Look,          &gStates[S_SKULL_STND]    },  // S_SKULL_STND2
    state_t{Spr(rSPR_LOSTSOUL,          0|FF_FULLBRIGHT),   3*4,       A_Chase,         &gStates[S_SKULL_RUN2]    },  // S_SKULL_RUN1
    state_t{Spr(rSPR_LOSTSOUL,          1|FF_FULLBRIGHT),   3*4,       A_Chase,         &gStates[S_SKULL_RUN1]    },  // S_SKULL_RUN2
    state_t{Spr(rSPR_LOSTSOUL,          2|FF_FULLBRIGHT),   5*4,       A_FaceTarget,    &gStates[S_SKULL_ATK2]    },  // S_SKULL_ATK1
    state_t{Spr(rSPR_LOSTSOUL,          3|FF_FULLBRIGHT),   2*4,       A_SkullAttack,   &gStates[S_SKULL_ATK3]    },  // S_SKULL_ATK2
    state_t{Spr(rSPR_LOSTSOUL,          2|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_SKULL_ATK4]    },  // S_SKULL_ATK3
    state_t{Spr(rSPR_LOSTSOUL,          3|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_SKULL_ATK3]    },  // S_SKULL_ATK4
    state_t{Spr(rSPR_LOSTSOUL,          4|FF_FULLBRIGHT),   1*4,       NO_MOBJ_ACTION,  &gStates[S_SKULL_PAIN2]   },  // S_SKULL_PAIN
    state_t{Spr(rSPR_LOSTSOUL,          4|FF_FULLBRIGHT),   2*4,       A_Pain,          &gStates[S_SKULL_RUN1]    },  // S_SKULL_PAIN2
    state_t{Spr(rSPR_LOSTSOUL,          5|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_SKULL_DIE2]    },  // S_SKULL_DIE1
    state_t{Spr(rSPR_LOSTSOUL,          6|FF_FULLBRIGHT),   3*4,       A_Scream,        &gStates[S_SKULL_DIE3]    },  // S_SKULL_DIE2
    state_t{Spr(rSPR_LOSTSOUL,          7|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_SKULL_DIE4]    },  // S_SKULL_DIE3
    state_t{Spr(rSPR_LOSTSOUL,          8|FF_FULLBRIGHT),   3*4,       A_Fall,          &gStates[S_SKULL_DIE5]    },  // S_SKULL_DIE4
    state_t{Spr(rSPR_LOSTSOUL,          9),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_SKULL_DIE6]    },  // S_SKULL_DIE5
    state_t{Spr(rSPR_LOSTSOUL,          10),                3*4,       NO_MOBJ_ACTION,  NULL_STATE                },  // S_SKULL_DIE6
    state_t{Spr(rSPR_GREENARMOR,        0),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_ARM1A]         },  // S_ARM1
    state_t{Spr(rSPR_GREENARMOR,        1|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_ARM1]          },  // S_ARM1A
    state_t{Spr(rSPR_BLUEARMOR,         0),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_ARM2A]         },  // S_ARM2
    state_t{Spr(rSPR_BLUEARMOR,         1|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_ARM2]          },  // S_ARM2A
    state_t{Spr(rSPR_BARREL,            0),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_BAR2]          },  // S_BAR1
    state_t{Spr(rSPR_BARREL,            1),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_BAR1]          },  // S_BAR2
    state_t{Spr(rSPR_BARREL,            2|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_BEXP2]         },  // S_BEXP
    state_t{Spr(rSPR_BARREL,            3|FF_FULLBRIGHT),   3*4,       A_Scream,        &gStates[S_BEXP3]         },  // S_BEXP2
    state_t{Spr(rSPR_BARREL,            4|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_BEXP4]         },  // S_BEXP3
    state_t{Spr(rSPR_BARREL,            5|FF_FULLBRIGHT),   5*4,       A_Explode,       &gStates[S_BEXP5]         },  // S_BEXP4
    state_t{Spr(rSPR_BARREL,            6|FF_FULLBRIGHT),   5*4,       NO_MOBJ_ACTION,  NULL_STATE                },  // S_BEXP5
    state_t{Spr(rSPR_FIRECAN,           0|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_BBAR2]         },  // S_BBAR1
    state_t{Spr(rSPR_FIRECAN,           1|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_BBAR3]         },  // S_BBAR2
    state_t{Spr(rSPR_FIRECAN,           2|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_BBAR1]         },  // S_BBAR3
    state_t{Spr(rSPR_HEALTHBONUS,       0),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_BON1A]         },  // S_BON1
    state_t{Spr(rSPR_HEALTHBONUS,       1),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_BON1B]         },  // S_BON1A
    state_t{Spr(rSPR_HEALTHBONUS,       2),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_BON1C]         },  // S_BON1B
    state_t{Spr(rSPR_HEALTHBONUS,       3),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_BON1D]         },  // S_BON1C
    state_t{Spr(rSPR_HEALTHBONUS,       2),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_BON1E]         },  // S_BON1D
    state_t{Spr(rSPR_HEALTHBONUS,       1),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_BON1]          },  // S_BON1E
    state_t{Spr(rSPR_ARMORBONUS,        0),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_BON2A]         },  // S_BON2
    state_t{Spr(rSPR_ARMORBONUS,        1),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_BON2B]         },  // S_BON2A
    state_t{Spr(rSPR_ARMORBONUS,        2),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_BON2C]         },  // S_BON2B
    state_t{Spr(rSPR_ARMORBONUS,        3),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_BON2D]         },  // S_BON2C
    state_t{Spr(rSPR_ARMORBONUS,        2),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_BON2E]         },  // S_BON2D
    state_t{Spr(rSPR_ARMORBONUS,        1),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_BON2]          },  // S_BON2E
    state_t{Spr(rSPR_BLUEKEYCARD,       0),                 5*4,       NO_MOBJ_ACTION,  &gStates[S_BKEY2]         },  // S_BKEY
    state_t{Spr(rSPR_BLUEKEYCARD,       1|FF_FULLBRIGHT),   5*4,       NO_MOBJ_ACTION,  &gStates[S_BKEY]          },  // S_BKEY2
    state_t{Spr(rSPR_REDKEYCARD,        0),                 5*4,       NO_MOBJ_ACTION,  &gStates[S_RKEY2]         },  // S_RKEY
    state_t{Spr(rSPR_REDKEYCARD,        1|FF_FULLBRIGHT),   5*4,       NO_MOBJ_ACTION,  &gStates[S_RKEY]          },  // S_RKEY2
    state_t{Spr(rSPR_YELLOWKEYCARD,     0),                 5*4,       NO_MOBJ_ACTION,  &gStates[S_YKEY2]         },  // S_YKEY
    state_t{Spr(rSPR_YELLOWKEYCARD,     1|FF_FULLBRIGHT),   5*4,       NO_MOBJ_ACTION,  &gStates[S_YKEY]          },  // S_YKEY2
    state_t{Spr(rSPR_BLUESKULLKEY,      0),                 5*4,       NO_MOBJ_ACTION,  &gStates[S_BSKULL2]       },  // S_BSKULL
    state_t{Spr(rSPR_BLUESKULLKEY,      1|FF_FULLBRIGHT),   5*4,       NO_MOBJ_ACTION,  &gStates[S_BSKULL]        },  // S_BSKULL2
    state_t{Spr(rSPR_REDSKULLKEY,       0),                 5*4,       NO_MOBJ_ACTION,  &gStates[S_RSKULL2]       },  // S_RSKULL
    state_t{Spr(rSPR_REDSKULLKEY,       1|FF_FULLBRIGHT),   5*4,       NO_MOBJ_ACTION,  &gStates[S_RSKULL]        },  // S_RSKULL2
    state_t{Spr(rSPR_YELLOWSKULLKEY,    0),                 5*4,       NO_MOBJ_ACTION,  &gStates[S_YSKULL2]       },  // S_YSKULL
    state_t{Spr(rSPR_YELLOWSKULLKEY,    1|FF_FULLBRIGHT),   5*4,       NO_MOBJ_ACTION,  &gStates[S_YSKULL]        },  // S_YSKULL2
    state_t{Spr(rSPR_STIMPACK,          0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_STIM
    state_t{Spr(rSPR_MEDIKIT,           0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_MEDI
    state_t{Spr(rSPR_SOULSPHERE,        0|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_SOUL2]         },  // S_SOUL
    state_t{Spr(rSPR_SOULSPHERE,        1|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_SOUL3]         },  // S_SOUL2
    state_t{Spr(rSPR_SOULSPHERE,        2|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_SOUL4]         },  // S_SOUL3
    state_t{Spr(rSPR_SOULSPHERE,        3|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_SOUL5]         },  // S_SOUL4
    state_t{Spr(rSPR_SOULSPHERE,        2|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_SOUL6]         },  // S_SOUL5
    state_t{Spr(rSPR_SOULSPHERE,        1|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_SOUL]          },  // S_SOUL6
    state_t{Spr(rSPR_INVULNERABILITY,   0|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PINV2]         },  // S_PINV
    state_t{Spr(rSPR_INVULNERABILITY,   1|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PINV3]         },  // S_PINV2
    state_t{Spr(rSPR_INVULNERABILITY,   2|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PINV4]         },  // S_PINV3
    state_t{Spr(rSPR_INVULNERABILITY,   3|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PINV]          },  // S_PINV4
    state_t{Spr(rSPR_BERZERKER,         0|FF_FULLBRIGHT),   TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_PSTR
    state_t{Spr(rSPR_INVISIBILITY,      0|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PINS2]         },  // S_PINS
    state_t{Spr(rSPR_INVISIBILITY,      1|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PINS3]         },  // S_PINS2
    state_t{Spr(rSPR_INVISIBILITY,      2|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PINS4]         },  // S_PINS3
    state_t{Spr(rSPR_INVISIBILITY,      3|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PINS]          },  // S_PINS4
    state_t{Spr(rSPR_RADIATIONSUIT,     0|FF_FULLBRIGHT),   TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_SUIT
    state_t{Spr(rSPR_COMPUTERMAP,       0|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PMAP2]         },  // S_PMAP
    state_t{Spr(rSPR_COMPUTERMAP,       1|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PMAP3]         },  // S_PMAP2
    state_t{Spr(rSPR_COMPUTERMAP,       2|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PMAP4]         },  // S_PMAP3
    state_t{Spr(rSPR_COMPUTERMAP,       3|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PMAP5]         },  // S_PMAP4
    state_t{Spr(rSPR_COMPUTERMAP,       2|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PMAP6]         },  // S_PMAP5
    state_t{Spr(rSPR_COMPUTERMAP,       1|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PMAP]          },  // S_PMAP6
    state_t{Spr(rSPR_IRGOGGLES,         0|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_PVIS2]         },  // S_PVIS
    state_t{Spr(rSPR_IRGOGGLES,         1),                 3*4,       NO_MOBJ_ACTION,  &gStates[S_PVIS]          },  // S_PVIS2
    state_t{Spr(rSPR_CLIP,              0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_CLIP
    state_t{Spr(rSPR_BOXAMMO,           0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_AMMO
    state_t{Spr(rSPR_ROCKET,            0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_ROCK
    state_t{Spr(rSPR_BOXROCKETS,        0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_BROK
    state_t{Spr(rSPR_CELL,              0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_CELL
    state_t{Spr(rSPR_CELLPACK,          0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_CELP
    state_t{Spr(rSPR_SHELLS,            0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_SHEL
    state_t{Spr(rSPR_BOXSHELLS,         0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_SBOX
    state_t{Spr(rSPR_BACKPACK,          0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_BPAK
    state_t{Spr(rSPR_BFG9000,           0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_BFUG
    state_t{Spr(rSPR_CHAINGUN,          0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_MGUN
    state_t{Spr(rSPR_CHAINSAW,          0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_CSAW
    state_t{Spr(rSPR_ROCKETLAUNCHER,    0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_LAUN
    state_t{Spr(rSPR_PLASMARIFLE,       0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_PLAS
    state_t{Spr(rSPR_SHOTGUN,           0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_SHOT
    state_t{Spr(rSPR_LIGHTCOLUMN,       0|FF_FULLBRIGHT),   TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_COLU
    state_t{Spr(rSPR_SMALLSTAL,         0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_STALAG
    state_t{Spr(rSPR_OURHERO,           13),                TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_DEADTORSO
    state_t{Spr(rSPR_OURHERO,           18),                TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_DEADBOTTOM
    state_t{Spr(rSPR_FIVESKULLPOLE,     0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_HEADSONSTICK
    state_t{Spr(rSPR_POOLBLOOD,         0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_GIBS
    state_t{Spr(rSPR_TALLSKULLPOLE,     0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_HEADONASTICK
    state_t{Spr(rSPR_BODYPOLE,          0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_DEADSTICK
    state_t{Spr(rSPR_HANGINGRUMP,       0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_MEAT5
    state_t{Spr(rSPR_LARGESTAL,         0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_STALAGTITE
    state_t{Spr(rSPR_SHORTGREENPILLAR,  0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_SHRTGRNCOL
    state_t{Spr(rSPR_SHORTREDPILLAR,    0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_SHRTREDCOL
    state_t{Spr(rSPR_CANDLE,            0|FF_FULLBRIGHT),   TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_CANDLESTIK
    state_t{Spr(rSPR_CANDLEABRA,        0|FF_FULLBRIGHT),   TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_CANDELABRA
    state_t{Spr(rSPR_MEDDEADTREE,       0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_TORCHTREE
    state_t{Spr(rSPR_TECHPILLAR,        0),                 TIME_INF,  NO_MOBJ_ACTION,  NULL_STATE                },  // S_TECHPILLAR
    state_t{Spr(rSPR_FLAMINGSKULLS,     0|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_FLOATSKULL2]   },  // S_FLOATSKULL
    state_t{Spr(rSPR_FLAMINGSKULLS,     1|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_FLOATSKULL3]   },  // S_FLOATSKULL2
    state_t{Spr(rSPR_FLAMINGSKULLS,     2|FF_FULLBRIGHT),   3*4,       NO_MOBJ_ACTION,  &gStates[S_FLOATSKULL]    },  // S_FLOATSKULL3
    state_t{Spr(rSPR_BLUETORCH,         0|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_BTORCHSHRT2]   },  // S_BTORCHSHRT
    state_t{Spr(rSPR_BLUETORCH,         1|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_BTORCHSHRT3]   },  // S_BTORCHSHRT2
    state_t{Spr(rSPR_BLUETORCH,         2|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_BTORCHSHRT4]   },  // S_BTORCHSHRT3
    state_t{Spr(rSPR_BLUETORCH,         3|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_BTORCHSHRT]    },  // S_BTORCHSHRT4
    state_t{Spr(rSPR_GREENTORCH,        0|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_GTORCHSHRT2]   },  // S_GTORCHSHRT
    state_t{Spr(rSPR_GREENTORCH,        1|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_GTORCHSHRT3]   },  // S_GTORCHSHRT2
    state_t{Spr(rSPR_GREENTORCH,        2|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_GTORCHSHRT4]   },  // S_GTORCHSHRT3
    state_t{Spr(rSPR_GREENTORCH,        3|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_GTORCHSHRT]    },  // S_GTORCHSHRT4
    state_t{Spr(rSPR_REDTORCH,          0|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_RTORCHSHRT2]   },  // S_RTORCHSHRT
    state_t{Spr(rSPR_REDTORCH,          1|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_RTORCHSHRT3]   },  // S_RTORCHSHRT2
    state_t{Spr(rSPR_REDTORCH,          2|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_RTORCHSHRT4]   },  // S_RTORCHSHRT3
    state_t{Spr(rSPR_REDTORCH,          3|FF_FULLBRIGHT),   2*4,       NO_MOBJ_ACTION,  &gStates[S_RTORCHSHRT]    }   // S_RTORCHSHRT4
};

//--------------------------------------------------------------------------------------------------
// Object info, consists of the following fields:
// 
//      (1)  spawnstate
//      (2)  seestate
//      (3)  painstate
//      (4)  meleestate
//      (5)  missilestate
//      (6)  deathstate
//      (6)  xdeathstate
//      (7)  radius
//      (8)  height
//      (9)  doomednum
//      (10) spawnhealth
//      (11) painchance
//      (12) mass
//      (13) flags
//      (14) speed
//      (15) reactiontime
//      (16) damage
//      (17) seesound
//      (18) attacksound
//      (19) painsound
//      (20) deathsound
//      (21) activesound
//--------------------------------------------------------------------------------------------------
mobjinfo_t gMObjInfo[NUMMOBJTYPES] = {
    {   // MT_PLAYER
        &gStates[S_PLAY],                                         // Spawn
        &gStates[S_PLAY_RUN1],                                    // See
        &gStates[S_PLAY_PAIN],                                    // Pain
        NULL_STATE,                                               // Melee
        &gStates[S_PLAY_ATK1],                                    // Missile
        &gStates[S_PLAY_DIE1],                                    // Death
        &gStates[S_PLAY_XDIE1],                                   // Gross death
        16 * FRACUNIT,                                            // Radius
        56 * FRACUNIT,                                            // Height
        UINT32_MAX,                                               // Spawn number
        100,                                                      // Health
        255,                                                      // Pain chance
        100,                                                      // Mass
        MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_PICKUP|MF_NOTDMATCH,  // Flags
        0,                                                        // Speed
        0,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_plpain,
        sfx_pldeth,
        sfx_None
    },
    {   // MT_POSSESSED
        &gStates[S_POSS_STND],                                    // Spawn
        &gStates[S_POSS_RUN1],                                    // See
        &gStates[S_POSS_PAIN],                                    // Pain
        NULL_STATE,                                               // Melee
        &gStates[S_POSS_ATK1],                                    // Missile
        &gStates[S_POSS_DIE1],                                    // Death
        &gStates[S_POSS_XDIE1],                                   // Gross death
        20 * FRACUNIT,                                            // Radius
        56 * FRACUNIT,                                            // Height
        3004,                                                     // Spawn number
        20,                                                       // Health
        200,                                                      // Pain chance
        100,                                                      // Mass
        MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL,                       // Flags
        8,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_posit1,
        sfx_pistol,
        sfx_popain,
        sfx_podth1,
        sfx_posact
    },
    {   // MT_SHOTGUY
        &gStates[S_SPOS_STND],                                    // Spawn
        &gStates[S_SPOS_RUN1],                                    // See
        &gStates[S_SPOS_PAIN],                                    // Pain
        NULL_STATE,                                               // Melee
        &gStates[S_SPOS_ATK1],                                    // Missile
        &gStates[S_SPOS_DIE1],                                    // Death
        &gStates[S_SPOS_XDIE1],                                   // Gross death
        20 * FRACUNIT,                                            // Radius
        56 * FRACUNIT,                                            // Height
        9,                                                        // Spawn number
        30,                                                       // Health
        170,                                                      // Pain chance
        100,                                                      // Mass
        MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL,                       // Flags
        8,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_posit2,
        0,
        sfx_popain,
        sfx_podth2,
        sfx_posact
    },
    {   // MT_TROOP
        &gStates[S_TROO_STND],                                    // Spawn
        &gStates[S_TROO_RUN1],                                    // See
        &gStates[S_TROO_PAIN],                                    // Pain
        &gStates[S_TROO_ATK1],                                    // Melee
        &gStates[S_TROO_ATK1],                                    // Missile
        &gStates[S_TROO_DIE1],                                    // Death
        &gStates[S_TROO_XDIE1],                                   // Gross death
        20 * FRACUNIT,                                            // Radius
        56 * FRACUNIT,                                            // Height
        3001,                                                     // Spawn number
        60,                                                       // Health
        200,                                                      // Pain chance
        100,                                                      // Mass
        MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL,                       // Flags
        8,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_bgsit1,
        0,
        sfx_popain,
        sfx_bgdth1,
        sfx_bgact
    },
    {   // MT_SERGEANT
        &gStates[S_SARG_STND],                                    // Spawn
        &gStates[S_SARG_RUN1],                                    // See
        &gStates[S_SARG_PAIN],                                    // Pain
        &gStates[S_SARG_ATK1],                                    // Melee
        NULL_STATE,                                               // Missile
        &gStates[S_SARG_DIE1],                                    // Death
        NULL_STATE,                                               // Gross death
        30 * FRACUNIT,                                            // Radius
        56 * FRACUNIT,                                            // Height
        3002,                                                     // Spawn number
        150,                                                      // Health
        180,                                                      // Pain chance
        400,                                                      // Mass
        MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL,                       // Flags
        8,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_sgtsit,
        sfx_sgtatk,
        sfx_dmpain,
        sfx_sgtdth,
        sfx_dmact
    },
    {   // MT_SHADOWS
        &gStates[S_SARG_STND],                                    // Spawn
        &gStates[S_SARG_RUN1],                                    // See
        &gStates[S_SARG_PAIN],                                    // Pain
        &gStates[S_SARG_ATK1],                                    // Melee
        NULL_STATE,                                               // Missile
        &gStates[S_SARG_DIE1],                                    // Death
        NULL_STATE,                                               // Gross death
        30 * FRACUNIT,                                            // Radius
        56 * FRACUNIT,                                            // Height
        58,                                                       // Spawn number
        150,                                                      // Health
        180,                                                      // Pain chance
        400,                                                      // Mass
        MF_SOLID|MF_SHOOTABLE|MF_SHADOW|MF_COUNTKILL,             // Flags
        8,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_sgtsit,
        sfx_sgtatk,
        sfx_dmpain,
        sfx_sgtdth,
        sfx_dmact
    },
    {   // MT_HEAD
        &gStates[S_HEAD_STND],                                    // Spawn
        &gStates[S_HEAD_RUN1],                                    // See
        &gStates[S_HEAD_PAIN],                                    // Pain
        NULL_STATE,                                               // Melee
        &gStates[S_HEAD_ATK1],                                    // Missile
        &gStates[S_HEAD_DIE1],                                    // Death
        NULL_STATE,                                               // Gross death
        31 * FRACUNIT,                                            // Radius
        56 * FRACUNIT,                                            // Height
        3005,                                                     // Spawn number
        400,                                                      // Health
        128,                                                      // Pain chance
        400,                                                      // Mass
        MF_SOLID|MF_SHOOTABLE|MF_FLOAT|MF_NOGRAVITY|MF_COUNTKILL, // Flags
        4,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_cacsit,
        0,
        sfx_dmpain,
        sfx_cacdth,
        sfx_dmact
    },
    {   // MT_BRUISER
        &gStates[S_BOSS_STND],                                    // Spawn
        &gStates[S_BOSS_RUN1],                                    // See
        &gStates[S_BOSS_PAIN],                                    // Pain
        &gStates[S_BOSS_ATK1],                                    // Melee
        &gStates[S_BOSS_ATK1],                                    // Missile
        &gStates[S_BOSS_DIE1],                                    // Death
        NULL_STATE,                                               // Gross death
        24 * FRACUNIT,                                            // Radius
        64 * FRACUNIT,                                            // Height
        3003,                                                     // Spawn number
        1000,                                                     // Health
        50,                                                       // Pain chance
        1000,                                                     // Mass
        MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL,                       // Flags
        8,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_brssit,
        0,
        sfx_dmpain,
        sfx_brsdth,
        sfx_dmact
    },
    {   // MT_SKULL
        &gStates[S_SKULL_STND],                                   // Spawn
        &gStates[S_SKULL_RUN1],                                   // See
        &gStates[S_SKULL_PAIN],                                   // Pain
        NULL_STATE,                                               // Melee
        &gStates[S_SKULL_ATK1],                                   // Missile
        &gStates[S_SKULL_DIE1],                                   // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        56 * FRACUNIT,                                            // Height
        3006,                                                     // Spawn number
        60,                                                       // Health
        256,                                                      // Pain chance
        50,                                                       // Mass
        MF_SOLID|MF_SHOOTABLE|MF_FLOAT|MF_NOGRAVITY|MF_COUNTKILL, // Flags
        8,                                                        // Speed
        8,                                                        // Reaction
        3,                                                        // Damage amount
        0,
        sfx_sklatk,
        sfx_dmpain,
        sfx_firxpl,
        sfx_dmact
    },
    {   // MT_BARREL
        &gStates[S_BAR1],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        &gStates[S_BEXP],                                         // Death
        NULL_STATE,                                               // Gross death
        10 * FRACUNIT,                                            // Radius
        42 * FRACUNIT,                                            // Height
        2035,                                                     // Spawn number
        10,                                                       // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD,                         // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_barexp,
        sfx_None
    },
    {   // MT_TROOPSHOT
        &gStates[S_TBALL1],                                       // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        &gStates[S_TBALLX1],                                      // Death
        NULL_STATE,                                               // Gross death
        6 * FRACUNIT,                                             // Radius
        8 * FRACUNIT,                                             // Height
        UINT32_MAX,                                               // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY,         // Flags
        20,                                                       // Speed
        8,                                                        // Reaction
        3,                                                        // Damage amount
        sfx_firsht,
        sfx_None,
        sfx_None,
        sfx_firxpl,
        sfx_None
    },
    {   // MT_HEADSHOT
        &gStates[S_RBALL1],                                       // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        &gStates[S_RBALLX1],                                      // Death
        NULL_STATE,                                               // Gross death
        6 * FRACUNIT,                                             // Radius
        8 * FRACUNIT,                                             // Height
        UINT32_MAX,                                               // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY,         // Flags
        20,                                                       // Speed
        8,                                                        // Reaction
        5,                                                        // Damage amount
        sfx_firsht,
        sfx_None,
        sfx_None,
        sfx_firxpl,
        sfx_None
    },
    {   // MT_BRUISERSHOT
        &gStates[S_BRBALL1],                                      // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        &gStates[S_BRBALLX1],                                     // Death
        NULL_STATE,                                               // Gross death
        6 * FRACUNIT,                                             // Radius
        8 * FRACUNIT,                                             // Height
        UINT32_MAX,                                               // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY,         // Flags
        30,                                                       // Speed
        8,                                                        // Reaction
        8,                                                        // Damage amount
        sfx_firsht,
        sfx_None,
        sfx_None,
        sfx_firxpl,
        sfx_None
    },
    {   // MT_ROCKET
        &gStates[S_ROCKET],                                       // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        &gStates[S_EXPLODE1],                                     // Death
        NULL_STATE,                                               // Gross death
        11 * FRACUNIT,                                            // Radius
        8 * FRACUNIT,                                             // Height
        UINT32_MAX,                                               // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY,         // Flags
        40,                                                       // Speed
        8,                                                        // Reaction
        20,                                                       // Damage amount
        sfx_rlaunc,
        sfx_None,
        sfx_None,
        sfx_barexp,
        sfx_None
    },
    {   // MT_PLASMA
        &gStates[S_PLASBALL],                                     // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        &gStates[S_PLASEXP],                                      // Death
        NULL_STATE,                                               // Gross death
        13 * FRACUNIT,                                            // Radius
        8 * FRACUNIT,                                             // Height
        UINT32_MAX,                                               // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY,         // Flags
        50,                                                       // Speed
        8,                                                        // Reaction
        5,                                                        // Damage amount
        sfx_plasma,
        sfx_None,
        sfx_None,
        sfx_firxpl,
        sfx_None
    },
    {   // MT_BFG
        &gStates[S_BFGSHOT],                                      // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        &gStates[S_BFGLAND],                                      // Death
        NULL_STATE,                                               // Gross death
        13 * FRACUNIT,                                            // Radius
        8 * FRACUNIT,                                             // Height
        UINT32_MAX,                                               // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY,         // Flags
        30,                                                       // Speed
        8,                                                        // Reaction
        100,                                                      // Damage amount
        0,
        sfx_None,
        sfx_None,
        sfx_rxplod,
        sfx_None
    },
    {   // MT_PUFF
        &gStates[S_PUFF1],                                        // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        UINT32_MAX,                                               // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_NOBLOCKMAP|MF_NOGRAVITY,                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_BLOOD
        &gStates[S_BLOOD1],                                       // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        UINT32_MAX,                                               // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_NOBLOCKMAP,                                            // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_TFOG
        &gStates[S_TFOG],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        UINT32_MAX,                                               // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_NOBLOCKMAP|MF_NOGRAVITY,                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_IFOG
        &gStates[S_IFOG],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        UINT32_MAX,                                               // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_NOBLOCKMAP|MF_NOGRAVITY,                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_TELEPORTMAN
        NULL_STATE,                                               // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        14,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_NOBLOCKMAP|MF_NOSECTOR,                                // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_EXTRABFG
        &gStates[S_BFGEXP],                                       // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        UINT32_MAX,                                               // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_NOBLOCKMAP|MF_NOGRAVITY,                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC0
        &gStates[S_ARM1],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2018,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC1
        &gStates[S_ARM2],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2019,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC2
        &gStates[S_BON1],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2014,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL|MF_COUNTITEM,                                  // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC3
        &gStates[S_BON2],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2015,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL|MF_COUNTITEM,                                  // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC4
        &gStates[S_BKEY],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        5,                                                        // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL|MF_NOTDMATCH,                                  // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC5
        &gStates[S_RKEY],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        13,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL|MF_NOTDMATCH,                                  // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC6
        &gStates[S_YKEY],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        6,                                                        // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL|MF_NOTDMATCH,                                  // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC7
        &gStates[S_YSKULL],                                       // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        39,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL|MF_NOTDMATCH,                                  // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC8
        &gStates[S_RSKULL],                                       // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        38,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL|MF_NOTDMATCH,                                  // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC9
        &gStates[S_BSKULL],                                       // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        40,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL|MF_NOTDMATCH,                                  // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // Stimpack
        &gStates[S_STIM],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2011,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC11
        &gStates[S_MEDI],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2012,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC12
        &gStates[S_SOUL],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2013,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL|MF_COUNTITEM,                                  // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_INVULNERABILITY
        &gStates[S_PINV],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2022,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL|MF_COUNTITEM,                                  // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC13
        &gStates[S_PSTR],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2023,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL|MF_COUNTITEM,                                  // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_INVISIBILITY
        &gStates[S_PINS],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2024,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL|MF_COUNTITEM,                                  // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC14
        &gStates[S_SUIT],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2025,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC15
        &gStates[S_PMAP],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2026,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL|MF_COUNTITEM,                                  // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC16
        &gStates[S_PVIS],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2045,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL|MF_COUNTITEM,                                  // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_CLIP
        &gStates[S_CLIP],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2007,                                                     // Spawn number
        10,                                                       // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC17
        &gStates[S_AMMO],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2048,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC18
        &gStates[S_ROCK],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2010,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC19
        &gStates[S_BROK],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2046,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC20
        &gStates[S_CELL],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2047,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC21
        &gStates[S_CELP],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        17,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC22
        &gStates[S_SHEL],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2008,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC23
        &gStates[S_SBOX],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2049,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC24
        &gStates[S_BPAK],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        8,                                                        // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC25
        &gStates[S_BFUG],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2006,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_CHAINGUN
        &gStates[S_MGUN],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2002,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC26
        &gStates[S_CSAW],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2005,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC27
        &gStates[S_LAUN],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2003,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC28
        &gStates[S_PLAS],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2004,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_SHOTGUN
        &gStates[S_SHOT],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2001,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPECIAL,                                               // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC29
        &gStates[S_COLU],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        2028,                                                     // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC30
        &gStates[S_SHRTGRNCOL],                                   // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        31,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC31
        &gStates[S_SHRTREDCOL],                                   // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        33,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC32
        &gStates[S_FLOATSKULL],                                   // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        42,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC33
        &gStates[S_TORCHTREE],                                    // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        43,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC34
        &gStates[S_BTORCHSHRT],                                   // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        55,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC35
        &gStates[S_GTORCHSHRT],                                   // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        56,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC36
        &gStates[S_RTORCHSHRT],                                   // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        57,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC37
        &gStates[S_STALAGTITE],                                   // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        47,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC38
        &gStates[S_TECHPILLAR],                                   // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        48,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC39
        &gStates[S_CANDLESTIK],                                   // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        34,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        0,                                                        // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC40
        &gStates[S_CANDELABRA],                                   // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        35,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC41
        &gStates[S_MEAT5],                                        // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        52 * FRACUNIT,                                            // Height
        53,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID|MF_SPAWNCEILING|MF_NOGRAVITY,                    // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC42
        &gStates[S_MEAT5],                                        // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        52 * FRACUNIT,                                            // Height
        62,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SPAWNCEILING|MF_NOGRAVITY,                             // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_DEADCACODEMON
        &gStates[S_HEAD_DIE6],                                    // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        22,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        0,                                                        // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC44
        &gStates[S_POSS_DIE5],                                    // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        18,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        0,                                                        // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_MISC45
        &gStates[S_SARG_DIE6],                                    // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        21,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        0,                                                        // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_DEADIMP
        &gStates[S_TROO_DIE5],                                    // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        20,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        0,                                                        // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_DEADSHOTGUY
        &gStates[S_SPOS_DIE5],                                    // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        19,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        0,                                                        // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_GROSSPLAYER1
        &gStates[S_PLAY_XDIE9],                                   // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        10,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        0,                                                        // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_GROSSPLAYER2
        &gStates[S_PLAY_XDIE9],                                   // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        12,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        0,                                                        // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_FIVESKULLS
        &gStates[S_HEADSONSTICK],                                 // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        28,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_GIBS
        &gStates[S_GIBS],                                         // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        20 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        24,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        0,                                                        // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_SKULLONPOLE
        &gStates[S_HEADONASTICK],                                 // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        27,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_IMPALEDHUMAN
        &gStates[S_DEADSTICK],                                    // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        25,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    },
    {   // MT_BURNINGBARREL
        &gStates[S_BBAR1],                                        // Spawn
        NULL_STATE,                                               // See
        NULL_STATE,                                               // Pain
        NULL_STATE,                                               // Melee
        NULL_STATE,                                               // Missile
        NULL_STATE,                                               // Death
        NULL_STATE,                                               // Gross death
        16 * FRACUNIT,                                            // Radius
        16 * FRACUNIT,                                            // Height
        70,                                                       // Spawn number
        1000,                                                     // Health
        0,                                                        // Pain chance
        100,                                                      // Mass
        MF_SOLID,                                                 // Flags
        0,                                                        // Speed
        8,                                                        // Reaction
        0,                                                        // Damage amount
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None,
        sfx_None
    }
};
