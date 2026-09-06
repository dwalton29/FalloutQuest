#include "fo3-collision-overlay.h"
#include "fo3-nif-collision-q6f.h"
#include "fo3-transition-q74.h"

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

constexpr int Q76_LAND_VERTS = 33;
constexpr int Q76_LAND_QUADS = 32;
constexpr float Q76_CELL_SIZE = 4096.0f;
constexpr float Q76_VERTEX_SPACING = Q76_CELL_SIZE / static_cast<float>(Q76_LAND_QUADS);

#define Q6F_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6F_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q6F_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define Q6G_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6G_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q6H_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6H_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q6I_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6I_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q76_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q76_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

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

// Q7.6 LAND is rendered as a solid heightfield and sampled directly for player
// grounding. It is intentionally not expanded into gWorldTriangles, avoiding a
// ~22k-triangle per-frame collision scan for the 11 Megaton LAND cells.
GLuint gTerrainProgramQ76 = 0;
GLuint gTerrainVaoQ76 = 0;
GLuint gTerrainVboQ76 = 0;
GLint gTerrainMvpLocationQ76 = -1;
GLsizei gTerrainVertexCountQ76 = 0;
size_t gTerrainTriangleCountQ76 = 0;
bool gTerrainLoggedVisibleQ76 = false;
std::vector<Fo3TerrainCellQ76> gTerrainCellsQ76;
float gTerrainCenterXQ76 = 0.0f;
float gTerrainCenterYQ76 = 0.0f;
float gTerrainFloorZQ76 = 0.0f;
float gTerrainSceneForwardQ76 = 0.0f;
float gTerrainFloorYQ76 = -1.55f;
float gTerrainUnitsPerMetreQ76 = 70.0f;

std::vector<CollisionTriangle> gWorldTriangles;
float gCollisionFloorY = -1.55f;
bool gPlayerCollisionReady = false;
bool gSafeSpawnResolved = false;
uint64_t gResolveCounter = 0;
uint64_t gContactLogCount = 0;

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
    return {c*v.x - s*v.y, s*v.x + c*v.z, v.z};
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

