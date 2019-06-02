cmake_minimum_required (VERSION 3.14)

project(PhoenixDoom)

set(GAME_NAME PhoenixDoom)
set(LIB_NAME BurgerLib)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

# TODO: This is Clang/GCC specific - wrap
set(CMAKE_C_FLAGS -g)
set(CMAKE_CXX_FLAGS -g)

set(LIB_SRC_DIR "${PROJECT_SOURCE_DIR}/lib")
set(GAME_SRC_DIR "${PROJECT_SOURCE_DIR}/source")

set(LIB_INCLUDE_DIRS
    ${GAME_SRC_DIR}
)

set(LIB_SOURCES
    ${LIB_SRC_DIR}/burger/burger.h
    ${LIB_SRC_DIR}/burger/clearthescreen.c
    ${LIB_SRC_DIR}/burger/detectmouse.c
    #${LIB_SRC_DIR}/burger/dlzss.s
    ${LIB_SRC_DIR}/burger/drawarect.c
    ${LIB_SRC_DIR}/burger/drawmshape.c
    ${LIB_SRC_DIR}/burger/drawrezshape.c
    ${LIB_SRC_DIR}/burger/drawshape.c
    ${LIB_SRC_DIR}/burger/erasembshape.c
    ${LIB_SRC_DIR}/burger/errormsg.c
    ${LIB_SRC_DIR}/burger/flushkeys.c
    #${LIB_SRC_DIR}/burger/fontlib/font3
    #${LIB_SRC_DIR}/burger/fontlib/font5
    #${LIB_SRC_DIR}/burger/fontlib/font7
    #${LIB_SRC_DIR}/burger/fontlib/font9
    #${LIB_SRC_DIR}/burger/fontlib/nice.fnt
    #${LIB_SRC_DIR}/burger/fontlib/normal.pal
    ${LIB_SRC_DIR}/burger/getakey.c
    #${LIB_SRC_DIR}/burger/getapixel.s
    ${LIB_SRC_DIR}/burger/getrandom.c
    ${LIB_SRC_DIR}/burger/getshapeheight.c
    ${LIB_SRC_DIR}/burger/getshapeindexptr.c
    ${LIB_SRC_DIR}/burger/getshapewidth.c
    ${LIB_SRC_DIR}/burger/halt.s
    ${LIB_SRC_DIR}/burger/initytable.c
    ${LIB_SRC_DIR}/burger/interceptkey.c
    ${LIB_SRC_DIR}/burger/keyarray.c
    ${LIB_SRC_DIR}/burger/keycodes.h
    ${LIB_SRC_DIR}/burger/keymodifiers.c
    ${LIB_SRC_DIR}/burger/killedsong.c
    ${LIB_SRC_DIR}/burger/lasttick.c
    ${LIB_SRC_DIR}/burger/longtoasc.c
    ${LIB_SRC_DIR}/burger/longwordtoasc.c
    ${LIB_SRC_DIR}/burger/mousepresent.c
    ${LIB_SRC_DIR}/burger/music.c
    ${LIB_SRC_DIR}/burger/mygetch.c
    ${LIB_SRC_DIR}/burger/mykbhit.c
    ${LIB_SRC_DIR}/burger/readjoybuttons.c
    ${LIB_SRC_DIR}/burger/readmouseabs.c
    ${LIB_SRC_DIR}/burger/readmousebuttons.c
    ${LIB_SRC_DIR}/burger/readtick.c
    ${LIB_SRC_DIR}/burger/saveafile.c
    ${LIB_SRC_DIR}/burger/screenheight.c
    ${LIB_SRC_DIR}/burger/screenwidth.c
    #${LIB_SRC_DIR}/burger/setapixel.s
    #${LIB_SRC_DIR}/burger/showmcga.s
    #${LIB_SRC_DIR}/burger/showpic.s
    #${LIB_SRC_DIR}/burger/showpicptr.s
    #${LIB_SRC_DIR}/burger/swaplong.s
    #${LIB_SRC_DIR}/burger/swapshort.s
    #${LIB_SRC_DIR}/burger/swapushort.s
    ${LIB_SRC_DIR}/burger/systemstate.c
    #${LIB_SRC_DIR}/burger/test.c
    #${LIB_SRC_DIR}/burger/testmbshape.s
    #${LIB_SRC_DIR}/burger/testmshape.s
    ${LIB_SRC_DIR}/burger/videopointer.c
    ${LIB_SRC_DIR}/burger/videowidth.c
    ${LIB_SRC_DIR}/burger/waitakey.c
    ${LIB_SRC_DIR}/burger/waitevent.c
    #${LIB_SRC_DIR}/burger/waitjoypress.s
    #${LIB_SRC_DIR}/burger/waitjoyrelease.s
    ${LIB_SRC_DIR}/burger/waittick.c
    ${LIB_SRC_DIR}/burger/waitticks.c
    ${LIB_SRC_DIR}/burger/waitticksevent.c
    ${LIB_SRC_DIR}/burger/ytable.c
    ${LIB_SRC_DIR}/intmath/im_dec2int.c
    #${LIB_SRC_DIR}/intmath/im_div.s
    #${LIB_SRC_DIR}/intmath/im_fixdiv.s
    #${LIB_SRC_DIR}/intmath/im_fixmul.s
    ${LIB_SRC_DIR}/intmath/im_hiword.c
    ${LIB_SRC_DIR}/intmath/im_long2fix.c
    ${LIB_SRC_DIR}/intmath/im_loword.c
    ${LIB_SRC_DIR}/intmath/im_multiply.c
    ${LIB_SRC_DIR}/intmath/im_sdivide.c
    ${LIB_SRC_DIR}/intmath/im_udivide.c
    ${LIB_SRC_DIR}/intmath/intmath.h
    #${LIB_SRC_DIR}/mvelib/mvelib.h
    #${LIB_SRC_DIR}/mvelib/nfadvance.s
    #${LIB_SRC_DIR}/mvelib/nfhicolordecomp.c
    #${LIB_SRC_DIR}/mvelib/nfhicolordecomp.s
    #${LIB_SRC_DIR}/mvelib/nfhicolordecomp.s2
    #${LIB_SRC_DIR}/mvelib/nfhicolordecomp.snew
)

