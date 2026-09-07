# Q7.22: treat thin modular endcaps as part of a continuous walkable manifold.
#
# Q7.21 proved the raised capsule can clear many snag contacts, but its seam
# helper required walkable support at the current capsule centre and only
# searched a little over one radius forward from the blocked centre. At first
# contact the centre is itself about one capsule radius before the physical
# endcap, so that search often never reached the adjoining ramp/plank. Worse,
# after one accepted seam frame the centre can temporarily have no top-face
# directly under it, causing the next frame's start-support test to reject.
#
# Q7.22 proves walkable support behind the player, searches beyond a full
# capsule radius for support ahead, and validates the complete raised capsule
# path to that support. We still move only by the user's requested substep; the
# forward probe is classification evidence, not a teleport.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q721.inc"
     Q722_CONTROLLER_SOURCE)
string(REPLACE "Q7.21" "Q7.22"
       Q722_CONTROLLER_SOURCE "${Q722_CONTROLLER_SOURCE}")

# Keep a compact surface->placement/model lookup so any remaining snag can be
# tied directly back to GECK/NIF instead of an opaque 64-bit surface key.
set(Q722_OLD_WORLD_TRIANGLES [=[
std::vector<CollisionTriangle> gWorldTriangles;
]=])
set(Q722_NEW_WORLD_TRIANGLES [=[
struct CollisionSurfaceSourceQ722 {
    uint32_t refFormId = 0u;
    std::string editorId;
    std::string modelPath;
};
std::vector<CollisionTriangle> gWorldTriangles;
std::unordered_map<uint64_t, CollisionSurfaceSourceQ722> gSurfaceSourcesQ722;
]=])
string(REPLACE "${Q722_OLD_WORLD_TRIANGLES}" "${Q722_NEW_WORLD_TRIANGLES}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

set(Q722_OLD_SURFACE_ASSIGN [=[
                worldTriangle.surfaceKeyQ714 = MakeSurfaceKeyQ714(
                    placement.refFormId, shape.sourceShapeBlock, subShapeIndexQ714);
]=])
set(Q722_NEW_SURFACE_ASSIGN [=[
                worldTriangle.surfaceKeyQ714 = MakeSurfaceKeyQ714(
                    placement.refFormId, shape.sourceShapeBlock, subShapeIndexQ714);
                if (gSurfaceSourcesQ722.find(worldTriangle.surfaceKeyQ714) == gSurfaceSourcesQ722.end()) {
                    gSurfaceSourcesQ722.emplace(
                        worldTriangle.surfaceKeyQ714,
                        CollisionSurfaceSourceQ722{placement.refFormId,
                                                   placement.editorId,
                                                   placement.modelPath});
                }
]=])
string(REPLACE "${Q722_OLD_SURFACE_ASSIGN}" "${Q722_NEW_SURFACE_ASSIGN}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
string(REPLACE
    "    gWorldTriangles.clear();"
    "    gWorldTriangles.clear();\n    gSurfaceSourcesQ722.clear();"
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# Raised path validation samples the whole classification probe rather than
# checking only the original blocked endpoint. This prevents a thin real wall
# from being skipped just because support happens to exist behind it.
set(Q722_RAISED_PATH_HELPER [=[
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

]=])
string(REPLACE
    "bool TryWalkableSeamBridgeQ721(float startX, float startZ,"
    "${Q722_RAISED_PATH_HELPER}bool TryWalkableSeamBridgeQ721(float startX, float startZ,"
    Q722_CONTROLLER_SOURCE "${Q722_CONTROLLER_SOURCE}")

# Q7.21 demanded exact support under the current centre. During traversal of a
# thin endcap that centre can be inside the ignored side face for several input
# substeps. Prove support slightly behind instead: this keeps the test anchored
# to a surface the player really came from without requiring the seam itself to
# have a walkable top face.
set(Q722_OLD_START_SUPPORT [=[
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
]=])
set(Q722_NEW_START_SUPPORT [=[
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
]=])
string(REPLACE "${Q722_OLD_START_SUPPORT}" "${Q722_NEW_START_SUPPORT}"
       Q722_CONTROLLER_SOURCE "${Q722_CONTROLLER_SOURCE}")

# Require both support ahead and a clear raised capsule sweep all the way to
# that support. This is the key distinction between an internal floor/ramp seam
# and a genuine wall.
set(Q722_OLD_LANDING_ACCEPT [=[
        if (rise < -0.10f || rise > maxStep + PLAYER_SKIN) continue;
        if (HasOverheadBlock(probeX, probeZ, candidateY)) continue;

        landingY = candidateY;
        landingAhead = ahead;
        foundLanding = true;
        break;
]=])
set(Q722_NEW_LANDING_ACCEPT [=[
        if (rise < -0.12f || rise > maxStep + PLAYER_SKIN) continue;
        if (HasOverheadBlock(probeX, probeZ, candidateY)) continue;
        if (!RaisedPathClearQ722(startX, startZ, probeX, probeZ, raisedFeetY)) continue;

        landingY = candidateY;
        landingAhead = ahead;
        foundLanding = true;
        break;
]=])
string(REPLACE "${Q722_OLD_LANDING_ACCEPT}" "${Q722_NEW_LANDING_ACCEPT}"
       Q722_CONTROLLER_SOURCE "${Q722_CONTROLLER_SOURCE}")

# Make the successful classification explicit in logs.
string(REPLACE
    "probeAhead=%.3f startY=%.3f landingY=%.3f rise=%.3f originalBlockers=%zu raisedBlockers=%zu raisedRaw=%zu persistent=%zu clear=1"
    "probeAhead=%.3f startY=%.3f landingY=%.3f rise=%.3f originalBlockers=%zu raisedBlockers=%zu raisedRaw=%zu persistent=%zu behindY=%.3f pathClear=1 clear=1"
    Q722_CONTROLLER_SOURCE "${Q722_CONTROLLER_SOURCE}")
string(REPLACE
    "blockers.size(), raisedBlockers.size(), raisedRaw, raisedPersistent);"
    "blockers.size(), raisedBlockers.size(), raisedRaw, raisedPersistent, supportBehindY);"
    Q722_CONTROLLER_SOURCE "${Q722_CONTROLLER_SOURCE}")

# Surface provenance for the remaining hard blocks.
set(Q722_SOURCE_LOG [=[
        const auto q722SourceIt = gSurfaceSourcesQ722.find(firstSurface);
        if (q722SourceIt != gSurfaceSourcesQ722.end()) {
            Q6G_LOGI("Q7.22 BLOCKER SOURCE: surface=%llu ref=%08X EDID=%s model=%s",
                     static_cast<unsigned long long>(firstSurface),
                     q722SourceIt->second.refFormId,
                     q722SourceIt->second.editorId.empty() ? "<none>" : q722SourceIt->second.editorId.c_str(),
                     q722SourceIt->second.modelPath.c_str());
        }
]=])
string(REPLACE
    "        Q6G_LOGI(\"Q7.22 BLOCKED:"
    "${Q722_SOURCE_LOG}        Q6G_LOGI(\"Q7.22 BLOCKED:"
    Q722_CONTROLLER_SOURCE "${Q722_CONTROLLER_SOURCE}")

string(REPLACE
    "mode=3d-capsule-triangle+sweep-slide+raised-step+seam-bridge persistentOverlap=depth-aware"
    "mode=3d-capsule-triangle+sweep-slide+raised-step+walkable-seam-manifold persistentOverlap=depth-aware"
    Q722_CONTROLLER_SOURCE "${Q722_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q722.inc"
     "${Q722_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q721.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q722.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
