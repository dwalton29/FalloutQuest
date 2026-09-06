#include "fo3-collision-overlay.h"
#include "fo3-nif-collision.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr size_t MAX_COLLISION_PLACEMENTS = 32u;
constexpr size_t MAX_LINE_VERTICES = 600000u;

#define Q6E_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6E_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q6E_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

GLuint gProgram = 0;
GLuint gVao = 0;
GLuint gVbo = 0;
GLint gMvpLocation = -1;
GLsizei gVertexCount = 0;
size_t gPlacementCount = 0;
size_t gCollisionMeshCount = 0;
size_t gTriangleCount = 0;
bool gLoggedVisible = false;

bool IsMegatonArchitecture(const std::string& path) {
    std::string lower = path;
    for (char& ch : lower) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        if (ch == '/') ch = '\\';
    }
    return lower.find("architecture\\megaton\\interior\\shackinteriors") != std::string::npos;
}

Vec3 RotateX(Vec3 v, float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    return {v.x, c*v.y - s*v.z, s*v.y + c*v.z};
}

Vec3 RotateY(Vec3 v, float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    return {c*v.x + s*v.z, v.y, -s*v.x + c*v.z};
}

Vec3 RotateZ(Vec3 v, float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    return {c*v.x - s*v.y, s*v.x + c*v.y, v.z};
}

Vec3 ApplyPlacement(Vec3 v, const Fo3WorldPlacement& p) {
    v.x *= p.scale;
    v.y *= p.scale;
    v.z *= p.scale;
    v = RotateX(v, p.rx);
    v = RotateY(v, p.ry);
    v = RotateZ(v, p.rz);
    v.x += p.x;
    v.y += p.y;
    v.z += p.z;
    return v;
}

