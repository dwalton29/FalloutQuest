# Q8.0: clean-room Fallout 3 / Havok-style character proxy.
#
# This does NOT contain Havok SDK code. It reproduces the observable architecture
# of the contemporaneous hkpCharacterProxy using FalloutQuest's own collision
# queries: shape/capsule linear casts, a persistent point/plane manifold,
# separate support queries, an up-forward-down step attempt and a small convex
# velocity-constraint (simplex-style) solve.
#
# Q7.25's model-name rules remain disabled. Q8.0 traversal is geometry-only.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q725.inc"
     Q800_CONTROLLER_SOURCE)
string(REPLACE "Q7.25" "Q8.0"
       Q800_CONTROLLER_SOURCE "${Q800_CONTROLLER_SOURCE}")

set(Q800_HAVOK_PROXY_SOURCE [=[
// Clean-room behavioural equivalents of the public hkpCharacterProxy concepts.
// Values are conservative FalloutQuest tuning values; they are not claimed to
// be Bethesda/Havok private defaults.
constexpr float Q800_KEEP_DISTANCE = 0.008f;
constexpr float Q800_KEEP_CONTACT_TOLERANCE = 0.030f;
constexpr float Q800_PLANE_MATCH_COS = 0.985f;
constexpr float Q800_PLANE_POINT_TOL = 0.060f;
constexpr int Q800_MAX_CAST_ITERATIONS = 8;
constexpr int Q800_MAX_MANIFOLD_PLANES = 24;

struct HkProxyContactQ800 {
    size_t triangleIndex = 0u;
    uint64_t surfaceKey = 0u;
    float nx = 0.0f;
    float ny = 0.0f;
    float nz = 0.0f;
    float pointX = 0.0f;
    float pointY = 0.0f;
    float pointZ = 0.0f;
    float signedDistance = 0.0f; // distance from capsule skin; negative = penetration
    float push = 0.0f;
    float triangleMinY = 0.0f;
    float triangleMaxY = 0.0f;
    uint32_t material = 0u;
    bool stairs = false;
    bool platform = false;
};

struct HkSupportQ800 {
    bool found = false;
    float y = 0.0f;
    float normalY = 0.0f;
    size_t triangleIndex = 0u;
};

std::vector<HkProxyContactQ800> gHkManifoldQ800;
float gHkManifoldXQ800 = 0.0f;
float gHkManifoldZQ800 = 0.0f;
bool gHkManifoldValidQ800 = false;
uint64_t gHkCastLogsQ800 = 0u;
uint64_t gHkStepLogsQ800 = 0u;
uint64_t gHkSlideLogsQ800 = 0u;
uint64_t gHkBlockedLogsQ800 = 0u;
uint64_t gHkManifoldLogsQ800 = 0u;

float HkDot3Q800(float ax, float ay, float az, float bx, float by, float bz) {
    return ax * bx + ay * by + az * bz;
}

float HkPointDistSqQ800(const HkProxyContactQ800& a, const HkProxyContactQ800& b) {
    const float dx = a.pointX - b.pointX;
    const float dy = a.pointY - b.pointY;
    const float dz = a.pointZ - b.pointZ;
    return dx * dx + dy * dy + dz * dz;
}

bool HkSamePlaneQ800(const HkProxyContactQ800& a, const HkProxyContactQ800& b) {
    if (a.surfaceKey != b.surfaceKey) return false;
    const float nd = HkDot3Q800(a.nx, a.ny, a.nz, b.nx, b.ny, b.nz);
    if (nd < Q800_PLANE_MATCH_COS) return false;
    return HkPointDistSqQ800(a, b) <= Q800_PLANE_POINT_TOL * Q800_PLANE_POINT_TOL;
}

bool HkContactIsWalkableQ800(const HkProxyContactQ800& contact) {
    if (contact.triangleIndex >= gWorldTriangles.size()) return false;
    const CollisionTriangle& tri = gWorldTriangles[contact.triangleIndex];
    return std::fabs(tri.normal.y) >= WalkableNormalThresholdQ714(tri);
}

void GatherHkProxyContactsQ800(float x, float z, float feetY, float tolerance,
                               std::vector<HkProxyContactQ800>& out,
                               size_t* outRawTriangles = nullptr) {
    out.clear();
    if (outRawTriangles) *outRawTriangles = 0u;

    const float radius = CollisionRadiusQ712();
    const float queryRadius = radius + std::max(0.0f, tolerance);
    const float queryRadius2 = queryRadius * queryRadius;
    const float lowerY = feetY + radius;
    const float upperY = feetY + PLAYER_HEIGHT - radius;
    const Vec3 segA{x, lowerY, z};
    const Vec3 segB{x, upperY, z};

    for (size_t triIndex = 0u; triIndex < gWorldTriangles.size(); ++triIndex) {
        const CollisionTriangle& tri = gWorldTriangles[triIndex];
        if (tri.maxY < feetY - queryRadius ||
            tri.minY > feetY + PLAYER_HEIGHT + queryRadius) continue;
        if (x < tri.minX - queryRadius || x > tri.maxX + queryRadius ||
            z < tri.minZ - queryRadius || z > tri.maxZ + queryRadius) continue;

        Vec3 capsulePoint;
        Vec3 trianglePoint;
        const float d2 = ClosestCapsuleSegmentTriangleQ719(segA, segB, tri,
                                                            capsulePoint, trianglePoint);
        if (!std::isfinite(d2) || d2 > queryRadius2) continue;
        if (outRawTriangles) ++(*outRawTriangles);

        const float distance = std::sqrt(std::max(0.0f, d2));
        Vec3 separation = Q719Sub(capsulePoint, trianglePoint);
        float sepLen = std::sqrt(Q719LengthSq(separation));
        float nx = 0.0f, ny = 0.0f, nz = 0.0f;
        if (sepLen > 1e-5f) {
            nx = separation.x / sepLen;
            ny = separation.y / sepLen;
            nz = separation.z / sepLen;
        } else {
            nx = tri.normal.x;
            ny = tri.normal.y;
            nz = tri.normal.z;
            const float centroidX = (tri.a.x + tri.b.x + tri.c.x) / 3.0f;
            const float centroidY = (tri.a.y + tri.b.y + tri.c.y) / 3.0f;
            const float centroidZ = (tri.a.z + tri.b.z + tri.c.z) / 3.0f;
            const float toward = nx * (x - centroidX) +
                                 ny * ((feetY + PLAYER_HEIGHT * 0.5f) - centroidY) +
                                 nz * (z - centroidZ);
            if (toward < 0.0f) { nx = -nx; ny = -ny; nz = -nz; }
        }

        HkProxyContactQ800 candidate;
        candidate.triangleIndex = triIndex;
        candidate.surfaceKey = tri.surfaceKeyQ714;
        candidate.nx = nx;
        candidate.ny = ny;
        candidate.nz = nz;
        candidate.pointX = trianglePoint.x;
        candidate.pointY = trianglePoint.y;
        candidate.pointZ = trianglePoint.z;
        candidate.signedDistance = distance - radius;
        candidate.push = std::max(0.0f, Q800_KEEP_DISTANCE - candidate.signedDistance);
        candidate.triangleMinY = tri.minY;
        candidate.triangleMaxY = tri.maxY;
        candidate.material = tri.havokMaterialQ714;
        candidate.stairs = tri.stairsQ714;
        candidate.platform = tri.platformQ714;

        // Havok manifolds contain contact points/planes, not one entry per model.
        // Only merge genuinely coincident points on effectively the same plane.
        bool merged = false;
        for (HkProxyContactQ800& existing : out) {
            if (!HkSamePlaneQ800(existing, candidate)) continue;
            if (candidate.signedDistance < existing.signedDistance) existing = candidate;
            else {
                existing.stairs = existing.stairs || candidate.stairs;
                existing.platform = existing.platform || candidate.platform;
            }
            merged = true;
            break;
        }
        if (!merged) out.push_back(candidate);
    }

    std::sort(out.begin(), out.end(), [](const HkProxyContactQ800& a,
                                         const HkProxyContactQ800& b) {
        return a.signedDistance < b.signedDistance;
    });
    if (out.size() > Q800_MAX_MANIFOLD_PLANES) out.resize(Q800_MAX_MANIFOLD_PLANES);
}

void RefreshHkManifoldQ800(float x, float z, float feetY) {
    if (gHkManifoldValidQ800) {
        const float dx = x - gHkManifoldXQ800;
        const float dz = z - gHkManifoldZQ800;
        if (dx * dx + dz * dz > 1.0f) gHkManifoldQ800.clear();
    }

    std::vector<HkProxyContactQ800> fresh;
    GatherHkProxyContactsQ800(x, z, feetY, Q800_KEEP_CONTACT_TOLERANCE, fresh);

    // Preserve plane identity/normals when a fresh contact corresponds to the
    // previous manifold. New and removed contacts otherwise naturally churn.
    for (HkProxyContactQ800& now : fresh) {
        for (const HkProxyContactQ800& old : gHkManifoldQ800) {
            if (!HkSamePlaneQ800(now, old)) continue;
            // Keep the current point/distance, but smooth the normal very lightly
            // to avoid triangle-edge normal flicker from frame to frame.
            float nx = now.nx * 0.75f + old.nx * 0.25f;
            float ny = now.ny * 0.75f + old.ny * 0.25f;
            float nz = now.nz * 0.75f + old.nz * 0.25f;
            const float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (nl > 1e-5f) { now.nx = nx / nl; now.ny = ny / nl; now.nz = nz / nl; }
            break;
        }
    }

    gHkManifoldQ800.swap(fresh);
    gHkManifoldXQ800 = x;
    gHkManifoldZQ800 = z;
    gHkManifoldValidQ800 = true;

    if (gHkManifoldLogsQ800 < 40u || (gResolveCounter % 360u) == 0u) {
        ++gHkManifoldLogsQ800;
        Q6G_LOGI("Q8.0 HAVOK MANIFOLD: pos=(%.3f %.3f) planes=%zu keepDistance=%.3f keepTolerance=%.3f",
                 x, z, gHkManifoldQ800.size(), Q800_KEEP_DISTANCE,
                 Q800_KEEP_CONTACT_TOLERANCE);
    }
}

const HkProxyContactQ800* FindHkPlaneQ800(const std::vector<HkProxyContactQ800>& contacts,
                                          const HkProxyContactQ800& needle) {
    for (const HkProxyContactQ800& contact : contacts) {
        if (HkSamePlaneQ800(contact, needle)) return &contact;
    }
    return nullptr;
}

bool HkPositionAllowedQ800(float targetX, float targetZ, float feetY,
                           float moveX, float moveZ,
                           const std::vector<HkProxyContactQ800>& baseline,
                           std::vector<HkProxyContactQ800>& blockers,
                           size_t* outRawTriangles = nullptr) {
    std::vector<HkProxyContactQ800> target;
    GatherHkProxyContactsQ800(targetX, targetZ, feetY, Q800_KEEP_DISTANCE,
                              target, outRawTriangles);
    blockers.clear();

    for (const HkProxyContactQ800& contact : target) {
        if (HkContactIsWalkableQ800(contact)) continue;
        const float hLen = std::sqrt(contact.nx * contact.nx + contact.nz * contact.nz);
        if (hLen < 1e-5f) continue;
        const float hx = contact.nx / hLen;
        const float hz = contact.nz / hLen;
        const float into = moveX * hx + moveZ * hz;
        if (into >= -Q718_DIRECTION_EPS) continue;

        const HkProxyContactQ800* old = FindHkPlaneQ800(baseline, contact);
        if (old && contact.signedDistance >= old->signedDistance - 0.0015f) continue;
        blockers.push_back(contact);
    }
    return blockers.empty();
}

bool LinearCastHkProxyQ800(float startX, float startZ,
                           float targetX, float targetZ,
                           float feetY,
                           float& outX, float& outZ,
                           std::vector<HkProxyContactQ800>& outBlockers,
                           size_t* outRawTriangles = nullptr) {
    outX = startX;
    outZ = startZ;
    outBlockers.clear();
    if (outRawTriangles) *outRawTriangles = 0u;

    const float dx = targetX - startX;
    const float dz = targetZ - startZ;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance < 1e-7f) return true;

    std::vector<HkProxyContactQ800> baseline;
    GatherHkProxyContactsQ800(startX, startZ, feetY, Q800_KEEP_CONTACT_TOLERANCE,
                              baseline);

    // Broad samples locate a bracket; bisection then approximates the linear
    // cast time-of-impact instead of treating frame endpoints as collision.
    const float sampleStep = std::max(0.008f, CollisionRadiusQ712() * 0.06f);
    const int samples = std::clamp(static_cast<int>(std::ceil(distance / sampleStep)), 1, 24);
    float clearT = 0.0f;
    std::vector<HkProxyContactQ800> blockers;
    size_t raw = 0u;

    for (int i = 1; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float px = startX + dx * t;
        const float pz = startZ + dz * t;
        if (HkPositionAllowedQ800(px, pz, feetY, dx * t, dz * t,
                                  baseline, blockers, &raw)) {
            clearT = t;
            continue;
        }

        float lo = clearT;
        float hi = t;
        std::vector<HkProxyContactQ800> hiBlockers = blockers;
        for (int iteration = 0; iteration < Q800_MAX_CAST_ITERATIONS; ++iteration) {
            const float mid = (lo + hi) * 0.5f;
            const float mx = startX + dx * mid;
            const float mz = startZ + dz * mid;
            std::vector<HkProxyContactQ800> midBlockers;
            size_t midRaw = 0u;
            if (HkPositionAllowedQ800(mx, mz, feetY, dx * mid, dz * mid,
                                      baseline, midBlockers, &midRaw)) {
                lo = mid;
            } else {
                hi = mid;
                hiBlockers.swap(midBlockers);
                raw = midRaw;
            }
        }
        outX = startX + dx * lo;
        outZ = startZ + dz * lo;
        outBlockers = std::move(hiBlockers);
        if (outRawTriangles) *outRawTriangles = raw;
        return false;
    }

    outX = targetX;
    outZ = targetZ;
    return true;
}

HkSupportQ800 FindHkSupportAtQ800(float x, float z, float referenceFeetY,
                                  float maxUp, float maxDown) {
    HkSupportQ800 result;
    const float radius = CollisionRadiusQ712() * 0.62f;
    const float d = radius * 0.70710678f;
    const std::array<Vec2, 9> probes{{
        {0.0f, 0.0f},
        { radius, 0.0f}, {-radius, 0.0f}, {0.0f, radius}, {0.0f, -radius},
        { d, d}, { d,-d}, {-d, d}, {-d,-d}
    }};
    float bestDistance = std::numeric_limits<float>::max();

    for (size_t triIndex = 0u; triIndex < gWorldTriangles.size(); ++triIndex) {
        const CollisionTriangle& tri = gWorldTriangles[triIndex];
        if (std::fabs(tri.normal.y) < WalkableNormalThresholdQ714(tri)) continue;
        for (const Vec2& probe : probes) {
            const float px = x + probe.x;
            const float pz = z + probe.y;
            if (px < tri.minX - 0.01f || px > tri.maxX + 0.01f ||
                pz < tri.minZ - 0.01f || pz > tri.maxZ + 0.01f) continue;
            float u = 0.0f, v = 0.0f, w = 0.0f;
            if (!BarycentricXZ(tri, px, pz, u, v, w)) continue;
            const float y = tri.a.y * u + tri.b.y * v + tri.c.y * w;
            const float delta = y - referenceFeetY;
            if (delta > maxUp || delta < -maxDown) continue;
            const float absDelta = std::fabs(delta);
            if (!result.found || absDelta < bestDistance - 0.0005f ||
                (std::fabs(absDelta - bestDistance) <= 0.0005f && y > result.y)) {
                result.found = true;
                result.y = y;
                result.normalY = std::fabs(tri.normal.y);
                result.triangleIndex = triIndex;
                bestDistance = absDelta;
            }
        }
    }
    return result;
}

HkSupportQ800 FindHkStepSupportAheadQ800(float startX, float startZ,
                                         float targetX, float targetZ,
                                         float feetY) {
    HkSupportQ800 best;
    const float dx = targetX - startX;
    const float dz = targetZ - startZ;
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1e-6f) return best;
    const float nx = dx / len;
    const float nz = dz / len;
    const float r = CollisionRadiusQ712();
    const std::array<float, 5> ahead{{r * 0.55f, r * 0.80f, r * 1.05f,
                                      r * 1.30f, r * 1.55f}};
    for (float a : ahead) {
        HkSupportQ800 support = FindHkSupportAtQ800(targetX + nx * a,
                                                     targetZ + nz * a,
                                                     feetY,
                                                     StepHeightQ78B() + PLAYER_SKIN,
                                                     0.16f);
        if (!support.found) continue;
        return support;
    }
    return best;
}

bool HkRaisedBlockersAreOnlyLandingLipQ800(const std::vector<HkProxyContactQ800>& blockers,
                                           float landingY) {
    if (blockers.empty()) return true;
    // A step/mesh lip may remain in the inflated cast because the side face and
    // walkable top share an edge. A genuine wall continues well above the
    // landing support. This is geometry-based edge welding, never model-name based.
    const float lipCeiling = landingY + CollisionRadiusQ712() * 0.90f;
    for (const HkProxyContactQ800& blocker : blockers) {
        if (blocker.triangleMaxY > lipCeiling) return false;
    }
    return true;
}

bool TryHkStepQ800(float startX, float startZ,
                   float targetX, float targetZ,
                   float feetY,
                   const std::vector<HkProxyContactQ800>& blockers,
                   float& outFeetY) {
    if (blockers.empty()) return false;

    const HkSupportQ800 landing = FindHkStepSupportAheadQ800(startX, startZ,
                                                             targetX, targetZ,
                                                             feetY);
    if (!landing.found) return false;
    const float rise = landing.y - feetY;
    if (rise < -0.16f || rise > StepHeightQ78B() + PLAYER_SKIN) return false;

    const float raisedFeetY = feetY + StepHeightQ78B() + PLAYER_SKIN + Q800_KEEP_DISTANCE;
    if (HasOverheadBlock(startX, startZ, raisedFeetY)) return false;

    // Cast far enough forward for the capsule centre to actually cross the lip.
    const float dx = targetX - startX;
    const float dz = targetZ - startZ;
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1e-6f) return false;
    const float nx = dx / len;
    const float nz = dz / len;
    const float castAhead = CollisionRadiusQ712() * 1.10f;
    const float castTargetX = targetX + nx * castAhead;
    const float castTargetZ = targetZ + nz * castAhead;

    float castX = startX, castZ = startZ;
    std::vector<HkProxyContactQ800> raisedBlockers;
    size_t raisedRaw = 0u;
    const bool raisedClear = LinearCastHkProxyQ800(startX, startZ,
                                                   castTargetX, castTargetZ,
                                                   raisedFeetY,
                                                   castX, castZ,
                                                   raisedBlockers, &raisedRaw);
    if (!raisedClear && !HkRaisedBlockersAreOnlyLandingLipQ800(raisedBlockers, landing.y)) {
        return false;
    }
    if (HasOverheadBlock(targetX, targetZ, landing.y)) return false;

    // Final capsule position must not contain a hard wall. Low side faces below
    // the walkable landing plane are treated as welded/internal edge contacts.
    std::vector<HkProxyContactQ800> finalContacts;
    GatherHkProxyContactsQ800(targetX, targetZ, landing.y, Q800_KEEP_DISTANCE,
                              finalContacts);
    std::vector<HkProxyContactQ800> hardFinal;
    for (const HkProxyContactQ800& contact : finalContacts) {
        if (HkContactIsWalkableQ800(contact)) continue;
        if (contact.triangleMaxY <= landing.y + CollisionRadiusQ712() * 0.90f) continue;
        hardFinal.push_back(contact);
    }
    if (!hardFinal.empty()) return false;

    outFeetY = landing.y;
    if (gHkStepLogsQ800 < 120u || (gResolveCounter % 240u) == 0u) {
        ++gHkStepLogsQ800;
        Q6G_LOGI("Q8.0 HAVOK STEP: from=(%.3f %.3f) target=(%.3f %.3f) landingRise=%.3f supportTri=%zu raisedClear=%d raisedBlockers=%zu hardFinal=%zu",
                 startX, startZ, targetX, targetZ, rise, landing.triangleIndex,
                 raisedClear ? 1 : 0, raisedBlockers.size(), hardFinal.size());
    }
    return true;
}

