#include "fo3-terrain-q76.h"
#include "fo3-texture-bsa.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr const char* Q76B_TAG = "FalloutQuest";
constexpr int Q76B_LAND_SIDE = 33;
constexpr int Q76B_LAND_QUADS = 32;
constexpr float Q76B_CELL_SIZE = 4096.0f;
constexpr float Q76B_VERTEX_SPACING = Q76B_CELL_SIZE / static_cast<float>(Q76B_LAND_QUADS);
constexpr size_t Q76B_HEIGHT_COUNT = static_cast<size_t>(Q76B_LAND_SIDE * Q76B_LAND_SIDE);
constexpr float Q711_TEXTURE_REPEAT_GAME_UNITS = 512.0f;
constexpr size_t Q711_MAX_REAL_TEXTURES = 16u;

#define Q76B_LOGI(...) __android_log_print(ANDROID_LOG_INFO, Q76B_TAG, __VA_ARGS__)
#define Q76B_LOGW(...) __android_log_print(ANDROID_LOG_WARN, Q76B_TAG, __VA_ARGS__)
#define Q76B_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, Q76B_TAG, __VA_ARGS__)

struct Q76BV3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Q711TerrainBatch {
    std::string texturePath;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint texture = 0;
    GLsizei vertexCount = 0;
    bool realTexture = false;
};

struct Q711CpuBatch {
    std::string texturePath;
    std::vector<float> vertices;
};

GLuint q76bProgram = 0;
GLint q76bMvpLocation = -1;
GLint q76bSamplerLocation = -1;
GLint q76bUseTextureLocation = -1;
std::vector<Q711TerrainBatch> q711TerrainBatches;
bool q76bReady = false;
bool q76bLoggedVisible = false;
uint32_t q76bWorldspace = 0;
size_t q76bTerrainCells = 0;
size_t q76bTriangles = 0;
size_t q711RealTextureBatches = 0;
size_t q711FallbackBatches = 0;

uint64_t Q711GridKey(int32_t x, int32_t y) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32u) |
           static_cast<uint32_t>(y);
}

Q76BV3 Q76BSub(const Q76BV3& a, const Q76BV3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Q76BV3 Q76BNormalize(Q76BV3 v) {
    const float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len < 1e-6f) return {0.0f, 1.0f, 0.0f};
    return {v.x / len, v.y / len, v.z / len};
}

Q76BV3 Q76BCross(const Q76BV3& a, const Q76BV3& b) {
    return {
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x,
    };
}

void Q711PushTriangle(std::vector<float>& out,
                      const Q76BV3& a, float au, float av,
                      const Q76BV3& b, float bu, float bv,
                      const Q76BV3& c, float cu, float cv) {
    Q76BV3 n = Q76BNormalize(Q76BCross(Q76BSub(b, a), Q76BSub(c, a)));
    if (n.y < 0.0f) n = {-n.x, -n.y, -n.z};
    const Q76BV3 points[3]{a, b, c};
    const float uv[6]{au, av, bu, bv, cu, cv};
    for (int i = 0; i < 3; ++i) {
        const Q76BV3& p = points[i];
        out.insert(out.end(), {
            p.x, p.y, p.z,
            n.x, n.y, n.z,
            uv[i * 2], uv[i * 2 + 1]
        });
    }
}

