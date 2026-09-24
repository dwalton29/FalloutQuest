// Q9.4 exterior character-controller response.
//
// Q9.4 replaces the old X/Z-projected "wall" test with an actual 3D
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
uint64_t gSeamBridgeLogsQ721 = 0u;
uint64_t gPersistentEscapeLogsQ718 = 0u;
uint64_t gBlockedLogsQ718 = 0u;
uint64_t gCapsuleContactLogsQ719 = 0u;
uint64_t gModuleEndcapLogsQ723 = 0u;
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
    const float radius2 = collisionRadius * collisionRadius;

    if (std::fabs(tri.normal.y) >= WalkableNormalThresholdQ714(tri)) return false;

    // Q9.4: do not hide low wall contacts here. The character controller must
    // see the real obstacle first and prove it can step over it with a raised
    // capsule test. Suppressing triangles by their maxY made the raised test
    // circular and could also let obstacles taller than maxStep disappear.
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

    if (false && gExteriorAllBhksQ78A && tri.walkableModuleQ723) {
        const float contactRise = trianglePoint.y - feetY;
        const float triTopRise = tri.maxY - feetY;
        const float maxLowContact = StepHeightQ78B() + PLAYER_SKIN + 0.020f;

        // A real tall side face remains a blocker because it will still touch
        // the capsule after the player body is raised by the normal step height.
        const float raisedFeetYQ724 = feetY + StepHeightQ78B() + PLAYER_SKIN;
        const Vec3 raisedSegAQ724{x, raisedFeetYQ724 + collisionRadius, z};
        const Vec3 raisedSegBQ724{x, raisedFeetYQ724 + PLAYER_HEIGHT - collisionRadius, z};
        Vec3 raisedCapsulePointQ724;
        Vec3 raisedTrianglePointQ724;
        const float raisedD2Q724 = ClosestCapsuleSegmentTriangleQ719(
            raisedSegAQ724, raisedSegBQ724, tri,
            raisedCapsulePointQ724, raisedTrianglePointQ724);
        const bool clearsWhenRaisedQ724 = !std::isfinite(raisedD2Q724) || raisedD2Q724 >= radius2;

        if (contactRise >= -0.080f && contactRise <= maxLowContact && clearsWhenRaisedQ724) {
            if (gModuleEndcapLogsQ723 < 160u || (gResolveCounter % 240u) == 0u) {
                ++gModuleEndcapLogsQ723;
                const auto sourceIt = gSurfaceSourcesQ722.find(tri.surfaceKeyQ714);
                if (sourceIt != gSurfaceSourcesQ722.end()) {
                    Q6G_LOGI("Q9.4 MODULE CONTACT PASS: surface=%llu ref=%08X EDID=%s model=%s contactRise=%.3f triTopRise=%.3f triNormalY=%.3f raisedClear=1 maxStep=%.3f",
                             static_cast<unsigned long long>(tri.surfaceKeyQ714),
                             sourceIt->second.refFormId,
                             sourceIt->second.editorId.empty() ? "<none>" : sourceIt->second.editorId.c_str(),
                             sourceIt->second.modelPath.c_str(),
                             contactRise, triTopRise, tri.normal.y, StepHeightQ78B());
                }
            }
            return false;
        }
    }

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
        Q6G_LOGI("Q9.4 CAPSULE CONTACT: surface=%llu distance=%.4f push=%.4f normal=(%.3f %.3f) triNormalY=%.3f triY=(%.3f..%.3f) feetY=%.3f material=%u stairs=%d platform=%d",
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
        Q6G_LOGI("Q9.4 STEP SWEEP: from=(%.3f %.3f) to=(%.3f %.3f) rise=%.3f raisedTestY=%.3f persistent=%zu maxStep=%.3f clear=1",
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
    const float raisedFeetY = feetY + maxStep + PLAYER_SKIN;
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

    // The raised horizontal path is clear. Now prove there is sensible ground
    // to land on; do not turn this into a ledge-jump or a hover state.
    float landingY = raisedFeetY;
    if (!FindGround(targetX, targetZ, raisedFeetY, landingY)) return false;
    const float landingRise = landingY - feetY;
    if (landingRise < -0.08f || landingRise > maxStep + PLAYER_SKIN) return false;

    outFeetY = landingY;
    if (gStepOverLogsQ718 < 120u || (gResolveCounter % 240u) == 0u) {
        ++gStepOverLogsQ718;
        Q6G_LOGI("Q9.4 RAISED STEP: from=(%.3f %.3f) to=(%.3f %.3f) raisedTestY=%.3f landingY=%.3f rise=%.3f originalBlockers=%zu raisedBlockers=%zu persistent=%zu clear=1",
                 startX, startZ, targetX, targetZ, raisedFeetY, landingY,
                 landingRise, blockers.size(), raisedBlockers.size(), persistent);
    }
    return true;
}

bool RaisedPathClearQ722(float startX, float startZ,
                         float endX, float endZ,
                         float raisedFeetY) {
    const float dx = endX - startX;
    const float dz = endZ - startZ;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length < 0.00001f) return true;

    const float sampleStep = std::max(0.025f, CollisionRadiusQ712() * 0.30f);
    const int samples = std::clamp(static_cast<int>(std::ceil(length / sampleStep)), 1, 16);
    float previousX = startX;
    float previousZ = startZ;
    std::vector<WallContactQ718> baseline;
    GatherWallContactsQ718(previousX, previousZ, raisedFeetY, baseline);

    for (int i = 1; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float x = startX + dx * t;
        const float z = startZ + dz * t;
        const float moveX = x - previousX;
        const float moveZ = z - previousZ;
        std::vector<WallContactQ718> blockers;
        size_t raw = 0u, persistent = 0u;
        if (!CandidateAllowedQ718(x, z, raisedFeetY, moveX, moveZ,
                                  baseline, blockers, &raw, &persistent)) {
            return false;
        }
        GatherWallContactsQ718(x, z, raisedFeetY, baseline);
        previousX = x;
        previousZ = z;
    }
    return true;
}

bool TryWalkableSeamBridgeQ721(float startX, float startZ,
                               float targetX, float targetZ,
                               float feetY,
                               const std::vector<WallContactQ718>& blockers,
                               float& outFeetY,
                               size_t& outRaisedBlockers,
                               float& outProbeAhead) {
    outRaisedBlockers = 0u;
    outProbeAhead = 0.0f;
    if (blockers.empty()) return false;

    const float moveX = targetX - startX;
    const float moveZ = targetZ - startZ;
    const float moveLength = std::sqrt(moveX * moveX + moveZ * moveZ);
    if (moveLength < 0.00001f) return false;
    const float dirX = moveX / moveLength;
    const float dirZ = moveZ / moveLength;

    // First prove this is not a real wall: the same horizontal endpoint must be
    // reachable by a capsule raised by the normal Fallout step allowance.
    const float maxStep = StepHeightQ78B();
    const float raisedFeetY = feetY + maxStep + PLAYER_SKIN;
    if (HasOverheadBlock(startX, startZ, raisedFeetY) ||
        HasOverheadBlock(targetX, targetZ, raisedFeetY)) return false;

    std::vector<WallContactQ718> raisedBaseline;
    GatherWallContactsQ718(startX, startZ, raisedFeetY, raisedBaseline);
    std::vector<WallContactQ718> raisedBlockers;
    size_t raisedRaw = 0u, raisedPersistent = 0u;
    CandidateAllowedQ718(targetX, targetZ, raisedFeetY, moveX, moveZ,
                         raisedBaseline, raisedBlockers,
                         &raisedRaw, &raisedPersistent);
    outRaisedBlockers = raisedBlockers.size();
    if (!raisedBlockers.empty()) return false;

    const float radius = CollisionRadiusQ712();

    bool supportedBehind = false;
    float supportBehindY = feetY;
    const std::array<float, 6> behindProbes{
        0.0f,
        radius * 0.25f,
        radius * 0.50f,
        radius * 0.75f,
        radius * 1.00f,
        radius * 1.25f,
    };
    for (float behind : behindProbes) {
        float candidateY = feetY;
        if (!FindGround(startX - dirX * behind,
                        startZ - dirZ * behind,
                        feetY, candidateY)) continue;
        const float delta = candidateY - feetY;
        if (delta < -0.20f || delta > 0.20f) continue;
        supportedBehind = true;
        supportBehindY = candidateY;
        break;
    }
    if (!supportedBehind) return false;

    // The capsule centre begins colliding before it reaches the physical edge.
    // Search beyond a complete radius so the probe genuinely reaches the next
    // authored top face. The maximum classification reach is still under 0.5m.
    const std::array<float, 7> probes{
        radius * 0.70f,
        radius * 0.95f,
        radius * 1.20f,
        radius * 1.45f,
        radius * 1.70f,
        radius * 1.95f,
        radius * 2.00f,
    };

    bool foundLanding = false;
    float landingY = feetY;
    float landingAhead = 0.0f;
    for (float ahead : probes) {
        const float probeX = targetX + dirX * ahead;
        const float probeZ = targetZ + dirZ * ahead;
        float candidateY = feetY;
        if (!FindGround(probeX, probeZ, feetY, candidateY)) continue;
        const float rise = candidateY - feetY;
        if (rise < -0.12f || rise > maxStep + PLAYER_SKIN) continue;
        if (HasOverheadBlock(probeX, probeZ, candidateY)) continue;
        if (!RaisedPathClearQ722(startX, startZ, probeX, probeZ, raisedFeetY)) continue;

        landingY = candidateY;
        landingAhead = ahead;
        foundLanding = true;
        break;
    }
    if (!foundLanding) return false;

    outFeetY = landingY;
    outProbeAhead = landingAhead;
    if (gSeamBridgeLogsQ721 < 120u || (gResolveCounter % 240u) == 0u) {
        ++gSeamBridgeLogsQ721;
        Q6G_LOGI("Q9.4 SEAM BRIDGE: from=(%.3f %.3f) to=(%.3f %.3f) probeAhead=%.3f startY=%.3f landingY=%.3f rise=%.3f originalBlockers=%zu raisedBlockers=%zu raisedRaw=%zu persistent=%zu behindY=%.3f pathClear=1 clear=1",
                 startX, startZ, targetX, targetZ, landingAhead,
                 feetY, landingY, landingY - feetY,
                 blockers.size(), raisedBlockers.size(), raisedRaw, raisedPersistent, supportBehindY);
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
            Q6G_LOGI("Q9.4 OVERLAP ESCAPE: from=(%.3f %.3f) to=(%.3f %.3f) baselineSurfaces=%zu persistent=%zu targetRaw=%zu allowed=1",
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

    // Q9.4: if the raised capsule is clear but the exact endpoint has no
    // walkable landing (typical thin modular end-cap), accept nearby support
    // immediately beyond the seam instead of turning the end face into a wall.
    size_t seamRaisedBlockers = 0u;
    float seamFeetY = feetY;
    float seamProbeAhead = 0.0f;
    if (TryWalkableSeamBridgeQ721(x, z, targetX, targetZ, feetY, blockers,
                                  seamFeetY, seamRaisedBlockers, seamProbeAhead)) {
        x = targetX; z = targetZ; feetY = seamFeetY; steppedThisFrame = true; return true;
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
                Q6G_LOGI("Q9.4 SWEEP SLIDE: from=(%.3f %.3f) requested=(%.3f %.3f) applied=(%.3f %.3f) blockers=%zu normals=%zu persistent=%zu",
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
        size_t slideSeamRaisedBlockers = 0u;
        float slideSeamFeetY = feetY;
        float slideSeamProbeAhead = 0.0f;
        if (TryWalkableSeamBridgeQ721(x, z, slideX, slideZ, feetY, slideBlockers,
                                      slideSeamFeetY, slideSeamRaisedBlockers,
                                      slideSeamProbeAhead)) {
            x = slideX; z = slideZ; feetY = slideSeamFeetY; steppedThisFrame = true; return true;
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
        const auto q722SourceIt = gSurfaceSourcesQ722.find(firstSurface);
        if (q722SourceIt != gSurfaceSourcesQ722.end()) {
            Q6G_LOGI("Q9.4 BLOCKER SOURCE: surface=%llu ref=%08X EDID=%s model=%s",
                     static_cast<unsigned long long>(firstSurface),
                     q722SourceIt->second.refFormId,
                     q722SourceIt->second.editorId.empty() ? "<none>" : q722SourceIt->second.editorId.c_str(),
                     q722SourceIt->second.modelPath.c_str());
        }
        Q6G_LOGI("Q9.4 BLOCKED: pos=(%.3f %.3f) requestedLen=%.3f clippedLen=%.3f rawTriangles=%zu blockers=%zu normals=%zu stepBlockers=%zu raisedStepBlockers=%zu blockerTop=%.3f feetY=%.3f baseline=%zu surface=%llu material=%u stairs=%d platform=%d",
                 x, z, requestedLength, appliedLength, rawTriangles, blockers.size(), usedNormals,
                 stepBlockers, raisedStepBlockers, blockerTop, feetY, baseline.size(),
                 static_cast<unsigned long long>(firstSurface), firstMaterial,
                 firstStairs ? 1 : 0, firstPlatform ? 1 : 0);
    }
    return false;
}

struct ProxyContactQ725 {
    uint64_t surfaceKey = 0u;
    float nx = 0.0f;
    float nz = 0.0f;
    float push = 0.0f;
    float contactY = 0.0f;
    float triangleTopY = 0.0f;
    uint32_t material = 0u;
    bool stairs = false;
    bool platform = false;
};

uint64_t gProxySweepLogsQ725 = 0u;
uint64_t gProxyStepLogsQ725 = 0u;
uint64_t gProxySlideLogsQ725 = 0u;
uint64_t gProxyBlockedLogsQ725 = 0u;
uint64_t gProxySupportLogsQ725 = 0u;

const ProxyContactQ725* FindProxySurfaceQ725(const std::vector<ProxyContactQ725>& contacts,
                                             uint64_t surfaceKey) {
    for (const ProxyContactQ725& contact : contacts) {
        if (contact.surfaceKey == surfaceKey) return &contact;
    }
    return nullptr;
}

void GatherProxyContactsQ725(float x, float z, float feetY,
                             std::vector<ProxyContactQ725>& out,
                             size_t* outRawTriangles = nullptr) {
    out.clear();
    if (outRawTriangles) *outRawTriangles = 0u;
    const float collisionRadius = CollisionRadiusQ712();
    const float radius2 = collisionRadius * collisionRadius;
    const float lowerY = feetY + collisionRadius;
    const float upperY = feetY + PLAYER_HEIGHT - collisionRadius;
    std::unordered_map<uint64_t, size_t> slotBySurface;
    slotBySurface.reserve(12u);

    for (const CollisionTriangle& tri : gWorldTriangles) {
        if (std::fabs(tri.normal.y) >= WalkableNormalThresholdQ714(tri)) continue;
        if (tri.maxY < feetY - PLAYER_SKIN || tri.minY > feetY + PLAYER_HEIGHT + PLAYER_SKIN) continue;
        if (x < tri.minX - collisionRadius || x > tri.maxX + collisionRadius ||
            z < tri.minZ - collisionRadius || z > tri.maxZ + collisionRadius) continue;

        const Vec3 segA{x, lowerY, z};
        const Vec3 segB{x, upperY, z};
        Vec3 capsulePoint;
        Vec3 trianglePoint;
        const float d2 = ClosestCapsuleSegmentTriangleQ719(segA, segB, tri,
                                                            capsulePoint, trianglePoint);
        if (!std::isfinite(d2) || d2 >= radius2) continue;
        if (outRawTriangles) ++(*outRawTriangles);

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
            if (horizontalLength < 1e-5f) continue;
            nx /= horizontalLength;
            nz /= horizontalLength;
            const float centroidX = (tri.a.x + tri.b.x + tri.c.x) / 3.0f;
            const float centroidZ = (tri.a.z + tri.b.z + tri.c.z) / 3.0f;
            if (nx * (x - centroidX) + nz * (z - centroidZ) < 0.0f) {
                nx = -nx;
                nz = -nz;
            }
        }

        ProxyContactQ725 candidate;
        candidate.surfaceKey = tri.surfaceKeyQ714;
        candidate.nx = nx;
        candidate.nz = nz;
        candidate.push = (collisionRadius - distance) + PLAYER_SKIN;
        candidate.contactY = trianglePoint.y;
        candidate.triangleTopY = tri.maxY;
        candidate.material = tri.havokMaterialQ714;
        candidate.stairs = tri.stairsQ714;
        candidate.platform = tri.platformQ714;

        const auto found = slotBySurface.find(candidate.surfaceKey);
        if (found == slotBySurface.end()) {
            const size_t slot = out.size();
            slotBySurface.emplace(candidate.surfaceKey, slot);
            out.push_back(candidate);
        } else {
            ProxyContactQ725& existing = out[found->second];
            existing.stairs = existing.stairs || candidate.stairs;
            existing.platform = existing.platform || candidate.platform;
            if (candidate.push > existing.push) existing = candidate;
        }
    }

    std::sort(out.begin(), out.end(), [](const ProxyContactQ725& a, const ProxyContactQ725& b) {
        return a.push > b.push;
    });
}

bool ProxyCandidateAllowedQ725(float targetX, float targetZ, float feetY,
                               float moveX, float moveZ,
                               const std::vector<ProxyContactQ725>& baseline,
                               std::vector<ProxyContactQ725>& blockers,
                               size_t* outRawTriangles = nullptr,
                               size_t* outPersistent = nullptr) {
    std::vector<ProxyContactQ725> target;
    GatherProxyContactsQ725(targetX, targetZ, feetY, target, outRawTriangles);
    blockers.clear();
    if (outPersistent) *outPersistent = 0u;

    for (const ProxyContactQ725& contact : target) {
        const ProxyContactQ725* previous = FindProxySurfaceQ725(baseline, contact.surfaceKey);
        const float into = moveX * contact.nx + moveZ * contact.nz;
        if (previous) {
            const float depthDelta = contact.push - previous->push;
            if (depthDelta <= 0.0025f || into >= -Q718_DIRECTION_EPS) {
                if (outPersistent) ++(*outPersistent);
                continue;
            }
        } else if (into >= -Q718_DIRECTION_EPS && contact.push <= 0.012f) {
            if (outPersistent) ++(*outPersistent);
            continue;
        }
        blockers.push_back(contact);
    }
    return blockers.empty();
}

bool SweepProxyCapsuleQ725(float startX, float startZ,
                           float targetX, float targetZ,
                           float feetY,
                           float& outX, float& outZ,
                           std::vector<ProxyContactQ725>& outBlockers,
                           size_t* outRawTriangles = nullptr,
                           size_t* outPersistent = nullptr) {
    outX = startX;
    outZ = startZ;
    outBlockers.clear();
    if (outRawTriangles) *outRawTriangles = 0u;
    if (outPersistent) *outPersistent = 0u;

    const float dx = targetX - startX;
    const float dz = targetZ - startZ;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length < 0.000001f) return true;

    const float sampleStep = std::max(0.012f, CollisionRadiusQ712() * 0.10f);
    const int samples = std::clamp(static_cast<int>(std::ceil(length / sampleStep)), 1, 12);
    std::vector<ProxyContactQ725> baseline;
    GatherProxyContactsQ725(startX, startZ, feetY, baseline);

    float previousX = startX;
    float previousZ = startZ;
    for (int i = 1; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float x = startX + dx * t;
        const float z = startZ + dz * t;
        const float stepX = x - previousX;
        const float stepZ = z - previousZ;
        std::vector<ProxyContactQ725> blockers;
        size_t raw = 0u, persistent = 0u;
        if (!ProxyCandidateAllowedQ725(x, z, feetY, stepX, stepZ,
                                       baseline, blockers, &raw, &persistent)) {
            outX = previousX;
            outZ = previousZ;
            outBlockers = std::move(blockers);
            if (outRawTriangles) *outRawTriangles = raw;
            if (outPersistent) *outPersistent = persistent;
            return false;
        }
        previousX = x;
        previousZ = z;
        GatherProxyContactsQ725(previousX, previousZ, feetY, baseline);
    }

    outX = targetX;
    outZ = targetZ;
    return true;
}

bool FindSupportFootprintQ725(float x, float z, float referenceFeetY,
                              float& groundY) {
    const float r = CollisionRadiusQ712() * 0.58f;
    const float d = r * 0.70710678f;
    const std::array<Vec2, 9> probes{{
        {0.0f, 0.0f},
        { r, 0.0f}, {-r, 0.0f}, {0.0f, r}, {0.0f, -r},
        { d, d}, { d,-d}, {-d, d}, {-d,-d}
    }};

    bool found = false;
    float bestY = referenceFeetY;
    float bestDistance = std::numeric_limits<float>::max();
    for (const CollisionTriangle& tri : gWorldTriangles) {
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
            if (delta > StepHeightQ78B() + PLAYER_SKIN || delta < -MAX_GROUND_DROP) continue;
            const float distance = std::fabs(delta);
            if (!found || distance < bestDistance - 0.0005f ||
                (std::fabs(distance - bestDistance) <= 0.0005f && y > bestY)) {
                found = true;
                bestDistance = distance;
                bestY = y;
            }
        }
    }
    if (found) groundY = bestY;
    return found;
}

bool TryProxyStepQ725(float startX, float startZ,
                      float targetX, float targetZ,
                      float feetY,
                      const std::vector<ProxyContactQ725>& blockers,
                      float& outFeetY) {
    if (blockers.empty()) return false;
    const float maxStep = StepHeightQ78B();

    float highestLocalContact = -1e30f;
    for (const ProxyContactQ725& blocker : blockers) {
        highestLocalContact = std::max(highestLocalContact, blocker.contactY);
    }
    const float contactRise = highestLocalContact - feetY;
    if (contactRise > maxStep + PLAYER_SKIN + 0.035f) return false;

    const float raisedFeetY = feetY + maxStep + PLAYER_SKIN;
    if (HasOverheadBlock(startX, startZ, raisedFeetY)) return false;

    float raisedX = startX, raisedZ = startZ;
    std::vector<ProxyContactQ725> raisedBlockers;
    size_t raisedRaw = 0u, raisedPersistent = 0u;
    if (!SweepProxyCapsuleQ725(startX, startZ, targetX, targetZ, raisedFeetY,
                               raisedX, raisedZ, raisedBlockers,
                               &raisedRaw, &raisedPersistent)) return false;

    float landingY = feetY;
    if (!FindSupportFootprintQ725(targetX, targetZ, feetY, landingY)) return false;
    const float rise = landingY - feetY;
    if (rise < -0.14f || rise > maxStep + PLAYER_SKIN) return false;
    if (HasOverheadBlock(targetX, targetZ, landingY)) return false;

    outFeetY = landingY;
    if (gProxyStepLogsQ725 < 120u || (gResolveCounter % 240u) == 0u) {
        ++gProxyStepLogsQ725;
        Q6G_LOGI("Q9.4 PROXY STEP: from=(%.3f %.3f) to=(%.3f %.3f) contactRise=%.3f landingRise=%.3f raisedRaw=%zu raisedPersistent=%zu blockers=%zu clear=1",
                 startX, startZ, targetX, targetZ, contactRise, rise,
                 raisedRaw, raisedPersistent, blockers.size());
    }
    return true;
}

Vec2 SolveProxyConstraintsQ725(Vec2 requested,
                               const std::vector<ProxyContactQ725>& contacts,
                               size_t& usedNormals) {
    struct Normal2Q725 { float x = 0.0f, z = 0.0f; };
    std::vector<Normal2Q725> normals;
    normals.reserve(std::min<size_t>(contacts.size(), 6u));
    for (const ProxyContactQ725& contact : contacts) {
        if (requested.x * contact.nx + requested.y * contact.nz >= -Q718_DIRECTION_EPS) continue;
        bool duplicate = false;
        for (const Normal2Q725& n : normals) {
            if (n.x * contact.nx + n.z * contact.nz > 0.985f) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) normals.push_back({contact.nx, contact.nz});
        if (normals.size() >= 4u) break;
    }

    Vec2 solved = requested;
    for (int pass = 0; pass < 5; ++pass) {
        bool changed = false;
        for (const Normal2Q725& n : normals) {
            const float into = solved.x * n.x + solved.y * n.z;
            if (into < 0.0f) {
                solved.x -= n.x * into;
                solved.y -= n.z * into;
                changed = true;
            }
        }
        if (!changed) break;
    }
    usedNormals = normals.size();
    return solved;
}

bool MoveExteriorSubstepQ725(float& x, float& z, float& feetY,
                             float moveX, float moveZ, bool& steppedThisFrame,
                             uint32_t& contactSurfaces) {
    if (moveX * moveX + moveZ * moveZ < 1e-10f) return true;

    const float startX = x;
    const float startZ = z;
    const float targetX = x + moveX;
    const float targetZ = z + moveZ;
    float sweptX = x, sweptZ = z;
    std::vector<ProxyContactQ725> blockers;
    size_t raw = 0u, persistent = 0u;

    if (SweepProxyCapsuleQ725(x, z, targetX, targetZ, feetY,
                              sweptX, sweptZ, blockers, &raw, &persistent)) {
        x = targetX;
        z = targetZ;
        return true;
    }

    // Preserve all collision-free progress up to first contact.
    x = sweptX;
    z = sweptZ;
    contactSurfaces += static_cast<uint32_t>(blockers.size());

    float stepFeetY = feetY;
    if (TryProxyStepQ725(x, z, targetX, targetZ, feetY, blockers, stepFeetY)) {
        x = targetX;
        z = targetZ;
        feetY = stepFeetY;
        steppedThisFrame = true;
        return true;
    }

    // Havok-style proxy response: solve the remaining desired velocity against
    // the whole contact manifold, then sweep the constrained displacement.
    for (int iteration = 0; iteration < 4; ++iteration) {
        const Vec2 remaining{targetX - x, targetZ - z};
        if (remaining.x * remaining.x + remaining.y * remaining.y < 0.00000025f) return true;

        size_t usedNormals = 0u;
        const Vec2 constrained = SolveProxyConstraintsQ725(remaining, blockers, usedNormals);
        if (constrained.x * constrained.x + constrained.y * constrained.y < 0.00000025f) break;

        const float slideTargetX = x + constrained.x;
        const float slideTargetZ = z + constrained.y;
        float slideX = x, slideZ = z;
        std::vector<ProxyContactQ725> slideBlockers;
        size_t slideRaw = 0u, slidePersistent = 0u;
        if (SweepProxyCapsuleQ725(x, z, slideTargetX, slideTargetZ, feetY,
                                  slideX, slideZ, slideBlockers,
                                  &slideRaw, &slidePersistent)) {
            if (gProxySlideLogsQ725 < 120u || (gResolveCounter % 240u) == 0u) {
                ++gProxySlideLogsQ725;
                Q6G_LOGI("Q9.4 PROXY SLIDE: from=(%.3f %.3f) requested=(%.3f %.3f) constrained=(%.3f %.3f) normals=%zu persistent=%zu",
                         x, z, remaining.x, remaining.y,
                         constrained.x, constrained.y, usedNormals, slidePersistent);
            }
            x = slideTargetX;
            z = slideTargetZ;
            return true;
        }

        x = slideX;
        z = slideZ;
        contactSurfaces += static_cast<uint32_t>(slideBlockers.size());

        float slideStepY = feetY;
        if (TryProxyStepQ725(x, z, slideTargetX, slideTargetZ,
                             feetY, slideBlockers, slideStepY)) {
            x = slideTargetX;
            z = slideTargetZ;
            feetY = slideStepY;
            steppedThisFrame = true;
            return true;
        }

        blockers.swap(slideBlockers);
        raw = slideRaw;
        persistent = slidePersistent;
    }

    if (gProxyBlockedLogsQ725 < 120u || (gResolveCounter % 240u) == 0u) {
        ++gProxyBlockedLogsQ725;
        float highestContact = -1e30f;
        uint64_t firstSurface = 0u;
        uint32_t firstMaterial = 0u;
        for (const ProxyContactQ725& blocker : blockers) {
            highestContact = std::max(highestContact, blocker.contactY);
            if (firstSurface == 0u) {
                firstSurface = blocker.surfaceKey;
                firstMaterial = blocker.material;
            }
        }
        const auto sourceIt = gSurfaceSourcesQ722.find(firstSurface);
        if (sourceIt != gSurfaceSourcesQ722.end()) {
            Q6G_LOGI("Q9.4 PROXY BLOCKER SOURCE: surface=%llu ref=%08X EDID=%s model=%s",
                     static_cast<unsigned long long>(firstSurface),
                     sourceIt->second.refFormId,
                     sourceIt->second.editorId.empty() ? "<none>" : sourceIt->second.editorId.c_str(),
                     sourceIt->second.modelPath.c_str());
        }
        Q6G_LOGI("Q9.4 PROXY BLOCKED: start=(%.3f %.3f) pos=(%.3f %.3f) target=(%.3f %.3f) blockers=%zu raw=%zu persistent=%zu contactRise=%.3f surface=%llu material=%u",
                 startX, startZ, x, z, targetX, targetZ,
                 blockers.size(), raw, persistent,
                 highestContact > -1e20f ? highestContact - feetY : -999.0f,
                 static_cast<unsigned long long>(firstSurface), firstMaterial);
    }
    return (x - startX) * (x - startX) + (z - startZ) * (z - startZ) > 0.00000025f;
}

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

struct HkWeldEdgeKeyQ801 {
    uint64_t mesh = 0u;
    uint32_t a = 0u;
    uint32_t b = 0u;
    bool operator==(const HkWeldEdgeKeyQ801& other) const {
        return mesh == other.mesh && a == other.a && b == other.b;
    }
};

struct HkWeldEdgeHashQ801 {
    size_t operator()(const HkWeldEdgeKeyQ801& key) const {
        uint64_t h = key.mesh ^ (static_cast<uint64_t>(key.a) << 32u) ^ key.b;
        h ^= h >> 33u; h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33u; h *= 0xc4ceb9fe1a85ec53ULL;
        h ^= h >> 33u;
        return static_cast<size_t>(h);
    }
};

std::vector<std::array<int32_t, 3>> gHkWeldNeighboursQ801;
bool gHkWeldAdjacencyReadyQ801 = false;
size_t gHkWeldTriangleCountQ801 = 0u;
const CollisionTriangle* gHkWeldTriangleDataQ801 = nullptr;
uint64_t gHkWeldPassLogsQ801 = 0u;
uint64_t gHkWeldReadyLogsQ801 = 0u;

void EnsureHkWeldAdjacencyQ801() {
    const CollisionTriangle* dataPtr = gWorldTriangles.empty() ? nullptr : gWorldTriangles.data();
    if (gHkWeldAdjacencyReadyQ801 &&
        gHkWeldTriangleCountQ801 == gWorldTriangles.size() &&
        gHkWeldTriangleDataQ801 == dataPtr) return;

    gHkWeldNeighboursQ801.assign(gWorldTriangles.size(), std::array<int32_t,3>{-1,-1,-1});
    struct EdgeOwnerQ801 { size_t tri = 0u; int edge = 0; };
    std::unordered_map<HkWeldEdgeKeyQ801, EdgeOwnerQ801, HkWeldEdgeHashQ801> first;
    first.reserve(gWorldTriangles.size() * 2u);

    size_t authoredWeldTriangles = 0u;
    size_t sharedEdges = 0u;
    for (size_t triIndex = 0u; triIndex < gWorldTriangles.size(); ++triIndex) {
        const CollisionTriangle& tri = gWorldTriangles[triIndex];
        if (tri.weldingInfoQ801 != 0u) ++authoredWeldTriangles;
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
                first.emplace(key, EdgeOwnerQ801{triIndex, edge});
            } else {
                const EdgeOwnerQ801 owner = found->second;
                if (gHkWeldNeighboursQ801[owner.tri][owner.edge] < 0 &&
                    gHkWeldNeighboursQ801[triIndex][edge] < 0) {
                    gHkWeldNeighboursQ801[owner.tri][owner.edge] = static_cast<int32_t>(triIndex);
                    gHkWeldNeighboursQ801[triIndex][edge] = static_cast<int32_t>(owner.tri);
                    ++sharedEdges;
                }
            }
        }
    }

    gHkWeldAdjacencyReadyQ801 = true;
    gHkWeldTriangleCountQ801 = gWorldTriangles.size();
    gHkWeldTriangleDataQ801 = dataPtr;
    if (gHkWeldReadyLogsQ801 < 8u) {
        ++gHkWeldReadyLogsQ801;
        Q6G_LOGI("Q9.4 HAVOK WELD READY: triangles=%zu authoredWeldTriangles=%zu sharedEdges=%zu mode=packed-welding+exact-vertex-adjacency",
                 gWorldTriangles.size(), authoredWeldTriangles, sharedEdges);
    }
}

