#include "Controls.h"

#include "Config.h"

BEGIN_NAMESPACE(Controls)

static GameActionBits gGameActionsActive;
static GameActionBits gGameActionsJustStarted;
static GameActionBits gGameActionsJustEnded;
static MenuActionBits gMenuActionsActive;
static MenuActionBits gMenuActionsJustStarted;
static MenuActionBits gMenuActionsJustEnded;

static void clearAllActionBits() noexcept {
    gGameActionsActive       = GameActions::NONE;
    gGameActionsJustStarted  = GameActions::NONE;
    gGameActionsJustEnded    = GameActions::NONE;
    gMenuActionsActive       = MenuActions::NONE;
    gMenuActionsJustStarted  = MenuActions::NONE;
    gMenuActionsJustEnded    = MenuActions::NONE;
}

bool GameActions::areActive(const GameActionBits gameActions) noexcept {
    return ((gGameActionsActive & gameActions) == gameActions);
}

bool GameActions::areJustStarted(const GameActionBits gameActions) noexcept {
    return ((gGameActionsJustStarted & gameActions) == gameActions);
}

bool GameActions::areJustEnded(const GameActionBits gameActions) noexcept {
    return ((gGameActionsJustEnded & gameActions) == gameActions);
}

bool MenuActions::areActive(const MenuActionBits menuActions) noexcept {
    return ((gMenuActionsActive & menuActions) == menuActions);
}

bool MenuActions::areJustStarted(const MenuActionBits menuActions) noexcept {
    return ((gMenuActionsJustStarted & menuActions) == menuActions);
}

bool MenuActions::areJustEnded(const MenuActionBits menuActions) noexcept {
    return ((gMenuActionsJustEnded & menuActions) == menuActions);
}

void init() noexcept {
    clearAllActionBits();
}

void shutdown() noexcept {
    clearAllActionBits();
}

void update() noexcept {
    clearAllActionBits();

    // Keyboard input: actions currently active
    const std::vector<uint16_t>& keyboardKeysPressed = Input::getKeyboardKeysPressed();

    for (uint16_t scancode : keyboardKeysPressed) {
        const uint32_t scancodeIdx = (uint32_t) scancode;

        if (scancodeIdx < Input::NUM_KEYBOARD_KEYS) {
            gGameActionsActive |= Config::gKeyboardGameActions[scancodeIdx];
            gMenuActionsActive |= Config::gKeyboardMenuActions[scancodeIdx];
        }
    }

    // Keyboard input: actions just started
    const std::vector<uint16_t>& keyboardKeysJustPressed = Input::getKeyboardKeysJustPressed();

    for (uint16_t scancode : keyboardKeysJustPressed) {
        const uint32_t scancodeIdx = (uint32_t) scancode;

        if (scancodeIdx < Input::NUM_KEYBOARD_KEYS) {
            gGameActionsJustStarted |= Config::gKeyboardGameActions[scancodeIdx];
            gMenuActionsJustStarted |= Config::gKeyboardMenuActions[scancodeIdx];
        }
    }

    // Keyboard input: actions just ended
    const std::vector<uint16_t>& keyboardKeysJustReleased = Input::getKeyboardKeysJustReleased();

    for (uint16_t scancode : keyboardKeysJustReleased) {
        const uint32_t scancodeIdx = (uint32_t) scancode;

        if (scancodeIdx < Input::NUM_KEYBOARD_KEYS) {
            gGameActionsJustEnded |= Config::gKeyboardGameActions[scancodeIdx];
            gMenuActionsJustEnded |= Config::gKeyboardMenuActions[scancodeIdx];
        }
    }
}

END_NAMESPACE(Controls)