GLuint Q76BCompile(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Q76B_LOGE("Q7.11 terrain shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint Q76BCreateProgram() {
    static const char* vertexSource = R"(
        #version 300 es
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec2 aTexCoord;
        uniform mat4 uMvp;
        out vec3 vNormal;
        out vec2 vTexCoord;
        void main() {
            vNormal = aNormal;
            vTexCoord = aTexCoord;
            gl_Position = uMvp * vec4(aPosition, 1.0);
        }
    )";
    static const char* fragmentSource = R"(
        #version 300 es
        precision mediump float;
        in vec3 vNormal;
        in vec2 vTexCoord;
        uniform sampler2D uDiffuse;
        uniform int uUseTexture;
        out vec4 fragColor;
        void main() {
            vec3 N = normalize(vNormal);
            vec3 lightDirection = normalize(vec3(0.35, 0.85, 0.40));
            float lambert = max(dot(N, lightDirection), 0.0);
            vec3 earthFallback = vec3(0.34, 0.27, 0.18);
            vec3 albedo = uUseTexture != 0 ? texture(uDiffuse, vTexCoord).rgb : earthFallback;
            vec3 lit = albedo * (0.48 + 0.52 * lambert);
            fragColor = vec4(lit, 1.0);
        }
    )";

    GLuint vs = Q76BCompile(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = Q76BCompile(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        Q76B_LOGE("Q7.11 terrain shader link failed: %s", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

Q76BV3 Q76BToVr(float gameX, float gameY, float gameZ,
                 float arrivalX, float arrivalY, float arrivalZ,
                 float sceneForward, float floorY, float unitsPerMetre) {
    return {
        (gameX - arrivalX) / unitsPerMetre,
        floorY + (gameZ - arrivalZ) / unitsPerMetre,
        sceneForward - (gameY - arrivalY) / unitsPerMetre,
    };
}

GLuint Q711UploadTexture(const std::string& path) {
    if (path.empty()) return 0;
    Fo3RgbaTexture image;
    if (!LoadFalloutTextureRgba(path, image) ||
        image.width <= 0 || image.height <= 0 || image.rgba.empty()) {
        Q76B_LOGW("Q7.11 TERRAIN DDS MISS: path=%s", path.c_str());
        return 0;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 image.width, image.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        Q76B_LOGW("Q7.11 TERRAIN DDS GPU MISS: path=%s glError=0x%X", path.c_str(), error);
        if (texture) glDeleteTextures(1, &texture);
        return 0;
    }

    Q76B_LOGI("Q7.11 TERRAIN DDS READY: path=%s resolved=%s size=%dx%d format=%s",
              path.c_str(), image.sourcePath.c_str(), image.width, image.height,
              image.format.c_str());
    return texture;
}

int Q711QuadrantForQuad(int x, int y) {
    const int top = y >= 16 ? 2 : 0;
    const int right = x >= 16 ? 1 : 0;
    return top + right;
}

} // namespace

void ShutdownFo3TerrainRenderQ76() {
    for (Q711TerrainBatch& batch : q711TerrainBatches) {
        if (batch.vbo) glDeleteBuffers(1, &batch.vbo);
        if (batch.vao) glDeleteVertexArrays(1, &batch.vao);
        if (batch.texture) glDeleteTextures(1, &batch.texture);
        batch = {};
    }
    q711TerrainBatches.clear();
    if (q76bProgram) glDeleteProgram(q76bProgram);
    q76bProgram = 0;
    q76bMvpLocation = -1;
    q76bSamplerLocation = -1;
    q76bUseTextureLocation = -1;
    q76bReady = false;
    q76bLoggedVisible = false;
    q76bWorldspace = 0;
    q76bTerrainCells = 0;
    q76bTriangles = 0;
    q711RealTextureBatches = 0;
    q711FallbackBatches = 0;
}

bool InitializeFo3TerrainRenderQ76(uint32_t worldspaceFormId,
                                   float arrivalX, float arrivalY, float arrivalZ,
                                   float sceneForward, float floorY,
                                   float unitsPerMetre) {
    ShutdownFo3TerrainRenderQ76();
    if (worldspaceFormId == 0u || unitsPerMetre <= 1e-4f) return false;

    if (!LoadFo3TerrainQ76(worldspaceFormId, arrivalX, arrivalY, arrivalZ)) {
        Q76B_LOGE("Q7.11 TERRAIN GPU FAILED: worldspace=%08X reason=LAND-load", worldspaceFormId);
        return false;
    }
    const std::vector<Fo3TerrainCellQ76>& terrain = GetFo3TerrainQ76();
    if (terrain.empty()) return false;

    // Q7.11 seam stitch. Parent-world LAND and child-world LAND can disagree by
    // a few height units on a shared boundary. Average each shared edge once so
    // both meshes upload the exact same border vertices and cannot expose a gap.
    std::vector<std::vector<float>> stitchedHeights;
    stitchedHeights.reserve(terrain.size());
    std::unordered_map<uint64_t, size_t> cellByGrid;
    for (size_t i = 0; i < terrain.size(); ++i) {
        stitchedHeights.push_back(terrain[i].heights);
        if (terrain[i].heights.size() == Q76B_HEIGHT_COUNT) {
            cellByGrid[Q711GridKey(terrain[i].gridX, terrain[i].gridY)] = i;
        }
    }

    size_t stitchedEdges = 0u;
    size_t adjustedVertices = 0u;
    float maxSeamDelta = 0.0f;
    for (size_t i = 0; i < terrain.size(); ++i) {
        if (stitchedHeights[i].size() != Q76B_HEIGHT_COUNT) continue;
        const Fo3TerrainCellQ76& cell = terrain[i];

        const auto eastIt = cellByGrid.find(Q711GridKey(cell.gridX + 1, cell.gridY));
        if (eastIt != cellByGrid.end()) {
            const size_t j = eastIt->second;
            if (stitchedHeights[j].size() == Q76B_HEIGHT_COUNT) {
                ++stitchedEdges;
                for (int y = 0; y < Q76B_LAND_SIDE; ++y) {
                    const size_t a = static_cast<size_t>(y * Q76B_LAND_SIDE + 32);
                    const size_t b = static_cast<size_t>(y * Q76B_LAND_SIDE);
                    const float delta = std::fabs(stitchedHeights[i][a] - stitchedHeights[j][b]);
                    maxSeamDelta = std::max(maxSeamDelta, delta);
                    if (delta > 0.001f) ++adjustedVertices;
                    const float average = (stitchedHeights[i][a] + stitchedHeights[j][b]) * 0.5f;
                    stitchedHeights[i][a] = average;
                    stitchedHeights[j][b] = average;
                }
            }
        }

        const auto northIt = cellByGrid.find(Q711GridKey(cell.gridX, cell.gridY + 1));
        if (northIt != cellByGrid.end()) {
            const size_t j = northIt->second;
            if (stitchedHeights[j].size() == Q76B_HEIGHT_COUNT) {
                ++stitchedEdges;
                for (int x = 0; x < Q76B_LAND_SIDE; ++x) {
                    const size_t a = static_cast<size_t>(32 * Q76B_LAND_SIDE + x);
                    const size_t b = static_cast<size_t>(x);
                    const float delta = std::fabs(stitchedHeights[i][a] - stitchedHeights[j][b]);
                    maxSeamDelta = std::max(maxSeamDelta, delta);
                    if (delta > 0.001f) ++adjustedVertices;
                    const float average = (stitchedHeights[i][a] + stitchedHeights[j][b]) * 0.5f;
                    stitchedHeights[i][a] = average;
                    stitchedHeights[j][b] = average;
                }
            }
        }
    }
    Q76B_LOGI("Q7.11 LAND SEAMS: cells=%zu sharedEdges=%zu adjustedVertices=%zu maxHeightDelta=%.2f",
              terrain.size(), stitchedEdges, adjustedVertices, maxSeamDelta);

    std::vector<Q711CpuBatch> cpuBatches;
    std::unordered_map<std::string, size_t> batchByTexture;
    auto getBatch = [&](const std::string& texturePath) -> Q711CpuBatch& {
        const std::string key = texturePath.empty() ? std::string("<fallback>") : texturePath;
        const auto found = batchByTexture.find(key);
        if (found != batchByTexture.end()) return cpuBatches[found->second];
        const size_t index = cpuBatches.size();
        batchByTexture.emplace(key, index);
        Q711CpuBatch batch;
        batch.texturePath = texturePath;
        cpuBatches.push_back(std::move(batch));
        return cpuBatches.back();
    };

    size_t acceptedCells = 0u;
    size_t texturedQuads = 0u;
    size_t fallbackQuads = 0u;
    for (size_t cellIndex = 0; cellIndex < terrain.size(); ++cellIndex) {
        const Fo3TerrainCellQ76& cell = terrain[cellIndex];
        if (stitchedHeights[cellIndex].size() != Q76B_HEIGHT_COUNT) continue;
        ++acceptedCells;
        const float originX = static_cast<float>(cell.gridX) * Q76B_CELL_SIZE;
        const float originY = static_cast<float>(cell.gridY) * Q76B_CELL_SIZE;

        auto gameHeight = [&](int x, int y) -> float {
            return stitchedHeights[cellIndex][static_cast<size_t>(y * Q76B_LAND_SIDE + x)];
        };
        auto point = [&](int x, int y) -> Q76BV3 {
            const float gameX = originX + static_cast<float>(x) * Q76B_VERTEX_SPACING;
            const float gameY = originY + static_cast<float>(y) * Q76B_VERTEX_SPACING;
            return Q76BToVr(gameX, gameY, gameHeight(x, y),
                            arrivalX, arrivalY, arrivalZ,
                            sceneForward, floorY, unitsPerMetre);
        };

        std::string quadrantTextures[4];
        for (int quadrant = 0; quadrant < 4; ++quadrant) {
            quadrantTextures[quadrant] = ResolveFo3TerrainBaseTextureQ711(cell.landFormId, quadrant);
        }

        for (int y = 0; y < Q76B_LAND_QUADS; ++y) {
            for (int x = 0; x < Q76B_LAND_QUADS; ++x) {
                const int quadrant = Q711QuadrantForQuad(x, y);
                Q711CpuBatch& batch = getBatch(quadrantTextures[quadrant]);
                if (quadrantTextures[quadrant].empty()) ++fallbackQuads;
                else ++texturedQuads;

                const float gameX0 = originX + static_cast<float>(x) * Q76B_VERTEX_SPACING;
                const float gameY0 = originY + static_cast<float>(y) * Q76B_VERTEX_SPACING;
                const float gameX1 = gameX0 + Q76B_VERTEX_SPACING;
                const float gameY1 = gameY0 + Q76B_VERTEX_SPACING;
                const float u0 = gameX0 / Q711_TEXTURE_REPEAT_GAME_UNITS;
                const float v0 = gameY0 / Q711_TEXTURE_REPEAT_GAME_UNITS;
                const float u1 = gameX1 / Q711_TEXTURE_REPEAT_GAME_UNITS;
                const float v1 = gameY1 / Q711_TEXTURE_REPEAT_GAME_UNITS;

                const Q76BV3 p00 = point(x, y);
                const Q76BV3 p10 = point(x + 1, y);
                const Q76BV3 p01 = point(x, y + 1);
                const Q76BV3 p11 = point(x + 1, y + 1);
                Q711PushTriangle(batch.vertices, p00, u0, v0, p10, u1, v0, p11, u1, v1);
                Q711PushTriangle(batch.vertices, p00, u0, v0, p11, u1, v1, p01, u0, v1);
            }
        }
    }

    if (acceptedCells == 0u || cpuBatches.empty()) return false;

    q76bProgram = Q76BCreateProgram();
    if (!q76bProgram) return false;
    q76bMvpLocation = glGetUniformLocation(q76bProgram, "uMvp");
    q76bSamplerLocation = glGetUniformLocation(q76bProgram, "uDiffuse");
    q76bUseTextureLocation = glGetUniformLocation(q76bProgram, "uUseTexture");
    if (q76bMvpLocation < 0 || q76bSamplerLocation < 0 || q76bUseTextureLocation < 0) {
        ShutdownFo3TerrainRenderQ76();
        return false;
    }

    size_t textureUploads = 0u;
    size_t totalVertices = 0u;
    for (Q711CpuBatch& cpu : cpuBatches) {
        if (cpu.vertices.empty()) continue;
        Q711TerrainBatch gpu;
        gpu.texturePath = cpu.texturePath;

        if (!cpu.texturePath.empty() && textureUploads < Q711_MAX_REAL_TEXTURES) {
            gpu.texture = Q711UploadTexture(cpu.texturePath);
            gpu.realTexture = gpu.texture != 0u;
            if (gpu.realTexture) {
                ++textureUploads;
                ++q711RealTextureBatches;
            }
        }
        if (!gpu.realTexture) ++q711FallbackBatches;

        glGenVertexArrays(1, &gpu.vao);
        glBindVertexArray(gpu.vao);
        glGenBuffers(1, &gpu.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(cpu.vertices.size() * sizeof(float)),
                     cpu.vertices.data(), GL_STATIC_DRAW);
        constexpr GLsizei stride = static_cast<GLsizei>(8 * sizeof(float));
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        gpu.vertexCount = static_cast<GLsizei>(cpu.vertices.size() / 8u);
        totalVertices += static_cast<size_t>(gpu.vertexCount);
        q711TerrainBatches.push_back(std::move(gpu));
    }

    const GLenum error = glGetError();
    if (error != GL_NO_ERROR || q711TerrainBatches.empty()) {
        Q76B_LOGE("Q7.11 TERRAIN GPU FAILED: worldspace=%08X glError=0x%X batches=%zu",
                  worldspaceFormId, error, q711TerrainBatches.size());
        ShutdownFo3TerrainRenderQ76();
        return false;
    }

    q76bTriangles = totalVertices / 3u;
    q76bTerrainCells = acceptedCells;
    q76bWorldspace = worldspaceFormId;
    q76bReady = true;
    Q76B_LOGI("Q7.11 TERRAIN GPU READY: worldspace=%08X cells=%zu batches=%zu realTextureBatches=%zu fallbackBatches=%zu texturedQuads=%zu fallbackQuads=%zu vertices=%zu triangles=%zu repeatGameUnits=%.0f seamStitch=1 baseLayerOnly=1 alphaLayers=NEXT",
              q76bWorldspace, q76bTerrainCells, q711TerrainBatches.size(),
              q711RealTextureBatches, q711FallbackBatches,
              texturedQuads, fallbackQuads, totalVertices, q76bTriangles,
              Q711_TEXTURE_REPEAT_GAME_UNITS);
    return true;
}

void RenderFo3TerrainQ76(const float* mvp) {
    if (!q76bReady || !mvp || !q76bProgram || q711TerrainBatches.empty()) return;

    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLint previousActiveTexture = 0;
    GLint previousTexture0 = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(q76bProgram);
    glUniformMatrix4fv(q76bMvpLocation, 1, GL_FALSE, mvp);
    glUniform1i(q76bSamplerLocation, 0);

    for (const Q711TerrainBatch& batch : q711TerrainBatches) {
        if (!batch.vao || batch.vertexCount <= 0) continue;
        glUniform1i(q76bUseTextureLocation, batch.realTexture ? 1 : 0);
        glBindTexture(GL_TEXTURE_2D, batch.realTexture ? batch.texture : 0u);
        glBindVertexArray(batch.vao);
        glDrawArrays(GL_TRIANGLES, 0, batch.vertexCount);
    }

    glBindVertexArray(static_cast<GLuint>(previousVao));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture0));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glUseProgram(static_cast<GLuint>(previousProgram));
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);

    if (!q76bLoggedVisible) {
        q76bLoggedVisible = true;
        Q76B_LOGI("Q7.11 TERRAIN VISIBLE: worldspace=%08X cells=%zu triangles=%zu batches=%zu textured=%zu fallback=%zu stereoRenderLayer=1",
                  q76bWorldspace, q76bTerrainCells, q76bTriangles,
                  q711TerrainBatches.size(), q711RealTextureBatches,
                  q711FallbackBatches);
    }
}