set(GAME_INCLUDE_DIRS
    ${LIB_SRC_DIR}/burger
    ${LIB_SRC_DIR}/intmath
)

set(GAME_SOURCES
    # ${GAME_SRC_DIR}/blitasm.s
    ${GAME_SRC_DIR}/ammain.c
    ${GAME_SRC_DIR}/base.c
    ${GAME_SRC_DIR}/ceilng.c
    ${GAME_SRC_DIR}/change.c
    ${GAME_SRC_DIR}/data.c
    ${GAME_SRC_DIR}/dmain.c
    ${GAME_SRC_DIR}/doom.h
    ${GAME_SRC_DIR}/DoomResources.cpp
    ${GAME_SRC_DIR}/DoomResources.h
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
    ${GAME_SRC_DIR}/Main.cpp
    ${GAME_SRC_DIR}/map.c
    ${GAME_SRC_DIR}/maputl.c
    ${GAME_SRC_DIR}/Math.c
    ${GAME_SRC_DIR}/Math.h
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
    ${GAME_SRC_DIR}/rmain.c
    ${GAME_SRC_DIR}/setup.c
    ${GAME_SRC_DIR}/shoot.c
    ${GAME_SRC_DIR}/sight.c
    ${GAME_SRC_DIR}/slide.c
    ${GAME_SRC_DIR}/sound.c
    ${GAME_SRC_DIR}/sounds.h
    ${GAME_SRC_DIR}/spec.c
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

add_library(${LIB_NAME} ${LIB_SOURCES})
include_directories(${LIB_INCLUDE_DIRS})

add_executable(${GAME_NAME} ${GAME_SOURCES})
include_directories(${GAME_INCLUDE_DIRS})
target_link_libraries(${GAME_NAME} ${LIB_NAME})
