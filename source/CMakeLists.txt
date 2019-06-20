set(SOURCE_FILES
    "ammain.c"
    "base.c"
    "ceilng.c"
    "CelUtils.cpp"
    "change.c"
    "data.c"
    "dmain.c"
    "doors.c"
    "enemy.c"
    "floor.c"
    "fmain.c"
    "game.c"
    "info.c"
    "inmain.c"
    "inter.c"
    "lights.c"
    "map.c"
    "MapData.cpp"
    "maputl.c"
    "MathUtils.c"
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
    "ResourceMgr.cpp"
    "Resources.cpp"
    "rmain.c"
    "setup.c"
    "shoot.c"
    "sight.c"
    "slide.c"
    "sound.c"
    "spec.c"
    "Sprites.cpp"
    "stmain.c"
    "switch.c"
    "tables.c"
    "telept.c"
    "Textures.cpp"
    "threedo.c"
    "tick.c"
    "user.c"
    #"blitasm.s"
)

set(HEADER_FILES
    "CelUtils.h"
    "doom.h"
    "doomrez.h"
    "Endian.h"
    "Macros.h"
    "MapData.h"
    "MathUtils.h"
    "Mem.h"
    "Resource.h"
    "ResourceMgr.h"
    "Resources.h"
    "sounds.h"
    "Sprites.h"
    "states.h"
    "Textures.h"
)

set(OTHER_FILES
)

if (PLATFORM_WINDOWS)
    list(APPEND SOURCE_FILES "Main_Windows.cpp")
endif()

if (PLATFORM_MAC)
    list(APPEND SOURCE_FILES "Main_Mac.mm")
endif()

add_executable(${GAME_NAME} ${SOURCE_FILES} ${HEADER_FILES} ${OTHER_FILES})
setup_source_groups("${SOURCE_FILES}" "${HEADER_FILES}" "${OTHER_FILES}")

target_link_libraries(${GAME_NAME} ${BURGERLIB_NAME} ${LIBSDL_NAME})
