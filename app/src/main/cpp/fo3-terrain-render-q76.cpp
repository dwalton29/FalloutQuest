#include "fo3-terrain-q76.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr const char* Q76B_TAG = "FalloutQuest";
constexpr int Q76B_LAND_SIDE = 33;
constexpr int Q76B_LAND_QUADS = 32;
constexpr float Q76B_CELL_SIZE = 4096.0f;
constexpr float Q76B_VERTEX_SPACING = Q76B_CELL_SIZE / static_cast<float>(Q76B_LAND_QUADS);
constexpr size_t Q76B_HEIGHT_COUNT = static_cast<size_t>(Q76B_LAND_SIDE * Q76B_LAND_SIDE);

#define Q76B_LOGI(...) __android_log_print(ANDROID_LOG_INFO, Q76B_TAG, __VA_ARGS__)
#define Q76B_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, Q76B_TAG, __VA_ARGS__)

struct Q76BV3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

GLuint q76bProgram = 0;
GLuint q76bVao = 0;
GLuint q76bVbo = 0;
GLint q76bMvpLocation = -1;
GLsizei q76bVertexCount = 0;
bool q76bReady = false;
bool q76bLoggedVisible = false;
uint32_t q76bWorldspace = 0;
size_t q76bTerrainCells = 0;
size_t q76bTriangles = 0;

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

