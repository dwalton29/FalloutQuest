# Q8.3: fix validated low-obstacle landing selection.
#
# Q8.2 asked FindHkSupportAtQ800 for one support surface, which intentionally
# returns the surface closest to the current feet height. On a stair riser that
# means the old floor (rise ~0) wins over the next tread (e.g. +0.245m). Q8.2
# then rejects that old floor when it knows a positive riser was hit and reports
# NO LANDING without ever considering the tread.
#
# Q8.3 enumerates every walkable triangle under each forward probe, validates
# footprint support for each candidate, and chooses the best legal raised tread
# when a positive obstacle was hit. No model names/material exceptions.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q802.inc"
     Q803_CONTROLLER_SOURCE)
string(REPLACE "Q8.2" "Q8.3"
       Q803_CONTROLLER_SOURCE "${Q803_CONTROLLER_SOURCE}")

set(Q803_LANDING_SOURCE [=[
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
            Q6G_LOGI("Q8.3 LOW OBSTACLE LANDING SELECT: probeAhead=%.3f rise=%.3f supportPoints=%d supportTri=%zu preferRaised=%d",
                     distance, bestAtProbe.y - feetY, bestAtProbe.supportPoints,
                     bestAtProbe.triangleIndex, preferRaisedLanding ? 1 : 0);
            return bestAtProbe;
        }
    }
    return result;
}

]=])

# Add the corrected selector immediately before Q8.2/Q8.3's traversal routine.
string(REPLACE
    "bool TryHkLowObstacleTraverseQ802(float startX, float startZ,"
    "${Q803_LANDING_SOURCE}bool TryHkLowObstacleTraverseQ802(float startX, float startZ,"
    Q803_CONTROLLER_SOURCE "${Q803_CONTROLLER_SOURCE}")

# Route only the landing selection through the all-surface enumerator. Keep the
# rest of the Q8.2 safety gates (footprint, raised capsule, overhead, walls).
string(REPLACE
    "const HkLowLandingQ802 landing = FindHkLowLandingQ802("
    "const HkLowLandingQ802 landing = FindHkLowLandingQ803("
    Q803_CONTROLLER_SOURCE "${Q803_CONTROLLER_SOURCE}")

string(REPLACE
    "mode=validated-low-obstacle-traversal+authored-welding+linear-cast+support-footprint+simplex persistentOverlap=keep-distance"
    "mode=validated-low-obstacle-traversal+all-surface-landing+authored-welding+linear-cast+support-footprint+simplex persistentOverlap=keep-distance"
    Q803_CONTROLLER_SOURCE "${Q803_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q803.inc"
     "${Q803_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q802.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q803.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
