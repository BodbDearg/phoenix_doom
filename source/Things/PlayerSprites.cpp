#include "PlayerSprites.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Random.h"
#include "Base/Tables.h"
#include "Game/Data.h"
#include "Info.h"
#include "Interactions.h"
#include "Map/Map.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "MapObj.h"

static constexpr uint32_t   BFGCELLS        = 40 * TICKSPERSEC;     // Number of energy units per blast
static constexpr int32_t    LOWERSPEED      = 18;                   // Speed to lower the player's weapon
static constexpr int32_t    RAISESPEED      = 18;                   // Speed to raise the player's weapon
static constexpr int32_t    WEAPONBOTTOM    = 128;                  // Bottommost Y for hiding weapon
static constexpr int32_t    WEAPONTOP       = 32;                   // Topmost Y for showing weapon

/**********************************

    This data will describe how each weapon is handled

**********************************/

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
    0,                          // Fists
    &gStates[S_PISTOLFLASH],    // Pistol
    &gStates[S_SGUNFLASH1],     // Shotgun
    &gStates[S_CHAINFLASH1],    // Chaingun
    &gStates[S_MISSILEFLASH1],  // Rocket launcher
    &gStates[S_PLASMAFLASH1],   // Plasma rifle
    &gStates[S_BFGFLASH1],      // BFG 9000
    0                           // Chainsaw
};

/**********************************

    Recursively scan all the sectors within earshot
    so the monsters will begin tracking the player.
    Called EXCLUSIVELY by NoiseAlert!

**********************************/

static mobj_t* gpSoundTarget;       // Player object to track (Make global to avoid passing it)

static void RecursiveSound(sector_t *sec, uint32_t soundblocks)
{
    uint32_t Count;
    line_t **checker;
    line_t *check;
    sector_t *front,*back;

// wake up all monsters in this sector

    if (sec->validcount != gValidCount || sec->soundtraversed > (soundblocks+1)) {
        sec->validcount = gValidCount;       // Mark for flood fill
        sec->soundtraversed = soundblocks+1;    // distance for sound (1 or 2)
        sec->soundtarget = gpSoundTarget;     // Set the noise maker source
        Count = sec->linecount;             // How many lines to check?
        checker = sec->lines;
        do {
            check = checker[0]; // Get the current line ptr
            back = check->backsector;   // Get the backface sector
            if (back) {         // Ah, it's double sided!
                front = check->frontsector; // Get the front side
                if (front->floorheight < back->ceilingheight && // Open door?
                    front->ceilingheight > back->floorheight) {
                    if (front == sec) {     // Use the front?
                        front = back;       // Nope, use the back
                    }
                    if (check->flags & ML_SOUNDBLOCK) { // Is sound blocked?
                        if (!soundblocks) {     // First level deep?
                            RecursiveSound(front,1);    // Muffled sound...
                        }
                    } else {
                        RecursiveSound(front,soundblocks);      // Continue sound level 1
                    }
                }
            }
            ++checker;      // Next line pointer
        } while (--Count);      // All sectors checked?
    }
}

/**********************************

    Begin scanning all the sectors within earshot
    so the monsters will begin tracking the player

**********************************/

static void NoiseAlert(player_t *player)
{
    sector_t *sec;

    sec = player->mo->subsector->sector;    // Get the current sector ptr
    if (player->lastsoundsector != sec) {   // Not the same one?
        player->lastsoundsector = sec;  // Set the new sector I made sound in
        gpSoundTarget = player->mo;   // Set the target for the monsters
        ++gValidCount;           // Set a unique number for sector flood fill
        RecursiveSound(sec,0);  // Wake the monsters
    }
}

/**********************************

    Set the player sprite's state
    (Gun or muzzle flash)

**********************************/

static void SetPlayerSprite(player_t* player, psprnum_e position, const state_t* state)
{
    pspdef_t *psp;

    psp = &player->psprites[position];  // Get the sprite for the flash or weapon

    for (;;) {  
        psp->StatePtr = state;      // Save the state pointer
        if (!state) {       // No state?
            psp->Time = -1; // Forever (Shut down)
            break;          // Leave now
        }
        psp->Time = state->Time;    // Set the time delay, Could be 0...
        if (state->pspAction) {             // Is there an action procedure?
            state->pspAction(player,psp);   // Call the think logic (May call myself)
            state = psp->StatePtr;          // Get the new state pointer

            if (!state) {       // No state?
                break;
            }
        }
        if (psp->Time) {        // Time is valid?
            break;      // Exit now
        }
        state = state->nextstate;   // Cycle to the next state now
    }
}

