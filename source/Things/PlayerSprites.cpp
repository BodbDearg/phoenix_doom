#include "PlayerSprites.h"

#include "Audio/Audio.h"
#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Random.h"
#include "Base/Tables.h"
#include "Game/Controls.h"
#include "Game/Data.h"
#include "Info.h"
#include "Interactions.h"
#include "Map/Map.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "MapObj.h"

static constexpr uint32_t   BFGCELLS        = 40;       // Number of energy units per blast
static constexpr int32_t    LOWERSPEED      = 18;       // Speed to lower the player's weapon
static constexpr int32_t    RAISESPEED      = 18;       // Speed to raise the player's weapon
static constexpr int32_t    WEAPONBOTTOM    = 128;      // Bottommost Y for hiding weapon
static constexpr int32_t    WEAPONTOP       = 32;       // Topmost Y for showing weapon

//----------------------------------------------------------------------------------------------------------------------
// This data will describe how each weapon is handled
//----------------------------------------------------------------------------------------------------------------------

// State to raise a weapon with
static constexpr state_t* WEAPON_UP_STATES[NUMWEAPONS] = {
    &gStates[S_PUNCHUP],        // Fists
    &gStates[S_PISTOLUP],       // Pistol
    &gStates[S_SGUNUP],         // Shotgun
    &gStates[S_CHAINUP],        // Chaingun
    &gStates[S_MISSILEUP],      // Rocket launcher
    &gStates[S_PLASMAUP],       // Plasma rifle
    &gStates[S_BFGUP],          // BFG 9000
    &gStates[S_SAWUP]           // Chainsaw
};

// State to raise a weapon with
static constexpr state_t* WEAPON_DOWN_STATES[NUMWEAPONS] = {
    &gStates[S_PUNCHDOWN],      // Fists
    &gStates[S_PISTOLDOWN],     // Pistol
    &gStates[S_SGUNDOWN],       // Shotgun
    &gStates[S_CHAINDOWN],      // Chaingun
    &gStates[S_MISSILEDOWN],    // Rocket launcher
    &gStates[S_PLASMADOWN],     // Plasma rifle
    &gStates[S_BFGDOWN],        // BFG 9000
    &gStates[S_SAWDOWN]         // Chainsaw
};

// State to raise a weapon with
static constexpr state_t* WEAPON_READY_STATES[NUMWEAPONS] = {
    &gStates[S_PUNCH],          // Fists
    &gStates[S_PISTOL],         // Pistol
    &gStates[S_SGUN],           // Shotgun
    &gStates[S_CHAIN],          // Chaingun
    &gStates[S_MISSILE],        // Rocket launcher
    &gStates[S_PLASMA],         // Plasma rifle
    &gStates[S_BFG],            // BFG 9000
    &gStates[S_SAW]             // Chainsaw
};

// State to raise a weapon with
static constexpr state_t* WEAPON_ATTACK_STATES[NUMWEAPONS] = {
    &gStates[S_PUNCH1],         // Fists
    &gStates[S_PISTOL1],        // Pistol
    &gStates[S_SGUN2],          // Shotgun
    &gStates[S_CHAIN1],         // Chaingun
    &gStates[S_MISSILE1],       // Rocket launcher
    &gStates[S_PLASMA1],        // Plasma rifle
    &gStates[S_BFG1],           // BFG 9000
    &gStates[S_SAW1]            // Chainsaw
};

// State to raise a weapon with
static constexpr state_t* WEAPON_FLASH_STATES[NUMWEAPONS] = {
    nullptr,                    // Fists
    &gStates[S_PISTOLFLASH],    // Pistol
    &gStates[S_SGUNFLASH1],     // Shotgun
    &gStates[S_CHAINFLASH1],    // Chaingun
    &gStates[S_MISSILEFLASH1],  // Rocket launcher
    &gStates[S_PLASMAFLASH1],   // Plasma rifle
    &gStates[S_BFGFLASH1],      // BFG 9000
    nullptr                     // Chainsaw
};

// Player object to track (Make global to avoid passing it)
static mobj_t* gpSoundTarget;

