#include "Cheats.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Input.h"
#include "Config.h"
#include "Data.h"
#include "DoomDefines.h"
#include "Things/Interactions.h"
#include "Things/MapObj.h"
#include "UI/Automap_Main.h"
#include <vector>
#include <SDL.h>

BEGIN_NAMESPACE(Cheats)

typedef void (*CheatAction)();

struct Cheat {
    const Config::CheatKeySequence*     pSequence;
    CheatAction                         pAction;
    uint32_t                            curSequenceIdx;
    uint32_t                            sequenceLength;
};

static std::vector<Cheat>   gCheats;
static bool                 gbReadyForWarpToMapCheat;
static bool                 gbReadyForMusicCheat;
static uint8_t              gWarpToMapCheatNum;
static uint8_t              gMusicCheatNum;
static uint8_t              gWarpToMapCheatNumDigitsEntered;
static uint8_t              gMusicCheatNumDigitsEntered;

//----------------------------------------------------------------------------------------------------------------------
// Converts an SDL scancode to a digit; returns '255' on failure.
//----------------------------------------------------------------------------------------------------------------------
static uint8_t sdlScanCodeToDigit(const uint16_t scanCode) {
    switch (scanCode) {
        case SDL_SCANCODE_1: return 1;
        case SDL_SCANCODE_2: return 2;
        case SDL_SCANCODE_3: return 3;
        case SDL_SCANCODE_4: return 4;
        case SDL_SCANCODE_5: return 5;
        case SDL_SCANCODE_6: return 6;
        case SDL_SCANCODE_7: return 7;
        case SDL_SCANCODE_8: return 8;
        case SDL_SCANCODE_9: return 9;
        case SDL_SCANCODE_0: return 0;
        case SDL_SCANCODE_KP_1: return 1;
        case SDL_SCANCODE_KP_2: return 2;
        case SDL_SCANCODE_KP_3: return 3;
        case SDL_SCANCODE_KP_4: return 4;
        case SDL_SCANCODE_KP_5: return 5;
        case SDL_SCANCODE_KP_6: return 6;
        case SDL_SCANCODE_KP_7: return 7;
        case SDL_SCANCODE_KP_8: return 8;
        case SDL_SCANCODE_KP_9: return 9;
        case SDL_SCANCODE_KP_0: return 0;
        
        default:
            return 255u;
    }
}

static void addCheat(const Config::CheatKeySequence& sequence, const CheatAction action) noexcept {
    Cheat& cheat = gCheats.emplace_back();
    cheat.pSequence = &sequence;
    cheat.pAction = action;
    cheat.curSequenceIdx = 0;
    cheat.sequenceLength = 0;

    for (uint32_t i = 0; i < Config::CheatKeySequence::MAX_KEYS; ++i) {
        if (sequence.keys[i] != 0) {
            ++cheat.sequenceLength;
        } else {
            break;
        }
    }
}

static void doCheatConfirmedFx() noexcept {
    S_StartSound(nullptr, sfx_itmbk);
    gPlayer.cheatFxTicksLeft = TICKSPERSEC / 2;
}

static void doToggleGodModeCheat() noexcept {
    doCheatConfirmedFx();

    // When god mode is being switched on, grant health
    if ((gPlayer.AutomapFlags & AF_GODMODE) == 0) {        
        gPlayer.health = 100;
        gPlayer.mo->MObjHealth = 100;        
    }

    gPlayer.AutomapFlags ^= AF_GODMODE;
}

static void doToggleNoClipCheat() noexcept {
    doCheatConfirmedFx();

    gPlayer.mo->flags ^= MF_NOCLIP;
    gPlayer.mo->flags ^= MF_SOLID;
    gPlayer.AutomapFlags ^= AF_NOCLIP;
}

