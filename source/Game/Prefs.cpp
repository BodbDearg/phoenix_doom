#include "Prefs.h"

#include "Base/FileUtils.h"
#include "Base/Finally.h"
#include <cstring>
#include <filesystem>
#include <SDL.h>
#include <string>

BEGIN_NAMESPACE(Prefs)

// Sanity check! These must agree:
static_assert(MAX_KEYBOARD_SCAN_CODES == SDL_NUM_SCANCODES);

static constexpr char* const DEFAULT_CONFIG_INI = R"(#---------------------------------------------------------------------------------------------------
# === Phoenix Doom config file ===
#
# If you want to regenerate this file to the defaults, just delete it and restart the game!
#---------------------------------------------------------------------------------------------------

#---------------------------------------------------------------------------------------------------
# Changing video resolution:
#
# Phoenix Doom has an unusual way of specifying render resolution. You specify it via 'RenderScale'
# as an integer multiple (1x, 2x, 3x etc.) of the original 320x200 resolution. This is done so the
# original game UI layouts can be maintained, and also so that pixels are never blurred as a result
# of non-integer scaling.
#
# Here is an example list of 'RenderScale' values and resulting game renderer resolutions:
#   1 = 320x200
#   2 = 640x400
#   3 = 960x600
#   4 = 1280x800
#   5 = 1600x1000
#   6 = 1920x1200
#   7 = 2240x1400
#
# Please note that higher resolutions will be MUCH slower due to the demands of software rendering
# on the CPU. I recommend 640x400 as it looks a LOT sharper than the original resolution but still
# maintains a nice 'chunky' retro feel while performing excellently. It also 'fits' reasonably well
# into a variety of monitor resolutions nicely (see below).
#
# Once the user has determined the renderer resolution, the game will then also attempt to fit that
# resolution to the desktop resolution of the host machine by performing pixel doubling, tripling
# etc. where it can. For example if I specify '640x400' as the renderer resolution and my desktop
# resolution is 2560x1440 then the game will repeat that 3x, for an effective output size of '1920x1200'.
# Similarly if I specify 320x200 as the game resolution, then that will be repeated 7x to output at
# '2240x1400'. In windowed mode this final 'output' resolution determines the window size, whereas
# in fullscreen mode it will determine the size of the display area, around which black empty space
# (i.e 'letterboxing') will appear. As you can see it is easier to fit the lower resolutions into
# any screen size, therefore '320x200' or '640x400' often work best as target renderer resolutions.
#
# Phew... that was a lot, but hopefully that sort of makes sense?!
#---------------------------------------------------------------------------------------------------
[Video]
Fullscreen      = 1
RenderScale     = 1

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
Left Ctrl       = shoot
Right Ctrl      = shoot
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

Controls::MenuActions gKeyboardMenuActions[MAX_KEYBOARD_SCAN_CODES];
Controls::GameActions gKeyboardGameActions[MAX_KEYBOARD_SCAN_CODES];

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
    
    std::string cfgFileMessage = "No config file found on disk!\nThe game will generate a new one at the following location:\n\n";
    cfgFileMessage += iniFilePath;
    cfgFileMessage += "\n\nEdit this file to change preferences.";

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

void clear() noexcept {
    std::memset(gKeyboardMenuActions, 0, sizeof(gKeyboardMenuActions));
    std::memset(gKeyboardGameActions, 0, sizeof(gKeyboardGameActions));
}

void read() noexcept {
    clear();

    // Get the path to the config ini file and regenerate it if it doesn't exist
    std::string iniFilePath = determineIniFilePath();
    regenerateDefaultConfigFileIfNotPresent(iniFilePath);

    // Firstly read the ini file bytes, if that fails then abort with an error
    /*
    std::byte* pIniFileData = nullptr;
    size_t iniFileDataSize = 0;
    FileUtils::getContentsOfFile(filePath, pIniFileData, iniFileDataSize);
    */
}

END_NAMESPACE(Prefs)