float HkPointSegmentDistanceQ801(float px, float py, float pz,
                                 const Vec3& a, const Vec3& b) {
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float abz = b.z - a.z;
    const float denom = abx*abx + aby*aby + abz*abz;
    if (denom <= 1e-12f) {
        const float dx = px - a.x, dy = py - a.y, dz = pz - a.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    float t = ((px-a.x)*abx + (py-a.y)*aby + (pz-a.z)*abz) / denom;
    t = std::clamp(t, 0.0f, 1.0f);
    const float qx = a.x + abx*t;
    const float qy = a.y + aby*t;
    const float qz = a.z + abz*t;
    const float dx = px-qx, dy = py-qy, dz = pz-qz;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

bool HkSuppressAuthoredWeldGhostQ801(const HkProxyContactQ800& contact,
                                     float feetY) {
    if (contact.triangleIndex >= gWorldTriangles.size()) return false;
    EnsureHkWeldAdjacencyQ801();
    if (contact.triangleIndex >= gHkWeldNeighboursQ801.size()) return false;

    const CollisionTriangle& tri = gWorldTriangles[contact.triangleIndex];
    if (std::fabs(tri.normal.y) >= WalkableNormalThresholdQ714(tri)) return false;

    const std::array<std::pair<Vec3,Vec3>,3> edges{{
        {tri.a, tri.b}, {tri.b, tri.c}, {tri.c, tri.a}
    }};
    const float edgeTolerance = std::max(0.018f, Q800_KEEP_CONTACT_TOLERANCE * 0.90f);
    const float maxReachY = feetY + StepHeightQ78B() + CollisionRadiusQ712() * 0.25f;
    const float minReachY = feetY - 0.20f;

    for (int edge = 0; edge < 3; ++edge) {
        const int32_t neighbourIndex = gHkWeldNeighboursQ801[contact.triangleIndex][edge];
        if (neighbourIndex < 0 || static_cast<size_t>(neighbourIndex) >= gWorldTriangles.size()) continue;
        const CollisionTriangle& neighbour = gWorldTriangles[static_cast<size_t>(neighbourIndex)];

        // This is the key OG-data gate: no authored welding on either side means
        // this is a normal mesh boundary and must remain solid.
        if (tri.weldingInfoQ801 == 0u && neighbour.weldingInfoQ801 == 0u) continue;
        if (std::fabs(neighbour.normal.y) < WalkableNormalThresholdQ714(neighbour)) continue;

        const float edgeDistance = HkPointSegmentDistanceQ801(
            contact.pointX, contact.pointY, contact.pointZ,
            edges[edge].first, edges[edge].second);
        if (edgeDistance > edgeTolerance) continue;

        const float edgeMinY = std::min(edges[edge].first.y, edges[edge].second.y);
        const float edgeMaxY = std::max(edges[edge].first.y, edges[edge].second.y);
        if (edgeMaxY > maxReachY || edgeMaxY < minReachY) continue;

        if (gHkWeldPassLogsQ801 < 160u || (gResolveCounter % 240u) == 0u) {
            ++gHkWeldPassLogsQ801;
            Q6G_LOGI("Q9.4 HAVOK WELD EDGE PASS: tri=%zu neighbour=%d weld=0x%04X neighbourWeld=0x%04X edge=%d edgeDist=%.4f edgeRise=(%.3f..%.3f) neighbourNormalY=%.3f",
                     contact.triangleIndex, neighbourIndex,
                     static_cast<unsigned>(tri.weldingInfoQ801),
                     static_cast<unsigned>(neighbour.weldingInfoQ801),
                     edge, edgeDistance, edgeMinY-feetY, edgeMaxY-feetY,
                     neighbour.normal.y);
        }
        return true;
    }
    return false;
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

        // Q9.4: apply Fallout's authored packed-triangle welding before this
        // contact becomes a proxy constraint. This removes ghost side/end-edge
        // planes while leaving unwelded/tall boundaries solid.
        if (HkSuppressAuthoredWeldGhostQ801(candidate, feetY)) continue;

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
        Q6G_LOGI("Q9.4 HAVOK MANIFOLD: pos=(%.3f %.3f) planes=%zu keepDistance=%.3f keepTolerance=%.3f",
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
        Q6G_LOGI("Q9.4 HAVOK STEP: from=(%.3f %.3f) target=(%.3f %.3f) landingRise=%.3f supportTri=%zu raisedClear=%d raisedBlockers=%zu hardFinal=%zu",
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

struct HkLowLandingQ802 {
    bool found = false;
    float y = 0.0f;
    float probeX = 0.0f;
    float probeZ = 0.0f;
    size_t triangleIndex = 0u;
    int supportPoints = 0;
};

uint64_t gHkLowPassLogsQ802 = 0u;
uint64_t gHkLowNoLandingLogsQ802 = 0u;
uint64_t gHkLowCapsuleBlockedLogsQ802 = 0u;

bool HkWalkableSupportPointQ802(float x, float z, float targetY,
                                float toleranceY) {
    for (const CollisionTriangle& tri : gWorldTriangles) {
        if (std::fabs(tri.normal.y) < WalkableNormalThresholdQ714(tri)) continue;
        if (x < tri.minX - 0.006f || x > tri.maxX + 0.006f ||
            z < tri.minZ - 0.006f || z > tri.maxZ + 0.006f) continue;
        float u = 0.0f, v = 0.0f, w = 0.0f;
        if (!BarycentricXZ(tri, x, z, u, v, w)) continue;
        const float y = tri.a.y * u + tri.b.y * v + tri.c.y * w;
        if (std::fabs(y - targetY) <= toleranceY) return true;
    }
    return false;
}

int HkLandingFootprintSupportQ802(float x, float z, float landingY,
                                  float moveNx, float moveNz) {
    const float r = CollisionRadiusQ712();
    const float lateralX = -moveNz;
    const float lateralZ = moveNx;
    const float side = r * 0.38f;
    const float fore = r * 0.30f;
    const std::array<Vec2, 5> probes{{
        {0.0f, 0.0f},
        { lateralX * side, lateralZ * side},
        {-lateralX * side,-lateralZ * side},
        { moveNx * fore,  moveNz * fore},
        {-moveNx * fore, -moveNz * fore}
    }};
    int count = 0;
    for (const Vec2& probe : probes) {
        if (HkWalkableSupportPointQ802(x + probe.x, z + probe.y,
                                       landingY, 0.055f)) {
            ++count;
        }
    }
    return count;
}

HkLowLandingQ802 FindHkLowLandingQ802(float startX, float startZ,
                                       float targetX, float targetZ,
                                       float feetY, bool preferRaisedLanding) {
    HkLowLandingQ802 result;
    const float dx = targetX - startX;
    const float dz = targetZ - startZ;
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1e-6f) return result;
    const float nx = dx / len;
    const float nz = dz / len;
    const float r = CollisionRadiusQ712();

    // Probe from the requested endpoint forward far enough that a capsule can
    // actually get its centre past a thin riser/lip. We still move only to the
    // requested endpoint; these points are used solely to prove landing exists.
    const std::array<float, 7> ahead{{
        0.0f, r * 0.25f, r * 0.50f, r * 0.75f,
        r * 1.00f, r * 1.25f, r * 1.50f
    }};

    for (float distance : ahead) {
        const float px = targetX + nx * distance;
        const float pz = targetZ + nz * distance;
        const HkSupportQ800 support = FindHkSupportAtQ800(
            px, pz, feetY,
            StepHeightQ78B() + PLAYER_SKIN + 0.008f,
            0.18f);
        if (!support.found) continue;
        const float rise = support.y - feetY;
        if (rise > StepHeightQ78B() + PLAYER_SKIN + 0.008f || rise < -0.18f) continue;

        // If we struck a real positive riser, do not accidentally select the
        // old floor behind the capsule just because it is closer in height.
        if (preferRaisedLanding && rise < 0.008f) continue;

        const int supportPoints = HkLandingFootprintSupportQ802(
            px, pz, support.y, nx, nz);
        if (supportPoints < 2) continue;

        result.found = true;
        result.y = support.y;
        result.probeX = px;
        result.probeZ = pz;
        result.triangleIndex = support.triangleIndex;
        result.supportPoints = supportPoints;
        return result;
    }
    return result;
}

HkLowLandingQ802 FindHkLowLandingQ803(float startX, float startZ,
                                       float targetX, float targetZ,
                                       float feetY, bool preferRaisedLanding) {
    HkLowLandingQ802 result;
    const float dx = targetX - startX;
    const float dz = targetZ - startZ;
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1e-6f) return result;
    const float nx = dx / len;
    const float nz = dz / len;
    const float r = CollisionRadiusQ712();
    const float maxRise = StepHeightQ78B() + PLAYER_SKIN + 0.008f;
    const float maxDrop = 0.18f;

    // Search from the requested endpoint to 1.5 capsule radii beyond it. At
    // each probe, enumerate ALL walkable surfaces instead of asking the generic
    // support query to collapse them to a single closest-height winner.
    const std::array<float, 7> ahead{{
        0.0f, r * 0.25f, r * 0.50f, r * 0.75f,
        r * 1.00f, r * 1.25f, r * 1.50f
    }};

    for (float distance : ahead) {
        const float px = targetX + nx * distance;
        const float pz = targetZ + nz * distance;

        HkLowLandingQ802 bestAtProbe;
        float bestScore = std::numeric_limits<float>::max();

        for (size_t triIndex = 0u; triIndex < gWorldTriangles.size(); ++triIndex) {
            const CollisionTriangle& tri = gWorldTriangles[triIndex];
            if (std::fabs(tri.normal.y) < WalkableNormalThresholdQ714(tri)) continue;
            if (px < tri.minX - 0.006f || px > tri.maxX + 0.006f ||
                pz < tri.minZ - 0.006f || pz > tri.maxZ + 0.006f) continue;

            float u = 0.0f, v = 0.0f, w = 0.0f;
            if (!BarycentricXZ(tri, px, pz, u, v, w)) continue;
            const float y = tri.a.y * u + tri.b.y * v + tri.c.y * w;
            const float rise = y - feetY;
            if (rise > maxRise || rise < -maxDrop) continue;

            // Positive blocker: old floor is not a valid answer. Consider every
            // raised tread in range, then choose the smallest legal positive
            // rise at the nearest forward probe.
            if (preferRaisedLanding && rise < 0.008f) continue;

            const int supportPoints = HkLandingFootprintSupportQ802(
                px, pz, y, nx, nz);
            if (supportPoints < 2) continue;

            const float score = preferRaisedLanding ? rise : std::fabs(rise);
            if (!bestAtProbe.found || score < bestScore - 0.0005f ||
                (std::fabs(score - bestScore) <= 0.0005f &&
                 supportPoints > bestAtProbe.supportPoints)) {
                bestAtProbe.found = true;
                bestAtProbe.y = y;
                bestAtProbe.probeX = px;
                bestAtProbe.probeZ = pz;
                bestAtProbe.triangleIndex = triIndex;
                bestAtProbe.supportPoints = supportPoints;
                bestScore = score;
            }
        }

        if (bestAtProbe.found) {
            Q6G_LOGI("Q9.4 LOW OBSTACLE LANDING SELECT: probeAhead=%.3f rise=%.3f supportPoints=%d supportTri=%zu preferRaised=%d",
                     distance, bestAtProbe.y - feetY, bestAtProbe.supportPoints,
                     bestAtProbe.triangleIndex, preferRaisedLanding ? 1 : 0);
            return bestAtProbe;
        }
    }
    return result;
}

struct HkCoherentShapeQ900 {
    uint64_t key = 0u;
    std::vector<size_t> triangles;
    float minX = 0.0f, maxX = 0.0f;
    float minY = 0.0f, maxY = 0.0f;
    float minZ = 0.0f, maxZ = 0.0f;
    size_t walkableTriangles = 0u;
};

struct HkShapeLandingQ900 {
    bool found = false;
    uint64_t shapeKey = 0u;
    size_t triangleIndex = 0u;
    float y = 0.0f;
    float probeX = 0.0f;
    float probeZ = 0.0f;
    float probeAhead = 0.0f;
    int supportPoints = 0;
};

std::vector<HkCoherentShapeQ900> gHkShapesQ900;
std::unordered_map<uint64_t, size_t> gHkShapeIndexQ900;
const CollisionTriangle* gHkShapeTriangleDataQ900 = nullptr;
size_t gHkShapeTriangleCountQ900 = 0u;
bool gHkShapesReadyQ900 = false;
uint64_t gHkShapeReadyLogsQ900 = 0u;
uint64_t gHkShapeStepLogsQ900 = 0u;
uint64_t gHkShapeNoTopLogsQ900 = 0u;
uint64_t gHkShapeBlockedLogsQ900 = 0u;

void EnsureHkCoherentShapesQ900() {
    const CollisionTriangle* data = gWorldTriangles.empty() ? nullptr : gWorldTriangles.data();
    if (gHkShapesReadyQ900 && data == gHkShapeTriangleDataQ900 &&
        gHkShapeTriangleCountQ900 == gWorldTriangles.size()) return;

    gHkShapesQ900.clear();
    gHkShapeIndexQ900.clear();
    gHkShapeIndexQ900.reserve(gWorldTriangles.size() / 8u + 32u);

    size_t shapedTriangles = 0u;
    size_t walkableTriangles = 0u;
    for (size_t triIndex = 0u; triIndex < gWorldTriangles.size(); ++triIndex) {
        const CollisionTriangle& tri = gWorldTriangles[triIndex];
        if (tri.meshKeyQ801 == 0u) continue; // LAND/legacy geometry stays on existing path.
        ++shapedTriangles;

        size_t shapeIndex = 0u;
        const auto existing = gHkShapeIndexQ900.find(tri.meshKeyQ801);
        if (existing == gHkShapeIndexQ900.end()) {
            shapeIndex = gHkShapesQ900.size();
            HkCoherentShapeQ900 shape;
            shape.key = tri.meshKeyQ801;
            shape.minX = tri.minX; shape.maxX = tri.maxX;
            shape.minY = tri.minY; shape.maxY = tri.maxY;
            shape.minZ = tri.minZ; shape.maxZ = tri.maxZ;
            gHkShapesQ900.push_back(std::move(shape));
            gHkShapeIndexQ900.emplace(tri.meshKeyQ801, shapeIndex);
        } else {
            shapeIndex = existing->second;
        }

        HkCoherentShapeQ900& shape = gHkShapesQ900[shapeIndex];
        shape.triangles.push_back(triIndex);
        shape.minX = std::min(shape.minX, tri.minX); shape.maxX = std::max(shape.maxX, tri.maxX);
        shape.minY = std::min(shape.minY, tri.minY); shape.maxY = std::max(shape.maxY, tri.maxY);
        shape.minZ = std::min(shape.minZ, tri.minZ); shape.maxZ = std::max(shape.maxZ, tri.maxZ);
        if (std::fabs(tri.normal.y) >= WalkableNormalThresholdQ714(tri)) {
            ++shape.walkableTriangles;
            ++walkableTriangles;
        }
    }

    gHkShapeTriangleDataQ900 = data;
    gHkShapeTriangleCountQ900 = gWorldTriangles.size();
    gHkShapesReadyQ900 = true;
    if (gHkShapeReadyLogsQ900 < 8u) {
        ++gHkShapeReadyLogsQ900;
        Q6G_LOGI("Q9.4 COHERENT SHAPES READY: shapes=%zu shapedTriangles=%zu walkableTriangles=%zu worldTriangles=%zu mode=placed-bhk-shape+concave-mesh",
                 gHkShapesQ900.size(), shapedTriangles, walkableTriangles,
                 gWorldTriangles.size());
    }
}

const HkCoherentShapeQ900* FindHkShapeQ900(uint64_t key) {
    if (key == 0u) return nullptr;
    EnsureHkCoherentShapesQ900();
    const auto found = gHkShapeIndexQ900.find(key);
    if (found == gHkShapeIndexQ900.end() || found->second >= gHkShapesQ900.size()) return nullptr;
    return &gHkShapesQ900[found->second];
}

bool HkShapeSurfaceAtQ900(const HkCoherentShapeQ900& shape,
                          float x, float z, float targetY,
                          float toleranceY) {
    for (size_t triIndex : shape.triangles) {
        if (triIndex >= gWorldTriangles.size()) continue;
        const CollisionTriangle& tri = gWorldTriangles[triIndex];
        if (std::fabs(tri.normal.y) < WalkableNormalThresholdQ714(tri)) continue;
        if (x < tri.minX - 0.006f || x > tri.maxX + 0.006f ||
            z < tri.minZ - 0.006f || z > tri.maxZ + 0.006f) continue;
        float u = 0.0f, v = 0.0f, w = 0.0f;
        if (!BarycentricXZ(tri, x, z, u, v, w)) continue;
        const float y = tri.a.y*u + tri.b.y*v + tri.c.y*w;
        if (std::fabs(y - targetY) <= toleranceY) return true;
    }
    return false;
}

int HkShapeFootprintSupportQ900(const HkCoherentShapeQ900& shape,
                                float x, float z, float y,
                                float nx, float nz) {
    const float r = CollisionRadiusQ712();
    const float lx = -nz, lz = nx;
    const float side = r * 0.40f;
    const float fore = r * 0.32f;
    const std::array<Vec2,5> probes{{
        {0.0f, 0.0f},
        { lx*side, lz*side}, {-lx*side,-lz*side},
        { nx*fore, nz*fore}, {-nx*fore,-nz*fore}
    }};
    int support = 0;
    for (const Vec2& p : probes) {
        if (HkShapeSurfaceAtQ900(shape, x+p.x, z+p.y, y, 0.055f)) ++support;
    }
    return support;
}

HkShapeLandingQ900 FindHkShapeLandingOnQ900(const HkCoherentShapeQ900& shape,
                                             float startX, float startZ,
                                             float targetX, float targetZ,
                                             float feetY, float contactRise) {
    HkShapeLandingQ900 result;
    if (shape.walkableTriangles == 0u) return result;
    const float dx = targetX - startX;
    const float dz = targetZ - startZ;
    const float len = std::sqrt(dx*dx + dz*dz);
    if (len < 1e-6f) return result;
    const float nx = dx / len, nz = dz / len;
    const float r = CollisionRadiusQ712();
    const float maxRise = StepHeightQ78B() + PLAYER_SKIN + 0.020f;
    const std::array<float,6> ahead{{0.0f, r*0.25f, r*0.50f,
                                     r*0.75f, r*1.00f, r*1.10f}};

    for (float distance : ahead) {
        const float px = targetX + nx*distance;
        const float pz = targetZ + nz*distance;
        if (px < shape.minX - r || px > shape.maxX + r ||
            pz < shape.minZ - r || pz > shape.maxZ + r) continue;

        HkShapeLandingQ900 best;
        float bestScore = std::numeric_limits<float>::max();
        for (size_t triIndex : shape.triangles) {
            if (triIndex >= gWorldTriangles.size()) continue;
            const CollisionTriangle& tri = gWorldTriangles[triIndex];
            if (std::fabs(tri.normal.y) < WalkableNormalThresholdQ714(tri)) continue;
            if (px < tri.minX - 0.006f || px > tri.maxX + 0.006f ||
                pz < tri.minZ - 0.006f || pz > tri.maxZ + 0.006f) continue;
            float u = 0.0f, v = 0.0f, w = 0.0f;
            if (!BarycentricXZ(tri, px, pz, u, v, w)) continue;
            const float y = tri.a.y*u + tri.b.y*v + tri.c.y*w;
            const float rise = y - feetY;
            if (rise < 0.006f || rise > maxRise) continue;

            const int support = HkShapeFootprintSupportQ900(shape, px, pz, y, nx, nz);
            if (support < 2) continue;

            // Prefer the top surface whose elevation best matches the face that
            // actually blocked the capsule. This rejects the old floor while
            // naturally following ramps and stair treads on the same shape.
            const float desiredRise = std::clamp(contactRise, 0.006f, maxRise);
            const float score = std::fabs(rise - desiredRise) + distance * 0.08f;
            if (!best.found || score < bestScore - 0.0005f ||
                (std::fabs(score-bestScore) <= 0.0005f && support > best.supportPoints)) {
                best.found = true;
                best.shapeKey = shape.key;
                best.triangleIndex = triIndex;
                best.y = y;
                best.probeX = px; best.probeZ = pz;
                best.probeAhead = distance;
                best.supportPoints = support;
                bestScore = score;
            }
        }
        if (best.found) return best; // nearest probe that reaches the top wins.
    }
    return result;
}

bool HkShapeAwareLandingClearQ900(float targetX, float targetZ, float landingY,
                                  uint64_t supportShapeKey,
                                  const std::vector<uint64_t>& blockerShapeKeys,
                                  std::string& reason) {
    std::vector<HkProxyContactQ800> contacts;
    GatherHkProxyContactsQ800(targetX, targetZ, landingY,
                              Q800_KEEP_DISTANCE, contacts);
    const float lowFaceAllowance = 0.075f;
    const float upperBodyY = landingY + PLAYER_HEIGHT * 0.58f;

    auto isBlockerShape = [&](uint64_t key) {
        if (key == supportShapeKey) return true;
        return std::find(blockerShapeKeys.begin(), blockerShapeKeys.end(), key) != blockerShapeKeys.end();
    };

    for (const HkProxyContactQ800& contact : contacts) {
        if (contact.triangleIndex >= gWorldTriangles.size()) continue;
        const CollisionTriangle& tri = gWorldTriangles[contact.triangleIndex];

        // Any contact in the upper body/head region remains solid, including a
        // horizontal ceiling whose normal would otherwise look "walkable".
        if (contact.pointY >= upperBodyY) {
            reason = "upper-body";
            return false;
        }

        if (HkContactIsWalkableQ800(contact)) continue;

        // The riser/end face belonging to the shape we are climbing is below
        // the newly selected support plane, so it is an internal boundary of
        // the coherent collider, not a wall at the new character height.
        if (isBlockerShape(tri.meshKeyQ801)) {
            if (contact.pointY <= landingY + lowFaceAllowance ||
                tri.maxY <= landingY + lowFaceAllowance) continue;
        }

        // A floor lip wholly below the new support plane cannot intersect the
        // character after the support transfer.
        if (tri.maxY <= landingY + PLAYER_SKIN + 0.010f) continue;

        reason = "hard-shape";
        return false;
    }
    return true;
}

bool TryHkCoherentShapeLedgeQ900(float startX, float startZ,
                                 float targetX, float targetZ,
                                 float feetY,
                                 const std::vector<HkProxyContactQ800>& blockers,
                                 float& outFeetY) {
    if (blockers.empty()) return false;
    EnsureHkCoherentShapesQ900();

    float highestRise = -std::numeric_limits<float>::max();
    std::vector<uint64_t> blockerShapeKeys;
    blockerShapeKeys.reserve(blockers.size());
    for (const HkProxyContactQ800& blocker : blockers) {
        highestRise = std::max(highestRise, blocker.pointY - feetY);
        if (blocker.triangleIndex >= gWorldTriangles.size()) continue;
        const uint64_t key = gWorldTriangles[blocker.triangleIndex].meshKeyQ801;
        if (key != 0u && std::find(blockerShapeKeys.begin(), blockerShapeKeys.end(), key) == blockerShapeKeys.end()) {
            blockerShapeKeys.push_back(key);
        }
    }
    if (highestRise > StepHeightQ78B() + PLAYER_SKIN + 0.020f) return false;
    if (blockerShapeKeys.empty()) return false;

    HkShapeLandingQ900 landing;
    float bestError = std::numeric_limits<float>::max();
    for (uint64_t key : blockerShapeKeys) {
        const HkCoherentShapeQ900* shape = FindHkShapeQ900(key);
        if (!shape) continue;
        HkShapeLandingQ900 candidate = FindHkShapeLandingOnQ900(
            *shape, startX, startZ, targetX, targetZ, feetY, highestRise);
        if (!candidate.found) continue;
        const float error = std::fabs((candidate.y-feetY) - highestRise) + candidate.probeAhead*0.08f;
        if (!landing.found || error < bestError) {
            landing = candidate;
            bestError = error;
        }
    }

    // Adjacent modular pieces are separate placed Havok shapes. If the blocker
    // shape has no top at the forward probe (e.g. ramp -> next ramp), search
    // nearby coherent shapes, still as complete shapes rather than world-triangle soup.
    if (!landing.found) {
        const float dx = targetX-startX, dz = targetZ-startZ;
        const float len = std::sqrt(dx*dx + dz*dz);
        if (len > 1e-6f) {
            const float nx=dx/len, nz=dz/len, r=CollisionRadiusQ712();
            const float px=targetX+nx*r*0.80f, pz=targetZ+nz*r*0.80f;
            for (const HkCoherentShapeQ900& shape : gHkShapesQ900) {
                if (std::find(blockerShapeKeys.begin(), blockerShapeKeys.end(), shape.key) != blockerShapeKeys.end()) continue;
                if (shape.walkableTriangles == 0u) continue;
                if (px < shape.minX-r || px > shape.maxX+r || pz < shape.minZ-r || pz > shape.maxZ+r) continue;
                if (shape.maxY < feetY-0.18f || shape.minY > feetY+StepHeightQ78B()+0.08f) continue;
                HkShapeLandingQ900 candidate = FindHkShapeLandingOnQ900(
                    shape, startX, startZ, targetX, targetZ, feetY, highestRise);
                if (!candidate.found) continue;
                const float error=std::fabs((candidate.y-feetY)-highestRise)+candidate.probeAhead*0.08f+0.02f;
                if (!landing.found || error<bestError) { landing=candidate; bestError=error; }
            }
        }
    }

    if (!landing.found) {
        if (gHkShapeNoTopLogsQ900 < 120u || (gResolveCounter % 240u) == 0u) {
            ++gHkShapeNoTopLogsQ900;
            Q6G_LOGI("Q9.4 SHAPE LEDGE NO TOP: from=(%.3f %.3f) target=(%.3f %.3f) contactRise=%.3f blockerShapes=%zu",
                     startX,startZ,targetX,targetZ,highestRise,blockerShapeKeys.size());
        }
        return false;
    }

    std::string clearReason;
    if (!HkShapeAwareLandingClearQ900(targetX,targetZ,landing.y,
                                      landing.shapeKey,blockerShapeKeys,clearReason)) {
        if (gHkShapeBlockedLogsQ900 < 120u || (gResolveCounter % 240u) == 0u) {
            ++gHkShapeBlockedLogsQ900;
            Q6G_LOGI("Q9.4 SHAPE LEDGE BLOCKED: reason=%s shape=%llu landingRise=%.3f probeAhead=%.3f support=%d",
                     clearReason.c_str(), static_cast<unsigned long long>(landing.shapeKey),
                     landing.y-feetY, landing.probeAhead, landing.supportPoints);
        }
        return false;
    }

    outFeetY = landing.y;
    if (gHkShapeStepLogsQ900 < 180u || (gResolveCounter % 240u) == 0u) {
        ++gHkShapeStepLogsQ900;
        Q6G_LOGI("Q9.4 SHAPE LEDGE CLIMB: shape=%llu contactRise=%.3f landingRise=%.3f probeAhead=%.3f support=%d tri=%zu",
                 static_cast<unsigned long long>(landing.shapeKey), highestRise,
                 landing.y-feetY, landing.probeAhead, landing.supportPoints,
                 landing.triangleIndex);
    }
    return true;
}

bool TryHkLowObstacleTraverseQ802(float startX, float startZ,
                                  float targetX, float targetZ,
                                  float feetY,
                                  const std::vector<HkProxyContactQ800>& blockers,
                                  float& outFeetY) {
    if (blockers.empty()) return false;

    const float dx = targetX - startX;
    const float dz = targetZ - startZ;
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1e-6f) return false;
    const float nx = dx / len;
    const float nz = dz / len;

    const float lowLimit = StepHeightQ78B() + PLAYER_SKIN + 0.010f;
    float highestContactRise = -std::numeric_limits<float>::max();
    for (const HkProxyContactQ800& blocker : blockers) {
        const float rise = blocker.pointY - feetY;
        highestContactRise = std::max(highestContactRise, rise);
        if (rise > lowLimit) return false; // genuinely higher than a step
    }

    const bool preferRaisedLanding = highestContactRise > 0.030f;
    const HkLowLandingQ802 landing = FindHkLowLandingQ803(
        startX, startZ, targetX, targetZ, feetY, preferRaisedLanding);
    if (!landing.found) {
        if (gHkLowNoLandingLogsQ802 < 120u || (gResolveCounter % 240u) == 0u) {
            ++gHkLowNoLandingLogsQ802;
            Q6G_LOGI("Q9.4 LOW OBSTACLE NO LANDING: from=(%.3f %.3f) target=(%.3f %.3f) highestContactRise=%.3f maxStep=%.3f blockers=%zu",
                     startX, startZ, targetX, targetZ,
                     highestContactRise, StepHeightQ78B(), blockers.size());
        }
        return false;
    }

    // Lift the whole capsule above the maximum legal step and cast through and
    // slightly beyond the obstacle. A real wall/rail/bridge side that continues
    // upward still blocks here; a low riser/lip does not.
    const float raisedFeetY = feetY + StepHeightQ78B() + PLAYER_SKIN +
                              Q800_KEEP_DISTANCE + 0.006f;
    if (HasOverheadBlock(startX, startZ, raisedFeetY) ||
        HasOverheadBlock(targetX, targetZ, landing.y)) {
        if (gHkLowCapsuleBlockedLogsQ802 < 120u || (gResolveCounter % 240u) == 0u) {
            ++gHkLowCapsuleBlockedLogsQ802;
            Q6G_LOGI("Q9.4 LOW OBSTACLE CAPSULE BLOCKED: reason=overhead from=(%.3f %.3f) target=(%.3f %.3f) landingRise=%.3f",
                     startX, startZ, targetX, targetZ, landing.y - feetY);
        }
        return false;
    }

    const float castAhead = CollisionRadiusQ712() * 1.10f;
    const float raisedTargetX = targetX + nx * castAhead;
    const float raisedTargetZ = targetZ + nz * castAhead;
    float raisedX = startX;
    float raisedZ = startZ;
    std::vector<HkProxyContactQ800> raisedBlockers;
    size_t raisedRaw = 0u;
    const bool raisedClear = LinearCastHkProxyQ800(
        startX, startZ, raisedTargetX, raisedTargetZ,
        raisedFeetY, raisedX, raisedZ, raisedBlockers, &raisedRaw);
    if (!raisedClear) {
        if (gHkLowCapsuleBlockedLogsQ802 < 120u || (gResolveCounter % 240u) == 0u) {
            ++gHkLowCapsuleBlockedLogsQ802;
            Q6G_LOGI("Q9.4 LOW OBSTACLE CAPSULE BLOCKED: reason=raised-cast from=(%.3f %.3f) target=(%.3f %.3f) landingRise=%.3f raisedBlockers=%zu raw=%zu",
                     startX, startZ, targetX, targetZ, landing.y - feetY,
                     raisedBlockers.size(), raisedRaw);
        }
        return false;
    }

    outFeetY = landing.y;
    if (gHkLowPassLogsQ802 < 160u || (gResolveCounter % 240u) == 0u) {
        ++gHkLowPassLogsQ802;
        Q6G_LOGI("Q9.4 LOW OBSTACLE PASS: from=(%.3f %.3f) target=(%.3f %.3f) highestContactRise=%.3f landingRise=%.3f supportPoints=%d supportTri=%zu raisedClear=1",
                 startX, startZ, targetX, targetZ,
                 highestContactRise, landing.y - feetY,
                 landing.supportPoints, landing.triangleIndex);
    }
    return true;
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
    if (TryHkCoherentShapeLedgeQ900(x, z, targetX, targetZ, feetY, blockers, stepY)) {
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
                Q6G_LOGI("Q9.4 HAVOK SIMPLEX: from=(%.3f %.3f) requested=(%.3f %.3f) solved=(%.3f %.3f) planes=%zu",
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
        if (TryHkCoherentShapeLedgeQ900(x, z, slideTargetX, slideTargetZ,
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
            Q6G_LOGI("Q9.4 HAVOK BLOCKER SOURCE: surface=%llu ref=%08X EDID=%s model=%s",
                     static_cast<unsigned long long>(firstSurface),
                     sourceIt->second.refFormId,
                     sourceIt->second.editorId.empty() ? "<none>" : sourceIt->second.editorId.c_str(),
                     sourceIt->second.modelPath.c_str());
        }
        Q6G_LOGI("Q9.4 HAVOK BLOCKED: start=(%.3f %.3f) pos=(%.3f %.3f) target=(%.3f %.3f) blockers=%zu raw=%zu manifold=%zu",
                 startX, startZ, x, z, targetX, targetZ,
                 blockers.size(), raw, gHkManifoldQ800.size());
    }
    RefreshHkManifoldQ800(x, z, feetY);
    return (x - startX) * (x - startX) + (z - startZ) * (z - startZ) > 0.00000025f;
}

constexpr float Q910_SWEEP_SKIN = 0.004f;
constexpr float Q910_LANDING_TOLERANCE = 0.050f;
constexpr float Q910_MAX_STEP_DROP = 0.18f;
constexpr int Q910_CAST_BISECTIONS = 7;

struct CapsuleHitQ910 {
    bool hit = false;
    size_t triangleIndex = 0u;
    float pointX = 0.0f, pointY = 0.0f, pointZ = 0.0f;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
};

struct DownSupportQ910 {
    bool found = false;
    float y = 0.0f;
    int supportPoints = 0;
    size_t triangleIndex = 0u;
};

uint64_t gQ910ReadyLogs = 0u;
uint64_t gQ910StepLogs = 0u;
uint64_t gQ910StepFailLogs = 0u;
uint64_t gQ910SlideLogs = 0u;
uint64_t gQ910BlockedLogs = 0u;

bool IsWalkableTriangleQ910(const CollisionTriangle& tri) {
    return std::fabs(tri.normal.y) >= WalkableNormalThresholdQ714(tri);
}

bool CapsuleContactTriangleQ910(float x, float z, float feetY,
                                size_t triIndex, CapsuleHitQ910& out) {
    if (triIndex >= gWorldTriangles.size()) return false;
    const CollisionTriangle& tri = gWorldTriangles[triIndex];
    const float radius = CollisionRadiusQ712();
    const float queryRadius = radius + Q910_SWEEP_SKIN;
    if (tri.maxY < feetY - queryRadius ||
        tri.minY > feetY + PLAYER_HEIGHT + queryRadius) return false;
    if (x < tri.minX - queryRadius || x > tri.maxX + queryRadius ||
        z < tri.minZ - queryRadius || z > tri.maxZ + queryRadius) return false;

    const Vec3 segA{x, feetY + radius, z};
    const Vec3 segB{x, feetY + PLAYER_HEIGHT - radius, z};
    Vec3 capsulePoint;
    Vec3 trianglePoint;
    const float d2 = ClosestCapsuleSegmentTriangleQ719(segA, segB, tri,
                                                        capsulePoint, trianglePoint);
    if (!std::isfinite(d2) || d2 > queryRadius * queryRadius) return false;

    Vec3 separation = Q719Sub(capsulePoint, trianglePoint);
    float sepLen = std::sqrt(Q719LengthSq(separation));
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    if (sepLen > 1e-5f) {
        nx = separation.x / sepLen;
        ny = separation.y / sepLen;
        nz = separation.z / sepLen;
    } else {
        nx = tri.normal.x; ny = tri.normal.y; nz = tri.normal.z;
        const float cx = (tri.a.x + tri.b.x + tri.c.x) / 3.0f;
        const float cy = (tri.a.y + tri.b.y + tri.c.y) / 3.0f;
        const float cz = (tri.a.z + tri.b.z + tri.c.z) / 3.0f;
        const float toward = nx*(x-cx) + ny*((feetY+PLAYER_HEIGHT*0.5f)-cy) + nz*(z-cz);
        if (toward < 0.0f) { nx=-nx; ny=-ny; nz=-nz; }
    }

    out.hit = true;
    out.triangleIndex = triIndex;
    out.pointX = trianglePoint.x;
    out.pointY = trianglePoint.y;
    out.pointZ = trianglePoint.z;
    out.nx = nx; out.ny = ny; out.nz = nz;
    return true;
}

bool CapsulePositionBlockedQ910(float x, float z, float feetY,
                                float moveX, float moveZ,
                                CapsuleHitQ910& outHit) {
    outHit = CapsuleHitQ910{};
    float strongestInto = 0.0f;
    bool blocked = false;

    for (size_t triIndex = 0u; triIndex < gWorldTriangles.size(); ++triIndex) {
        const CollisionTriangle& tri = gWorldTriangles[triIndex];
        // Floors/ramps are support, never horizontal wall constraints.
        if (IsWalkableTriangleQ910(tri)) continue;

        CapsuleHitQ910 hit;
        if (!CapsuleContactTriangleQ910(x, z, feetY, triIndex, hit)) continue;
        const float hLen = std::sqrt(hit.nx*hit.nx + hit.nz*hit.nz);
        if (hLen < 1e-5f) continue;
        const float hx = hit.nx / hLen;
        const float hz = hit.nz / hLen;
        const float into = moveX*hx + moveZ*hz;
        if (into >= -Q718_DIRECTION_EPS) continue;

        if (!blocked || into < strongestInto) {
            blocked = true;
            strongestInto = into;
            outHit = hit;
        }
    }
    return blocked;
}

bool SweepCapsuleHorizontalQ910(float startX, float startZ,
                                float targetX, float targetZ,
                                float feetY,
                                float& outX, float& outZ,
                                CapsuleHitQ910& outHit) {
    outX = startX; outZ = startZ; outHit = CapsuleHitQ910{};
    const float dx = targetX-startX, dz = targetZ-startZ;
    const float distance = std::sqrt(dx*dx + dz*dz);
    if (distance < 1e-7f) { outX=targetX; outZ=targetZ; return true; }

    const float sampleStep = std::max(0.008f, CollisionRadiusQ712()*0.05f);
    const int samples = std::clamp(static_cast<int>(std::ceil(distance/sampleStep)), 1, 24);
    float clearT = 0.0f;

    for (int i=1; i<=samples; ++i) {
        const float t = static_cast<float>(i)/static_cast<float>(samples);
        CapsuleHitQ910 hit;
        if (!CapsulePositionBlockedQ910(startX+dx*t, startZ+dz*t, feetY,
                                        dx, dz, hit)) {
            clearT = t;
            continue;
        }

        float lo=clearT, hi=t;
        CapsuleHitQ910 hiHit=hit;
        for (int iteration=0; iteration<Q910_CAST_BISECTIONS; ++iteration) {
            const float mid=(lo+hi)*0.5f;
            CapsuleHitQ910 midHit;
            if (!CapsulePositionBlockedQ910(startX+dx*mid, startZ+dz*mid, feetY,
                                            dx, dz, midHit)) {
                lo=mid;
            } else {
                hi=mid;
                hiHit=midHit;
            }
        }
        outX=startX+dx*lo;
        outZ=startZ+dz*lo;
        outHit=hiHit;
        return false;
    }

    outX=targetX; outZ=targetZ;
    return true;
}

bool WalkableSurfaceAtQ910(float x, float z, float targetY, float tolerance) {
    for (const CollisionTriangle& tri : gWorldTriangles) {
        if (!IsWalkableTriangleQ910(tri)) continue;
        if (x < tri.minX-0.008f || x > tri.maxX+0.008f ||
            z < tri.minZ-0.008f || z > tri.maxZ+0.008f) continue;
        float u=0.0f,v=0.0f,w=0.0f;
        if (!BarycentricXZ(tri,x,z,u,v,w)) continue;
        const float y=tri.a.y*u+tri.b.y*v+tri.c.y*w;
        if (std::fabs(y-targetY) <= tolerance) return true;
    }
    return false;
}

int LandingFootprintSupportQ910(float x, float z, float y,
                                float nx, float nz) {
    const float r=CollisionRadiusQ712();
    const float lx=-nz, lz=nx;
    const float side=r*0.52f;
    const float fore=r*0.82f;
    const std::array<Vec2,7> probes{{
        {0.0f,0.0f},
        {lx*side,lz*side},{-lx*side,-lz*side},
        {nx*fore,nz*fore},{-nx*fore,-nz*fore},
        {nx*fore+lx*side,nz*fore+lz*side},
        {nx*fore-lx*side,nz*fore-lz*side}
    }};
    int support=0;
    for (const Vec2& p : probes) {
        if (WalkableSurfaceAtQ910(x+p.x,z+p.y,y,Q910_LANDING_TOLERANCE)) ++support;
    }
    return support;
}

DownSupportQ910 SweepDownForSupportQ910(float x, float z,
                                        float raisedFeetY, float originalFeetY,
                                        float nx, float nz) {
    DownSupportQ910 result;
    const float maxY = originalFeetY + StepHeightQ78B() + PLAYER_SKIN + 0.012f;
    const float minY = originalFeetY - Q910_MAX_STEP_DROP;
    const float r=CollisionRadiusQ712();
    const float lx=-nz,lz=nx;
    const float side=r*0.52f, fore=r*0.82f;
    const std::array<Vec2,7> probes{{
        {0.0f,0.0f},
        {lx*side,lz*side},{-lx*side,-lz*side},
        {nx*fore,nz*fore},{-nx*fore,-nz*fore},
        {nx*fore+lx*side,nz*fore+lz*side},
        {nx*fore-lx*side,nz*fore-lz*side}
    }};

    struct CandidateQ910 { float y; size_t tri; };
    std::vector<CandidateQ910> candidates;
    candidates.reserve(32u);
    for (size_t triIndex=0u; triIndex<gWorldTriangles.size(); ++triIndex) {
        const CollisionTriangle& tri=gWorldTriangles[triIndex];
        if (!IsWalkableTriangleQ910(tri)) continue;
        if (tri.maxY < minY-0.05f || tri.minY > raisedFeetY+0.02f) continue;
        for (const Vec2& p : probes) {
            const float px=x+p.x,pz=z+p.y;
            if (px < tri.minX-0.008f || px > tri.maxX+0.008f ||
                pz < tri.minZ-0.008f || pz > tri.maxZ+0.008f) continue;
            float u=0.0f,v=0.0f,w=0.0f;
            if (!BarycentricXZ(tri,px,pz,u,v,w)) continue;
            const float y=tri.a.y*u+tri.b.y*v+tri.c.y*w;
            if (y > maxY || y < minY || y > raisedFeetY+0.01f) continue;
            candidates.push_back({y,triIndex});
        }
    }
    std::sort(candidates.begin(),candidates.end(),[](const CandidateQ910& a,const CandidateQ910& b){
        return a.y>b.y;
    });

    float lastY=std::numeric_limits<float>::infinity();
    for (const CandidateQ910& candidate : candidates) {
        if (std::fabs(candidate.y-lastY) < 0.004f) continue;
        lastY=candidate.y;
        const int support=LandingFootprintSupportQ910(x,z,candidate.y,nx,nz);
        if (support < 2) continue;
        result.found=true;
        result.y=candidate.y;
        result.supportPoints=support;
        result.triangleIndex=candidate.tri;
        return result; // first surface hit by the downward sweep = highest valid support.
    }
    return result;
}

bool TryStandardStepQ910(float startX,float startZ,
                         float targetX,float targetZ,float feetY,
                         const CapsuleHitQ910& initialHit,
                         float& outFeetY) {
    const float dx=targetX-startX,dz=targetZ-startZ;
    const float len=std::sqrt(dx*dx+dz*dz);
    if (len < 1e-7f) return false;
    const float nx=dx/len,nz=dz/len;
    const float raisedFeetY=feetY+StepHeightQ78B()+PLAYER_SKIN+0.008f;

    // UP. This is a real character-controller clearance check, not a ledge
    // classification. If the capsule cannot occupy the raised position, no step.
    if (HasOverheadBlock(startX,startZ,raisedFeetY)) {
        if (gQ910StepFailLogs < 120u) {
            ++gQ910StepFailLogs;
            Q6G_LOGI("Q9.4 STANDARD STEP FAIL: phase=UP reason=overhead contactRise=%.3f",
                     initialHit.pointY-feetY);
        }
        return false;
    }

    // FORWARD. Sweep only the user's ORIGINAL requested displacement while the
    // capsule is raised. Tall walls remain walls; a <= step-height riser falls
    // below the raised capsule and therefore disappears naturally.
    float raisedX=startX,raisedZ=startZ;
    CapsuleHitQ910 raisedHit;
    if (!SweepCapsuleHorizontalQ910(startX,startZ,targetX,targetZ,raisedFeetY,
                                    raisedX,raisedZ,raisedHit)) {
        if (gQ910StepFailLogs < 120u) {
            ++gQ910StepFailLogs;
            Q6G_LOGI("Q9.4 STANDARD STEP FAIL: phase=FORWARD reason=blocked contactRise=%.3f raisedHitY=%.3f",
                     initialHit.pointY-feetY,raisedHit.pointY-feetY);
        }
        return false;
    }

    // DOWN. The first/highest walkable support under the raised capsule wins.
    // This is why the old floor cannot hide the next stair tread.
    const DownSupportQ910 landing=SweepDownForSupportQ910(targetX,targetZ,
                                                           raisedFeetY,feetY,nx,nz);
    if (!landing.found) {
        if (gQ910StepFailLogs < 120u) {
            ++gQ910StepFailLogs;
            Q6G_LOGI("Q9.4 STANDARD STEP FAIL: phase=DOWN reason=no-support contactRise=%.3f",
                     initialHit.pointY-feetY);
        }
        return false;
    }

    const float rise=landing.y-feetY;
    if (rise > StepHeightQ78B()+PLAYER_SKIN+0.012f || rise < -Q910_MAX_STEP_DROP) return false;
    // For an actual upward blocker, do not accept the old floor as the landing.
    if (initialHit.pointY-feetY > 0.015f && rise < 0.006f) return false;

    outFeetY=landing.y;
    if (gQ910StepLogs < 240u || (gResolveCounter%240u)==0u) {
        ++gQ910StepLogs;
        Q6G_LOGI("Q9.4 STANDARD STEP PASS: up=%.3f forward=%.3f down=%.3f landingRise=%.3f support=%d tri=%zu contactRise=%.3f",
                 raisedFeetY-feetY,len,raisedFeetY-landing.y,rise,
                 landing.supportPoints,landing.triangleIndex,initialHit.pointY-feetY);
    }
    return true;
}

uint64_t gQ920EnvelopePassLogs = 0u;
uint64_t gQ920EnvelopeFailLogs = 0u;

bool HasHardEnvelopeBlockerQ920(float x, float z, float feetY,
                                float moveX, float moveZ,
                                CapsuleHitQ910& outHit) {
    outHit = CapsuleHitQ910{};
    const float hardTopY = feetY + StepHeightQ78B() + PLAYER_SKIN + 0.025f;
    float strongestInto = 0.0f;
    bool blocked = false;

    for (size_t triIndex = 0u; triIndex < gWorldTriangles.size(); ++triIndex) {
        const CollisionTriangle& tri = gWorldTriangles[triIndex];
        if (IsWalkableTriangleQ910(tri)) continue;

        CapsuleHitQ910 hit;
        if (!CapsuleContactTriangleQ910(x, z, feetY, triIndex, hit)) continue;
        const float hLen = std::sqrt(hit.nx*hit.nx + hit.nz*hit.nz);
        if (hLen < 1e-5f) continue;
        const float nx = hit.nx / hLen;
        const float nz = hit.nz / hLen;
        const float into = moveX*nx + moveZ*nz;
        if (into >= -Q718_DIRECTION_EPS) continue;

        // The defining Q9.4 rule: non-walkable geometry that ends within one
        // step above the newly resolved support cannot cancel horizontal motion.
        if (tri.maxY <= hardTopY) continue;

        if (!blocked || into < strongestInto) {
            blocked = true;
            strongestInto = into;
            outHit = hit;
        }
    }
    return blocked;
}

bool TryStepEnvelopeQ920(float startX, float startZ,
                         float targetX, float targetZ, float feetY,
                         const CapsuleHitQ910& initialHit,
                         float& outFeetY) {
    if (!initialHit.hit || initialHit.triangleIndex >= gWorldTriangles.size()) return false;
    const CollisionTriangle& initialTri = gWorldTriangles[initialHit.triangleIndex];
    const float dx = targetX - startX;
    const float dz = targetZ - startZ;
    const float len = std::sqrt(dx*dx + dz*dz);
    if (len < 1e-7f) return false;
    const float nx = dx / len;
    const float nz = dz / len;
    const float maxStep = StepHeightQ78B() + PLAYER_SKIN + 0.025f;
    const float initialTopRise = initialTri.maxY - feetY;

    // A face that itself continues above the one-step envelope is a real wall.
    // We do NOT use contact-point height here: a tall wall can be touched low.
    if (initialTopRise > maxStep) {
        if (gQ920EnvelopeFailLogs < 160u || (gResolveCounter % 240u) == 0u) {
            ++gQ920EnvelopeFailLogs;
            Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=INITIAL reason=tall-face contactRise=%.3f topRise=%.3f maxStep=%.3f tri=%zu",
                     initialHit.pointY-feetY, initialTopRise, maxStep,
                     initialHit.triangleIndex);
        }
        return false;
    }

    // Preserve the full requested horizontal move first. There is deliberately
    // NO raised-forward sweep here. Resolve support at the destination using the
    // already-proven highest-walkable-surface query from Q9.1.
    const float queryTopY = feetY + StepHeightQ78B() + PLAYER_SKIN + 0.012f;
    const DownSupportQ910 landing = SweepDownForSupportQ910(
        targetX, targetZ, queryTopY, feetY, nx, nz);
    if (!landing.found) {
        if (gQ920EnvelopeFailLogs < 160u || (gResolveCounter % 240u) == 0u) {
            ++gQ920EnvelopeFailLogs;
            Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=SUPPORT reason=no-support contactRise=%.3f topRise=%.3f",
                     initialHit.pointY-feetY, initialTopRise);
        }
        return false;
    }

    const float rise = landing.y - feetY;
    if (rise > StepHeightQ78B() + PLAYER_SKIN + 0.012f ||
        rise < -Q910_MAX_STEP_DROP) {
        if (gQ920EnvelopeFailLogs < 160u || (gResolveCounter % 240u) == 0u) {
            ++gQ920EnvelopeFailLogs;
            Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=SUPPORT reason=rise-range landingRise=%.3f",
                     rise);
        }
        return false;
    }

    // Actual head/upper-body occupancy at the resulting position is still real
    // collision. This is not the old raised-step veto; it checks only the final
    // character pose after support has been chosen.
    if (HasOverheadBlock(targetX, targetZ, landing.y)) {
        if (gQ920EnvelopeFailLogs < 160u || (gResolveCounter % 240u) == 0u) {
            ++gQ920EnvelopeFailLogs;
            Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=FINAL reason=headroom landingRise=%.3f",
                     rise);
        }
        return false;
    }

    // After the support transfer, reject only geometry that still extends above
    // one complete step envelope. The next stair riser remains non-blocking;
    // a wall/rail/large obstacle remains solid.
    CapsuleHitQ910 hardHit;
    if (HasHardEnvelopeBlockerQ920(targetX, targetZ, landing.y,
                                   dx, dz, hardHit)) {
        if (gQ920EnvelopeFailLogs < 160u || (gResolveCounter % 240u) == 0u) {
            ++gQ920EnvelopeFailLogs;
            const CollisionTriangle& hardTri = gWorldTriangles[hardHit.triangleIndex];
            Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=FINAL reason=hard-above-envelope landingRise=%.3f hardTopRise=%.3f hardContactRise=%.3f tri=%zu",
                     rise, hardTri.maxY-landing.y,
                     hardHit.pointY-landing.y, hardHit.triangleIndex);
        }
        return false;
    }

    outFeetY = landing.y;
    if (gQ920EnvelopePassLogs < 320u || (gResolveCounter % 240u) == 0u) {
        ++gQ920EnvelopePassLogs;
        Q6G_LOGI("Q9.4 STEP ENVELOPE PASS: forward=%.3f contactRise=%.3f initialTopRise=%.3f landingRise=%.3f support=%d tri=%zu raisedForwardVeto=0",
                 len, initialHit.pointY-feetY, initialTopRise, rise,
                 landing.supportPoints, landing.triangleIndex);
    }
    return true;
}

