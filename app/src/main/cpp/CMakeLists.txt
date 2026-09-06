cmake_minimum_required(VERSION 3.22.1)
project(falloutquest LANGUAGES C CXX)

find_package(OpenXR REQUIRED CONFIG)

# Q6B is generated from the proven Q6A renderer so the OpenXR/GLES path stays
# byte-for-byte familiar while selection changes from a generic proof cluster to
# a coherent, ESM-authored room around the real Megaton player-house bed.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/q6a-native.cpp" Q6B_NATIVE_SOURCE)

# Give the milestone its own log/wrapper names.
string(REPLACE "Q6A" "Q6B" Q6B_NATIVE_SOURCE "${Q6B_NATIVE_SOURCE}")

# A room needs more objects and larger architectural pieces than the Q6A proof.
string(REPLACE
    "constexpr float SCENE_FORWARD = -3.20f;"
    "constexpr float SCENE_FORWARD = -2.50f;"
    Q6B_NATIVE_SOURCE "${Q6B_NATIVE_SOURCE}")
string(REPLACE
    "constexpr size_t MAX_SCENE_OBJECTS = 8u;"
    "constexpr size_t MAX_SCENE_OBJECTS = 18u;"
    Q6B_NATIVE_SOURCE "${Q6B_NATIVE_SOURCE}")
string(REPLACE
    "constexpr size_t MAX_MODEL_ATTEMPTS = 48u;"
    "constexpr size_t MAX_MODEL_ATTEMPTS = 128u;"
    Q6B_NATIVE_SOURCE "${Q6B_NATIVE_SOURCE}")
string(REPLACE
    "constexpr float MAX_MODEL_EXTENT_UNITS = 500.0f;"
    "constexpr float MAX_MODEL_EXTENT_UNITS = 1600.0f;"
    Q6B_NATIVE_SOURCE "${Q6B_NATIVE_SOURCE}")

# Q6A sorted around the centroid of all 916 model placements, which was useful
# for proving the pipeline but produced a random-looking sample. Q6B finds the
# actual HD01PlayerBed02 REFR and ranks objects around that Bethesda-authored
# location. Interior architecture is strongly favoured, followed by doors and
# furniture. XMarkers are deliberately sent to the end.
set(Q6B_OLD_SORT [=[
    std::stable_sort(placements.begin(), placements.end(),
                     [&](const Fo3WorldPlacement& a, const Fo3WorldPlacement& b) {
        const float adx = a.x - centroidX, ady = a.y - centroidY, adz = a.z - centroidZ;
        const float bdx = b.x - centroidX, bdy = b.y - centroidY, bdz = b.z - centroidZ;
        return adx * adx + ady * ady + adz * adz < bdx * bdx + bdy * bdy + bdz * bdz;
    });
]=])
set(Q6B_NEW_SORT [=[
    float anchorX = centroidX, anchorY = centroidY, anchorZ = centroidZ;
    bool foundBedAnchor = false;
    for (const Fo3WorldPlacement& p : placements) {
        if (p.editorId == "HD01PlayerBed02") {
            anchorX = p.x;
            anchorY = p.y;
            anchorZ = p.z;
            foundBedAnchor = true;
            break;
        }
    }
    if (!foundBedAnchor) {
        for (const Fo3WorldPlacement& p : placements) {
            if (p.editorId == "Chair03F") {
                anchorX = p.x;
                anchorY = p.y;
                anchorZ = p.z;
                break;
            }
        }
    }

    auto roomScore = [&](const Fo3WorldPlacement& p) {
        if (p.editorId == "XMarker") return 1.0e30f;
        const float dx = p.x - anchorX;
        const float dy = p.y - anchorY;
        const float dz = p.z - anchorZ;
        float score = dx * dx + dy * dy + dz * dz * 2.5f;

        const bool megatonInterior =
            p.modelPath.find("Architecture\\Megaton\\interior\\ShackInteriors") != std::string::npos ||
            p.modelPath.find("architecture\\megaton\\interior\\shackinteriors") != std::string::npos;
        if (megatonInterior) score *= 0.20f;
        if (p.baseRecordType == "DOOR") score *= 0.40f;
        if (p.baseRecordType == "FURN") score *= 0.55f;
        if (p.editorId == "HD01PlayerBed02") score = -3.0f;
        if (p.editorId == "Chair03F") score = -2.0f;
        return score;
    };

    Q6B_LOGI("Q6B ROOM ANCHOR: source=%s P=(%.1f %.1f %.1f)",
             foundBedAnchor ? "HD01PlayerBed02" : "fallback",
             anchorX, anchorY, anchorZ);

    std::stable_sort(placements.begin(), placements.end(),
                     [&](const Fo3WorldPlacement& a, const Fo3WorldPlacement& b) {
        return roomScore(a) < roomScore(b);
    });
]=])
string(REPLACE "${Q6B_OLD_SORT}" "${Q6B_NEW_SORT}"
       Q6B_NATIVE_SOURCE "${Q6B_NATIVE_SOURCE}")

# q6b-native.cpp still includes the proven Q4 renderer as its OpenXR host. Both
# use two generic helper names in anonymous namespaces, so rename only Q6B's.
string(REPLACE
    "GLuint CompileShader(GLenum type, const char* source)"
    "GLuint CompileQ6BShader(GLenum type, const char* source)"
    Q6B_NATIVE_SOURCE "${Q6B_NATIVE_SOURCE}")
string(REPLACE
    "GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSource);"
    "GLuint vs = CompileQ6BShader(GL_VERTEX_SHADER, vertexSource);"
    Q6B_NATIVE_SOURCE "${Q6B_NATIVE_SOURCE}")
string(REPLACE
    "GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);"
    "GLuint fs = CompileQ6BShader(GL_FRAGMENT_SHADER, fragmentSource);"
    Q6B_NATIVE_SOURCE "${Q6B_NATIVE_SOURCE}")
string(REPLACE
    "GLuint CreateProgram()"
    "GLuint CreateQ6BProgram()"
    Q6B_NATIVE_SOURCE "${Q6B_NATIVE_SOURCE}")
string(REPLACE
    "gProgram = CreateProgram();"
    "gProgram = CreateQ6BProgram();"
    Q6B_NATIVE_SOURCE "${Q6B_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6b-native-generated.cpp" "${Q6B_NATIVE_SOURCE}")

add_library(falloutquest SHARED
    ${CMAKE_CURRENT_BINARY_DIR}/q6b-native-generated.cpp
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
