set(INCLUDE_PATHS
    "${GAME_SRC_DIR}"
    "burger"
)

set(EXPORTED_INCLUDE_PATHS
    "burger"
    "intmath"
)

set(SOURCE_FILES
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
    #"mvelib/nfadvance.s"
    #"mvelib/nfhicolordecomp.c"
    #"mvelib/nfhicolordecomp.s"
    #"mvelib/nfhicolordecomp.s2"
    #"mvelib/nfhicolordecomp.snew"
)

set(HEADER_FILES
    "burger/burger.h"
    "intmath/intmath.h"
    #"mvelib/mvelib.h"
)

set(OTHER_FILES
)

add_library(${BURGERLIB_NAME} ${SOURCE_FILES} ${HEADER_FILES} ${OTHER_FILES})
setup_source_groups("${SOURCE_FILES}" "${HEADER_FILES}" "${OTHER_FILES}")

include_directories(${INCLUDE_PATHS})
target_include_directories(${BURGERLIB_NAME} INTERFACE ${EXPORTED_INCLUDE_PATHS})
target_link_libraries(${BURGERLIB_NAME} ${LIBSDL_NAME})
