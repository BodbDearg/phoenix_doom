#include "Config.h"

#include "Base/FileUtils.h"
#include "Base/Finally.h"
#include "Base/IniUtils.h"
#include <filesystem>
#include <SDL.h>

BEGIN_NAMESPACE(Config)

// Sanity check! These must agree:
static_assert(Input::NUM_KEYBOARD_KEYS == SDL_NUM_SCANCODES);

static constexpr char* const DEFAULT_CONFIG_INI = R"(#---------------------------------------------------------------------------------------------------
# === Phoenix Doom config file ===
#
# If you want to regenerate this file to the defaults, just delete it and restart the game!
#---------------------------------------------------------------------------------------------------

[Video]

#---------------------------------------------------------------------------------------------------
# Fullscreen or windowed mode toggle.
# Set to '1' cause the app to launch in fullscreen mode, and '0' to use windowed mode.
#---------------------------------------------------------------------------------------------------
Fullscreen = 1

#---------------------------------------------------------------------------------------------------
# Game render resolution.
#
# This controls the resolution that the game engine renders at internally, before being upscaled
# to the specified output resolution. It is specified as an integer multiple of the original
# 320x200 resolution.
# 
# Here is an example list of 'RenderScale' values and resulting render resolutions:
#   1 = 320x200
#   2 = 640x400
#   3 = 960x600
#   4 = 1280x800
#   5 = 1600x1000
#   6 = 1920x1200
#   7 = 2240x1400
#
# Please note that higher resolutions will be MUCH slower due to the demands of software rendering
# on the CPU. I recommend '640x400' since it looks a lot sharper than the original resolution but
# still maintains a nice 'chunky' retro feel while performing excellently. It also 'fits' reasonably
# well into a variety of output resolutions nicely with not too much space leftover (see settings
# further below for more details on output scaling).
#---------------------------------------------------------------------------------------------------
RenderScale = 1

#---------------------------------------------------------------------------------------------------
# Output resolution, width and height.
#
# In windowed mode this is the size of the window.
# In fullscreen mode this is the actual screen resolution to use.
# If the values are '0' or less (auto resolution) then this means use the current (desktop)
# resolution in fullscreen mode, and use as large as possible of a window size in windowed mode.
#---------------------------------------------------------------------------------------------------
OutputResolutionW = -1
OutputResolutionH = -1

#---------------------------------------------------------------------------------------------------
# Force integer-only scaling of the output image, toggle.
#
# Controls how the image produced by the renderer is scaled to fit the output resolution size.
#
# If set to '1' (enabled) then the game can only upscale in integer multiples, for example it can
# only evenly 1x, 2x, or 3x etc. stretch the pixels in order to try and fit the rendered image to
# the target output area. Any parts of the output area which are leftover in this mode are displayed
# in black (letterboxed). Note that higher renderer resolutions will be harder to fit to any
# arbitrarily sized output area without leaving lots of space leftover, therefore '320x200' and
# '640x400' are often the best renderer resolutions to use with this scaling mode.
#
# If set to '0' (disabled) then the game can stretch by any decimal amount to fit the rendered image
# to the output area, but some rows or columns of pixels may be doubled up while others won't be.
# Note: in all cases nearest filtering is used so the result will never be blurry.
#---------------------------------------------------------------------------------------------------
IntegerOutputScaling = 1

#---------------------------------------------------------------------------------------------------
# Force aspect correct scaling of the output image, toggle.
#
# If '1' (enabled) then the game must preserve the original aspect ratio (1.6) of the images
# produced by the renderer when scaling them to fit the output area.
#
# If '0' (disabled) then the game is free to stretch/distort the images in any way for a better fit.
# Note: disabling aspect correct upscaling might only really take effect if integer output scaling
# is also disabled, since in many cases integer-only scaling might disallow this kind of strech.
#---------------------------------------------------------------------------------------------------
AspectCorrectOutputScaling = 1

