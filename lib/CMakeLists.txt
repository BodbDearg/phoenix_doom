set(INCLUDE_PATHS
    "${GAME_SRC_DIR}"
    "burger"
)

set(EXPORTED_INCLUDE_PATHS
    "burger"
    "intmath"
)

set(SOURCES
    "burger/burger.h"
    "burger/drawarect.c"
    "burger/drawmshape.c"
    "burger/drawrezshape.c"
    "burger/drawshape.c"
    "burger/getrandom.c"
    "burger/getshapeindexptr.c"
    "burger/interceptkey.c"
    "burger/killedsong.c"
    "burger/lasttick.c"
    "burger/longwordtoasc.c"
    "burger/music.c"
    "burger/readjoybuttons.c"
    "burger/readtick.c"
    "burger/saveafile.c"
    "burger/screenheight.c"
    "burger/screenwidth.c"
    "burger/systemstate.c"
    "burger/videopointer.c"
    "intmath/intmath.h"
    #"mvelib/mvelib.h"
    #"mvelib/nfadvance.s"
    #"mvelib/nfhicolordecomp.c"
    #"mvelib/nfhicolordecomp.s"
    #"mvelib/nfhicolordecomp.s2"
    #"mvelib/nfhicolordecomp.snew"
)

add_library(${BURGERLIB_NAME} ${SOURCES})
include_directories(${INCLUDE_PATHS})
target_include_directories(${BURGERLIB_NAME} INTERFACE ${EXPORTED_INCLUDE_PATHS})
target_link_libraries(${BURGERLIB_NAME} ${LIBSDL_NAME})
