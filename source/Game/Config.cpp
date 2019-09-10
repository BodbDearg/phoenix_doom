#include "Config.h"

#include "Base/FileUtils.h"
#include "Base/Finally.h"
#include "Base/IniUtils.h"
#include "DoomDefines.h"
#include <filesystem>
#include <SDL.h>

BEGIN_NAMESPACE(Config)

// Sanity checks!
static_assert(Input::NUM_KEYBOARD_KEYS == SDL_NUM_SCANCODES);   // Must agree!
static_assert(Input::NUM_MOUSE_WHEEL_AXES == 2);

// Note: these had to be split up into sections because I hit the maximum string limit size of MSVC!
static constexpr char* const DEFAULT_CONFIG_INI_SECTION_1 = 
R"(#---------------------------------------------------------------------------------------------------
# === Phoenix Doom config file ===
#
# If you want to regenerate this file to the defaults, just delete it and restart the game!
#---------------------------------------------------------------------------------------------------

)";

static constexpr char* const DEFAULT_CONFIG_INI_SECTION_2 =
R"(####################################################################################################
[Video]
####################################################################################################

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
IntegerOutputScaling = 0

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

)";

static constexpr char* const DEFAULT_CONFIG_INI_SECTION_3 = 
R"(####################################################################################################
[Graphics]
####################################################################################################

#---------------------------------------------------------------------------------------------------
# When set to '1' the game will simulate an RGB555 style (16 bit) framebuffer similar to what the
# original 3DO hardware used. Color precision is lost in this mode and there will be much more
# banding artifacts, however the result will be much closer to the original 3DO Doom visually.
#---------------------------------------------------------------------------------------------------
Simulate16BitFramebuffer = 0

#---------------------------------------------------------------------------------------------------
# If set to '0' then the game will NOT apply so called 'fake contrast', a technique which darkens
# walls at certain orientations in order to make corners stand out more. The original 3DO Doom did
# not do fake contrast (unlike the PC version) so this makes the game look more '3DO like'.
#---------------------------------------------------------------------------------------------------
DoFakeContrast = 1

)";

static constexpr char* const DEFAULT_CONFIG_INI_SECTION_4 =
R"(####################################################################################################
[InputGeneral]
####################################################################################################

#---------------------------------------------------------------------------------------------------
# Default setting for always run.
# Note: can be toggled in game with the right keys.
# This is best left OFF for game controller input and ON for mouse controls.
#---------------------------------------------------------------------------------------------------
DefaultAlwaysRun = 0

#---------------------------------------------------------------------------------------------------
# 0-1 range: controls the point at which an analogue axis like a trigger, stick etc. is regarded
# as 'pressed' when treated as a digital input (e.g trigger used for 'shoot' action).
# The default of '0.5' (halfway depressed) is probably reasonable for most users.
#---------------------------------------------------------------------------------------------------
AnalogToDigitalThreshold = 0.5

)";

static constexpr char* const DEFAULT_CONFIG_INI_SECTION_5 = 
R"(####################################################################################################
[KeyboardControls]
####################################################################################################

#---------------------------------------------------------------------------------------------------
# Hopefully these actions are fairly self explanatory...
# See the end of this file for a full list of available keyboard keys etc. that can be assigned.
#---------------------------------------------------------------------------------------------------
Up              = move_forward, menu_up
Down            = move_backward, menu_down
Left            = turn_left, menu_left
Right           = turn_right, menu_right
A               = strafe_left, menu_left
D               = strafe_right, menu_right
W               = move_forward, menu_up
S               = move_backward, menu_down
Left Ctrl       = attack
Right Ctrl      = attack
Space           = use, menu_ok
Return          = use, menu_ok
E               = use
Escape          = options, menu_back
Backspace       = options, menu_back
Left Shift      = run
Right Shift     = run
\[              = prev_weapon
\]              = next_weapon
,               = prev_weapon
.               = next_weapon
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
F               = automap_free_cam_toggle
X               = automap_free_cam_toggle
-               = automap_free_cam_zoom_out
\=              = automap_free_cam_zoom_in
Keypad -        = automap_free_cam_zoom_out
Keypad +        = automap_free_cam_zoom_in
PageUp          = debug_move_camera_up
PageDown        = debug_move_camera_down
R               = toggle_always_run

)";

static constexpr char* const DEFAULT_CONFIG_INI_SECTION_6 =
R"(####################################################################################################
[MouseControls]
####################################################################################################

