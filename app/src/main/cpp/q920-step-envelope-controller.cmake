# Q9.2: stop requiring the raised capsule to clear the whole stair flight.
#
# Q9.1 proved the resistance was caused by its raised FORWARD sweep: after
# lifting over one riser, the capsule immediately contacted the next riser and
# the move fell back to wall projection. Q9.2 treats geometry wholly inside the
# current step envelope as non-blocking horizontal detail, preserves the full
# requested X/Z move, then resolves feet onto the highest valid walkable support
# within one step. Tall geometry remains a wall.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q910.inc"
     Q920_CONTROLLER_SOURCE)
string(REPLACE "Q9.1" "Q9.2"
       Q920_CONTROLLER_SOURCE "${Q920_CONTROLLER_SOURCE}")

set(Q920_STEP_ENVELOPE_SOURCE [=[
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

        // The defining Q9.2 rule: non-walkable geometry that ends within one
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
            Q6G_LOGI("Q9.2 STEP ENVELOPE FAIL: phase=INITIAL reason=tall-face contactRise=%.3f topRise=%.3f maxStep=%.3f tri=%zu",
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
            Q6G_LOGI("Q9.2 STEP ENVELOPE FAIL: phase=SUPPORT reason=no-support contactRise=%.3f topRise=%.3f",
                     initialHit.pointY-feetY, initialTopRise);
        }
        return false;
    }

    const float rise = landing.y - feetY;
    if (rise > StepHeightQ78B() + PLAYER_SKIN + 0.012f ||
        rise < -Q910_MAX_STEP_DROP) {
        if (gQ920EnvelopeFailLogs < 160u || (gResolveCounter % 240u) == 0u) {
            ++gQ920EnvelopeFailLogs;
            Q6G_LOGI("Q9.2 STEP ENVELOPE FAIL: phase=SUPPORT reason=rise-range landingRise=%.3f",
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
            Q6G_LOGI("Q9.2 STEP ENVELOPE FAIL: phase=FINAL reason=headroom landingRise=%.3f",
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
            Q6G_LOGI("Q9.2 STEP ENVELOPE FAIL: phase=FINAL reason=hard-above-envelope landingRise=%.3f hardTopRise=%.3f hardContactRise=%.3f tri=%zu",
                     rise, hardTri.maxY-landing.y,
                     hardHit.pointY-landing.y, hardHit.triangleIndex);
        }
        return false;
    }

    outFeetY = landing.y;
    if (gQ920EnvelopePassLogs < 320u || (gResolveCounter % 240u) == 0u) {
        ++gQ920EnvelopePassLogs;
        Q6G_LOGI("Q9.2 STEP ENVELOPE PASS: forward=%.3f contactRise=%.3f initialTopRise=%.3f landingRise=%.3f support=%d tri=%zu raisedForwardVeto=0",
                 len, initialHit.pointY-feetY, initialTopRise, rise,
                 landing.supportPoints, landing.triangleIndex);
    }
    return true;
}

]=])

string(REPLACE
    "Vec2 ProjectAlongWallQ910(Vec2 move,const CapsuleHitQ910& hit) {"
    "${Q920_STEP_ENVELOPE_SOURCE}Vec2 ProjectAlongWallQ910(Vec2 move,const CapsuleHitQ910& hit) {"
    Q920_CONTROLLER_SOURCE "${Q920_CONTROLLER_SOURCE}")

# Primary blocked movement: Q9.2 envelope is the only step decision.
string(REPLACE
    "if (TryStandardStepQ910(startX,startZ,targetX,targetZ,feetY,hit,stepY))"
    "if (TryStepEnvelopeQ920(startX,startZ,targetX,targetZ,feetY,hit,stepY))"
    Q920_CONTROLLER_SOURCE "${Q920_CONTROLLER_SOURCE}")

# Same rule for a second obstacle encountered during ordinary wall sliding.
string(REPLACE
    "if (TryStandardStepQ910(x,z,slideTargetX,slideTargetZ,feetY,\n                                slideHit,slideStepY))"
    "if (TryStepEnvelopeQ920(x,z,slideTargetX,slideTargetZ,feetY,\n                              slideHit,slideStepY))"
    Q920_CONTROLLER_SOURCE "${Q920_CONTROLLER_SOURCE}")

string(REPLACE
    "mode=raw-capsule-sweep+up-forward-down+wall-projection oldStepPath=disabled oldSimplex=disabled"
    "mode=lower-body-step-envelope+post-move-support+wall-projection raisedForwardVeto=disabled oldSimplex=disabled"
    Q920_CONTROLLER_SOURCE "${Q920_CONTROLLER_SOURCE}")
string(REPLACE
    "mode=standard-capsule-controller+up-forward-down+wall-projection persistentOverlap=none"
    "mode=step-envelope+post-move-support+wall-projection persistentOverlap=none"
    Q920_CONTROLLER_SOURCE "${Q920_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q920.inc"
     "${Q920_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q910.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q920.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
