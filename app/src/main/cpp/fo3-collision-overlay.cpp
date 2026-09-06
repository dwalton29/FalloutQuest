#include "fo3-collision-overlay.h"
#include "fo3-nif-collision-q6f.h"
#include "fo3-terrain-q76.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr size_t MAX_COLLISION_PLACEMENTS = 256u;
constexpr size_t MAX_LINE_VERTICES = 800000u;
constexpr bool SHOW_COLLISION_DEBUG_Q6G = false;

constexpr float PLAYER_RADIUS = 0.26f;
constexpr float PLAYER_HEIGHT = 1.70f;
constexpr float PLAYER_SKIN = 0.003f;
constexpr float MAX_STEP_UP = 0.32f;
constexpr float MAX_GROUND_DROP = 0.80f;
constexpr float SAFE_SPAWN_VERTICAL_SEARCH = 8.0f;
constexpr int MAX_DEPENETRATION_PASSES = 6;

#define Q6F_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6F_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q6F_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define Q6G_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6G_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q6H_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6H_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q6I_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6I_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct CollisionTriangle {
    Vec3 a, b, c;
    Vec3 normal;
    float minX = 0.0f, maxX = 0.0f;
    float minY = 0.0f, maxY = 0.0f;
    float minZ = 0.0f, maxZ = 0.0f;
};

GLuint gProgram = 0;
GLuint gVao = 0;
GLuint gVbo = 0;
GLint gMvpLocation = -1;
GLsizei gVertexCount = 0;
size_t gPlacementCount = 0;
size_t gCollisionShapeCount = 0;
size_t gTriangleCount = 0;
std::array<size_t, 5> gKindCounts{};
bool gLoggedVisible = false;

std::vector<CollisionTriangle> gWorldTriangles;
float gCollisionFloorY = -1.55f;
bool gPlayerCollisionReady = false;
bool gSafeSpawnResolved = false;
uint64_t gResolveCounter = 0;
uint64_t gContactLogCount = 0;
uint64_t gTerrainGroundLogCountQ77 = 0;

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

Vec3 Cross(Vec3 a, Vec3 b) {
    return {
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x,
    };
}

Vec3 Sub(Vec3 a, Vec3 b) {
    return {a.x-b.x, a.y-b.y, a.z-b.z};
}