void Q76BPushTriangle(std::vector<float>& out,
                      const Q76BV3& a, const Q76BV3& b, const Q76BV3& c) {
    Q76BV3 n = Q76BNormalize(Q76BCross(Q76BSub(b, a), Q76BSub(c, a)));
    if (n.y < 0.0f) n = {-n.x, -n.y, -n.z};
    const Q76BV3 points[3]{a, b, c};
    for (const Q76BV3& p : points) {
        out.insert(out.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
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
        Q76B_LOGE("Q7.6b terrain shader compile failed: %s", log);
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
        uniform mat4 uMvp;
        out vec3 vNormal;
        void main() {
            vNormal = aNormal;
            gl_Position = uMvp * vec4(aPosition, 1.0);
        }
    )";
    static const char* fragmentSource = R"(
        #version 300 es
        precision mediump float;
        in vec3 vNormal;
        out vec4 fragColor;
        void main() {
            vec3 N = normalize(vNormal);
            vec3 lightDirection = normalize(vec3(0.35, 0.85, 0.40));
            float lambert = max(dot(N, lightDirection), 0.0);
            vec3 earth = vec3(0.34, 0.27, 0.18);
            vec3 lit = earth * (0.42 + 0.58 * lambert);
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
        Q76B_LOGE("Q7.6b terrain shader link failed: %s", log);
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

} // namespace

void ShutdownFo3TerrainRenderQ76() {
    if (q76bVbo) glDeleteBuffers(1, &q76bVbo);
    if (q76bVao) glDeleteVertexArrays(1, &q76bVao);
    if (q76bProgram) glDeleteProgram(q76bProgram);
    q76bVbo = 0;
    q76bVao = 0;
    q76bProgram = 0;
    q76bMvpLocation = -1;
    q76bVertexCount = 0;
    q76bReady = false;
    q76bLoggedVisible = false;
    q76bWorldspace = 0;
    q76bTerrainCells = 0;
    q76bTriangles = 0;
}

bool InitializeFo3TerrainRenderQ76(uint32_t worldspaceFormId,
                                   float arrivalX, float arrivalY, float arrivalZ,
                                   float sceneForward, float floorY,
                                   float unitsPerMetre) {
    ShutdownFo3TerrainRenderQ76();
    if (worldspaceFormId == 0u || unitsPerMetre <= 1e-4f) return false;

    if (!LoadFo3TerrainQ76(worldspaceFormId, arrivalX, arrivalY, arrivalZ)) {
        Q76B_LOGE("Q7.6b TERRAIN GPU FAILED: worldspace=%08X reason=LAND-load", worldspaceFormId);
        return false;
    }
    const std::vector<Fo3TerrainCellQ76>& terrain = GetFo3TerrainQ76();
    if (terrain.empty()) return false;

    std::vector<float> vertices;
    vertices.reserve(terrain.size() * static_cast<size_t>(Q76B_LAND_QUADS * Q76B_LAND_QUADS * 6 * 6));

    size_t acceptedCells = 0u;
    for (const Fo3TerrainCellQ76& cell : terrain) {
        if (cell.heights.size() != Q76B_HEIGHT_COUNT) continue;
        ++acceptedCells;
        const float originX = static_cast<float>(cell.gridX) * Q76B_CELL_SIZE;
        const float originY = static_cast<float>(cell.gridY) * Q76B_CELL_SIZE;
        auto point = [&](int x, int y) -> Q76BV3 {
            const float gameX = originX + static_cast<float>(x) * Q76B_VERTEX_SPACING;
            const float gameY = originY + static_cast<float>(y) * Q76B_VERTEX_SPACING;
            const float gameZ = cell.heights[static_cast<size_t>(y * Q76B_LAND_SIDE + x)];
            return Q76BToVr(gameX, gameY, gameZ,
                            arrivalX, arrivalY, arrivalZ,
                            sceneForward, floorY, unitsPerMetre);
        };

        for (int y = 0; y < Q76B_LAND_QUADS; ++y) {
            for (int x = 0; x < Q76B_LAND_QUADS; ++x) {
                const Q76BV3 p00 = point(x, y);
                const Q76BV3 p10 = point(x + 1, y);
                const Q76BV3 p01 = point(x, y + 1);
                const Q76BV3 p11 = point(x + 1, y + 1);
                Q76BPushTriangle(vertices, p00, p10, p11);
                Q76BPushTriangle(vertices, p00, p11, p01);
            }
        }
    }

    if (acceptedCells == 0u || vertices.empty()) return false;

    q76bProgram = Q76BCreateProgram();
    if (!q76bProgram) return false;
    q76bMvpLocation = glGetUniformLocation(q76bProgram, "uMvp");
    if (q76bMvpLocation < 0) {
        ShutdownFo3TerrainRenderQ76();
        return false;
    }

    glGenVertexArrays(1, &q76bVao);
    glBindVertexArray(q76bVao);
    glGenBuffers(1, &q76bVbo);
    glBindBuffer(GL_ARRAY_BUFFER, q76bVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);
    constexpr GLsizei stride = static_cast<GLsizei>(6 * sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        Q76B_LOGE("Q7.6b TERRAIN GPU FAILED: worldspace=%08X glError=0x%X", worldspaceFormId, error);
        ShutdownFo3TerrainRenderQ76();
        return false;
    }

    q76bVertexCount = static_cast<GLsizei>(vertices.size() / 6u);
    q76bTriangles = static_cast<size_t>(q76bVertexCount / 3);
    q76bTerrainCells = acceptedCells;
    q76bWorldspace = worldspaceFormId;
    q76bReady = true;
    Q76B_LOGI("Q7.6b TERRAIN GPU READY: worldspace=%08X cells=%zu vertices=%d triangles=%zu visualOnly=1 collisionUntouched=1",
              q76bWorldspace, q76bTerrainCells, q76bVertexCount, q76bTriangles);
    return true;
}

void RenderFo3TerrainQ76(const float* mvp) {
    if (!q76bReady || !mvp || !q76bProgram || !q76bVao || q76bVertexCount <= 0) return;

    GLint previousProgram = 0;
    GLint previousVao = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(q76bProgram);
    glUniformMatrix4fv(q76bMvpLocation, 1, GL_FALSE, mvp);
    glBindVertexArray(q76bVao);
    glDrawArrays(GL_TRIANGLES, 0, q76bVertexCount);

    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);

    if (!q76bLoggedVisible) {
        q76bLoggedVisible = true;
        Q76B_LOGI("Q7.6b TERRAIN VISIBLE: worldspace=%08X cells=%zu triangles=%zu stereoRenderLayer=1 visualOnly=1",
                  q76bWorldspace, q76bTerrainCells, q76bTriangles);
    }
}
