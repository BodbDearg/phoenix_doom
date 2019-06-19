cmake_minimum_required (VERSION 3.14)

project(PhoenixDoom)

set(CMAKE_C_STANDARD    11)
set(CMAKE_CXX_STANDARD  17)
set(CMAKE_BUILD_TYPE    Debug)

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
    set(PLATFORM_MAC TRUE)  # Note: could also be iOS etc. but not targetting those - can just assume Mac...
endif()

# Clang, Apple Clang, or GCC - enable debugging
if (COMPILER_CLANG OR COMPILER_GCC)
    set(CMAKE_C_FLAGS -g)
    set(CMAKE_CXX_FLAGS -g)
endif()

# Global identifiers for each project/target
set(GAME_NAME           PhoenixDoom)
set(BURGERLIB_NAME      BurgerLib)
set(LIBSDL_NAME         SDL)

# Adding individual projects and libraries
set(GAME_SRC_DIR            "${PROJECT_SOURCE_DIR}/source")
set(BURGERLIB_SRC_DIR       "${PROJECT_SOURCE_DIR}/lib")
set(THIRD_PARTY_LIBS_DIR    "${PROJECT_SOURCE_DIR}/third_party_libs")
set(LIBSDL_DIR              "${THIRD_PARTY_LIBS_DIR}/libsdl")

add_subdirectory("${LIBSDL_DIR}")
add_subdirectory("${BURGERLIB_SRC_DIR}")
add_subdirectory("${GAME_SRC_DIR}")