float Length(Vec3 v) {
    return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

bool BuildTriangle(const Vec3& a, const Vec3& b, const Vec3& c,
                   CollisionTriangle& out) {
    Vec3 n = Cross(Sub(b, a), Sub(c, a));
    const float length = Length(n);
    if (!(length > 1e-7f) || !std::isfinite(length)) return false;
    n.x /= length; n.y /= length; n.z /= length;
    out.a = a; out.b = b; out.c = c; out.normal = n;
    out.minX = std::min({a.x, b.x, c.x});
    out.maxX = std::max({a.x, b.x, c.x});
    out.minY = std::min({a.y, b.y, c.y});
    out.maxY = std::max({a.y, b.y, c.y});
    out.minZ = std::min({a.z, b.z, c.z});
    out.maxZ = std::max({a.z, b.z, c.z});
    return true;
}

Vec2 ClosestPointOnSegment2D(Vec2 p, Vec2 a, Vec2 b) {
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float denom = abx*abx + aby*aby;
    if (denom <= 1e-10f) return a;
    float t = ((p.x-a.x)*abx + (p.y-a.y)*aby) / denom;
    t = std::clamp(t, 0.0f, 1.0f);
    return {a.x + abx*t, a.y + aby*t};
}

float Dist2(Vec2 a, Vec2 b) {
    const float dx = a.x-b.x;
    const float dy = a.y-b.y;
    return dx*dx + dy*dy;
}

bool BarycentricXZ(const CollisionTriangle& tri, float x, float z,
                   float& u, float& v, float& w) {
    const float x0 = tri.a.x, z0 = tri.a.z;
    const float x1 = tri.b.x, z1 = tri.b.z;
    const float x2 = tri.c.x, z2 = tri.c.z;
    const float denom = (z1-z2)*(x0-x2) + (x2-x1)*(z0-z2);
    if (std::fabs(denom) < 1e-8f) return false;
    u = ((z1-z2)*(x-x2) + (x2-x1)*(z-z2)) / denom;
    v = ((z2-z0)*(x-x2) + (x0-x2)*(z-z2)) / denom;
    w = 1.0f - u - v;
    constexpr float eps = -0.0005f;
    return u >= eps && v >= eps && w >= eps;
}

Vec2 ClosestPointTriangleXZ(const CollisionTriangle& tri, Vec2 p, bool& inside) {
    float u = 0.0f, v = 0.0f, w = 0.0f;
    inside = BarycentricXZ(tri, p.x, p.y, u, v, w);
    if (inside) return p;

    const Vec2 a{tri.a.x, tri.a.z};
    const Vec2 b{tri.b.x, tri.b.z};
    const Vec2 c{tri.c.x, tri.c.z};
    const Vec2 qab = ClosestPointOnSegment2D(p, a, b);
    const Vec2 qbc = ClosestPointOnSegment2D(p, b, c);
    const Vec2 qca = ClosestPointOnSegment2D(p, c, a);
    const float dab = Dist2(p, qab);
    const float dbc = Dist2(p, qbc);
    const float dca = Dist2(p, qca);
    if (dab <= dbc && dab <= dca) return qab;
    if (dbc <= dca) return qbc;
    return qca;
}

bool FindGround(float x, float z, float feetY, float& groundY) {
    bool found = false;
    float best = -1e30f;
    for (const CollisionTriangle& tri : gWorldTriangles) {
        if (std::fabs(tri.normal.y) < 0.55f) continue;
        if (x < tri.minX - 0.02f || x > tri.maxX + 0.02f ||
            z < tri.minZ - 0.02f || z > tri.maxZ + 0.02f) continue;

        float u = 0.0f, v = 0.0f, w = 0.0f;
        if (!BarycentricXZ(tri, x, z, u, v, w)) continue;
        const float y = tri.a.y*u + tri.b.y*v + tri.c.y*w;
        if (y > feetY + MAX_STEP_UP || y < feetY - MAX_GROUND_DROP) continue;
        if (y > best) {
            best = y;
            found = true;
        }
    }
    if (found) groundY = best;
    return found;
}

bool FindGroundWide(float x, float z, float referenceY, float& groundY) {
    bool found = false;
    float bestDistance = std::numeric_limits<float>::max();
    float bestY = referenceY;
    for (const CollisionTriangle& tri : gWorldTriangles) {
        if (std::fabs(tri.normal.y) < 0.55f) continue;
        if (x < tri.minX - 0.02f || x > tri.maxX + 0.02f ||
            z < tri.minZ - 0.02f || z > tri.maxZ + 0.02f) continue;

        float u = 0.0f, v = 0.0f, w = 0.0f;
        if (!BarycentricXZ(tri, x, z, u, v, w)) continue;
        const float y = tri.a.y*u + tri.b.y*v + tri.c.y*w;
        const float distance = std::fabs(y - referenceY);
        if (distance > SAFE_SPAWN_VERTICAL_SEARCH) continue;
        if (!found || distance < bestDistance) {
            bestDistance = distance;
            bestY = y;
            found = true;
        }
    }
    if (found) groundY = bestY;
    return found;
}

uint32_t ResolveWallPenetrations(float& x, float& z, float feetY) {
    uint32_t contacts = 0;
    const float topY = feetY + PLAYER_HEIGHT;
    const float radius2 = PLAYER_RADIUS * PLAYER_RADIUS;

    for (int pass = 0; pass < MAX_DEPENETRATION_PASSES; ++pass) {
        bool changed = false;
        for (const CollisionTriangle& tri : gWorldTriangles) {
            if (std::fabs(tri.normal.y) >= 0.75f) continue;
            if (tri.maxY < feetY + 0.04f || tri.minY > topY) continue;
            if (x < tri.minX - PLAYER_RADIUS || x > tri.maxX + PLAYER_RADIUS ||
                z < tri.minZ - PLAYER_RADIUS || z > tri.maxZ + PLAYER_RADIUS) continue;

            const Vec2 p{x, z};
            bool inside = false;
            const Vec2 q = ClosestPointTriangleXZ(tri, p, inside);
            float dx = x - q.x;
            float dz = z - q.y;
            float d2 = dx*dx + dz*dz;
            if (d2 >= radius2) continue;

            float distance = std::sqrt(std::max(d2, 0.0f));
            if (distance > 1e-5f) {
                dx /= distance;
                dz /= distance;
            } else {
                dx = tri.normal.x;
                dz = tri.normal.z;
                float horizontalLength = std::sqrt(dx*dx + dz*dz);
                if (horizontalLength < 1e-5f) continue;
                dx /= horizontalLength;
                dz /= horizontalLength;
                const float centroidX = (tri.a.x + tri.b.x + tri.c.x) / 3.0f;
                const float centroidZ = (tri.a.z + tri.b.z + tri.c.z) / 3.0f;
                if (dx*(x-centroidX) + dz*(z-centroidZ) < 0.0f) {
                    dx = -dx;
                    dz = -dz;
                }
                distance = 0.0f;
            }

            const float push = (PLAYER_RADIUS - distance) + PLAYER_SKIN;
            x += dx * push;
            z += dz * push;
            ++contacts;
            changed = true;
        }
        if (!changed) break;
    }
    return contacts;
}

bool HasOverheadBlock(float x, float z, float feetY) {
    const float headY = feetY + PLAYER_HEIGHT;
    for (const CollisionTriangle& tri : gWorldTriangles) {
        if (std::fabs(tri.normal.y) < 0.75f) continue;
        if (tri.maxY <= feetY + 0.10f || tri.minY >= headY + 0.02f) continue;
        if (x < tri.minX - PLAYER_RADIUS || x > tri.maxX + PLAYER_RADIUS ||
            z < tri.minZ - PLAYER_RADIUS || z > tri.maxZ + PLAYER_RADIUS) continue;
        float u = 0.0f, v = 0.0f, w = 0.0f;
        if (BarycentricXZ(tri, x, z, u, v, w)) return true;
    }
    return false;
}

bool SpawnCandidateClear(float x, float z, float seedFeetY, float& outGroundY) {
    float groundY = seedFeetY;
    if (!FindGroundWide(x, z, seedFeetY, groundY)) return false;
    float resolvedX = x;
    float resolvedZ = z;
    const uint32_t contacts = ResolveWallPenetrations(resolvedX, resolvedZ, groundY);
    const float dx = resolvedX - x;
    const float dz = resolvedZ - z;
    if (contacts != 0u || dx*dx + dz*dz > 0.0001f) return false;
    if (HasOverheadBlock(x, z, groundY)) return false;
    outGroundY = groundY;
    return true;
}

bool FindSafeSpawn(float seedX, float seedZ, float currentPlayerYOffset,
                   float& outX, float& outZ, float& outPlayerYOffset) {
    const float seedFeetY = gCollisionFloorY + currentPlayerYOffset;
    constexpr std::array<float, 13> radii{
        0.0f, 0.30f, 0.60f, 0.90f, 1.20f, 1.50f, 1.80f,
        2.10f, 2.40f, 3.00f, 3.60f, 4.20f, 4.80f
    };
    constexpr int ANGLES = 32;
    for (float radius : radii) {
        const int samples = radius == 0.0f ? 1 : ANGLES;
        for (int i = 0; i < samples; ++i) {
            const float angle = samples == 1 ? 0.0f :
                (6.28318530717958647692f * static_cast<float>(i) / static_cast<float>(samples));
            const float x = seedX + std::cos(angle) * radius;
            const float z = seedZ + std::sin(angle) * radius;
            float groundY = seedFeetY;
            if (!SpawnCandidateClear(x, z, seedFeetY, groundY)) continue;
            outX = x;
            outZ = z;
            outPlayerYOffset = groundY - gCollisionFloorY;
            return true;
        }
    }
    return false;
}

bool FindGroundedSpawnRescue(float seedX, float seedZ, float currentPlayerYOffset,
                             float& outX, float& outZ, float& outPlayerYOffset) {
    const float seedFeetY = gCollisionFloorY + currentPlayerYOffset;
    constexpr std::array<float, 13> radii{
        0.0f, 0.30f, 0.60f, 0.90f, 1.20f, 1.50f, 1.80f,
        2.10f, 2.40f, 3.00f, 3.60f, 4.20f, 4.80f
    };
    constexpr int ANGLES = 32;
    for (float radius : radii) {
        const int samples = radius == 0.0f ? 1 : ANGLES;
        for (int i = 0; i < samples; ++i) {
            const float angle = samples == 1 ? 0.0f :
                (6.28318530717958647692f * static_cast<float>(i) / static_cast<float>(samples));
            float x = seedX + std::cos(angle) * radius;
            float z = seedZ + std::sin(angle) * radius;
            float groundY = seedFeetY;
            if (!FindGroundWide(x, z, seedFeetY, groundY)) continue;
            ResolveWallPenetrations(x, z, groundY);
            if (HasOverheadBlock(x, z, groundY)) continue;
            outX = x;
            outZ = z;
            outPlayerYOffset = groundY - gCollisionFloorY;
            return true;
        }
    }
    return false;
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
        Q6F_LOGE("Q6F COLLISION shader compile failed: %s", log);
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
        Q6F_LOGE("Q6F COLLISION shader link failed: %s", log);
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
    gCollisionFloorY = floorY;

    std::unordered_set<uint32_t> seenRefs;
    std::unordered_map<std::string, std::vector<Fo3NifCollisionShapeQ6F>> modelCache;
    std::vector<float> lines;
    lines.reserve(65536u);
    gWorldTriangles.reserve(8192u);

    size_t misses = 0;
    size_t cacheHits = 0;
    bool capped = false;

    for (const Fo3WorldPlacement& placement : placements) {
        if (gPlacementCount >= MAX_COLLISION_PLACEMENTS) break;
        if (!IsMegatonArchitecture(placement.modelPath)) continue;
        if (!seenRefs.insert(placement.refFormId).second) continue;

        auto cached = modelCache.find(placement.modelPath);
        if (cached == modelCache.end()) {
            std::vector<Fo3NifCollisionShapeQ6F> shapes;
            if (!LoadFo3NifCollisionShapesQ6F(placement.modelPath, shapes)) {
                ++misses;
                continue;
            }
            cached = modelCache.emplace(placement.modelPath, std::move(shapes)).first;
        } else {
            ++cacheHits;
        }

        size_t placementTriangles = 0;
        size_t placementShapes = 0;
        std::array<size_t, 5> placementKinds{};
        const std::vector<Fo3NifCollisionShapeQ6F>& shapes = cached->second;
        for (const Fo3NifCollisionShapeQ6F& shape : shapes) {
            const size_t vertexCount = shape.positions.size() / 3u;
            if (vertexCount == 0u || shape.indices.empty()) continue;

            for (size_t i = 0; i + 2u < shape.indices.size(); i += 3u) {
                const uint32_t ia = shape.indices[i];
                const uint32_t ib = shape.indices[i + 1u];
                const uint32_t ic = shape.indices[i + 2u];
                if (ia >= vertexCount || ib >= vertexCount || ic >= vertexCount) continue;

                auto point = [&](uint32_t index) {
                    Vec3 p{
                        shape.positions[index * 3u],
                        shape.positions[index * 3u + 1u],
                        shape.positions[index * 3u + 2u],
                    };
                    return ToVr(ApplyPlacement(p, placement),
                                centerX, centerY, floorZ,
                                sceneForward, floorY, unitsPerMetre);
                };
                const Vec3 a = point(ia);
                const Vec3 b = point(ib);
                const Vec3 c = point(ic);

                CollisionTriangle worldTriangle;
                if (!BuildTriangle(a, b, c, worldTriangle)) continue;
                gWorldTriangles.push_back(worldTriangle);

                if (SHOW_COLLISION_DEBUG_Q6G) {
                    if ((lines.size() / 3u) + 6u > MAX_LINE_VERTICES) {
                        capped = true;
                        break;
                    }
                    AppendPoint(lines, a); AppendPoint(lines, b);
                    AppendPoint(lines, b); AppendPoint(lines, c);
                    AppendPoint(lines, c); AppendPoint(lines, a);
                }
                ++placementTriangles;
            }
            if (capped) break;
            ++placementShapes;
            ++placementKinds[static_cast<size_t>(shape.kind)];
        }

        if (placementTriangles > 0u) {
            ++gPlacementCount;
            gCollisionShapeCount += placementShapes;
            gTriangleCount += placementTriangles;
            for (size_t i = 0; i < gKindCounts.size(); ++i) gKindCounts[i] += placementKinds[i];
            Q6F_LOGI("Q6F COLLISION OBJECT: ref=%08X EDID=%s model=%s shapes=%zu triangles=%zu packed=%zu convex=%zu box=%zu sphere=%zu capsule=%zu",
                     placement.refFormId,
                     placement.editorId.empty() ? "<none>" : placement.editorId.c_str(),
                     placement.modelPath.c_str(), placementShapes, placementTriangles,
                     placementKinds[0], placementKinds[1], placementKinds[2], placementKinds[3], placementKinds[4]);
        }
        if (capped) break;
    }

    if (gWorldTriangles.empty()) {
        Q6F_LOGW("Q6F COLLISION READY FAILED: placements=0 misses=%zu uniqueModels=%zu",
                 misses, modelCache.size());
        return false;
    }

    gPlayerCollisionReady = true;
    Q6F_LOGI("Q6F COLLISION READY: placements=%zu shapes=%zu triangles=%zu uniqueModels=%zu modelCacheHits=%zu misses=%zu packed=%zu convex=%zu box=%zu sphere=%zu capsule=%zu capped=%d",
             gPlacementCount, gCollisionShapeCount, gTriangleCount,
             modelCache.size(), cacheHits, misses,
             gKindCounts[0], gKindCounts[1], gKindCounts[2], gKindCounts[3], gKindCounts[4], capped ? 1 : 0);
    Q6G_LOGI("Q6G PHYSICS READY: authoredTriangles=%zu capsuleRadius=%.2f capsuleHeight=%.2f floorReference=%.2f debugOverlay=%d",
             gWorldTriangles.size(), PLAYER_RADIUS, PLAYER_HEIGHT,
             gCollisionFloorY, SHOW_COLLISION_DEBUG_Q6G ? 1 : 0);

    if (!SHOW_COLLISION_DEBUG_Q6G) return true;

    gProgram = BuildProgram();
    if (!gProgram) return true;
    gMvpLocation = glGetUniformLocation(gProgram, "uMvp");
    if (gMvpLocation < 0) {
        glDeleteProgram(gProgram);
        gProgram = 0;
        return true;
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
    return true;
}

bool ResolveFo3PlayerMotionQ6G(float currentX, float currentZ,
                               float desiredX, float desiredZ,
                               float currentPlayerYOffset,
                               float* outX, float* outZ,
                               float* outPlayerYOffset) {
    if (!outX || !outZ || !outPlayerYOffset || !gPlayerCollisionReady ||
        gWorldTriangles.empty()) return false;

    ++gResolveCounter;

    if (!gSafeSpawnResolved) {
        // Q7.7: after an exterior XTEL transition, LAND is already decoded and
        // aligned to the same origin as the visible worldspace. Resolve the
        // first grounded frame at the authored X/Z instead of searching nearby
        // Havok floors. In interiors the sampler is inactive and this block is
        // a strict no-op, preserving the proven Q7.5/Q6K spawn path.
        float terrainSpawnY = currentPlayerYOffset;
        if (SampleFo3TerrainGroundQ77(currentX, currentZ, &terrainSpawnY)) {
            *outX = currentX;
            *outZ = currentZ;
            *outPlayerYOffset = terrainSpawnY;
            gSafeSpawnResolved = true;
            Q6I_LOGI("Q7.7 LAND SAFE SPAWN: seed=(%.3f %.3f) resolvedSameXZ=1 playerY=%.3f source=VHGT",
                     currentX, currentZ, terrainSpawnY);
            return true;
        }

        float spawnX = currentX;
        float spawnZ = currentZ;
        float spawnPlayerY = currentPlayerYOffset;
        if (FindSafeSpawn(currentX, currentZ, currentPlayerYOffset,
                          spawnX, spawnZ, spawnPlayerY)) {
            *outX = spawnX;
            *outZ = spawnZ;
            *outPlayerYOffset = spawnPlayerY;
            gSafeSpawnResolved = true;
            Q6I_LOGI("Q6I SAFE SPAWN: seed=(%.3f %.3f) resolved=(%.3f %.3f) playerY=%.3f capsuleClear=1 grounded=1 verticalSearch=%.1f",
                     currentX, currentZ, spawnX, spawnZ, spawnPlayerY, SAFE_SPAWN_VERTICAL_SEARCH);
            return true;
        }

        if (FindGroundedSpawnRescue(currentX, currentZ, currentPlayerYOffset,
                                    spawnX, spawnZ, spawnPlayerY)) {
            *outX = spawnX;
            *outZ = spawnZ;
            *outPlayerYOffset = spawnPlayerY;
            gSafeSpawnResolved = true;
            Q6I_LOGW("Q6I SAFE SPAWN RESCUE: seed=(%.3f %.3f) resolved=(%.3f %.3f) playerY=%.3f grounded=1 capsuleDepenetrated=1",
                     currentX, currentZ, spawnX, spawnZ, spawnPlayerY);
            return true;
        }

        // Never mark an ungrounded spawn as resolved. Keep trying on subsequent
        // frames rather than accepting the Q6H behaviour that left the headset
        // poking through the floor.
        *outX = currentX;
        *outZ = currentZ;
        *outPlayerYOffset = currentPlayerYOffset;
        Q6I_LOGW("Q6I SAFE SPAWN DEFER: no grounded collision candidate within 4.80m; movement held and retrying");
        return true;
    }

    float x = currentX;
    float z = currentZ;
    float feetY = gCollisionFloorY + currentPlayerYOffset;
    uint32_t contacts = ResolveWallPenetrations(x, z, feetY);

    const float dx = desiredX - currentX;
    const float dz = desiredZ - currentZ;
    const float distance = std::sqrt(dx*dx + dz*dz);
    const float maxSubstep = PLAYER_RADIUS * 0.40f;
    const int steps = std::clamp(static_cast<int>(std::ceil(distance / maxSubstep)), 1, 12);

    for (int step = 1; step <= steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        float targetX = currentX + dx*t;
        float targetZ = currentZ + dz*t;

        if (step == 1) {
            targetX += x - currentX;
            targetZ += z - currentZ;
        } else {
            const float prevT = static_cast<float>(step - 1) / static_cast<float>(steps);
            const float requestedPrevX = currentX + dx*prevT;
            const float requestedPrevZ = currentZ + dz*prevT;
            targetX += x - requestedPrevX;
            targetZ += z - requestedPrevZ;
        }
        x = targetX;
        z = targetZ;
        contacts += ResolveWallPenetrations(x, z, feetY);
    }

    float groundY = feetY;
    bool grounded = FindGround(x, z, feetY, groundY);
    const bool authoredGrounded = grounded;

    // Q7.7 merge rule: authored Havok remains authoritative for stairs,
    // platforms and structural floors. LAND fills the gaps where exterior
    // worldspace ground has no bhk triangle; if terrain is physically above an
    // authored candidate, prefer the higher surface to avoid sinking through it.
    float terrainPlayerY = currentPlayerYOffset;
    bool terrainGrounded = SampleFo3TerrainGroundQ77(x, z, &terrainPlayerY);
    if (terrainGrounded) {
        const float terrainGroundY = gCollisionFloorY + terrainPlayerY;
        if (!grounded || terrainGroundY > groundY) {
            groundY = terrainGroundY;
            grounded = true;
        }
        if (gTerrainGroundLogCountQ77 < 12u || (gResolveCounter % 360u) == 0u) {
            ++gTerrainGroundLogCountQ77;
            Q6G_LOGI("Q7.7 LAND GROUND: pos=(%.3f %.3f) terrainY=%.3f selectedGroundY=%.3f authoredGround=%d",
                     x, z, terrainGroundY, groundY, authoredGrounded ? 1 : 0);
        }
    }

    float playerYOffset = currentPlayerYOffset;
    if (grounded) playerYOffset = groundY - gCollisionFloorY;

    *outX = x;
    *outZ = z;
    *outPlayerYOffset = playerYOffset;

    const float correctionX = x - desiredX;
    const float correctionZ = z - desiredZ;
    const bool blocked = contacts > 0u || (correctionX*correctionX + correctionZ*correctionZ) > 0.000004f;
    if (blocked && (gContactLogCount < 24u || (gResolveCounter % 120u) == 0u)) {
        ++gContactLogCount;
        Q6G_LOGI("Q6G CONTACT: desired=(%.3f %.3f) resolved=(%.3f %.3f) correction=(%.3f %.3f) contacts=%u grounded=%d playerY=%.3f",
                 desiredX, desiredZ, x, z,
                 correctionX, correctionZ, contacts,
                 grounded ? 1 : 0, playerYOffset);
    }
    return true;
}

bool IsFo3PlayerCollisionReadyQ6G() {
    return gPlayerCollisionReady && !gWorldTriangles.empty();
}

void RenderFo3CollisionOverlay(const float* mvp16) {
    if (!SHOW_COLLISION_DEBUG_Q6G) return;
    if (!mvp16 || !gProgram || !gVao || gVertexCount <= 0) return;

    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLint previousProgram = 0;
    GLint previousVao = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);

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
        Q6F_LOGI("Q6F COLLISION VISIBLE: placements=%zu shapes=%zu triangles=%zu authored Fallout collision submitted as cyan both-eye wireframe",
                 gPlacementCount, gCollisionShapeCount, gTriangleCount);
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
    gCollisionShapeCount = 0;
    gTriangleCount = 0;
    gKindCounts.fill(0u);
    gLoggedVisible = false;
    gWorldTriangles.clear();
    gPlayerCollisionReady = false;
    gSafeSpawnResolved = false;
    gResolveCounter = 0;
    gContactLogCount = 0;
    gTerrainGroundLogCountQ77 = 0;
}