/**********************************

    Starts bringing the pending weapon up from the bottom of the screen
    Uses player

**********************************/

static void BringUpWeapon(player_t *player)
{
    if (player->pendingweapon == wp_nochange) {     // Is the weapon to be changed?
        player->pendingweapon = player->readyweapon;    // Set to the new weapon
    }
    if (player->pendingweapon == wp_chainsaw) {     // Starting the chainsaw?
        S_StartSound(&player->mo->x,sfx_sawup);     // Play the chainsaw sound...
    }
    player->psprites[ps_weapon].WeaponY = WEAPONBOTTOM; // Set the screen Y to bottom
    SetPlayerSprite(player,ps_weapon,WEAPON_UP_STATES[player->pendingweapon]);        // Set the weapon sprite
    player->pendingweapon = wp_nochange;            // No new weapon pending
}

/**********************************

    Returns true if there is enough ammo to shoot
    if not, selects the next weapon to use

**********************************/

static bool CheckAmmo(player_t *player)
{
    ammotype_e ammo;    // Ammo type
    weapontype_e Weapon;
    uint32_t Count;         // Minimum ammo to fire with

    ammo = gWeaponAmmos[player->readyweapon];    // What ammo type to use?
    Count = 1;              // Assume 1 round
    if (player->readyweapon == wp_bfg) {        // BFG 9000?
        Count = BFGCELLS;   // Get the BFG energy requirements
    }
    if (ammo == am_noammo || player->ammo[ammo] >= Count) { // Enough ammo?
        return true;        // I can shoot!
    }

    // Out of ammo, pick a weapon to change to

    if (player->weaponowned[wp_plasma] && player->ammo[am_cell]) {
        Weapon = wp_plasma;
    } else if (player->weaponowned[wp_chaingun] && player->ammo[am_clip]) {
        Weapon = wp_chaingun;
    } else if (player->weaponowned[wp_shotgun] && player->ammo[am_shell]) {
        Weapon = wp_shotgun;
    } else if (player->ammo[am_clip]) {     // Any bullets?
        Weapon = wp_pistol;
    } else if (player->weaponowned[wp_chainsaw]) {  // Have a chainsaw?
        Weapon = wp_chainsaw;
    } else if (player->weaponowned[wp_missile] && player->ammo[am_misl]) {
        Weapon = wp_missile;
    } else if (player->weaponowned[wp_bfg] && player->ammo[am_cell]>=BFGCELLS) {
        Weapon = wp_bfg;
    } else {
        Weapon = wp_fist;       // Always drop down to the fists
    }
    player->pendingweapon = Weapon;     // Save the new weapon
                    // Lower the existing weapon
    SetPlayerSprite(player,ps_weapon,WEAPON_DOWN_STATES[player->readyweapon]);
    return false;   // Can't shoot!
}

/**********************************

    Fire the player's current weapon

**********************************/

static void FireWeapon(player_t *player)
{
    if (CheckAmmo(player)) {        // Do I have ammo? (Change weapon if not)
        SetMObjState(player->mo,&gStates[S_PLAY_ATK1]);  // Player is in attack mode
        player->psprites[ps_weapon].WeaponX = 0;    // Reset the weapon's screen
        player->psprites[ps_weapon].WeaponY = WEAPONTOP;    // position
        SetPlayerSprite(player,ps_weapon,WEAPON_ATTACK_STATES[player->readyweapon]);  // Begin animation
        NoiseAlert(player);     // Alert the monsters...
    }
}

/**********************************

    Player died, so put the weapon away for good

**********************************/

void LowerPlayerWeapon(player_t *player)
{
    SetPlayerSprite(player,ps_weapon,WEAPON_DOWN_STATES[player->readyweapon]);
}

/**********************************

    The player can fire the weapon or change to another weapon at this time

**********************************/

