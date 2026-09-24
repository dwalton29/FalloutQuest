cmake_minimum_required(VERSION 3.22.1)
project(falloutquest LANGUAGES C CXX)

find_package(OpenXR REQUIRED CONFIG)

# Q17 consolidated baseline: normal sources, no generated-source patch chain.
add_library(falloutquest SHARED
    fo3-runtime.cpp
    fo3-megaton-scene.cpp
    fo3-cell-world.cpp
    fo3-static-nif-runtime.cpp
    fo3-nif-collision.cpp
    fo3-collision-runtime.cpp
    fo3-bsa-reader.cpp
    fo3-texture-bsa.cpp
    fo3-data-probe.cpp
    fo3-esm-scan.cpp
    fo3-megaton-contents.cpp
    fo3-bsa-extract.cpp
    fo3-fog-ab-q1450.cpp
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

target_include_directories(falloutquest PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(falloutquest PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/openxr-platform-prelude.h
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
