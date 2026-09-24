extern void PumpFo3AndroidEventsQ1860();
#include "fo3-collision-overlay.h"
#include "fo3-nif-collision-q6f.h"
#include "fo3-transition-q74.h"
#include "fo3-nif-metadata-q714.h"
#include "fo3-terrain-q76.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr size_t MAX_COLLISION_PLACEMENTS = 256u;
constexpr size_t MAX_EXTERIOR_COLLISION_PLACEMENTS_Q78A = 1024u;
constexpr size_t MAX_EXTERIOR_COLLISION_TRIANGLES_Q78A = 250000u;
constexpr size_t MAX_LINE_VERTICES = 800000u;
constexpr bool SHOW_COLLISION_DEBUG_Q6G = false;

constexpr float PLAYER_RADIUS = 0.26f;
constexpr float PLAYER_HEIGHT = 1.70f;
constexpr float PLAYER_SKIN = 0.003f;
constexpr float MAX_STEP_UP = 0.32f;
constexpr float EXTERIOR_MAX_STEP_UP_Q78B = 0.30f;
constexpr float STEP_MIN_RISE_Q78B = 0.015f;
constexpr float EXTERIOR_CONTACT_SLOP_Q712 = 0.015f;
constexpr float EXTERIOR_WALKABLE_NORMAL_Y_Q713 = 0.70f;
constexpr float EXTERIOR_STAIRS_NORMAL_Y_Q714 = 0.55f;
constexpr size_t EXTERIOR_MAX_MANIFOLD_CONTACTS_Q714 = 3u;
constexpr float SPAWN_AUTHORED_BELOW_Q78B = 0.80f;
constexpr float SPAWN_AUTHORED_ABOVE_Q78B = 0.80f;
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

    // Q7.14: preserve the authored packed-Havok identity instead of flattening
    // every triangle into one anonymous soup. Triangles from the same packed
    // subshape share a surface key and therefore contribute one manifold contact.
    uint64_t surfaceKeyQ714 = 0u;
    uint8_t havokLayerQ714 = 0u;
    uint32_t havokMaterialQ714 = 0u;
    bool stairsQ714 = false;
    bool platformQ714 = false;
    bool walkableModuleQ723 = false;

    // Q8.1: packed Havok welding and exact source-mesh edge identity.
    uint16_t weldingInfoQ801 = 0u;
    uint64_t meshKeyQ801 = 0u;
    uint32_t vertexAQ801 = 0xffffffffu;
    uint32_t vertexBQ801 = 0xffffffffu;
    uint32_t vertexCQ801 = 0xffffffffu;
};

struct WallContactQ714 {
    uint64_t surfaceKey = 0u;
    float nx = 0.0f;
    float nz = 0.0f;
    float push = 0.0f;
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

struct CollisionSurfaceSourceQ722 {
    uint32_t refFormId = 0u;
    std::string editorId;
    std::string modelPath;
};
std::vector<CollisionTriangle> gWorldTriangles;
std::unordered_map<uint64_t, CollisionSurfaceSourceQ722> gSurfaceSourcesQ722;

// Q16.27: collision is represented by transformed authored triangles rather than
// persistent physics-engine bodies. Cache the expensive per-REFR transform result;
// each rolling 3x3 rebuild can then memcpy the six overlapping cells and transform
// only the entering strip. The cache is scoped to one immutable exterior origin.
struct Q1990CachedCollisionPlacement {
    std::vector<CollisionTriangle> triangles;
    // One source record per authored Havok surface, not one per triangle.
    // Q18.2 uses this to rebuild the active source map without hashing every
    // triangle in the retained 3x3 on each CELL crossing.
    std::vector<uint64_t> surfaceKeys;
    size_t shapeCount = 0u;
    std::array<size_t, 5> kindCounts{};
    uint64_t lastUse = 0u;
};
std::unordered_map<uint32_t, Q1990CachedCollisionPlacement> gQ1990CollisionPlacementCache;
std::unordered_set<uint32_t> gQ1820NegativeCollisionPlacementCache;

// Immutable NIF collision assets are process-lifetime. Q18.2 needs the same
// caches both for active-world assembly and for budgeted prewarming.
std::unordered_map<std::string, std::vector<Fo3NifCollisionShapeQ6F>>
    gQ1820CollisionModelCache;
std::unordered_map<std::string, Fo3PackedMetadataMapQ714>
    gQ1820CollisionMetadataCache;
std::unordered_set<std::string> gQ1820NoCollisionModels;
uint64_t gQ1990CollisionCacheSerial = 0u;
bool gQ1990CollisionCacheContextValid = false;
float gQ1990CollisionCacheCenterX = 0.0f;
float gQ1990CollisionCacheCenterY = 0.0f;
float gQ1990CollisionCacheFloorZ = 0.0f;
float gQ1990CollisionCacheSceneForward = 0.0f;
float gQ1990CollisionCacheFloorY = 0.0f;
float gQ1990CollisionCacheUnitsPerMetre = 0.0f;
float gCollisionFloorY = -1.55f;
bool gPlayerCollisionReady = false;
bool gSafeSpawnResolved = false;
bool gExteriorAllBhksQ78A = false;
int gNextCollisionExteriorOverrideQ1931 = -1;

uint64_t gResolveCounter = 0;
uint64_t gContactLogCount = 0;
uint64_t gTerrainGroundLogCountQ77 = 0;
uint64_t gStepUpLogCountQ78B = 0;
uint64_t gManifoldLogCountQ714 = 0;

std::string NormalizeModelPathQ78A(const std::string& path) {
    std::string lower = path;
    for (char& ch : lower) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        if (ch == '/') ch = '\\';
    }
    return lower;
}

bool IsMegatonArchitecture(const std::string& path) {
    const std::string lower = NormalizeModelPathQ78A(path);
    return lower.find("architecture\\megaton") != std::string::npos;
}

bool IsWalkableModuleQ723(const Fo3WorldPlacement& placement) {
    std::string edid = placement.editorId;
    for (char& ch : edid) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    const std::string model = NormalizeModelPathQ78A(placement.modelPath);
    return edid.rfind("megatonramp", 0) == 0 ||
           edid.rfind("scrapgroundplate", 0) == 0 ||
           edid.rfind("woodplankgroup", 0) == 0 ||
           model.find("megatonramp") != std::string::npos ||
           model.find("scrapgroundplate") != std::string::npos ||
           model.find("woodplankgroup") != std::string::npos;
}

bool IsExteriorMegatonPlacementSetQ78A(const std::vector<Fo3WorldPlacement>& placements) {
    for (const Fo3WorldPlacement& placement : placements) {
        const std::string lower = NormalizeModelPathQ78A(placement.modelPath);
        const bool megatonArchitecture = lower.find("architecture\\megaton") != std::string::npos;
        const bool houseKit = lower.find("architecture\\megaton\\interior\\shackinteriors") != std::string::npos;
        if (megatonArchitecture && !houseKit) return true;
    }
    return false;
}

float StepHeightQ78B() {
    return gExteriorAllBhksQ78A ? EXTERIOR_MAX_STEP_UP_Q78B : MAX_STEP_UP;
}

float GroundNormalThresholdQ713() {
    return gExteriorAllBhksQ78A ? EXTERIOR_WALKABLE_NORMAL_Y_Q713 : 0.55f;
}

float WalkableNormalThresholdQ714(const CollisionTriangle& tri) {
    if (gExteriorAllBhksQ78A && tri.stairsQ714) {
        return std::min(GroundNormalThresholdQ713(), EXTERIOR_STAIRS_NORMAL_Y_Q714);
    }
    return GroundNormalThresholdQ713();
}

float CollisionRadiusQ712() {
    return gExteriorAllBhksQ78A
        ? std::max(0.05f, PLAYER_RADIUS - EXTERIOR_CONTACT_SLOP_Q712)
        : PLAYER_RADIUS;
}

uint64_t MakeSurfaceKeyQ714(uint32_t placementRef, uint32_t shapeBlock, uint16_t subShape) {
    // FNV-1a style combine; deterministic and sufficiently collision-resistant for
    // the few thousand collision surfaces present in one loaded scene.
    uint64_t key = 1469598103934665603ull;
    auto mix = [&](uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            key ^= (value >> (i * 8)) & 0xffu;
            key *= 1099511628211ull;
        }
    };
    mix(placementRef);
    mix(shapeBlock);
    mix(subShape);
    return key;
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
    float bestDistance = std::numeric_limits<float>::max();
    const float maxStepUp = StepHeightQ78B();
    for (const CollisionTriangle& tri : gWorldTriangles) {
        if (std::fabs(tri.normal.y) < WalkableNormalThresholdQ714(tri)) continue;
        if (x < tri.minX - 0.02f || x > tri.maxX + 0.02f ||
            z < tri.minZ - 0.02f || z > tri.maxZ + 0.02f) continue;

        float u = 0.0f, v = 0.0f, w = 0.0f;
        if (!BarycentricXZ(tri, x, z, u, v, w)) continue;
        const float y = tri.a.y*u + tri.b.y*v + tri.c.y*w;
        if (y > feetY + maxStepUp || y < feetY - MAX_GROUND_DROP) continue;

        if (gExteriorAllBhksQ78A) {
            const float verticalDistance = std::fabs(y - feetY);
            if (!found || verticalDistance < bestDistance - 0.0001f ||
                (std::fabs(verticalDistance - bestDistance) <= 0.0001f && y > best)) {
                bestDistance = verticalDistance;
                best = y;
                found = true;
            }
        } else if (y > best) {
            best = y;
            found = true;
        }
    }
    if (found) groundY = best;
    return found;
}