void A_WeaponReady(player_t *player,pspdef_t *psp)
{
    int BobValue;   // Must be signed!!
    uint32_t angle;     // Angle for weapon bobbing

    // Special case for chainsaw's idle sound

    if (player->readyweapon == wp_chainsaw && psp->StatePtr == &gStates[S_SAW]) {
        S_StartSound(&player->mo->x,sfx_sawidl);    // Play the idle sound
    }

// Check for change, if player is dead, put the weapon away

    if (player->pendingweapon != wp_nochange || !player->health) {  // Alive?
        SetPlayerSprite(player,ps_weapon,WEAPON_DOWN_STATES[player->readyweapon]);
        return;
    }

// check for weapon fire

    if (gJoyPadButtons & gPadAttack) {        // Attack?
        FireWeapon(player);             // Fire the weapon...
        return;             // Exit now
    }

// Bob the weapon based on movement speed

    BobValue = (player->bob>>FRACBITS); // Isolate the integer
    angle = (gTotalGameTicks<<6)&FINEMASK;       // Get the angle
    psp->WeaponX = (BobValue * gFineCosine[angle])>>FRACBITS;
    angle &= (FINEANGLES/2)-1;      // Force semi-circle
    psp->WeaponY = ((BobValue * gFineSine[angle])>>FRACBITS) + WEAPONTOP;
}

/**********************************

    The player can refire the weapon without lowering it entirely

**********************************/

void A_ReFire(player_t *player,pspdef_t *psp)
{

// Check for fire (if a weaponchange is pending, let it go through instead)

    if ( (gJoyPadButtons & gPadAttack) && // Still firing?
        player->pendingweapon == wp_nochange && player->health) {
        player->refire = true;      // Count for grimacing player face
        FireWeapon(player); // Shoot...
    } else {
        player->refire = false;     // Reset firing
        CheckAmmo(player);  // Still have ammo?
    }
}

/**********************************

    Handle an animation step for lowering a weapon

**********************************/

void A_Lower(player_t *player,pspdef_t *psp)
{
    psp->WeaponY += LOWERSPEED;     // Lower the Y coord
    if (player->playerstate == PST_DEAD) {  // Are you dead?
        psp->WeaponY = WEAPONBOTTOM;        // Force at the bottom
        return;             // Never allow it to go back up
    }
    
    if (psp->WeaponY >= WEAPONBOTTOM) { // Off the bottom?

// The old weapon has been lowered off the screen, so change the weapon
// and start raising it

        if (!player->health) {  // player is dead, so keep the weapon off screen
            SetPlayerSprite(player,ps_weapon,0);
            return;
        }
        player->readyweapon = player->pendingweapon;    // Set the new weapon
        BringUpWeapon(player);  // Begin raising the new weapon
    }
}

/**********************************

    Handle an animation step for raising a weapon

**********************************/

void A_Raise(player_t *player,pspdef_t *psp)
{
    psp->WeaponY -= RAISESPEED;     // Raise the weapon
    if (psp->WeaponY <= WEAPONTOP) {    // At the top?
        psp->WeaponY = WEAPONTOP;   // Make sure it's ok!
// the weapon has been raised all the way, so change to the ready state
        SetPlayerSprite(player,ps_weapon,WEAPON_READY_STATES[player->readyweapon]);
    }
}

/**********************************

    Handle an animation frame for the gun muzzle flash

**********************************/

void A_GunFlash(player_t *player,pspdef_t *psp)
{
    SetPlayerSprite(player,ps_flash,WEAPON_FLASH_STATES[player->readyweapon]);
}

/**********************************

    Punch a monster

**********************************/

void A_Punch(player_t *player,pspdef_t *psp)
{
    angle_t angle;
    mobj_t *mo,*target;
    uint32_t damage;

    damage = (Random::nextU32(7)+1)*3;        // 1D8 * 3
    if (player->powers[pw_strength]) {  // Are you a berserker?
        damage *= 10;
    }
    mo = player->mo;            // Get the object into a local
    angle = mo->angle;          // Get the player's angle
    angle += (255-Random::nextU32(511))<<18;  // Adjust for direction of attack
    LineAttack(mo,angle,MELEERANGE,FIXED_MAX,damage);  // Attack!
    target = gLineTarget;
    if (target) {       // Did I hit someone?
        // Point the player to the victim
        mo->angle = PointToAngle(mo->x,mo->y,target->x,target->y);
        S_StartSound(&mo->x, sfx_punch);    // DC: Bug fix to 3DO version - was missing the punch sound!
    }
}

/**********************************

    Chainsaw a monster!

**********************************/

