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
    rendering/mesh/fo3-static-nif.cpp
    physics/fo3-nif-collision-parser.cpp
    physics/fo3-collision-runtime.cpp
    data/fo3-bsa-reader.cpp
    data/fo3-bsa-archive.cpp
    data/fo3-asset-store.cpp
    data/fo3-esm-reader.cpp
    data/fo3-texture-bsa.cpp
    rendering/water/fo3-water.cpp
    npc/fo3-npc.cpp
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
