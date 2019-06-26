set(INCLUDE_PATHS
    "."
)

set(SOURCE_FILES
    "Audio/Audio.cpp"
    "Audio/Audio.h"
    "Audio/AudioData.h"
    "Audio/AudioDataMgr.cpp"
    "Audio/AudioDataMgr.h"
    "Audio/AudioLoader.cpp"
    "Audio/AudioLoader.h"
    "Audio/AudioOutputDevice.cpp"
    "Audio/AudioOutputDevice.h"
    "Audio/AudioSystem.cpp"
    "Audio/AudioSystem.h"
    "Audio/AudioVoice.h"
    "Automap_Main.cpp"
    "Base.cpp"
    "Burger.cpp"
    "Burger.h"
    "Ceiling.cpp"
    "CelUtils.cpp"
    "CelUtils.h"
    "Change.cpp"
    "Data.cpp"
    "Doom.h"
    "Doom_Main.cpp"
    "DoomRez.h"
    "Doors.cpp"
    "Endian.h"
    "Enemy.cpp"
    "Finale_Main.cpp"
    "Finally.h"
    "Floor.cpp"
    "Game.cpp"
    "Info.cpp"
    "Inter.cpp"
    "Intermission_Main.cpp"
    "Lights.cpp"
    "Macros.h"
    "Map.cpp"
    "MapData.cpp"
    "MapData.h"
    "MapObj.cpp"
    "MapUtil.cpp"
    "MathUtils.h"
    "Mem.h"
    "Menu_Main.cpp"
    "Move.cpp"
    "Options_Main.cpp"
    "Platforms.cpp"
    "PlayerSprites.cpp"
    "Render_Data.cpp"
    "Render_Main.cpp"
    "Render_Phase1.cpp"
    "Render_Phase2.cpp"
    "Render_Phase6.cpp"
    "Render_Phase7.cpp"
    "Render_Phase8.cpp"
    "Resource.h"
    "ResourceMgr.cpp"
    "ResourceMgr.h"
    "Resources.cpp"
    "Resources.h"
    "Setup.cpp"
    "Shoot.cpp"
    "Sight.cpp"
    "Slide.cpp"
    "Sound.cpp"
    "Sounds.h"
    "Specials.cpp"
    "Sprites.cpp"
    "Sprites.h"
    "states.h"
    "StatusBar_Main.cpp"
    "Switch.cpp"
    "Tables.cpp"
    "Teleport.cpp"
    "Textures.cpp"
    "Textures.h"
    "ThreeDO.cpp"
    "Tick.cpp"
    "User.cpp"
)

set(OTHER_FILES
    "../TODO.txt"
)

if (PLATFORM_WINDOWS)
    list(APPEND SOURCE_FILES "Main_Windows.cpp")
endif()

if (PLATFORM_MAC)
    list(APPEND SOURCE_FILES "Main_Mac.mm")
endif()

if (COMPILER_MSVC)
    # MSVC: Don't complain about using regular 'std::fopen()' etc.
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_executable(${GAME_NAME} ${SOURCE_FILES} ${HEADER_FILES} ${OTHER_FILES})
setup_source_groups("${SOURCE_FILES}" "${HEADER_FILES}" "${OTHER_FILES}")

include_directories(${INCLUDE_PATHS})
target_link_libraries(${GAME_NAME} ${LIBSDL_NAME})
