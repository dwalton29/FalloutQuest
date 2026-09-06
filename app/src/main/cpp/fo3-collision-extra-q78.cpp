// Q7.8a: exterior-only supplemental authored collision.
//
// The proven Q7.7 collision engine remains authoritative. Q7.8 widened its
// legacy path filter to Architecture\\Megaton, but some visible Megaton pieces
// (notably walkways/platform components) live under other model folders. This
// layer asks the real NIF bhk decoder instead of guessing from the path:
// if a placed model contains authored collision, it contributes here.
//
// The wrapper is dormant in MegatonPlayerHouse. It only applies supplemental
// collision after Q7.7 LAND grounding is active, preserving the known-good
// interior startup/spawn/movement path.

#ifdef InitializeFo3CollisionOverlay
#undef InitializeFo3CollisionOverlay
#endif
#ifdef ResolveFo3PlayerMotionQ6G
#undef ResolveFo3PlayerMotionQ6G
#endif

#include "fo3-collision-overlay.h"
#include "fo3-nif-collision-q6f.h"
#include "fo3-terrain-q76.h"

#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char* TAG_Q78 = "FalloutQuest";
constexpr size_t MAX_EXTRA_PLACEMENTS_Q78 = 1024u;
constexpr size_t MAX_EXTRA_TRIANGLES_Q78 = 250000u;
constexpr float PLAYER_RADIUS_Q78 = 0.26f;
constexpr float PLAYER_HEIGHT_Q78 = 1.70f;
constexpr float PLAYER_SKIN_Q78 = 0.003f;
constexpr float MAX_STEP_UP_Q78 = 0.32f;
constexpr float MAX_GROUND_DROP_Q78 = 0.80f;
constexpr int MAX_DEPENETRATION_PASSES_Q78 = 6;

#define Q78_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG_Q78, __VA_ARGS__)
#define Q78_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG_Q78, __VA_ARGS__)

struct Vec3Q78 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec2Q78 {
    float x = 0.0f;
    float y = 0.0f;
};

struct TriangleQ78 {
    Vec3Q78 a, b, c;
    Vec3Q78 normal;
    float minX = 0.0f, maxX = 0.0f;
    float minY = 0.0f, maxY = 0.0f;
    float minZ = 0.0f, maxZ = 0.0f;
};

std::vector<TriangleQ78> gExtraTrianglesQ78;
float gFloorYQ78 = -1.55f;
bool gExtraReadyQ78 = false;
uint64_t gExtraContactLogsQ78 = 0u;
uint64_t gExtraGroundLogsQ78 = 0u;