//----------------------------------------------------------------------------------------------------------------------
// Recursively scan all the sectors within earshot so the monsters will begin tracking the player.
// Called EXCLUSIVELY by NoiseAlert!
//----------------------------------------------------------------------------------------------------------------------
static void RecursiveSound(sector_t& sec, const uint32_t soundblocks) noexcept {
    // Wake up all monsters in this sector
    if (sec.validcount != gValidCount || sec.soundtraversed > (soundblocks + 1)) {
        sec.validcount = gValidCount;           // Mark for flood fill
        sec.soundtraversed = soundblocks+1;     // distance for sound (1 or 2)
        sec.soundtarget = gpSoundTarget;        // Set the noise maker source
        uint32_t count = sec.linecount;         // How many lines to check?
        line_t** ppChecker = sec.lines;

        do {
            line_t* pCheck = ppChecker[0];                  // Get the current line ptr
            sector_t* const pBack = pCheck->backsector;     // Get the backface sector
            if (pBack) {                                    // Ah, it's double sided!
                sector_t* pFront = pCheck->frontsector;     // Get the front side

                if ((pFront->floorheight < pBack->ceilingheight) &&   // Open door?
                    (pFront->ceilingheight > pBack->floorheight)
                ) {
                    if (pFront == &sec) {   // Use the front?
                        pFront = pBack;     // Nope, use the back
                    }

                    if ((pCheck->flags & ML_SOUNDBLOCK) != 0) {     // Is sound blocked?
                        if (!soundblocks) {                         // First level deep?
                            RecursiveSound(*pFront, 1);             // Muffled sound...
                        }
                    } else {
                        RecursiveSound(*pFront, soundblocks);   // Continue sound level 1
                    }
                }
            }
            ++ppChecker;        // Next line pointer
        } while (--count);      // All sectors checked?
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Begin scanning all the sectors within earshot so the monsters will begin tracking the player
//----------------------------------------------------------------------------------------------------------------------
static void NoiseAlert(player_t& player) noexcept {
    ASSERT(player.mo->subsector->sector);
    sector_t& sec = *player.mo->subsector->sector;

    if (player.lastsoundsector != &sec) {       // Not the same one?
        player.lastsoundsector = &sec;          // Set the new sector I made sound in
        gpSoundTarget = player.mo;              // Set the target for the monsters
        ++gValidCount;                          // Set a unique number for sector flood fill
        RecursiveSound(sec, 0);                 // Wake the monsters
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Set the player sprite's state (Gun or muzzle flash)
//----------------------------------------------------------------------------------------------------------------------
static void SetPlayerSprite(player_t& player, const psprnum_e position, const state_t* pState) noexcept {
    pspdef_t& psp = player.psprites[position];  // Get the sprite for the flash or weapon

    for (;;) {
        psp.StatePtr = pState;      // Save the state pointer
        if (!pState) {              // No state?
            psp.Time = UINT32_MAX;  // Forever (Shut down)
            break;                  // Leave now
        }
        psp.Time = pState->Time;    // Set the time delay, Could be 0...

        if (pState->pspAction) {                // Is there an action procedure?
            pState->pspAction(player,psp);      // Call the think logic (May call myself)
            pState = psp.StatePtr;              // Get the new state pointer
            if (!pState) {                      // No state?
                break;
            }
        }

        if (psp.Time > 0) {             // Time is valid?
            break;                      // Exit now
        }
        pState = pState->nextstate;     // Cycle to the next state now
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Starts bringing the pending weapon up from the bottom of the screen
//----------------------------------------------------------------------------------------------------------------------
static void BringUpWeapon(player_t& player) noexcept {
    if (player.pendingweapon == wp_nochange) {          // Is the weapon to be changed?
        player.pendingweapon = player.readyweapon;      // Set to the new weapon
    }

    if (player.pendingweapon == wp_chainsaw) {      // Starting the chainsaw?
        S_StartSound(&player.mo->x,sfx_sawup);      // Play the chainsaw sound...
    }

    player.psprites[ps_weapon].WeaponY = WEAPONBOTTOM;                              // Set the screen Y to bottom
    SetPlayerSprite(player, ps_weapon, WEAPON_UP_STATES[player.pendingweapon]);     // Set the weapon sprite
    player.pendingweapon = wp_nochange;                                             // No new weapon pending
}

//----------------------------------------------------------------------------------------------------------------------
// Returns true if there is enough ammo to shoot if not, selects the next weapon to use.
//----------------------------------------------------------------------------------------------------------------------
static bool CheckAmmo(player_t& player) noexcept {
    const ammotype_e ammoType = gWeaponAmmos[player.readyweapon];   // What ammo type to use?
    uint32_t ammoRequired = 1;                                      // Minimum ammo to fire with: assume 1 round initially

    if (player.readyweapon == wp_bfg) {     // BFG 9000?
        ammoRequired = BFGCELLS;            // Get the BFG energy requirements
    }

    // If there is enough ammo to shoot can return 'true' now
    if (ammoType == am_noammo || player.ammo[ammoType] >= ammoRequired) {
        return true;
    }

    // Out of ammo, pick a weapon to change to:
    weapontype_e nextWeapon;

    if (player.weaponowned[wp_plasma] && player.ammo[am_cell] > 0) {
        nextWeapon = wp_plasma;
    } else if (player.weaponowned[wp_chaingun] && player.ammo[am_clip] > 0) {
        nextWeapon = wp_chaingun;
    } else if (player.weaponowned[wp_shotgun] && player.ammo[am_shell] > 0) {
        nextWeapon = wp_shotgun;
    } else if (player.ammo[am_clip] > 0) {  // Any bullets?
        nextWeapon = wp_pistol;
    } else if (player.weaponowned[wp_chainsaw]) {  // Have a chainsaw?
        nextWeapon = wp_chainsaw;
    } else if (player.weaponowned[wp_missile] && player.ammo[am_misl] > 0) {
        nextWeapon = wp_missile;
    } else if (player.weaponowned[wp_bfg] && player.ammo[am_cell] >= BFGCELLS) {
        nextWeapon = wp_bfg;
    } else {
        nextWeapon = wp_fist;   // Always drop down to the fists
    }

    // Save the new weapon and lower the existing weapon
    player.pendingweapon = nextWeapon;
    SetPlayerSprite(player, ps_weapon, WEAPON_DOWN_STATES[player.readyweapon]);

    return false;   // Can't shoot!
}

//----------------------------------------------------------------------------------------------------------------------
// Fire the player's current weapon
//----------------------------------------------------------------------------------------------------------------------
static void FireWeapon(player_t& player) noexcept {
    ASSERT(player.mo);

    if (CheckAmmo(player)) {                                                                // Do I have ammo? (Change weapon if not)
        SetMObjState(*player.mo, gStates[S_PLAY_ATK1]);                                     // Player is in attack mode
        player.psprites[ps_weapon].WeaponX = 0;                                             // Reset the weapon's screen position
        player.psprites[ps_weapon].WeaponY = WEAPONTOP;
        SetPlayerSprite(player, ps_weapon, WEAPON_ATTACK_STATES[player.readyweapon]);       // Begin animation
        NoiseAlert(player);                                                                 // Alert the monsters...
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Player died, so put the weapon away for good
//----------------------------------------------------------------------------------------------------------------------
void LowerPlayerWeapon(player_t& player) noexcept {
    SetPlayerSprite(player, ps_weapon, WEAPON_DOWN_STATES[player.readyweapon]);
}

//----------------------------------------------------------------------------------------------------------------------
// The player can fire the weapon or change to another weapon at this time
//----------------------------------------------------------------------------------------------------------------------
void A_WeaponReady(player_t& player, pspdef_t& psp) noexcept {
    // Special case for chainsaw's idle sound
    if (player.readyweapon == wp_chainsaw && psp.StatePtr == &gStates[S_SAW]) {
        S_StartSound(&player.mo->x, sfx_sawidl);    // Play the idle sound
        Audio::stopAllInstancesOfSound(sfx_sawful);
    }

    // Check for change, if player is dead, put the weapon away
    if (player.pendingweapon != wp_nochange || player.health <= 0) {
        SetPlayerSprite(player, ps_weapon, WEAPON_DOWN_STATES[player.readyweapon]);
        return;
    }

    // Check for weapon firing
    if (!player.isOptionsMenuActive()) {
        if (GAME_ACTION(ATTACK)) {
            FireWeapon(player);
            return;
        }
    }

    // Bob the weapon based on movement speed
    int32_t bobValue = (player.bob >> FRACBITS);                                // Isolate the integer
    angle_t angle = (gTotalGameTicks << 6) & FINEMASK;                          // Get the angle for weapon bobbing
    psp.WeaponX = (bobValue * gFineCosine[angle]) >> FRACBITS;
    angle &= (FINEANGLES / 2) - 1;                                              // Force semi-circle
    psp.WeaponY = ((bobValue * gFineSine[angle]) >> FRACBITS) + WEAPONTOP;
}

//----------------------------------------------------------------------------------------------------------------------
// The player can refire the weapon without lowering it entirely
//----------------------------------------------------------------------------------------------------------------------
void A_ReFire(player_t& player, [[maybe_unused]] pspdef_t& psp) noexcept {
    // Check for fire (if a weaponchange is pending, let it go through instead)
    const bool bFiring = (
        GAME_ACTION(ATTACK) &&
        (!player.isOptionsMenuActive()) &&
        (player.pendingweapon == wp_nochange) &&
        (player.health > 0)
    );

    if (bFiring) {
        player.refire = true;       // Count for grimacing player face
        FireWeapon(player);         // Shoot...
    } else {
        player.refire = false;      // Reset firing
        CheckAmmo(player);          // Still have ammo?
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Handle an animation step for lowering a weapon
//----------------------------------------------------------------------------------------------------------------------
void A_Lower(player_t& player, pspdef_t& psp) noexcept {
    if (player.readyweapon == wp_chainsaw) {
        Audio::stopAllInstancesOfSound(sfx_sawful);
    }

    psp.WeaponY += LOWERSPEED;                  // Lower the Y coord
    if (player.playerstate == PST_DEAD) {       // Are you dead?
        psp.WeaponY = WEAPONBOTTOM;             // Force at the bottom
        return;                                 // Never allow it to go back up
    }

    // If the old weapon has been lowered off the screen change the weapon and start raising it
    if (psp.WeaponY >= WEAPONBOTTOM) {
        if (player.health <= 0) {
            // Player is dead, so keep the weapon off screen
            SetPlayerSprite(player, ps_weapon, nullptr);
            return;
        }
        player.readyweapon = player.pendingweapon;      // Set the new weapon
        BringUpWeapon(player);                          // Begin raising the new weapon
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Handle an animation step for raising a weapon
//----------------------------------------------------------------------------------------------------------------------
void A_Raise(player_t& player, pspdef_t& psp) noexcept {
    // Raise the weapon
    psp.WeaponY -= RAISESPEED;

    // If weapon has been raised all the way to the top then change to the ready state
    if (psp.WeaponY <= WEAPONTOP) {
        psp.WeaponY = WEAPONTOP;
        SetPlayerSprite(player, ps_weapon, WEAPON_READY_STATES[player.readyweapon]);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Handle an animation frame for the gun muzzle flash
//----------------------------------------------------------------------------------------------------------------------
void A_GunFlash(player_t& player, [[maybe_unused]] pspdef_t& psp) noexcept {
    SetPlayerSprite(player, ps_flash, WEAPON_FLASH_STATES[player.readyweapon]);
}

//----------------------------------------------------------------------------------------------------------------------
// Punch a monster
//----------------------------------------------------------------------------------------------------------------------
void A_Punch(player_t& player, [[maybe_unused]] pspdef_t& psp) noexcept {
    uint32_t damage = (Random::nextU32(7) + 1) * 3;     // 1D8 * 3
    if (player.powers[pw_strength]) {                   // Are you a berserker?
        damage *= 10;
    }

    ASSERT(player.mo);
    mobj_t& mo = *player.mo;                                // Get the object into a local
    angle_t angle = mo.angle;                               // Get the player's angle
    angle += (255 - Random::nextU32(511)) << 18;            // Adjust for direction of attack
    LineAttack(mo, angle, MELEERANGE, FRACMAX, damage);     // Attack!
    mobj_t* const pTarget = gpLineTarget;

    if (pTarget) {                                                      // Did I hit someone?
        mo.angle = PointToAngle(mo.x, mo.y, pTarget->x, pTarget->y);    // Point the player to the victim
        S_StartSound(&mo.x, sfx_punch);                                 // DC: Bug fix to 3DO version - was missing the punch sound!
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Chainsaw a monster!
//----------------------------------------------------------------------------------------------------------------------
void A_Saw(player_t& player, [[maybe_unused]] pspdef_t& psp) noexcept {
    ASSERT(player.mo);
    const uint32_t damage = (Random::nextU32(7) + 1) * 3;   // 1D8 * 3
    mobj_t& mo = *player.mo;                                // Get the player's object
    angle_t angle = mo.angle;                               // Get the current facing angle
    angle += (255 - Random::nextU32(511)) << 18;            // Add a little randomness

    // Use meleerange + 1 so the puff doesn't skip the flash
    LineAttack(mo, angle, MELEERANGE + 1, FRACMAX, damage);
    mobj_t* const pTarget = gpLineTarget;

    if (!pTarget) {
        // If nobody is hit just do a loud saw sound every so often
        S_StartSound(&mo.x, sfx_sawful, true);
        return;
    }

    S_StartSound(&mo.x, sfx_sawhit, true);  // Meat grinding sound!! :) Yumm!
    
    // Turn to face target and jiggle around to make it more visually appealing to those who like more gore
    angle = PointToAngle(mo.x, mo.y, pTarget->x, pTarget->y);
    const angle_t testangle = angle - mo.angle;

    if (testangle > ANG180) {
        if (testangle < negateAngle(ANG90 / 20)) {
            mo.angle = angle + (ANG90 / 21);
        } else {
            mo.angle -= (ANG90 / 20);
        }
    } else {
        if (testangle > (ANG90 / 20)) {
            mo.angle = angle - (ANG90 / 21);
        } else {
            mo.angle += (ANG90 / 20);
        }
    }

    mo.flags |= MF_JUSTATTACKED;    // I have attacked someone...
}

//----------------------------------------------------------------------------------------------------------------------
// Fire a rocket from the rocket launcher
//----------------------------------------------------------------------------------------------------------------------
void A_FireMissile(player_t& player, [[maybe_unused]] pspdef_t& psp) noexcept {
    --player.ammo[am_misl];                                     // Remove a round
    SpawnPlayerMissile(*player.mo, gMObjInfo[MT_ROCKET]);       // Create a missile object
}

//----------------------------------------------------------------------------------------------------------------------
// Fire a BFG shot
//----------------------------------------------------------------------------------------------------------------------
void A_FireBFG(player_t& player, [[maybe_unused]] pspdef_t& psp) noexcept {
    player.ammo[am_cell] -= BFGCELLS;
    SpawnPlayerMissile(*player.mo, gMObjInfo[MT_BFG]);
}

//----------------------------------------------------------------------------------------------------------------------
// Fire a plasma rifle round
//----------------------------------------------------------------------------------------------------------------------
void A_FirePlasma(player_t& player, [[maybe_unused]] pspdef_t& psp) noexcept {
    --player.ammo[am_cell];     // Remove a round

    // I have two flash states, choose one randomly and then spawn the missile
    SetPlayerSprite(player, ps_flash, WEAPON_FLASH_STATES[player.readyweapon] + Random::nextU32(1));
    SpawnPlayerMissile(*player.mo, gMObjInfo[MT_PLASMA]);
}

//----------------------------------------------------------------------------------------------------------------------
// Shoot a single bullet round
//----------------------------------------------------------------------------------------------------------------------
static void GunShot(mobj_t& mo, const bool bAccurate) noexcept {
    const uint32_t damage = (Random::nextU32(3) + 1) * 4;   // Damage done: 1D4 * 4
    angle_t angle = mo.angle;                               // Get the angle of fire
    if (!bAccurate) {
        angle += (255 - Random::nextU32(511)) << 18;        // Make it a little random
    }

    LineAttack(mo, angle, MISSILERANGE, FRACMAX, damage);   // Inflict damage
}

//----------------------------------------------------------------------------------------------------------------------
// Fire a single round from the pistol
//----------------------------------------------------------------------------------------------------------------------
void A_FirePistol(player_t& player, [[maybe_unused]] pspdef_t& psp) noexcept {
    S_StartSound(&player.mo->x, sfx_pistol);                                        // Bang!!
    --player.ammo[am_clip];                                                         // Remove a round
    SetPlayerSprite(player, ps_flash, WEAPON_FLASH_STATES[player.readyweapon]);     // Flash weapon
    GunShot(*player.mo, (!player.refire));                                          // Shoot a round
}

//----------------------------------------------------------------------------------------------------------------------
// Fire a standard shotgun blast
//----------------------------------------------------------------------------------------------------------------------
void A_FireShotgun(player_t& player, [[maybe_unused]] pspdef_t& psp) noexcept {
    mobj_t& mo = *player.mo;
    S_StartSound(&mo.x, sfx_shotgn);    // Bang!
    --player.ammo[am_shell];            // Remove a round

    SetPlayerSprite(player, ps_flash, WEAPON_FLASH_STATES[player.readyweapon]);

    // Shotgun pellets all go at a fixed slope.
    // There are also 7 pellets.
    Fixed slope = AimLineAttack(mo, mo.angle, MISSILERANGE);

    for (uint32_t i = 7; i > 0; --i) {
        uint32_t damage = (Random::nextU32(3) + 1) * 4;         // Damage done: 1D4 * 4
        angle_t angle = mo.angle;
        angle += (255 - Random::nextU32(511)) << 18;            // Get some randomness
        LineAttack(mo, angle, MISSILERANGE, slope, damage);     // Do damage
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Make the shotgun cocking sound...
//----------------------------------------------------------------------------------------------------------------------
void A_CockSgun(player_t& player, [[maybe_unused]] pspdef_t& psp) noexcept {
    S_StartSound(&player.mo->x, sfx_sgcock);
}

//----------------------------------------------------------------------------------------------------------------------
// Fire a round from the chain gun!
//----------------------------------------------------------------------------------------------------------------------
void A_FireCGun(player_t& player, pspdef_t& psp) noexcept {
    mobj_t& mo = *player.mo;
    S_StartSound(&mo.x,sfx_pistol);     // Bang!

    if (player.ammo[am_clip] > 0) {     // Any ammo left?
        --player.ammo[am_clip];

        // Make sure the flash matches the weapon frame state.
        // DC: I fixed this from the 3DO version - this logic wasn't quite correct previously for the chaingun.
        state_t* pNewState = WEAPON_FLASH_STATES[wp_chaingun];
        if (psp.StatePtr == &gStates[S_CHAIN2]) {
            pNewState += 1;
        } else if (psp.StatePtr == &gStates[S_CHAIN3]) {
            pNewState += 2;
        }

        SetPlayerSprite(player, ps_flash, pNewState);
        GunShot(mo, !player.refire);    // Shoot a bullet
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Adjust the lighting based on the weapon fired
//----------------------------------------------------------------------------------------------------------------------
void A_Light0(player_t& player, [[maybe_unused]] pspdef_t& psp) noexcept {
    player.extralight = 0;
}

void A_Light1(player_t& player, [[maybe_unused]] pspdef_t& psp) noexcept {
    player.extralight = 1;
}

void A_Light2(player_t& player, [[maybe_unused]] pspdef_t& psp) noexcept {
    player.extralight = 2;
}

//----------------------------------------------------------------------------------------------------------------------
// Spawn a BFG explosion on every monster in view
//----------------------------------------------------------------------------------------------------------------------
void A_BFGSpray(mobj_t& mo) noexcept {
    // Offset angles from its attack angle
    angle_t an = (mo.angle - (ANG90 / 2));

    for (uint32_t i = 40; i > 0; --i) {
        // mo->target is the originator (player) of the missile
        AimLineAttack(*mo.target, an, 16 * 64 * FRACUNIT);
        mobj_t* const pTarget = gpLineTarget;

        if (pTarget) {
            SpawnMObj(
                pTarget->x,
                pTarget->y,
                pTarget->z + (pTarget->height >> 2),
                gMObjInfo[MT_EXTRABFG]
            );

            uint32_t damage = 15;   // Minimum 15 points of damage
            for (uint32_t j = 15; j > 0; --j) {
                damage += Random::nextU32(7);
            }

            DamageMObj(*pTarget, mo.target, mo.target, damage);
        }

        an += (ANG90 / 40);     // Step the angle for the attack
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Play the BFG sound for detonation
//----------------------------------------------------------------------------------------------------------------------
void A_BFGsound(player_t& player, [[maybe_unused]] pspdef_t& psp) noexcept {
    S_StartSound(&player.mo->x, sfx_bfg);
}

//----------------------------------------------------------------------------------------------------------------------
// Called at start of level for each player
//----------------------------------------------------------------------------------------------------------------------
void SetupPSprites(player_t& player) noexcept {
    // Remove all psprites
    pspdef_t *psp = player.psprites;

    for (uint32_t i = 0; i < NUMPSPRITES; ++i) {
        psp->StatePtr = nullptr;
        psp->Time = UINT32_MAX;
        ++psp;
    }

    // Set the current weapon as pending and begin showing it
    player.pendingweapon = player.readyweapon;
    BringUpWeapon(player);
}

//----------------------------------------------------------------------------------------------------------------------
// Called every by player thinking routine.
// Count down the tick timer and execute the think routines.
//----------------------------------------------------------------------------------------------------------------------
void MovePSprites(player_t& player) noexcept {
    pspdef_t* psp = player.psprites;

    for (uint32_t i = 0; i < NUMPSPRITES; ++i) {
        if (psp->Time != UINT32_MAX) {      // Never change state?
            if (psp->Time > 1) {            // Has enough time elapsed?
                --psp->Time;
            } else {
                SetPlayerSprite(player, (psprnum_e) i, psp->StatePtr->nextstate);   // Next state
            }
        }
        ++psp;
    }

    psp = player.psprites;
    psp[ps_flash].WeaponX = psp[ps_weapon].WeaponX;     // Attach the flash to the weapon
    psp[ps_flash].WeaponY = psp[ps_weapon].WeaponY;
}