void A_Saw(player_t *player,pspdef_t *psp)
{
    angle_t angle;      // Angle of attack
    long testangle;     // Must be SIGNED!
    uint32_t damage;
    mobj_t *mo,*target;

    damage = (Random::nextU32(7)+1)*3;        // 1D8 * 3
    mo = player->mo;                    // Get the player's object
    angle = mo->angle;                  // Get the current facing angle
    angle += (255-Random::nextU32(511))<<18;  // Add a little randomness

// use meleerange + 1 so the puff doesn't skip the flash

    LineAttack(mo,angle,MELEERANGE+1,FIXED_MAX,damage);
    target = gLineTarget;
    if (!target) {          // Anyone hit?
        S_StartSound(&mo->x,sfx_sawful);    // Loud saw sound effect
        return;
    }
    S_StartSound(&mo->x,sfx_sawhit);        // Meat grinding sound!! :) Yumm!

// Turn to face target and jiggle around to make it more visually appealing
// to those who like more gore

    angle = PointToAngle(mo->x,mo->y,target->x,target->y);  // Get the angle
    testangle = angle-mo->angle;
    if (testangle > ANG180) {       // Handle the chainsaw jiggle
        if (testangle < (-(ANG90/20))) {
            mo->angle = angle + (ANG90/21);
        } else {
            mo->angle -= (ANG90/20);
        }
    } else {
        if (testangle > (ANG90/20)) {
            mo->angle = angle - (ANG90/21);
        } else {
            mo->angle += (ANG90/20);
        }
    }
    mo->flags |= MF_JUSTATTACKED;       // I have attacked someone...
}

/**********************************

    Fire a rocket from the rocket launcher

**********************************/

void A_FireMissile(player_t *player,pspdef_t *psp)
{
    --player->ammo[am_misl];    // Remove a round
    SpawnPlayerMissile(player->mo,&gMObjInfo[MT_ROCKET]);        // Create a missile object
}

/**********************************

    Fire a BFG shot

**********************************/

void A_FireBFG(player_t *player,pspdef_t *psp)
{
    player->ammo[am_cell] -= BFGCELLS;
    SpawnPlayerMissile(player->mo,&gMObjInfo[MT_BFG]);
}

/**********************************

    Fire a plasma rifle round

**********************************/

void A_FirePlasma(player_t *player,pspdef_t *psp)
{
    --player->ammo[am_cell];    // Remove a round
    // I have two flash states, choose one randomly
    SetPlayerSprite(player,ps_flash,WEAPON_FLASH_STATES[player->readyweapon]+Random::nextU32(1));
    SpawnPlayerMissile(player->mo,&gMObjInfo[MT_PLASMA]);        // Spawn the missile
}

/**********************************

    Shoot a single bullet round

**********************************/

static void GunShot(mobj_t *mo, bool accurate)
{
    angle_t angle;      // Angle of fire
    uint32_t damage;        // Damage done

    damage = (Random::nextU32(3)+1)*4;        // 1D4 * 4
    angle = mo->angle;              // Get the angle
    if (!accurate) {
        angle += (255-Random::nextU32(511))<<18;  // Make it a little random
    }
    LineAttack(mo,angle,MISSILERANGE,FIXED_MAX,damage);    // Inflict damage
}

/**********************************

    Fire a single round from the pistol

**********************************/

void A_FirePistol(player_t *player,pspdef_t *psp)
{
    S_StartSound(&player->mo->x,sfx_pistol);        // Bang!!

    --player->ammo[am_clip];    // Remove a round
    SetPlayerSprite(player,ps_flash,WEAPON_FLASH_STATES[player->readyweapon]);    // Flash weapon
    GunShot(player->mo,!player->refire);    // Shoot a round
}

/**********************************

    Fire a standard shotgun blast

**********************************/

void A_FireShotgun(player_t *player, pspdef_t *psp)
{
    angle_t angle;
    uint32_t damage;
    uint32_t i;
    int slope;
    mobj_t *mo;

    mo = player->mo;        // Get mobj pointer for player
    S_StartSound(&mo->x,sfx_shotgn);    // Bang!
    --player->ammo[am_shell];   // Remove a round
    SetPlayerSprite(player,ps_flash,WEAPON_FLASH_STATES[player->readyweapon]);
    slope = AimLineAttack(mo,mo->angle,MISSILERANGE);

// shotgun pellets all go at a fixed slope

    i = 7;      // Init index
    do {
        damage = (Random::nextU32(3)+1)*4;        // 1D4 * 4
        angle = mo->angle;
        angle += (255-Random::nextU32(511))<<18;      // Get some randomness
        LineAttack(mo,angle,MISSILERANGE,slope,damage); // Do damage
    } while (--i);      // 7 pellets
}

