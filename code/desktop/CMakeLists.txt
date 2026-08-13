cmake_minimum_required(VERSION 3.19)
# LovyanGFX's image and QR decoders are C, so both languages are needed
project(c4_desktop C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(ROBOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../robot)
set(DEPS_DIR ${CMAKE_BINARY_DIR}/deps)

# Deps are downloaded into the build tree, never into the repo.
set(LGFX_VERSION 1.2.26)
set(SDL2_VERSION 2.30.9)

function(fetch_archive name url dir)
    if(NOT EXISTS ${dir})
        message(STATUS "downloading ${name}")
        file(DOWNLOAD ${url} ${DEPS_DIR}/${name}.tar.gz STATUS status SHOW_PROGRESS)
        list(GET status 0 code)
        if(NOT code EQUAL 0)
            list(GET status 1 message)
            file(REMOVE ${DEPS_DIR}/${name}.tar.gz)
            message(FATAL_ERROR "could not download ${name}: ${message}")
        endif()
        file(ARCHIVE_EXTRACT INPUT ${DEPS_DIR}/${name}.tar.gz DESTINATION ${DEPS_DIR}/${name})
    endif()
endfunction()

fetch_archive(lgfx
    https://github.com/lovyan03/LovyanGFX/archive/refs/tags/${LGFX_VERSION}.tar.gz
    ${DEPS_DIR}/lgfx)
set(LGFX_SRC ${DEPS_DIR}/lgfx/LovyanGFX-${LGFX_VERSION}/src)

fetch_archive(sdl2
    https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-mingw.tar.gz
    ${DEPS_DIR}/sdl2)
set(SDL2_ROOT ${DEPS_DIR}/sdl2/SDL2-${SDL2_VERSION}/x86_64-w64-mingw32)

# Only the subset LovyanGFX's own SDL example builds; the ESP32 platform
# sources are not portable and are deliberately left out.
file(GLOB LGFX_SOURCES CONFIGURE_DEPENDS
    ${LGFX_SRC}/lgfx/Fonts/efont/*.c
    ${LGFX_SRC}/lgfx/Fonts/IPA/*.c
    ${LGFX_SRC}/lgfx/Fonts/lvgl/*.c
    ${LGFX_SRC}/lgfx/utility/*.c
    ${LGFX_SRC}/lgfx/v1/*.cpp
    ${LGFX_SRC}/lgfx/v1/lv_font/*.c
    ${LGFX_SRC}/lgfx/v1/misc/*.cpp
    ${LGFX_SRC}/lgfx/v1/panel/Panel_Device.cpp
    ${LGFX_SRC}/lgfx/v1/panel/Panel_FrameBufferBase.cpp
    ${LGFX_SRC}/lgfx/v1/platforms/sdl/*.cpp)

# The firmware's own sources, compiled unmodified
set(SHARED_SOURCES
    ${ROBOT_DIR}/src/board.cpp
    ${ROBOT_DIR}/src/memo.cpp
    ${ROBOT_DIR}/src/player.cpp
    ${ROBOT_DIR}/src/main.cpp
    ${ROBOT_DIR}/src/platform.cpp
    ${ROBOT_DIR}/src/ui.cpp)

add_executable(c4_desktop
    ${SHARED_SOURCES}
    ${LGFX_SOURCES}
    src/main.cpp)

# LGFX_USE_V1 comes from display.h so both targets agree on it
target_compile_definitions(c4_desktop PRIVATE C4_DESKTOP LGFX_SDL)

target_include_directories(c4_desktop PRIVATE
    ${ROBOT_DIR}/include
    ${LGFX_SRC}
    ${SDL2_ROOT}/include
    ${SDL2_ROOT}/include/SDL2)

target_link_directories(c4_desktop PRIVATE ${SDL2_ROOT}/lib)
target_link_libraries(c4_desktop PRIVATE mingw32 SDL2main SDL2 pthread)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # The warning set applies to our code only; LovyanGFX is noisy under -Wextra
    set_source_files_properties(${SHARED_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra")
endif()

# SDL2 links against its DLL, so the exe needs it alongside
add_custom_command(TARGET c4_desktop POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL2_ROOT}/bin/SDL2.dll $<TARGET_FILE_DIR:c4_desktop>)