static void doMapAndThingsRevealToggleCheat() noexcept {
    doCheatConfirmedFx();

    // 3 possible states (cheat cycles through them):
    //  (1) No lines, no things (normal)
    //  (2) Show all lines, no things
    //  (3) Show all lines, show all things
    if (gShowAllAutomapLines) {
        if (gShowAllAutomapThings) {
            gShowAllAutomapLines = false;
            gShowAllAutomapThings = false;
        } else {
            gShowAllAutomapThings = true;
        }
    } else {
        gShowAllAutomapLines = true;
    }
}

static void giveAllKeycards() noexcept {
    // 0-2 are keys, 3-5 are skulls
    for (uint32_t i = 0; i < NUMCARDS; ++i) {        
        gPlayer.cards[i] = (i < 3) ? true : false;
    }
}

static void giveFullArmor() noexcept {
    gPlayer.armorpoints = 200;      // Full armor
    gPlayer.armortype = 2;          // Mega armor
}

static void giveAllWeapons() noexcept {
    for (uint32_t i = 0; i < NUMWEAPONS; ++i) {        
        gPlayer.weaponowned[i] = true;
    }
}

static void giveAllAmmo() noexcept {
    givePlayerABackpack(gPlayer);

    for (uint32_t i = 0; i < NUMAMMO; ++i) {        
        gPlayer.ammo[i] = gPlayer.maxammo[i];
    }
}

static void doAllWeaponsAndAmmoCheat() noexcept {
    doCheatConfirmedFx();
    giveFullArmor();
    giveAllWeapons();
    giveAllAmmo();
}

static void doAllWeaponsAmmoAndKeysCheat() noexcept {
    doCheatConfirmedFx();
    giveAllKeycards();
    giveFullArmor();
    giveAllWeapons();
    giveAllAmmo();
}

static void doReadyForWarpToMapCheat() noexcept {
    gbReadyForWarpToMapCheat = true;
    gWarpToMapCheatNumDigitsEntered = 0;
    gWarpToMapCheatNum = 0;
}

static void doReadyForMusicCheat() noexcept {
    gbReadyForMusicCheat = true;
    gMusicCheatNumDigitsEntered = 0;
    gMusicCheatNum = 0;
}

static void doGrantInvisibilityCheat() noexcept {
    doCheatConfirmedFx();
    GivePower(gPlayer, pw_invisibility);
}

static void doGrantRadSuitCheat() noexcept {
    doCheatConfirmedFx();
    GivePower(gPlayer, pw_ironfeet);
}

static void doGrantBeserkCheat() noexcept {
    doCheatConfirmedFx();
    GivePower(gPlayer, pw_strength);
}

static void doGrantInvulnerabilityCheat() noexcept {
    doCheatConfirmedFx();
    GivePower(gPlayer, pw_invulnerability);
}

static void updateWarpToMapCheat() noexcept {
    if (!gbReadyForWarpToMapCheat)
        return;

    const std::vector<uint16_t>& keys = Input::getKeyboardKeysJustReleased();

    if (keys.empty())
        return;

    bool bCancelCheat = true;

    if (keys.size() == 1) {
        const uint8_t digit = sdlScanCodeToDigit(keys[0]);
        
        if (digit < 10) {
            gWarpToMapCheatNum *= 10;
            gWarpToMapCheatNum += digit;
            ++gWarpToMapCheatNumDigitsEntered;

            if (gWarpToMapCheatNumDigitsEntered >= 2) {
                if (gWarpToMapCheatNum >= 1 && gWarpToMapCheatNum <= 24) {
                    doCheatConfirmedFx();
                    gGameMap = gWarpToMapCheatNum;
                    gNextMap = gWarpToMapCheatNum;
                    gGameAction = ga_warped;
                }
            } else {
                bCancelCheat = false;
            }
        }
    }

    if (bCancelCheat) {
        gbReadyForWarpToMapCheat = false;
        gWarpToMapCheatNum = 0;
        gWarpToMapCheatNumDigitsEntered = 0;
    }
}

