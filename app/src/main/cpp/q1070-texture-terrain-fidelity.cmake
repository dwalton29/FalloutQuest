# Q10.7: make the Q10.6 terrain material path robust on Quest and preserve
# texture detail at VR grazing angles. No geometry/collision changes.

# LAND UVs are aligned to game X/Y, which map to renderer +X/-Z. Build the
# tangent basis directly from that authored mapping instead of fragment
# derivatives; this is cheaper, deterministic and avoids mobile derivative
# precision issues while retaining the TX01 tangent-space normal map.
set(Q1070_OLD_TERRAIN_NORMAL [=[
            vec3 N = normalize(vNormal);
            vec4 normalGloss = vec4(0.5, 0.5, 1.0, 0.0);
            if (uUseNormal != 0) {
                normalGloss = texture(uNormalGloss, vTexCoord);
                vec3 tangentNormal = normalGloss.xyz * 2.0 - 1.0;
                vec3 dp1 = dFdx(vPosition);
                vec3 dp2 = dFdy(vPosition);
                vec2 duv1 = dFdx(vTexCoord);
                vec2 duv2 = dFdy(vTexCoord);
                float determinant = duv1.x * duv2.y - duv1.y * duv2.x;
                if (abs(determinant) > 0.000001) {
                    vec3 T = normalize(dp1 * duv2.y - dp2 * duv1.y);
                    T = normalize(T - N * dot(N, T));
                    vec3 B = normalize(cross(N, T));
                    if (determinant < 0.0) B = -B;
                    N = normalize(mat3(T, B, N) * tangentNormal);
                }
            }
]=])
set(Q1070_NEW_TERRAIN_NORMAL [=[
            vec3 N = normalize(vNormal);
            vec4 normalGloss = vec4(0.5, 0.5, 1.0, 0.0);
            if (uUseNormal != 0) {
                normalGloss = texture(uNormalGloss, vTexCoord);
                vec3 tangentNormal = normalize(normalGloss.xyz * 2.0 - 1.0);
                vec3 authoredU = vec3(1.0, 0.0, 0.0);
                vec3 T = authoredU - N * dot(N, authoredU);
                if (dot(T, T) < 0.000001) T = vec3(0.0, 0.0, -1.0);
                T = normalize(T);
                // Increasing LAND V follows game +Y, which is renderer -Z.
                vec3 B = normalize(cross(N, T));
                N = normalize(mat3(T, B, N) * tangentNormal);
            }
]=])
string(REPLACE "${Q1070_OLD_TERRAIN_NORMAL}" "${Q1070_NEW_TERRAIN_NORMAL}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Anisotropic filtering is one of the largest perceived-quality wins for LAND
# in a headset because most of it is viewed at a steep angle. Apply 4x only
# when the Quest driver advertises EXT_texture_filter_anisotropic.
string(REPLACE
    "#include <cstdint>\n#include <string>"
    "#include <cstdint>\n#include <cstring>\n#include <string>"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
set(Q1070_TERRAIN_ANISO_HELPER [=[
void Q1070ApplyTerrainAnisotropy() {
    static bool checked = false;
    static float level = 1.0f;
    static bool logged = false;
    if (!checked) {
        checked = true;
        const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (extensions && std::strstr(extensions, "GL_EXT_texture_filter_anisotropic")) {
            constexpr GLenum GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT_Q1070 = 0x84FF;
            GLfloat driverMax = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT_Q1070, &driverMax);
            level = std::max(1.0f, std::min(4.0f, static_cast<float>(driverMax)));
        }
    }
    if (level > 1.0f) {
        constexpr GLenum GL_TEXTURE_MAX_ANISOTROPY_EXT_Q1070 = 0x84FE;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT_Q1070, level);
    }
    if (!logged) {
        logged = true;
        Q76B_LOGI("Q10.7 TERRAIN FILTER: anisotropy=%.1fx trilinear=1 source=Quest-GLES-extension", level);
    }
}

]=])
string(REPLACE
    "GLuint Q711UploadTexture(const std::string& path) {"
    "${Q1070_TERRAIN_ANISO_HELPER}GLuint Q711UploadTexture(const std::string& path) {"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);\n    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);"
    "    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);\n    Q1070ApplyTerrainAnisotropy();\n    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Stage-specific runtime proof. The previous Q10.6 index log proves only that