#---------------------------------------------------------------------------------------------------
# Keyboard controls:
# Hopefully these are all self explanatory...
# See the end of this file for a full list of available keyboard keys etc. that can be assigned.
#---------------------------------------------------------------------------------------------------
[KeyboardControls]
Up              = move_forward,     menu_up
Down            = move_backward,    menu_down
Left            = turn_left,        menu_left
Right           = turn_right,       menu_right
A               = strafe_left,      menu_left
D               = strafe_right,     menu_right
W               = move_forward,     menu_up
S               = move_backward,    menu_down
Left Ctrl       = attack
Right Ctrl      = attack
Space           = use,              menu_ok
Return          = use,              menu_ok
E               = use
Escape          = options,          menu_back
Backspace       = options,          menu_back
Left Shift      = run
Right Shift     = run
\[              = prev_weapon
\]              = next_weapon
P               = pause
Pause           = pause
1               = weapon_1
2               = weapon_2
3               = weapon_3
4               = weapon_4
5               = weapon_5
6               = weapon_6
7               = weapon_7
Tab             = automap_toggle
M               = automap_toggle
X               = automap_free_cam_toggle
-               = automap_free_cam_zoom_out
\=              = automap_free_cam_zoom_in
Keypad -        = automap_free_cam_zoom_out
Keypad +        = automap_free_cam_zoom_in

#---------------------------------------------------------------------------------------------------
# Available keyboard keys:
#   A-Z
#   0-9
#   Return
#   Escape
#   Backspace
#   Tab
#   Space
#   -
#   =
#   [
#   ]
#   \
#   #
#   ;
#   '
#   `
#   ,
#   .
#   /
#   CapsLock
#   F1
#   F2
#   F3
#   F4
#   F5
#   F6
#   F7
#   F8
#   F9
#   F10
#   F11
#   F12
#   PrintScreen
#   ScrollLock
#   Pause
#   Insert
#   Home
#   PageUp
#   Delete
#   End
#   PageDown
#   Right
#   Left
#   Down
#   Up
#   Numlock
#   Keypad /
#   Keypad *
#   Keypad -
#   Keypad +
#   Keypad Enter
#   Keypad 1
#   Keypad 2
#   Keypad 3
#   Keypad 4
#   Keypad 5
#   Keypad 6
#   Keypad 7
#   Keypad 8
#   Keypad 9
#   Keypad 0
#   Keypad .
#   Application
#   Power
#   Keypad =
#   F13
#   F14
#   F15
#   F16
#   F17
#   F18
#   F19
#   F20
#   F21
#   F22
#   F23
#   F24
#   Execute
#   Help
#   Menu
#   Select
#   Stop
#   Again
#   Undo
#   Cut
#   Copy
#   Paste
#   Find
#   Mute
#   VolumeUp
#   VolumeDown
#   Keypad ,
#   Keypad = (AS400)
#   AltErase
#   SysReq
#   Cancel
#   Clear
#   Prior
#   Return
#   Separator
#   Out
#   Oper
#   Clear / Again
#   CrSel
#   ExSel
#   Keypad 00
#   Keypad 000
#   ThousandsSeparator
#   DecimalSeparator
#   CurrencyUnit
#   CurrencySubUnit
#   Keypad (
#   Keypad )
#   Keypad {
#   Keypad }
#   Keypad Tab
#   Keypad Backspace
#   Keypad A
#   Keypad B
#   Keypad C
#   Keypad D
#   Keypad E
#   Keypad F
#   Keypad XOR
#   Keypad ^
#   Keypad %
#   Keypad <
#   Keypad >
#   Keypad &
#   Keypad &&
#   Keypad |
#   Keypad ||
#   Keypad :
#   Keypad #
#   Keypad Space
#   Keypad @
#   Keypad !
#   Keypad MemStore
#   Keypad MemRecall
#   Keypad MemClear
#   Keypad MemAdd
#   Keypad MemSubtract
#   Keypad MemMultiply
#   Keypad MemDivide
#   Keypad +/-
#   Keypad Clear
#   Keypad ClearEntry
#   Keypad Binary
#   Keypad Octal
#   Keypad Decimal
#   Keypad Hexadecimal
#   Left Ctrl
#   Left Shift
#   Left Alt
#   Left GUI
#   Right Ctrl
#   Right Shift
#   Right Alt
#   Right GUI
#   ModeSwitch
#   AudioNext
#   AudioPrev
#   AudioStop
#   AudioPlay
#   AudioMute
#   MediaSelect
#   WWW
#   Mail
#   Calculator
#   Computer
#   AC Search
#   AC Home
#   AC Back
#   AC Forward
#   AC Stop
#   AC Refresh
#   AC Bookmarks
#   BrightnessDown
#   BrightnessUp
#   DisplaySwitch
#   KBDIllumToggle
#   KBDIllumDown
#   KBDIllumUp
#   Eject
#   Sleep
#   App1
#   App2
#   AudioRewind
#   AudioFastForward
#---------------------------------------------------------------------------------------------------
)";

