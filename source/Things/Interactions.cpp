#include "Interactions.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Random.h"
#include "Base/Tables.h"
#include "Game/Data.h"
#include "Game/DoomDefines.h"
#include "Game/DoomRez.h"
#include "Info.h"
#include "Map/MapUtil.h"
#include "MapObj.h"
#include "UI/StatusBarUI.h"

static constexpr uint32_t   INVULNTICS      = 30 * TICKSPERSEC;         // Time for invulnerability
static constexpr uint32_t   INVISTICS       = 60 * TICKSPERSEC;         // Time for invisibility
static constexpr uint32_t   IRONTICS        = 60 * TICKSPERSEC;         // Time for rad suit
static constexpr uint32_t   BONUSADD        = 16;                       // Time added for bonus color (ticks)
static constexpr uint32_t   BASETHRESHOLD   = (7 * TICKSPERSEC) / 4;    // Number of tics to exclusivly follow a target

// A weapon is found with two clip loads, a big item has five clip loads.
// This table is for ammo for a normal clip:
static constexpr uint32_t CLIP_AMMO[NUMAMMO] = { 10, 4, 20, 1 };

//----------------------------------------------------------------------------------------------------------------------
// Num is the number of clip loads, not the individual count (0 = 1/2 clip).
// Returns false if the ammo can't be picked up at all.
// Also I switch weapons if I am using a wimpy weapon and I get ammo for a much better weapon.
//----------------------------------------------------------------------------------------------------------------------
static bool GiveAmmo(player_t& player, const ammotype_e ammo, uint32_t numofclips) noexcept {
    if (ammo == am_noammo || ammo >= NUMAMMO) {     // Is this not ammo?
        return false;                               // Can't pick it up
    }

    uint32_t oldammo = player.ammo[ammo];       // Get the current ammo
    uint32_t maxammo = player.maxammo[ammo];    // Get the maximum

    if (oldammo >= maxammo) {                   // Full already?
        return false;                           // Can't pick it up
    }

    if (numofclips) {                       // Valid ammo count?
        numofclips *= CLIP_AMMO[ammo];      // Get the ammo adder
    } else {
        numofclips = CLIP_AMMO[ammo] / 2;   // Give a half ration
    }

    if (gGameSkill == sk_baby) {
        numofclips <<= 1;           // give double ammo in wimp mode
    }

    oldammo += numofclips;          // Add in the new ammo
    if (oldammo >= maxammo) {
        oldammo = maxammo;          // Max it out
    }

    player.ammo[ammo] = oldammo;    // Save the new ammo count
    if (oldammo != numofclips) {    // Only possible if oldammo == 0
        return true;                // Don't change up weapons, player was lower on purpose
    }

    // Which type was picked up
    switch (ammo) {
        case am_clip: {
            if (player.readyweapon == wp_fist) {            // I have fists up?
                if (player.weaponowned[wp_chaingun]) {      // Start the chaingun
                    player.pendingweapon = wp_chaingun;
                } else {
                    player.pendingweapon = wp_pistol;       // Try the pistol
                }
            }
        }   break;

        case am_shell: {
            if (player.readyweapon == wp_fist || player.readyweapon == wp_pistol) {
                if (player.weaponowned[wp_shotgun]) {
                    player.pendingweapon = wp_shotgun;
                }
            }
        }   break;

        case am_cell: {
            if (player.readyweapon == wp_fist || player.readyweapon == wp_pistol) {
                if (player.weaponowned[wp_plasma]) {
                    player.pendingweapon = wp_plasma;
                }
            }
        }   break;

        case am_misl: {
            if (player.readyweapon == wp_fist) {            // Only using fists?
                if (player.weaponowned[wp_missile]) {
                    player.pendingweapon = wp_missile;      // Use rocket launcher
                }
            }
        }   break;

        case am_noammo:
        case NUMAMMO:
            break;
    }
    return true;            // I picked it up!
}

