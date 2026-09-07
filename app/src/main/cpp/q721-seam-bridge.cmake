# Q7.21: bridge internal/end-cap seams between authored walkable modular pieces.
#
# Q7.20 proved that many Megaton snag contacts clear when the capsule is raised,
# but its final landing test samples only the exact blocked endpoint. At joins
# such as MegatonRamp -> MegatonRamp, ScrapGroundPlates, and WoodPlankGroup,
# that endpoint can lie on the thin vertical end face itself even though valid
# walkable support exists a few centimetres beyond it. Fallout/Havok treats the
# adjoining pieces as a continuous character-support manifold; our triangle
# controller was still treating the end face as a wall.
#
# Keep Q7.20's strict raised-capsule proof. Only when that raised path is clear,
# probe forward by fractions of the capsule radius for nearby walkable support.
# Flat, descending, and small-rising joins are accepted; real walls remain
# blocked because the raised capsule still intersects them.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q720.inc"
     Q721_CONTROLLER_SOURCE)
string(REPLACE "Q7.20" "Q7.21"
       Q721_CONTROLLER_SOURCE "${Q721_CONTROLLER_SOURCE}")

set(Q721_BEFORE_COUNTERS "${Q721_CONTROLLER_SOURCE}")
string(REPLACE
    "uint64_t gStepOverLogsQ718 = 0u;"
    "uint64_t gStepOverLogsQ718 = 0u;\nuint64_t gSeamBridgeLogsQ721 = 0u;"
    Q721_CONTROLLER_SOURCE "${Q721_CONTROLLER_SOURCE}")
if(Q721_CONTROLLER_SOURCE STREQUAL Q721_BEFORE_COUNTERS)
    message(FATAL_ERROR "Q7.21 could not add seam-bridge log counter")
endif()

set(Q721_SEAM_HELPER [=[
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

    // We should already be standing on authored walkable support. This prevents
    // the bridge test from becoming a generic ledge/air traversal mechanism.
    float startGroundY = feetY;
    if (!FindGround(startX, startZ, feetY, startGroundY)) return false;
    if (std::fabs(startGroundY - feetY) > 0.10f) return false;

    // Unlike FindStepGroundQ713, this deliberately accepts flat and gently
    // descending support. Those are exactly the joins that the positive-rise
    // step code cannot represent. The probes extend only ~1/3 capsule diameter.
    const float radius = CollisionRadiusQ712();
    const std::array<float, 6> probes{
        0.0f,
        radius * 0.20f,
        radius * 0.40f,
        radius * 0.65f,
        radius * 0.90f,
        radius * 1.15f,
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
        if (rise < -0.10f || rise > maxStep + PLAYER_SKIN) continue;
        if (HasOverheadBlock(probeX, probeZ, candidateY)) continue;

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
        Q6G_LOGI("Q7.21 SEAM BRIDGE: from=(%.3f %.3f) to=(%.3f %.3f) probeAhead=%.3f startY=%.3f landingY=%.3f rise=%.3f originalBlockers=%zu raisedBlockers=%zu raisedRaw=%zu persistent=%zu clear=1",
                 startX, startZ, targetX, targetZ, landingAhead,
                 feetY, landingY, landingY - feetY,
                 blockers.size(), raisedBlockers.size(), raisedRaw, raisedPersistent);
    }
    return true;
}

]=])

set(Q721_BEFORE_HELPER "${Q721_CONTROLLER_SOURCE}")
string(REPLACE
    "bool MoveExteriorSubstepQ718(float& x, float& z, float& feetY,"
    "${Q721_SEAM_HELPER}bool MoveExteriorSubstepQ718(float& x, float& z, float& feetY,"
    Q721_CONTROLLER_SOURCE "${Q721_CONTROLLER_SOURCE}")
if(Q721_CONTROLLER_SOURCE STREQUAL Q721_BEFORE_HELPER)
    message(FATAL_ERROR "Q7.21 could not inject seam-bridge helper")
endif()

set(Q721_OLD_MAIN [=[
    size_t raisedStepBlockers = 0u;
    float stepOverFeetY = feetY;
    if (TryLowBlockerStepOverQ718(x, z, targetX, targetZ, feetY, blockers,
                                  stepOverFeetY, raisedStepBlockers)) {
        x = targetX; z = targetZ; feetY = stepOverFeetY; steppedThisFrame = true; return true;
    }
    Vec2 slide{moveX, moveZ};
]=])
set(Q721_NEW_MAIN [=[
    size_t raisedStepBlockers = 0u;
    float stepOverFeetY = feetY;
    if (TryLowBlockerStepOverQ718(x, z, targetX, targetZ, feetY, blockers,
                                  stepOverFeetY, raisedStepBlockers)) {
        x = targetX; z = targetZ; feetY = stepOverFeetY; steppedThisFrame = true; return true;
    }

    // Q7.21: if the raised capsule is clear but the exact endpoint has no
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
]=])
set(Q721_BEFORE_MAIN "${Q721_CONTROLLER_SOURCE}")
string(REPLACE "${Q721_OLD_MAIN}" "${Q721_NEW_MAIN}"
       Q721_CONTROLLER_SOURCE "${Q721_CONTROLLER_SOURCE}")
if(Q721_CONTROLLER_SOURCE STREQUAL Q721_BEFORE_MAIN)
    message(FATAL_ERROR "Q7.21 could not wire main seam bridge")
endif()

set(Q721_OLD_SLIDE [=[
        size_t slideRaisedBlockers = 0u;
        float slideStepOverY = feetY;
        if (TryLowBlockerStepOverQ718(x, z, slideX, slideZ, feetY, slideBlockers,
                                      slideStepOverY, slideRaisedBlockers)) {
            x = slideX; z = slideZ; feetY = slideStepOverY; steppedThisFrame = true; return true;
        }
        blockers.swap(slideBlockers);
]=])
set(Q721_NEW_SLIDE [=[
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
]=])
set(Q721_BEFORE_SLIDE "${Q721_CONTROLLER_SOURCE}")
string(REPLACE "${Q721_OLD_SLIDE}" "${Q721_NEW_SLIDE}"
       Q721_CONTROLLER_SOURCE "${Q721_CONTROLLER_SOURCE}")
if(Q721_CONTROLLER_SOURCE STREQUAL Q721_BEFORE_SLIDE)
    message(FATAL_ERROR "Q7.21 could not wire slide seam bridge")
endif()

string(REPLACE
    "mode=3d-capsule-triangle+sweep-slide+raised-step persistentOverlap=depth-aware"
    "mode=3d-capsule-triangle+sweep-slide+raised-step+seam-bridge persistentOverlap=depth-aware"
    Q721_CONTROLLER_SOURCE "${Q721_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q721.inc"
     "${Q721_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q720.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q721.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
