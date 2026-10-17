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
    src/aim.c
    src/aim_input.c
    src/assets.c
    src/coords.c
    src/course.c
    src/course_settings.c
    src/feedback.c
    src/hud.c
    src/input.c
    src/objects.c
    src/physics.c
    src/ramp.c
    src/shot.c
    src/splash.c
    src/terrain.c
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
set(COURSE_GENERATED_MAPS)
set(SDK_TILED_CONVERTER "$ENV{ZVB_SDK_PATH}/tools/tiled2zeal/tiled2zeal.py")
foreach(course_index RANGE 0 14)
    if(course_index LESS 10)
        set(course_suffix "000${course_index}")
    else()
        set(course_suffix "00${course_index}")
    endif()
    list(APPEND COURSE_GENERATED_MAPS "${CMAKE_BINARY_DIR}/assets/minigolf${course_suffix}.ztm")
endforeach()

# The SDK handles the multi-screen conversion and compression. List all outputs
# explicitly because this atlas produces fifteen .ztm files.
add_custom_command(OUTPUT ${COURSE_GENERATED_MAPS}
    COMMAND ${Python3_EXECUTABLE} "${SDK_TILED_CONVERTER}"
        -i "${CMAKE_SOURCE_DIR}/assets/minigolf.tmx" -o "${CMAKE_BINARY_DIR}/assets"
        -s 40x30 -z
    DEPENDS "${CMAKE_SOURCE_DIR}/assets/minigolf.tmx" "${SDK_TILED_CONVERTER}" VERBATIM)
set(SDK_SPLASH_MAPS "${CMAKE_BINARY_DIR}/assets/splash0000.ztm" "${CMAKE_BINARY_DIR}/assets/splash0001.ztm")
add_custom_command(OUTPUT ${SDK_SPLASH_MAPS}
    COMMAND ${Python3_EXECUTABLE} "${SDK_TILED_CONVERTER}"
        -i "${CMAKE_SOURCE_DIR}/assets/splash.tmx" -o "${CMAKE_BINARY_DIR}/assets" -z
    DEPENDS "${CMAKE_SOURCE_DIR}/assets/splash.tmx" "${SDK_TILED_CONVERTER}" VERBATIM)
add_custom_target(${target_name}_sdk_splash_maps DEPENDS ${SDK_SPLASH_MAPS})
add_dependencies(${target_name} ${target_name}_sdk_splash_maps)

# SDK outputs stay generated; gameplay settings are ordinary maintained C tables.
add_custom_target(${target_name}_course_maps DEPENDS ${COURSE_GENERATED_MAPS})
add_dependencies(${target_name} ${target_name}_course_maps)
target_include_directories(${target_name} PRIVATE "${CMAKE_SOURCE_DIR}/src")

# -------------------------------
# Assets
# -------------------------------
gif2zeal(${target_name} FILES
    "assets/minigolf.gif"
    STRIP 182
    BIT 4
    COLORS 16
)


# Assembly incbin input changes must also invalidate assets.c.
set_property(SOURCE src/assets.c APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_BINARY_DIR}/assets/minigolf.zts;${CMAKE_BINARY_DIR}/assets/minigolf.ztp;${COURSE_GENERATED_MAPS};${SDK_SPLASH_MAPS}")

# Map bytes come from SDK tiled2zeal. Gameplay settings live in src/course_settings.c and src/splash_settings.h.

# -------------------------------
# Convert to binary
# -------------------------------
zgdk_add_outputs(${target_name})
add_custom_target(${target_name}_memory_check ALL
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/check_link_map.py"
        "${CMAKE_SOURCE_DIR}/debug/${target_name}.map"
    DEPENDS ${target_name}_bin
    VERBATIM
)
