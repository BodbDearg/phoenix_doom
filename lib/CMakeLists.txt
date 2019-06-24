set(INCLUDE_PATHS
    "${GAME_SRC_DIR}"
    "burger"
)

set(EXPORTED_INCLUDE_PATHS
    "burger"
    "intmath"
)

set(SOURCE_FILES
    "burger/burger.h"
    "burger/drawarect.c"
    "burger/drawrezshape.c"
    "burger/getrandom.c"
    "burger/getshapeindexptr.c"
    "burger/lasttick.c"
    "burger/longwordtoasc.c"
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

set(OTHER_FILES
)

add_library(${BURGERLIB_NAME} ${SOURCE_FILES} ${OTHER_FILES})
setup_source_groups("${SOURCE_FILES}" "${OTHER_FILES}")

include_directories(${INCLUDE_PATHS})
target_include_directories(${BURGERLIB_NAME} INTERFACE ${EXPORTED_INCLUDE_PATHS})
target_link_libraries(${BURGERLIB_NAME} ${LIBSDL_NAME})
