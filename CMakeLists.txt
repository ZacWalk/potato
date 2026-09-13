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
    GIT_TAG        894afaf337c722c4eafa0ec6b91fa6b83e06966e
)
FetchContent_MakeAvailable(platform_h)

# ── libwebvis ────────────────────────────────────────────────────────────────
# A standalone HTML/CSS layout and rendering engine. It deliberately does NOT
# link platform-h: everything platform-specific reaches it through the
# device_context and host interfaces in src/libwebvis/webvis.h.

# The user-agent stylesheet is compiled into the library so consumers need not
# ship a data file alongside it.
set(webvis_generated "${CMAKE_CURRENT_BINARY_DIR}/generated/webvis")
set(master_css_cpp "${webvis_generated}/master_css.cpp")

add_custom_command(
    OUTPUT "${master_css_cpp}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${webvis_generated}"
    COMMAND ${CMAKE_COMMAND}
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/src/libwebvis/res/master.css
            -DOUTPUT=${master_css_cpp}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_master_css.cmake
    DEPENDS src/libwebvis/res/master.css cmake/embed_master_css.cmake
    COMMENT "Embedding the libwebvis user-agent stylesheet"
    VERBATIM
)

add_library(webvis STATIC
    src/libwebvis/core.cpp
    src/libwebvis/document.cpp
    src/libwebvis/element.cpp
    src/libwebvis/style.cpp
    "${master_css_cpp}"
)

add_library(webvis::webvis ALIAS webvis)

target_include_directories(webvis PUBLIC src/libwebvis)
target_compile_options(webvis PRIVATE /W3 /sdl /MP)
target_precompile_headers(webvis PRIVATE src/libwebvis/pch.h)

set_target_properties(webvis PROPERTIES
    DEBUG_POSTFIX d
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
)

# ── potato ───────────────────────────────────────────────────────────────────
# The browser: window, toolbar, navigation history, and the platform-h backed
# implementations of the libwebvis device_context and host interfaces.

platform_add_app(potato
    SOURCES
        src/potato/main.cpp
        src/potato/webvis_host.cpp
    ICON        src/potato/res/potato.ico
    DESCRIPTION "Potato browser"
    OUTPUT_NAME potato-64
    EMBED
        src/potato/res/test.htm
)

target_include_directories(potato PRIVATE src/potato)

target_link_libraries(potato PRIVATE webvis::webvis)

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

target_precompile_headers(potato PRIVATE src/potato/pch.h)

include(CTest)
if(BUILD_TESTING)
    add_executable(potato_tests tests/smoke.cpp)
    add_test(NAME potato_unit COMMAND potato_tests)
    set_tests_properties(potato_unit PROPERTIES TIMEOUT 30 LABELS potato)
endif()
