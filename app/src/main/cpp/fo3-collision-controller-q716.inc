// Q7.19 exterior character-controller response.
//
// Q7.19 replaces the old X/Z-projected "wall" test with an actual 3D
// vertical capsule-vs-authored-Havok-triangle closest-distance test. Q7.18's
// sweep/slide/step and persistent-overlap behavior remains otherwise intact.

namespace {

struct WallContactQ718 {
    uint64_t surfaceKey = 0u;
    float nx = 0.0f;
    float nz = 0.0f;
    float push = 0.0f;
    float maxY = -1e30f;
    uint32_t material = 0u;
    bool stairs = false;
    bool platform = false;
};

constexpr float Q718_DEPTH_EPS = 0.0015f;
constexpr float Q718_DIRECTION_EPS = 0.00002f;
constexpr float Q718_TANGENT_NEW_CONTACT_MAX = 0.010f;
constexpr float Q718_LOW_BLOCKER_TOP_EPS = 0.012f;
constexpr float Q719_GEOMETRY_EPS = 1e-7f;

uint64_t gSweepSlideLogsQ718 = 0u;
uint64_t gStepSweepLogsQ718 = 0u;
uint64_t gStepOverLogsQ718 = 0u;
uint64_t gPersistentEscapeLogsQ718 = 0u;
uint64_t gBlockedLogsQ718 = 0u;
uint64_t gCapsuleContactLogsQ719 = 0u;
bool gControllerReadyLoggedQ718 = false;
bool gTerrainCellLogsQ719 = false;

Vec3 Q719Sub(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Q719Add(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 Q719Mul(const Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

float Q719Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Q719LengthSq(const Vec3& v) {
    return Q719Dot(v, v);
}

Vec3 Q719Cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

Vec3 ClosestPointTriangleQ719(const Vec3& p, const CollisionTriangle& tri) {
    const Vec3 ab = Q719Sub(tri.b, tri.a);
    const Vec3 ac = Q719Sub(tri.c, tri.a);
    const Vec3 ap = Q719Sub(p, tri.a);
    const float d1 = Q719Dot(ab, ap);
    const float d2 = Q719Dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return tri.a;

    const Vec3 bp = Q719Sub(p, tri.b);
    const float d3 = Q719Dot(ab, bp);
    const float d4 = Q719Dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return tri.b;

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        const float v = d1 / (d1 - d3);
        return Q719Add(tri.a, Q719Mul(ab, v));
    }

    const Vec3 cp = Q719Sub(p, tri.c);
    const float d5 = Q719Dot(ab, cp);
    const float d6 = Q719Dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return tri.c;

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        const float w = d2 / (d2 - d6);
        return Q719Add(tri.a, Q719Mul(ac, w));
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        const Vec3 bc = Q719Sub(tri.c, tri.b);
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return Q719Add(tri.b, Q719Mul(bc, w));
    }

    const float denom = 1.0f / (va + vb + vc);
    const float v = vb * denom;
    const float w = vc * denom;
    return Q719Add(tri.a, Q719Add(Q719Mul(ab, v), Q719Mul(ac, w)));
}

void ClosestSegmentSegmentQ719(const Vec3& p1, const Vec3& q1,
                               const Vec3& p2, const Vec3& q2,
                               Vec3& c1, Vec3& c2) {
    const Vec3 d1 = Q719Sub(q1, p1);
    const Vec3 d2 = Q719Sub(q2, p2);
    const Vec3 r = Q719Sub(p1, p2);
    const float a = Q719Dot(d1, d1);
    const float e = Q719Dot(d2, d2);
    const float f = Q719Dot(d2, r);
    float s = 0.0f;
    float t = 0.0f;

    if (a <= Q719_GEOMETRY_EPS && e <= Q719_GEOMETRY_EPS) {
        c1 = p1;
        c2 = p2;
        return;
    }
    if (a <= Q719_GEOMETRY_EPS) {
        s = 0.0f;
        t = std::clamp(f / e, 0.0f, 1.0f);
    } else {
        const float c = Q719Dot(d1, r);
        if (e <= Q719_GEOMETRY_EPS) {
            t = 0.0f;
            s = std::clamp(-c / a, 0.0f, 1.0f);
        } else {
            const float b = Q719Dot(d1, d2);
            const float denom = a * e - b * b;
            if (std::fabs(denom) > Q719_GEOMETRY_EPS) {
                s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            }
            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = std::clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = std::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    c1 = Q719Add(p1, Q719Mul(d1, s));
    c2 = Q719Add(p2, Q719Mul(d2, t));
}

float ClosestCapsuleSegmentTriangleQ719(const Vec3& segA, const Vec3& segB,
                                        const CollisionTriangle& tri,
                                        Vec3& outSegment, Vec3& outTriangle) {
    float best = std::numeric_limits<float>::max();

    auto consider = [&](const Vec3& s, const Vec3& t) {
        const float d2 = Q719LengthSq(Q719Sub(s, t));
        if (d2 < best) {
            best = d2;
            outSegment = s;
            outTriangle = t;
        }
    };

    consider(segA, ClosestPointTriangleQ719(segA, tri));
    consider(segB, ClosestPointTriangleQ719(segB, tri));

    const std::array<std::pair<Vec3, Vec3>, 3> edges{{
        {tri.a, tri.b}, {tri.b, tri.c}, {tri.c, tri.a}
    }};
    for (const auto& edge : edges) {
        Vec3 s, t;
        ClosestSegmentSegmentQ719(segA, segB, edge.first, edge.second, s, t);
        consider(s, t);
    }

    const Vec3 segDir = Q719Sub(segB, segA);
    const Vec3 planeN = Q719Cross(Q719Sub(tri.b, tri.a), Q719Sub(tri.c, tri.a));
    const float denom = Q719Dot(planeN, segDir);
    if (std::fabs(denom) > Q719_GEOMETRY_EPS) {
        const float t = Q719Dot(planeN, Q719Sub(tri.a, segA)) / denom;
        if (t >= 0.0f && t <= 1.0f) {
            const Vec3 p = Q719Add(segA, Q719Mul(segDir, t));
            const Vec3 q = ClosestPointTriangleQ719(p, tri);
            if (Q719LengthSq(Q719Sub(p, q)) <= 1e-8f) {
                outSegment = p;
                outTriangle = p;
                best = 0.0f;
            }
        }
    }
    return best;
}

bool ComputeWallContactQ719(const CollisionTriangle& tri, float x, float z,
                            float feetY, float collisionRadius,
                            WallContactQ714& out) {
    const float stepHeight = StepHeightQ78B();
    const float radius2 = collisionRadius * collisionRadius;

    if (std::fabs(tri.normal.y) >= WalkableNormalThresholdQ714(tri)) return false;
    if (gExteriorAllBhksQ78A && tri.maxY <= feetY + stepHeight + PLAYER_SKIN) return false;

    const float lowerY = feetY + collisionRadius;
    const float upperY = feetY + PLAYER_HEIGHT - collisionRadius;
    if (tri.maxY < lowerY - collisionRadius || tri.minY > upperY + collisionRadius) return false;
    if (x < tri.minX - collisionRadius || x > tri.maxX + collisionRadius ||
        z < tri.minZ - collisionRadius || z > tri.maxZ + collisionRadius) return false;

    const Vec3 segA{x, lowerY, z};
    const Vec3 segB{x, upperY, z};
    Vec3 capsulePoint;
    Vec3 trianglePoint;
    const float d2 = ClosestCapsuleSegmentTriangleQ719(segA, segB, tri, capsulePoint, trianglePoint);
    if (!std::isfinite(d2) || d2 >= radius2) return false;

    const float distance = std::sqrt(std::max(d2, 0.0f));
    const Vec3 separation = Q719Sub(capsulePoint, trianglePoint);
    float nx = separation.x;
    float nz = separation.z;
    float horizontalLength = std::sqrt(nx * nx + nz * nz);

    if (horizontalLength > 1e-5f) {
        nx /= horizontalLength;
        nz /= horizontalLength;
    } else {
        nx = tri.normal.x;
        nz = tri.normal.z;
        horizontalLength = std::sqrt(nx * nx + nz * nz);
        if (horizontalLength < 1e-5f) return false;
        nx /= horizontalLength;
        nz /= horizontalLength;
        const float centroidX = (tri.a.x + tri.b.x + tri.c.x) / 3.0f;
        const float centroidZ = (tri.a.z + tri.b.z + tri.c.z) / 3.0f;
        if (nx * (x - centroidX) + nz * (z - centroidZ) < 0.0f) {
            nx = -nx;
            nz = -nz;
        }
    }

    out.surfaceKey = tri.surfaceKeyQ714;
    out.nx = nx;
    out.nz = nz;
    out.push = (collisionRadius - distance) + PLAYER_SKIN;

    if (gCapsuleContactLogsQ719 < 30u) {
        ++gCapsuleContactLogsQ719;
        Q6G_LOGI("Q7.19 CAPSULE CONTACT: surface=%llu distance=%.4f push=%.4f normal=(%.3f %.3f) triNormalY=%.3f triY=(%.3f..%.3f) feetY=%.3f material=%u stairs=%d platform=%d",
                 static_cast<unsigned long long>(tri.surfaceKeyQ714), distance, out.push,
                 out.nx, out.nz, tri.normal.y, tri.minY, tri.maxY, feetY,
                 tri.havokMaterialQ714, tri.stairsQ714 ? 1 : 0, tri.platformQ714 ? 1 : 0);
    }
    return out.push > 0.0f;
}

void GatherWallContactsQ718(float x, float z, float feetY,
                            std::vector<WallContactQ718>& out,
                            size_t* outRawTriangles = nullptr) {
    out.clear();
    if (outRawTriangles) *outRawTriangles = 0u;
    const float collisionRadius = CollisionRadiusQ712();
    std::unordered_map<uint64_t, size_t> slotBySurface;
    slotBySurface.reserve(8u);

    for (const CollisionTriangle& tri : gWorldTriangles) {
        WallContactQ714 contact;
        if (!ComputeWallContactQ719(tri, x, z, feetY, collisionRadius, contact)) continue;
        if (outRawTriangles) ++(*outRawTriangles);
        const auto found = slotBySurface.find(contact.surfaceKey);
        if (found == slotBySurface.end()) {
            const size_t slot = out.size();
            slotBySurface.emplace(contact.surfaceKey, slot);
            WallContactQ718 added;
            added.surfaceKey = contact.surfaceKey;
            added.nx = contact.nx;
            added.nz = contact.nz;
            added.push = contact.push;
            added.maxY = tri.maxY;
            added.material = tri.havokMaterialQ714;
            added.stairs = tri.stairsQ714;
            added.platform = tri.platformQ714;
            out.push_back(added);
        } else {
            WallContactQ718& existing = out[found->second];
            existing.stairs = existing.stairs || tri.stairsQ714;
            existing.platform = existing.platform || tri.platformQ714;
            existing.maxY = std::max(existing.maxY, tri.maxY);
            if (contact.push > existing.push) {
                existing.nx = contact.nx;
                existing.nz = contact.nz;
                existing.push = contact.push;
                existing.material = tri.havokMaterialQ714;
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const WallContactQ718& a, const WallContactQ718& b) {
        return a.push > b.push;
    });
}

const WallContactQ718* FindSurfaceQ718(const std::vector<WallContactQ718>& contacts,
                                       uint64_t surfaceKey) {
    for (const WallContactQ718& contact : contacts) {
        if (contact.surfaceKey == surfaceKey) return &contact;
    }
    return nullptr;
}

bool CandidateAllowedQ718(float targetX, float targetZ, float feetY,
                          float moveX, float moveZ,
                          const std::vector<WallContactQ718>& baseline,
                          std::vector<WallContactQ718>& blockers,
                          size_t* outRawTriangles = nullptr,
                          size_t* outPersistent = nullptr) {
    std::vector<WallContactQ718> target;
    GatherWallContactsQ718(targetX, targetZ, feetY, target, outRawTriangles);
    blockers.clear();
    if (outPersistent) *outPersistent = 0u;
    for (const WallContactQ718& contact : target) {
        const WallContactQ718* previous = FindSurfaceQ718(baseline, contact.surfaceKey);
        const float into = moveX * contact.nx + moveZ * contact.nz;
        if (previous) {
            const float depthDelta = contact.push - previous->push;
            if (depthDelta <= Q718_DEPTH_EPS || into >= -Q718_DIRECTION_EPS) {
                if (outPersistent) ++(*outPersistent);
                continue;
            }
        } else if (into >= -Q718_DIRECTION_EPS && contact.push <= Q718_TANGENT_NEW_CONTACT_MAX) {
            if (outPersistent) ++(*outPersistent);
            continue;
        }
        blockers.push_back(contact);
    }
    return blockers.empty();
}

Vec2 ClipMoveAgainstContactsQ718(Vec2 move,
                                 const std::vector<WallContactQ718>& contacts,
                                 size_t& usedNormals) {
    struct Normal2 { float x = 0.0f, z = 0.0f; };
    std::vector<Normal2> normals;
    normals.reserve(std::min<size_t>(contacts.size(), 4u));
    for (const WallContactQ718& contact : contacts) {
        const float into = move.x * contact.nx + move.y * contact.nz;
        if (into >= -Q718_DIRECTION_EPS) continue;
        bool duplicate = false;
        for (const Normal2& n : normals) {
            if (n.x * contact.nx + n.z * contact.nz > 0.985f) { duplicate = true; break; }
        }
        if (!duplicate) normals.push_back({contact.nx, contact.nz});
        if (normals.size() >= 3u) break;
    }
    for (int pass = 0; pass < 3; ++pass) {
        for (const Normal2& n : normals) {
            const float into = move.x * n.x + move.y * n.z;
            if (into < 0.0f) {
                move.x -= n.x * into;
                move.y -= n.z * into;
            }
        }
    }
    usedNormals = normals.size();
    return move;
}

bool FindStepLandingQ718(float targetX, float targetZ, float moveX, float moveZ,
                         float feetY, float& outLandingY) {
    const float length = std::sqrt(moveX * moveX + moveZ * moveZ);
    if (length < 0.00001f) return false;
    const float nx = moveX / length;
    const float nz = moveZ / length;
    const float radius = CollisionRadiusQ712();
    const std::array<float, 5> probes{0.0f, radius * 0.45f, radius * 0.75f,
                                      radius * 1.05f, radius * 1.35f};
    bool found = false;
    float lowest = std::numeric_limits<float>::max();
    for (float ahead : probes) {
        float candidateY = feetY;
        if (!FindStepGroundQ713(targetX + nx * ahead, targetZ + nz * ahead,
                                feetY, candidateY)) continue;
        if (!found || candidateY < lowest) { lowest = candidateY; found = true; }
    }
    if (found) outLandingY = lowest;
    return found;
}

bool TryStepSweepQ718(float startX, float startZ, float targetX, float targetZ,
                      float feetY, float& outFeetY, size_t& blockingSurfaces) {
    const float moveX = targetX - startX;
    const float moveZ = targetZ - startZ;
    float landingY = feetY;
    if (!FindStepLandingQ718(targetX, targetZ, moveX, moveZ, feetY, landingY)) return false;
    const float rise = landingY - feetY;
    if (rise <= STEP_MIN_RISE_Q78B || rise > StepHeightQ78B() + PLAYER_SKIN) return false;
    const float raisedFeetY = feetY + StepHeightQ78B() + PLAYER_SKIN;
    if (HasOverheadBlock(startX, startZ, raisedFeetY) ||
        HasOverheadBlock(targetX, targetZ, landingY)) return false;
    std::vector<WallContactQ718> raisedBaseline;
    GatherWallContactsQ718(startX, startZ, raisedFeetY, raisedBaseline);
    std::vector<WallContactQ718> raisedBlockers;
    size_t raw = 0u, persistent = 0u;
    CandidateAllowedQ718(targetX, targetZ, raisedFeetY, moveX, moveZ,
                         raisedBaseline, raisedBlockers, &raw, &persistent);
    blockingSurfaces = raisedBlockers.size();
    if (!raisedBlockers.empty()) return false;
    outFeetY = landingY;
    if (gStepSweepLogsQ718 < 80u || (gResolveCounter % 360u) == 0u) {
        ++gStepSweepLogsQ718;
        Q6G_LOGI("Q7.19 STEP SWEEP: from=(%.3f %.3f) to=(%.3f %.3f) rise=%.3f raisedTestY=%.3f persistent=%zu maxStep=%.3f clear=1",
                 startX, startZ, targetX, targetZ, rise, raisedFeetY, persistent, StepHeightQ78B());
    }
    return true;
}

bool TryLowBlockerStepOverQ718(float startX, float startZ, float targetX, float targetZ,
                               float feetY, const std::vector<WallContactQ718>& blockers,
                               float& outFeetY, size_t& outRaisedBlockers) {
    outRaisedBlockers = 0u;
    if (blockers.empty()) return false;
    const float maxStep = StepHeightQ78B();
    float blockerTop = -1e30f;
    for (const WallContactQ718& blocker : blockers) {
        if (!std::isfinite(blocker.maxY)) return false;
        blockerTop = std::max(blockerTop, blocker.maxY);
    }
    const float riseToTop = blockerTop - feetY;
    if (riseToTop <= STEP_MIN_RISE_Q78B ||
        riseToTop > maxStep + PLAYER_SKIN + Q718_LOW_BLOCKER_TOP_EPS) return false;
    const float raisedFeetY = std::min(feetY + maxStep + PLAYER_SKIN,
                                      blockerTop + PLAYER_SKIN + Q718_LOW_BLOCKER_TOP_EPS);
    if (HasOverheadBlock(startX, startZ, raisedFeetY) ||
        HasOverheadBlock(targetX, targetZ, raisedFeetY)) return false;
    const float moveX = targetX - startX;
    const float moveZ = targetZ - startZ;
    std::vector<WallContactQ718> raisedBaseline;
    GatherWallContactsQ718(startX, startZ, raisedFeetY, raisedBaseline);
    std::vector<WallContactQ718> raisedBlockers;
    size_t raw = 0u, persistent = 0u;
    CandidateAllowedQ718(targetX, targetZ, raisedFeetY, moveX, moveZ,
                         raisedBaseline, raisedBlockers, &raw, &persistent);
    outRaisedBlockers = raisedBlockers.size();
    if (!raisedBlockers.empty()) return false;
    outFeetY = raisedFeetY;
    if (gStepOverLogsQ718 < 80u || (gResolveCounter % 240u) == 0u) {
        ++gStepOverLogsQ718;
        Q6G_LOGI("Q7.19 STEP OVER: from=(%.3f %.3f) to=(%.3f %.3f) blockerTop=%.3f rise=%.3f raisedTestY=%.3f blockers=%zu persistent=%zu clear=1",
                 startX, startZ, targetX, targetZ, blockerTop, riseToTop, raisedFeetY,
                 blockers.size(), persistent);
    }
    return true;
}

bool MoveExteriorSubstepQ718(float& x, float& z, float& feetY,
                             float moveX, float moveZ, bool& steppedThisFrame,
                             uint32_t& contactSurfaces) {
    if (moveX * moveX + moveZ * moveZ < 1e-10f) return true;
    std::vector<WallContactQ718> baseline;
    size_t baselineRaw = 0u;
    GatherWallContactsQ718(x, z, feetY, baseline, &baselineRaw);
    const float targetX = x + moveX;
    const float targetZ = z + moveZ;
    std::vector<WallContactQ718> blockers;
    size_t rawTriangles = 0u, persistent = 0u;
    if (CandidateAllowedQ718(targetX, targetZ, feetY, moveX, moveZ, baseline,
                             blockers, &rawTriangles, &persistent)) {
        if (!baseline.empty() &&
            (gPersistentEscapeLogsQ718 < 80u || (gResolveCounter % 240u) == 0u)) {
            ++gPersistentEscapeLogsQ718;
            Q6G_LOGI("Q7.19 OVERLAP ESCAPE: from=(%.3f %.3f) to=(%.3f %.3f) baselineSurfaces=%zu persistent=%zu targetRaw=%zu allowed=1",
                     x, z, targetX, targetZ, baseline.size(), persistent, rawTriangles);
        }
        x = targetX; z = targetZ; return true;
    }
    contactSurfaces += static_cast<uint32_t>(blockers.size());
    size_t stepBlockers = 0u;
    float steppedFeetY = feetY;
    if (TryStepSweepQ718(x, z, targetX, targetZ, feetY, steppedFeetY, stepBlockers)) {
        x = targetX; z = targetZ; feetY = steppedFeetY; steppedThisFrame = true; return true;
    }
    size_t raisedStepBlockers = 0u;
    float stepOverFeetY = feetY;
    if (TryLowBlockerStepOverQ718(x, z, targetX, targetZ, feetY, blockers,
                                  stepOverFeetY, raisedStepBlockers)) {
        x = targetX; z = targetZ; feetY = stepOverFeetY; steppedThisFrame = true; return true;
    }
    Vec2 slide{moveX, moveZ};
    const float requestedLength = std::sqrt(moveX * moveX + moveZ * moveZ);
    size_t usedNormals = 0u;
    for (int iteration = 0; iteration < 3; ++iteration) {
        slide = ClipMoveAgainstContactsQ718(slide, blockers, usedNormals);
        if (slide.x * slide.x + slide.y * slide.y < 0.00000025f) break;
        const float slideX = x + slide.x;
        const float slideZ = z + slide.y;
        std::vector<WallContactQ718> slideBlockers;
        size_t slideRawTriangles = 0u, slidePersistent = 0u;
        if (CandidateAllowedQ718(slideX, slideZ, feetY, slide.x, slide.y, baseline,
                                 slideBlockers, &slideRawTriangles, &slidePersistent)) {
            if (gSweepSlideLogsQ718 < 100u || (gResolveCounter % 240u) == 0u) {
                ++gSweepSlideLogsQ718;
                Q6G_LOGI("Q7.19 SWEEP SLIDE: from=(%.3f %.3f) requested=(%.3f %.3f) applied=(%.3f %.3f) blockers=%zu normals=%zu persistent=%zu",
                         x, z, moveX, moveZ, slide.x, slide.y,
                         blockers.size(), usedNormals, slidePersistent);
            }
            x = slideX; z = slideZ; return true;
        }
        float slideStepFeetY = feetY;
        size_t slideStepBlockers = 0u;
        if (TryStepSweepQ718(x, z, slideX, slideZ, feetY, slideStepFeetY, slideStepBlockers)) {
            x = slideX; z = slideZ; feetY = slideStepFeetY; steppedThisFrame = true; return true;
        }
        size_t slideRaisedBlockers = 0u;
        float slideStepOverY = feetY;
        if (TryLowBlockerStepOverQ718(x, z, slideX, slideZ, feetY, slideBlockers,
                                      slideStepOverY, slideRaisedBlockers)) {
            x = slideX; z = slideZ; feetY = slideStepOverY; steppedThisFrame = true; return true;
        }
        blockers.swap(slideBlockers);
        rawTriangles = slideRawTriangles;
        contactSurfaces += static_cast<uint32_t>(blockers.size());
    }
    if (gBlockedLogsQ718 < 80u || (gResolveCounter % 240u) == 0u) {
        ++gBlockedLogsQ718;
        const float appliedLength = std::sqrt(slide.x * slide.x + slide.y * slide.y);
        float blockerTop = -1e30f;
        uint64_t firstSurface = 0u;
        uint32_t firstMaterial = 0u;
        bool firstStairs = false, firstPlatform = false;
        for (const WallContactQ718& blocker : blockers) {
            blockerTop = std::max(blockerTop, blocker.maxY);
            if (firstSurface == 0u) {
                firstSurface = blocker.surfaceKey;
                firstMaterial = blocker.material;
                firstStairs = blocker.stairs;
                firstPlatform = blocker.platform;
            }
        }
        Q6G_LOGI("Q7.19 BLOCKED: pos=(%.3f %.3f) requestedLen=%.3f clippedLen=%.3f rawTriangles=%zu blockers=%zu normals=%zu stepBlockers=%zu raisedStepBlockers=%zu blockerTop=%.3f feetY=%.3f baseline=%zu surface=%llu material=%u stairs=%d platform=%d",
                 x, z, requestedLength, appliedLength, rawTriangles, blockers.size(), usedNormals,
                 stepBlockers, raisedStepBlockers, blockerTop, feetY, baseline.size(),
                 static_cast<unsigned long long>(firstSurface), firstMaterial,
                 firstStairs ? 1 : 0, firstPlatform ? 1 : 0);
    }
    return false;
}

void LogTerrainCellsQ719() {
    if (gTerrainCellLogsQ719) return;
    gTerrainCellLogsQ719 = true;
    const auto& terrain = GetFo3TerrainQ76();
    Q6G_LOGI("Q7.19 LAND DIAGNOSTICS: cells=%zu note=child-worldspace-may-mix-local-and-parent-LAND", terrain.size());
    for (const Fo3TerrainCellQ76& cell : terrain) {
        if (cell.heights.empty()) continue;
        const auto range = std::minmax_element(cell.heights.begin(), cell.heights.end());
        Q6G_LOGI("Q7.19 LAND CELL: cell=%08X LAND=%08X grid=(%d,%d) minHeight=%.1f maxHeight=%.1f",
                 cell.cellFormId, cell.landFormId, cell.gridX, cell.gridY, *range.first, *range.second);
    }
}

} // namespace

bool ResolveFo3PlayerMotionQ6G(float currentX, float currentZ,
                               float desiredX, float desiredZ,
                               float currentPlayerYOffset,
                               float* outX, float* outZ,
                               float* outPlayerYOffset) {
    if (!outX || !outZ || !outPlayerYOffset) return false;
    if (ConsumeFo3PlayerResetQ74()) {
        *outX = 0.0f; *outZ = 0.0f; *outPlayerYOffset = 0.0f;
        gSafeSpawnResolved = false;
        Q6G_LOGI("Q7.19 PLAYER RESET: bodyCenter=(0,0) source=XTEL newCollisionReady=%d",
                 gPlayerCollisionReady ? 1 : 0);
        return true;
    }
    if (!gPlayerCollisionReady || gWorldTriangles.empty()) return false;
    if (!gExteriorAllBhksQ78A || !gSafeSpawnResolved) {
        return ResolveFo3PlayerMotionLegacyQ716(currentX, currentZ, desiredX, desiredZ,
                                                currentPlayerYOffset, outX, outZ, outPlayerYOffset);
    }
    ++gResolveCounter;
    if (!gControllerReadyLoggedQ718) {
        gControllerReadyLoggedQ718 = true;
        Q6G_LOGI("Q7.19 CONTROLLER READY: mode=3d-capsule-triangle+sweep-slide-step persistentOverlap=depth-aware depenetration=0 maxStep=%.3f depthEps=%.4f",
                 StepHeightQ78B(), Q718_DEPTH_EPS);
        LogTerrainCellsQ719();
    }
    float x = currentX, z = currentZ;
    float feetY = gCollisionFloorY + currentPlayerYOffset;
    const float requestedDx = desiredX - currentX;
    const float requestedDz = desiredZ - currentZ;
    const float distance = std::sqrt(requestedDx * requestedDx + requestedDz * requestedDz);
    const float maxSubstep = CollisionRadiusQ712() * 0.22f;
    const int steps = std::clamp(static_cast<int>(std::ceil(distance / maxSubstep)), 1, 16);
    const float subDx = requestedDx / static_cast<float>(steps);
    const float subDz = requestedDz / static_cast<float>(steps);
    bool steppedThisFrame = false;
    uint32_t contactSurfaces = 0u;
    for (int step = 0; step < steps; ++step) {
        if (!MoveExteriorSubstepQ718(x, z, feetY, subDx, subDz,
                                     steppedThisFrame, contactSurfaces)) break;
    }
    float groundY = feetY;
    bool grounded = FindGround(x, z, feetY, groundY);
    const bool authoredGrounded = grounded;
    float terrainPlayerY = currentPlayerYOffset;
    const bool terrainGrounded = SampleFo3TerrainGroundQ77(x, z, &terrainPlayerY);
    if (terrainGrounded) {
        const float terrainGroundY = gCollisionFloorY + terrainPlayerY;
        if (!grounded || terrainGroundY > groundY) { groundY = terrainGroundY; grounded = true; }
        if (gTerrainGroundLogCountQ77 < 20u || (gResolveCounter % 360u) == 0u) {
            ++gTerrainGroundLogCountQ77;
            Q6G_LOGI("Q7.19 LAND GROUND: pos=(%.3f %.3f) terrainY=%.3f selectedGroundY=%.3f authoredGround=%d",
                     x, z, terrainGroundY, groundY, authoredGrounded ? 1 : 0);
        }
    }
    if (steppedThisFrame && (!grounded || groundY < feetY - 0.05f)) {
        groundY = feetY; grounded = true;
    }
    float playerYOffset = feetY - gCollisionFloorY;
    if (grounded) playerYOffset = groundY - gCollisionFloorY;
    *outX = x; *outZ = z; *outPlayerYOffset = playerYOffset;
    const float correctionX = x - desiredX;
    const float correctionZ = z - desiredZ;
    const bool blocked = contactSurfaces > 0u ||
        correctionX * correctionX + correctionZ * correctionZ > 0.000004f;
    if (blocked && (gContactLogCount < 80u || (gResolveCounter % 120u) == 0u)) {
        ++gContactLogCount;
        Q6G_LOGI("Q7.19 CONTACT: desired=(%.3f %.3f) resolved=(%.3f %.3f) correction=(%.3f %.3f) blockers=%u stepped=%d grounded=%d playerY=%.3f",
                 desiredX, desiredZ, x, z, correctionX, correctionZ, contactSurfaces,
                 steppedThisFrame ? 1 : 0, grounded ? 1 : 0, playerYOffset);
    }
    return true;
}