uint64_t gQ930EnvelopePassLogs = 0u;
uint64_t gQ930EnvelopeFailLogs = 0u;
constexpr int Q930_UPPER_SWEEP_SAMPLES_MAX = 24;

bool UpperBodyTriangleContactQ930(float x, float z, float feetY,
                                  size_t triIndex,
                                  CapsuleHitQ910& out) {
    if (triIndex >= gWorldTriangles.size()) return false;
    const CollisionTriangle& tri = gWorldTriangles[triIndex];
    const float radius = CollisionRadiusQ712();
    const float queryRadius = radius + Q910_SWEEP_SKIN;
    const float envelopeTop = feetY + StepHeightQ78B() + PLAYER_SKIN + 0.025f;
    const float lowerCenterY = envelopeTop + radius;
    const float upperCenterY = feetY + PLAYER_HEIGHT - radius;
    if (lowerCenterY > upperCenterY) return false;

    if (tri.maxY < envelopeTop - Q910_SWEEP_SKIN ||
        tri.minY > feetY + PLAYER_HEIGHT + queryRadius) return false;
    if (x < tri.minX - queryRadius || x > tri.maxX + queryRadius ||
        z < tri.minZ - queryRadius || z > tri.maxZ + queryRadius) return false;

    const Vec3 segA{x, lowerCenterY, z};
    const Vec3 segB{x, upperCenterY, z};
    Vec3 capsulePoint;
    Vec3 trianglePoint;
    const float d2 = ClosestCapsuleSegmentTriangleQ719(segA, segB, tri,
                                                        capsulePoint, trianglePoint);
    if (!std::isfinite(d2) || d2 > queryRadius * queryRadius) return false;

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
        const float cx = (tri.a.x + tri.b.x + tri.c.x) / 3.0f;
        const float cy = (tri.a.y + tri.b.y + tri.c.y) / 3.0f;
        const float cz = (tri.a.z + tri.b.z + tri.c.z) / 3.0f;
        const float bodyMidY = (lowerCenterY + upperCenterY) * 0.5f;
        const float toward = nx*(x-cx) + ny*(bodyMidY-cy) + nz*(z-cz);
        if (toward < 0.0f) { nx=-nx; ny=-ny; nz=-nz; }
    }

    out.hit = true;
    out.triangleIndex = triIndex;
    out.pointX = trianglePoint.x;
    out.pointY = trianglePoint.y;
    out.pointZ = trianglePoint.z;
    out.nx = nx; out.ny = ny; out.nz = nz;
    return true;
}

