# Q8.2: universal validated low-obstacle traversal.
#
# Gameplay rule: a contact below the normal exterior step height is not allowed
# to cancel horizontal movement when there is a genuinely walkable landing just
# beyond it and the full capsule can clear the obstacle. This is geometry-only:
# no model names, editor IDs or material exceptions.
#
# Safety gates:
#   1) every blocking contact is locally at/below StepHeightQ78B(),
#   2) a supported walkable landing exists ahead within the same step range,
#   3) more than one footprint probe supports that landing,
#   4) an elevated full-capsule forward cast clears the obstacle,
#   5) there is no overhead collision at start/landing.
# If any gate fails, the normal Q8.1 manifold/simplex collision response remains.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q801.inc"
     Q802_CONTROLLER_SOURCE)
string(REPLACE "Q8.1" "Q8.2"
       Q802_CONTROLLER_SOURCE "${Q802_CONTROLLER_SOURCE}")

set(Q802_LOW_OBSTACLE_SOURCE [=[
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
    const HkLowLandingQ802 landing = FindHkLowLandingQ802(
        startX, startZ, targetX, targetZ, feetY, preferRaisedLanding);
    if (!landing.found) {
        if (gHkLowNoLandingLogsQ802 < 120u || (gResolveCounter % 240u) == 0u) {
            ++gHkLowNoLandingLogsQ802;
            Q6G_LOGI("Q8.2 LOW OBSTACLE NO LANDING: from=(%.3f %.3f) target=(%.3f %.3f) highestContactRise=%.3f maxStep=%.3f blockers=%zu",
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
            Q6G_LOGI("Q8.2 LOW OBSTACLE CAPSULE BLOCKED: reason=overhead from=(%.3f %.3f) target=(%.3f %.3f) landingRise=%.3f",
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
            Q6G_LOGI("Q8.2 LOW OBSTACLE CAPSULE BLOCKED: reason=raised-cast from=(%.3f %.3f) target=(%.3f %.3f) landingRise=%.3f raisedBlockers=%zu raw=%zu",
                     startX, startZ, targetX, targetZ, landing.y - feetY,
                     raisedBlockers.size(), raisedRaw);
        }
        return false;
    }

    outFeetY = landing.y;
    if (gHkLowPassLogsQ802 < 160u || (gResolveCounter % 240u) == 0u) {
        ++gHkLowPassLogsQ802;
        Q6G_LOGI("Q8.2 LOW OBSTACLE PASS: from=(%.3f %.3f) target=(%.3f %.3f) highestContactRise=%.3f landingRise=%.3f supportPoints=%d supportTri=%zu raisedClear=1",
                 startX, startZ, targetX, targetZ,
                 highestContactRise, landing.y - feetY,
                 landing.supportPoints, landing.triangleIndex);
    }
    return true;
}

]=])

string(REPLACE
    "bool MoveExteriorSubstepQ800(float& x, float& z, float& feetY,"
    "${Q802_LOW_OBSTACLE_SOURCE}bool MoveExteriorSubstepQ800(float& x, float& z, float& feetY,"
    Q802_CONTROLLER_SOURCE "${Q802_CONTROLLER_SOURCE}")

# Q8.2 makes validated low-obstacle traversal the first step response. If it
# rejects the geometry, execution falls through to the existing manifold/simplex
# collision handling exactly as before.
string(REPLACE
    "if (TryHkStepQ800(x, z, targetX, targetZ, feetY, blockers, stepY))"
    "if (TryHkLowObstacleTraverseQ802(x, z, targetX, targetZ, feetY, blockers, stepY))"
    Q802_CONTROLLER_SOURCE "${Q802_CONTROLLER_SOURCE}")
string(REPLACE
    "if (TryHkStepQ800(x, z, slideTargetX, slideTargetZ,\n                          feetY, slideBlockers, slideStepY))"
    "if (TryHkLowObstacleTraverseQ802(x, z, slideTargetX, slideTargetZ,\n                                      feetY, slideBlockers, slideStepY))"
    Q802_CONTROLLER_SOURCE "${Q802_CONTROLLER_SOURCE}")

string(REPLACE
    "mode=fo3-havok-proxy-cleanroom+authored-welding+linear-cast+persistent-point-manifold+simplex+support-check persistentOverlap=keep-distance"
    "mode=validated-low-obstacle-traversal+authored-welding+linear-cast+support-footprint+simplex persistentOverlap=keep-distance"
    Q802_CONTROLLER_SOURCE "${Q802_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q802.inc"
     "${Q802_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q801.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q802.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