#---------------------------------------------------------------------------------------------------
# How sensitive the mouse is for turning.
# Note: the x axis is the only part of mouse movement that is used, and it is hardcoded to turning!
#---------------------------------------------------------------------------------------------------
TurnSensitivity = 1.0

#---------------------------------------------------------------------------------------------------
# Whether to invert the mouse wheel x and y axis
#---------------------------------------------------------------------------------------------------
InvertMWheelXAxis = 0
InvertMWheelYAxis = 0

#---------------------------------------------------------------------------------------------------
# These are all of the mouse buttons and mouse wheel axes available
#---------------------------------------------------------------------------------------------------
Button_Left     = attack, menu_ok
Button_Right    = use
Button_Middle   =
Button_X1       =
Button_X2       =
Axis_X          =
Axis_Y          = weapon_next_prev

)";

static constexpr char* const DEFAULT_CONFIG_INI_SECTION_7 =
R"(####################################################################################################
[GameControllerControls]
####################################################################################################

#---------------------------------------------------------------------------------------------------
# 0-1 range: controls when minor controller inputs are discarded.
# The default of '0.125' only registers movement if the stick is at least 12.5% moved.
# Setting too low may result in unwanted jitter and movement.
#---------------------------------------------------------------------------------------------------
DeadZone = 0.125

#---------------------------------------------------------------------------------------------------
# Sensitivity multiplier that affects how sensitive the gamepad is for left/right turning
#---------------------------------------------------------------------------------------------------
TurnSensitivity = 1.0

#---------------------------------------------------------------------------------------------------
# The actual Button bindings for the game controller.
# These are all the buttons and axes available.
#---------------------------------------------------------------------------------------------------
Button_A                = use, menu_ok
Button_B                = menu_back
Button_X                = automap_free_cam_toggle
Button_Y                = automap_toggle
Button_Back             = options
Button_Guide            = 
Button_Start            = pause
Button_LeftStick        = 
Button_Rightstick       = toggle_always_run
Button_Leftshoulder     = prev_weapon
Button_Rightshoulder    = next_weapon
Button_DpUp             = move_forward, menu_up
Button_DpDown           = move_backward, menu_down
Button_DpLeft           = turn_left, menu_left
Button_DpRight          = turn_right, menu_right
Axis_LeftX              = strafe_left_right, menu_left_right
Axis_LeftY              = move_forward_backward, menu_up_down
Axis_RightX             = turn_left_right, menu_left_right
Axis_RightY             = automap_free_cam_zoom_in_out, menu_up_down
Axis_LeftTrigger        = run
Axis_RightTrigger       = attack

)";

static constexpr char* const DEFAULT_CONFIG_INI_SECTION_8 =
R"(####################################################################################################
[Debug]
####################################################################################################

#---------------------------------------------------------------------------------------------------
# If set to '1' then the camera can be moved up and down with the 'debug_cam_move_up' and
# 'debug_cam_move_down' key actions. This is useful for testing the renderer at different heights.
#---------------------------------------------------------------------------------------------------
AllowCameraUpDownMovement = 0

####################################################################################################
[CheatKeySequences]
####################################################################################################

#---------------------------------------------------------------------------------------------------
# These are cheat key sequences that can be typed in game to activate cheats.
# 
# Notes:
#   - Key sequences can be at most 16 characters (excluding level numbers).
#     Any characters more than this will be ignored.
#   - Only alpha (A-Z) and numeric (0-9) characters are allowed in the key sequences.
#   - Warp and music cheats must be followed by two digits specifying a level number.
#       E.G: IDCLEV05 and IDMUS12
#     Special: enter IDMUS00 to hear the unused bunny music from the 3DO version of the game :P
#---------------------------------------------------------------------------------------------------
GodMode                     = IDDQD
NoClip                      = IDCLIP
MapAndThingsRevealToggle    = IDDT
AllWeaponsAndAmmo           = IDFA
AllWeaponsAmmoAndKeys       = IDKFA
WarpToMap                   = IDCLEV
ChangeMusic                 = IDMUS
GrantInvisibility           = IDBEHOLDI
GrantRadSuit                = IDBEHOLDR
GrantBeserk                 = IDBEHOLDS
GrantInvulnerability        = IDBEHOLDV

)";