bool UpperBodyDirectionalBlockedQ930(float x, float z, float feetY,
                                     float moveX, float moveZ,
                                     CapsuleHitQ910& outHit) {
    outHit = CapsuleHitQ910{};
    float strongestInto = 0.0f;
    bool blocked = false;
    for (size_t triIndex = 0u; triIndex < gWorldTriangles.size(); ++triIndex) {
        CapsuleHitQ910 hit;
        if (!UpperBodyTriangleContactQ930(x, z, feetY, triIndex, hit)) continue;
        const float hLen = std::sqrt(hit.nx*hit.nx + hit.nz*hit.nz);
        if (hLen < 1e-5f) continue;
        const float nx = hit.nx / hLen;
        const float nz = hit.nz / hLen;
        const float into = moveX*nx + moveZ*nz;
        if (into >= -Q718_DIRECTION_EPS) continue;
        if (!blocked || into < strongestInto) {
            blocked = true;
            strongestInto = into;
            outHit = hit;
        }
    }
    return blocked;
}

bool UpperBodyAnyOverlapQ930(float x, float z, float feetY,
                             CapsuleHitQ910& outHit) {
    outHit = CapsuleHitQ910{};
    for (size_t triIndex = 0u; triIndex < gWorldTriangles.size(); ++triIndex) {
        CapsuleHitQ910 hit;
        if (!UpperBodyTriangleContactQ930(x, z, feetY, triIndex, hit)) continue;
        outHit = hit;
        return true;
    }
    return false;
}

