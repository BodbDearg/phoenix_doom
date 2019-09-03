#pragma once

#include "Base/Macros.h"
#include <cstdint>

BEGIN_NAMESPACE(Controls)

//----------------------------------------------------------------------------------------------------------------------
// Available actions in the menu
//----------------------------------------------------------------------------------------------------------------------
typedef uint32_t MenuActionBits;

namespace MenuActions {
    static constexpr uint32_t NONE      = 0x00000000u;
    static constexpr uint32_t UP        = 0x00000001u;
    static constexpr uint32_t DOWN      = 0x00000002u;
    static constexpr uint32_t LEFT      = 0x00000004u;
    static constexpr uint32_t RIGHT     = 0x00000008u;
    static constexpr uint32_t OK        = 0x00000010u;
    static constexpr uint32_t BACK      = 0x00000020u;

    // N.B: the condition must be true for ALL actions given, else 'false' is returned
    bool areActive(const MenuActionBits menuActions) noexcept;
    bool areJustStarted(const MenuActionBits menuActions) noexcept;
    bool areJustEnded(const MenuActionBits menuActions) noexcept;

    // Convenience macros to shorten things
    #define MENU_ACTION(NAME)           Controls::MenuActions::areActive(Controls::MenuActions::NAME)
    #define MENU_ACTION_STARTED(NAME)   Controls::MenuActions::areJustStarted(Controls::MenuActions::NAME)
    #define MENU_ACTION_ENDED(NAME)     Controls::MenuActions::areJustEnded(Controls::MenuActions::NAME)
}

//----------------------------------------------------------------------------------------------------------------------
// Available actions in the game, except for analogue movement via a gamepad (that is hardcoded)
//----------------------------------------------------------------------------------------------------------------------
typedef uint32_t GameActionBits;

namespace GameActions {
    static constexpr uint32_t NONE                          = 0x00000000u;
    static constexpr uint32_t MOVE_FORWARD                  = 0x00000001u;
    static constexpr uint32_t MOVE_BACKWARD                 = 0x00000002u;
    static constexpr uint32_t TURN_LEFT                     = 0x00000004u;
    static constexpr uint32_t TURN_RIGHT                    = 0x00000008u;
    static constexpr uint32_t STRAFE_LEFT                   = 0x00000010u;
    static constexpr uint32_t STRAFE_RIGHT                  = 0x00000020u;
    static constexpr uint32_t ATTACK                        = 0x00000040u;
    static constexpr uint32_t USE                           = 0x00000080u;
    static constexpr uint32_t RUN                           = 0x00000100u;
    static constexpr uint32_t NEXT_WEAPON                   = 0x00000200u;
    static constexpr uint32_t PREV_WEAPON                   = 0x00000400u;
    static constexpr uint32_t PAUSE                         = 0x00000800u;
    static constexpr uint32_t WEAPON_1                      = 0x00001000u;
    static constexpr uint32_t WEAPON_2                      = 0x00002000u;
    static constexpr uint32_t WEAPON_3                      = 0x00004000u;
    static constexpr uint32_t WEAPON_4                      = 0x00008000u;
    static constexpr uint32_t WEAPON_5                      = 0x00010000u;
    static constexpr uint32_t WEAPON_6                      = 0x00020000u;
    static constexpr uint32_t WEAPON_7                      = 0x00040000u;
    static constexpr uint32_t AUTOMAP_TOGGLE                = 0x00080000u;
    static constexpr uint32_t AUTOMAP_FREE_CAM_TOGGLE       = 0x00100000u;
    static constexpr uint32_t AUTOMAP_FREE_CAM_ZOOM_IN      = 0x00200000u;
    static constexpr uint32_t AUTOMAP_FREE_CAM_ZOOM_OUT     = 0x00400000u;
    static constexpr uint32_t OPTIONS                       = 0x00800000u;

    // N.B: the condition must be true for ALL actions given, else 'false' is returned
    bool areActive(const GameActionBits gameActions) noexcept;
    bool areJustStarted(const GameActionBits gameActions) noexcept;
    bool areJustEnded(const GameActionBits gameActions) noexcept;

    // Convenience macros to shorten things
    #define GAME_ACTION(NAME)           Controls::GameActions::areActive(Controls::GameActions::NAME)
    #define GAME_ACTION_STARTED(NAME)   Controls::GameActions::areJustStarted(Controls::GameActions::NAME)
    #define GAME_ACTION_ENDED(NAME)     Controls::GameActions::areJustEnded(Controls::GameActions::NAME)
}

// Startup and shutdown control processing
void init() noexcept;
void shutdown() noexcept;

// Updates what actions are currently active, have just been activated or deactivated
void update() noexcept;

END_NAMESPACE(Controls)