static constexpr char* const DEFAULT_CONFIG_INI_SECTION_9 = 
R"(####################################################################################################
#
# Appendix
#
####################################################################################################

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
bool                        gbSimulate16BitFramebuffer;
bool                        gbDoFakeContrast;
float                       gInputAnalogToDigitalThreshold;
bool                        gDefaultAlwaysRun;
Controls::MenuActionBits    gKeyboardMenuActions[Input::NUM_KEYBOARD_KEYS];
Controls::GameActionBits    gKeyboardGameActions[Input::NUM_KEYBOARD_KEYS];
float                       gMouseTurnSensitivity;
bool                        gInvertMouseWheelAxis[Input::NUM_MOUSE_WHEEL_AXES];
Controls::MenuActionBits    gMouseMenuActions[NUM_MOUSE_BUTTONS];
Controls::GameActionBits    gMouseGameActions[NUM_MOUSE_BUTTONS];
Controls::AxisBits          gMouseWheelAxisBindings[Input::NUM_MOUSE_WHEEL_AXES];
float                       gGamepadDeadZone;
float                       gGamepadTurnSensitivity;
Controls::MenuActionBits    gGamepadMenuActions[NUM_CONTROLLER_INPUTS];
Controls::GameActionBits    gGamepadGameActions[NUM_CONTROLLER_INPUTS];
Controls::AxisBits          gGamepadAxisBindings[NUM_CONTROLLER_INPUTS];
bool                        gbAllowDebugCameraUpDownMovement;
CheatKeySequence            gCheatKeys_GodMode;
CheatKeySequence            gCheatKeys_NoClip;
CheatKeySequence            gCheatKeys_MapAndThingsRevealToggle;
CheatKeySequence            gCheatKeys_AllWeaponsAndAmmo;
CheatKeySequence            gCheatKeys_AllWeaponsAmmoAndKeys;
CheatKeySequence            gCheatKeys_WarpToMap;
CheatKeySequence            gCheatKeys_ChangeMusic;
CheatKeySequence            gCheatKeys_GrantInvisibility;
CheatKeySequence            gCheatKeys_GrantRadSuit;
CheatKeySequence            gCheatKeys_GrantBeserk;
CheatKeySequence            gCheatKeys_GrantInvulnerability;

