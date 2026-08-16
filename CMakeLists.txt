cmake_minimum_required(VERSION 3.21)

project(potato LANGUAGES CXX VERSION 1.0.0)

if(NOT MSVC)
    message(FATAL_ERROR "Potato targets the Windows SDK through MSVC; no other toolchain is supported.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Statically linked CRT, so the executable runs with no redistributable. Set
# before the platform layer is pulled in, so it is built the same way.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Both configurations carry debug information; Release keeps the optimisation
# settings the performance notes in AGENTS.md were measured against.
set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /RTC1 /D_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "/Ox /Oi /Ot /Oy /GT /GF /Gy /fp:fast /Zi /DNDEBUG")

# Develop against a sibling platform-h checkout when one exists; CI and a fresh
# clone fall back to the pinned revision below.
if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_PLATFORM_H
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../platform-h/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_PLATFORM_H "${CMAKE_CURRENT_SOURCE_DIR}/../platform-h"
        CACHE PATH "Local platform-h checkout to build against")
endif()

include(FetchContent)
FetchContent_Declare(platform_h
    GIT_REPOSITORY https://github.com/ZacWalk/platform-h.git
    GIT_TAG        8e9be5de233d1797ce7a2b50fe663042469611a9
)
FetchContent_MakeAvailable(platform_h)

platform_add_app(potato
    SOURCES
        src/core.cpp
        src/document.cpp
        src/element.cpp
        src/main.cpp
        src/style.cpp
    ICON        src/res/potato.ico
    DESCRIPTION "Potato browser"
    OUTPUT_NAME potato-64
    EMBED
        src/res/master.css
        src/res/test.htm
)

target_include_directories(potato PRIVATE src)

target_compile_definitions(potato PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS)

target_compile_options(potato PRIVATE /W3 /sdl /MP)

target_link_options(potato PRIVATE
    /DEBUG
    $<$<CONFIG:Release>:/OPT:REF>
    $<$<CONFIG:Release>:/OPT:ICF>
)

set_target_properties(potato PROPERTIES
    DEBUG_POSTFIX d
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Exe"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/Exe"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Exe"
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
)

target_precompile_headers(potato PRIVATE src/pch.h)