Vec2 SolveHkSimplex2DQ800(Vec2 requested,
                          const std::vector<HkProxyContactQ800>& contacts,
                          size_t& usedPlanes) {
    struct Plane2Q800 { float x = 0.0f; float z = 0.0f; };
    std::vector<Plane2Q800> planes;
    planes.reserve(8u);
    for (const HkProxyContactQ800& contact : contacts) {
        if (HkContactIsWalkableQ800(contact)) continue;
        const float hl = std::sqrt(contact.nx * contact.nx + contact.nz * contact.nz);
        if (hl < 1e-5f) continue;
        const Plane2Q800 p{contact.nx / hl, contact.nz / hl};
        bool duplicate = false;
        for (const Plane2Q800& existing : planes) {
            if (existing.x * p.x + existing.z * p.z > Q800_PLANE_MATCH_COS) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) planes.push_back(p);
        if (planes.size() >= 8u) break;
    }
    usedPlanes = planes.size();

    auto feasible = [&](const Vec2& v) {
        for (const Plane2Q800& p : planes) {
            if (v.x * p.x + v.y * p.z < -0.00001f) return false;
        }
        return true;
    };

    if (feasible(requested)) return requested;

    Vec2 best{0.0f, 0.0f};
    float bestError = requested.x * requested.x + requested.y * requested.y;
    for (const Plane2Q800& p : planes) {
        const float into = requested.x * p.x + requested.y * p.z;
        Vec2 candidate{requested.x - p.x * into,
                       requested.y - p.z * into};
        if (!feasible(candidate)) continue;
        // Never allow the constraint solve to reverse the player's intention.
        if (candidate.x * requested.x + candidate.y * requested.y <= 0.0f) continue;
        const float ex = candidate.x - requested.x;
        const float ez = candidate.y - requested.y;
        const float error = ex * ex + ez * ez;
        if (error < bestError) { bestError = error; best = candidate; }
    }
    return best;
}