bool FindStepGroundQ713(float x, float z, float feetY, float& groundY) {
    bool found = false;
    float lowest = std::numeric_limits<float>::max();
    const float maxStepUp = StepHeightQ78B();
    for (const CollisionTriangle& tri : gWorldTriangles) {
        if (std::fabs(tri.normal.y) < WalkableNormalThresholdQ714(tri)) continue;
        if (x < tri.minX - 0.02f || x > tri.maxX + 0.02f ||
            z < tri.minZ - 0.02f || z > tri.maxZ + 0.02f) continue;

        float u = 0.0f, v = 0.0f, w = 0.0f;
        if (!BarycentricXZ(tri, x, z, u, v, w)) continue;
        const float y = tri.a.y*u + tri.b.y*v + tri.c.y*w;
        const float rise = y - feetY;
        if (rise <= STEP_MIN_RISE_Q78B || rise > maxStepUp + PLAYER_SKIN) continue;
        if (y < lowest) {
            lowest = y;
            found = true;
        }
    }
    if (found) groundY = lowest;
    return found;
}

bool FindGroundWide(float x, float z, float referenceY, float& groundY) {
    bool found = false;
    float bestDistance = std::numeric_limits<float>::max();
    float bestY = referenceY;
    for (const CollisionTriangle& tri : gWorldTriangles) {
        if (std::fabs(tri.normal.y) < WalkableNormalThresholdQ714(tri)) continue;
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

bool FindAuthoredSpawnGroundQ78B(float x, float z, float referenceY, float& groundY) {
    bool found = false;
    float highest = -1e30f;
    for (const CollisionTriangle& tri : gWorldTriangles) {
        if (std::fabs(tri.normal.y) < WalkableNormalThresholdQ714(tri)) continue;
        if (x < tri.minX - 0.02f || x > tri.maxX + 0.02f ||
            z < tri.minZ - 0.02f || z > tri.maxZ + 0.02f) continue;
        float u = 0.0f, v = 0.0f, w = 0.0f;
        if (!BarycentricXZ(tri, x, z, u, v, w)) continue;
        const float y = tri.a.y*u + tri.b.y*v + tri.c.y*w;
        if (y < referenceY - SPAWN_AUTHORED_BELOW_Q78B ||
            y > referenceY + SPAWN_AUTHORED_ABOVE_Q78B) continue;
        if (!found || y > highest) {
            highest = y;
            found = true;
        }
    }
    if (found) groundY = highest;
    return found;
}

bool ComputeWallContactQ714(const CollisionTriangle& tri, float x, float z,
                            float feetY, float collisionRadius,
                            WallContactQ714& out) {
    const float topY = feetY + PLAYER_HEIGHT;
    const float stepHeight = StepHeightQ78B();
    const float radius2 = collisionRadius * collisionRadius;

    if (std::fabs(tri.normal.y) >= 0.75f) return false;
    if (gExteriorAllBhksQ78A && tri.maxY <= feetY + stepHeight + PLAYER_SKIN) return false;
    if (tri.maxY < feetY + 0.04f || tri.minY > topY) return false;
    if (x < tri.minX - collisionRadius || x > tri.maxX + collisionRadius ||
        z < tri.minZ - collisionRadius || z > tri.maxZ + collisionRadius) return false;

    const Vec2 p{x, z};
    bool inside = false;
    const Vec2 q = ClosestPointTriangleXZ(tri, p, inside);
    float dx = x - q.x;
    float dz = z - q.y;
    const float d2 = dx*dx + dz*dz;
    if (d2 >= radius2) return false;

    float distance = std::sqrt(std::max(d2, 0.0f));
    if (distance > 1e-5f) {
        dx /= distance;
        dz /= distance;
    } else {
        dx = tri.normal.x;
        dz = tri.normal.z;
        float horizontalLength = std::sqrt(dx*dx + dz*dz);
        if (horizontalLength < 1e-5f) return false;
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

    out.surfaceKey = tri.surfaceKeyQ714;
    out.nx = dx;
    out.nz = dz;
    out.push = (collisionRadius - distance) + PLAYER_SKIN;
    return out.push > 0.0f;
}

uint32_t ResolveWallPenetrations(float& x, float& z, float feetY) {
    const float collisionRadius = CollisionRadiusQ712();

    // Preserve the proven interior Q6G response exactly. Q7.14 is deliberately
    // exterior-only until the authored metadata/manifold behaviour is validated.
    if (!gExteriorAllBhksQ78A) {
        uint32_t contacts = 0;
        for (int pass = 0; pass < MAX_DEPENETRATION_PASSES; ++pass) {
            bool changed = false;
            for (const CollisionTriangle& tri : gWorldTriangles) {
                WallContactQ714 contact;
                if (!ComputeWallContactQ714(tri, x, z, feetY, collisionRadius, contact)) continue;
                x += contact.nx * contact.push;
                z += contact.nz * contact.push;
                ++contacts;
                changed = true;
            }
            if (!changed) break;
        }
        return contacts;
    }

    uint32_t contacts = 0;
    for (int pass = 0; pass < MAX_DEPENETRATION_PASSES; ++pass) {
        std::unordered_map<uint64_t, WallContactQ714> strongestBySurface;
        size_t rawCandidates = 0u;

        for (const CollisionTriangle& tri : gWorldTriangles) {
            WallContactQ714 contact;
            if (!ComputeWallContactQ714(tri, x, z, feetY, collisionRadius, contact)) continue;
            ++rawCandidates;

            auto found = strongestBySurface.find(contact.surfaceKey);
            if (found == strongestBySurface.end() || contact.push > found->second.push) {
                strongestBySurface[contact.surfaceKey] = contact;
            }
        }

        if (strongestBySurface.empty()) break;

        std::vector<WallContactQ714> manifold;
        manifold.reserve(strongestBySurface.size());
        for (const auto& entry : strongestBySurface) manifold.push_back(entry.second);
        std::sort(manifold.begin(), manifold.end(), [](const WallContactQ714& a, const WallContactQ714& b) {
            return a.push > b.push;
        });

        const size_t applied = std::min(EXTERIOR_MAX_MANIFOLD_CONTACTS_Q714, manifold.size());
        for (size_t i = 0u; i < applied; ++i) {
            x += manifold[i].nx * manifold[i].push;
            z += manifold[i].nz * manifold[i].push;
            ++contacts;
        }

        if (rawCandidates >= 8u &&
            (gManifoldLogCountQ714 < 20u || (gResolveCounter % 360u) == 0u)) {
            ++gManifoldLogCountQ714;
            Q6G_LOGI("Q7.14 MANIFOLD: pass=%d rawTriangles=%zu surfaces=%zu applied=%zu pos=(%.3f %.3f)",
                     pass, rawCandidates, manifold.size(), applied, x, z);
        }
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

void SetNextFo3CollisionExteriorModeQ1931(bool exterior) {
    gNextCollisionExteriorOverrideQ1931 = exterior ? 1 : 0;
}

bool IsFo3CollisionPlacementCachedQ1820(uint32_t refFormId) {
    if (refFormId == 0u) return true;
    return gQ1990CollisionPlacementCache.find(refFormId) !=
               gQ1990CollisionPlacementCache.end() ||
           gQ1820NegativeCollisionPlacementCache.find(refFormId) !=
               gQ1820NegativeCollisionPlacementCache.end();
}

bool PrimeFo3CollisionPlacementCacheQ1820(
        const Fo3WorldPlacement& placement,
        float centerX, float centerY, float floorZ,
        float sceneForward, float floorY, float unitsPerMetre,
        size_t* outTriangles) {
    if (outTriangles) *outTriangles = 0u;
    if (placement.refFormId == 0u || unitsPerMetre <= 0.0f) return true;

    const bool sameContext = gQ1990CollisionCacheContextValid &&
        std::fabs(gQ1990CollisionCacheCenterX - centerX) < 0.01f &&
        std::fabs(gQ1990CollisionCacheCenterY - centerY) < 0.01f &&
        std::fabs(gQ1990CollisionCacheFloorZ - floorZ) < 0.01f &&
        std::fabs(gQ1990CollisionCacheSceneForward - sceneForward) < 0.0001f &&
        std::fabs(gQ1990CollisionCacheFloorY - floorY) < 0.0001f &&
        std::fabs(gQ1990CollisionCacheUnitsPerMetre - unitsPerMetre) < 0.0001f;
    if (!sameContext) {
        const size_t oldEntries = gQ1990CollisionPlacementCache.size();
        gQ1990CollisionPlacementCache.clear();
        gQ1820NegativeCollisionPlacementCache.clear();
        gQ1990CollisionCacheContextValid = true;
        gQ1990CollisionCacheCenterX = centerX;
        gQ1990CollisionCacheCenterY = centerY;
        gQ1990CollisionCacheFloorZ = floorZ;
        gQ1990CollisionCacheSceneForward = sceneForward;
        gQ1990CollisionCacheFloorY = floorY;
        gQ1990CollisionCacheUnitsPerMetre = unitsPerMetre;
        Q6F_LOGI("Q18.2 COLLISION PREWARM CONTEXT: oldEntries=%zu origin=(%.2f %.2f %.2f)",
                 oldEntries, centerX, centerY, floorZ);
    }

    auto already = gQ1990CollisionPlacementCache.find(placement.refFormId);
    if (already != gQ1990CollisionPlacementCache.end()) {
        already->second.lastUse = ++gQ1990CollisionCacheSerial;
        if (outTriangles) *outTriangles = already->second.triangles.size();
        return true;
    }
    if (gQ1820NegativeCollisionPlacementCache.find(placement.refFormId) !=
        gQ1820NegativeCollisionPlacementCache.end()) return true;

    if (gQ1820NoCollisionModels.find(placement.modelPath) !=
        gQ1820NoCollisionModels.end()) {
        gQ1820NegativeCollisionPlacementCache.insert(placement.refFormId);
        return true;
    }

    auto modelIt = gQ1820CollisionModelCache.find(placement.modelPath);
    if (modelIt == gQ1820CollisionModelCache.end()) {
        std::vector<Fo3NifCollisionShapeQ6F> shapes;
        if (!LoadFo3NifCollisionShapesQ6F(placement.modelPath, shapes) ||
            shapes.empty()) {
            gQ1820NoCollisionModels.insert(placement.modelPath);
            gQ1820NegativeCollisionPlacementCache.insert(placement.refFormId);
            return true;
        }
        modelIt = gQ1820CollisionModelCache.emplace(
            placement.modelPath, std::move(shapes)).first;
    }

    auto metadataIt = gQ1820CollisionMetadataCache.find(placement.modelPath);
    if (metadataIt == gQ1820CollisionMetadataCache.end()) {
        Fo3PackedMetadataMapQ714 metadata;
        LoadFo3PackedMetadataQ714(placement.modelPath, metadata);
        metadataIt = gQ1820CollisionMetadataCache.emplace(
            placement.modelPath, std::move(metadata)).first;
    }
    const Fo3PackedMetadataMapQ714& modelMetadata = metadataIt->second;

    Q1990CachedCollisionPlacement entry;
    std::unordered_set<uint64_t> uniqueSurfaceKeys;
    for (const Fo3NifCollisionShapeQ6F& shape : modelIt->second) {
        const size_t vertexCount = shape.positions.size() / 3u;
        if (vertexCount == 0u || shape.indices.empty()) continue;

        const std::vector<Fo3PackedSubShapeMetadataQ714>* packedMetadata = nullptr;
        if (shape.dataBlock != 0xffffffffu) {
            const auto metaIt = modelMetadata.find(shape.dataBlock);
            if (metaIt != modelMetadata.end()) packedMetadata = &metaIt->second;
        }

        const size_t beforeShape = entry.triangles.size();
        for (size_t i = 0; i + 2u < shape.indices.size(); i += 3u) {
            const uint32_t ia = shape.indices[i];
            const uint32_t ib = shape.indices[i + 1u];
            const uint32_t ic = shape.indices[i + 2u];
            if (ia >= vertexCount || ib >= vertexCount || ic >= vertexCount) continue;

            uint16_t subShapeIndex = 0xffffu;
            const Fo3PackedSubShapeMetadataQ714* meta = nullptr;
            if (packedMetadata) {
                subShapeIndex = FindFo3PackedSubShapeForTriangleQ714(
                    *packedMetadata, ia, ib, ic);
                if (subShapeIndex != 0xffffu &&
                    subShapeIndex < packedMetadata->size()) {
                    meta = &(*packedMetadata)[subShapeIndex];
                }
            }
            if (meta && !meta->BlocksPlayer()) continue;

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

            CollisionTriangle tri;
            if (!BuildTriangle(point(ia), point(ib), point(ic), tri)) continue;
            tri.walkableModuleQ723 = IsWalkableModuleQ723(placement);
            tri.surfaceKeyQ714 = MakeSurfaceKeyQ714(
                placement.refFormId, shape.sourceShapeBlock, subShapeIndex);
            tri.meshKeyQ801 = MakeSurfaceKeyQ714(
                placement.refFormId, shape.sourceShapeBlock, 0xfffeu);
            tri.vertexAQ801 = ia;
            tri.vertexBQ801 = ib;
            tri.vertexCQ801 = ic;
            const size_t ordinal = i / 3u;
            if (ordinal < shape.triangleWeldingInfo.size())
                tri.weldingInfoQ801 = shape.triangleWeldingInfo[ordinal];
            if (meta) {
                tri.havokLayerQ714 = meta->layer;
                tri.havokMaterialQ714 = meta->material;
                tri.stairsQ714 = meta->IsStairs();
                tri.platformQ714 = meta->IsPlatform();
            }
            if (uniqueSurfaceKeys.insert(tri.surfaceKeyQ714).second)
                entry.surfaceKeys.push_back(tri.surfaceKeyQ714);
            entry.triangles.push_back(tri);

            if (entry.triangles.size() > MAX_EXTERIOR_COLLISION_TRIANGLES_Q78A)
                return false;
        }
        if (entry.triangles.size() > beforeShape) {
            ++entry.shapeCount;
            ++entry.kindCounts[static_cast<size_t>(shape.kind)];
        }
    }

    if (entry.triangles.empty()) {
        gQ1820NegativeCollisionPlacementCache.insert(placement.refFormId);
        return true;
    }

    entry.lastUse = ++gQ1990CollisionCacheSerial;
    if (outTriangles) *outTriangles = entry.triangles.size();
    gQ1990CollisionPlacementCache[placement.refFormId] = std::move(entry);
    return true;
}

void InvalidateDerivedCollisionCachesQ17();

bool InitializeFo3CollisionOverlay(const std::vector<Fo3WorldPlacement>& placements,
                                   float centerX, float centerY, float floorZ,
                                   float sceneForward, float floorY,
                                   float unitsPerMetre) {
    ShutdownFo3CollisionOverlay();
    if (placements.empty() || unitsPerMetre <= 0.0f) return false;
    gCollisionFloorY = floorY;

    // Q18.2 also calls this from the budgeted prewarm path. It is a no-op for
    // ordinary rolling CELL streams because the exterior render origin is fixed.
    const bool q1990SameCollisionContext = gQ1990CollisionCacheContextValid &&
        std::fabs(gQ1990CollisionCacheCenterX - centerX) < 0.01f &&
        std::fabs(gQ1990CollisionCacheCenterY - centerY) < 0.01f &&
        std::fabs(gQ1990CollisionCacheFloorZ - floorZ) < 0.01f &&
        std::fabs(gQ1990CollisionCacheSceneForward - sceneForward) < 0.0001f &&
        std::fabs(gQ1990CollisionCacheFloorY - floorY) < 0.0001f &&
        std::fabs(gQ1990CollisionCacheUnitsPerMetre - unitsPerMetre) < 0.0001f;
    if (!q1990SameCollisionContext) {
        const size_t q1990OldEntries = gQ1990CollisionPlacementCache.size();
        gQ1990CollisionPlacementCache.clear();
        gQ1820NegativeCollisionPlacementCache.clear();
        gQ1990CollisionCacheContextValid = true;
        gQ1990CollisionCacheCenterX = centerX;
        gQ1990CollisionCacheCenterY = centerY;
        gQ1990CollisionCacheFloorZ = floorZ;
        gQ1990CollisionCacheSceneForward = sceneForward;
        gQ1990CollisionCacheFloorY = floorY;
        gQ1990CollisionCacheUnitsPerMetre = unitsPerMetre;
        Q6F_LOGI("Q18.2 COLLISION CACHE RESET: oldEntries=%zu origin=(%.2f %.2f %.2f) unitsPerMetre=%.2f",
                 q1990OldEntries, centerX, centerY, floorZ, unitsPerMetre);
    }

    const bool inferredExteriorQ1931 = IsExteriorMegatonPlacementSetQ78A(placements);
    const bool exteriorAllBhksQ78A =
        gNextCollisionExteriorOverrideQ1931 >= 0
            ? (gNextCollisionExteriorOverrideQ1931 != 0)
            : inferredExteriorQ1931;
    if (gNextCollisionExteriorOverrideQ1931 >= 0) {
        Q6F_LOGI("Q16.25 COLLISION MODE OVERRIDE: exterior=%d inferredMegaton=%d placements=%zu",
                 exteriorAllBhksQ78A ? 1 : 0,
                 inferredExteriorQ1931 ? 1 : 0,
                 placements.size());
    }
    gNextCollisionExteriorOverrideQ1931 = -1;
    gExteriorAllBhksQ78A = exteriorAllBhksQ78A;
    const size_t placementLimitQ78A = exteriorAllBhksQ78A
        ? MAX_EXTERIOR_COLLISION_PLACEMENTS_Q78A
        : MAX_COLLISION_PLACEMENTS;

    std::unordered_set<uint32_t> seenRefs;
    // Q18.2: shared with budgeted collision prewarming.
    auto& modelCache = gQ1820CollisionModelCache;
    auto& metadataCacheQ714 = gQ1820CollisionMetadataCache;
    auto& noCollisionModelsQ78A = gQ1820NoCollisionModels;
    std::vector<float> lines;
    lines.reserve(65536u);
    gWorldTriangles.reserve(exteriorAllBhksQ78A ? 65536u : 8192u);

    size_t misses = 0;
    size_t cacheHits = 0;
    size_t q1990PlacementCacheHits = 0u;
    size_t q1990PlacementCacheMisses = 0u;
    size_t q1990PlacementCacheTriangles = 0u;
    size_t negativeCacheHitsQ78A = 0;
    size_t pathFilteredQ78A = 0;
    size_t bhkAttemptsQ78A = 0;
    size_t metadataBlocksQ714 = 0u;
    size_t metadataTaggedTrianglesQ714 = 0u;
    size_t metadataUnmatchedTrianglesQ714 = 0u;
    size_t filteredNonSolidTrianglesQ714 = 0u;
    size_t stairTrianglesQ714 = 0u;
    size_t platformTrianglesQ714 = 0u;
    bool capped = false;
    size_t q1860CollisionPumpCount = 0u;

    for (const Fo3WorldPlacement& placement : placements) {
        PumpFo3AndroidEventsQ1860();
        ++q1860CollisionPumpCount;
        if (gPlacementCount >= placementLimitQ78A) {
            capped = true;
            break;
        }
        if (exteriorAllBhksQ78A &&
            gWorldTriangles.size() >= MAX_EXTERIOR_COLLISION_TRIANGLES_Q78A) {
            capped = true;
            break;
        }
        if (!exteriorAllBhksQ78A && !IsMegatonArchitecture(placement.modelPath)) {
            ++pathFilteredQ78A;
            continue;
        }
        if (!seenRefs.insert(placement.refFormId).second) continue;
        ++bhkAttemptsQ78A;

        if (exteriorAllBhksQ78A) {
            auto q1990CachedPlacement = gQ1990CollisionPlacementCache.find(placement.refFormId);
            if (q1990CachedPlacement != gQ1990CollisionPlacementCache.end()) {
                Q1990CachedCollisionPlacement& q1990Entry = q1990CachedPlacement->second;
                if (gWorldTriangles.size() + q1990Entry.triangles.size() >
                    MAX_EXTERIOR_COLLISION_TRIANGLES_Q78A) {
                    capped = true;
                    break;
                }
                gWorldTriangles.insert(gWorldTriangles.end(),
                                       q1990Entry.triangles.begin(), q1990Entry.triangles.end());
                for (uint64_t q1990SurfaceKey : q1990Entry.surfaceKeys) {
                    if (gSurfaceSourcesQ722.find(q1990SurfaceKey) ==
                        gSurfaceSourcesQ722.end()) {
                        gSurfaceSourcesQ722.emplace(
                            q1990SurfaceKey,
                            CollisionSurfaceSourceQ722{placement.refFormId,
                                                       placement.editorId,
                                                       placement.modelPath});
                    }
                }
                ++gPlacementCount;
                gCollisionShapeCount += q1990Entry.shapeCount;
                gTriangleCount += q1990Entry.triangles.size();
                for (size_t q1990Kind = 0u; q1990Kind < gKindCounts.size(); ++q1990Kind)
                    gKindCounts[q1990Kind] += q1990Entry.kindCounts[q1990Kind];
                ++q1990PlacementCacheHits;
                q1990PlacementCacheTriangles += q1990Entry.triangles.size();
                q1990Entry.lastUse = ++gQ1990CollisionCacheSerial;
                continue;
            }
            ++q1990PlacementCacheMisses;
        }

        if (exteriorAllBhksQ78A &&
            gQ1820NegativeCollisionPlacementCache.find(placement.refFormId) !=
                gQ1820NegativeCollisionPlacementCache.end()) {
            ++negativeCacheHitsQ78A;
            continue;
        }
        if (noCollisionModelsQ78A.find(placement.modelPath) != noCollisionModelsQ78A.end()) {
            if (exteriorAllBhksQ78A && placement.refFormId != 0u)
                gQ1820NegativeCollisionPlacementCache.insert(placement.refFormId);
            ++negativeCacheHitsQ78A;
            continue;
        }

        auto cached = modelCache.find(placement.modelPath);
        if (cached == modelCache.end()) {
            std::vector<Fo3NifCollisionShapeQ6F> shapes;
            if (!LoadFo3NifCollisionShapesQ6F(placement.modelPath, shapes) || shapes.empty()) {
                noCollisionModelsQ78A.insert(placement.modelPath);
                ++misses;
                continue;
            }
            cached = modelCache.emplace(placement.modelPath, std::move(shapes)).first;

            Fo3PackedMetadataMapQ714 metadata;
            LoadFo3PackedMetadataQ714(placement.modelPath, metadata);
            metadataBlocksQ714 += metadata.size();
            metadataCacheQ714.emplace(placement.modelPath, std::move(metadata));
        } else {
            ++cacheHits;
        }

        if (metadataCacheQ714.find(placement.modelPath) == metadataCacheQ714.end()) {
            Fo3PackedMetadataMapQ714 metadata;
            LoadFo3PackedMetadataQ714(placement.modelPath, metadata);
            metadataBlocksQ714 += metadata.size();
            metadataCacheQ714.emplace(placement.modelPath, std::move(metadata));
        }
        const Fo3PackedMetadataMapQ714& modelMetadataQ714 = metadataCacheQ714.at(placement.modelPath);

        const size_t q1990PlacementTriangleStart = gWorldTriangles.size();
        size_t placementTriangles = 0;
        size_t placementShapes = 0;
        std::array<size_t, 5> placementKinds{};
        const std::vector<Fo3NifCollisionShapeQ6F>& shapes = cached->second;
        for (const Fo3NifCollisionShapeQ6F& shape : shapes) {
            const size_t vertexCount = shape.positions.size() / 3u;
            if (vertexCount == 0u || shape.indices.empty()) continue;

            const std::vector<Fo3PackedSubShapeMetadataQ714>* packedMetadata = nullptr;
            if (shape.dataBlock != 0xffffffffu) {
                const auto metaIt = modelMetadataQ714.find(shape.dataBlock);
                if (metaIt != modelMetadataQ714.end()) packedMetadata = &metaIt->second;
            }

            for (size_t i = 0; i + 2u < shape.indices.size(); i += 3u) {
                if (exteriorAllBhksQ78A &&
                    gWorldTriangles.size() >= MAX_EXTERIOR_COLLISION_TRIANGLES_Q78A) {
                    capped = true;
                    break;
                }
                const uint32_t ia = shape.indices[i];
                const uint32_t ib = shape.indices[i + 1u];
                const uint32_t ic = shape.indices[i + 2u];
                if (ia >= vertexCount || ib >= vertexCount || ic >= vertexCount) continue;

                uint16_t subShapeIndexQ714 = 0xffffu;
                const Fo3PackedSubShapeMetadataQ714* metaQ714 = nullptr;
                if (packedMetadata) {
                    subShapeIndexQ714 = FindFo3PackedSubShapeForTriangleQ714(
                        *packedMetadata, ia, ib, ic);
                    if (subShapeIndexQ714 != 0xffffu &&
                        subShapeIndexQ714 < packedMetadata->size()) {
                        metaQ714 = &(*packedMetadata)[subShapeIndexQ714];
                    }
                }

                if (exteriorAllBhksQ78A && metaQ714 && !metaQ714->BlocksPlayer()) {
                    ++filteredNonSolidTrianglesQ714;
                    continue;
                }

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
                worldTriangle.walkableModuleQ723 = IsWalkableModuleQ723(placement);
                worldTriangle.surfaceKeyQ714 = MakeSurfaceKeyQ714(
                    placement.refFormId, shape.sourceShapeBlock, subShapeIndexQ714);
                worldTriangle.meshKeyQ801 = MakeSurfaceKeyQ714(
                    placement.refFormId, shape.sourceShapeBlock, 0xfffeu);
                worldTriangle.vertexAQ801 = ia;
                worldTriangle.vertexBQ801 = ib;
                worldTriangle.vertexCQ801 = ic;
                const size_t packedTriOrdinalQ801 = i / 3u;
                if (packedTriOrdinalQ801 < shape.triangleWeldingInfo.size()) {
                    worldTriangle.weldingInfoQ801 =
                        shape.triangleWeldingInfo[packedTriOrdinalQ801];
                }
                if (gSurfaceSourcesQ722.find(worldTriangle.surfaceKeyQ714) == gSurfaceSourcesQ722.end()) {
                    gSurfaceSourcesQ722.emplace(
                        worldTriangle.surfaceKeyQ714,
                        CollisionSurfaceSourceQ722{placement.refFormId,
                                                   placement.editorId,
                                                   placement.modelPath});
                }
                if (metaQ714) {
                    worldTriangle.havokLayerQ714 = metaQ714->layer;
                    worldTriangle.havokMaterialQ714 = metaQ714->material;
                    worldTriangle.stairsQ714 = metaQ714->IsStairs();
                    worldTriangle.platformQ714 = metaQ714->IsPlatform();
                    ++metadataTaggedTrianglesQ714;
                    if (worldTriangle.stairsQ714) ++stairTrianglesQ714;
                    if (worldTriangle.platformQ714) ++platformTrianglesQ714;
                } else if (packedMetadata) {
                    ++metadataUnmatchedTrianglesQ714;
                }
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
            if (exteriorAllBhksQ78A && !capped && placement.refFormId != 0u &&
                q1990PlacementTriangleStart < gWorldTriangles.size()) {
                Q1990CachedCollisionPlacement q1990Entry;
                q1990Entry.triangles.assign(
                    gWorldTriangles.begin() + static_cast<std::ptrdiff_t>(q1990PlacementTriangleStart),
                    gWorldTriangles.end());
                {
                    std::unordered_set<uint64_t> q1820SurfaceKeys;
                    for (const CollisionTriangle& q1820Tri : q1990Entry.triangles) {
                        if (q1820SurfaceKeys.insert(q1820Tri.surfaceKeyQ714).second)
                            q1990Entry.surfaceKeys.push_back(q1820Tri.surfaceKeyQ714);
                    }
                }
                q1990Entry.shapeCount = placementShapes;
                q1990Entry.kindCounts = placementKinds;
                q1990Entry.lastUse = ++gQ1990CollisionCacheSerial;
                gQ1990CollisionPlacementCache[placement.refFormId] = std::move(q1990Entry);
            }
            if (gPlacementCount <= 80u) {
                Q6F_LOGI("Q6F COLLISION OBJECT: ref=%08X EDID=%s model=%s shapes=%zu triangles=%zu packed=%zu convex=%zu box=%zu sphere=%zu capsule=%zu",
                         placement.refFormId,
                         placement.editorId.empty() ? "<none>" : placement.editorId.c_str(),
                         placement.modelPath.c_str(), placementShapes, placementTriangles,
                         placementKinds[0], placementKinds[1], placementKinds[2], placementKinds[3], placementKinds[4]);
            }
        }
        if (capped) break;
    }

    Q6G_LOGI("Q7.8A COLLISION COVERAGE: exterior=%d policy=%s inputPlacements=%zu bhkAttempts=%zu successfulPlacements=%zu triangles=%zu uniqueCollisionModels=%zu noCollisionModels=%zu cacheHits=%zu negativeCacheHits=%zu pathFiltered=%zu placementLimit=%zu triangleLimit=%zu capped=%d",
             exteriorAllBhksQ78A ? 1 : 0,
             exteriorAllBhksQ78A ? "all-authored-bhk" : "megaton-architecture-only",
             placements.size(), bhkAttemptsQ78A, gPlacementCount, gWorldTriangles.size(),
             modelCache.size(), noCollisionModelsQ78A.size(), cacheHits,
             negativeCacheHitsQ78A, pathFilteredQ78A, placementLimitQ78A,
             exteriorAllBhksQ78A ? MAX_EXTERIOR_COLLISION_TRIANGLES_Q78A : 0u,
             capped ? 1 : 0);

    Q6G_LOGI("Q7.14 HAVOK META READY: metadataBlocks=%zu taggedTriangles=%zu unmatchedPackedTriangles=%zu filteredNonSolidTriangles=%zu stairsTriangles=%zu platformTriangles=%zu exteriorFilter=%d",
             metadataBlocksQ714, metadataTaggedTrianglesQ714,
             metadataUnmatchedTrianglesQ714, filteredNonSolidTrianglesQ714,
             stairTrianglesQ714, platformTrianglesQ714,
             exteriorAllBhksQ78A ? 1 : 0);

    // Bound the persistent transformed cache. A few thousand refs cover several
    // neighbouring 3x3 windows/backtracking without turning traversal into an
    // unbounded memory sink.
    if (gQ1990CollisionPlacementCache.size() > 4096u) {
        std::vector<std::pair<uint64_t, uint32_t>> q1990Ages;
        q1990Ages.reserve(gQ1990CollisionPlacementCache.size());
        for (const auto& q1990Pair : gQ1990CollisionPlacementCache)
            q1990Ages.emplace_back(q1990Pair.second.lastUse, q1990Pair.first);
        std::sort(q1990Ages.begin(), q1990Ages.end());
        const size_t q1990Remove = gQ1990CollisionPlacementCache.size() - 3072u;
        for (size_t q1990Index = 0u; q1990Index < q1990Remove; ++q1990Index)
            gQ1990CollisionPlacementCache.erase(q1990Ages[q1990Index].second);
    }
    Q6F_LOGI("Q16.27 COLLISION CACHE: placementHits=%zu placementMisses=%zu reusedTriangles=%zu entries=%zu mode=persistent-transformed-REFR-chunks",
             q1990PlacementCacheHits, q1990PlacementCacheMisses,
             q1990PlacementCacheTriangles, gQ1990CollisionPlacementCache.size());

    if (gWorldTriangles.empty()) {
        Q6F_LOGW("Q6F COLLISION READY FAILED: placements=0 misses=%zu uniqueModels=%zu",
                 misses, modelCache.size());
        return false;
    }

    InvalidateDerivedCollisionCachesQ17();
    gPlayerCollisionReady = true;
    Q6F_LOGI("Q6F COLLISION READY: placements=%zu shapes=%zu triangles=%zu uniqueModels=%zu modelCacheHits=%zu misses=%zu packed=%zu convex=%zu box=%zu sphere=%zu capsule=%zu capped=%d",
             gPlacementCount, gCollisionShapeCount, gTriangleCount,
             modelCache.size(), cacheHits, misses,
             gKindCounts[0], gKindCounts[1], gKindCounts[2], gKindCounts[3], gKindCounts[4], capped ? 1 : 0);
    Q6G_LOGI("Q7.16 PHYSICS READY: controller=sweep-slide-step recoveryOnly=1 authoredTriangles=%zu capsuleRadius=%.2f exteriorCollisionRadius=%.3f contactSlop=%.3f interiorStep=%.2f exteriorStep=%.2f exteriorWalkableNormalY=%.2f authoredStairsNormalY=%.2f manifoldContacts=%zu predictiveStep=0 exterior=%d",
             gWorldTriangles.size(), PLAYER_RADIUS, CollisionRadiusQ712(),
             gExteriorAllBhksQ78A ? EXTERIOR_CONTACT_SLOP_Q712 : 0.0f,
             MAX_STEP_UP, EXTERIOR_MAX_STEP_UP_Q78B,
             gExteriorAllBhksQ78A ? EXTERIOR_WALKABLE_NORMAL_Y_Q713 : 0.55f,
             EXTERIOR_STAIRS_NORMAL_Y_Q714,
             gExteriorAllBhksQ78A ? EXTERIOR_MAX_MANIFOLD_CONTACTS_Q714 : 0u,
             gExteriorAllBhksQ78A ? 1 : 0);

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

bool ResolveFo3PlayerMotionLegacyQ716(float currentX, float currentZ,
                               float desiredX, float desiredZ,
                               float currentPlayerYOffset,
                               float* outX, float* outZ,
                               float* outPlayerYOffset) {
    if (!outX || !outZ || !outPlayerYOffset) return false;
    if (ConsumeFo3PlayerResetQ74()) {
        *outX = 0.0f;
        *outZ = 0.0f;
        *outPlayerYOffset = 0.0f;
        gSafeSpawnResolved = false;
        Q6G_LOGI("Q7.4 PLAYER RESET: bodyCenter=(0,0) source=XTEL newCollisionReady=%d",
                 gPlayerCollisionReady ? 1 : 0);
        return true;
    }
    if (!gPlayerCollisionReady || gWorldTriangles.empty()) return false;

    ++gResolveCounter;

    if (!gSafeSpawnResolved) {
        if (gExteriorAllBhksQ78A) {
            const float referenceFeetY = gCollisionFloorY + currentPlayerYOffset;
            float authoredSpawnGroundY = referenceFeetY;
            if (FindAuthoredSpawnGroundQ78B(currentX, currentZ,
                                            referenceFeetY, authoredSpawnGroundY) &&
                !HasOverheadBlock(currentX, currentZ, authoredSpawnGroundY)) {
                *outX = currentX;
                *outZ = currentZ;
                *outPlayerYOffset = authoredSpawnGroundY - gCollisionFloorY;
                gSafeSpawnResolved = true;
                Q6I_LOGI("Q7.8B XTEL AUTHORED SPAWN: seed=(%.3f %.3f) referenceY=%.3f groundY=%.3f playerY=%.3f source=bhk LANDfallback=0",
                         currentX, currentZ, referenceFeetY, authoredSpawnGroundY,
                         *outPlayerYOffset);
                return true;
            }
        }

        float terrainSpawnY = currentPlayerYOffset;
        if (SampleFo3TerrainGroundQ77(currentX, currentZ, &terrainSpawnY)) {
            *outX = currentX;
            *outZ = currentZ;
            *outPlayerYOffset = terrainSpawnY;
            gSafeSpawnResolved = true;
            Q6I_LOGI("Q7.8B XTEL LAND FALLBACK: seed=(%.3f %.3f) playerY=%.3f source=VHGT authoredNearMarker=0",
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

        *outX = currentX;
        *outZ = currentZ;
        *outPlayerYOffset = currentPlayerYOffset;
        Q6I_LOGW("Q6I SAFE SPAWN DEFER: no grounded collision candidate within 4.80m; movement held and retrying");
        return true;
    }

    float x = currentX;
    float z = currentZ;
    float feetY = gCollisionFloorY + currentPlayerYOffset;

    if (gExteriorAllBhksQ78A) {
        float currentGroundY = feetY;
        if (FindGround(x, z, feetY, currentGroundY) &&
            currentGroundY > feetY + STEP_MIN_RISE_Q78B &&
            !HasOverheadBlock(x, z, currentGroundY)) {
            feetY = currentGroundY;
        }
    }

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

        if (gExteriorAllBhksQ78A) {
            float stepGroundY = feetY;
            if (FindStepGroundQ713(targetX, targetZ, feetY, stepGroundY) &&
                !HasOverheadBlock(targetX, targetZ, stepGroundY)) {
                const float rise = stepGroundY - feetY;
                feetY = stepGroundY;
                if (gStepUpLogCountQ78B < 20u || (gResolveCounter % 360u) == 0u) {
                    ++gStepUpLogCountQ78B;
                    Q6G_LOGI("Q7.14 STEP UP: target=(%.3f %.3f) rise=%.3f feetY=%.3f maxStep=%.3f baseWalkableNormalY=%.2f authoredStairs=1-or-normal predictive=0",
                             targetX, targetZ, rise, feetY, StepHeightQ78B(),
                             GroundNormalThresholdQ713());
                }
            }
        }

        x = targetX;
        z = targetZ;
        contacts += ResolveWallPenetrations(x, z, feetY);
    }

    float groundY = feetY;
    bool grounded = FindGround(x, z, feetY, groundY);
    const bool authoredGrounded = grounded;

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

    float playerYOffset = feetY - gCollisionFloorY;
    if (grounded) playerYOffset = groundY - gCollisionFloorY;

    *outX = x;
    *outZ = z;
    *outPlayerYOffset = playerYOffset;

    const float correctionX = x - desiredX;
    const float correctionZ = z - desiredZ;
    const bool blocked = contacts > 0u || (correctionX*correctionX + correctionZ*correctionZ) > 0.000004f;
    if (blocked && (gContactLogCount < 24u || (gResolveCounter % 120u) == 0u)) {
        ++gContactLogCount;
        Q6G_LOGI("Q7.14 CONTACT: desired=(%.3f %.3f) resolved=(%.3f %.3f) correction=(%.3f %.3f) manifoldContacts=%u grounded=%d playerY=%.3f",
                 desiredX, desiredZ, x, z,
                 correctionX, correctionZ, contacts,
                 grounded ? 1 : 0, playerYOffset);
    }
    return true;
}


#include "fo3-player-controller-runtime.inc"

// Q19.3 exterior collision snapshot. All expensive derived structures are built
// from the prewarmed transformed REFR cache on the serialized asset worker.
// The live controller continues using the previous snapshot until publish.
struct Q1930PreparedCollisionSnapshot {
    std::vector<CollisionTriangle> triangles;
    std::unordered_map<uint64_t, CollisionSurfaceSourceQ722> surfaceSources;
    size_t placementCount = 0u;
    size_t collisionShapeCount = 0u;
    size_t triangleCount = 0u;
    std::array<size_t, 5> kindCounts{};
    float floorY = -1.55f;

    std::vector<std::array<int32_t, 3>> weldNeighbours;
    std::vector<HkCoherentShapeQ900> shapes;
    std::unordered_map<uint64_t, size_t> shapeIndex;

    std::vector<Q950CollisionObject> q950Objects;
    std::vector<uint32_t> q950TriangleObject;
    std::unordered_map<uint64_t, Q950GridCell> q950Grid;
    std::vector<uint32_t> q950LargeObjects;
    std::vector<uint32_t> q950LargeLegacy;
    std::vector<uint32_t> q950ObjectStamp;
    std::vector<uint32_t> q950TriangleStamp;

    size_t shapedTriangles = 0u;
    size_t walkableTriangles = 0u;
    size_t authoredWeldTriangles = 0u;
    size_t sharedEdges = 0u;
    size_t q950ShapedTriangles = 0u;
    size_t q950LegacyTriangles = 0u;

    uint64_t assemblyUs = 0u;
    uint64_t shapesUs = 0u;
    uint64_t weldUs = 0u;
    uint64_t broadphaseUs = 0u;
    uint64_t totalUs = 0u;
};

std::mutex gQ1930SnapshotMutex;
std::unordered_map<uint64_t, std::shared_ptr<Q1930PreparedCollisionSnapshot>>
    gQ1930PreparedSnapshots;
uint64_t gQ1930SnapshotSerial = 0u;

float Q1930WalkableThreshold(const CollisionTriangle& tri) {
    return tri.stairsQ714
        ? std::min(EXTERIOR_WALKABLE_NORMAL_Y_Q713,
                   EXTERIOR_STAIRS_NORMAL_Y_Q714)
        : EXTERIOR_WALKABLE_NORMAL_Y_Q713;
}

bool Q1930AssembleCachedExterior(
        const std::vector<Fo3WorldPlacement>& placements,
        Q1930PreparedCollisionSnapshot& snapshot) {
    std::unordered_set<uint32_t> seenRefs;
    snapshot.triangles.reserve(140000u);

    for (const Fo3WorldPlacement& placement : placements) {
        if (placement.refFormId == 0u ||
            !seenRefs.insert(placement.refFormId).second) continue;
        if (snapshot.placementCount >= MAX_EXTERIOR_COLLISION_PLACEMENTS_Q78A)
            break;

        const auto cached =
            gQ1990CollisionPlacementCache.find(placement.refFormId);
        if (cached == gQ1990CollisionPlacementCache.end()) {
            if (gQ1820NegativeCollisionPlacementCache.find(placement.refFormId) !=
                gQ1820NegativeCollisionPlacementCache.end()) {
                continue;
            }
            Q6G_LOGW("Q19.3 COLLISION SNAPSHOT MISS: ref=%08X model=%s reason=not-prewarmed",
                     placement.refFormId, placement.modelPath.c_str());
            return false;
        }

        Q1990CachedCollisionPlacement& entry = cached->second;
        if (snapshot.triangles.size() + entry.triangles.size() >
            MAX_EXTERIOR_COLLISION_TRIANGLES_Q78A) {
            break;
        }

        snapshot.triangles.insert(snapshot.triangles.end(),
                                  entry.triangles.begin(),
                                  entry.triangles.end());
        for (uint64_t surfaceKey : entry.surfaceKeys) {
            snapshot.surfaceSources.emplace(
                surfaceKey,
                CollisionSurfaceSourceQ722{
                    placement.refFormId,
                    placement.editorId,
                    placement.modelPath});
        }
        ++snapshot.placementCount;
        snapshot.collisionShapeCount += entry.shapeCount;
        snapshot.triangleCount += entry.triangles.size();
        for (size_t i = 0u; i < snapshot.kindCounts.size(); ++i)
            snapshot.kindCounts[i] += entry.kindCounts[i];
        entry.lastUse = ++gQ1990CollisionCacheSerial;
    }
    return !snapshot.triangles.empty();
}

void Q1930BuildShapes(Q1930PreparedCollisionSnapshot& snapshot) {
    snapshot.shapes.clear();
    snapshot.shapeIndex.clear();
    snapshot.shapeIndex.reserve(snapshot.triangles.size() / 8u + 32u);

    for (size_t triIndex = 0u; triIndex < snapshot.triangles.size(); ++triIndex) {
        const CollisionTriangle& tri = snapshot.triangles[triIndex];
        if (tri.meshKeyQ801 == 0u) continue;
        ++snapshot.shapedTriangles;

        size_t shapeIndex = 0u;
        const auto found = snapshot.shapeIndex.find(tri.meshKeyQ801);
        if (found == snapshot.shapeIndex.end()) {
            shapeIndex = snapshot.shapes.size();
            HkCoherentShapeQ900 shape;
            shape.key = tri.meshKeyQ801;
            shape.minX = tri.minX; shape.maxX = tri.maxX;
            shape.minY = tri.minY; shape.maxY = tri.maxY;
            shape.minZ = tri.minZ; shape.maxZ = tri.maxZ;
            snapshot.shapes.push_back(std::move(shape));
            snapshot.shapeIndex.emplace(tri.meshKeyQ801, shapeIndex);
        } else {
            shapeIndex = found->second;
        }

        HkCoherentShapeQ900& shape = snapshot.shapes[shapeIndex];
        shape.triangles.push_back(triIndex);
        shape.minX = std::min(shape.minX, tri.minX);
        shape.maxX = std::max(shape.maxX, tri.maxX);
        shape.minY = std::min(shape.minY, tri.minY);
        shape.maxY = std::max(shape.maxY, tri.maxY);
        shape.minZ = std::min(shape.minZ, tri.minZ);
        shape.maxZ = std::max(shape.maxZ, tri.maxZ);
        if (std::fabs(tri.normal.y) >= Q1930WalkableThreshold(tri)) {
            ++shape.walkableTriangles;
            ++snapshot.walkableTriangles;
        }
    }
}

void Q1930BuildWeldAdjacency(Q1930PreparedCollisionSnapshot& snapshot) {
    snapshot.weldNeighbours.assign(
        snapshot.triangles.size(), std::array<int32_t,3>{-1,-1,-1});
    struct EdgeOwner { size_t tri = 0u; int edge = 0; };
    std::unordered_map<HkWeldEdgeKeyQ801, EdgeOwner, HkWeldEdgeHashQ801> first;
    first.reserve(snapshot.triangles.size() * 2u);

    for (size_t triIndex = 0u; triIndex < snapshot.triangles.size(); ++triIndex) {
        const CollisionTriangle& tri = snapshot.triangles[triIndex];
        if (tri.weldingInfoQ801 != 0u) ++snapshot.authoredWeldTriangles;
        if (tri.meshKeyQ801 == 0u ||
            tri.vertexAQ801 == 0xffffffffu ||
            tri.vertexBQ801 == 0xffffffffu ||
            tri.vertexCQ801 == 0xffffffffu) continue;

        const std::array<std::pair<uint32_t,uint32_t>,3> edges{{
            {tri.vertexAQ801, tri.vertexBQ801},
            {tri.vertexBQ801, tri.vertexCQ801},
            {tri.vertexCQ801, tri.vertexAQ801}
        }};
        for (int edge = 0; edge < 3; ++edge) {
            uint32_t a = edges[edge].first;
            uint32_t b = edges[edge].second;
            if (a > b) std::swap(a, b);
            const HkWeldEdgeKeyQ801 key{tri.meshKeyQ801, a, b};
            const auto found = first.find(key);
            if (found == first.end()) {
                first.emplace(key, EdgeOwner{triIndex, edge});
            } else {
                const EdgeOwner owner = found->second;
                if (snapshot.weldNeighbours[owner.tri][owner.edge] < 0 &&
                    snapshot.weldNeighbours[triIndex][edge] < 0) {
                    snapshot.weldNeighbours[owner.tri][owner.edge] =
                        static_cast<int32_t>(triIndex);
                    snapshot.weldNeighbours[triIndex][edge] =
                        static_cast<int32_t>(owner.tri);
                    ++snapshot.sharedEdges;
                }
            }
        }
    }
}

void Q1930BuildBroadphase(Q1930PreparedCollisionSnapshot& snapshot) {
    snapshot.q950Objects.clear();
    snapshot.q950TriangleObject.assign(
        snapshot.triangles.size(), 0xffffffffu);
    snapshot.q950Grid.clear();
    snapshot.q950LargeObjects.clear();
    snapshot.q950LargeLegacy.clear();

    std::unordered_map<uint64_t,uint32_t> objectByKey;
    objectByKey.reserve(snapshot.shapes.size() + 32u);

    for (const HkCoherentShapeQ900& shape : snapshot.shapes) {
        if (shape.triangles.empty()) continue;
        uint32_t refFormId = 0u;
        for (size_t triIndex : shape.triangles) {
            if (triIndex >= snapshot.triangles.size()) continue;
            const auto src = snapshot.surfaceSources.find(
                snapshot.triangles[triIndex].surfaceKeyQ714);
            if (src != snapshot.surfaceSources.end() &&
                src->second.refFormId != 0u) {
                refFormId = src->second.refFormId;
                break;
            }
        }
        const uint64_t objectKey = refFormId != 0u
            ? (0x8000000000000000ULL | static_cast<uint64_t>(refFormId))
            : shape.key;

        uint32_t objectIndex = 0u;
        const auto found = objectByKey.find(objectKey);
        if (found == objectByKey.end()) {
            objectIndex = static_cast<uint32_t>(snapshot.q950Objects.size());
            Q950CollisionObject object;
            object.key = objectKey;
            object.refFormId = refFormId;
            object.minX = shape.minX; object.maxX = shape.maxX;
            object.minY = shape.minY; object.maxY = shape.maxY;
            object.minZ = shape.minZ; object.maxZ = shape.maxZ;
            snapshot.q950Objects.push_back(std::move(object));
            objectByKey.emplace(objectKey, objectIndex);
        } else {
            objectIndex = found->second;
        }

        Q950CollisionObject& object = snapshot.q950Objects[objectIndex];
        object.shapeKeys.push_back(shape.key);
        object.minX = std::min(object.minX, shape.minX);
        object.maxX = std::max(object.maxX, shape.maxX);
        object.minY = std::min(object.minY, shape.minY);
        object.maxY = std::max(object.maxY, shape.maxY);
        object.minZ = std::min(object.minZ, shape.minZ);
        object.maxZ = std::max(object.maxZ, shape.maxZ);
        object.walkableTriangles += shape.walkableTriangles;
        for (size_t triIndex : shape.triangles) {
            if (triIndex >= snapshot.triangles.size()) continue;
            object.triangles.push_back(triIndex);
            snapshot.q950TriangleObject[triIndex] = objectIndex;
            ++snapshot.q950ShapedTriangles;
        }
    }

    auto insertObject = [&](uint32_t objectIndex) {
        const Q950CollisionObject& object = snapshot.q950Objects[objectIndex];
        const int minCx = Q950CellCoord(object.minX);
        const int maxCx = Q950CellCoord(object.maxX);
        const int minCz = Q950CellCoord(object.minZ);
        const int maxCz = Q950CellCoord(object.maxZ);
        const int64_t spanX = static_cast<int64_t>(maxCx) - minCx + 1;
        const int64_t spanZ = static_cast<int64_t>(maxCz) - minCz + 1;
        if (spanX <= 0 || spanZ <= 0 ||
            static_cast<uint64_t>(spanX * spanZ) >
                Q950_MAX_GRID_CELLS_PER_ITEM) {
            snapshot.q950LargeObjects.push_back(objectIndex);
            return;
        }
        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cz = minCz; cz <= maxCz; ++cz)
                snapshot.q950Grid[Q950CellKey(cx,cz)].objects.push_back(
                    objectIndex);
    };
    for (uint32_t i = 0u; i < snapshot.q950Objects.size(); ++i)
        insertObject(i);

    for (uint32_t triIndex = 0u;
         triIndex < snapshot.triangles.size(); ++triIndex) {
        if (snapshot.q950TriangleObject[triIndex] != 0xffffffffu) continue;
        ++snapshot.q950LegacyTriangles;
        const CollisionTriangle& tri = snapshot.triangles[triIndex];
        const int minCx = Q950CellCoord(tri.minX);
        const int maxCx = Q950CellCoord(tri.maxX);
        const int minCz = Q950CellCoord(tri.minZ);
        const int maxCz = Q950CellCoord(tri.maxZ);
        const int64_t spanX = static_cast<int64_t>(maxCx) - minCx + 1;
        const int64_t spanZ = static_cast<int64_t>(maxCz) - minCz + 1;
        if (spanX <= 0 || spanZ <= 0 ||
            static_cast<uint64_t>(spanX * spanZ) >
                Q950_MAX_GRID_CELLS_PER_ITEM) {
            snapshot.q950LargeLegacy.push_back(triIndex);
            continue;
        }
        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cz = minCz; cz <= maxCz; ++cz)
                snapshot.q950Grid[Q950CellKey(cx,cz)]
                    .legacyTriangles.push_back(triIndex);
    }

    snapshot.q950ObjectStamp.assign(snapshot.q950Objects.size(), 0u);
    snapshot.q950TriangleStamp.assign(snapshot.triangles.size(), 0u);
}

bool PrepareFo3CollisionSnapshotQ1930(
        const std::vector<Fo3WorldPlacement>& placements,
        float centerX, float centerY, float floorZ,
        float sceneForward, float floorY, float unitsPerMetre,
        uint64_t* outToken) {
    if (outToken) *outToken = 0u;
    if (!outToken || placements.empty() || unitsPerMetre <= 0.0f) return false;

    // Priming and snapshot construction are serialized by Q19.3's scheduler.
    // Guard against a context mismatch rather than rebuilding transformed data.
    const bool sameContext = gQ1990CollisionCacheContextValid &&
        std::fabs(gQ1990CollisionCacheCenterX - centerX) < 0.01f &&
        std::fabs(gQ1990CollisionCacheCenterY - centerY) < 0.01f &&
        std::fabs(gQ1990CollisionCacheFloorZ - floorZ) < 0.01f &&
        std::fabs(gQ1990CollisionCacheSceneForward - sceneForward) < 0.0001f &&
        std::fabs(gQ1990CollisionCacheFloorY - floorY) < 0.0001f &&
        std::fabs(gQ1990CollisionCacheUnitsPerMetre - unitsPerMetre) < 0.0001f;
    if (!sameContext) {
        Q6G_LOGW("Q19.3 COLLISION SNAPSHOT FAILED: reason=cache-context-mismatch");
        return false;
    }

    const auto totalStarted = std::chrono::steady_clock::now();
    auto snapshot = std::make_shared<Q1930PreparedCollisionSnapshot>();
    snapshot->floorY = floorY;

    auto phaseStarted = std::chrono::steady_clock::now();
    if (!Q1930AssembleCachedExterior(placements, *snapshot)) return false;
    snapshot->assemblyUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - phaseStarted).count());

    phaseStarted = std::chrono::steady_clock::now();
    Q1930BuildShapes(*snapshot);
    snapshot->shapesUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - phaseStarted).count());

    phaseStarted = std::chrono::steady_clock::now();
    Q1930BuildWeldAdjacency(*snapshot);
    snapshot->weldUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - phaseStarted).count());

    phaseStarted = std::chrono::steady_clock::now();
    Q1930BuildBroadphase(*snapshot);
    snapshot->broadphaseUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - phaseStarted).count());

    snapshot->totalUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - totalStarted).count());

    uint64_t token = 0u;
    {
        std::lock_guard<std::mutex> lock(gQ1930SnapshotMutex);
        token = ++gQ1930SnapshotSerial;
        gQ1930PreparedSnapshots[token] = snapshot;
    }
    *outToken = token;

    Q6G_LOGI("Q19.3 COLLISION SNAPSHOT PREPARED: token=%llu placements=%zu triangles=%zu shapes=%zu weldEdges=%zu objects=%zu gridCells=%zu assemblyUs=%llu shapesUs=%llu weldUs=%llu broadphaseUs=%llu totalUs=%llu thread=serialized-asset-worker",
             static_cast<unsigned long long>(token),
             snapshot->placementCount, snapshot->triangles.size(),
             snapshot->shapes.size(), snapshot->sharedEdges,
             snapshot->q950Objects.size(), snapshot->q950Grid.size(),
             static_cast<unsigned long long>(snapshot->assemblyUs),
             static_cast<unsigned long long>(snapshot->shapesUs),
             static_cast<unsigned long long>(snapshot->weldUs),
             static_cast<unsigned long long>(snapshot->broadphaseUs),
             static_cast<unsigned long long>(snapshot->totalUs));
    return true;
}