bool SweepUpperBodyQ930(float startX, float startZ, float startFeetY,
                        float targetX, float targetZ, float targetFeetY,
                        CapsuleHitQ910& outHit) {
    outHit = CapsuleHitQ910{};
    const float dx = targetX - startX;
    const float dz = targetZ - startZ;
    const float distance = std::sqrt(dx*dx + dz*dz);
    if (distance < 1e-7f) return true;

    const float sampleStep = std::max(0.008f, CollisionRadiusQ712()*0.05f);
    const int samples = std::clamp(static_cast<int>(std::ceil(distance/sampleStep)),
                                   1, Q930_UPPER_SWEEP_SAMPLES_MAX);
    for (int i=1; i<=samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float x = startX + dx*t;
        const float z = startZ + dz*t;
        const float y = startFeetY + (targetFeetY-startFeetY)*t;
        CapsuleHitQ910 hit;
        if (UpperBodyDirectionalBlockedQ930(x, z, y, dx, dz, hit)) {
            outHit = hit;
            return false;
        }
    }
    return true;
}

bool TryLocalStepEnvelopeQ930(float startX, float startZ,
                              float targetX, float targetZ, float feetY,
                              const CapsuleHitQ910& initialHit,
                              float& outFeetY) {
    if (!initialHit.hit || initialHit.triangleIndex >= gWorldTriangles.size()) return false;
    const float dx = targetX - startX;
    const float dz = targetZ - startZ;
    const float len = std::sqrt(dx*dx + dz*dz);
    if (len < 1e-7f) return false;
    const float nx = dx / len;
    const float nz = dz / len;

    // First decide where the feet would be after preserving the requested X/Z.
    // No global triangle maxY check is allowed here.
    const float queryTopY = feetY + StepHeightQ78B() + PLAYER_SKIN + 0.012f;
    const DownSupportQ910 landing = SweepDownForSupportQ910(
        targetX, targetZ, queryTopY, feetY, nx, nz);
    if (!landing.found) {
        if (gQ930EnvelopeFailLogs < 200u || (gResolveCounter % 240u) == 0u) {
            ++gQ930EnvelopeFailLogs;
            Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=SUPPORT reason=no-support contactRise=%.3f tri=%zu",
                     initialHit.pointY-feetY, initialHit.triangleIndex);
        }
        return false;
    }

    const float rise = landing.y - feetY;
    if (rise > StepHeightQ78B() + PLAYER_SKIN + 0.012f ||
        rise < -Q910_MAX_STEP_DROP) {
        if (gQ930EnvelopeFailLogs < 200u || (gResolveCounter % 240u) == 0u) {
            ++gQ930EnvelopeFailLogs;
            Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=SUPPORT reason=rise-range contactRise=%.3f landingRise=%.3f",
                     initialHit.pointY-feetY, rise);
        }
        return false;
    }

    // Sweep only the body above the step envelope. A stair riser, pipe, plank
    // lip, etc. is free to overlap the lower step zone; a real wall/rail still
    // intersects this truncated capsule locally and blocks the move.
    CapsuleHitQ910 upperHit;
    if (!SweepUpperBodyQ930(startX, startZ, feetY,
                            targetX, targetZ, landing.y, upperHit)) {
        if (gQ930EnvelopeFailLogs < 200u || (gResolveCounter % 240u) == 0u) {
            ++gQ930EnvelopeFailLogs;
            Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=UPPER reason=sweep-block contactRise=%.3f landingRise=%.3f upperHitRise=%.3f tri=%zu",
                     initialHit.pointY-feetY, rise,
                     upperHit.pointY-landing.y, upperHit.triangleIndex);
        }
        return false;
    }

    // Final occupancy catches horizontal ceilings/overhangs whose normal has no
    // useful X/Z component for the directional sweep.
    CapsuleHitQ910 overlapHit;
    if (UpperBodyAnyOverlapQ930(targetX, targetZ, landing.y, overlapHit)) {
        const float hLen = std::sqrt(overlapHit.nx*overlapHit.nx + overlapHit.nz*overlapHit.nz);
        const float into = hLen > 1e-5f
            ? dx*(overlapHit.nx/hLen) + dz*(overlapHit.nz/hLen)
            : 0.0f;
        // Directional side contact while moving away/tangent is harmless. Pure
        // vertical overlap (e.g. ceiling) remains a hard failure.
        if (hLen < 1e-5f || into < -Q718_DIRECTION_EPS) {
            if (gQ930EnvelopeFailLogs < 200u || (gResolveCounter % 240u) == 0u) {
                ++gQ930EnvelopeFailLogs;
                Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=FINAL reason=upper-overlap contactRise=%.3f landingRise=%.3f overlapHitRise=%.3f tri=%zu",
                         initialHit.pointY-feetY, rise,
                         overlapHit.pointY-landing.y, overlapHit.triangleIndex);
            }
            return false;
        }
    }

    outFeetY = landing.y;
    if (gQ930EnvelopePassLogs < 400u || (gResolveCounter % 240u) == 0u) {
        ++gQ930EnvelopePassLogs;
        Q6G_LOGI("Q9.4 STEP ENVELOPE PASS: forward=%.3f contactRise=%.3f landingRise=%.3f support=%d tri=%zu localUpperClear=1 wholeTriMaxYVeto=0 raisedForwardVeto=0",
                 len, initialHit.pointY-feetY, rise,
                 landing.supportPoints, landing.triangleIndex);
    }
    return true;
}

constexpr float Q940_STEP_TOLERANCE = 0.020f;
constexpr float Q940_RAISED_MIN = 0.008f;
constexpr float Q940_LOCAL_PAD = 0.030f;
constexpr float Q940_LOOKAHEAD_MULT = 1.50f;
uint64_t gQ940LandingLogs = 0u;
uint64_t gQ940LocalLogs = 0u;
uint64_t gQ940PassLogs = 0u;
uint64_t gQ940FailLogs = 0u;

struct LocalCandidatesQ940 {
    std::vector<size_t> tris;
};

LocalCandidatesQ940 CollectLocalCandidatesQ940(float startX, float startZ,
                                                float targetX, float targetZ,
                                                float feetY,
                                                float nx, float nz) {
    LocalCandidatesQ940 result;
    const float r = CollisionRadiusQ712();
    const float ahead = r * Q940_LOOKAHEAD_MULT;
    const float ax = targetX + nx * ahead;
    const float az = targetZ + nz * ahead;
    const float minX = std::min({startX, targetX, ax}) - r - Q940_LOCAL_PAD;
    const float maxX = std::max({startX, targetX, ax}) + r + Q940_LOCAL_PAD;
    const float minZ = std::min({startZ, targetZ, az}) - r - Q940_LOCAL_PAD;
    const float maxZ = std::max({startZ, targetZ, az}) + r + Q940_LOCAL_PAD;
    const float minY = feetY - Q910_MAX_STEP_DROP - r - 0.05f;
    const float maxY = feetY + PLAYER_HEIGHT + r + Q940_STEP_TOLERANCE;

    result.tris.reserve(192u);
    for (size_t i = 0u; i < gWorldTriangles.size(); ++i) {
        const CollisionTriangle& tri = gWorldTriangles[i];
        if (tri.maxX < minX || tri.minX > maxX ||
            tri.maxZ < minZ || tri.minZ > maxZ ||
            tri.maxY < minY || tri.minY > maxY) continue;
        result.tris.push_back(i);
    }

    if (gQ940LocalLogs < 80u || (gResolveCounter % 360u) == 0u) {
        ++gQ940LocalLogs;
        Q6G_LOGI("Q9.4 LOCAL CANDIDATES: world=%zu local=%zu ahead=%.3f",
                 gWorldTriangles.size(), result.tris.size(), ahead);
    }
    return result;
}

int LocalFootprintSupportQ940(const LocalCandidatesQ940& local,
                              float x, float z, float y,
                              float nx, float nz) {
    const float r = CollisionRadiusQ712();
    const float lx = -nz, lz = nx;
    const float side = r * 0.52f;
    const float fore = r * 0.82f;
    const std::array<Vec2,7> probes{{
        {0.0f,0.0f},
        {lx*side,lz*side},{-lx*side,-lz*side},
        {nx*fore,nz*fore},{-nx*fore,-nz*fore},
        {nx*fore+lx*side,nz*fore+lz*side},
        {nx*fore-lx*side,nz*fore-lz*side}
    }};

    int support = 0;
    for (const Vec2& p : probes) {
        const float px = x + p.x;
        const float pz = z + p.y;
        bool thisProbe = false;
        for (size_t triIndex : local.tris) {
            const CollisionTriangle& tri = gWorldTriangles[triIndex];
            if (!IsWalkableTriangleQ910(tri)) continue;
            if (px < tri.minX-0.008f || px > tri.maxX+0.008f ||
                pz < tri.minZ-0.008f || pz > tri.maxZ+0.008f) continue;
            float u=0.0f,v=0.0f,w=0.0f;
            if (!BarycentricXZ(tri,px,pz,u,v,w)) continue;
            const float sy = tri.a.y*u + tri.b.y*v + tri.c.y*w;
            if (std::fabs(sy-y) <= Q910_LANDING_TOLERANCE) {
                thisProbe = true;
                break;
            }
        }
        if (thisProbe) ++support;
    }
    return support;
}