//----------------------------------------------------------------------------------------------------------------------
// Pick up a weapon.
// The weapon name may have a MF_DROPPED flag or'd in so that it will affect the amount of ammo contained inside.
//----------------------------------------------------------------------------------------------------------------------
static uint32_t GiveWeapon(player_t& player, const weapontype_e weapon, uint32_t dropped) noexcept {
    bool bPickedUp = false;

    // Give one clip with a dropped weapon, two clips with a found weapon
    if (gWeaponAmmos[weapon] != am_noammo) {    // Any ammo inside?
        dropped = dropped ? 1 : 2;              // 1 or 2 clips
        bPickedUp = GiveAmmo(player, gWeaponAmmos[weapon], dropped);
    }

    if (!player.weaponowned[weapon]) {          // Already had such a weapon?
        bPickedUp = true;
        player.weaponowned[weapon] = true;      // I have it now
        player.pendingweapon = weapon;          // Use this weapon
        gStBar.specialFace = f_gotgat;          // He he he! Evil grin!
    }

    return bPickedUp;   // Did you pick it up?
}

//----------------------------------------------------------------------------------------------------------------------
// Increase the player's health; returns false if the health isn't needed at all.
//----------------------------------------------------------------------------------------------------------------------
static uint32_t GiveBody(player_t& player, uint32_t num) noexcept {
    if (player.health >= MAXHEALTH) {   // Already maxxed out?
        return false;                   // Don't get anymore
    }

    num += player.health;       // Make new health
    if (num >= MAXHEALTH) {
        num = MAXHEALTH;        // Set to maximum
    }

    player.health = num;            // Save the new health
    player.mo->MObjHealth = num;    // Save in MObj record as well
    return true;                    // Pick it up
}

//----------------------------------------------------------------------------------------------------------------------
// Award a new suit of armor.
// Returns false if the armor is worse than the current armor.
//----------------------------------------------------------------------------------------------------------------------
static uint32_t GiveArmor(player_t& player, const uint32_t armortype) noexcept {
    const uint32_t hits = armortype * 100;      // 100 or 200%
    if (player.armorpoints >= hits) {           // Already has this armor?
        return false;                           // Don't pick up
    }

    player.armortype = armortype;       // Set the type
    player.armorpoints = hits;          // Set the new value
    return true;                        // Pick it up
}

