#include "fo3-terrain-q76.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr int LAND_SIDE = 33;
constexpr int LAND_QUADS = 32;
constexpr float CELL_SIZE = 4096.0f;
constexpr float VERTEX_SPACING = CELL_SIZE / static_cast<float>(LAND_QUADS);
constexpr size_t HEIGHT_COUNT = static_cast<size_t>(LAND_SIDE * LAND_SIDE);

#define Q76B_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q76B_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

struct V3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

GLuint gProgram = 0;
GLuint gVao = 0;
GLuint gVbo = 0;
GLint gMvpLocation = -1;
GLsizei gVertexCount = 0;
bool gReady = false;
bool gLoggedVisible = false;
uint32_t gWorldspace = 0;
size_t gTerrainCells = 0;
size_t gTriangles = 0;

V3 Sub(const V3& a, const V3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

V3 Normalize(V3 v) {
    const float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len < 1e-6f) return {0.0f, 1.0f, 0.0f};
    return {v.x / len, v.y / len, v.z / len};
}

V3 Cross(const V3& a, const V3& b) {
    return {
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x,
    };
}

void PushTriangle(std::vector<float>& out, const V3& a, const V3& b, const V3& c) {
    V3 n = Normalize(Cross(Sub(b, a), Sub(c, a)));
    if (n.y < 0.0f) n = {-n.x, -n.y, -n.z};
    const V3 points[3]{a, b, c};
    for (const V3& p : points) {
        out.insert(out.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
    }
}

GLuint Compile(GLenum type, const char* source) {
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

GLuint CreateProgram() {
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

    GLuint vs = Compile(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = Compile(GL_FRAGMENT_SHADER, fragmentSource);
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

V3 ToVr(float gameX, float gameY, float gameZ,
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
    if (gVbo) glDeleteBuffers(1, &gVbo);
    if (gVao) glDeleteVertexArrays(1, &gVao);
    if (gProgram) glDeleteProgram(gProgram);
    gVbo = 0;
    gVao = 0;
    gProgram = 0;
    gMvpLocation = -1;
    gVertexCount = 0;
    gReady = false;
    gLoggedVisible = false;
    gWorldspace = 0;
    gTerrainCells = 0;
    gTriangles = 0;
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
    vertices.reserve(terrain.size() * static_cast<size_t>(LAND_QUADS * LAND_QUADS * 6 * 6));

    size_t acceptedCells = 0u;
    for (const Fo3TerrainCellQ76& cell : terrain) {
        if (cell.heights.size() != HEIGHT_COUNT) continue;
        ++acceptedCells;
        const float originX = static_cast<float>(cell.gridX) * CELL_SIZE;
        const float originY = static_cast<float>(cell.gridY) * CELL_SIZE;
        auto point = [&](int x, int y) -> V3 {
            const float gameX = originX + static_cast<float>(x) * VERTEX_SPACING;
            const float gameY = originY + static_cast<float>(y) * VERTEX_SPACING;
            const float gameZ = cell.heights[static_cast<size_t>(y * LAND_SIDE + x)];
            return ToVr(gameX, gameY, gameZ,
                        arrivalX, arrivalY, arrivalZ,
                        sceneForward, floorY, unitsPerMetre);
        };

        for (int y = 0; y < LAND_QUADS; ++y) {
            for (int x = 0; x < LAND_QUADS; ++x) {
                const V3 p00 = point(x, y);
                const V3 p10 = point(x + 1, y);
                const V3 p01 = point(x, y + 1);
                const V3 p11 = point(x + 1, y + 1);
                PushTriangle(vertices, p00, p10, p11);
                PushTriangle(vertices, p00, p11, p01);
            }
        }
    }

    if (acceptedCells == 0u || vertices.empty()) return false;

    gProgram = CreateProgram();
    if (!gProgram) return false;
    gMvpLocation = glGetUniformLocation(gProgram, "uMvp");
    if (gMvpLocation < 0) {
        ShutdownFo3TerrainRenderQ76();
        return false;
    }

    glGenVertexArrays(1, &gVao);
    glBindVertexArray(gVao);
    glGenBuffers(1, &gVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
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

    gVertexCount = static_cast<GLsizei>(vertices.size() / 6u);
    gTriangles = static_cast<size_t>(gVertexCount / 3);
    gTerrainCells = acceptedCells;
    gWorldspace = worldspaceFormId;
    gReady = true;
    Q76B_LOGI("Q7.6b TERRAIN GPU READY: worldspace=%08X cells=%zu vertices=%d triangles=%zu visualOnly=1 collisionUntouched=1",
              gWorldspace, gTerrainCells, gVertexCount, gTriangles);
    return true;
}

void RenderFo3TerrainQ76(const float* mvp) {
    if (!gReady || !mvp || !gProgram || !gVao || gVertexCount <= 0) return;

    GLint previousProgram = 0;
    GLint previousVao = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(gProgram);
    glUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, mvp);
    glBindVertexArray(gVao);
    glDrawArrays(GL_TRIANGLES, 0, gVertexCount);

    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);

    if (!gLoggedVisible) {
        gLoggedVisible = true;
        Q76B_LOGI("Q7.6b TERRAIN VISIBLE: worldspace=%08X cells=%zu triangles=%zu stereoRenderLayer=1 visualOnly=1",
                  gWorldspace, gTerrainCells, gTriangles);
    }
}
