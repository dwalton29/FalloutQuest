cmake_minimum_required(VERSION 3.22.1)
project(falloutquest LANGUAGES C CXX)

find_package(OpenXR REQUIRED CONFIG)

add_library(falloutquest SHARED
    native-lib.cpp
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

target_include_directories(falloutquest PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
)

# openxr_platform.h expects Android JNI and EGL platform types to already be
# declared when XR_USE_PLATFORM_ANDROID / XR_USE_GRAPHICS_API_OPENGL_ES are set.
# Force-include them before native-lib.cpp's OpenXR headers so the same source
# remains portable while we iterate on the Quest runtime.
target_compile_options(falloutquest PRIVATE
    -include jni.h
    -include EGL/egl.h
)

find_library(android-lib android)
find_library(log-lib log)
find_library(egl-lib EGL)
find_library(gles-lib GLESv3)

target_link_libraries(falloutquest
    OpenXR::openxr_loader
    ${android-lib}
    ${log-lib}
    ${egl-lib}
    ${gles-lib}
)
