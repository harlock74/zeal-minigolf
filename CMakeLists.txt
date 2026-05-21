cmake_minimum_required(VERSION 3.16)

# -------------------------------
# CMake Setup
# -------------------------------
set(APP_VERSION_MAJOR 0)
set(APP_VERSION_MINOR 0)
set(APP_VERSION_PATCH 0)
set(APP_NAME "zgolf")
set(target_name "zgolf")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin)

# -------------------------------
# Toolchain
# -------------------------------
set(ZOS_TOOLCHAIN sdcc)
include($ENV{ZOS_PATH}/cmake/zos_init.cmake)

# -------------------------------
# Project
# -------------------------------
project(${target_name} C ASM)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# -------------------------------
# Executable
# -------------------------------
add_executable(${target_name}
    src/main.c
    src/assets.c
    src/course.c
    src/hud.c
    src/input.c
    src/objects.c
)

# -------------------------------
# Include and link SDKs
# -------------------------------
zos_use(coreutils)
find_package(COREUTILS REQUIRED)

zos_use(zgdk)
find_package(ZGDK REQUIRED)

zos_use(zvb)
find_package(ZVB_SDK REQUIRED)

target_link_libraries(${target_name} PUBLIC zvb_gfx zvb_sound)
target_link_libraries(${target_name} PUBLIC core)
target_link_libraries(${target_name} PUBLIC zgdk)

# -------------------------------
# Generated Course Asset Symbols
# -------------------------------
set(COURSE_WIDTH_TILES 40)
set(COURSE_HEIGHT_TILES 30)
set(GAME_COURSE_COUNT 15)
set(COURSE_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(COURSE_GENERATED_HEADER "${COURSE_GENERATED_DIR}/course_assets.generated.h")
set(COURSE_GENERATED_SOURCE "${COURSE_GENERATED_DIR}/course_assets.generated.inc")
set(COURSE_INFO_GENERATED_SOURCE "${COURSE_GENERATED_DIR}/course_infos.generated.c")

add_custom_command(
    OUTPUT "${COURSE_GENERATED_HEADER}" "${COURSE_GENERATED_SOURCE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${COURSE_GENERATED_DIR}"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/gen_course_assets.py"
            --tmx "${CMAKE_SOURCE_DIR}/assets/minigolf.tmx"
            --course-width-tiles ${COURSE_WIDTH_TILES}
            --course-height-tiles ${COURSE_HEIGHT_TILES}
            --header "${COURSE_GENERATED_HEADER}"
            --source "${COURSE_GENERATED_SOURCE}"
    DEPENDS
            "${CMAKE_SOURCE_DIR}/tools/gen_course_assets.py"
            "${CMAKE_SOURCE_DIR}/assets/minigolf.tmx"
    COMMENT "Generating course tilemap asset symbols"
    VERBATIM
)

add_custom_target(${target_name}_course_asset_codegen
    DEPENDS "${COURSE_GENERATED_HEADER}" "${COURSE_GENERATED_SOURCE}"
)
add_dependencies(${target_name} ${target_name}_course_asset_codegen)

add_custom_command(
    OUTPUT "${COURSE_INFO_GENERATED_SOURCE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${COURSE_GENERATED_DIR}"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/gen_course_markers.py"
            --tmx "${CMAKE_SOURCE_DIR}/assets/minigolf.tmx"
            --course-width-tiles ${COURSE_WIDTH_TILES}
            --course-height-tiles ${COURSE_HEIGHT_TILES}
            --tile-size 16
            --course-count ${GAME_COURSE_COUNT}
            --config "${CMAKE_SOURCE_DIR}/src/config.h"
            --output "${COURSE_INFO_GENERATED_SOURCE}"
    DEPENDS
            "${CMAKE_SOURCE_DIR}/tools/gen_course_markers.py"
            "${CMAKE_SOURCE_DIR}/assets/minigolf.tmx"
            "${CMAKE_SOURCE_DIR}/src/config.h"
    COMMENT "Generating course marker metadata"
    VERBATIM
)

add_custom_target(${target_name}_course_marker_codegen
    DEPENDS "${COURSE_INFO_GENERATED_SOURCE}"
)
add_dependencies(${target_name} ${target_name}_course_marker_codegen)
target_sources(${target_name} PRIVATE "${COURSE_INFO_GENERATED_SOURCE}")
target_include_directories(${target_name} PRIVATE "${COURSE_GENERATED_DIR}" "${CMAKE_SOURCE_DIR}/src")

# -------------------------------
# Assets
# -------------------------------
gif2zeal(${target_name} FILES
    "assets/minigolf.gif"
    STRIP 192
    BIT 4
    COLORS 16
)

tiled2zeal(${target_name} FILES
    "assets/minigolf.tmx"
    VERBOSE
    LAYER 0
    SIZE ${COURSE_WIDTH_TILES}x${COURSE_HEIGHT_TILES}
)

# -------------------------------
# Convert to binary
# -------------------------------
zgdk_add_outputs(${target_name})
