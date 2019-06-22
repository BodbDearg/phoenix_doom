set(INCLUDE_PATHS
    "."
)

set(SOURCE_FILES
    "ammain.c"
    "Audio/AudioData.h"
    "Audio/AudioDataMgr.cpp"
    "Audio/AudioDataMgr.h"
    "Audio/AudioLoader.cpp"
    "Audio/AudioLoader.h"
    "Audio/AudioOutputDevice.cpp"
    "Audio/AudioOutputDevice.h"
    "base.c"
    "ceilng.c"
    "CelUtils.cpp"
    "CelUtils.h"
    "change.c"
    "data.c"
    "dmain.c"
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
    "inter.c"
    "lights.c"
    "Macros.h"
    "map.c"
    "MapData.cpp"
    "MapData.h"
    "maputl.c"
    "MathUtils.c"
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
    "rdata.c"
    "Resource.h"
    "ResourceMgr.cpp"
    "ResourceMgr.h"
    "Resources.cpp"
    "Resources.h"
    "rmain.c"
    "setup.c"
    "shoot.c"
    "sight.c"
    "slide.c"
    "sound.c"
    "sounds.h"
    "spec.c"
    "Sprites.cpp"
    "Sprites.h"
    "states.h"
    "stmain.c"
    "switch.c"
    "tables.c"
    "telept.c"
    "Textures.cpp"
    "Textures.h"
    "threedo.c"
    "tick.c"
    "user.c"
    #"blitasm.s"
)

set(OTHER_FILES
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
target_link_libraries(${GAME_NAME} ${BURGERLIB_NAME} ${LIBSDL_NAME})