//----------------------------------------------------------------------------------------------------------------------
// Tells if a string is prefixed by another string, case insensitive
//----------------------------------------------------------------------------------------------------------------------
static bool strHasPrefixCaseInsensitive(const char* pStr, const char* pPrefix) noexcept {
    ASSERT(pStr);
    ASSERT(pPrefix);

    while (pPrefix[0]) {
        const char c1 = (char) std::toupper(pStr[0]);
        const char c2 = (char) std::toupper(pPrefix[0]);

        if (c1 == c2) {
            ++pStr;
            ++pPrefix;
        } else {
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Tells if two strings are equal using case insensitive comparison (assumes ASCII)
//----------------------------------------------------------------------------------------------------------------------
static bool strsMatchCaseInsensitive(const char* pStr1, const char* pStr2) noexcept {
    ASSERT(pStr1);
    ASSERT(pStr2);

    while (true) {
        const char c1 = (char) std::toupper(pStr1[0]);
        const char c2 = (char) std::toupper(pStr2[0]);

        if (c1 != c2)
            return false;
        
        if (c1 == 0)
            break;
        
        ++pStr1;
        ++pStr2;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Assign a cheat key sequence a value from a string.
// Only alpha and numeric characters are allowed and the maximum sequence length is 16.
//----------------------------------------------------------------------------------------------------------------------
static void setCheatKeySequence(CheatKeySequence& sequence, const char* const pKeysStr) noexcept {
    // Handle all of the chars in the string, up to 16 chars
    ASSERT(pKeysStr);    
    uint32_t keyIdx = 0;
    const char* pCurChar = pKeysStr;

    do {
        const char c = (char) std::toupper(pCurChar[0]);
        ++pCurChar;

        if (c == 0) {
            break;
        }

        if (c >= 'A' && c <= 'Z') {
            const uint8_t key = (uint8_t) SDL_SCANCODE_A + (uint8_t) c - 'A';
            sequence.keys[keyIdx] = key;
            ++keyIdx;
        }
        else if (c >= '0' && c <= '9') {
            const uint8_t key = (uint8_t) SDL_SCANCODE_0 + (uint8_t) c - '0';
            sequence.keys[keyIdx] = key;
            ++keyIdx;
        }
    } while (keyIdx < CheatKeySequence::MAX_KEYS);

    // If we did not complete the sequence then null the rest of the chars
    while (keyIdx < CheatKeySequence::MAX_KEYS) {
        sequence.keys[keyIdx] = 0;
        ++keyIdx;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Determines the path to the config .ini for the game
//----------------------------------------------------------------------------------------------------------------------
static std::string determineIniFilePath() noexcept {
    char* const pCfgFilePath = SDL_GetPrefPath(SAVE_FILE_ORG, SAVE_FILE_PRODUCT);
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
        "How to configure Phoenix Doom",
        cfgFileMessage.c_str(),
        nullptr
    );

    std::string configFile;
    configFile.reserve(1024 * 64);
    configFile.append(DEFAULT_CONFIG_INI_SECTION_1);
    configFile.append(DEFAULT_CONFIG_INI_SECTION_2);
    configFile.append(DEFAULT_CONFIG_INI_SECTION_3);
    configFile.append(DEFAULT_CONFIG_INI_SECTION_4);
    configFile.append(DEFAULT_CONFIG_INI_SECTION_5);
    configFile.append(DEFAULT_CONFIG_INI_SECTION_6);
    configFile.append(DEFAULT_CONFIG_INI_SECTION_7);
    configFile.append(DEFAULT_CONFIG_INI_SECTION_8);
    configFile.append(DEFAULT_CONFIG_INI_SECTION_9);

    if (!FileUtils::writeDataToFile(iniFilePath.c_str(), (const std::byte*) configFile.data(), configFile.length())) {
        FATAL_ERROR_F(
            "Failed to generate/write the default config file for the game to path '%s'! "
            "Is there write access to this location, or is the disk full?",
            iniFilePath.c_str()
        );
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Parse a single game or menu action or axis string
//----------------------------------------------------------------------------------------------------------------------
static void parseSingleActionOrAxisString(
    const std::string& actionStr,
    Controls::GameActionBits& gameActionsOut,
    Controls::MenuActionBits& menuActionsOut,
    Controls::AxisBits& axisBindingsOut
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

    auto handleAxis = [&](const char* const pName, const Controls::AxisBits axisBits) noexcept {
        if (actionStr == pName) {
            axisBindingsOut |= axisBits;
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
    handleGameAction("debug_move_camera_up",        Controls::GameActions::DEBUG_MOVE_CAMERA_UP);
    handleGameAction("debug_move_camera_down",      Controls::GameActions::DEBUG_MOVE_CAMERA_DOWN);
    handleGameAction("toggle_always_run",           Controls::GameActions::TOGGLE_ALWAYS_RUN);

    handleMenuAction("menu_up",     Controls::MenuActions::UP);
    handleMenuAction("menu_down",   Controls::MenuActions::DOWN);
    handleMenuAction("menu_left",   Controls::MenuActions::LEFT);
    handleMenuAction("menu_right",  Controls::MenuActions::RIGHT);
    handleMenuAction("menu_ok",     Controls::MenuActions::OK);
    handleMenuAction("menu_back",   Controls::MenuActions::BACK);

    handleAxis("turn_left_right",               Controls::Axis::TURN_LEFT_RIGHT);
    handleAxis("move_forward_backward",         Controls::Axis::MOVE_FORWARD_BACK);
    handleAxis("strafe_left_right",             Controls::Axis::STRAFE_LEFT_RIGHT);
    handleAxis("automap_free_cam_zoom_in_out",  Controls::Axis::AUTOMAP_FREE_CAM_ZOOM_IN_OUT);
    handleAxis("menu_up_down",                  Controls::Axis::MENU_UP_DOWN);
    handleAxis("menu_left_right",               Controls::Axis::MENU_LEFT_RIGHT);
    handleAxis("weapon_next_prev",              Controls::Axis::WEAPON_NEXT_PREV);
}

//----------------------------------------------------------------------------------------------------------------------
// Tells if a character is an actions string delimiter character
//----------------------------------------------------------------------------------------------------------------------
static inline bool isActionsStrDelimiterChar(const char c) noexcept {
    return (
        (c == ' ') ||
        (c == '\t') ||
        (c == '\n') ||
        (c == '\r') ||
        (c == '\f') ||
        (c == '\v') ||
        (c == ',')
    );
}

//----------------------------------------------------------------------------------------------------------------------
// Parse a game and menu actions bindings string
//----------------------------------------------------------------------------------------------------------------------
static void parseActionsAndAxesString(
    const std::string& actionsStr,
    Controls::GameActionBits& gameActionsOut,
    Controls::MenuActionBits& menuActionsOut,
    Controls::AxisBits& axisBindingsOut
) noexcept {
    gameActionsOut = Controls::GameActions::NONE;
    menuActionsOut = Controls::MenuActions::NONE;
    axisBindingsOut = Controls::Axis::NONE;

    const char* pActionStart = actionsStr.c_str();
    const char* const pActionsStrEnd = actionsStr.c_str() + actionsStr.size();

    while (pActionStart < pActionsStrEnd) {
        // Skip over commas and whitespace etc.
        const char firstChar = pActionStart[0];
        const bool bSkipFirstChar = isActionsStrDelimiterChar(firstChar);

        if (bSkipFirstChar) {
            ++pActionStart;
            continue;
        }

        // Now find the end of this action, stop at the string end or ','
        const char* pActionEnd = pActionStart + 1;

        while (pActionEnd < pActionsStrEnd) {
            if (isActionsStrDelimiterChar(pActionEnd[0])) {
                break;
            } else {
                ++pActionEnd;
            }
        }

        // Parse this action string
        const size_t actionLen = (size_t)(pActionEnd - pActionStart);
        gTmpActionStr.clear();
        gTmpActionStr.assign(pActionStart, actionLen);

        parseSingleActionOrAxisString(gTmpActionStr, gameActionsOut, menuActionsOut, axisBindingsOut);

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
    Controls::AxisBits axisBindings = Controls::Axis::NONE;

    parseActionsAndAxesString(actionsStr, gameActions, menuActions, axisBindings);
    
    // Note: for the keyboard we ignore any attempts to bind analogue axes to keyboard keys!
    // It doesn't make sense because we can only get a '0' or a '1' from the keyboard, and
    // not the -1 to +1 range that an axis requires.
    MARK_UNUSED(axisBindings);

    gKeyboardGameActions[keyIdx] = gameActions;
    gKeyboardMenuActions[keyIdx] = menuActions;
}

//----------------------------------------------------------------------------------------------------------------------
// Parse the bindings/actions for a particular mouse button
//----------------------------------------------------------------------------------------------------------------------
static void parseMouseButtonOrWheelActions(const std::string buttonOrAxisName, const std::string& actionsStr) noexcept {
    // Parse the actions string
    Controls::GameActionBits gameActions = Controls::GameActions::NONE;
    Controls::MenuActionBits menuActions = Controls::MenuActions::NONE;
    Controls::AxisBits axisBindings = Controls::Axis::NONE;

    parseActionsAndAxesString(actionsStr, gameActions, menuActions, axisBindings);

    // Get what button or wheel we are dealing with
    if (strHasPrefixCaseInsensitive(buttonOrAxisName.c_str(), "Button_")) {
        MouseButton button = MouseButton::INVALID;

        if (strsMatchCaseInsensitive(buttonOrAxisName.c_str(), "Button_Left")) {
            button = MouseButton::LEFT;
        } else if (strsMatchCaseInsensitive(buttonOrAxisName.c_str(), "Button_Right")) {
            button = MouseButton::RIGHT;
        } else if (strsMatchCaseInsensitive(buttonOrAxisName.c_str(), "Button_Middle")) {
            button = MouseButton::MIDDLE;
        } else if (strsMatchCaseInsensitive(buttonOrAxisName.c_str(), "Button_X1")) {
            button = MouseButton::X1;
        } else if (strsMatchCaseInsensitive(buttonOrAxisName.c_str(), "Button_X2")) {
            button = MouseButton::X2;
        } else {
            return;
        }

        // Note: for the mouse buttons we ignore any attempts to bind analogue axes (same as keyboard)
        const uint8_t buttonIdx = (uint8_t) button;
        gMouseGameActions[buttonIdx] = gameActions;
        gMouseMenuActions[buttonIdx] = menuActions;
    }
    else if (strHasPrefixCaseInsensitive(buttonOrAxisName.c_str(), "Axis_")) {
        uint8_t axisIdx = 0;

        if (strsMatchCaseInsensitive(buttonOrAxisName.c_str(), "Axis_X")) {
            axisIdx = 0;
        } else if (strsMatchCaseInsensitive(buttonOrAxisName.c_str(), "Axis_Y")) {
            axisIdx = 1;
        } else {
            return;
        }

        // Note: the mouse wheel can only be bound to an axis!
        gMouseWheelAxisBindings[axisIdx] = axisBindings;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Figures out what controller input a button or axis name in the .ini corresponds to.
// Returns an invalid controller input if not recognized.
//----------------------------------------------------------------------------------------------------------------------
static constexpr char* CONTROLLER_BTN_NAME_PREFIX = "Button_";
static constexpr char* CONTROLLER_AXIS_NAME_PREFIX = "Axis_";

static ControllerInput getControllerInputFromName(const std::string name) noexcept {
    const char* const pStr = name.c_str();
    
    if (strHasPrefixCaseInsensitive(pStr, CONTROLLER_BTN_NAME_PREFIX)) {
        const uint8_t button = (uint8_t) SDL_GameControllerGetButtonFromString(pStr + std::strlen(CONTROLLER_BTN_NAME_PREFIX));
        return sdlControllerButtonToInput(button);
    }
    else if (strHasPrefixCaseInsensitive(pStr, CONTROLLER_AXIS_NAME_PREFIX)) {
        const uint8_t axis = (uint8_t) SDL_GameControllerGetAxisFromString(pStr + std::strlen(CONTROLLER_AXIS_NAME_PREFIX));
        return sdlControllerAxisToInput(axis);
    }
    else {
        return ControllerInput::INVALID;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Parse the bindings/actions for a particular controller input
//----------------------------------------------------------------------------------------------------------------------
static void parseControllerInputActions(const std::string controllerInputName, const std::string& actionsStr) noexcept {
    ControllerInput input = getControllerInputFromName(controllerInputName);
    const uint8_t inputIdx = (uint8_t) input;

    if (inputIdx >= NUM_CONTROLLER_INPUTS)
        return;
    
    Controls::GameActionBits gameActions = Controls::GameActions::NONE;
    Controls::MenuActionBits menuActions = Controls::MenuActions::NONE;
    Controls::AxisBits axisBindings = Controls::Axis::NONE;

    parseActionsAndAxesString(actionsStr, gameActions, menuActions, axisBindings);

    gGamepadGameActions[inputIdx] = gameActions;
    gGamepadMenuActions[inputIdx] = menuActions;
    gGamepadAxisBindings[inputIdx] = axisBindings;
}

//----------------------------------------------------------------------------------------------------------------------
// Parse a cheat key sequence
//----------------------------------------------------------------------------------------------------------------------
static void parseCheatKeySequence(const std::string& name, const char* const pSequenceKeys) noexcept {
    if (!pSequenceKeys)
        return;
    
    auto parseSequence = [&](const char* pExpectedName, CheatKeySequence& sequence) noexcept {
        if (name == pExpectedName) {
            setCheatKeySequence(sequence, pSequenceKeys);
        }
    };

    parseSequence("GodMode",                    gCheatKeys_GodMode);
    parseSequence("NoClip",                     gCheatKeys_NoClip);
    parseSequence("MapAndThingsRevealToggle",   gCheatKeys_MapAndThingsRevealToggle);
    parseSequence("AllWeaponsAndAmmo",          gCheatKeys_AllWeaponsAndAmmo);
    parseSequence("AllWeaponsAmmoAndKeys",      gCheatKeys_AllWeaponsAmmoAndKeys);
    parseSequence("WarpToMap",                  gCheatKeys_WarpToMap);
    parseSequence("ChangeMusic",                gCheatKeys_ChangeMusic);
    parseSequence("GrantInvisibility",          gCheatKeys_GrantInvisibility);
    parseSequence("GrantRadSuit",               gCheatKeys_GrantRadSuit);
    parseSequence("GrantBeserk",                gCheatKeys_GrantBeserk);
    parseSequence("GrantInvulnerability",       gCheatKeys_GrantInvulnerability);
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
    else if (entry.section == "Graphics") {
        if (entry.key == "Simulate16BitFramebuffer") {
            gbSimulate16BitFramebuffer = entry.getBoolValue(gbSimulate16BitFramebuffer);
        }
        else if (entry.key == "DoFakeContrast") {
            gbDoFakeContrast = entry.getBoolValue(gbDoFakeContrast);
        }
    }
    else if (entry.section == "InputGeneral") {
        if (entry.key == "AnalogToDigitalThreshold") {
            gInputAnalogToDigitalThreshold = entry.getFloatValue(gInputAnalogToDigitalThreshold);
        }
        else if (entry.key == "DefaultAlwaysRun") {
            gDefaultAlwaysRun = entry.getBoolValue(gDefaultAlwaysRun);
        }
    }
    else if (entry.section == "KeyboardControls") {
        const SDL_Scancode scancode = SDL_GetScancodeFromName(entry.key.c_str());

        if (scancode != SDL_SCANCODE_UNKNOWN) {
            const uint16_t scancodeIdx = (uint16_t) scancode;

            if (scancodeIdx < Input::NUM_KEYBOARD_KEYS) {
                parseKeyboardKeyActions(scancodeIdx, entry.value);
            }
        }
    }
    else if (entry.section == "MouseControls") {
        if (entry.key == "TurnSensitivity") {
            gMouseTurnSensitivity = entry.getFloatValue(gMouseTurnSensitivity);
        }
        else if (entry.key == "InvertMWheelXAxis") {
            gInvertMouseWheelAxis[0] = entry.getBoolValue(gInvertMouseWheelAxis[0]);
        }
        else if (entry.key == "InvertMWheelYAxis") {
            gInvertMouseWheelAxis[1] = entry.getBoolValue(gInvertMouseWheelAxis[1]);
        }
        else {
            parseMouseButtonOrWheelActions(entry.key, entry.value);
        }
    }
    else if (entry.section == "GameControllerControls") {
        if (entry.key == "DeadZone") {
            gGamepadDeadZone = entry.getFloatValue(gGamepadDeadZone);
        }
        else if (entry.key == "TurnSensitivity") {
            gGamepadTurnSensitivity = entry.getFloatValue(gGamepadTurnSensitivity);
        }
        else {
            parseControllerInputActions(entry.key, entry.value);
        }
    }
    else if (entry.section == "Debug") {
        if (entry.key == "AllowCameraUpDownMovement") {
            gbAllowDebugCameraUpDownMovement = entry.getBoolValue(gbAllowDebugCameraUpDownMovement);
        }
    }
    else if (entry.section == "CheatKeySequences") {
        parseCheatKeySequence(entry.key, entry.value.c_str());
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

    gbSimulate16BitFramebuffer = false;
    gbDoFakeContrast = true;

    gInputAnalogToDigitalThreshold = 0.5f;
    gDefaultAlwaysRun = false;

    std::memset(gKeyboardMenuActions, 0, sizeof(gKeyboardMenuActions));
    std::memset(gKeyboardGameActions, 0, sizeof(gKeyboardGameActions));

    gMouseTurnSensitivity = 1.0f;
    gInvertMouseWheelAxis[0] = false;
    gInvertMouseWheelAxis[1] = false;
    std::memset(gMouseMenuActions, 0, sizeof(gMouseMenuActions));
    std::memset(gMouseGameActions, 0, sizeof(gMouseGameActions));

    gGamepadDeadZone = 0.15f;    
    gGamepadTurnSensitivity = 1.0f;
    std::memset(gGamepadMenuActions, 0, sizeof(gGamepadMenuActions));
    std::memset(gGamepadGameActions, 0, sizeof(gGamepadGameActions));
    std::memset(gGamepadGameActions, 0, sizeof(gGamepadAxisBindings));
    
    gbAllowDebugCameraUpDownMovement = false;

    setCheatKeySequence(gCheatKeys_GodMode,                     "IDDQD");
    setCheatKeySequence(gCheatKeys_NoClip,                      "IDCLIP");
    setCheatKeySequence(gCheatKeys_MapAndThingsRevealToggle,    "IDDT");
    setCheatKeySequence(gCheatKeys_AllWeaponsAndAmmo,           "IDFA");
    setCheatKeySequence(gCheatKeys_AllWeaponsAmmoAndKeys,       "IDKFA");
    setCheatKeySequence(gCheatKeys_WarpToMap,                   "IDCLEV");
    setCheatKeySequence(gCheatKeys_ChangeMusic,                 "IDMUS");
    setCheatKeySequence(gCheatKeys_GrantInvisibility,           "IDBEHOLDI");
    setCheatKeySequence(gCheatKeys_GrantRadSuit,                "IDBEHOLDR");
    setCheatKeySequence(gCheatKeys_GrantBeserk,                 "IDBEHOLDS");
    setCheatKeySequence(gCheatKeys_GrantInvulnerability,        "IDBEHOLDV");
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