bool MoveExteriorSubstepQ800(float& x, float& z, float& feetY,
                             float moveX, float moveZ, bool& steppedThisFrame,
                             uint32_t& contactSurfaces) {
    if (moveX * moveX + moveZ * moveZ < 1e-10f) return true;

    RefreshHkManifoldQ800(x, z, feetY);
    const float startX = x;
    const float startZ = z;
    const float targetX = x + moveX;
    const float targetZ = z + moveZ;

    float castX = x, castZ = z;
    std::vector<HkProxyContactQ800> blockers;
    size_t raw = 0u;
    if (LinearCastHkProxyQ800(x, z, targetX, targetZ, feetY,
                              castX, castZ, blockers, &raw)) {
        x = targetX;
        z = targetZ;
        RefreshHkManifoldQ800(x, z, feetY);
        return true;
    }

    x = castX;
    z = castZ;
    contactSurfaces += static_cast<uint32_t>(blockers.size());

    float stepY = feetY;
    if (TryHkStepQ800(x, z, targetX, targetZ, feetY, blockers, stepY)) {
        x = targetX;
        z = targetZ;
        feetY = stepY;
        steppedThisFrame = true;
        RefreshHkManifoldQ800(x, z, feetY);
        return true;
    }

    // Build a surface-constraint set from the cast hit plus the persistent
    // manifold, then solve the closest feasible horizontal velocity.
    std::vector<HkProxyContactQ800> constraints = blockers;
    for (const HkProxyContactQ800& contact : gHkManifoldQ800) {
        bool duplicate = false;
        for (const HkProxyContactQ800& existing : constraints) {
            if (HkSamePlaneQ800(existing, contact)) { duplicate = true; break; }
        }
        if (!duplicate) constraints.push_back(contact);
    }

    for (int iteration = 0; iteration < 4; ++iteration) {
        const Vec2 remaining{targetX - x, targetZ - z};
        if (remaining.x * remaining.x + remaining.y * remaining.y < 0.00000025f) {
            RefreshHkManifoldQ800(x, z, feetY);
            return true;
        }

        size_t usedPlanes = 0u;
        const Vec2 solved = SolveHkSimplex2DQ800(remaining, constraints, usedPlanes);
        if (solved.x * solved.x + solved.y * solved.y < 0.00000025f) break;

        const float slideTargetX = x + solved.x;
        const float slideTargetZ = z + solved.y;
        float slideX = x, slideZ = z;
        std::vector<HkProxyContactQ800> slideBlockers;
        size_t slideRaw = 0u;
        if (LinearCastHkProxyQ800(x, z, slideTargetX, slideTargetZ, feetY,
                                  slideX, slideZ, slideBlockers, &slideRaw)) {
            if (gHkSlideLogsQ800 < 120u || (gResolveCounter % 240u) == 0u) {
                ++gHkSlideLogsQ800;
                Q6G_LOGI("Q8.0 HAVOK SIMPLEX: from=(%.3f %.3f) requested=(%.3f %.3f) solved=(%.3f %.3f) planes=%zu",
                         x, z, remaining.x, remaining.y, solved.x, solved.y, usedPlanes);
            }
            x = slideTargetX;
            z = slideTargetZ;
            RefreshHkManifoldQ800(x, z, feetY);
            return true;
        }

        x = slideX;
        z = slideZ;
        contactSurfaces += static_cast<uint32_t>(slideBlockers.size());

        float slideStepY = feetY;
        if (TryHkStepQ800(x, z, slideTargetX, slideTargetZ,
                          feetY, slideBlockers, slideStepY)) {
            x = slideTargetX;
            z = slideTargetZ;
            feetY = slideStepY;
            steppedThisFrame = true;
            RefreshHkManifoldQ800(x, z, feetY);
            return true;
        }

        constraints.swap(slideBlockers);
        raw = slideRaw;
    }

    if (gHkBlockedLogsQ800 < 120u || (gResolveCounter % 240u) == 0u) {
        ++gHkBlockedLogsQ800;
        uint64_t firstSurface = blockers.empty() ? 0u : blockers.front().surfaceKey;
        const auto sourceIt = gSurfaceSourcesQ722.find(firstSurface);
        if (sourceIt != gSurfaceSourcesQ722.end()) {
            Q6G_LOGI("Q8.0 HAVOK BLOCKER SOURCE: surface=%llu ref=%08X EDID=%s model=%s",
                     static_cast<unsigned long long>(firstSurface),
                     sourceIt->second.refFormId,
                     sourceIt->second.editorId.empty() ? "<none>" : sourceIt->second.editorId.c_str(),
                     sourceIt->second.modelPath.c_str());
        }
        Q6G_LOGI("Q8.0 HAVOK BLOCKED: start=(%.3f %.3f) pos=(%.3f %.3f) target=(%.3f %.3f) blockers=%zu raw=%zu manifold=%zu",
                 startX, startZ, x, z, targetX, targetZ,
                 blockers.size(), raw, gHkManifoldQ800.size());
    }
    RefreshHkManifoldQ800(x, z, feetY);
    return (x - startX) * (x - startX) + (z - startZ) * (z - startZ) > 0.00000025f;
}