//----------------------------------------------------------------------------------------------------------------------
// Award a keycard or skull key
//----------------------------------------------------------------------------------------------------------------------
static void GiveCard(player_t& player, const card_e card) noexcept {
    if (!player.cards[card]) {              // I don't have it already?
        player.bonuscount = BONUSADD;       // Add the bonus value for color
        player.cards[card] = true;          // I have it now!
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Award a powerup
//----------------------------------------------------------------------------------------------------------------------
uint32_t GivePower(player_t& player, const powertype_e power) noexcept {
    switch (power) {
        case pw_invulnerability:                    // God mode?
            player.powers[power] = INVULNTICS;      // Set the time
            break;

        case pw_invisibility:
            player.powers[power] = INVISTICS;       // I am invisible!
            player.mo->flags |= MF_SHADOW;
            break;

        case pw_ironfeet:                           // Radiation suit?
            player.powers[power] = IRONTICS;        // Set the time
            break;

        case pw_strength:                   // Berzerker pack
            GiveBody(player, 100);          // Full health
            player.powers[power] = true;    // I have the power
            break;

        default: {
            if (player.powers[power]) {         // Already have the power up?
                return false;                   // Already got it, don't get it again
            }
            player.powers[power] = true;        // Award the power up
        }   break;
    }

    return true;    // Pick it up
}

//----------------------------------------------------------------------------------------------------------------------
// The case statement got too big, moved the rest here.
// Returns sound to play, or UINT32_MAX if no sound
//----------------------------------------------------------------------------------------------------------------------
static uint32_t TouchSpecialThing2(mobj_t& toucher, const uint32_t sprite) noexcept {
    ASSERT(toucher.player);
    player_t& player = *toucher.player;

    switch (sprite) {
        case rSPR_GREENARMOR: { // Green armor
            if (!GiveArmor(player, 1)) {
                return UINT32_MAX;
            }
            player.message = "You pick up the armor.";
        }   break;

        case rSPR_BLUEARMOR: {  // Blue armor
            if (!GiveArmor(player, 2)) {
                return UINT32_MAX;
            }
            player.message = "You got the MegaArmor!";
        }   break;

        // Cards, leave cards for everyone
        case rSPR_BLUEKEYCARD:
            if (!player.cards[it_bluecard]) {
                player.message = "You pick up a blue keycard.";
            }
            GiveCard(player, it_bluecard);
        CardSound:
            break;

        case rSPR_YELLOWKEYCARD: {
            if (!player.cards[it_yellowcard]) {
                player.message = "You pick up a yellow keycard.";
            }
            GiveCard(player, it_yellowcard);
            goto CardSound;
        }   break;

        case rSPR_REDKEYCARD: {
            if (!player.cards[it_redcard]) {
                player.message = "You pick up a red keycard.";
            }
            GiveCard(player, it_redcard);
            goto CardSound;
        }   break;

        case rSPR_BLUESKULLKEY: {
            if (!player.cards[it_blueskull]) {
                player.message = "You pick up a blue skull key.";
            }
            GiveCard(player, it_blueskull);
            goto CardSound;
        }   break;

        case rSPR_YELLOWSKULLKEY: {
            if (!player.cards[it_yellowskull]) {
                player.message = "You pick up a yellow skull key.";
            }
            GiveCard(player, it_yellowskull);
            goto CardSound;
        }   break;

        case rSPR_REDSKULLKEY: {
            if (!player.cards[it_redskull]) {
                player.message = "You pick up a red skull key.";
            }
            GiveCard(player, it_redskull);
            goto CardSound;
        }   break;

        // Heals
        case rSPR_STIMPACK: {   // Stim pack
            if (!GiveBody(player, 10)) {
                return UINT32_MAX;
            }
            player.message = "You pick up a stimpack.";
        }   break;

        case rSPR_MEDIKIT: {
            if (!GiveBody(player, 25)) {     // Medkit
                return UINT32_MAX;
            }
            if (player.health < 50) {
                player.message = "You pick up a medikit that you REALLY need!";
            } else {
                player.message = "You pick up a medikit.";
            }
        }   break;

        // Power ups
        case rSPR_INVULNERABILITY: {    // God mode!!
            if (!GivePower(player, pw_invulnerability)) {
                return UINT32_MAX;
            }
            player.message = "Invulnerability!";
        }   break;

        case rSPR_BERZERKER: {  // Berserker pack
            if (!GivePower(player, pw_strength)) {
                return UINT32_MAX;
            }
            player.message = "Berserk!";
            if (player.readyweapon != wp_fist) {    // Already fists?
                player.pendingweapon = wp_fist;     // Set to fists
            }
        }   break;

        case rSPR_INVISIBILITY: {   // Invisibility
            if (!GivePower(player, pw_invisibility)) {
                return UINT32_MAX;
            }
            player.message = "Invisibility!";
        }   break;

        case rSPR_RADIATIONSUIT: {  // Radiation suit
            if (!GivePower(player, pw_ironfeet)) {
                return UINT32_MAX;
            }
            player.message = "Radiation Shielding Suit";
        }   break;

        case rSPR_COMPUTERMAP: {    // Computer map
            if (!GivePower(player, pw_allmap)) {
                return UINT32_MAX;
            }
            player.message = "Computer Area Map";
        }   break;

        case rSPR_IRGOGGLES:    // Light amplification visor
            break;
    }

    return sfx_itemup;  // Return the sound effect
}

void givePlayerABackpack(player_t& player) noexcept {
    // Double the ammo capacity if we haven't already got a backpack
    if (!player.backpack) {
        player.backpack = true;

        for (uint32_t i = 0; i < NUMAMMO; ++i) {
            player.maxammo[i] *= 2;
        }
    }

    // Give the player 1 clip of everything
    for (uint32_t i = 0; i < NUMAMMO; ++i) {
        GiveAmmo(player, (ammotype_e) i, 1);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Award the item to the player if needed
//----------------------------------------------------------------------------------------------------------------------
void TouchSpecialThing(mobj_t& special, mobj_t& toucher) noexcept {
    const Fixed delta = special.z - toucher.z;      // Differances between z's
    if (delta > toucher.height || (delta < (-8 * FRACUNIT))) {
        return;     // Out of reach
    }

    ASSERT(toucher.player);
    player_t& player = *toucher.player;

    if (toucher.MObjHealth <= 0) {      // Dead player shape?
        return;                         // Can happen with a sliding player corpse
    }

    uint32_t sound = sfx_itemup;        // Get item sound
    uint32_t i = special.state->SpriteFrame >> FF_SPRITESHIFT;

    switch (i) {
        // Bonus items
        case rSPR_HEALTHBONUS: {
            // Health bonus
            player.message = "You pick up a health bonus.";
            i = 2;                          // Award size
        Healthy:
            i += player.health;             // Can go over 100%
            if (i >= 201) {
                i = 200;
            }
            player.health = i;              // Save new health
            player.mo->MObjHealth = i;
        }   break;

        case rSPR_SOULSPHERE: {
            // Supercharge sphere
            player.message = "Supercharge!";
            i = 100; // Award 100 points
            goto Healthy;
        }   break;

        case rSPR_ARMORBONUS: {             // Armor bonus
            i = player.armorpoints + 2;     // Can go over 100%
            if (i >= 201) {
                i = 200;                    // But not 200%!
            }
            player.armorpoints = i;
            if (player.armortype <= 0) {    // Any armor?
                player.armortype = 1;       // Set to green type
            }
            player.message = "You pick up an armor bonus.";
        }   break;

        // Ammo
        case rSPR_CLIP: {
            i = ((special.flags & MF_DROPPED) != 0) ? 0 : 1;    // Half or full clip
            if (!GiveAmmo(player, am_clip, i)) {                // Give the ammo
                return;
            }
            player.message = "Picked up a clip.";
        }   break;

        case rSPR_BOXAMMO: {
            if (!GiveAmmo(player, am_clip, 5)) {    // Give 5 clips worth
                return;
            }
            player.message = "Picked up a box of bullets.";
        }   break;

        case rSPR_ROCKET: {
            if (!GiveAmmo(player, am_misl, 1)) {    // 1 clip of rockets
                return;
            }
            player.message = "Picked up a rocket.";
        }   break;

        case rSPR_BOXROCKETS: {
            if (!GiveAmmo(player, am_misl, 5)) {    // 5 rockets
                return;
            }
            player.message = "Picked up a box of rockets.";
        }   break;

        case rSPR_CELL: {
            if (!GiveAmmo(player, am_cell, 1)) {    // Single charge cell
                return;
            }
            player.message = "Picked up an energy cell.";
        }   break;

        case rSPR_CELLPACK: {
            if (!GiveAmmo(player, am_cell, 5)) {    // Big energy cell
                return;
            }
            player.message = "Picked up an energy cell pack.";
        }   break;

        case rSPR_SHELLS: {
            if (!GiveAmmo(player, am_shell, 1)) {   // Clip of shells
                return;
            }
            player.message = "Picked up 4 shotgun shells.";
        }   break;

        case rSPR_BOXSHELLS: {
            if (!GiveAmmo(player, am_shell, 5)) { // Box of shells
                return;
            }
            player.message = "Picked up a box of shotgun shells.";
        }   break;

        case rSPR_BACKPACK: {
            givePlayerABackpack(player);
            player.message = "Picked up a backpack full of ammo!";
        }   break;

        // Weapons
        case rSPR_BFG9000: {    // BFG 9000
            if (!GiveWeapon(player, wp_bfg, false)) {
                return;
            }
            player.message = "You got the BFG9000!  Oh, yes.";
            sound = sfx_wpnup;
        }   break;

        case rSPR_CHAINGUN: {   // Chain gun
            if (!GiveWeapon(player, wp_chaingun, false)) {
                return;
            }
            player.message = "You got the chaingun!";
            sound = sfx_wpnup;
        }   break;

        case rSPR_CHAINSAW: {   // Chainsaw
            if (!GiveWeapon(player, wp_chainsaw, false)) {
                return;
            }
            player.message = "A chainsaw!  Find some meat!";
            sound = sfx_wpnup;
        }   break;

        case rSPR_ROCKETLAUNCHER: { // Rocket launcher
            if (!GiveWeapon(player, wp_missile, false)) {
                return;
            }
            player.message = "You got the rocket launcher!";
            sound = sfx_wpnup;
        }   break;

        case rSPR_PLASMARIFLE: {    // Plasma rifle
            if (!GiveWeapon(player, wp_plasma, false)) {
                return;
            }
            player.message = "You got the plasma gun!";
            sound = sfx_wpnup;
        }   break;

        case rSPR_SHOTGUN: {    // Shotgun
            if (!GiveWeapon(player, wp_shotgun, ((special.flags & MF_DROPPED) != 0) ? true : false)) {
                return;
            }
            player.message = "You got the shotgun!";
            sound = sfx_wpnup;
        }   break;

        default: {  // None of the above?
            sound = TouchSpecialThing2(toucher, i);     // Try extra code
            if (sound == -1) {                          // No good?
                return;                                 // Exit
            }
        }   break;
    }

    if ((special.flags & MF_COUNTITEM) != 0) {      // Bonus items?
        ++player.itemcount;                         // Add to the item count
    }

    P_RemoveMobj(special);              // Remove the item
    player.bonuscount += BONUSADD;      // Add to the bonus count color
    S_StartSound(&toucher.x, sound);    // Play the sound
}

//----------------------------------------------------------------------------------------------------------------------
// The mobj was just killed, adjust the totals and turn the object into death warmed over.
//----------------------------------------------------------------------------------------------------------------------
static void KillMobj(mobj_t& target, const uint32_t Overkill) noexcept {
    ASSERT(target.InfoPtr);
    const mobjinfo_t* pInfo = target.InfoPtr;
    target.flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY);  // Clear flags

    if (pInfo != &gMObjInfo[MT_SKULL]) {        // Skulls can stay in the air
        target.flags &= ~MF_NOGRAVITY;          // Make eye creatures fall
    }

    target.flags |= MF_CORPSE|MF_DROPOFF;       // It's dead and can fall
    target.height >>= 2;                        // Reduce the height a lot

    // Count all monster deaths, even those caused by other monsters
    if ((target.flags & MF_COUNTKILL) != 0) {
        ++gPlayer.killcount;
    }

    if (target.player) {                            // Was the dead one a player?
        target.flags &= ~MF_SOLID;                  // Walk over the body!
        target.player->playerstate = PST_DEAD;      // You are dead!
        LowerPlayerWeapon(*target.player);          // Drop current weapon on screen

        if (target.player == &gPlayer) {
            gStBar.gotgibbed = true;                // Gooey!
        }

        if (Overkill >= 50) {                       // Were you a real mess?
            S_StartSound(&target.x, sfx_slop);      // Juicy, gorey death!
        } else {
            S_StartSound(&target.x, sfx_pldeth);    // Arrrgh!!
        }
    }

    if (Overkill >= pInfo->spawnhealth && pInfo->xdeathstate) {     // Horrible death?
        SetMObjState(target, pInfo->xdeathstate);                   // Death state
    } else {
        SetMObjState(target, pInfo->deathstate);                    // Boring death
    }

    Sub1RandomTick(target);     // Add a little randomness to the gibbing time

    // Drop stuff
    if (pInfo == &gMObjInfo[MT_POSSESSED]) {
        pInfo = &gMObjInfo[MT_CLIP];
    } else if (pInfo == &gMObjInfo[MT_SHOTGUY]) {
        pInfo = &gMObjInfo[MT_SHOTGUN];
    } else {
        return;
    }

    mobj_t& droppedObj = SpawnMObj(target.x, target.y, ONFLOORZ, *pInfo);       // Drop it
    droppedObj.flags |= MF_DROPPED;                                             // Avoid respawning!
}

//----------------------------------------------------------------------------------------------------------------------
// Damages both enemies and players.
// Inflictor is the thing that caused the damage creature or missile, can be NULL (slime, etc).
// Source is the thing to target after taking damage (creature or NULL).
// Source and inflictor are the same for melee attacks.
// Source can be null for barrel explosions and other environmental stuff.
//----------------------------------------------------------------------------------------------------------------------
void DamageMObj(
    mobj_t& target,
    mobj_t* const pInflictor,
    mobj_t* const pSource,
    uint32_t damage
) noexcept {
    // Target must be shootable
    if ((target.flags & MF_SHOOTABLE) == 0) {
        return;
    }

    // Does nothing if target is already dead
    if (target.MObjHealth <= 0) {
        return;
    }

    // Stop a skull from flying
    if ((target.flags & MF_SKULLFLY) != 0) {
        target.momx = target.momy = target.momz = 0;
    }

    // Handle player damage
    player_t* const pPlayer = target.player;

    if (pPlayer) {
        if (gGameSkill == sk_baby) {
            damage >>= 1;   // Take half damage in trainer mode
        }
        if ((damage >= 31) && pPlayer == &gPlayer) {
            gStBar.specialFace = f_hurtbad;     // Ouch face
        }
    }

    // Kick away unless using the chainsaw
    angle_t impactAng;

    if (pInflictor && (
            (!pSource) ||
            (!pSource->player) ||
            (pSource->player->readyweapon != wp_chainsaw)
        )
    ) {
        impactAng = PointToAngle(pInflictor->x, pInflictor->y, target.x, target.y);
        Fixed thrust = (damage * (25 * FRACUNIT)) / target.InfoPtr->mass;    // Thrust of death motion

        // Make fall forwards sometimes
        if ((damage < 40) &&
            (damage > target.MObjHealth) &&
            ((target.z - pInflictor->z) > 64 * FRACUNIT) &&
            Random::nextBool()
        ) {
            impactAng += ANG180;    // Do a 180 degree turn
            thrust *= 4;            // Move a little faster
        }

        const uint32_t an = impactAng >> ANGLETOFINESHIFT;      // Index to angle table: convert to sine table value
        thrust >>= FRACBITS;                                    // Get the integer
        target.momx += thrust * gFineCosine[an];                // Get some momentum
        target.momy += thrust * gFineSine[an];
    } else {
        impactAng = target.angle;   // Get facing
    }

    // Player specific
    if (pPlayer) {
        if (((pPlayer->AutomapFlags & AF_GODMODE) != 0) || pPlayer->powers[pw_invulnerability]) {
            return;     // Don't hurt in god mode
        }

        // Where did the attack come from?
        if (pPlayer == &gPlayer) {
            impactAng -= target.angle;                                              // Get angle differance
            if (impactAng >= 0x30000000UL && impactAng < 0x80000000UL) {
                gStBar.specialFace = f_faceright;                                   // Face toward attacker
            } else if (impactAng >= 0x80000000UL && impactAng < 0xD0000000UL) {
                gStBar.specialFace = f_faceleft;
            }
        }

        // Remove damage using armor
        if (pPlayer->armortype != 0) {
            uint32_t saved;                         // Damage armor prevented
            if (pPlayer->armortype == 1) {          // Normal armor
                saved = damage / 3;
            } else {
                saved = damage / 2;                 // Mega armor
            }
            if (pPlayer->armorpoints <= saved) {    // Armor is used up
                saved = pPlayer->armorpoints;       // Use the maximum from armor
                pPlayer->armortype = 0;             // Remove the previous armor
            }
            pPlayer->armorpoints -= saved;          // Deduct from armor
            damage -= saved;                        // Deduct damage from armor
        }

        S_StartSound(&target.x, sfx_plpain);    // Ouch!
        if (pPlayer->health <= damage) {        // Mirror mobj health here for Dave
            pPlayer->health = 0;                // You died!
        } else {
            pPlayer->health -= damage;          // Remove health
        }

        pPlayer->attacker = pSource;                // Mark the source of the attack
        pPlayer->damagecount += (damage << 1);      // Add damage after armor / invuln
    }

    // Do the damage
    if (target.MObjHealth <= damage) {                          // Killed?
        const uint32_t ouch = damage - target.MObjHealth;       // Get the overkill factor
        target.MObjHealth = 0;                                  // Zap the health
        KillMobj(target, ouch);                                 // Perform death animation
        return;                                                 // Exit now
    }

    target.MObjHealth -= damage;    // Remove damage from target

    if ((Random::nextU32(255) < target.InfoPtr->painchance) &&  // Should it react in pain?
        ((target.flags & MF_SKULLFLY) == 0)
    ) {
        target.flags |= MF_JUSTHIT;     // fight back!
        SetMObjState(target, target.InfoPtr->painstate);
    }

    target.reactiontime = 0;    // We're awake now...

    // If not intent on another player, chase after this one
    if (target.threshold == 0 && pSource) {
        target.target = pSource;                // Target the attacker
        target.threshold = BASETHRESHOLD;       // Reset the threshold

        if (target.state == target.InfoPtr->spawnstate && target.InfoPtr->seestate) {
            // Reset actor
            SetMObjState(target, target.InfoPtr->seestate);
        }
    }
}
