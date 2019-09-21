#include "User.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Tables.h"
#include "Game/Config.h"
#include "Game/Data.h"
#include "GFX/Renderer.h"
#include "Info.h"
#include "Map/Map.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "Map/Specials.h"
#include "MapObj.h"
#include "Slide.h"
#include "UI/StatusBarUI.h"
#include <algorithm>

static constexpr Fixed MAXBOB           = 16 << FRACBITS;   // 16 pixels of bobbing up and down
static constexpr float MOUSE_TURN_SCALE = 1.0f / 15.0f;     // Controls how much mouse movement translates into turning (can be multiplied by the user through config also)

static constexpr Fixed FORWARD_MOVE[2] = {
    0x38000 >> 2,
    0x60000 >> 2
};

static constexpr Fixed SIDE_MOVE[2] = {
    0x38000 >> 2,
    0x58000 >> 2
};

static constexpr Fixed FAST_ANGLE_TURN[] = {
    600 << FRACBITS,
    600 << FRACBITS,
    650 << FRACBITS,
    700 << FRACBITS,
    700 << FRACBITS,
    750 << FRACBITS,
    750 << FRACBITS,
    800 << FRACBITS,
    850 << FRACBITS,
    850 << FRACBITS,
    900 << FRACBITS
};

static constexpr Fixed ANGLE_TURN[] = {
    300 << FRACBITS,
    300 << FRACBITS,
    350 << FRACBITS,
    400 << FRACBITS,
    450 << FRACBITS,
    450 << FRACBITS,
    500 << FRACBITS,
    500 << FRACBITS,
    550 << FRACBITS,
    550 << FRACBITS,
    600 << FRACBITS
};

static bool gbOnGround;     // True if the player is on the ground

