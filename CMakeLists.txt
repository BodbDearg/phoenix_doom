cmake_minimum_required (VERSION 3.14)

project(PhoenixDoom)

set(GAME_NAME PhoenixDoom)
set(BURGERLIB_NAME BurgerLib)
set(LIBSDL_NAME SDL)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

# Compiler detection
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(COMPILER_CLANG TRUE)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(COMPILER_GCC TRUE)
endif()

if (MSVC)
    set(COMPILER_MSVC TRUE)
endif()

# OS/Platform detection
if (WIN32)
    set(PLATFORM_WINDOWS TRUE)
endif()

if (APPLE)
    # Could also be iOS etc. but not targetting that so just assume Mac
    set(PLATFORM_MAC TRUE)
endif()

# Clang, Apple Clang, or GCC - enable debugging
if (COMPILER_CLANG OR COMPILER_GCC)
    set(CMAKE_C_FLAGS -g)
    set(CMAKE_CXX_FLAGS -g)
endif()

set(BURGERLIB_SRC_DIR "${PROJECT_SOURCE_DIR}/lib")
set(GAME_SRC_DIR "${PROJECT_SOURCE_DIR}/source")
set(THIRD_PARTY_LIBS_DIR "${PROJECT_SOURCE_DIR}/third_party_libs")

# Adding individual projects and libraries
add_subdirectory("${THIRD_PARTY_LIBS_DIR}/libsdl")

###################################################################################################
# Burger lib
###################################################################################################
set(BURGERLIB_INCLUDE_PATHS
    ${GAME_SRC_DIR}
)

