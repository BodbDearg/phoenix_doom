set(BASE_DIR        "libsoundio")
set(SRC_DIR         "${BASE_DIR}/src")
set(INCLUDE_DIR     "${BASE_DIR}/soundio")

set(INCLUDE_PATHS
    "${BASE_DIR}"
    "."
)

set(EXPORTED_INCLUDE_PATHS
    "${BASE_DIR}"
)

set(SOURCES
    "${INCLUDE_DIR}/endian.h"
    "${INCLUDE_DIR}/soundio.h"
    "${SRC_DIR}/atomics.h"
    "${SRC_DIR}/channel_layout.c"
    "${SRC_DIR}/config.h.in"
    "${SRC_DIR}/dummy.c"
    "${SRC_DIR}/dummy.h"
    "${SRC_DIR}/list.h"
    "${SRC_DIR}/os.c"
    "${SRC_DIR}/os.h"
    "${SRC_DIR}/ring_buffer.c"
    "${SRC_DIR}/ring_buffer.h"
    "${SRC_DIR}/soundio.c"
    "${SRC_DIR}/soundio_internal.h"
    "${SRC_DIR}/soundio_private.h"
    "${SRC_DIR}/util.c"
    "${SRC_DIR}/util.h"
)

set(SOURCES_WINDOWS
    "${SRC_DIR}/wasapi.c"
    "${SRC_DIR}/wasapi.h"    
)

set(SOURCES_MAC
    "${SRC_DIR}/coreaudio.c"
    "${SRC_DIR}/coreaudio.h"
)

if (PLATFORM_WINDOWS)
    list(APPEND SOURCES ${SOURCES_WINDOWS})
endif()

if (PLATFORM_MAC)
    list(APPEND SOURCES ${SOURCES_MAC})
endif()

if (COMPILER_MSVC)
    # MSVC: Compile as C++ code so we can access standard library atomic stuff.
    # This compiler does not properly support C11 yet (sigh) and we need C11 for that stuff...
    add_compile_options(/TP)
endif()

add_library(${LIBSOUNDIO_NAME} ${SOURCES})
include_directories(${INCLUDE_PATHS})
target_include_directories(${LIBSOUNDIO_NAME} INTERFACE ${EXPORTED_INCLUDE_PATHS})
