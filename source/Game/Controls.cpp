#include "Controls.h"

#include "Config.h"

BEGIN_NAMESPACE(Controls)

static GameActionBits   gGameActionsActive;
static GameActionBits   gGameActionsJustStarted;
static GameActionBits   gGameActionsJustEnded;
static MenuActionBits   gMenuActionsActive;
static MenuActionBits   gMenuActionsJustStarted;
static MenuActionBits   gMenuActionsJustEnded;

static float gAxis_TurnLeftRight;
static float gAxis_MoveForwardBack;
static float gAxis_StrafeLeftRight;
static float gAxis_AutomapFreeCamZoomInOut;
static float gAxis_MenuUpDown;
static float gAxis_MenuLeftRight;

static void clearAllActionBits() noexcept {
    gGameActionsActive       = GameActions::NONE;
    gGameActionsJustStarted  = GameActions::NONE;
    gGameActionsJustEnded    = GameActions::NONE;
    gMenuActionsActive       = MenuActions::NONE;
    gMenuActionsJustStarted  = MenuActions::NONE;
    gMenuActionsJustEnded    = MenuActions::NONE;
}

static void clearAllAxes() noexcept {
    gAxis_TurnLeftRight             = 0.0f;
    gAxis_MoveForwardBack           = 0.0f;
    gAxis_StrafeLeftRight           = 0.0f;
    gAxis_AutomapFreeCamZoomInOut   = 0.0f;
    gAxis_MenuUpDown                = 0.0f;
    gAxis_MenuLeftRight             = 0.0f;
}

static void clearAllInputs() noexcept {
    clearAllActionBits();
    clearAllAxes();
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

static void handleAnalogInput(const float inputValue, const Controls::AxisBits bindings) noexcept {
    if ((bindings & Controls::Axis::TURN_LEFT_RIGHT) != 0) {
        gAxis_TurnLeftRight += inputValue * Config::gGamepadTurnSensitivity;
    }

    if ((bindings & Controls::Axis::MOVE_FORWARD_BACK) != 0) {
        gAxis_MoveForwardBack += inputValue;
    }

    if ((bindings & Controls::Axis::STRAFE_LEFT_RIGHT) != 0) {
        gAxis_StrafeLeftRight += inputValue;
    }

    if ((bindings & Controls::Axis::AUTOMAP_FREE_CAM_ZOOM_IN_OUT) != 0) {
        gAxis_AutomapFreeCamZoomInOut += inputValue;
    }

    if ((bindings & Controls::Axis::MENU_UP_DOWN) != 0) {
        gAxis_MenuUpDown += inputValue;
    }

    if ((bindings & Controls::Axis::MENU_LEFT_RIGHT) != 0) {
        gAxis_MenuLeftRight += inputValue;
    }
}

static void updateAxesFromControllerInput() noexcept {    
    auto handleGamepadAnalogInput = [&](const ControllerInput input) noexcept {
        const float inputValue = Input::getControllerInputValue(input);
        const Controls::AxisBits bindings = Config::gGamepadAxisBindings[(uint8_t) input];
        handleAnalogInput(inputValue, bindings);
    };

    handleGamepadAnalogInput(ControllerInput::AXIS_LEFT_X);
    handleGamepadAnalogInput(ControllerInput::AXIS_LEFT_Y);
    handleGamepadAnalogInput(ControllerInput::AXIS_RIGHT_X);
    handleGamepadAnalogInput(ControllerInput::AXIS_RIGHT_Y);
    handleGamepadAnalogInput(ControllerInput::AXIS_TRIG_LEFT);
    handleGamepadAnalogInput(ControllerInput::AXIS_TRIG_RIGHT);
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

float Axis::getValue(const AxisBits axis) noexcept {
    if (axis == TURN_LEFT_RIGHT) {
        return gAxis_TurnLeftRight;
    }
    else if (axis == MOVE_FORWARD_BACK) {
        return gAxis_MoveForwardBack;
    }
    else if (axis == STRAFE_LEFT_RIGHT) {
        return gAxis_StrafeLeftRight;
    }
    else if (axis == AUTOMAP_FREE_CAM_ZOOM_IN_OUT) {
        return gAxis_AutomapFreeCamZoomInOut;
    }
    else if (axis == MENU_UP_DOWN) {
        return gAxis_MenuUpDown;
    }
    else if (axis == MENU_LEFT_RIGHT) {
        return gAxis_MenuLeftRight;
    }
    else {
        return 0.0f;
    }
}

void init() noexcept {
    clearAllInputs();
}

void shutdown() noexcept {
    clearAllInputs();    
}

void update() noexcept {
    clearAllInputs();
    updateActionsFromKeyboardInput();
    updateActionsFromControllerInput();
    updateAxesFromControllerInput();
}

END_NAMESPACE(Controls)