set(BURGERLIB_SOURCES
    ${BURGERLIB_SRC_DIR}/burger/burger.h
    ${BURGERLIB_SRC_DIR}/burger/clearthescreen.c
    ${BURGERLIB_SRC_DIR}/burger/detectmouse.c
    #${BURGERLIB_SRC_DIR}/burger/dlzss.s
    ${BURGERLIB_SRC_DIR}/burger/drawarect.c
    ${BURGERLIB_SRC_DIR}/burger/drawmshape.c
    ${BURGERLIB_SRC_DIR}/burger/drawrezshape.c
    ${BURGERLIB_SRC_DIR}/burger/drawshape.c
    ${BURGERLIB_SRC_DIR}/burger/erasembshape.c
    ${BURGERLIB_SRC_DIR}/burger/errormsg.c
    ${BURGERLIB_SRC_DIR}/burger/flushkeys.c
    #${BURGERLIB_SRC_DIR}/burger/fontlib/font3
    #${BURGERLIB_SRC_DIR}/burger/fontlib/font5
    #${BURGERLIB_SRC_DIR}/burger/fontlib/font7
    #${BURGERLIB_SRC_DIR}/burger/fontlib/font9
    #${BURGERLIB_SRC_DIR}/burger/fontlib/nice.fnt
    #${BURGERLIB_SRC_DIR}/burger/fontlib/normal.pal
    ${BURGERLIB_SRC_DIR}/burger/getakey.c
    #${BURGERLIB_SRC_DIR}/burger/getapixel.s
    ${BURGERLIB_SRC_DIR}/burger/getrandom.c
    ${BURGERLIB_SRC_DIR}/burger/getshapeindexptr.c
    ${BURGERLIB_SRC_DIR}/burger/halt.s
    ${BURGERLIB_SRC_DIR}/burger/initytable.c
    ${BURGERLIB_SRC_DIR}/burger/interceptkey.c
    ${BURGERLIB_SRC_DIR}/burger/keyarray.c
    ${BURGERLIB_SRC_DIR}/burger/keycodes.h
    ${BURGERLIB_SRC_DIR}/burger/keymodifiers.c
    ${BURGERLIB_SRC_DIR}/burger/killedsong.c
    ${BURGERLIB_SRC_DIR}/burger/lasttick.c
    ${BURGERLIB_SRC_DIR}/burger/longtoasc.c
    ${BURGERLIB_SRC_DIR}/burger/longwordtoasc.c
    ${BURGERLIB_SRC_DIR}/burger/mousepresent.c
    ${BURGERLIB_SRC_DIR}/burger/music.c
    ${BURGERLIB_SRC_DIR}/burger/mygetch.c
    ${BURGERLIB_SRC_DIR}/burger/mykbhit.c
    ${BURGERLIB_SRC_DIR}/burger/readjoybuttons.c
    ${BURGERLIB_SRC_DIR}/burger/readmouseabs.c
    ${BURGERLIB_SRC_DIR}/burger/readmousebuttons.c
    ${BURGERLIB_SRC_DIR}/burger/readtick.c
    ${BURGERLIB_SRC_DIR}/burger/saveafile.c
    ${BURGERLIB_SRC_DIR}/burger/screenheight.c
    ${BURGERLIB_SRC_DIR}/burger/screenwidth.c
    #${BURGERLIB_SRC_DIR}/burger/setapixel.s
    #${BURGERLIB_SRC_DIR}/burger/showmcga.s
    #${BURGERLIB_SRC_DIR}/burger/showpic.s
    #${BURGERLIB_SRC_DIR}/burger/showpicptr.s
    #${BURGERLIB_SRC_DIR}/burger/swaplong.s
    #${BURGERLIB_SRC_DIR}/burger/swapshort.s
    #${BURGERLIB_SRC_DIR}/burger/swapushort.s
    ${BURGERLIB_SRC_DIR}/burger/systemstate.c
    #${BURGERLIB_SRC_DIR}/burger/test.c
    #${BURGERLIB_SRC_DIR}/burger/testmbshape.s
    #${BURGERLIB_SRC_DIR}/burger/testmshape.s
    ${BURGERLIB_SRC_DIR}/burger/videopointer.c
    ${BURGERLIB_SRC_DIR}/burger/videowidth.c
    ${BURGERLIB_SRC_DIR}/burger/waitakey.c
    ${BURGERLIB_SRC_DIR}/burger/waitevent.c
    #${BURGERLIB_SRC_DIR}/burger/waitjoypress.s
    #${BURGERLIB_SRC_DIR}/burger/waitjoyrelease.s
    ${BURGERLIB_SRC_DIR}/burger/waittick.c
    ${BURGERLIB_SRC_DIR}/burger/waitticks.c
    ${BURGERLIB_SRC_DIR}/burger/waitticksevent.c
    ${BURGERLIB_SRC_DIR}/burger/ytable.c
    ${BURGERLIB_SRC_DIR}/intmath/im_dec2int.c
    #${BURGERLIB_SRC_DIR}/intmath/im_div.s
    #${BURGERLIB_SRC_DIR}/intmath/im_fixdiv.s
    #${BURGERLIB_SRC_DIR}/intmath/im_fixmul.s
    ${BURGERLIB_SRC_DIR}/intmath/im_hiword.c
    ${BURGERLIB_SRC_DIR}/intmath/im_long2fix.c
    ${BURGERLIB_SRC_DIR}/intmath/im_loword.c
    ${BURGERLIB_SRC_DIR}/intmath/im_multiply.c
    ${BURGERLIB_SRC_DIR}/intmath/im_sdivide.c
    ${BURGERLIB_SRC_DIR}/intmath/im_udivide.c
    ${BURGERLIB_SRC_DIR}/intmath/intmath.h
    #${BURGERLIB_SRC_DIR}/mvelib/mvelib.h
    #${BURGERLIB_SRC_DIR}/mvelib/nfadvance.s
    #${BURGERLIB_SRC_DIR}/mvelib/nfhicolordecomp.c
    #${BURGERLIB_SRC_DIR}/mvelib/nfhicolordecomp.s
    #${BURGERLIB_SRC_DIR}/mvelib/nfhicolordecomp.s2
    #${BURGERLIB_SRC_DIR}/mvelib/nfhicolordecomp.snew
)

add_library(${BURGERLIB_NAME} ${BURGERLIB_SOURCES})
include_directories(${BURGERLIB_INCLUDE_PATHS})
target_link_libraries(${BURGERLIB_NAME} ${LIBSDL_NAME})

###################################################################################################
# Game
###################################################################################################
set(GAME_INCLUDE_PATHS
    ${BURGERLIB_SRC_DIR}/burger
    ${BURGERLIB_SRC_DIR}/intmath
    ${LIBSDL_INCLUDE_DIR}
)