DownSupportQ910 FindLocalSupportQ940(const LocalCandidatesQ940& local,
                                     float x, float z, float feetY,
                                     float nx, float nz,
                                     bool raisedOnly) {
    DownSupportQ910 result;
    const float maxRise = StepHeightQ78B() + Q940_STEP_TOLERANCE;
    const float minY = feetY - Q910_MAX_STEP_DROP;
    const float maxY = feetY + maxRise;
    const float r = CollisionRadiusQ712();
    const float lx = -nz, lz = nx;
    const float side = r * 0.52f;
    const float fore = r * 0.82f;
    const std::array<Vec2,7> probes{{
        {0.0f,0.0f},
        {lx*side,lz*side},{-lx*side,-lz*side},
        {nx*fore,nz*fore},{-nx*fore,-nz*fore},
        {nx*fore+lx*side,nz*fore+lz*side},
        {nx*fore-lx*side,nz*fore-lz*side}
    }};

    struct CandidateQ940 { float y; size_t tri; };
    std::vector<CandidateQ940> candidates;
    candidates.reserve(48u);
    for (size_t triIndex : local.tris) {
        const CollisionTriangle& tri = gWorldTriangles[triIndex];
        if (!IsWalkableTriangleQ910(tri)) continue;
        if (tri.maxY < minY-0.05f || tri.minY > maxY+0.05f) continue;
        for (const Vec2& p : probes) {
            const float px = x+p.x, pz = z+p.y;
            if (px < tri.minX-0.008f || px > tri.maxX+0.008f ||
                pz < tri.minZ-0.008f || pz > tri.maxZ+0.008f) continue;
            float u=0.0f,v=0.0f,w=0.0f;
            if (!BarycentricXZ(tri,px,pz,u,v,w)) continue;
            const float y = tri.a.y*u + tri.b.y*v + tri.c.y*w;
            const float rise = y-feetY;
            if (y < minY || y > maxY) continue;
            if (raisedOnly && rise < Q940_RAISED_MIN) continue;
            candidates.push_back({y,triIndex});
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [feetY,raisedOnly](const CandidateQ940& a,const CandidateQ940& b) {
        if (raisedOnly) return a.y < b.y; // nearest next tread, never jump a flight.
        return std::fabs(a.y-feetY) < std::fabs(b.y-feetY);
    });

    float lastY = std::numeric_limits<float>::infinity();
    for (const CandidateQ940& candidate : candidates) {
        if (std::fabs(candidate.y-lastY) < 0.004f) continue;
        lastY = candidate.y;
        const int support = LocalFootprintSupportQ940(local,x,z,candidate.y,nx,nz);
        if (support < 2) continue;
        result.found = true;
        result.y = candidate.y;
        result.supportPoints = support;
        result.triangleIndex = candidate.tri;
        return result;
    }
    return result;
}

DownSupportQ910 FindRaisedLandingAheadQ940(const LocalCandidatesQ940& local,
                                           float targetX, float targetZ,
                                           float feetY, float nx, float nz,
                                           float& outAhead) {
    const float r = CollisionRadiusQ712();
    const std::array<float,7> ahead{{
        0.0f, r*0.25f, r*0.50f, r*0.75f,
        r*1.00f, r*1.25f, r*1.50f
    }};
    for (float d : ahead) {
        const DownSupportQ910 support = FindLocalSupportQ940(
            local, targetX+nx*d, targetZ+nz*d, feetY, nx, nz, true);
        if (!support.found) continue;
        outAhead = d;
        return support;
    }
    return DownSupportQ910{};
}

bool LocalUpperBodyBlockedQ940(const LocalCandidatesQ940& local,
                               float x, float z, float feetY,
                               float moveX, float moveZ,
                               CapsuleHitQ910& outHit) {
    outHit = CapsuleHitQ910{};
    float strongestInto = 0.0f;
    bool blocked = false;
    for (size_t triIndex : local.tris) {
        CapsuleHitQ910 hit;
        if (!UpperBodyTriangleContactQ930(x,z,feetY,triIndex,hit)) continue;
        const float hLen = std::sqrt(hit.nx*hit.nx + hit.nz*hit.nz);
        if (hLen < 1e-5f) {
            outHit = hit; // ceiling/overhang overlap.
            return true;
        }
        const float into = moveX*(hit.nx/hLen) + moveZ*(hit.nz/hLen);
        if (into >= -Q718_DIRECTION_EPS) continue;
        if (!blocked || into < strongestInto) {
            blocked = true;
            strongestInto = into;
            outHit = hit;
        }
    }
    return blocked;
}

bool TryRaisedStepEnvelopeQ940(float startX, float startZ,
                               float targetX, float targetZ, float feetY,
                               const CapsuleHitQ910& initialHit,
                               float& outFeetY) {
    if (!initialHit.hit || initialHit.triangleIndex >= gWorldTriangles.size()) return false;
    const float dx = targetX-startX;
    const float dz = targetZ-startZ;
    const float len = std::sqrt(dx*dx+dz*dz);
    if (len < 1e-7f) return false;
    const float nx = dx/len, nz = dz/len;
    const float contactRise = initialHit.pointY-feetY;
    const float maxRise = StepHeightQ78B() + Q940_STEP_TOLERANCE;

    const LocalCandidatesQ940 local = CollectLocalCandidatesQ940(
        startX,startZ,targetX,targetZ,feetY,nx,nz);

    bool usedRaised = false;
    float probeAhead = 0.0f;
    DownSupportQ910 landing;

    // Positive riser: the old floor is NOT allowed to win first. Search for the
    // nearest supported raised tread, including a short look-ahead. This is the
    // specific failure seen on ShackStairs and ScrapGroundPlates.
    if (contactRise > 0.015f) {
        landing = FindRaisedLandingAheadQ940(local,targetX,targetZ,feetY,nx,nz,probeAhead);
        if (landing.found) usedRaised = true;
    }

    // No raised tread means this may simply be a low lip/pipe/seam. Preserve the
    // Q9.3 win by allowing ordinary current-floor support as fallback. A real
    // wall is still rejected by the local upper-body occupancy check below.
    if (!landing.found) {
        landing = FindLocalSupportQ940(local,targetX,targetZ,feetY,nx,nz,false);
    }
    if (!landing.found) {
        if (gQ940FailLogs < 160u || (gResolveCounter % 240u) == 0u) {
            ++gQ940FailLogs;
            Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=SUPPORT reason=no-support contactRise=%.3f local=%zu",
                     contactRise, local.tris.size());
        }
        return false;
    }

    const float rise = landing.y-feetY;
    if (rise > maxRise || rise < -Q910_MAX_STEP_DROP) {
        if (gQ940FailLogs < 160u || (gResolveCounter % 240u) == 0u) {
            ++gQ940FailLogs;
            Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=SUPPORT reason=rise-range contactRise=%.3f landingRise=%.3f maxRise=%.3f",
                     contactRise,rise,maxRise);
        }
        return false;
    }

    if (usedRaised && (gQ940LandingLogs < 240u || (gResolveCounter % 240u) == 0u)) {
        ++gQ940LandingLogs;
        Q6G_LOGI("Q9.4 RAISED LANDING SELECT: probeAhead=%.3f contactRise=%.3f landingRise=%.3f support=%d tri=%zu local=%zu maxRise=%.3f",
                 probeAhead,contactRise,rise,landing.supportPoints,
                 landing.triangleIndex,local.tris.size(),maxRise);
    }

    // Q9.3 sampled the whole world repeatedly along the move. Q9.4 performs one
    // final exact upper-body test against only nearby candidates, after the new
    // support height is known. A next stair riser is therefore below the new
    // envelope; a wall/rail/ceiling still intersects the upper body.
    CapsuleHitQ910 upperHit;
    if (LocalUpperBodyBlockedQ940(local,targetX,targetZ,landing.y,dx,dz,upperHit)) {
        if (gQ940FailLogs < 160u || (gResolveCounter % 240u) == 0u) {
            ++gQ940FailLogs;
            Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=UPPER reason=local-target-block contactRise=%.3f landingRise=%.3f upperHitRise=%.3f tri=%zu local=%zu",
                     contactRise,rise,upperHit.pointY-landing.y,
                     upperHit.triangleIndex,local.tris.size());
        }
        return false;
    }

    outFeetY = landing.y;
    if (gQ940PassLogs < 360u || (gResolveCounter % 240u) == 0u) {
        ++gQ940PassLogs;
        Q6G_LOGI("Q9.4 STEP ENVELOPE PASS: forward=%.3f contactRise=%.3f landingRise=%.3f mode=%s probeAhead=%.3f support=%d local=%zu maxRise=%.3f fullWorldUpperSweep=0",
                 len,contactRise,rise,usedRaised?"RAISED":"FLOOR",
                 probeAhead,landing.supportPoints,local.tris.size(),maxRise);
    }
    return true;
}

constexpr float Q950_GRID_CELL = 2.0f;
constexpr float Q950_STEP_TOLERANCE = 0.020f;
constexpr float Q950_QUERY_PAD = 0.035f;
constexpr int Q950_CAST_BISECTIONS = 5;
constexpr int Q950_MAX_CAST_SAMPLES = 12;
constexpr size_t Q950_MAX_GRID_CELLS_PER_ITEM = 64u;

struct Q950CollisionObject {
    uint64_t key = 0u;
    uint32_t refFormId = 0u;
    std::vector<uint64_t> shapeKeys;
    std::vector<size_t> triangles;
    float minX = 0.0f, maxX = 0.0f;
    float minY = 0.0f, maxY = 0.0f;
    float minZ = 0.0f, maxZ = 0.0f;
    size_t walkableTriangles = 0u;
};

struct Q950GridCell {
    std::vector<uint32_t> objects;
    std::vector<uint32_t> legacyTriangles;
};

struct Q950Candidates {
    std::vector<uint32_t> objects;
    std::vector<uint32_t> legacyTriangles;
};

struct Q950Landing {
    bool found = false;
    float y = 0.0f;
    float rise = 0.0f;
    float score = 0.0f;
    float probeAhead = 0.0f;
    size_t triangleIndex = 0u;
    uint32_t objectIndex = 0xffffffffu;
    bool topology = false;
};

std::vector<Q950CollisionObject> gQ950Objects;
std::vector<uint32_t> gQ950TriangleObject;
std::unordered_map<uint64_t,Q950GridCell> gQ950Grid;
std::vector<uint32_t> gQ950LargeObjects;
std::vector<uint32_t> gQ950LargeLegacy;
std::vector<uint32_t> gQ950ObjectStamp;
std::vector<uint32_t> gQ950TriangleStamp;
uint32_t gQ950QuerySerial = 1u;
const CollisionTriangle* gQ950Data = nullptr;
size_t gQ950TriangleCount = 0u;
bool gQ950Ready = false;
uint64_t gQ950ReadyLogs = 0u;
uint64_t gQ950StepLogs = 0u;
uint64_t gQ950LowPassLogs = 0u;
uint64_t gQ950BlockedLogs = 0u;
uint64_t gQ950SlideLogs = 0u;

uint64_t Q950CellKey(int x,int z) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32u) |
           static_cast<uint32_t>(z);
}

int Q950CellCoord(float v) {
    return static_cast<int>(std::floor(v / Q950_GRID_CELL));
}

bool Q950AabbOverlap(float minX,float maxX,float minY,float maxY,float minZ,float maxZ,
                     float bMinX,float bMaxX,float bMinY,float bMaxY,float bMinZ,float bMaxZ) {
    return !(maxX < bMinX || minX > bMaxX ||
             maxY < bMinY || minY > bMaxY ||
             maxZ < bMinZ || minZ > bMaxZ);
}

