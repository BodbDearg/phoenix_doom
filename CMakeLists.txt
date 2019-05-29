cmake_minimum_required (VERSION 3.14)

set(PROJECT_NAME PhoenixDoom)
project(${PROJECT_NAME})

set(GAME_SRC_DIR "${PROJECT_SOURCE_DIR}/source")

set(SOURCES
    ${GAME_SRC_DIR}/ammain.c
    ${GAME_SRC_DIR}/base.c
    # ${GAME_SRC_DIR}/blitasm.s
    ${GAME_SRC_DIR}/ceilng.c
    ${GAME_SRC_DIR}/change.c
    ${GAME_SRC_DIR}/data.c
    ${GAME_SRC_DIR}/dmain.c
    ${GAME_SRC_DIR}/doom.h
    ${GAME_SRC_DIR}/doom.make
    ${GAME_SRC_DIR}/doomrez.h
    ${GAME_SRC_DIR}/doors.c
    ${GAME_SRC_DIR}/enemy.c
    ${GAME_SRC_DIR}/floor.c
    ${GAME_SRC_DIR}/fmain.c
    ${GAME_SRC_DIR}/game.c
    ${GAME_SRC_DIR}/info.c
    ${GAME_SRC_DIR}/inmain.c
    ${GAME_SRC_DIR}/inter.c
    ${GAME_SRC_DIR}/lights.c
    ${GAME_SRC_DIR}/mac.c
    ${GAME_SRC_DIR}/map.c
    ${GAME_SRC_DIR}/maputl.c
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
    ${GAME_SRC_DIR}/threedo.c
    ${GAME_SRC_DIR}/tick.c
    ${GAME_SRC_DIR}/user.c
)


add_executable(${PROJECT_NAME} ${SOURCES})