bool SampleTerrainGroundQ76(float vrX, float vrZ, float& outVrY) {
    if (gTerrainCellsQ76.empty() || gTerrainUnitsPerMetreQ76 <= 0.0f) return false;
    const float gameX = gTerrainCenterXQ76 + vrX * gTerrainUnitsPerMetreQ76;
    const float gameY = gTerrainCenterYQ76
        + (gTerrainSceneForwardQ76 - vrZ) * gTerrainUnitsPerMetreQ76;
    const int32_t gridX = static_cast<int32_t>(std::floor(gameX / Q76_CELL_SIZE));
    const int32_t gridY = static_cast<int32_t>(std::floor(gameY / Q76_CELL_SIZE));

    for (const Fo3TerrainCellQ76& cell : gTerrainCellsQ76) {
        if (cell.gridX != gridX || cell.gridY != gridY ||
            cell.heights.size() != static_cast<size_t>(Q76_LAND_VERTS * Q76_LAND_VERTS)) continue;
        const float localX = gameX - static_cast<float>(gridX) * Q76_CELL_SIZE;
        const float localY = gameY - static_cast<float>(gridY) * Q76_CELL_SIZE;
        const float gx = std::clamp(localX / Q76_VERTEX_SPACING, 0.0f, 32.0f);
        const float gy = std::clamp(localY / Q76_VERTEX_SPACING, 0.0f, 32.0f);
        const int x0 = std::clamp(static_cast<int>(std::floor(gx)), 0, 32);
        const int y0 = std::clamp(static_cast<int>(std::floor(gy)), 0, 32);
        const int x1 = std::min(x0 + 1, 32);
        const int y1 = std::min(y0 + 1, 32);
        const float tx = gx - static_cast<float>(x0);
        const float ty = gy - static_cast<float>(y0);
        auto h = [&](int x, int y) {
            return cell.heights[static_cast<size_t>(y * Q76_LAND_VERTS + x)];
        };
        const float h0 = h(x0, y0) * (1.0f - tx) + h(x1, y0) * tx;
        const float h1 = h(x0, y1) * (1.0f - tx) + h(x1, y1) * tx;
        const float gameHeight = h0 * (1.0f - ty) + h1 * ty;
        outVrY = gTerrainFloorYQ76
            + (gameHeight - gTerrainFloorZQ76) / gTerrainUnitsPerMetreQ76;
        return true;
    }
    return false;
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

    float terrainY = feetY;
    if (SampleTerrainGroundQ76(x, z, terrainY) &&
        terrainY <= feetY + MAX_STEP_UP && terrainY >= feetY - MAX_GROUND_DROP &&
        (!found || terrainY > best)) {
        best = terrainY;
        found = true;
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

    float terrainY = referenceY;
    if (SampleTerrainGroundQ76(x, z, terrainY)) {
        const float distance = std::fabs(terrainY - referenceY);
        if (distance <= SAFE_SPAWN_VERTICAL_SEARCH && (!found || distance < bestDistance)) {
            bestDistance = distance;
            bestY = terrainY;
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

GLuint BuildTerrainProgramQ76() {
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
            vec3 L = normalize(vec3(0.35, 0.85, 0.40));
            float lambert = max(dot(N, L), 0.0);
            vec3 earth = vec3(0.34, 0.30, 0.23);
            fragColor = vec4(earth * (0.38 + 0.62 * lambert), 1.0);
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
        Q76_LOGW("Q7.6 TERRAIN shader link failed: %s", log);
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

void AppendTerrainVertexQ76(std::vector<float>& vertices, const Vec3& p, const Vec3& n) {
    vertices.insert(vertices.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
}

bool PrepareTerrainQ76(float centerX, float centerY, float floorZ,
                       float sceneForward, float floorY, float unitsPerMetre) {
    gTerrainCellsQ76 = GetFo3TerrainQ76();
    if (gTerrainCellsQ76.empty() || unitsPerMetre <= 0.0f) return false;

    gTerrainCenterXQ76 = centerX;
    gTerrainCenterYQ76 = centerY;
    gTerrainFloorZQ76 = floorZ;
    gTerrainSceneForwardQ76 = sceneForward;
    gTerrainFloorYQ76 = floorY;
    gTerrainUnitsPerMetreQ76 = unitsPerMetre;

    std::vector<float> vertices;
    vertices.reserve(gTerrainCellsQ76.size()
                     * static_cast<size_t>(Q76_LAND_QUADS * Q76_LAND_QUADS * 6)
                     * 6u);
    size_t decodedCells = 0u;
    size_t triangles = 0u;

    for (const Fo3TerrainCellQ76& cell : gTerrainCellsQ76) {
        if (cell.heights.size() != static_cast<size_t>(Q76_LAND_VERTS * Q76_LAND_VERTS)) continue;
        ++decodedCells;
        auto point = [&](int row, int col) {
            const float gameX = static_cast<float>(cell.gridX) * Q76_CELL_SIZE
                              + static_cast<float>(col) * Q76_VERTEX_SPACING;
            const float gameY = static_cast<float>(cell.gridY) * Q76_CELL_SIZE
                              + static_cast<float>(row) * Q76_VERTEX_SPACING;
            const float gameZ = cell.heights[static_cast<size_t>(row * Q76_LAND_VERTS + col)];
            return ToVr({gameX, gameY, gameZ}, centerX, centerY, floorZ,
                        sceneForward, floorY, unitsPerMetre);
        };

        auto appendTriangle = [&](Vec3 a, Vec3 b, Vec3 c) {
            CollisionTriangle tri;
            if (!BuildTriangle(a, b, c, tri)) return;
            if (tri.normal.y < 0.0f) {
                if (!BuildTriangle(a, c, b, tri)) return;
            }
            AppendTerrainVertexQ76(vertices, tri.a, tri.normal);
            AppendTerrainVertexQ76(vertices, tri.b, tri.normal);
            AppendTerrainVertexQ76(vertices, tri.c, tri.normal);
            ++triangles;
        };

        for (int row = 0; row < Q76_LAND_QUADS; ++row) {
            for (int col = 0; col < Q76_LAND_QUADS; ++col) {
                const Vec3 p00 = point(row, col);
                const Vec3 p10 = point(row, col + 1);
                const Vec3 p01 = point(row + 1, col);
                const Vec3 p11 = point(row + 1, col + 1);
                appendTriangle(p00, p10, p11);
                appendTriangle(p00, p11, p01);
            }
        }
    }

    if (vertices.empty() || triangles == 0u) {
        gTerrainCellsQ76.clear();
        return false;
    }

    gTerrainProgramQ76 = BuildTerrainProgramQ76();
    if (!gTerrainProgramQ76) return false;
    gTerrainMvpLocationQ76 = glGetUniformLocation(gTerrainProgramQ76, "uMvp");
    if (gTerrainMvpLocationQ76 < 0) return false;

    glGenVertexArrays(1, &gTerrainVaoQ76);
    glBindVertexArray(gTerrainVaoQ76);
    glGenBuffers(1, &gTerrainVboQ76);
    glBindBuffer(GL_ARRAY_BUFFER, gTerrainVboQ76);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);
    constexpr GLsizei stride = 6 * static_cast<GLsizei>(sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    gTerrainVertexCountQ76 = static_cast<GLsizei>(vertices.size() / 6u);
    gTerrainTriangleCountQ76 = triangles;
    Q76_LOGI("Q7.6 TERRAIN GPU READY: cells=%zu triangles=%zu vertices=%d heightfieldCollision=1 material=earth-fallback",
             decodedCells, triangles, gTerrainVertexCountQ76);
    return true;
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

    const bool terrainReadyQ76 = PrepareTerrainQ76(centerX, centerY, floorZ,
                                                    sceneForward, floorY, unitsPerMetre);

    if (gWorldTriangles.empty() && !terrainReadyQ76) {
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
    Q76_LOGI("Q7.6 TERRAIN PHYSICS READY: cells=%zu heightfield=%d renderTriangles=%zu perFrameTriangleExpansion=0",
             gTerrainCellsQ76.size(), terrainReadyQ76 ? 1 : 0, gTerrainTriangleCountQ76);

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
    const bool grounded = FindGround(x, z, feetY, groundY);
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
    return gPlayerCollisionReady && (!gWorldTriangles.empty() || !gTerrainCellsQ76.empty());
}

void RenderFo3CollisionOverlay(const float* mvp16) {
    if (!mvp16) return;

    if (gTerrainProgramQ76 && gTerrainVaoQ76 && gTerrainVertexCountQ76 > 0) {
        GLint previousProgram = 0;
        GLint previousVao = 0;
        GLboolean previousDepthMask = GL_TRUE;
        glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
        const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
        const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
        const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);

        if (!depthWasEnabled) glEnable(GL_DEPTH_TEST);
        if (blendWasEnabled) glDisable(GL_BLEND);
        if (cullWasEnabled) glDisable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        glUseProgram(gTerrainProgramQ76);
        glUniformMatrix4fv(gTerrainMvpLocationQ76, 1, GL_FALSE, mvp16);
        glBindVertexArray(gTerrainVaoQ76);
        glDrawArrays(GL_TRIANGLES, 0, gTerrainVertexCountQ76);

        glBindVertexArray(static_cast<GLuint>(previousVao));
        glUseProgram(static_cast<GLuint>(previousProgram));
        glDepthMask(previousDepthMask);
        if (cullWasEnabled) glEnable(GL_CULL_FACE);
        if (blendWasEnabled) glEnable(GL_BLEND);
        if (!depthWasEnabled) glDisable(GL_DEPTH_TEST);

        if (!gTerrainLoggedVisibleQ76) {
            gTerrainLoggedVisibleQ76 = true;
            Q76_LOGI("Q7.6 TERRAIN VISIBLE: cells=%zu triangles=%zu solidBothEyes=1 heightfieldCollision=1",
                     gTerrainCellsQ76.size(), gTerrainTriangleCountQ76);
        }
    }

    if (!SHOW_COLLISION_DEBUG_Q6G) return;
    if (!gProgram || !gVao || gVertexCount <= 0) return;

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
    if (gTerrainVboQ76) glDeleteBuffers(1, &gTerrainVboQ76);
    if (gTerrainVaoQ76) glDeleteVertexArrays(1, &gTerrainVaoQ76);
    if (gTerrainProgramQ76) glDeleteProgram(gTerrainProgramQ76);
    gVbo = 0;
    gVao = 0;
    gProgram = 0;
    gMvpLocation = -1;
    gVertexCount = 0;
    gTerrainVboQ76 = 0;
    gTerrainVaoQ76 = 0;
    gTerrainProgramQ76 = 0;
    gTerrainMvpLocationQ76 = -1;
    gTerrainVertexCountQ76 = 0;
    gTerrainTriangleCountQ76 = 0;
    gTerrainLoggedVisibleQ76 = false;
    gTerrainCellsQ76.clear();
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
}
