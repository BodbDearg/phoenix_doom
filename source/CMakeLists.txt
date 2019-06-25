set(INCLUDE_PATHS
    "."
)

set(SOURCE_FILES
    "ammain.c"
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
    "base.c"
    "Burger.cpp"
    "Burger.h"
    "ceilng.c"
    "CelUtils.cpp"
    "CelUtils.h"
    "change.c"
    "D_Main.cpp"
    "Data.cpp"
    "doom.h"
    "doomrez.h"
    "doors.c"
    "Endian.h"
    "enemy.c"
    "Finally.h"
    "floor.c"
    "fmain.c"
    "game.c"
    "info.c"
    "inmain.c"
    "Inter.cpp"
    "Lights.cpp"
    "Macros.h"
    "Map.cpp"
    "MapData.cpp"
    "MapData.h"
    "MapUtil.cpp"
    "MathUtils.h"
    "Mem.h"
    "mmain.c"
    "mobj.c"
    "move.c"
    "omain.c"
    "phase1.c"
    "phase2.c"
    "phase6.c"
    "phase7.c"
    "phase8.c"
    "plats.c"
    "pspr.c"
    "R_Data.cpp"
    "R_Main.cpp"
    "Resource.h"
    "ResourceMgr.cpp"
    "ResourceMgr.h"
    "Resources.cpp"
    "Resources.h"
    "Setup.cpp"
    "shoot.c"
    "sight.c"
    "slide.c"
    "Sound.cpp"
    "Sounds.h"
    "spec.c"
    "Sprites.cpp"
    "Sprites.h"
    "states.h"
    "stmain.c"
    "Switch.cpp"
    "Tables.cpp"
    "Teleport.cpp"
    "Textures.cpp"
    "Textures.h"
    "threedo.c"
    "Tick.cpp"
    "user.c"
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