bool FindHkGroundQ800(float x, float z, float feetY, float& groundY) {
    const HkSupportQ800 support = FindHkSupportAtQ800(x, z, feetY,
                                                      StepHeightQ78B(),
                                                      MAX_GROUND_DROP);
    if (!support.found) return false;
    groundY = support.y;
    return true;
}

]=])

string(REPLACE
    "void LogTerrainCellsQ719() {"
    "${Q800_HAVOK_PROXY_SOURCE}void LogTerrainCellsQ719() {"
    Q800_CONTROLLER_SOURCE "${Q800_CONTROLLER_SOURCE}")

# Route only the exterior character response through the clean-room proxy.
string(REPLACE
    "if (!MoveExteriorSubstepQ725(x, z, feetY, subDx, subDz,"
    "if (!MoveExteriorSubstepQ800(x, z, feetY, subDx, subDz,"
    Q800_CONTROLLER_SOURCE "${Q800_CONTROLLER_SOURCE}")

# Separate hkpCharacterProxy-style support check from movement integration.
string(REPLACE
    "bool grounded = FindSupportFootprintQ725(x, z, feetY, groundY);"
    "bool grounded = FindHkGroundQ800(x, z, feetY, groundY);"
    Q800_CONTROLLER_SOURCE "${Q800_CONTROLLER_SOURCE}")

string(REPLACE
    "mode=proxy-manifold+swept-capsule+footprint-support+up-forward-down-step persistentOverlap=depth-aware"
    "mode=fo3-havok-proxy-cleanroom+linear-cast+persistent-point-manifold+simplex+support-check persistentOverlap=keep-distance"
    Q800_CONTROLLER_SOURCE "${Q800_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q800.inc"
     "${Q800_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q725.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q800.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