# TXST records were found; these lines prove the actual GPU terrain path became
# active and identify shader/GPU failures immediately.
string(REPLACE
    "    q76bProgram = Q76BCreateProgram();\n    if (!q76bProgram) return false;"
    "    q76bProgram = Q76BCreateProgram();\n    if (!q76bProgram) {\n        Q76B_LOGE(\"Q10.7 TERRAIN FAILED: stage=shader-program\");\n        return false;\n    }"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q76bReady = true;"
    "    q76bReady = true;\n    size_t q1070VclrCells = 0u;\n    for (const Fo3TerrainCellQ76& c : terrain) {\n        if (GetFo3TerrainVertexColorsQ1060(c.landFormId).size() == Q76B_HEIGHT_COUNT * 3u) ++q1070VclrCells;\n    }\n    Q76B_LOGI(\"Q10.7 TERRAIN MATERIAL ACTIVE: cells=%zu VCLR=%zu normalUploads=%zu normalMisses=%zu strideFloats=12 TX01=1\", acceptedCells, q1070VclrCells, q1060NormalUploads, q1060NormalMisses);"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        Q76B_LOGE(\"Q7.20 TERRAIN GPU FAILED: worldspace=%08X glError=0x%X batches=%zu\","
    "        Q76B_LOGE(\"Q10.7 TERRAIN FAILED: stage=gpu-upload worldspace=%08X glError=0x%X batches=%zu\", worldspaceFormId, error, q711TerrainBatches.size());\n        Q76B_LOGE(\"Q7.20 TERRAIN GPU FAILED: worldspace=%08X glError=0x%X batches=%zu\","
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Apply the same conservative 4x anisotropy to static diffuse/normal/glow maps.
string(REPLACE
    "#include <cstdint>\n#include <string>"
    "#include <cstdint>\n#include <cstring>\n#include <string>"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
set(Q1070_STATIC_ANISO_HELPER [=[
void Q1070ApplyStaticAnisotropy() {
    static bool checked = false;
    static float level = 1.0f;
    static bool logged = false;
    if (!checked) {
        checked = true;
        const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (extensions && std::strstr(extensions, "GL_EXT_texture_filter_anisotropic")) {
            constexpr GLenum GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT_Q1070 = 0x84FF;
            GLfloat driverMax = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT_Q1070, &driverMax);
            level = std::max(1.0f, std::min(4.0f, static_cast<float>(driverMax)));
        }
    }
    if (level > 1.0f) {
        constexpr GLenum GL_TEXTURE_MAX_ANISOTROPY_EXT_Q1070 = 0x84FE;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT_Q1070, level);
    }
    if (!logged) {
        logged = true;
        Q6H_LOGI("Q10.7 STATIC FILTER: anisotropy=%.1fx trilinear=1", level);
    }
}

]=])
string(REPLACE
    "bool UploadTexture(const std::string& path,"
    "${Q1070_STATIC_ANISO_HELPER}bool UploadTexture(const std::string& path,"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);\n    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture.width, texture.height, 0,"
    "    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);\n    Q1070ApplyStaticAnisotropy();\n    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture.width, texture.height, 0,"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "Q10.7 TERRAIN MATERIAL ACTIVE" Q1070_TERRAIN_ACTIVE_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "Q1070ApplyTerrainAnisotropy" Q1070_TERRAIN_ANISO_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q1070ApplyStaticAnisotropy" Q1070_STATIC_ANISO_OK)
if(Q1070_TERRAIN_ACTIVE_OK LESS 0 OR Q1070_TERRAIN_ANISO_OK LESS 0 OR Q1070_STATIC_ANISO_OK LESS 0)
    message(FATAL_ERROR "Q10.7 texture/terrain patch drifted: active=${Q1070_TERRAIN_ACTIVE_OK} terrainAniso=${Q1070_TERRAIN_ANISO_OK} staticAniso=${Q1070_STATIC_ANISO_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp" "${Q720_TERRAIN_RENDER_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