bool PublishFo3CollisionSnapshotQ1930(uint64_t token, uint64_t* outSwapUs) {
    if (outSwapUs) *outSwapUs = 0u;
    std::shared_ptr<Q1930PreparedCollisionSnapshot> snapshot;
    {
        std::lock_guard<std::mutex> lock(gQ1930SnapshotMutex);
        const auto found = gQ1930PreparedSnapshots.find(token);
        if (found == gQ1930PreparedSnapshots.end()) return false;
        snapshot = std::move(found->second);
        gQ1930PreparedSnapshots.erase(found);
    }
    if (!snapshot || snapshot->triangles.empty()) return false;

    const auto started = std::chrono::steady_clock::now();

    // Debug collision GL is disabled in normal Quest builds. If enabled later,
    // rebuild its visual overlay separately; locomotion data below is complete.
    gWorldTriangles = std::move(snapshot->triangles);
    gSurfaceSourcesQ722 = std::move(snapshot->surfaceSources);
    gPlacementCount = snapshot->placementCount;
    gCollisionShapeCount = snapshot->collisionShapeCount;
    gTriangleCount = snapshot->triangleCount;
    gKindCounts = snapshot->kindCounts;
    gCollisionFloorY = snapshot->floorY;
    gExteriorAllBhksQ78A = true;
    gPlayerCollisionReady = true;

    gHkWeldNeighboursQ801 = std::move(snapshot->weldNeighbours);
    gHkWeldAdjacencyReadyQ801 = true;
    gHkWeldTriangleCountQ801 = gWorldTriangles.size();
    gHkWeldTriangleDataQ801 =
        gWorldTriangles.empty() ? nullptr : gWorldTriangles.data();

    gHkShapesQ900 = std::move(snapshot->shapes);
    gHkShapeIndexQ900 = std::move(snapshot->shapeIndex);
    gHkShapesReadyQ900 = true;
    gHkShapeTriangleCountQ900 = gWorldTriangles.size();
    gHkShapeTriangleDataQ900 =
        gWorldTriangles.empty() ? nullptr : gWorldTriangles.data();

    gQ950Objects = std::move(snapshot->q950Objects);
    gQ950TriangleObject = std::move(snapshot->q950TriangleObject);
    gQ950Grid = std::move(snapshot->q950Grid);
    gQ950LargeObjects = std::move(snapshot->q950LargeObjects);
    gQ950LargeLegacy = std::move(snapshot->q950LargeLegacy);
    gQ950ObjectStamp = std::move(snapshot->q950ObjectStamp);
    gQ950TriangleStamp = std::move(snapshot->q950TriangleStamp);
    gQ950QuerySerial = 1u;
    gQ950Data = gWorldTriangles.empty() ? nullptr : gWorldTriangles.data();
    gQ950TriangleCount = gWorldTriangles.size();
    gQ950Ready = true;

    // A rolling exterior snapshot is not a teleport. Preserve the player's
    // already-resolved standing state, but discard contact manifold indices that
    // referred to the previous triangle vector.
    gHkManifoldValidQ800 = false;

    const uint64_t swapUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count());
    if (outSwapUs) *outSwapUs = swapUs;

    Q6G_LOGI("Q19.3 COLLISION SNAPSHOT PUBLISHED: token=%llu triangles=%zu shapes=%zu objects=%zu gridCells=%zu swapUs=%llu derivedReadyAtPublish=1 lazyRebuild=0",
             static_cast<unsigned long long>(token),
             gWorldTriangles.size(), gHkShapesQ900.size(),
             gQ950Objects.size(), gQ950Grid.size(),
             static_cast<unsigned long long>(swapUs));
    return true;
}

void DiscardFo3CollisionSnapshotQ1930(uint64_t token) {
    if (token == 0u) return;
    std::lock_guard<std::mutex> lock(gQ1930SnapshotMutex);
    gQ1930PreparedSnapshots.erase(token);
}

void InvalidateDerivedCollisionCachesQ17() {
    gHkWeldAdjacencyReadyQ801 = false;
    gHkShapesReadyQ900 = false;
    gQ950Ready = false;
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
    gSurfaceSourcesQ722.clear();
    gPlayerCollisionReady = false;
    gSafeSpawnResolved = false;
    gExteriorAllBhksQ78A = false;
    gResolveCounter = 0;
    gContactLogCount = 0;
    gTerrainGroundLogCountQ77 = 0;
    gStepUpLogCountQ78B = 0;
    gManifoldLogCountQ714 = 0;
}