static std::string gTmpActionStr;   // Re-use this buffer during parsing to prevent reallocs. Not that it probably matters that much...

bool                        gbFullscreen;
uint32_t                    gRenderScale;
int32_t                     gOutputResolutionW;
int32_t                     gOutputResolutionH;
bool                        gbIntegerOutputScaling;
bool                        gbAspectCorrectOutputScaling;
Controls::MenuActionBits    gKeyboardMenuActions[Input::NUM_KEYBOARD_KEYS];
Controls::GameActionBits    gKeyboardGameActions[Input::NUM_KEYBOARD_KEYS];

//----------------------------------------------------------------------------------------------------------------------
// Determines the path to the config .ini for the game
//----------------------------------------------------------------------------------------------------------------------
static std::string determineIniFilePath() noexcept {
    char* const pCfgFilePath = SDL_GetPrefPath("com.darraghcoy", "phoenix_doom");
    auto cleanupCfgFilePath = finally([&](){
        SDL_free(pCfgFilePath);
    });

    if (!pCfgFilePath) {
        FATAL_ERROR("Unable to create or determine the path to the configuration for the game!");
        return std::string();
    }

    std::string path = pCfgFilePath;
    path += "config.ini";   // Note: path is guaranteed to have a separator at the end, as per SDL docs!
    return path;
}