set(GAME_SOURCES
    # ${GAME_SRC_DIR}/blitasm.s
    ${GAME_SRC_DIR}/ammain.c
    ${GAME_SRC_DIR}/base.c
    ${GAME_SRC_DIR}/ceilng.c
    ${GAME_SRC_DIR}/CelUtils.cpp
    ${GAME_SRC_DIR}/CelUtils.h
    ${GAME_SRC_DIR}/change.c
    ${GAME_SRC_DIR}/data.c
    ${GAME_SRC_DIR}/dmain.c
    ${GAME_SRC_DIR}/doom.h
    ${GAME_SRC_DIR}/doomrez.h
    ${GAME_SRC_DIR}/doors.c
    ${GAME_SRC_DIR}/Endian.h
    ${GAME_SRC_DIR}/enemy.c
    ${GAME_SRC_DIR}/floor.c
    ${GAME_SRC_DIR}/fmain.c
    ${GAME_SRC_DIR}/game.c
    ${GAME_SRC_DIR}/info.c
    ${GAME_SRC_DIR}/inmain.c
    ${GAME_SRC_DIR}/inter.c
    ${GAME_SRC_DIR}/lights.c
    ${GAME_SRC_DIR}/mac.c
    ${GAME_SRC_DIR}/Macros.h
    ${GAME_SRC_DIR}/map.c
    ${GAME_SRC_DIR}/MapData.cpp
    ${GAME_SRC_DIR}/MapData.h
    ${GAME_SRC_DIR}/maputl.c
    ${GAME_SRC_DIR}/MathUtils.c
    ${GAME_SRC_DIR}/MathUtils.h
    ${GAME_SRC_DIR}/Mem.h    
    ${GAME_SRC_DIR}/mmain.c
    ${GAME_SRC_DIR}/mobj.c
    ${GAME_SRC_DIR}/move.c
    ${GAME_SRC_DIR}/omain.c
    ${GAME_SRC_DIR}/phase1.c
    ${GAME_SRC_DIR}/phase2.c
    ${GAME_SRC_DIR}/phase6.c
    ${GAME_SRC_DIR}/phase7.c
    ${GAME_SRC_DIR}/phase8.c
    ${GAME_SRC_DIR}/plats.c
    ${GAME_SRC_DIR}/pspr.c
    ${GAME_SRC_DIR}/rdata.c
    ${GAME_SRC_DIR}/Resource.h
    ${GAME_SRC_DIR}/ResourceMgr.cpp
    ${GAME_SRC_DIR}/ResourceMgr.h
    ${GAME_SRC_DIR}/Resources.cpp
    ${GAME_SRC_DIR}/Resources.h
    ${GAME_SRC_DIR}/rmain.c
    ${GAME_SRC_DIR}/setup.c
    ${GAME_SRC_DIR}/shoot.c
    ${GAME_SRC_DIR}/sight.c
    ${GAME_SRC_DIR}/slide.c
    ${GAME_SRC_DIR}/sound.c
    ${GAME_SRC_DIR}/sounds.h
    ${GAME_SRC_DIR}/spec.c
    ${GAME_SRC_DIR}/Sprites.cpp
    ${GAME_SRC_DIR}/Sprites.h
    ${GAME_SRC_DIR}/states.h
    ${GAME_SRC_DIR}/stmain.c
    ${GAME_SRC_DIR}/switch.c
    ${GAME_SRC_DIR}/tables.c
    ${GAME_SRC_DIR}/telept.c
    ${GAME_SRC_DIR}/Textures.cpp
    ${GAME_SRC_DIR}/Textures.h
    ${GAME_SRC_DIR}/threedo.c
    ${GAME_SRC_DIR}/tick.c
    ${GAME_SRC_DIR}/user.c
)

if (PLATFORM_WINDOWS)
    list(APPEND GAME_SOURCES ${GAME_SRC_DIR}/Main_Windows.cpp)
endif()

if (PLATFORM_MAC)
    list(APPEND GAME_SOURCES ${GAME_SRC_DIR}/Main_Mac.mm)
endif()

add_executable(${GAME_NAME} ${GAME_SOURCES})
include_directories(${GAME_INCLUDE_PATHS})
target_link_libraries(${GAME_NAME} ${BURGERLIB_NAME} ${LIBSDL_NAME})