Vec3 ToVr(Vec3 game, float centerX, float centerY, float floorZ,
          float sceneForward, float floorY, float unitsPerMetre) {
    return {
        (game.x - centerX) / unitsPerMetre,
        floorY + (game.z - floorZ) / unitsPerMetre,
        sceneForward - (game.y - centerY) / unitsPerMetre,
    };
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
        Q6E_LOGE("Q6E COLLISION shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint BuildProgram() {
    static const char* vertexSource = R"(
        #version 300 es
        layout(location = 0) in vec3 aPosition;
        uniform mat4 uMvp;
        void main() {
            gl_Position = uMvp * vec4(aPosition, 1.0);
        }
    )";
    static const char* fragmentSource = R"(
        #version 300 es
        precision mediump float;
        out vec4 fragColor;
        void main() {
            fragColor = vec4(0.0, 1.0, 1.0, 1.0);
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
        Q6E_LOGE("Q6E COLLISION shader link failed: %s", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

void AppendPoint(std::vector<float>& lines, const Vec3& p) {
    lines.push_back(p.x);
    lines.push_back(p.y);
    lines.push_back(p.z);
}

} // namespace

bool InitializeFo3CollisionOverlay(const std::vector<Fo3WorldPlacement>& placements,
                                   float centerX, float centerY, float floorZ,
                                   float sceneForward, float floorY,
                                   float unitsPerMetre) {
    ShutdownFo3CollisionOverlay();
    if (placements.empty() || unitsPerMetre <= 0.0f) return false;

    std::unordered_set<uint32_t> seenRefs;
    std::unordered_map<std::string, std::vector<Fo3NifCollisionMesh>> modelCache;
    std::vector<float> lines;
    lines.reserve(65536u);

    size_t misses = 0;
    size_t cacheHits = 0;
    bool capped = false;

    for (const Fo3WorldPlacement& placement : placements) {
        if (gPlacementCount >= MAX_COLLISION_PLACEMENTS) break;
        if (!IsMegatonArchitecture(placement.modelPath)) continue;
        if (!seenRefs.insert(placement.refFormId).second) continue;

        auto cached = modelCache.find(placement.modelPath);
        if (cached == modelCache.end()) {
            std::vector<Fo3NifCollisionMesh> meshes;
            if (!LoadFo3NifCollisionMeshes(placement.modelPath, meshes)) {
                ++misses;
                continue;
            }
            cached = modelCache.emplace(placement.modelPath, std::move(meshes)).first;
        } else {
            ++cacheHits;
        }

        size_t placementTriangles = 0;
        size_t placementMeshes = 0;
        const std::vector<Fo3NifCollisionMesh>& meshes = cached->second;
        for (const Fo3NifCollisionMesh& mesh : meshes) {
            const size_t vertexCount = mesh.positions.size() / 3u;
            if (vertexCount == 0u || mesh.indices.empty()) continue;

            for (size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
                if ((lines.size() / 3u) + 6u > MAX_LINE_VERTICES) {
                    capped = true;
                    break;
                }
                const uint32_t ia = mesh.indices[i];
                const uint32_t ib = mesh.indices[i + 1u];
                const uint32_t ic = mesh.indices[i + 2u];
                if (ia >= vertexCount || ib >= vertexCount || ic >= vertexCount) continue;

                auto point = [&](uint32_t index) {
                    Vec3 p{
                        mesh.positions[index * 3u],
                        mesh.positions[index * 3u + 1u],
                        mesh.positions[index * 3u + 2u],
                    };
                    return ToVr(ApplyPlacement(p, placement),
                                centerX, centerY, floorZ,
                                sceneForward, floorY, unitsPerMetre);
                };
                const Vec3 a = point(ia);
                const Vec3 b = point(ib);
                const Vec3 c = point(ic);
                AppendPoint(lines, a); AppendPoint(lines, b);
                AppendPoint(lines, b); AppendPoint(lines, c);
                AppendPoint(lines, c); AppendPoint(lines, a);
                ++placementTriangles;
            }
            if (capped) break;
            ++placementMeshes;
        }

        if (placementTriangles > 0u) {
            ++gPlacementCount;
            gCollisionMeshCount += placementMeshes;
            gTriangleCount += placementTriangles;
            Q6E_LOGI("Q6E COLLISION OBJECT: ref=%08X EDID=%s model=%s meshes=%zu triangles=%zu",
                     placement.refFormId,
                     placement.editorId.empty() ? "<none>" : placement.editorId.c_str(),
                     placement.modelPath.c_str(), placementMeshes, placementTriangles);
        }
        if (capped) break;
    }

    if (lines.empty()) {
        Q6E_LOGW("Q6E COLLISION READY FAILED: placements=0 misses=%zu uniqueModels=%zu",
                 misses, modelCache.size());
        return false;
    }

    gProgram = BuildProgram();
    if (!gProgram) return false;
    gMvpLocation = glGetUniformLocation(gProgram, "uMvp");
    if (gMvpLocation < 0) {
        ShutdownFo3CollisionOverlay();
        return false;
    }

    glGenVertexArrays(1, &gVao);
    glBindVertexArray(gVao);
    glGenBuffers(1, &gVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(lines.size() * sizeof(float)),
                 lines.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    gVertexCount = static_cast<GLsizei>(lines.size() / 3u);
    if (glGetError() != GL_NO_ERROR) {
        ShutdownFo3CollisionOverlay();
        return false;
    }

    Q6E_LOGI("Q6E COLLISION READY: placements=%zu meshes=%zu triangles=%zu lineVertices=%d uniqueModels=%zu modelCacheHits=%zu misses=%zu capped=%d",
             gPlacementCount, gCollisionMeshCount, gTriangleCount, gVertexCount,
             modelCache.size(), cacheHits, misses, capped ? 1 : 0);
    return true;
}

void RenderFo3CollisionOverlay(const float* mvp16) {
    if (!mvp16 || !gProgram || !gVao || gVertexCount <= 0) return;

    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLint previousProgram = 0;
    GLint previousVao = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);

    // X-ray mode is deliberate for Q6E verification: collision embedded exactly
    // in the visible wall remains readable instead of disappearing to z-fighting.
    glDisable(GL_DEPTH_TEST);
    glUseProgram(gProgram);
    glUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, mvp16);
    glBindVertexArray(gVao);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, gVertexCount);

    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);

    if (!gLoggedVisible) {
        gLoggedVisible = true;
        Q6E_LOGI("Q6E COLLISION VISIBLE: placements=%zu triangles=%zu original Fallout collision submitted as cyan both-eye wireframe",
                 gPlacementCount, gTriangleCount);
    }
}

void ShutdownFo3CollisionOverlay() {
    if (gVbo) glDeleteBuffers(1, &gVbo);
    if (gVao) glDeleteVertexArrays(1, &gVao);
    if (gProgram) glDeleteProgram(gProgram);
    gVbo = 0;
    gVao = 0;
    gProgram = 0;
    gMvpLocation = -1;
    gVertexCount = 0;
    gPlacementCount = 0;
    gCollisionMeshCount = 0;
    gTriangleCount = 0;
    gLoggedVisible = false;
}