//----------------------------------------------------------------------------------------------------------------------
// Regenerates the config ini file if it doesn't exist on disk
//----------------------------------------------------------------------------------------------------------------------
static void regenerateDefaultConfigFileIfNotPresent(const std::string& iniFilePath) noexcept {
    bool cfgFileExists;

    try {
        cfgFileExists = std::filesystem::exists(iniFilePath);
    } catch (...) {
        FATAL_ERROR("Unable to determine if the game configuration file exists!");
    }

    if (cfgFileExists)
        return;
    
    std::string cfgFileMessage = 
        "No config.ini file found on disk! (probably because this is your first time launching)\n"
        "The game will generate a new one at the following location:\n\n";
    
    cfgFileMessage += iniFilePath;
    cfgFileMessage += "\n\nEdit this file to change controls, screen resolution etc.";

    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_INFORMATION,
        "Configuring Phoenix Doom",
        cfgFileMessage.c_str(),
        nullptr
    );

    if (!FileUtils::writeDataToFile(iniFilePath.c_str(), (const std::byte*) DEFAULT_CONFIG_INI, std::strlen(DEFAULT_CONFIG_INI))) {
        FATAL_ERROR_F(
            "Failed to generate/write the default config file for the game to path '%s'! "
            "Is there write access to this location, or is the disk full?",
            iniFilePath.c_str()
        );
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Parse a single game or menu action string
//----------------------------------------------------------------------------------------------------------------------
static void parseSingleActionString(
    const std::string& actionStr,
    Controls::GameActionBits& gameActionsOut,
    Controls::MenuActionBits& menuActionsOut
) noexcept {
    // These cut down on repetivie code
    auto handleGameAction = [&](const char* const pName, const Controls::GameActionBits actionBits) noexcept {
        if (actionStr == pName) {
            gameActionsOut |= actionBits;
        }
    };

    auto handleMenuAction = [&](const char* const pName, const Controls::MenuActionBits actionBits) noexcept {
        if (actionStr == pName) {
            menuActionsOut |= actionBits;
        }
    };

    handleGameAction("move_forward",                Controls::GameActions::MOVE_FORWARD);
    handleGameAction("move_backward",               Controls::GameActions::MOVE_BACKWARD);
    handleGameAction("turn_left",                   Controls::GameActions::TURN_LEFT);
    handleGameAction("turn_right",                  Controls::GameActions::TURN_RIGHT);
    handleGameAction("strafe_left",                 Controls::GameActions::STRAFE_LEFT);
    handleGameAction("strafe_right",                Controls::GameActions::STRAFE_RIGHT);
    handleGameAction("attack",                      Controls::GameActions::ATTACK);
    handleGameAction("use",                         Controls::GameActions::USE);
    handleGameAction("options",                     Controls::GameActions::OPTIONS);
    handleGameAction("run",                         Controls::GameActions::RUN);
    handleGameAction("prev_weapon",                 Controls::GameActions::PREV_WEAPON);
    handleGameAction("next_weapon",                 Controls::GameActions::NEXT_WEAPON);
    handleGameAction("pause",                       Controls::GameActions::PAUSE);
    handleGameAction("weapon_1",                    Controls::GameActions::WEAPON_1);
    handleGameAction("weapon_2",                    Controls::GameActions::WEAPON_2);
    handleGameAction("weapon_3",                    Controls::GameActions::WEAPON_3);
    handleGameAction("weapon_4",                    Controls::GameActions::WEAPON_4);
    handleGameAction("weapon_5",                    Controls::GameActions::WEAPON_5);
    handleGameAction("weapon_6",                    Controls::GameActions::WEAPON_6);
    handleGameAction("weapon_7",                    Controls::GameActions::WEAPON_7);
    handleGameAction("automap_toggle",              Controls::GameActions::AUTOMAP_TOGGLE);
    handleGameAction("automap_free_cam_toggle",     Controls::GameActions::AUTOMAP_FREE_CAM_TOGGLE);
    handleGameAction("automap_free_cam_zoom_out",   Controls::GameActions::AUTOMAP_FREE_CAM_ZOOM_OUT);
    handleGameAction("automap_free_cam_zoom_in",    Controls::GameActions::AUTOMAP_FREE_CAM_ZOOM_IN);

    handleMenuAction("menu_up",     Controls::MenuActions::UP);
    handleMenuAction("menu_down",   Controls::MenuActions::DOWN);
    handleMenuAction("menu_left",   Controls::MenuActions::LEFT);
    handleMenuAction("menu_right",  Controls::MenuActions::RIGHT);
    handleMenuAction("menu_ok",     Controls::MenuActions::OK);
    handleMenuAction("menu_back",   Controls::MenuActions::BACK);
}

//----------------------------------------------------------------------------------------------------------------------
// Parse a game and menu actions bindings string
//----------------------------------------------------------------------------------------------------------------------
static void parseActionsString(
    const std::string& actionsStr,
    Controls::GameActionBits& gameActionsOut,
    Controls::MenuActionBits& menuActionsOut
) noexcept {
    gameActionsOut = Controls::GameActions::NONE;
    menuActionsOut = Controls::MenuActions::NONE;

    const char* pActionStart = actionsStr.c_str();
    const char* const pActionsStrEnd = actionsStr.c_str() + actionsStr.size();

    while (pActionStart < pActionsStrEnd) {
        // Skip over commas and whitespace
        const char firstChar = pActionStart[0];
        const bool bSkipFirstChar = (
            (firstChar == ' ') ||
            (firstChar == '\t') ||
            (firstChar == '\n') ||
            (firstChar == '\r') ||
            (firstChar == '\f') ||
            (firstChar == '\v') ||
            (firstChar == ',')
        );

        if (bSkipFirstChar) {
            ++pActionStart;
            continue;
        }

        // Now find the end of this action, stop at the string end or ','
        const char* pActionEnd = pActionStart + 1;

        while (pActionEnd < pActionsStrEnd) {
            if (pActionEnd[0] == ',') {
                break;
            } else {
                ++pActionEnd;
            }
        }

        // Parse this action string
        const size_t actionLen = (size_t)(pActionEnd - pActionStart);
        gTmpActionStr.clear();
        gTmpActionStr.assign(pActionStart, actionLen);

        parseSingleActionString(gTmpActionStr, gameActionsOut, menuActionsOut);

        // Move on to the next action in the string (if any)
        pActionStart = pActionEnd + 1;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Parse the bindings/actions for a particular keyboard key
//----------------------------------------------------------------------------------------------------------------------
static void parseKeyboardKeyActions(const uint16_t keyIdx, const std::string& actionsStr) noexcept {
    if (keyIdx >= Input::NUM_KEYBOARD_KEYS)
        return;
    
    Controls::GameActionBits gameActions = Controls::GameActions::NONE;
    Controls::MenuActionBits menuActions = Controls::MenuActions::NONE;
    parseActionsString(actionsStr, gameActions, menuActions);

    gKeyboardGameActions[keyIdx] = gameActions;
    gKeyboardMenuActions[keyIdx] = menuActions;
}

//----------------------------------------------------------------------------------------------------------------------
// Handle a config file entry.
// This is not a particularly elegant or fast implementation, but it gets the job done... 
//----------------------------------------------------------------------------------------------------------------------
static void handleConfigEntry(const IniUtils::Entry& entry) noexcept {
    if (entry.section == "Video") {
        if (entry.key == "Fullscreen") {
            gbFullscreen = entry.getBoolValue(gbFullscreen);
        }
        else if (entry.key == "RenderScale") {
            gRenderScale = std::min(std::max(entry.getIntValue(gRenderScale), 1), 1000);
        }
        else if (entry.key == "OutputResolutionW") {
            gOutputResolutionW = entry.getIntValue(gOutputResolutionW);
        }
        else if (entry.key == "OutputResolutionH") {
            gOutputResolutionH = entry.getIntValue(gOutputResolutionH);
        }
        else if (entry.key == "IntegerOutputScaling") {
            gbIntegerOutputScaling = entry.getBoolValue(gbIntegerOutputScaling);
        }
        else if (entry.key == "AspectCorrectOutputScaling") {
            gbAspectCorrectOutputScaling = entry.getBoolValue(gbAspectCorrectOutputScaling);
        }
    }
    else if (entry.section == "KeyboardControls") {
        const SDL_Scancode scancode = SDL_GetScancodeFromName(entry.key.c_str());

        if (scancode != SDL_SCANCODE_UNKNOWN) {
            const uint16_t scancodeIdx = (uint16_t) scancode;

            if (scancodeIdx < Input::NUM_KEYBOARD_KEYS) {
                parseKeyboardKeyActions(scancodeIdx, entry.value.c_str());
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Clears all settings (used prior to reading config)
//----------------------------------------------------------------------------------------------------------------------
static void clear() noexcept {
    gbFullscreen = true;
    gRenderScale = 1;
    gOutputResolutionW = -1;
    gOutputResolutionH = -1;
    gbIntegerOutputScaling = true;
    gbAspectCorrectOutputScaling = true;

    std::memset(gKeyboardMenuActions, 0, sizeof(gKeyboardMenuActions));
    std::memset(gKeyboardGameActions, 0, sizeof(gKeyboardGameActions));    
}

void init() noexcept {
    clear();

    // Get the path to the config ini file and regenerate it if it doesn't exist
    std::string iniFilePath = determineIniFilePath();
    regenerateDefaultConfigFileIfNotPresent(iniFilePath);

    // Firstly read the ini file bytes, if that fails then abort with an error
    std::byte* pIniFileData = nullptr;
    size_t iniFileDataSize = 0;

    auto cleanupIniFileData = finally([&](){
        delete[] pIniFileData;
    });

    if (!FileUtils::getContentsOfFile(iniFilePath.c_str(), pIniFileData, iniFileDataSize)) {
        FATAL_ERROR_F("Failed to read the game config file at path '%s'!", iniFilePath.c_str());
    }

    // Parse the ini file
    IniUtils::parseIniFromString((const char*) pIniFileData, iniFileDataSize, handleConfigEntry);
}

void shutdown() noexcept {
    gTmpActionStr.clear();
    gTmpActionStr.shrink_to_fit();
}

END_NAMESPACE(Config)
