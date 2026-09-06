cmake_minimum_required(VERSION 3.22.1)
project(falloutquest LANGUAGES C CXX)

find_package(OpenXR REQUIRED CONFIG)

# q6a-native.cpp includes the proven Q4 renderer as its OpenXR host. Both files
# originally used two generic helper names in anonymous namespaces. Generate the
# Q6A translation unit with only its two helpers renamed, leaving the checked-in
# Q6A source and the known-good Q4 renderer otherwise byte-for-byte untouched.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/q6a-native.cpp" Q6A_NATIVE_SOURCE)
string(REPLACE
    "GLuint CompileShader(GLenum type, const char* source)"
    "GLuint CompileQ6AShader(GLenum type, const char* source)"
    Q6A_NATIVE_SOURCE "${Q6A_NATIVE_SOURCE}")
string(REPLACE
    "GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSource);"
    "GLuint vs = CompileQ6AShader(GL_VERTEX_SHADER, vertexSource);"
    Q6A_NATIVE_SOURCE "${Q6A_NATIVE_SOURCE}")
string(REPLACE
    "GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);"
    "GLuint fs = CompileQ6AShader(GL_FRAGMENT_SHADER, fragmentSource);"
    Q6A_NATIVE_SOURCE "${Q6A_NATIVE_SOURCE}")
string(REPLACE
    "GLuint CreateProgram()"
    "GLuint CreateQ6AProgram()"
    Q6A_NATIVE_SOURCE "${Q6A_NATIVE_SOURCE}")
string(REPLACE
    "gProgram = CreateProgram();"
    "gProgram = CreateQ6AProgram();"
    Q6A_NATIVE_SOURCE "${Q6A_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6a-native-fixed.cpp" "${Q6A_NATIVE_SOURCE}")

add_library(falloutquest SHARED
    ${CMAKE_CURRENT_BINARY_DIR}/q6a-native-fixed.cpp
    fo3-megaton-scene.cpp
    fo3-static-nif.cpp
    fo3-bsa-reader.cpp
    fo3-texture-bsa.cpp
    fo3-data-probe.cpp
    fo3-esm-scan.cpp
    fo3-megaton-contents.cpp
    fo3-bsa-extract.cpp
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
