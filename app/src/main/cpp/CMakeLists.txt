cmake_minimum_required(VERSION 3.22.1)
project(falloutquest LANGUAGES C CXX)

find_package(OpenXR REQUIRED CONFIG)

add_library(falloutquest SHARED
    q4-native.cpp
    fo3-data-probe.cpp
    fo3-esm-scan.cpp
    fo3-megaton-contents.cpp
    fo3-bsa-extract.cpp
    fo3-nif-mesh.cpp
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

target_include_directories(falloutquest PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# openxr_platform.h expects Android JNI and EGL platform types to already be
# declared when XR_USE_PLATFORM_ANDROID / XR_USE_GRAPHICS_API_OPENGL_ES are set.
# Force-include a single prelude header before the OpenXR platform headers.
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
