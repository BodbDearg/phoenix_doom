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

static void updateActionsFromKeyboardInput() noexcept {
    // Add to actions currently active
    {
        const std::vector<uint16_t>& keyboardKeysPressed = Input::getKeyboardKeysPressed();

        for (uint16_t scancode : keyboardKeysPressed) {
            const uint32_t scancodeIdx = (uint32_t) scancode;

            if (scancodeIdx < Input::NUM_KEYBOARD_KEYS) {
                gGameActionsActive |= Config::gKeyboardGameActions[scancodeIdx];
                gMenuActionsActive |= Config::gKeyboardMenuActions[scancodeIdx];
            }
        }
    }

    // Add to actions just started
    {
        const std::vector<uint16_t>& keyboardKeysJustPressed = Input::getKeyboardKeysJustPressed();

        for (uint16_t scancode : keyboardKeysJustPressed) {
            const uint32_t scancodeIdx = (uint32_t) scancode;

            if (scancodeIdx < Input::NUM_KEYBOARD_KEYS) {
                gGameActionsJustStarted |= Config::gKeyboardGameActions[scancodeIdx];
                gMenuActionsJustStarted |= Config::gKeyboardMenuActions[scancodeIdx];
            }
        }
    }

    // Add to actions just ended
    {
        const std::vector<uint16_t>& keyboardKeysJustReleased = Input::getKeyboardKeysJustReleased();

        for (uint16_t scancode : keyboardKeysJustReleased) {
            const uint32_t scancodeIdx = (uint32_t) scancode;

            if (scancodeIdx < Input::NUM_KEYBOARD_KEYS) {
                gGameActionsJustEnded |= Config::gKeyboardGameActions[scancodeIdx];
                gMenuActionsJustEnded |= Config::gKeyboardMenuActions[scancodeIdx];
            }
        }
    }
}

static void updateActionsFromControllerInput() noexcept {
    // Add to actions currently active
    {
        const std::vector<ControllerInput>& inputsPressed = Input::getControllerInputsPressed();

        for (ControllerInput input : inputsPressed) {
            const uint8_t inputIdx = (uint8_t) input;

            if (inputIdx < NUM_CONTROLLER_INPUTS) {
                gGameActionsActive |= Config::gGamepadGameActions[inputIdx];
                gMenuActionsActive |= Config::gGamepadMenuActions[inputIdx];
            }
        }
    }

    // Add to actions just started
    {
        const std::vector<ControllerInput>& inputsJustPressed = Input::getControllerInputsJustPressed();

        for (ControllerInput input : inputsJustPressed) {
            const uint8_t inputIdx = (uint8_t) input;

            if (inputIdx < NUM_CONTROLLER_INPUTS) {
                gGameActionsJustStarted |= Config::gGamepadGameActions[inputIdx];
                gMenuActionsJustStarted |= Config::gGamepadMenuActions[inputIdx];
            }
        }
    }

    // Add to actions just ended
    {
        const std::vector<ControllerInput>& inputsJustReleased = Input::getControllerInputsJustReleased();

        for (ControllerInput input : inputsJustReleased) {
            const uint8_t inputIdx = (uint8_t) input;

            if (inputIdx < NUM_CONTROLLER_INPUTS) {
                gGameActionsJustEnded |= Config::gGamepadGameActions[inputIdx];
                gMenuActionsJustEnded |= Config::gGamepadMenuActions[inputIdx];
            }
        }
    }
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
    updateActionsFromKeyboardInput();
    updateActionsFromControllerInput();
}

END_NAMESPACE(Controls)