//----------------------------------------------------------------------------------------------------------------------
// Move the player along the momentum
//----------------------------------------------------------------------------------------------------------------------
static void P_PlayerMove(mobj_t& mo) noexcept {
    Fixed momx = mo.momx >> 2;      // Get the momemtum
    Fixed momy = mo.momy >> 2;

    // DC: added a noclip cheat
    if ((mo.flags & MF_NOCLIP) != 0) {
        Slide::gSlideX = mo.x + momx * 4;
        Slide::gSlideY = mo.y + momy * 4;
    } else {
        Slide::doSliding(mo);   // Slide the player ahead
    }

    if (Slide::gSlideX != mo.x || Slide::gSlideY != mo.y) {         // No motion at all?
        if (P_TryMove(mo, Slide::gSlideX, Slide::gSlideY)) {        // Can I move?
            goto dospecial;                                         // Movement OK, just special and exit
        }
    }

    if (momx > MAXMOVE) {       // Clip the momentum to maximum
        momx = MAXMOVE;
    }
    if (momx < -MAXMOVE) {
        momx = -MAXMOVE;
    }
    if (momy > MAXMOVE) {
        momy = MAXMOVE;
    }
    if (momy < -MAXMOVE) {
        momy = -MAXMOVE;
    }

    // Since I can't either slide or directly go to my destination point,
    // stairstep to the closest point to the wall:
    if (P_TryMove(mo, mo.x, mo.y + momy)) {             // Try Y motion only
        mo.momx = 0;                                    // Kill the X motion
        mo.momy = momy;                                 // Keep the Y motion
    } else if (P_TryMove(mo, mo.x + momx, mo.y)) {      // Try X motion only
        mo.momx = momx;                                 // Keep the X motion
        mo.momy = 0;                                    // Kill the Y motion
    } else {
        mo.momx = mo.momy = 0;                          // No more sliding
    }

dospecial:
    if (Slide::gpSpecialLine) {                             // Did a line get crossed?
        P_CrossSpecialLine(*Slide::gpSpecialLine, mo);      // Call the special event
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Move the player in the X and Y directions
//----------------------------------------------------------------------------------------------------------------------
static constexpr Fixed STOPSPEED    = 0x1000;   // Speed to stop at
static constexpr Fixed FRICTION     = 0xD240;   // 841/1024 0.821289 Friction rate

static void P_PlayerXYMovement(mobj_t& mo) noexcept {
    P_PlayerMove(mo);   // Move the player

    // Slow down
    if (mo.z <= mo.floorz) {                                            // Touching the floor?
        if ((mo.flags & MF_CORPSE) != 0) {                              // Corpse's will fall off
            if (mo.floorz != mo.subsector->sector->floorheight) {
                return;                                                 // Don't stop halfway off a step
            }
        }

        if ((mo.momx > -STOPSPEED) &&   // Too slow?
            (mo.momx < STOPSPEED) &&
            (mo.momy > -STOPSPEED) &&
            (mo.momy < STOPSPEED)
        ) {
            mo.momx = 0;    // Kill momentum
            mo.momy = 0;
        } else {
            mo.momx = fixed16Mul(mo.momx, FRICTION);    // Slow down
            mo.momy = fixed16Mul(mo.momy, FRICTION);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Handle motion in the Z axis (Falling or gravity)
//----------------------------------------------------------------------------------------------------------------------
static void P_PlayerZMovement(mobj_t& mo) noexcept {
    // Check for smooth step up
    if (mo.z < mo.floorz) {                             // My z lower than the floor?
        mo.player->viewheight -= mo.floorz - mo.z;      // Adjust the view for floor
        mo.player->deltaviewheight = (VIEWHEIGHT - mo.player->viewheight) >> 2;
    }
    mo.z += mo.momz;    // Apply gravity to the player's z

    // Clip movement
    if (mo.z <= mo.floorz) {                                    // Hit the floor
        if (mo.momz < 0) {                                      // Going down?
            if (mo.momz < - GRAVITY * 14) {                     // Squat down (Hit hard!)
                mo.player->deltaviewheight = mo.momz >> 3;
                S_StartSound(&mo.x, sfx_oof);                   // Ouch!
            }
            mo.momz = 0;    // Stop the fall
        }
        mo.z = mo.floorz;   // Set the proper z
    } else {
        if (mo.momz == 0) {             // No fall?
            mo.momz = -GRAVITY * 2;     // Apply a little gravity
        } else {
            mo.momz -= GRAVITY;         // Add some more gravity
        }
    }

    if (mo.z + mo.height > mo.ceilingz) {   // Hit the ceiling?
        if (mo.momz > 0) {                  // Going up?
            mo.momz = 0;                    // Stop the motion
        }
        mo.z = mo.ceilingz - mo.height;     // Peg at the ceiling
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Cycle through the game state logic so that the animation frames drawn are the rate of motion.
//----------------------------------------------------------------------------------------------------------------------
static void P_PlayerMobjThink(mobj_t& mobj) noexcept {
    // Momentum movement
    P_PlayerXYMovement(mobj);
    P_PlayerZMovement(mobj);

    // Cycle through states, calling action functions at transitions
    if (mobj.tics != UINT32_MAX) {
        if (mobj.tics > 1) {    // Time to cycle?
            --mobj.tics;
            return;             // Not time to cycle yet
        }

        mobj.tics = 0;                                          // Reset the tic count
        const state_t* const pState = mobj.state->nextstate;    // Get the next state index
        ASSERT(pState);
        mobj.state = pState;
        mobj.tics = pState->Time;                               // Reset the timer ticks
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Convert joypad inputs into player motion
//----------------------------------------------------------------------------------------------------------------------
static void P_BuildMove(player_t& player) noexcept {
    const bool bCanMoveAndTurn = (
        (!player.isOptionsMenuActive()) && (
            (!player.isAutomapActive()) ||
            (player.isAutomapFollowModeActive())
        )
    );

    const bool bIsRunning = (gbAlwaysRun || GAME_ACTION(RUN));

    if (bCanMoveAndTurn) {
        // Use two stage accelerative turning on the joypad.
        // If none of the turn keys are pressed then reset acceleration.
        uint32_t turnIndex = player.turnheld + 1;

        const bool bResetTurnAcceleration = (
            (!GAME_ACTION(TURN_LEFT)) &&
            (!GAME_ACTION(TURN_RIGHT)) &&
            (INPUT_AXIS(TURN_LEFT_RIGHT) == 0.0f)
        );

        if (bResetTurnAcceleration) {
            turnIndex = 0;
        }
    
        if (turnIndex >= C_ARRAY_SIZE(FAST_ANGLE_TURN)) {   // Detect overflow
            turnIndex = C_ARRAY_SIZE(FAST_ANGLE_TURN) - 1;
        }

        player.turnheld = turnIndex;

        // Is the player running?
        const uint32_t speedIndex = bIsRunning ? 1 : 0;

        // Do side stepping
        float sideMoveFracF = INPUT_AXIS(STRAFE_LEFT_RIGHT);
        
        if (GAME_ACTION(STRAFE_LEFT)) {
            sideMoveFracF -= 1.0f;
        }

        if (GAME_ACTION(STRAFE_RIGHT)) {
            sideMoveFracF += 1.0f;
        }

        sideMoveFracF = std::max(sideMoveFracF, -1.0f);
        sideMoveFracF = std::min(sideMoveFracF, 1.0f);

        const Fixed sideMoveFrac = floatToFixed16(sideMoveFracF);
        player.sidemove = fixed16Mul(sideMoveFrac, SIDE_MOVE[speedIndex]);

        // Do turning
        float angleTurnFracF = -INPUT_AXIS(TURN_LEFT_RIGHT);
        angleTurnFracF -= Input::getMouseXMovement() * MOUSE_TURN_SCALE * Config::gMouseTurnSensitivity;

        if (GAME_ACTION(TURN_LEFT)) {
            angleTurnFracF += 1.0f;
        }

        if (GAME_ACTION(TURN_RIGHT)) {
            angleTurnFracF -= 1.0f;
        }

        const Fixed angleTurnFrac = floatToFixed16(angleTurnFracF);

        if ((speedIndex != 0) && 
            (!GAME_ACTION(MOVE_FORWARD)) &&
            (!GAME_ACTION(MOVE_BACKWARD)) &&
            (INPUT_AXIS(MOVE_FORWARD_BACK) == 0.0f) &&
            (INPUT_AXIS(STRAFE_LEFT_RIGHT) == 0.0f)
        ) {
            player.angleturn = (angle_t) fixed16Mul(angleTurnFrac, FAST_ANGLE_TURN[turnIndex]);
        } else {
            player.angleturn = (angle_t) fixed16Mul(angleTurnFrac, ANGLE_TURN[turnIndex]);
        }

        // Do move forward and backward
        float forwardMoveFracF = -INPUT_AXIS(MOVE_FORWARD_BACK);
        
        if (GAME_ACTION(MOVE_FORWARD)) {
            forwardMoveFracF += 1.0f;
        }

        if (GAME_ACTION(MOVE_BACKWARD)) {
            forwardMoveFracF -= 1.0f;
        }

        forwardMoveFracF = std::max(forwardMoveFracF, -1.0f);
        forwardMoveFracF = std::min(forwardMoveFracF, 1.0f);

        const Fixed forwardMoveFrac = floatToFixed16(forwardMoveFracF);
        player.forwardmove = fixed16Mul(forwardMoveFrac, FORWARD_MOVE[speedIndex]);

        // Debug camera movement
        if (Config::gbAllowDebugCameraUpDownMovement) {
            if (GAME_ACTION(DEBUG_MOVE_CAMERA_UP)) {
                Renderer::gDebugCameraZOffset += 2.0f;
            }

            if (GAME_ACTION(DEBUG_MOVE_CAMERA_DOWN)) {
                Renderer::gDebugCameraZOffset -= 2.0f;
            }
        }
    }
    else {
        // In the options menu or automap follow mode off!
        player.turnheld = 0;
        player.angleturn = 0;
        player.sidemove = 0;
        player.forwardmove = 0;
    }

    // If slowed down to a stop, change to a standing frame
    ASSERT(player.mo);
    mobj_t& mo = *player.mo;

    if (mo.momx == 0 && mo.momy == 0 && (!player.forwardmove) && (!player.sidemove)) {
        // If in a walking frame, stop moving
        const state_t* const pState = mo.state;

        if ((pState == &gStates[S_PLAY_RUN1]) ||
            (pState == &gStates[S_PLAY_RUN2]) ||
            (pState == &gStates[S_PLAY_RUN3]) ||
            (pState == &gStates[S_PLAY_RUN4])
        ) {
            SetMObjState(mo, gStates[S_PLAY]);      // Standing frame
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Moves the given origin along a given angle
//----------------------------------------------------------------------------------------------------------------------
static void PlayerThrust(mobj_t& mObj, angle_t angle, Fixed move) noexcept {
    if (move != 0) {
        angle >>= ANGLETOFINESHIFT;                         // Convert to index to table
        move >>= FRACBITS - 8;                              // Convert to integer (With 8 bit frac)
        mObj.momx += (move * gFineCosine[angle]) >> 8;      // Add momentum
        mObj.momy += (move * gFineSine[angle]) >> 8;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Calculate the walking / running height adjustment; this will bob the camera up and down.
// Note : I MUST calculate the bob value or the gun will not be locked onto the player's view properly!!
//----------------------------------------------------------------------------------------------------------------------
void PlayerCalcHeight(player_t& player) noexcept {
    // Regular movement bobbing (needs to be calculated for gun swing even if not on ground)
    const Fixed momX = player.mo->momx;
    const Fixed momY = player.mo->momy;

    constexpr Fixed BOB_SCALE = floatToFixed16(0.25f * (60.0f / 35.0f));    // Note: 60/35 adjustment to account for different frame rate from PC
    Fixed bob = fixed16Mul(momX, momX) + fixed16Mul(momY, momY);
    bob = fixed16Mul(bob, BOB_SCALE);

    if (bob > MAXBOB) {
        bob = MAXBOB;
    }

    player.bob = bob;   // Save the new vertical adjustment

    if (!gbOnGround) {
        // Don't bob the view if in the air: zap the bob factor
        bob = 0;
    } else {
        // Calculate the viewing angle offset since the camera is now bobbing up and down. The multiply constant is based
        // on a movement of a complete cycle bob every 0.666 seconds or 2/3 * TICKSPERSEC or 40 ticks (60 ticks a sec).
        // This is translated to 8192/40 or 204.8 angles per tick. Neat eh?
        constexpr uint32_t TICK_FREQ = (2 * TICKSPERSEC) / 3;
        const uint32_t angle = ((FINEANGLES / TICK_FREQ) * gTotalGameTicks) & FINEMASK;
        bob = fixed16Mul(bob / 2, gFineSine[angle]);

        if (player.playerstate == PST_LIVE) {                   // If the player is alive...
            Fixed deltaViewHeight = player.deltaviewheight;     // Get gravity
            player.viewheight += deltaViewHeight;               // Adjust to delta

            if (player.viewheight > VIEWHEIGHT) {
                player.viewheight = VIEWHEIGHT;     // Too high!
                deltaViewHeight = 0;                // Kill delta
            }

            if (player.viewheight < VIEWHEIGHT / 2) {   // Too low?
                player.viewheight = VIEWHEIGHT / 2;     // Set the lowest
                if (deltaViewHeight <= 0) {             // Minimum squat is 1
                    deltaViewHeight = 1;
                }
            }

            if (deltaViewHeight != 0) {             // Going down?
                deltaViewHeight += FRACUNIT / 2;    // Increase the drop speed
                if (deltaViewHeight == 0) {         // Zero is special case
                    deltaViewHeight = 1;            // Make sure it's not zero!
                }
            }

            player.deltaviewheight = deltaViewHeight;
        }
    }

    Fixed viewZ = player.mo->z + player.viewheight + bob;       // Set the view z
    const Fixed top = player.mo->ceilingz - (4 * FRACUNIT);     // Adjust for ceiling height

    if (viewZ > top) {       // Did I hit my head on the ceiling?
        viewZ = top;         // Peg at the ceiling
    }

    player.viewz = viewZ;   // Set the new z coord
}

//----------------------------------------------------------------------------------------------------------------------
// Take all the motion constants and apply it to the player. Allow clipping and bumping.
//----------------------------------------------------------------------------------------------------------------------
static void MoveThePlayer(player_t& player) noexcept {
    ASSERT(player.mo);
    mobj_t& mObj = *player.mo;                                  // Cache the MObj
    const angle_t newangle = mObj.angle + player.angleturn;     // Get the angle; adjust turning angle always
    mObj.angle = newangle;                                      // Save it, but keep for future use

    // Don't let the player control movement if not onground.
    // Save this for 'PlayerCalcHeight':
    gbOnGround = (mObj.z <= mObj.floorz);

    if (gbOnGround) {   // Can I move?
        PlayerThrust(mObj, newangle, player.forwardmove);
        PlayerThrust(mObj, newangle - ANG90, player.sidemove);
    }

    // Am I moving and in normal play?
    if ((player.forwardmove || player.sidemove) && (mObj.state == &gStates[S_PLAY])) {
        SetMObjState(mObj, gStates[S_PLAY_RUN1]);   // Set the sprite
    }
}

//----------------------------------------------------------------------------------------------------------------------
// I am dead, just view the critter or player that killed me.
//----------------------------------------------------------------------------------------------------------------------

// Move in increments of 5 degrees
static constexpr angle_t ANG5 = ANG90 / 18;

static void P_DeathThink(player_t& player) noexcept {
    // Animate the weapon sprites and shoot if possible
    MovePSprites(player);

    // Fall to the ground
    if (player.viewheight > 8 * FRACUNIT) {         // Still above the ground
        player.viewheight -= FRACUNIT;              // Fall over
        if (player.viewheight < 8 * FRACUNIT) {     // Too far down?
            player.viewheight = 8 * FRACUNIT;       // Set to the bottom
        }
    }

    gbOnGround = (player.mo->z <= player.mo->floorz);       // Get the floor state
    PlayerCalcHeight(player);                               // Calc the height of the player

    // Only face killer if I didn't kill myself or jumped into lava
    if (player.attacker && player.attacker != player.mo) {
        const angle_t angle = PointToAngle(
            player.mo->x,
            player.mo->y,
            player.attacker->x,
            player.attacker->y
        );

        const angle_t delta = angle - player.mo->angle;     // Get differance

        if (delta < ANG5 || delta >= negateAngle(ANG5)) {
            // Looking at killer, so fade damage flash down
            player.mo->angle = angle;       // Set the angle
            goto DownDamage;                // Fade down
        } else if (delta < ANG180) {        // Which way?
            player.mo->angle += ANG5;       // Turn towards the killer
        } else {
            player.mo->angle -= ANG5;
        }
    } else {
    DownDamage:
        if (player.damagecount) {                           // Fade down the redness on the screen
            --player.damagecount;                           // Count down time
            if ((player.damagecount & 0x8000) != 0) {       // Negative
                player.damagecount=0;                       // Force zero
            }
        }
    }

    // Respawn keys pressed?
    if (GAME_ACTION_ENDED(USE) || GAME_ACTION_ENDED(ATTACK) || MENU_ACTION_ENDED(OK)) {
        // Only allow if the player height is below a certain threshold
        if (player.viewheight < 8 * FRACUNIT + 1) {
            player.playerstate = PST_REBORN;    // Restart the player
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Function to adjust for wraparound since the pending weapons are added with 1 or -1
//----------------------------------------------------------------------------------------------------------------------
static weapontype_e WrapAroundWeaponType(const weapontype_e weapon) noexcept {
    const int8_t weaponIdx = (int8_t) weapon;

    if (weaponIdx < 0) {
        return (weapontype_e)(NUMWEAPONS - 1);
    }
    else if (weaponIdx >= NUMWEAPONS) {
        return (weapontype_e) 0;
    }
    else {
        return weapon;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Check if the requested weapon is present
//----------------------------------------------------------------------------------------------------------------------
static bool WeaponAllowed(player_t& player, const weapontype_e weapon) noexcept {    
    const uint8_t weaponIdx = (uint8_t) WrapAroundWeaponType(weapon);
    return (player.weaponowned[weaponIdx]);     // Do I have this?
}

static bool WeaponAllowedAndDifferent(player_t& player, const weapontype_e weapon) noexcept {    
    return (
        (weapon != player.pendingweapon) &&
        (weapon != player.readyweapon) &&
        WeaponAllowed(player, weapon)
    );
}

static bool PendingWeaponAllowed(player_t& player) noexcept {
    player.pendingweapon = WrapAroundWeaponType(player.pendingweapon);  // Handle wrap around for weapon
    return WeaponAllowed(player, player.pendingweapon);
}

//----------------------------------------------------------------------------------------------------------------------
// Handle controls that change player weapons
//----------------------------------------------------------------------------------------------------------------------
static void doWeaponChangeControls(player_t& player) noexcept {
    if (player.pendingweapon == wp_nochange) {
        // Next/previous weapon change both from buttons and the next/prev weapon axis
        int8_t weaponCycleDir = 0; 

        if (GAME_ACTION_ENDED(NEXT_WEAPON)) {
            weaponCycleDir += 1;
        }

        if (GAME_ACTION_ENDED(PREV_WEAPON)) {
            weaponCycleDir -= 1;
        }

        {
            float weaponUpDownAxis = INPUT_AXIS(WEAPON_NEXT_PREV);

            if (weaponUpDownAxis <= -Config::gInputAnalogToDigitalThreshold) {
                weaponCycleDir = -1;
            } else if (weaponUpDownAxis >= Config::gInputAnalogToDigitalThreshold) {
                weaponCycleDir = +1;
            }
        }

        if (weaponCycleDir != 0) {                          // Pressed the shifts?            
            player.pendingweapon = player.readyweapon;      // Init the weapon

            // Cycle to next weapon
            do {
                player.pendingweapon = (weapontype_e)(player.pendingweapon + weaponCycleDir);
            } while (!PendingWeaponAllowed(player));    // Ok to keep?
        }
        else {
            // If not cycling then try a specific weapon change.
            // Note: only allow the change if we are actually changing to a different weapon!
            if (GAME_ACTION_ENDED(WEAPON_1) && (WeaponAllowedAndDifferent(player, wp_fist) || WeaponAllowedAndDifferent(player, wp_chainsaw))) {
                // Prefer to switch to the chainsaw first over fist, unless we have beserk...
                const bool bBeserk = (player.powers[pw_strength] > 0);
                const weapontype_e firstPref = (bBeserk) ? wp_fist : wp_chainsaw;
                const weapontype_e secondPref = (bBeserk) ? wp_chainsaw : wp_fist;

                if (WeaponAllowedAndDifferent(player, firstPref)) {
                    player.pendingweapon = firstPref;
                }
                else if (WeaponAllowedAndDifferent(player, secondPref)) {
                    player.pendingweapon = secondPref;
                }
            }
            else if (GAME_ACTION_ENDED(WEAPON_2) && WeaponAllowedAndDifferent(player, wp_pistol)) {
                player.pendingweapon = wp_pistol;
            }
            else if (GAME_ACTION_ENDED(WEAPON_3) && WeaponAllowedAndDifferent(player, wp_shotgun)) {
                player.pendingweapon = wp_shotgun;
            }
            else if (GAME_ACTION_ENDED(WEAPON_4) && WeaponAllowedAndDifferent(player, wp_chaingun)) {
                player.pendingweapon = wp_chaingun;
            }
            else if (GAME_ACTION_ENDED(WEAPON_5) && WeaponAllowedAndDifferent(player, wp_missile)) {
                player.pendingweapon = wp_missile;
            }
            else if (GAME_ACTION_ENDED(WEAPON_6) && WeaponAllowedAndDifferent(player, wp_plasma)) {
                player.pendingweapon = wp_plasma;
            }
            else if (GAME_ACTION_ENDED(WEAPON_7) && WeaponAllowedAndDifferent(player, wp_bfg)) {
                player.pendingweapon = wp_bfg;
            }
        }
    }    
}

//----------------------------------------------------------------------------------------------------------------------
// Perform all the actions to move the player by reading the joypad information and acting upon it.
// (Called from Tick.c)
//----------------------------------------------------------------------------------------------------------------------
void P_PlayerThink(player_t& player) noexcept {
    ASSERT(player.mo);
    P_PlayerMobjThink(*player.mo);      // Perform the inertia movement
    P_BuildMove(player);                // Convert joypad info to motion

    // Toggle always run if the right key is pressed
    if (GAME_ACTION_ENDED(TOGGLE_ALWAYS_RUN)) {
        gbAlwaysRun = (!gbAlwaysRun);
    }

    // I use MF_JUSTATTACKED when the chainsaw is being used
    if (player.mo->flags & MF_JUSTATTACKED) {       // Chainsaw attack?
        player.angleturn = 0;                       // Don't allow player turning.
        player.forwardmove = 0xc800;                // Moving into the enemy
        player.sidemove = 0;                        // Don't move side to side
        player.mo->flags &= ~MF_JUSTATTACKED;       // Clear the flag
    }

    if (player.playerstate == PST_DEAD) {   // Am I dead?
        P_DeathThink(player);               // Just face your killer
        return;                             // Exit now
    }

    // Reactiontime is used to prevent movement for a bit after a teleport
    if (player.mo->reactiontime <= 0) {
        MoveThePlayer(player);
    } else {
        --player.mo->reactiontime;
    }

    PlayerCalcHeight(player);      // Adjust the player's z coord
    
    if (!player.isOptionsMenuActive()) {
        // Handle special sectors
        ASSERT(player.mo->subsector);
        sector_t& sector = *player.mo->subsector->sector;   // Get sector I'm standing on
        if (sector.special) {                               // Am I standing on a special?
            PlayerInSpecialSector(player, sector);          // Process special event
        }

        // Process weapon changes
        doWeaponChangeControls(player);

        // Was use just pressed? If so then process.
        if (GAME_ACTION_ENDED(USE)) {
            P_UseLines(player);
        }

        // Process weapon attacks
        if (GAME_ACTION(ATTACK)) {
            ++player.attackdown;
            if (player.attackdown >= TICKSPERSEC * 2) {
                gStBar.specialFace = f_mowdown;
            }
        } else {
            player.attackdown = 0;
        }

        // Process the weapon sprites and shoot
        MovePSprites(player);
    }

    // Timed counters
    if (!player.isOptionsMenuActive()) {
        if (player.powers[pw_strength] && player.powers[pw_strength] < 255) {
            // Strength counts up to diminish fade
            ++player.powers[pw_strength];               // Add some time
            if (player.powers[pw_strength] >= 256) {    // Time up?
                player.powers[pw_strength] = 255;       // Maximum
            }
        }

        // Count down timers for powers and screen colors
        if (player.powers[pw_invulnerability]) {    // God mode
            --player.powers[pw_invulnerability];
            if (player.powers[pw_invulnerability] & 0x8000) {
                player.powers[pw_invulnerability] = 0;
            }
        }

        if (player.powers[pw_invisibility]) {   // Invisible?
            --player.powers[pw_invisibility];
            if (player.powers[pw_invisibility] & 0x8000) {
                player.powers[pw_invisibility] = 0;
            }
            if (!player.powers[pw_invisibility]) {
                player.mo->flags &= ~MF_SHADOW;
            }
        }

        if (player.powers[pw_ironfeet]) {   // Radiation suit
            --player.powers[pw_ironfeet];
            if (player.powers[pw_ironfeet] & 0x8000) {
                player.powers[pw_ironfeet] = 0;
            }
        }

        if (player.damagecount) {   // Red factor
            --player.damagecount;
            if (player.damagecount & 0x8000) {
                player.damagecount = 0;
            }
        }

        if (player.bonuscount) {    // Gold factor
            --player.bonuscount;
            if (player.bonuscount & 0x8000) {
                player.bonuscount = 0;
            }
        }
    }
}
