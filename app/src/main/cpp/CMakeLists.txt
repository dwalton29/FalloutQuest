cmake_minimum_required(VERSION 3.22.1)
project(falloutquest LANGUAGES C CXX)

find_package(OpenXR REQUIRED CONFIG)

# Native runtime sources.
add_library(falloutquest SHARED
    core/fo3-runtime.cpp
    world/fo3-megaton-scene.cpp
    world/fo3-worldspace-runtime.cpp
    world/fo3-world-streaming.cpp
    world/fo3-cell-world.cpp
    world/interaction/fo3-door-resources.cpp
    world/fo3-resident-authored-admission.cpp
    scripting/fo3-script-master-match.cpp
    rendering/mesh/fo3-static-nif.cpp
    physics/fo3-nif-collision-parser.cpp
    physics/fo3-collision-runtime.cpp
    data/fo3-bsa-reader.cpp
    data/fo3-bsa-archive.cpp
    data/fo3-asset-store.cpp
    rendering/environment/fo3-interior-lighting.cpp
    data/fo3-esm-reader.cpp
    data/fo3-xtel-index.cpp
    npc/fo3-unloaded-ai.cpp
    npc/fo3-npc-conversation.cpp
    data/fo3-texture-bsa.cpp
    rendering/water/fo3-water.cpp
    npc/fo3-npc.cpp
    npc/fo3-actor-animation.cpp
    player/fo3-player-state.cpp
    weapons/fo3-weapon-data.cpp
    weapons/fo3-weapon-player.cpp
    weapons/fo3-weapon-assets.cpp
    player/fo3-pipboy-player.cpp
    pipboy/fo3-pipboy-data.cpp
    pipboy/fo3-pipboy-session.cpp
    pipboy/fo3-radio-runtime.cpp
    dialogue/fo3-dialogue-conditions.cpp
    dialogue/fo3-dialogue-results.cpp
    dialogue/fo3-dialogue-session.cpp
    player/fo3-container-state.cpp
    audio/fo3-audio.cpp
    audio/fo3-audio-catalog.cpp
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

target_include_directories(falloutquest PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/data
    ${CMAKE_CURRENT_SOURCE_DIR}/world
    ${CMAKE_CURRENT_SOURCE_DIR}/world/interaction
    ${CMAKE_CURRENT_SOURCE_DIR}/rendering
    ${CMAKE_CURRENT_SOURCE_DIR}/rendering/mesh
    ${CMAKE_CURRENT_SOURCE_DIR}/rendering/terrain
    ${CMAKE_CURRENT_SOURCE_DIR}/physics
    ${CMAKE_CURRENT_SOURCE_DIR}/player
    ${CMAKE_CURRENT_SOURCE_DIR}/vr
    ${CMAKE_CURRENT_SOURCE_DIR}/npc
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/interaction
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/loading
)

target_compile_options(falloutquest PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/vr/openxr-platform-prelude.h
)

execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE FQ_SOURCE_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT FQ_SOURCE_COMMIT)
    set(FQ_SOURCE_COMMIT "unknown")
endif()
# Keep the build SHA accurate in fo3-runtime-loop.inc, which is included by
# fo3-runtime.cpp. Defining it for the whole target used to invalidate every
# native translation unit on every GitHub push, defeating incremental builds.
set_source_files_properties(core/fo3-runtime.cpp PROPERTIES
    COMPILE_DEFINITIONS "FQ_SOURCE_COMMIT=\"${FQ_SOURCE_COMMIT}\"")

# CI uses a persistent ccache directory between ephemeral runners. Developers
# keep the normal compiler unless they explicitly opt in.
if(DEFINED ENV{FALLOUTQUEST_USE_CCACHE} AND "$ENV{FALLOUTQUEST_USE_CCACHE}" STREQUAL "1")
    find_program(FQ_CCACHE_PROGRAM ccache REQUIRED)
    set_property(TARGET falloutquest PROPERTY C_COMPILER_LAUNCHER "${FQ_CCACHE_PROGRAM}")
    set_property(TARGET falloutquest PROPERTY CXX_COMPILER_LAUNCHER "${FQ_CCACHE_PROGRAM}")
endif()

# Profile uses the SAME debug-enabled source and diagnostics but emits
# optimised native machine code. Keep this in CMake rather than Gradle
# cppFlags so -O2 appears after CMAKE_CXX_FLAGS_DEBUG's -O0.
option(FALLOUTQUEST_OPTIMIZED_PROFILE "Enable native -O2 without NDEBUG" OFF)
if(FALLOUTQUEST_OPTIMIZED_PROFILE)
    target_compile_options(falloutquest PRIVATE -O2)
    target_compile_definitions(falloutquest PRIVATE FQ_OPTIMIZED_PROFILE=1)
endif()

find_library(android-lib android)
find_library(log-lib log)
find_library(egl-lib EGL)
find_library(gles-lib GLESv3)
find_library(z-lib z)

target_link_libraries(falloutquest
    OpenXR::openxr_loader
    ${android-lib}
    ${log-lib}
    ${egl-lib}
    ${gles-lib}
    ${z-lib}
)

# Native Vulkan migration stays opt-in while GLES owns gameplay presentation.
# OFF also verifies the previous GLES-only build without Vulkan dependencies.
option(FALLOUTQUEST_VULKAN "Build Vulkan world validation backend" ON)
if(FALLOUTQUEST_VULKAN)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_program(FQ_SHADER_COMPILER NAMES glslc glslangValidator
        HINTS "${ANDROID_NDK}/shader-tools/linux-x86_64"
              "${ANDROID_NDK}/shader-tools/darwin-x86_64"
              "${ANDROID_NDK}/shader-tools/windows-x86_64" REQUIRED)
    set(FQ_VULKAN_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/vulkan-shaders")
    add_custom_command(OUTPUT "${FQ_VULKAN_GENERATED}/fo3-vulkan-shaders.h"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/../../../../tools/build_vulkan_shaders.py"
            --compiler "${FQ_SHADER_COMPILER}" --output "${FQ_VULKAN_GENERATED}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../../../tools/build_vulkan_shaders.py"
                core/fo3-runtime.cpp rendering/terrain/fo3-terrain-v237-shaders.h
                rendering/environment/fo3-pc-sky.h
        VERBATIM)
    target_sources(falloutquest PRIVATE rendering/vulkan/fo3-vulkan-renderer.cpp
        "${FQ_VULKAN_GENERATED}/fo3-vulkan-shaders.h")
    target_include_directories(falloutquest PRIVATE "${FQ_VULKAN_GENERATED}")
    target_compile_definitions(falloutquest PRIVATE FQ_ENABLE_VULKAN=1)
    find_library(vulkan-lib vulkan REQUIRED)
    target_link_libraries(falloutquest ${vulkan-lib})
endif()
