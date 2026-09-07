# Q9.3: fix Q9.2's last whole-triangle-height mistake.
#
# Q9.2 still rejected a valid stair riser when the CONTACT was only ~0.245 m
# above the feet but that triangle's remote vertex/maxY reached ~0.400 m. That
# recreated the old false-wall problem. Q9.3 never classifies a step from the
# triangle's global maxY. It first resolves the destination support, then sweeps
# only the character portion ABOVE the one-step envelope through the requested
# move. Lower-body geometry may overlap during a step; true walls/rails/ceilings
# still collide because they intersect the upper-body capsule locally.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q920.inc"
     Q930_CONTROLLER_SOURCE)
string(REPLACE "Q9.2" "Q9.3"
       Q930_CONTROLLER_SOURCE "${Q930_CONTROLLER_SOURCE}")

set(Q930_LOCAL_ENVELOPE_SOURCE [=[
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
            Q6G_LOGI("Q9.3 STEP ENVELOPE FAIL: phase=SUPPORT reason=no-support contactRise=%.3f tri=%zu",
                     initialHit.pointY-feetY, initialHit.triangleIndex);
        }
        return false;
    }

    const float rise = landing.y - feetY;
    if (rise > StepHeightQ78B() + PLAYER_SKIN + 0.012f ||
        rise < -Q910_MAX_STEP_DROP) {
        if (gQ930EnvelopeFailLogs < 200u || (gResolveCounter % 240u) == 0u) {
            ++gQ930EnvelopeFailLogs;
            Q6G_LOGI("Q9.3 STEP ENVELOPE FAIL: phase=SUPPORT reason=rise-range contactRise=%.3f landingRise=%.3f",
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
            Q6G_LOGI("Q9.3 STEP ENVELOPE FAIL: phase=UPPER reason=sweep-block contactRise=%.3f landingRise=%.3f upperHitRise=%.3f tri=%zu",
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
                Q6G_LOGI("Q9.3 STEP ENVELOPE FAIL: phase=FINAL reason=upper-overlap contactRise=%.3f landingRise=%.3f overlapHitRise=%.3f tri=%zu",
                         initialHit.pointY-feetY, rise,
                         overlapHit.pointY-landing.y, overlapHit.triangleIndex);
            }
            return false;
        }
    }

    outFeetY = landing.y;
    if (gQ930EnvelopePassLogs < 400u || (gResolveCounter % 240u) == 0u) {
        ++gQ930EnvelopePassLogs;
        Q6G_LOGI("Q9.3 STEP ENVELOPE PASS: forward=%.3f contactRise=%.3f landingRise=%.3f support=%d tri=%zu localUpperClear=1 wholeTriMaxYVeto=0 raisedForwardVeto=0",
                 len, initialHit.pointY-feetY, rise,
                 landing.supportPoints, landing.triangleIndex);
    }
    return true;
}

]=])

string(REPLACE
    "Vec2 ProjectAlongWallQ910(Vec2 move,const CapsuleHitQ910& hit) {"
    "${Q930_LOCAL_ENVELOPE_SOURCE}Vec2 ProjectAlongWallQ910(Vec2 move,const CapsuleHitQ910& hit) {"
    Q930_CONTROLLER_SOURCE "${Q930_CONTROLLER_SOURCE}")

# Q9.3 owns both step decisions. Q9.2 remains compiled for comparison only.
string(REPLACE
    "if (TryStepEnvelopeQ920(startX,startZ,targetX,targetZ,feetY,hit,stepY))"
    "if (TryLocalStepEnvelopeQ930(startX,startZ,targetX,targetZ,feetY,hit,stepY))"
    Q930_CONTROLLER_SOURCE "${Q930_CONTROLLER_SOURCE}")
string(REPLACE
    "if (TryStepEnvelopeQ920(x,z,slideTargetX,slideTargetZ,feetY,\n                              slideHit,slideStepY))"
    "if (TryLocalStepEnvelopeQ930(x,z,slideTargetX,slideTargetZ,feetY,\n                                   slideHit,slideStepY))"
    Q930_CONTROLLER_SOURCE "${Q930_CONTROLLER_SOURCE}")

string(REPLACE
    "mode=lower-body-step-envelope+post-move-support+wall-projection raisedForwardVeto=disabled oldSimplex=disabled"
    "mode=local-step-envelope+upper-body-sweep+post-move-support+wall-projection wholeTriMaxYVeto=disabled raisedForwardVeto=disabled oldSimplex=disabled"
    Q930_CONTROLLER_SOURCE "${Q930_CONTROLLER_SOURCE}")
string(REPLACE
    "mode=step-envelope+post-move-support+wall-projection persistentOverlap=none"
    "mode=local-step-envelope+upper-body-sweep+post-move-support+wall-projection persistentOverlap=none"
    Q930_CONTROLLER_SOURCE "${Q930_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q930.inc"
     "${Q930_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q920.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q930.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