std::string LowerPathQ78(const std::string& path) {
    std::string lower = path;
    for (char& ch : lower) {
        if (ch == '/') ch = '\\';
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return lower;
}

bool IsExteriorMegatonSetQ78(const std::vector<Fo3WorldPlacement>& placements) {
    for (const Fo3WorldPlacement& placement : placements) {
        const std::string lower = LowerPathQ78(placement.modelPath);
        if (lower.find("architecture\\megaton") != std::string::npos &&
            lower.find("architecture\\megaton\\interior\\shackinteriors") == std::string::npos) {
            return true;
        }
    }
    return false;
}

bool CoveredByBaseQ78(const std::string& path) {
    return LowerPathQ78(path).find("architecture\\megaton") != std::string::npos;
}

Vec3Q78 RotateXQ78(Vec3Q78 v, float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    return {v.x, c * v.y - s * v.z, s * v.y + c * v.z};
}

Vec3Q78 RotateYQ78(Vec3Q78 v, float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    return {c * v.x + s * v.z, v.y, -s * v.x + c * v.z};
}

Vec3Q78 RotateZQ78(Vec3Q78 v, float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    return {c * v.x - s * v.y, s * v.x + c * v.y, v.z};
}

Vec3Q78 ApplyPlacementQ78(Vec3Q78 v, const Fo3WorldPlacement& p) {
    v.x *= p.scale;
    v.y *= p.scale;
    v.z *= p.scale;
    v = RotateXQ78(v, p.rx);
    v = RotateYQ78(v, p.ry);
    v = RotateZQ78(v, p.rz);
    v.x += p.x;
    v.y += p.y;
    v.z += p.z;
    return v;
}

Vec3Q78 ToVrQ78(Vec3Q78 game,
                 float centerX, float centerY, float floorZ,
                 float sceneForward, float floorY, float unitsPerMetre) {
    return {
        (game.x - centerX) / unitsPerMetre,
        floorY + (game.z - floorZ) / unitsPerMetre,
        sceneForward - (game.y - centerY) / unitsPerMetre,
    };
}

Vec3Q78 SubQ78(Vec3Q78 a, Vec3Q78 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3Q78 CrossQ78(Vec3Q78 a, Vec3Q78 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float LengthQ78(Vec3Q78 v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

bool BuildTriangleQ78(const Vec3Q78& a, const Vec3Q78& b, const Vec3Q78& c,
                      TriangleQ78& out) {
    Vec3Q78 n = CrossQ78(SubQ78(b, a), SubQ78(c, a));
    const float len = LengthQ78(n);
    if (!(len > 1e-7f) || !std::isfinite(len)) return false;
    n.x /= len; n.y /= len; n.z /= len;
    out.a = a; out.b = b; out.c = c; out.normal = n;
    out.minX = std::min({a.x, b.x, c.x});
    out.maxX = std::max({a.x, b.x, c.x});
    out.minY = std::min({a.y, b.y, c.y});
    out.maxY = std::max({a.y, b.y, c.y});
    out.minZ = std::min({a.z, b.z, c.z});
    out.maxZ = std::max({a.z, b.z, c.z});
    return true;
}

bool BarycentricXZQ78(const TriangleQ78& tri, float x, float z,
                      float& u, float& v, float& w) {
    const float x0 = tri.a.x, z0 = tri.a.z;
    const float x1 = tri.b.x, z1 = tri.b.z;
    const float x2 = tri.c.x, z2 = tri.c.z;
    const float denom = (z1 - z2) * (x0 - x2) + (x2 - x1) * (z0 - z2);
    if (std::fabs(denom) < 1e-8f) return false;
    u = ((z1 - z2) * (x - x2) + (x2 - x1) * (z - z2)) / denom;
    v = ((z2 - z0) * (x - x2) + (x0 - x2) * (z - z2)) / denom;
    w = 1.0f - u - v;
    constexpr float eps = -0.0005f;
    return u >= eps && v >= eps && w >= eps;
}

Vec2Q78 ClosestSegmentQ78(Vec2Q78 p, Vec2Q78 a, Vec2Q78 b) {
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float denom = abx * abx + aby * aby;
    if (denom <= 1e-10f) return a;
    float t = ((p.x - a.x) * abx + (p.y - a.y) * aby) / denom;
    t = std::clamp(t, 0.0f, 1.0f);
    return {a.x + abx * t, a.y + aby * t};
}

float Dist2Q78(Vec2Q78 a, Vec2Q78 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

Vec2Q78 ClosestTriangleXZQ78(const TriangleQ78& tri, Vec2Q78 p) {
    float u = 0.0f, v = 0.0f, w = 0.0f;
    if (BarycentricXZQ78(tri, p.x, p.y, u, v, w)) return p;
    const Vec2Q78 a{tri.a.x, tri.a.z};
    const Vec2Q78 b{tri.b.x, tri.b.z};
    const Vec2Q78 c{tri.c.x, tri.c.z};
    const Vec2Q78 qab = ClosestSegmentQ78(p, a, b);
    const Vec2Q78 qbc = ClosestSegmentQ78(p, b, c);
    const Vec2Q78 qca = ClosestSegmentQ78(p, c, a);
    const float dab = Dist2Q78(p, qab);
    const float dbc = Dist2Q78(p, qbc);
    const float dca = Dist2Q78(p, qca);
    if (dab <= dbc && dab <= dca) return qab;
    if (dbc <= dca) return qbc;
    return qca;
}

uint32_t ResolveExtraWallsQ78(float& x, float& z, float feetY) {
    uint32_t contacts = 0u;
    const float topY = feetY + PLAYER_HEIGHT_Q78;
    const float radius2 = PLAYER_RADIUS_Q78 * PLAYER_RADIUS_Q78;
    for (int pass = 0; pass < MAX_DEPENETRATION_PASSES_Q78; ++pass) {
        bool changed = false;
        for (const TriangleQ78& tri : gExtraTrianglesQ78) {
            if (std::fabs(tri.normal.y) >= 0.75f) continue;
            if (tri.maxY < feetY + 0.04f || tri.minY > topY) continue;
            if (x < tri.minX - PLAYER_RADIUS_Q78 || x > tri.maxX + PLAYER_RADIUS_Q78 ||
                z < tri.minZ - PLAYER_RADIUS_Q78 || z > tri.maxZ + PLAYER_RADIUS_Q78) continue;

            const Vec2Q78 p{x, z};
            const Vec2Q78 q = ClosestTriangleXZQ78(tri, p);
            float dx = x - q.x;
            float dz = z - q.y;
            const float d2 = dx * dx + dz * dz;
            if (d2 >= radius2) continue;

            float distance = std::sqrt(std::max(d2, 0.0f));
            if (distance > 1e-5f) {
                dx /= distance;
                dz /= distance;
            } else {
                dx = tri.normal.x;
                dz = tri.normal.z;
                float horizontal = std::sqrt(dx * dx + dz * dz);
                if (horizontal < 1e-5f) continue;
                dx /= horizontal;
                dz /= horizontal;
                const float cx = (tri.a.x + tri.b.x + tri.c.x) / 3.0f;
                const float cz = (tri.a.z + tri.b.z + tri.c.z) / 3.0f;
                if (dx * (x - cx) + dz * (z - cz) < 0.0f) {
                    dx = -dx;
                    dz = -dz;
                }
                distance = 0.0f;
            }
            const float push = (PLAYER_RADIUS_Q78 - distance) + PLAYER_SKIN_Q78;
            x += dx * push;
            z += dz * push;
            ++contacts;
            changed = true;
        }
        if (!changed) break;
    }
    return contacts;
}

bool FindExtraGroundQ78(float x, float z, float referenceFeetY, float& outGroundY) {
    bool found = false;
    float best = -1e30f;
    for (const TriangleQ78& tri : gExtraTrianglesQ78) {
        if (std::fabs(tri.normal.y) < 0.55f) continue;
        if (x < tri.minX - 0.02f || x > tri.maxX + 0.02f ||
            z < tri.minZ - 0.02f || z > tri.maxZ + 0.02f) continue;
        float u = 0.0f, v = 0.0f, w = 0.0f;
        if (!BarycentricXZQ78(tri, x, z, u, v, w)) continue;
        const float y = tri.a.y * u + tri.b.y * v + tri.c.y * w;
        if (y > referenceFeetY + MAX_STEP_UP_Q78 ||
            y < referenceFeetY - MAX_GROUND_DROP_Q78) continue;
        if (!found || y > best) {
            best = y;
            found = true;
        }
    }
    if (found) outGroundY = best;
    return found;
}

} // namespace

bool InitializeFo3CollisionOverlayQ78(const std::vector<Fo3WorldPlacement>& placements,
                                      float centerX, float centerY, float floorZ,
                                      float sceneForward, float floorY,
                                      float unitsPerMetre) {
    // Always preserve the proven Q7.7/Q7.8 base collision first.
    const bool baseReady = InitializeFo3CollisionOverlay(placements,
                                                         centerX, centerY, floorZ,
                                                         sceneForward, floorY,
                                                         unitsPerMetre);

    gExtraTrianglesQ78.clear();
    gExtraReadyQ78 = false;
    gFloorYQ78 = floorY;
    gExtraContactLogsQ78 = 0u;
    gExtraGroundLogsQ78 = 0u;

    // Do not even parse supplemental models in the player house. The exterior
    // placement set is recognizable by real Megaton architecture that is not
    // the ShackInteriors kit.
    if (!IsExteriorMegatonSetQ78(placements) || unitsPerMetre <= 1e-4f) {
        Q78_LOGI("Q7.8A EXTRA COLLISION DORMANT: placements=%zu exteriorSet=0 housePathUnchanged=1 baseReady=%d",
                 placements.size(), baseReady ? 1 : 0);
        return baseReady;
    }

    std::unordered_set<uint32_t> seenRefs;
    std::unordered_map<std::string, std::vector<Fo3NifCollisionShapeQ6F>> modelCache;
    std::unordered_set<std::string> noCollisionModels;
    size_t successfulPlacements = 0u;
    size_t cacheHits = 0u;
    size_t negativeCacheHits = 0u;
    size_t modelMisses = 0u;
    bool capped = false;

    gExtraTrianglesQ78.reserve(16384u);

    for (const Fo3WorldPlacement& placement : placements) {
        if (successfulPlacements >= MAX_EXTRA_PLACEMENTS_Q78 ||
            gExtraTrianglesQ78.size() >= MAX_EXTRA_TRIANGLES_Q78) {
            capped = true;
            break;
        }
        if (placement.modelPath.empty() || CoveredByBaseQ78(placement.modelPath)) continue;
        if (!seenRefs.insert(placement.refFormId).second) continue;

        if (noCollisionModels.find(placement.modelPath) != noCollisionModels.end()) {
            ++negativeCacheHits;
            continue;
        }

        auto cached = modelCache.find(placement.modelPath);
        if (cached == modelCache.end()) {
            std::vector<Fo3NifCollisionShapeQ6F> shapes;
            if (!LoadFo3NifCollisionShapesQ6F(placement.modelPath, shapes) || shapes.empty()) {
                noCollisionModels.insert(placement.modelPath);
                ++modelMisses;
                continue;
            }
            cached = modelCache.emplace(placement.modelPath, std::move(shapes)).first;
        } else {
            ++cacheHits;
        }

        size_t placementTriangles = 0u;
        for (const Fo3NifCollisionShapeQ6F& shape : cached->second) {
            const size_t vertexCount = shape.positions.size() / 3u;
            if (vertexCount == 0u || shape.indices.size() < 3u) continue;
            for (size_t i = 0; i + 2u < shape.indices.size(); i += 3u) {
                if (gExtraTrianglesQ78.size() >= MAX_EXTRA_TRIANGLES_Q78) {
                    capped = true;
                    break;
                }
                const uint32_t ia = shape.indices[i];
                const uint32_t ib = shape.indices[i + 1u];
                const uint32_t ic = shape.indices[i + 2u];
                if (ia >= vertexCount || ib >= vertexCount || ic >= vertexCount) continue;

                auto point = [&](uint32_t index) {
                    Vec3Q78 p{
                        shape.positions[index * 3u],
                        shape.positions[index * 3u + 1u],
                        shape.positions[index * 3u + 2u],
                    };
                    return ToVrQ78(ApplyPlacementQ78(p, placement),
                                   centerX, centerY, floorZ,
                                   sceneForward, floorY, unitsPerMetre);
                };

                TriangleQ78 tri;
                if (!BuildTriangleQ78(point(ia), point(ib), point(ic), tri)) continue;
                gExtraTrianglesQ78.push_back(tri);
                ++placementTriangles;
            }
            if (capped) break;
        }
        if (placementTriangles > 0u) ++successfulPlacements;
        if (capped) break;
    }

    gExtraReadyQ78 = !gExtraTrianglesQ78.empty();
    Q78_LOGI("Q7.8A EXTRA COLLISION READY: active=%d source=all-authored-bhk pathFilter=NONE placements=%zu successful=%zu triangles=%zu uniqueCollisionModels=%zu noCollisionModels=%zu cacheHits=%zu negativeCacheHits=%zu misses=%zu capped=%d baseReady=%d",
             gExtraReadyQ78 ? 1 : 0, placements.size(), successfulPlacements,
             gExtraTrianglesQ78.size(), modelCache.size(), noCollisionModels.size(),
             cacheHits, negativeCacheHits, modelMisses, capped ? 1 : 0,
             baseReady ? 1 : 0);
    return baseReady || gExtraReadyQ78;
}

bool ResolveFo3PlayerMotionQ78(float currentX, float currentZ,
                               float desiredX, float desiredZ,
                               float currentPlayerYOffset,
                               float* outX, float* outZ,
                               float* outPlayerYOffset) {
    if (!outX || !outZ || !outPlayerYOffset) return false;

    float baseX = currentX;
    float baseZ = currentZ;
    float basePlayerY = currentPlayerYOffset;
    const bool baseResolved = ResolveFo3PlayerMotionQ6G(currentX, currentZ,
                                                        desiredX, desiredZ,
                                                        currentPlayerYOffset,
                                                        &baseX, &baseZ, &basePlayerY);
    if (!baseResolved) return false;

    // Supplemental collision is strictly exterior-only. LAND activation is the
    // authoritative signal that the Q7.5 exterior swap completed.
    if (!gExtraReadyQ78 || !IsFo3TerrainGroundingActiveQ77()) {
        *outX = baseX;
        *outZ = baseZ;
        *outPlayerYOffset = basePlayerY;
        return true;
    }

    const float referenceFeetY = gFloorYQ78 + currentPlayerYOffset;
    float x = baseX;
    float z = baseZ;
    const uint32_t contacts = ResolveExtraWallsQ78(x, z, referenceFeetY);

    float extraGroundY = referenceFeetY;
    const bool extraGrounded = FindExtraGroundQ78(x, z, referenceFeetY, extraGroundY);
    if (extraGrounded) {
        // This intentionally uses the previous frame's feet as the step/drop
        // reference, not basePlayerY. The base Q7.7 resolver may have selected
        // LAND below an elevated walkway; using current feet prevents that
        // temporary LAND result from pulling the player off the platform.
        basePlayerY = extraGroundY - gFloorYQ78;
        if (gExtraGroundLogsQ78 < 12u) {
            ++gExtraGroundLogsQ78;
            Q78_LOGI("Q7.8A EXTRA GROUND: pos=(%.3f %.3f) groundY=%.3f playerY=%.3f source=authored-bhk",
                     x, z, extraGroundY, basePlayerY);
        }
    }

    *outX = x;
    *outZ = z;
    *outPlayerYOffset = basePlayerY;

    if (contacts > 0u && gExtraContactLogsQ78 < 24u) {
        ++gExtraContactLogsQ78;
        Q78_LOGI("Q7.8A EXTRA CONTACT: desired=(%.3f %.3f) base=(%.3f %.3f) resolved=(%.3f %.3f) contacts=%u source=authored-bhk",
                 desiredX, desiredZ, baseX, baseZ, x, z, contacts);
    }
    return true;
}