static void updateMusicCheat() noexcept {
    if (!gbReadyForMusicCheat)
        return;

    const std::vector<uint16_t>& keys = Input::getKeyboardKeysJustReleased();

    if (keys.empty())
        return;

    bool bCancelCheat = true;

    if (keys.size() == 1) {
        const uint8_t digit = sdlScanCodeToDigit(keys[0]);
        
        if (digit < 10) {
            gMusicCheatNum *= 10;
            gMusicCheatNum += digit;
            ++gMusicCheatNumDigitsEntered;

            if (gMusicCheatNumDigitsEntered >= 2) {
                if (gMusicCheatNum >= 0 && gMusicCheatNum <= 24) {
                    doCheatConfirmedFx();

                    if (gMusicCheatNum == 0) {
                        // Easter egg: play the hidden BUNNY track! Was not used in the original 3DO version :)
                        S_StartSong(3);
                    } else {
                        S_StartSong(4 + gMusicCheatNum);
                    }
                }
            } else {
                bCancelCheat = false;
            }
        }
    }

    if (bCancelCheat) {
        gbReadyForMusicCheat = false;
        gMusicCheatNum = 0;
        gMusicCheatNumDigitsEntered = 0;
    }
}

static void updateSequencesForKeypress(const uint16_t key) noexcept {
    for (Cheat& cheat : gCheats) {
        // Did we press the right key?
        if (key == cheat.pSequence->keys[cheat.curSequenceIdx]) {
            ++cheat.curSequenceIdx;

            if (cheat.curSequenceIdx >= cheat.sequenceLength) {
                cheat.curSequenceIdx = 0;
                cheat.pAction();
            }
        } else {
            cheat.curSequenceIdx = 0;
        }
    }
}

void init() noexcept {
    addCheat(Config::gCheatKeys_GodMode,                    doToggleGodModeCheat);
    addCheat(Config::gCheatKeys_NoClip,                     doToggleNoClipCheat);
    addCheat(Config::gCheatKeys_MapAndThingsRevealToggle,   doMapAndThingsRevealToggleCheat);
    addCheat(Config::gCheatKeys_AllWeaponsAndAmmo,          doAllWeaponsAndAmmoCheat);
    addCheat(Config::gCheatKeys_AllWeaponsAmmoAndKeys,      doAllWeaponsAmmoAndKeysCheat);
    addCheat(Config::gCheatKeys_WarpToMap,                  doReadyForWarpToMapCheat);
    addCheat(Config::gCheatKeys_ChangeMusic,                doReadyForMusicCheat);
    addCheat(Config::gCheatKeys_GrantInvisibility,          doGrantInvisibilityCheat);
    addCheat(Config::gCheatKeys_GrantRadSuit,               doGrantRadSuitCheat);
    addCheat(Config::gCheatKeys_GrantBeserk,                doGrantBeserkCheat);
    addCheat(Config::gCheatKeys_GrantInvulnerability,       doGrantInvulnerabilityCheat);

    gbReadyForWarpToMapCheat = false;
    gbReadyForMusicCheat = false;
    gWarpToMapCheatNum = 0;
    gMusicCheatNum = 0;
    gWarpToMapCheatNumDigitsEntered = 0;
    gMusicCheatNumDigitsEntered = 0;
}

void shutdown() noexcept {
    gCheats.clear();
    gCheats.shrink_to_fit();
}

void update() noexcept {
    // Update the cheat fx flash
    if (gPlayer.cheatFxTicksLeft > 0) {
        --gPlayer.cheatFxTicksLeft;
    }

    // Do these special cheats that require 2 digits after them.
    // The must also be done BEFORE updating key sequences in order to work properly.
    updateWarpToMapCheat();
    updateMusicCheat();

    // Check for cheat key sequences
    for (const uint16_t key : Input::getKeyboardKeysJustReleased()) {
        updateSequencesForKeypress(key);
    }
}

END_NAMESPACE(Cheats)