void EnsureQ950SpatialIndex() {
    const CollisionTriangle* data = gWorldTriangles.empty() ? nullptr : gWorldTriangles.data();
    if (gQ950Ready && data == gQ950Data && gQ950TriangleCount == gWorldTriangles.size()) return;

    EnsureHkCoherentShapesQ900();
    EnsureHkWeldAdjacencyQ801();

    gQ950Objects.clear();
    gQ950TriangleObject.assign(gWorldTriangles.size(),0xffffffffu);
    gQ950Grid.clear();
    gQ950LargeObjects.clear();
    gQ950LargeLegacy.clear();

    std::unordered_map<uint64_t,uint32_t> objectByKey;
    objectByKey.reserve(gHkShapesQ900.size()+32u);
    size_t shapedTriangles = 0u;

    // Merge Q9.0's per-bhk-shape meshes into one placed authored collision
    // object when they belong to the same placed reference. This keeps separate
    // Havok leaf shapes available through shapeKeys while letting a stair NIF's
    // riser and tread participate in one object-level traversal query.
    for (const HkCoherentShapeQ900& shape : gHkShapesQ900) {
        if (shape.triangles.empty()) continue;
        uint32_t refFormId = 0u;
        for (size_t triIndex : shape.triangles) {
            if (triIndex >= gWorldTriangles.size()) continue;
            const auto src = gSurfaceSourcesQ722.find(gWorldTriangles[triIndex].surfaceKeyQ714);
            if (src != gSurfaceSourcesQ722.end() && src->second.refFormId != 0u) {
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
            objectIndex = static_cast<uint32_t>(gQ950Objects.size());
            Q950CollisionObject object;
            object.key = objectKey;
            object.refFormId = refFormId;
            object.minX = shape.minX; object.maxX = shape.maxX;
            object.minY = shape.minY; object.maxY = shape.maxY;
            object.minZ = shape.minZ; object.maxZ = shape.maxZ;
            gQ950Objects.push_back(std::move(object));
            objectByKey.emplace(objectKey,objectIndex);
        } else {
            objectIndex = found->second;
        }

        Q950CollisionObject& object = gQ950Objects[objectIndex];
        object.shapeKeys.push_back(shape.key);
        object.minX = std::min(object.minX,shape.minX); object.maxX = std::max(object.maxX,shape.maxX);
        object.minY = std::min(object.minY,shape.minY); object.maxY = std::max(object.maxY,shape.maxY);
        object.minZ = std::min(object.minZ,shape.minZ); object.maxZ = std::max(object.maxZ,shape.maxZ);
        object.walkableTriangles += shape.walkableTriangles;
        for (size_t triIndex : shape.triangles) {
            if (triIndex >= gWorldTriangles.size()) continue;
            object.triangles.push_back(triIndex);
            gQ950TriangleObject[triIndex] = objectIndex;
            ++shapedTriangles;
        }
    }

    auto insertObject = [&](uint32_t objectIndex) {
        const Q950CollisionObject& object = gQ950Objects[objectIndex];
        const int minCx=Q950CellCoord(object.minX), maxCx=Q950CellCoord(object.maxX);
        const int minCz=Q950CellCoord(object.minZ), maxCz=Q950CellCoord(object.maxZ);
        const int64_t spanX=static_cast<int64_t>(maxCx)-minCx+1;
        const int64_t spanZ=static_cast<int64_t>(maxCz)-minCz+1;
        if (spanX <= 0 || spanZ <= 0 ||
            static_cast<uint64_t>(spanX*spanZ) > Q950_MAX_GRID_CELLS_PER_ITEM) {
            gQ950LargeObjects.push_back(objectIndex);
            return;
        }
        for (int cx=minCx;cx<=maxCx;++cx)
            for (int cz=minCz;cz<=maxCz;++cz)
                gQ950Grid[Q950CellKey(cx,cz)].objects.push_back(objectIndex);
    };
    for (uint32_t i=0u;i<gQ950Objects.size();++i) insertObject(i);

    size_t legacyTriangles = 0u;
    for (uint32_t triIndex=0u;triIndex<gWorldTriangles.size();++triIndex) {
        if (gQ950TriangleObject[triIndex] != 0xffffffffu) continue;
        ++legacyTriangles;
        const CollisionTriangle& tri=gWorldTriangles[triIndex];
        const int minCx=Q950CellCoord(tri.minX), maxCx=Q950CellCoord(tri.maxX);
        const int minCz=Q950CellCoord(tri.minZ), maxCz=Q950CellCoord(tri.maxZ);
        const int64_t spanX=static_cast<int64_t>(maxCx)-minCx+1;
        const int64_t spanZ=static_cast<int64_t>(maxCz)-minCz+1;
        if (spanX <= 0 || spanZ <= 0 ||
            static_cast<uint64_t>(spanX*spanZ) > Q950_MAX_GRID_CELLS_PER_ITEM) {
            gQ950LargeLegacy.push_back(triIndex);
            continue;
        }
        for (int cx=minCx;cx<=maxCx;++cx)
            for (int cz=minCz;cz<=maxCz;++cz)
                gQ950Grid[Q950CellKey(cx,cz)].legacyTriangles.push_back(triIndex);
    }

    gQ950ObjectStamp.assign(gQ950Objects.size(),0u);
    gQ950TriangleStamp.assign(gWorldTriangles.size(),0u);
    gQ950QuerySerial=1u;
    gQ950Data=data;
    gQ950TriangleCount=gWorldTriangles.size();
    gQ950Ready=true;

    if (gQ950ReadyLogs < 8u) {
        ++gQ950ReadyLogs;
        Q6G_LOGI("Q9.5 COHERENT BROADPHASE READY: objects=%zu q900Shapes=%zu shapedTriangles=%zu legacyTriangles=%zu cells=%zu largeObjects=%zu mode=placed-object-grid+shape-narrowphase fullWorldMovementScans=0",
                 gQ950Objects.size(),gHkShapesQ900.size(),shapedTriangles,legacyTriangles,
                 gQ950Grid.size(),gQ950LargeObjects.size());
    }
}

void Q950BeginQuery() {
    ++gQ950QuerySerial;
    if (gQ950QuerySerial == 0u) {
        std::fill(gQ950ObjectStamp.begin(),gQ950ObjectStamp.end(),0u);
        std::fill(gQ950TriangleStamp.begin(),gQ950TriangleStamp.end(),0u);
        gQ950QuerySerial=1u;
    }
}

void Q950AddObject(Q950Candidates& out,uint32_t index) {
    if (index>=gQ950Objects.size()) return;
    if (gQ950ObjectStamp[index]==gQ950QuerySerial) return;
    gQ950ObjectStamp[index]=gQ950QuerySerial;
    out.objects.push_back(index);
}

void Q950AddLegacy(Q950Candidates& out,uint32_t index) {
    if (index>=gWorldTriangles.size()) return;
    if (gQ950TriangleStamp[index]==gQ950QuerySerial) return;
    gQ950TriangleStamp[index]=gQ950QuerySerial;
    out.legacyTriangles.push_back(index);
}

Q950Candidates QueryQ950(float minX,float maxX,float minY,float maxY,float minZ,float maxZ) {
    EnsureQ950SpatialIndex();
    Q950BeginQuery();
    Q950Candidates result;
    result.objects.reserve(24u);
    result.legacyTriangles.reserve(48u);
    const int minCx=Q950CellCoord(minX),maxCx=Q950CellCoord(maxX);
    const int minCz=Q950CellCoord(minZ),maxCz=Q950CellCoord(maxZ);
    for (int cx=minCx;cx<=maxCx;++cx) {
        for (int cz=minCz;cz<=maxCz;++cz) {
            const auto found=gQ950Grid.find(Q950CellKey(cx,cz));
            if (found==gQ950Grid.end()) continue;
            for (uint32_t objectIndex:found->second.objects) {
                const Q950CollisionObject& object=gQ950Objects[objectIndex];
                if (Q950AabbOverlap(minX,maxX,minY,maxY,minZ,maxZ,
                                    object.minX,object.maxX,object.minY,object.maxY,object.minZ,object.maxZ))
                    Q950AddObject(result,objectIndex);
            }
            for (uint32_t triIndex:found->second.legacyTriangles) {
                const CollisionTriangle& tri=gWorldTriangles[triIndex];
                if (Q950AabbOverlap(minX,maxX,minY,maxY,minZ,maxZ,
                                    tri.minX,tri.maxX,tri.minY,tri.maxY,tri.minZ,tri.maxZ))
                    Q950AddLegacy(result,triIndex);
            }
        }
    }
    for (uint32_t objectIndex:gQ950LargeObjects) {
        const Q950CollisionObject& object=gQ950Objects[objectIndex];
        if (Q950AabbOverlap(minX,maxX,minY,maxY,minZ,maxZ,
                            object.minX,object.maxX,object.minY,object.maxY,object.minZ,object.maxZ))
            Q950AddObject(result,objectIndex);
    }
    for (uint32_t triIndex:gQ950LargeLegacy) {
        const CollisionTriangle& tri=gWorldTriangles[triIndex];
        if (Q950AabbOverlap(minX,maxX,minY,maxY,minZ,maxZ,
                            tri.minX,tri.maxX,tri.minY,tri.maxY,tri.minZ,tri.maxZ))
            Q950AddLegacy(result,triIndex);
    }
    return result;
}

bool Q950PositionBlocked(float x,float z,float feetY,float moveX,float moveZ,
                         const Q950Candidates& candidates,
                         CapsuleHitQ910& outHit,uint32_t& outObject) {
    outHit=CapsuleHitQ910{};
    outObject=0xffffffffu;
    float strongestInto=0.0f;
    bool blocked=false;

    auto testTri=[&](size_t triIndex,uint32_t objectIndex) {
        if (triIndex>=gWorldTriangles.size()) return;
        const CollisionTriangle& tri=gWorldTriangles[triIndex];
        if (IsWalkableTriangleQ910(tri)) return;
        CapsuleHitQ910 hit;
        if (!CapsuleContactTriangleQ910(x,z,feetY,triIndex,hit)) return;
        const float hLen=std::sqrt(hit.nx*hit.nx+hit.nz*hit.nz);
        if (hLen<1e-5f) return;
        const float into=moveX*(hit.nx/hLen)+moveZ*(hit.nz/hLen);
        if (into>=-Q718_DIRECTION_EPS) return;
        if (!blocked || into<strongestInto) {
            blocked=true;
            strongestInto=into;
            outHit=hit;
            outObject=objectIndex;
        }
    };

    for (uint32_t objectIndex:candidates.objects) {
        const Q950CollisionObject& object=gQ950Objects[objectIndex];
        for (size_t triIndex:object.triangles) testTri(triIndex,objectIndex);
    }
    for (uint32_t triIndex:candidates.legacyTriangles) testTri(triIndex,0xffffffffu);
    return blocked;
}

bool SweepQ950(float startX,float startZ,float targetX,float targetZ,float feetY,
               float& outX,float& outZ,CapsuleHitQ910& outHit,uint32_t& outObject) {
    outX=startX; outZ=startZ; outHit=CapsuleHitQ910{}; outObject=0xffffffffu;
    const float dx=targetX-startX,dz=targetZ-startZ;
    const float distance=std::sqrt(dx*dx+dz*dz);
    if (distance<1e-7f) { outX=targetX; outZ=targetZ; return true; }

    const float r=CollisionRadiusQ712()+Q910_SWEEP_SKIN+Q950_QUERY_PAD;
    const Q950Candidates candidates=QueryQ950(std::min(startX,targetX)-r,std::max(startX,targetX)+r,
                                               feetY-r,feetY+PLAYER_HEIGHT+r,
                                               std::min(startZ,targetZ)-r,std::max(startZ,targetZ)+r);
    const float sampleStep=std::max(0.015f,CollisionRadiusQ712()*0.08f);
    const int samples=std::clamp(static_cast<int>(std::ceil(distance/sampleStep)),1,Q950_MAX_CAST_SAMPLES);
    float clearT=0.0f;
    for (int i=1;i<=samples;++i) {
        const float t=static_cast<float>(i)/static_cast<float>(samples);
        CapsuleHitQ910 hit; uint32_t objectIndex=0xffffffffu;
        if (!Q950PositionBlocked(startX+dx*t,startZ+dz*t,feetY,dx,dz,candidates,hit,objectIndex)) {
            clearT=t;
            continue;
        }
        float lo=clearT,hi=t;
        CapsuleHitQ910 hiHit=hit; uint32_t hiObject=objectIndex;
        for (int iteration=0;iteration<Q950_CAST_BISECTIONS;++iteration) {
            const float mid=(lo+hi)*0.5f;
            CapsuleHitQ910 midHit; uint32_t midObject=0xffffffffu;
            if (!Q950PositionBlocked(startX+dx*mid,startZ+dz*mid,feetY,dx,dz,candidates,midHit,midObject)) {
                lo=mid;
            } else {
                hi=mid; hiHit=midHit; hiObject=midObject;
            }
        }
        outX=startX+dx*lo; outZ=startZ+dz*lo;
        outHit=hiHit; outObject=hiObject;
        return false;
    }
    outX=targetX; outZ=targetZ;
    return true;
}

bool Q950SurfaceY(size_t triIndex,float x,float z,float& outY) {
    if (triIndex>=gWorldTriangles.size()) return false;
    const CollisionTriangle& tri=gWorldTriangles[triIndex];
    if (!IsWalkableTriangleQ910(tri)) return false;
    if (x<tri.minX-0.008f || x>tri.maxX+0.008f || z<tri.minZ-0.008f || z>tri.maxZ+0.008f) return false;
    float u=0.0f,v=0.0f,w=0.0f;
    if (!BarycentricXZ(tri,x,z,u,v,w)) return false;
    outY=tri.a.y*u+tri.b.y*v+tri.c.y*w;
    return true;
}

void Q950ConsiderLandingTriangle(Q950Landing& best,size_t triIndex,uint32_t objectIndex,
                                 float targetX,float targetZ,float feetY,float nx,float nz,
                                 float contactRise,bool topology) {
    if (triIndex>=gWorldTriangles.size()) return;
    const CollisionTriangle& tri=gWorldTriangles[triIndex];
    if (!IsWalkableTriangleQ910(tri)) return;
    const float maxRise=StepHeightQ78B()+Q950_STEP_TOLERANCE;
    const float r=CollisionRadiusQ712();
    const std::array<float,9> ahead{{0.0f,r*0.20f,r*0.40f,r*0.60f,r*0.80f,
                                     r*1.00f,r*1.25f,r*1.50f,r*1.85f}};
    const float lx=-nz,lz=nx;
    const std::array<float,3> lateral{{0.0f,r*0.32f,-r*0.32f}};
    for (float d:ahead) {
        for (float side:lateral) {
            const float px=targetX+nx*d+lx*side;
            const float pz=targetZ+nz*d+lz*side;
            float y=0.0f;
            if (!Q950SurfaceY(triIndex,px,pz,y)) continue;
            const float rise=y-feetY;
            if (rise<0.006f || rise>maxRise) continue;
            const float desired=std::clamp(contactRise,0.006f,maxRise);
            const float score=d*0.12f+std::fabs(rise-desired)*0.22f+std::fabs(side)*0.04f+(topology?-0.025f:0.0f);
            if (!best.found || score<best.score) {
                best.found=true; best.y=y; best.rise=rise; best.score=score;
                best.probeAhead=d; best.triangleIndex=triIndex; best.objectIndex=objectIndex;
                best.topology=topology;
            }
        }
    }

    // If the tread is shorter than our probe lattice, its centroid is still a
    // valid object-top candidate provided it lies directly ahead of the capsule.
    const float cx=(tri.a.x+tri.b.x+tri.c.x)/3.0f;
    const float cy=(tri.a.y+tri.b.y+tri.c.y)/3.0f;
    const float cz=(tri.a.z+tri.b.z+tri.c.z)/3.0f;
    const float relX=cx-targetX,relZ=cz-targetZ;
    const float forward=relX*nx+relZ*nz;
    const float side=std::fabs(relX*(-nz)+relZ*nx);
    const float rise=cy-feetY;
    if (forward>=-r*0.35f && forward<=r*2.20f && side<=r*1.20f && rise>=0.006f && rise<=maxRise) {
        const float desired=std::clamp(contactRise,0.006f,maxRise);
        const float score=std::max(0.0f,forward)*0.14f+side*0.08f+std::fabs(rise-desired)*0.24f+(topology?-0.020f:0.018f);
        if (!best.found || score<best.score) {
            best.found=true; best.y=cy; best.rise=rise; best.score=score;
            best.probeAhead=std::max(0.0f,forward); best.triangleIndex=triIndex;
            best.objectIndex=objectIndex; best.topology=topology;
        }
    }
}

Q950Landing FindQ950Landing(float startX,float startZ,float targetX,float targetZ,float feetY,
                            const CapsuleHitQ910& hit,uint32_t hitObject) {
    Q950Landing best;
    const float dx=targetX-startX,dz=targetZ-startZ;
    const float len=std::sqrt(dx*dx+dz*dz);
    if (len<1e-7f) return best;
    const float nx=dx/len,nz=dz/len;
    const float contactRise=hit.pointY-feetY;

    // 1) Exact topology first. Walk a few rings of authored shared-edge
    // adjacency beginning at the facet that actually hit the capsule.
    if (hitObject!=0xffffffffu && hit.triangleIndex<gHkWeldNeighboursQ801.size()) {
        std::vector<size_t> queue;
        queue.reserve(48u);
        queue.push_back(hit.triangleIndex);
        std::vector<size_t> visited;
        visited.reserve(48u);
        for (size_t cursor=0u;cursor<queue.size() && cursor<48u;++cursor) {
            const size_t triIndex=queue[cursor];
            if (std::find(visited.begin(),visited.end(),triIndex)!=visited.end()) continue;
            visited.push_back(triIndex);
            Q950ConsiderLandingTriangle(best,triIndex,hitObject,targetX,targetZ,feetY,nx,nz,contactRise,true);
            if (triIndex>=gHkWeldNeighboursQ801.size()) continue;
            for (int edge=0;edge<3;++edge) {
                const int32_t neighbour=gHkWeldNeighboursQ801[triIndex][edge];
                if (neighbour<0) continue;
                const size_t ni=static_cast<size_t>(neighbour);
                if (ni>=gQ950TriangleObject.size() || gQ950TriangleObject[ni]!=hitObject) continue;
                if (std::find(visited.begin(),visited.end(),ni)==visited.end()) queue.push_back(ni);
            }
        }
    }

    // 2) Same placed authored object. This crosses separate bhk leaf blocks in
    // a stair NIF while still refusing unrelated world triangles.
    if (hitObject!=0xffffffffu && hitObject<gQ950Objects.size()) {
        for (size_t triIndex:gQ950Objects[hitObject].triangles)
            Q950ConsiderLandingTriangle(best,triIndex,hitObject,targetX,targetZ,feetY,nx,nz,contactRise,false);
    }
    if (best.found) return best;

    // 3) Separate modular piece immediately ahead (ramp-to-ramp etc.). Query the
    // spatial broadphase, then inspect only those nearby coherent objects.
    const float r=CollisionRadiusQ712()*2.25f;
    const float maxRise=StepHeightQ78B()+Q950_STEP_TOLERANCE;
    const Q950Candidates nearby=QueryQ950(targetX-r,targetX+r,feetY-0.05f,feetY+maxRise+0.08f,targetZ-r,targetZ+r);
    for (uint32_t objectIndex:nearby.objects) {
        if (objectIndex==hitObject) continue;
        const Q950CollisionObject& object=gQ950Objects[objectIndex];
        if (object.walkableTriangles==0u) continue;
        for (size_t triIndex:object.triangles)
            Q950ConsiderLandingTriangle(best,triIndex,objectIndex,targetX,targetZ,feetY,nx,nz,contactRise,false);
    }
    return best;
}

bool UpperBlockedQ950(float x,float z,float feetY,float moveX,float moveZ,CapsuleHitQ910& outHit) {
    const float r=CollisionRadiusQ712()+Q910_SWEEP_SKIN+Q950_QUERY_PAD;
    const Q950Candidates nearby=QueryQ950(x-r,x+r,feetY+StepHeightQ78B()-r,
                                           feetY+PLAYER_HEIGHT+r,z-r,z+r);
    outHit=CapsuleHitQ910{};
    float strongestInto=0.0f;
    bool blocked=false;
    auto testTri=[&](size_t triIndex) {
        CapsuleHitQ910 hit;
        if (!UpperBodyTriangleContactQ930(x,z,feetY,triIndex,hit)) return;
        const float hLen=std::sqrt(hit.nx*hit.nx+hit.nz*hit.nz);
        if (hLen<1e-5f) { outHit=hit; blocked=true; strongestInto=-1000.0f; return; }
        const float into=moveX*(hit.nx/hLen)+moveZ*(hit.nz/hLen);
        if (into>=-Q718_DIRECTION_EPS) return;
        if (!blocked || into<strongestInto) { blocked=true; strongestInto=into; outHit=hit; }
    };
    for (uint32_t objectIndex:nearby.objects)
        for (size_t triIndex:gQ950Objects[objectIndex].triangles) testTri(triIndex);
    for (uint32_t triIndex:nearby.legacyTriangles) testTri(triIndex);
    return blocked;
}

bool FindGroundQ950(float x,float z,float feetY,float& groundY) {
    const float probeRadius=CollisionRadiusQ712()*0.62f;
    const float d=probeRadius*0.70710678f;
    const std::array<Vec2,9> probes{{
        {0.0f,0.0f},{probeRadius,0.0f},{-probeRadius,0.0f},{0.0f,probeRadius},{0.0f,-probeRadius},
        {d,d},{d,-d},{-d,d},{-d,-d}
    }};
    const float maxUp=StepHeightQ78B()+Q950_STEP_TOLERANCE;
    const float maxDown=MAX_GROUND_DROP;
    const float r=probeRadius+0.04f;
    const Q950Candidates nearby=QueryQ950(x-r,x+r,feetY-maxDown-0.05f,feetY+maxUp+0.05f,z-r,z+r);
    bool found=false;
    float bestDistance=std::numeric_limits<float>::max();
    float bestY=feetY;
    auto testTri=[&](size_t triIndex) {
        if (triIndex>=gWorldTriangles.size() || !IsWalkableTriangleQ910(gWorldTriangles[triIndex])) return;
        for (const Vec2& p:probes) {
            float y=0.0f;
            if (!Q950SurfaceY(triIndex,x+p.x,z+p.y,y)) continue;
            const float delta=y-feetY;
            if (delta>maxUp || delta<-maxDown) continue;
            const float ad=std::fabs(delta);
            if (!found || ad<bestDistance-0.0005f || (std::fabs(ad-bestDistance)<=0.0005f && y>bestY)) {
                found=true; bestDistance=ad; bestY=y;
            }
        }
    };
    for (uint32_t objectIndex:nearby.objects)
        for (size_t triIndex:gQ950Objects[objectIndex].triangles) testTri(triIndex);
    for (uint32_t triIndex:nearby.legacyTriangles) testTri(triIndex);
    if (!found) return false;
    groundY=bestY;
    return true;
}

uint64_t gQ960LandingLogs = 0u;
uint64_t gQ960StepLogs = 0u;
uint64_t gQ960RejectLogs = 0u;

struct Q960FootprintHit {
    bool found = false;
    float y = 0.0f;
    float radial = 0.0f;
    float forward = 0.0f;
};

bool Q960FootprintSupportOnTriangle(size_t triIndex,
                                    float cx,float cz,float radius,
                                    float feetY,float maxRise,
                                    float nx,float nz,
                                    Q960FootprintHit& out) {
    if (triIndex>=gWorldTriangles.size()) return false;
    const CollisionTriangle& tri=gWorldTriangles[triIndex];
    if (!IsWalkableTriangleQ910(tri)) return false;
    if (cx < tri.minX-radius-0.004f || cx > tri.maxX+radius+0.004f ||
        cz < tri.minZ-radius-0.004f || cz > tri.maxZ+radius+0.004f) return false;

    const float r2=radius*radius;
    bool found=false;
    float bestY=-std::numeric_limits<float>::infinity();
    float bestRadial=std::numeric_limits<float>::infinity();
    float bestForward=-std::numeric_limits<float>::infinity();

    auto consider=[&](float px,float pz) {
        const float ox=px-cx,oz=pz-cz;
        const float d2=ox*ox+oz*oz;
        if (d2>r2+0.000025f) return;
        float u=0.0f,v=0.0f,w=0.0f;
        if (!BarycentricXZ(tri,px,pz,u,v,w)) return;
        // Numerical edge tolerance only. BarycentricXZ already performs the
        // actual projected-triangle containment test.
        if (u < -0.0025f || v < -0.0025f || w < -0.0025f) return;
        const float y=tri.a.y*u+tri.b.y*v+tri.c.y*w;
        const float rise=y-feetY;
        if (rise<0.006f || rise>maxRise) return;
        const float forward=ox*nx+oz*nz;
        // Ignore support that exists only on the far rear edge of the capsule.
        // Center/side overlap and any forward overlap remain valid.
        if (forward < -radius*0.45f) return;
        const float radial=std::sqrt(std::max(0.0f,d2));
        if (!found || y>bestY+0.0005f ||
            (std::fabs(y-bestY)<=0.0005f && radial<bestRadial-0.0005f) ||
            (std::fabs(y-bestY)<=0.0005f && std::fabs(radial-bestRadial)<=0.0005f && forward>bestForward)) {
            found=true;
            bestY=y;
            bestRadial=radial;
            bestForward=forward;
        }
    };

    // Circle centre inside the projected tread.
    consider(cx,cz);

    const std::array<Vec2,3> p{{
        {tri.a.x,tri.a.z},{tri.b.x,tri.b.z},{tri.c.x,tri.c.z}
    }};
    for (const Vec2& v:p) consider(v.x,v.y);

    // Exact circle/segment intersection plus the closest segment point. The
    // union of these candidates contains extrema of a linear height field over
    // triangle-projection intersect circle, which is exactly the footprint
    // support region for a walkable facet.
    for (int edge=0;edge<3;++edge) {
        const Vec2& a=p[edge];
        const Vec2& b=p[(edge+1)%3];
        const float ex=b.x-a.x,ez=b.y-a.y;
        const float len2=ex*ex+ez*ez;
        if (len2<1e-10f) continue;

        const float tClosest=std::clamp(((cx-a.x)*ex+(cz-a.y)*ez)/len2,0.0f,1.0f);
        consider(a.x+ex*tClosest,a.y+ez*tClosest);

        const float fx=a.x-cx,fz=a.y-cz;
        const float A=len2;
        const float B=2.0f*(fx*ex+fz*ez);
        const float C=fx*fx+fz*fz-r2;
        const float disc=B*B-4.0f*A*C;
        if (disc>=0.0f) {
            const float root=std::sqrt(std::max(0.0f,disc));
            const float inv=0.5f/A;
            const float t0=(-B-root)*inv;
            const float t1=(-B+root)*inv;
            if (t0>=0.0f && t0<=1.0f) consider(a.x+ex*t0,a.y+ez*t0);
            if (t1>=0.0f && t1<=1.0f && std::fabs(t1-t0)>1e-6f)
                consider(a.x+ex*t1,a.y+ez*t1);
        }
    }

    if (!found) return false;
    out.found=true;
    out.y=bestY;
    out.radial=bestRadial;
    out.forward=bestForward;
    return true;
}

void Q960ConsiderObjectFootprint(Q950Landing& best,uint32_t objectIndex,
                                 float targetX,float targetZ,float feetY,
                                 float nx,float nz,bool sameObject) {
    if (objectIndex>=gQ950Objects.size()) return;
    const float maxRise=StepHeightQ78B()+Q950_STEP_TOLERANCE;
    // The blocking sweep stops the centre about one capsule radius from a riser.
    // Use the real collision footprint plus a tiny numerical skin so the tread
    // touching that circle is considered immediately, without a probe lattice.
    const float radius=CollisionRadiusQ712()+Q910_SWEEP_SKIN+0.006f;
    const Q950CollisionObject& object=gQ950Objects[objectIndex];
    for (size_t triIndex:object.triangles) {
        Q960FootprintHit hit;
        if (!Q960FootprintSupportOnTriangle(triIndex,targetX,targetZ,radius,
                                            feetY,maxRise,nx,nz,hit)) continue;
        const float rise=hit.y-feetY;
        // Highest reachable support wins. This naturally chooses the first
        // stair tread because the next tread is > one step above current feet.
        // On irregular scrap it selects the real ~0.318 m top even if the
        // capsule contact point on the side face is ~0.35 m high.
        const float score=-rise + hit.radial*0.0005f + (sameObject?-0.001f:0.001f);
        if (!best.found || score<best.score) {
            best.found=true;
            best.y=hit.y;
            best.rise=rise;
            best.score=score;
            best.probeAhead=std::max(0.0f,hit.forward);
            best.triangleIndex=triIndex;
            best.objectIndex=objectIndex;
            best.topology=sameObject;
        }
    }
}

Q950Landing FindQ960FootprintLanding(float startX,float startZ,
                                     float targetX,float targetZ,float feetY,
                                     const CapsuleHitQ910& hit,uint32_t hitObject) {
    Q950Landing best;
    const float dx=targetX-startX,dz=targetZ-startZ;
    const float len=std::sqrt(dx*dx+dz*dz);
    if (len<1e-7f) return best;
    const float nx=dx/len,nz=dz/len;

    // 1) The coherent object that actually blocked the capsule owns the first
    // chance to provide support. No contactRise comparison and no point probes.
    if (hitObject!=0xffffffffu && hitObject<gQ950Objects.size())
        Q960ConsiderObjectFootprint(best,hitObject,targetX,targetZ,feetY,nx,nz,true);

    // 2) If the blocking object has no reachable top, allow an immediately
    // adjacent coherent modular object to provide it (catwalk/ramp transitions).
    if (!best.found) {
        const float radius=CollisionRadiusQ712()+Q910_SWEEP_SKIN+0.020f;
        const float maxRise=StepHeightQ78B()+Q950_STEP_TOLERANCE;
        const Q950Candidates nearby=QueryQ950(targetX-radius,targetX+radius,
                                               feetY-0.03f,feetY+maxRise+0.05f,
                                               targetZ-radius,targetZ+radius);
        for (uint32_t objectIndex:nearby.objects) {
            if (objectIndex==hitObject) continue;
            if (objectIndex>=gQ950Objects.size() || gQ950Objects[objectIndex].walkableTriangles==0u) continue;
            Q960ConsiderObjectFootprint(best,objectIndex,targetX,targetZ,feetY,nx,nz,false);
        }

        // LAND/legacy triangles are not authored Q9.5 objects, but may still be
        // the receiving support at an object boundary. Test only the already
        // spatially-filtered nearby legacy set, never the whole world.
        const float supportRadius=CollisionRadiusQ712()+Q910_SWEEP_SKIN+0.006f;
        for (uint32_t triIndex:nearby.legacyTriangles) {
            Q960FootprintHit fh;
            if (!Q960FootprintSupportOnTriangle(triIndex,targetX,targetZ,supportRadius,
                                                feetY,maxRise,nx,nz,fh)) continue;
            const float rise=fh.y-feetY;
            const float score=-rise+fh.radial*0.0005f+0.002f;
            if (!best.found || score<best.score) {
                best.found=true; best.y=fh.y; best.rise=rise; best.score=score;
                best.probeAhead=std::max(0.0f,fh.forward); best.triangleIndex=triIndex;
                best.objectIndex=0xffffffffu; best.topology=false;
            }
        }
    }

    if (best.found && (gQ960LandingLogs<180u || (gResolveCounter%360u)==0u)) {
        ++gQ960LandingLogs;
        const Q950CollisionObject* object=best.objectIndex<gQ950Objects.size()?&gQ950Objects[best.objectIndex]:nullptr;
        Q6G_LOGI("Q9.6 FOOTPRINT LANDING: hitObject=%u landingObject=%u ref=%08X contactRise=%.3f landingRise=%.3f overlapForward=%.3f sameObject=%d tri=%zu exactCircleTriangle=1",
                 hitObject,best.objectIndex,object?object->refFormId:0u,
                 hit.pointY-feetY,best.rise,best.probeAhead,best.topology?1:0,best.triangleIndex);
    }
    return best;
}

bool TryCoherentStepQ960(float startX,float startZ,float targetX,float targetZ,float feetY,
                         const CapsuleHitQ910& hit,uint32_t hitObject,float& outFeetY) {
    const float dx=targetX-startX,dz=targetZ-startZ;
    const float len=std::sqrt(dx*dx+dz*dz);
    if (len<1e-7f) return false;

    const Q950Landing landing=FindQ960FootprintLanding(startX,startZ,targetX,targetZ,feetY,hit,hitObject);
    if (landing.found) {
        CapsuleHitQ910 upper;
        if (!UpperBlockedQ950(targetX,targetZ,landing.y,dx,dz,upper)) {
            outFeetY=landing.y;
            if (gQ960StepLogs<180u || (gResolveCounter%360u)==0u) {
                ++gQ960StepLogs;
                const Q950CollisionObject* object=landing.objectIndex<gQ950Objects.size()?&gQ950Objects[landing.objectIndex]:nullptr;
                Q6G_LOGI("Q9.6 SHAPE STEP: hitObject=%u landingObject=%u ref=%08X contactRise=%.3f landingRise=%.3f sameObject=%d tri=%zu supportMode=exact-footprint",
                         hitObject,landing.objectIndex,object?object->refFormId:0u,
                         hit.pointY-feetY,landing.rise,landing.topology?1:0,landing.triangleIndex);
            }
            return true;
        }
        // A real raised support was found. If the upper capsule cannot occupy
        // the resulting pose, do NOT silently fall back to old-floor traversal;
        // that was the Q9.5 stair behaviour we are removing.
        if (gQ960RejectLogs<120u || (gResolveCounter%360u)==0u) {
            ++gQ960RejectLogs;
            Q6G_LOGI("Q9.6 STEP REJECT: phase=UPPER hitObject=%u contactRise=%.3f landingRise=%.3f upperHitRise=%.3f tri=%zu",
                     hitObject,hit.pointY-feetY,landing.rise,upper.pointY-landing.y,upper.triangleIndex);
        }
        return false;
    }

    if (gQ960RejectLogs<120u || (gResolveCounter%360u)==0u) {
        ++gQ960RejectLogs;
        Q6G_LOGI("Q9.6 STEP REJECT: phase=LANDING hitObject=%u contactRise=%.3f reason=no-footprint-support",
                 hitObject,hit.pointY-feetY);
    }

    // Preserve Q9.5's proven low-lip/catwalk behaviour only when there is no
    // raised footprint support at all.
    float floorY=feetY;
    CapsuleHitQ910 upper;
    if (FindGroundQ950(targetX,targetZ,feetY,floorY) &&
        std::fabs(floorY-feetY)<=0.08f &&
        !UpperBlockedQ950(targetX,targetZ,floorY,dx,dz,upper)) {
        outFeetY=floorY;
        return true;
    }
    return false;
}

bool TryCoherentStepQ950(float startX,float startZ,float targetX,float targetZ,float feetY,
                         const CapsuleHitQ910& hit,uint32_t hitObject,float& outFeetY) {
    const float dx=targetX-startX,dz=targetZ-startZ;
    const float len=std::sqrt(dx*dx+dz*dz);
    if (len<1e-7f) return false;

    const Q950Landing landing=FindQ950Landing(startX,startZ,targetX,targetZ,feetY,hit,hitObject);
    if (landing.found) {
        CapsuleHitQ910 upper;
        if (!UpperBlockedQ950(targetX,targetZ,landing.y,dx,dz,upper)) {
            outFeetY=landing.y;
            if (gQ950StepLogs<120u || (gResolveCounter%360u)==0u) {
                ++gQ950StepLogs;
                const Q950CollisionObject* object=landing.objectIndex<gQ950Objects.size()?&gQ950Objects[landing.objectIndex]:nullptr;
                Q6G_LOGI("Q9.5 SHAPE STEP: hitObject=%u landingObject=%u ref=%08X contactRise=%.3f landingRise=%.3f probeAhead=%.3f topology=%d tri=%zu",
                         hitObject,landing.objectIndex,object?object->refFormId:0u,
                         hit.pointY-feetY,landing.rise,landing.probeAhead,landing.topology?1:0,landing.triangleIndex);
            }
            return true;
        }
    }

    // No raised top: preserve Q9.3's useful low-lip/wood/pipe behaviour, but
    // only when ordinary support still exists and the target has no upper-body
    // collision. This is an envelope rule, not a triangle-height rule.
    float floorY=feetY;
    CapsuleHitQ910 upper;
    if (FindGroundQ950(targetX,targetZ,feetY,floorY) &&
        std::fabs(floorY-feetY)<=0.08f &&
        !UpperBlockedQ950(targetX,targetZ,floorY,dx,dz,upper)) {
        outFeetY=floorY;
        if (gQ950LowPassLogs<60u || (gResolveCounter%480u)==0u) {
            ++gQ950LowPassLogs;
            Q6G_LOGI("Q9.5 LOW ENVELOPE PASS: hitObject=%u contactRise=%.3f floorDelta=%.3f",
                     hitObject,hit.pointY-feetY,floorY-feetY);
        }
        return true;
    }
    return false;
}

Vec2 ProjectAlongWallQ950(Vec2 move,const CapsuleHitQ910& hit) {
    const float hLen=std::sqrt(hit.nx*hit.nx+hit.nz*hit.nz);
    if (hLen<1e-6f) return Vec2{0.0f,0.0f};
    const float nx=hit.nx/hLen,nz=hit.nz/hLen;
    const float into=move.x*nx+move.y*nz;
    if (into<0.0f) { move.x-=nx*into; move.y-=nz*into; }
    return move;
}

bool MoveExteriorSubstepQ950(float& x,float& z,float& feetY,
                             float moveX,float moveZ,bool& steppedThisFrame,
                             uint32_t& contactSurfaces) {
    if (moveX*moveX+moveZ*moveZ<1e-10f) return true;
    EnsureQ950SpatialIndex();
    static bool loggedReady=false;
    if (!loggedReady) {
        loggedReady=true;
        Q6G_LOGI("Q9.6 CONTROLLER READY: mode=coherent-placed-objects+spatial-grid+exact-footprint-support+shape-narrowphase triangleSoupStepPath=DISABLED probeLatticeStepPath=DISABLED fullWorldMovementScans=0 oldQ91toQ94StepPath=disabled");
    }

    const float startX=x,startZ=z;
    const float targetX=x+moveX,targetZ=z+moveZ;
    float castX=x,castZ=z;
    CapsuleHitQ910 hit; uint32_t hitObject=0xffffffffu;
    if (SweepQ950(startX,startZ,targetX,targetZ,feetY,castX,castZ,hit,hitObject)) {
        x=targetX; z=targetZ;
        return true;
    }
    ++contactSurfaces;

    float stepY=feetY;
    if (TryCoherentStepQ960(startX,startZ,targetX,targetZ,feetY,hit,hitObject,stepY)) {
        x=targetX; z=targetZ; feetY=stepY; steppedThisFrame=true;
        return true;
    }

    x=castX; z=castZ;
    CapsuleHitQ910 wallHit=hit;
    uint32_t wallObject=hitObject;
    for (int iteration=0;iteration<2;++iteration) {
        Vec2 remaining{targetX-x,targetZ-z};
        if (remaining.x*remaining.x+remaining.y*remaining.y<0.00000025f) break;
        const Vec2 slide=ProjectAlongWallQ950(remaining,wallHit);
        if (slide.x*slide.x+slide.y*slide.y<0.00000025f) break;
        if (slide.x*remaining.x+slide.y*remaining.y<=0.0f) break;
        const float sxTarget=x+slide.x,szTarget=z+slide.y;
        float sx=x,sz=z; CapsuleHitQ910 slideHit; uint32_t slideObject=0xffffffffu;
        if (SweepQ950(x,z,sxTarget,szTarget,feetY,sx,sz,slideHit,slideObject)) {
            x=sxTarget; z=szTarget;
            if (gQ950SlideLogs<30u) {
                ++gQ950SlideLogs;
                Q6G_LOGI("Q9.5 WALL SLIDE: requested=(%.3f %.3f) projected=(%.3f %.3f)",
                         remaining.x,remaining.y,slide.x,slide.y);
            }
            return true;
        }
        float slideStepY=feetY;
        if (TryCoherentStepQ960(x,z,sxTarget,szTarget,feetY,slideHit,slideObject,slideStepY)) {
            x=sxTarget; z=szTarget; feetY=slideStepY; steppedThisFrame=true;
            return true;
        }
        x=sx; z=sz; wallHit=slideHit; wallObject=slideObject; ++contactSurfaces;
    }

    if (gQ950BlockedLogs<40u || (gResolveCounter%480u)==0u) {
        ++gQ950BlockedLogs;
        uint32_t ref=0u;
        if (wallObject<gQ950Objects.size()) ref=gQ950Objects[wallObject].refFormId;
        const CollisionTriangle* tri=(hit.hit&&hit.triangleIndex<gWorldTriangles.size())?&gWorldTriangles[hit.triangleIndex]:nullptr;
        const auto src=tri?gSurfaceSourcesQ722.find(tri->surfaceKeyQ714):gSurfaceSourcesQ722.end();
        Q6G_LOGI("Q9.5 BLOCKED: object=%u ref=%08X EDID=%s start=(%.3f %.3f) pos=(%.3f %.3f) target=(%.3f %.3f) contactRise=%.3f",
                 hitObject,ref,(src!=gSurfaceSourcesQ722.end()&&!src->second.editorId.empty())?src->second.editorId.c_str():"<none>",
                 startX,startZ,x,z,targetX,targetZ,hit.pointY-feetY);
    }
    return (x-startX)*(x-startX)+(z-startZ)*(z-startZ)>0.00000025f;
}

Vec2 ProjectAlongWallQ910(Vec2 move,const CapsuleHitQ910& hit) {
    const float hLen=std::sqrt(hit.nx*hit.nx+hit.nz*hit.nz);
    if (hLen < 1e-6f) return Vec2{0.0f,0.0f};
    const float nx=hit.nx/hLen,nz=hit.nz/hLen;
    const float into=move.x*nx+move.y*nz;
    if (into < 0.0f) {
        move.x-=nx*into;
        move.y-=nz*into;
    }
    // Never allow collision response to reverse the requested direction.
    return move;
}

bool MoveExteriorSubstepQ910(float& x,float& z,float& feetY,
                             float moveX,float moveZ,bool& steppedThisFrame,
                             uint32_t& contactSurfaces) {
    if (moveX*moveX+moveZ*moveZ < 1e-10f) return true;
    if (gQ910ReadyLogs < 1u) {
        ++gQ910ReadyLogs;
        Q6G_LOGI("Q9.4 STANDARD CONTROLLER READY: mode=raised-support-step-envelope+local-candidates+target-upper-body+wall-projection wholeTriMaxYVeto=disabled raisedForwardVeto=disabled oldSimplex=disabled");
    }

    const float startX=x,startZ=z;
    const float targetX=x+moveX,targetZ=z+moveZ;
    float castX=x,castZ=z;
    CapsuleHitQ910 hit;
    if (SweepCapsuleHorizontalQ910(startX,startZ,targetX,targetZ,feetY,
                                   castX,castZ,hit)) {
        x=targetX; z=targetZ;
        return true;
    }
    ++contactSurfaces;

    // STEP IS TRIED BEFORE COMMITTING TO TIME-OF-IMPACT. This is the key change:
    // a valid ledge never gets one or more frames of resisted movement first.
    float stepY=feetY;
    if (TryRaisedStepEnvelopeQ940(startX,startZ,targetX,targetZ,feetY,hit,stepY)) {
        x=targetX; z=targetZ; feetY=stepY;
        steppedThisFrame=true;
        return true;
    }

    // Not a step: now, and only now, move to impact and perform ordinary wall
    // sliding by removing the into-wall component. No manifold/simplex solver.
    x=castX; z=castZ;
    CapsuleHitQ910 wallHit=hit;
    for (int iteration=0; iteration<3; ++iteration) {
        Vec2 remaining{targetX-x,targetZ-z};
        if (remaining.x*remaining.x+remaining.y*remaining.y < 0.00000025f) break;
        const Vec2 slide=ProjectAlongWallQ910(remaining,wallHit);
        if (slide.x*slide.x+slide.y*slide.y < 0.00000025f) break;
        if (slide.x*remaining.x+slide.y*remaining.y <= 0.0f) break;

        const float slideTargetX=x+slide.x,slideTargetZ=z+slide.y;
        float sx=x,sz=z;
        CapsuleHitQ910 slideHit;
        if (SweepCapsuleHorizontalQ910(x,z,slideTargetX,slideTargetZ,feetY,
                                       sx,sz,slideHit)) {
            if (gQ910SlideLogs < 160u || (gResolveCounter%240u)==0u) {
                ++gQ910SlideLogs;
                Q6G_LOGI("Q9.4 WALL SLIDE: requested=(%.3f %.3f) projected=(%.3f %.3f)",
                         remaining.x,remaining.y,slide.x,slide.y);
            }
            x=slideTargetX; z=slideTargetZ;
            return true;
        }

        // A second obstacle encountered while sliding gets the same standard
        // step opportunity before its collision response is accepted.
        float slideStepY=feetY;
        if (TryRaisedStepEnvelopeQ940(x,z,slideTargetX,slideTargetZ,feetY,
                                   slideHit,slideStepY)) {
            x=slideTargetX; z=slideTargetZ; feetY=slideStepY;
            steppedThisFrame=true;
            return true;
        }
        x=sx; z=sz; wallHit=slideHit; ++contactSurfaces;
    }

    if (gQ910BlockedLogs < 160u || (gResolveCounter%240u)==0u) {
        ++gQ910BlockedLogs;
        const CollisionTriangle* tri=(hit.hit && hit.triangleIndex<gWorldTriangles.size())
                                     ? &gWorldTriangles[hit.triangleIndex] : nullptr;
        uint64_t surface=tri?tri->surfaceKeyQ714:0u;
        const auto sourceIt=gSurfaceSourcesQ722.find(surface);
        if (sourceIt!=gSurfaceSourcesQ722.end()) {
            Q6G_LOGI("Q9.4 BLOCKER SOURCE: surface=%llu ref=%08X EDID=%s model=%s",
                     static_cast<unsigned long long>(surface),sourceIt->second.refFormId,
                     sourceIt->second.editorId.empty()?"<none>":sourceIt->second.editorId.c_str(),
                     sourceIt->second.modelPath.c_str());
        }
        Q6G_LOGI("Q9.4 BLOCKED: start=(%.3f %.3f) pos=(%.3f %.3f) target=(%.3f %.3f) contactRise=%.3f",
                 startX,startZ,x,z,targetX,targetZ,hit.pointY-feetY);
    }
    return (x-startX)*(x-startX)+(z-startZ)*(z-startZ)>0.00000025f;
}

bool FindHkGroundQ800(float x, float z, float feetY, float& groundY) {
    const HkSupportQ800 support = FindHkSupportAtQ800(x, z, feetY,
                                                      StepHeightQ78B(),
                                                      MAX_GROUND_DROP);
    if (!support.found) return false;
    groundY = support.y;
    return true;
}

void LogTerrainCellsQ719() {
    if (gTerrainCellLogsQ719) return;
    gTerrainCellLogsQ719 = true;
    const auto& terrain = GetFo3TerrainQ76();
    Q6G_LOGI("Q9.4 LAND DIAGNOSTICS: cells=%zu note=child-worldspace-may-mix-local-and-parent-LAND", terrain.size());
    for (const Fo3TerrainCellQ76& cell : terrain) {
        if (cell.heights.empty()) continue;
        const auto range = std::minmax_element(cell.heights.begin(), cell.heights.end());
        Q6G_LOGI("Q9.4 LAND CELL: cell=%08X LAND=%08X grid=(%d,%d) minHeight=%.1f maxHeight=%.1f",
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
        Q6G_LOGI("Q9.4 PLAYER RESET: bodyCenter=(0,0) source=XTEL newCollisionReady=%d",
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
        Q6G_LOGI("Q9.4 CONTROLLER READY: mode=coherent-placed-objects+spatial-grid+topology-step+shape-narrowphase persistentOverlap=none depenetration=0 maxStep=%.3f depthEps=%.4f",
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
        if (!MoveExteriorSubstepQ950(x, z, feetY, subDx, subDz,
                                     steppedThisFrame, contactSurfaces)) break;
    }
    float groundY = feetY;
    bool grounded = FindGroundQ950(x, z, feetY, groundY);
    const bool authoredGrounded = grounded;
    float terrainPlayerY = currentPlayerYOffset;
    const bool terrainGrounded = SampleFo3TerrainGroundQ77(x, z, &terrainPlayerY);
    if (terrainGrounded) {
        const float terrainGroundY = gCollisionFloorY + terrainPlayerY;
        if (!grounded || terrainGroundY > groundY) { groundY = terrainGroundY; grounded = true; }
        if (gTerrainGroundLogCountQ77 < 20u || (gResolveCounter % 360u) == 0u) {
            ++gTerrainGroundLogCountQ77;
            Q6G_LOGI("Q9.4 LAND GROUND: pos=(%.3f %.3f) terrainY=%.3f selectedGroundY=%.3f authoredGround=%d",
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
        Q6G_LOGI("Q9.4 CONTACT: desired=(%.3f %.3f) resolved=(%.3f %.3f) correction=(%.3f %.3f) blockers=%u stepped=%d grounded=%d playerY=%.3f",
                 desiredX, desiredZ, x, z, correctionX, correctionZ, contactSurfaces,
                 steppedThisFrame ? 1 : 0, grounded ? 1 : 0, playerYOffset);
    }
    return true;
}
