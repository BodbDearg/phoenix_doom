set(BASE_DIR "openal-soft")

set(INCLUDE_PATHS
    "${BASE_DIR}/Alc"
    "${BASE_DIR}/common"
    "${BASE_DIR}/include"
    "${BASE_DIR}/OpenAL32/include"
    "."
)

set(EXPORTED_INCLUDE_PATHS
    "${BASE_DIR}/include"
)

set(SOURCES
    "${BASE_DIR}/OpenAL32/alAuxEffectSlot.c"
    "${BASE_DIR}/OpenAL32/alBuffer.c"
    "${BASE_DIR}/OpenAL32/alEffect.c"
    "${BASE_DIR}/OpenAL32/alError.c"
    "${BASE_DIR}/OpenAL32/alExtension.c"
    "${BASE_DIR}/OpenAL32/alFilter.c"
    "${BASE_DIR}/OpenAL32/alListener.c"
    "${BASE_DIR}/OpenAL32/alSource.c"
    "${BASE_DIR}/OpenAL32/alState.c"
    "${BASE_DIR}/OpenAL32/event.c"
    "${BASE_DIR}/OpenAL32/Include"
    "${BASE_DIR}/OpenAL32/Include/alAuxEffectSlot.h"
    "${BASE_DIR}/OpenAL32/Include/alBuffer.h"
    "${BASE_DIR}/OpenAL32/Include/alEffect.h"
    "${BASE_DIR}/OpenAL32/Include/alError.h"
    "${BASE_DIR}/OpenAL32/Include/alFilter.h"
    "${BASE_DIR}/OpenAL32/Include/alListener.h"
    "${BASE_DIR}/OpenAL32/Include/alMain.h"
    "${BASE_DIR}/OpenAL32/Include/alSource.h"
    "${BASE_DIR}/OpenAL32/Include/alu.h"
    "${BASE_DIR}/OpenAL32/Include/bs2b.h"
    "${BASE_DIR}/OpenAL32/Include/sample_cvt.h"
    "${BASE_DIR}/OpenAL32/sample_cvt.c"
    "config.h"
)

set(SOURCES_WINDOWS
)

set(SOURCES_MAC
)

if (PLATFORM_WINDOWS)
    list(APPEND SOURCES ${SOURCES_WINDOWS})
endif()

if (PLATFORM_MAC)
    list(APPEND SOURCES ${SOURCES_MAC})
endif()

add_library(${OPENAL_LIB_NAME} ${SOURCES})
include_directories(${INCLUDE_PATHS})
target_include_directories(${OPENAL_LIB_NAME} INTERFACE ${EXPORTED_INCLUDE_PATHS})