/**********************************

    Make the shotgun cocking sound...

**********************************/

void A_CockSgun(player_t *player,pspdef_t *psp)
{
    S_StartSound(&player->mo->x,sfx_sgcock);
}

/**********************************

    Fire a round from the chain gun!

**********************************/

void A_FireCGun(player_t *player,pspdef_t *psp)
{
    mobj_t *mo;
    state_t *NewState;
    
    mo = player->mo;
    S_StartSound(&mo->x,sfx_pistol);        // Bang!
    if (player->ammo[am_clip]) {    // Any ammo?
        --player->ammo[am_clip];
        // Make sure the flash matches the weapon frame state
        NewState = WEAPON_FLASH_STATES[wp_chaingun];
        if (psp->StatePtr==&gStates[S_CHAIN1]) {
            ++NewState;
        }
        SetPlayerSprite(player,ps_flash,NewState);
        GunShot(mo,!player->refire);        // Shoot a bullet
    }
}


/**********************************

    Adjust the lighting based on the weapon fired

**********************************/

void A_Light0(player_t *player,pspdef_t *psp)
{
    player->extralight = 0;     // No extra light
}

void A_Light1(player_t *player,pspdef_t *psp)
{
    player->extralight = 1;     // 1 unit of extra light
}

void A_Light2(player_t *player,pspdef_t *psp)
{
    player->extralight = 2;     // 2 units of extra light
}

/**********************************

    Spawn a BFG explosion on every monster in view

**********************************/

void A_BFGSpray(mobj_t *mo)
{
    uint32_t i,j,damage;
    angle_t an;
    mobj_t *target;

    // offset angles from its attack angle
    i = 40;
    an = (mo->angle - (ANG90/2));
    do {
        // mo->target is the originator (player) of the missile
        AimLineAttack(mo->target,an,16*64*FRACUNIT);
        target = gLineTarget;
        if (target) {
            SpawnMObj(target->x,target->y,target->z + (target->height>>2),&gMObjInfo[MT_EXTRABFG]);
            damage = 15;        // Minimum 15 points of damage
            j = 15;
            do {
                damage += Random::nextU32(7);
            } while (--j);
            DamageMObj(target,mo->target,mo->target,damage);
        }
        an += (ANG90/40);       // Step the angle for the attack
    } while (--i);
}

/**********************************

    Play the BFG sound for detonation

**********************************/

void A_BFGsound(player_t *player,pspdef_t *psp)
{
    S_StartSound(&player->mo->x,sfx_bfg);
}

/**********************************

    Called at start of level for each player

**********************************/

void SetupPSprites(player_t *player)
{
    uint32_t i;
    pspdef_t *psp;

// Remove all psprites

    i = NUMPSPRITES;
    psp = player->psprites;
    do {
        psp->StatePtr = 0;
        psp->Time = -1;
        ++psp;
    } while (--i);

    // Set the current weapon as pending

    player->pendingweapon = player->readyweapon;
    BringUpWeapon(player);      // Show the weapon
}

/**********************************

    Called every by player thinking routine
    Count down the tick timer and execute the think routines.
    
**********************************/

void MovePSprites(player_t *player)
{
    uint32_t i;
    pspdef_t *psp;

    psp = player->psprites;     // Index to the player's sprite records
    i = 0;              // How many to process? (Must go from 0-NUMPSPRITES)
    do {
        uint32_t Remainder;
        Remainder = gElapsedTime;    // Get the atomic count
        while (psp->Time!=-1) { // Never change state?
            if (psp->Time>Remainder) {  // Has enough time elapsed?
                psp->Time-=Remainder;   // Remove time and exit
                break;
            }
            Remainder -= psp->Time;     // Remove residual time
            SetPlayerSprite(player, (psprnum_e) i, psp->StatePtr->nextstate);  // Next state        
        }
        ++psp;  // Next entry
    } while (++i<NUMPSPRITES);
    psp = player->psprites;
    psp[ps_flash].WeaponX = psp[ps_weapon].WeaponX; // Attach the flash to the weapon    
    psp[ps_flash].WeaponY = psp[ps_weapon].WeaponY;
}
