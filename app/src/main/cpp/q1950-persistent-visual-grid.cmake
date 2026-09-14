# Q16.23: move exterior streaming toward Fallout 3's vanilla residency model.
#
# Q16.22 device logs proved starvation was fixed, but every 3x3 recenter still
# staged hundreds of visual shapes, rebuilt the complete collision overlay, and
# rebuilt all 25 LAND cells. Q16.23 deliberately separates those responsibilities:
#   - full-detail static visuals use a 5x5 authored CELL window (radius 2),
#   - the existing REFR-retention streamer keeps overlapping GPU shapes live,
#     so a one-cell shift retains the 20 overlapping visual cells and stages only
#     the entering five-cell strip,
#   - collision remains local to a 3x3 radius around the target CELL,
#   - the existing 5x5 LAND window is retained across adjacent visual shifts and
#     only recentred after the visual target moves two cells from its LAND centre,
#   - edge prefetch begins slightly earlier to pay for the wider entering strip.
#
# This is intentionally NOT the native distant-LOD milestone. It establishes the
# persistent full-detail grid first; Bethesda terrain/object LOD can then sit
# outside it without hiding a broken near-world streamer.

# -----------------------------------------------------------------------------
# A. Final worldspace selector: force the generated runtime source to radius 2.
# Earlier layers intentionally experimented with 3x3 even though the checked-in
# q75 source has since returned to radius 2. Patching the final generated file
# makes the live runtime unambiguous and prevents an old transform winning later.
# -----------------------------------------------------------------------------
set(Q1950_WORLDSPACE_FILE "${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q720.cpp")
if(NOT EXISTS "${Q1950_WORLDSPACE_FILE}")
    message(FATAL_ERROR "Q16.23 expected generated worldspace source")
endif()
file(READ "${Q1950_WORLDSPACE_FILE}" Q1950_WORLDSPACE_SOURCE)

string(REGEX MATCH "constexpr int GRID_RADIUS_Q75 = [0-9]+;"
       Q1950_WORLD_RADIUS_MATCH "${Q1950_WORLDSPACE_SOURCE}")
if(Q1950_WORLD_RADIUS_MATCH STREQUAL "")
    message(FATAL_ERROR "Q16.23 could not find final worldspace GRID_RADIUS_Q75")
endif()
string(REGEX REPLACE
       "constexpr int GRID_RADIUS_Q75 = [0-9]+;"
       "constexpr int GRID_RADIUS_Q75 = 2; // Q16.23 vanilla-style 5x5 full-detail visuals"
       Q1950_WORLDSPACE_SOURCE "${Q1950_WORLDSPACE_SOURCE}")
string(REPLACE "Q7.5 WORLDSPACE CELLS:"
       "Q16.23 VISUAL WINDOW:"
       Q1950_WORLDSPACE_SOURCE "${Q1950_WORLDSPACE_SOURCE}")
file(WRITE "${Q1950_WORLDSPACE_FILE}" "${Q1950_WORLDSPACE_SOURCE}")

# -----------------------------------------------------------------------------
# B. Mature renderer: 5x5 visuals are retained by the existing Q16.18 REFR-ID
# overlap machinery, but collision must NOT inherit that wider radius. Filter the
# staged collision set back to +/-1 authored CELL around the target centre.
# -----------------------------------------------------------------------------
set(Q1950_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1950_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.23 expected Q16.22 generated renderer")
endif()
file(READ "${Q1950_NATIVE_FILE}" Q1950_NATIVE_SOURCE)

set(Q1950_STREAM_COLLISION_LOOP_OLD [==[
    for (const Fo3WorldPlacement& placement : gPendingStreamQ1900.targetPlacements) {
        if (placement.refFormId == 0u ||
            gPendingStreamQ1900.visibleRefs.find(placement.refFormId) ==
                gPendingStreamQ1900.visibleRefs.end() ||
            !seenRefs.emplace(placement.refFormId, 1u).second ||
            Q74ShouldSkipPlacement(placement)) {
            continue;
        }
        collisionPlacements.push_back(placement);
    }

    PumpFo3AndroidEventsQ1860();
]==])
set(Q1950_STREAM_COLLISION_LOOP_NEW [==[
    size_t q1950OutsideCollisionWindow = 0u;
    constexpr int Q1950_COLLISION_GRID_RADIUS = 1;
    for (const Fo3WorldPlacement& placement : gPendingStreamQ1900.targetPlacements) {
        const int32_t placementGridX = static_cast<int32_t>(
            std::floor(placement.x / Q1890_EXTERIOR_CELL_SIZE));
        const int32_t placementGridY = static_cast<int32_t>(
            std::floor(placement.y / Q1890_EXTERIOR_CELL_SIZE));
        if (std::abs(placementGridX - gPendingStreamQ1900.targetGridX) > Q1950_COLLISION_GRID_RADIUS ||
            std::abs(placementGridY - gPendingStreamQ1900.targetGridY) > Q1950_COLLISION_GRID_RADIUS) {
            ++q1950OutsideCollisionWindow;
            continue;
        }
        if (placement.refFormId == 0u ||
            gPendingStreamQ1900.visibleRefs.find(placement.refFormId) ==
                gPendingStreamQ1900.visibleRefs.end() ||
            !seenRefs.emplace(placement.refFormId, 1u).second ||
            Q74ShouldSkipPlacement(placement)) {
            continue;
        }
        collisionPlacements.push_back(placement);
    }

    Q6H_LOGI("Q16.23 COLLISION WINDOW: generation=%llu targetGrid=(%d,%d) visualPlacements=%zu localRenderableRefs=%zu outside3x3=%zu radius=1",
             static_cast<unsigned long long>(gPendingStreamQ1900.generation),
             gPendingStreamQ1900.targetGridX, gPendingStreamQ1900.targetGridY,
             gPendingStreamQ1900.targetPlacements.size(), collisionPlacements.size(),
             q1950OutsideCollisionWindow);
    PumpFo3AndroidEventsQ1860();
]==])
string(FIND "${Q1950_NATIVE_SOURCE}" "${Q1950_STREAM_COLLISION_LOOP_OLD}"
       Q1950_STREAM_COLLISION_LOOP_POS)
if(Q1950_STREAM_COLLISION_LOOP_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find staged collision placement loop")
endif()
string(REPLACE "${Q1950_STREAM_COLLISION_LOOP_OLD}" "${Q1950_STREAM_COLLISION_LOOP_NEW}"
       Q1950_NATIVE_SOURCE "${Q1950_NATIVE_SOURCE}")

# The first Wasteland scene swap is now allowed to build 5x5 visuals too. Keep
# its synchronous collision initialization local as well, and deduplicate REFRs
# before the overlay sees them. Megaton's small child worldspace remains whole.
set(Q1950_SCENE_COLLISION_OLD [==[
    std::vector<Fo3WorldPlacement> collisionPlacements;
    collisionPlacements.reserve(selected.size());
    for (const CpuObject& cpu : selected) collisionPlacements.push_back(cpu.placement);

    const bool collisionReady = InitializeFo3CollisionOverlay(collisionPlacements,
]==])
set(Q1950_SCENE_COLLISION_NEW [==[
    std::vector<Fo3WorldPlacement> collisionPlacements;
    collisionPlacements.reserve(selected.size());
    std::unordered_map<uint32_t, uint8_t> q1950SceneCollisionRefs;
    size_t q1950SceneOutsideCollisionWindow = 0u;
    const bool q1950LocalExteriorCollision = request.worldspaceFormId == 0x0000003Cu;
    const int32_t q1950SceneGridX = static_cast<int32_t>(
        std::floor(request.x / Q1890_EXTERIOR_CELL_SIZE));
    const int32_t q1950SceneGridY = static_cast<int32_t>(
        std::floor(request.y / Q1890_EXTERIOR_CELL_SIZE));
    for (const CpuObject& cpu : selected) {
        const Fo3WorldPlacement& placement = cpu.placement;
        if (q1950LocalExteriorCollision) {
            const int32_t placementGridX = static_cast<int32_t>(
                std::floor(placement.x / Q1890_EXTERIOR_CELL_SIZE));
            const int32_t placementGridY = static_cast<int32_t>(
                std::floor(placement.y / Q1890_EXTERIOR_CELL_SIZE));
            if (std::abs(placementGridX - q1950SceneGridX) > 1 ||
                std::abs(placementGridY - q1950SceneGridY) > 1) {
                ++q1950SceneOutsideCollisionWindow;
                continue;
            }
        }
        if (placement.refFormId != 0u &&
            !q1950SceneCollisionRefs.emplace(placement.refFormId, 1u).second) continue;
        collisionPlacements.push_back(placement);
    }
    if (q1950LocalExteriorCollision) SetNextFo3CollisionExteriorModeQ1931(true);
    Q6H_LOGI("Q16.23 SCENE COLLISION WINDOW: worldspace=%08X centreGrid=(%d,%d) visualShapes=%zu localRefs=%zu outside3x3=%zu localMode=%d",
             request.worldspaceFormId, q1950SceneGridX, q1950SceneGridY,
             selected.size(), collisionPlacements.size(), q1950SceneOutsideCollisionWindow,
             q1950LocalExteriorCollision ? 1 : 0);

    const bool collisionReady = InitializeFo3CollisionOverlay(collisionPlacements,
]==])
string(FIND "${Q1950_NATIVE_SOURCE}" "${Q1950_SCENE_COLLISION_OLD}"
       Q1950_SCENE_COLLISION_POS)
if(Q1950_SCENE_COLLISION_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find scene-swap collision block")
endif()
string(REPLACE "${Q1950_SCENE_COLLISION_OLD}" "${Q1950_SCENE_COLLISION_NEW}"
       Q1950_NATIVE_SOURCE "${Q1950_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# C. LAND window retention. Q16.22 rebuilt all 25 terrain cells after every
# object-window shift even when 20/25 were the same. Until terrain VBOs are split
# into independently owned cell chunks, hold the already-live 5x5 LAND window
# across a one-cell visual recenter. Rebuild only when the target is two cells
# from the retained LAND centre. This removes the ~2s terrain GPU rebuild from
# most adjacent CELL transitions while preserving a full-cell safety margin.
# -----------------------------------------------------------------------------
set(Q1950_TERRAIN_ENTRY_OLD [==[
void Q1900AdvanceTerrain() {
    PumpFo3AndroidEventsQ1860();
]==])
set(Q1950_TERRAIN_ENTRY_NEW [==[
void Q1900AdvanceTerrain() {
    static bool q1950TerrainWindowValid = false;
    static uint32_t q1950TerrainWorldspace = 0u;
    static uint32_t q1950TerrainPersistent = 0u;
    static int32_t q1950TerrainGridX = 0;
    static int32_t q1950TerrainGridY = 0;

    const bool q1950TerrainContextChanged =
        !q1950TerrainWindowValid ||
        q1950TerrainWorldspace != gExteriorWorldspaceQ1890 ||
        q1950TerrainPersistent != gExteriorPersistentCellQ1890;
    if (q1950TerrainContextChanged) {
        // The scene transition has already initialized the radius-2 LAND window
        // around the currently committed exterior grid.
        q1950TerrainWindowValid = true;
        q1950TerrainWorldspace = gExteriorWorldspaceQ1890;
        q1950TerrainPersistent = gExteriorPersistentCellQ1890;
        q1950TerrainGridX = gExteriorWindowGridXQ1890;
        q1950TerrainGridY = gExteriorWindowGridYQ1890;
        Q6H_LOGI("Q16.23 TERRAIN RESIDENCY RESET: worldspace=%08X persistent=%08X centre=(%d,%d) radius=2",
                 q1950TerrainWorldspace, q1950TerrainPersistent,
                 q1950TerrainGridX, q1950TerrainGridY);
    }

    const int q1950TerrainDx = std::abs(
        gPendingStreamQ1900.targetGridX - q1950TerrainGridX);
    const int q1950TerrainDy = std::abs(
        gPendingStreamQ1900.targetGridY - q1950TerrainGridY);
    if (q1950TerrainDx <= 1 && q1950TerrainDy <= 1) {
        gPendingStreamQ1900.terrainReady = true;
        gPendingStreamQ1900.phase = Q1900StreamPhase::Commit;
        Q6H_LOGI("Q16.23 TERRAIN RETAIN: generation=%llu retainedCentre=(%d,%d) target=(%d,%d) delta=(%d,%d) radius=2 fullGpuRebuild=0",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 q1950TerrainGridX, q1950TerrainGridY,
                 gPendingStreamQ1900.targetGridX, gPendingStreamQ1900.targetGridY,
                 q1950TerrainDx, q1950TerrainDy);
        return;
    }

    PumpFo3AndroidEventsQ1860();
]==])
string(FIND "${Q1950_NATIVE_SOURCE}" "${Q1950_TERRAIN_ENTRY_OLD}"
       Q1950_TERRAIN_ENTRY_POS)
if(Q1950_TERRAIN_ENTRY_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find staged terrain entry")
endif()
string(REPLACE "${Q1950_TERRAIN_ENTRY_OLD}" "${Q1950_TERRAIN_ENTRY_NEW}"
       Q1950_NATIVE_SOURCE "${Q1950_NATIVE_SOURCE}")

set(Q1950_TERRAIN_RESULT_OLD [==[
    SetFo3TerrainSelectionOverrideQ1890(false, 0.0f, 0.0f);
    if (gPendingStreamQ1900.terrainReady) {
]==])
set(Q1950_TERRAIN_RESULT_NEW [==[
    SetFo3TerrainSelectionOverrideQ1890(false, 0.0f, 0.0f);
    if (gPendingStreamQ1900.terrainReady) {
        q1950TerrainGridX = gPendingStreamQ1900.targetGridX;
        q1950TerrainGridY = gPendingStreamQ1900.targetGridY;
        Q6H_LOGI("Q16.23 TERRAIN RECENTER: generation=%llu centre=(%d,%d) radius=2 fullGpuRebuild=1",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 q1950TerrainGridX, q1950TerrainGridY);
    }
    if (gPendingStreamQ1900.terrainReady) {
]==])
string(FIND "${Q1950_NATIVE_SOURCE}" "${Q1950_TERRAIN_RESULT_OLD}"
       Q1950_TERRAIN_RESULT_POS)
if(Q1950_TERRAIN_RESULT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find staged terrain result")
endif()
string(REPLACE "${Q1950_TERRAIN_RESULT_OLD}" "${Q1950_TERRAIN_RESULT_NEW}"
       Q1950_NATIVE_SOURCE "${Q1950_NATIVE_SOURCE}")

# A 5x5 visual shift introduces a five-cell strip rather than a three-cell strip.
# Start the same bounded 8/3 CPU/GPU pipeline a little earlier instead of raising
# the per-frame GPU upload budget and making VR judder worse.
set(Q1950_PREFETCH_OLD
    "constexpr float Q1920_PREFETCH_DISTANCE = 3072.0f; // Q16.22 early adjacent prefetch")
set(Q1950_PREFETCH_NEW
    "constexpr float Q1920_PREFETCH_DISTANCE = 3584.0f; // Q16.23 5x5 entering-strip lead time")
string(FIND "${Q1950_NATIVE_SOURCE}" "${Q1950_PREFETCH_OLD}" Q1950_PREFETCH_POS)
if(Q1950_PREFETCH_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find Q16.22 prefetch distance")
endif()
string(REPLACE "${Q1950_PREFETCH_OLD}" "${Q1950_PREFETCH_NEW}"
       Q1950_NATIVE_SOURCE "${Q1950_NATIVE_SOURCE}")

string(REPLACE "rebuildMode=early-prefetch+finish-inflight+cached-assets"
       "rebuildMode=5x5-visual+3x3-collision+terrain-window-retain"
       Q1950_NATIVE_SOURCE "${Q1950_NATIVE_SOURCE}")
string(REPLACE "Q16.22" "Q16.23"
       Q1950_NATIVE_SOURCE "${Q1950_NATIVE_SOURCE}")
file(WRITE "${Q1950_NATIVE_FILE}" "${Q1950_NATIVE_SOURCE}")

# Keep collision/TERRAIN translation-unit diagnostics aligned with native logs.
string(REPLACE "Q16.22 COLLISION MODE OVERRIDE" "Q16.23 COLLISION MODE OVERRIDE"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
string(REPLACE "Q16.22 TERRAIN WINDOW" "Q16.23 TERRAIN WINDOW"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")

# -----------------------------------------------------------------------------
# D. Visible build label Q16.23. Far clip remains 300m until native FO3 distant
# LOD exists; increasing the camera alone would only reveal more empty world.
# -----------------------------------------------------------------------------
set(Q1950_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1950_Q4_FILE}")
    message(FATAL_ERROR "Q16.23 expected Q16.22 OpenXR source")
endif()
file(READ "${Q1950_Q4_FILE}" Q1950_Q4_SOURCE)
set(Q1950_LABEL_OLD [==[
        q1600Digit(q1600X, 0x5Bu); // Q16.22: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x5Bu); // Q16.22: 2 = A B G E D
]==])
set(Q1950_LABEL_NEW [==[
        q1600Digit(q1600X, 0x5Bu); // Q16.23: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x4Fu); // Q16.23: 3 = A B C D G
]==])
string(FIND "${Q1950_Q4_SOURCE}" "${Q1950_LABEL_OLD}" Q1950_LABEL_POS)
if(Q1950_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find Q16.22 build-label digits")
endif()
string(REPLACE "${Q1950_LABEL_OLD}" "${Q1950_LABEL_NEW}"
       Q1950_Q4_SOURCE "${Q1950_Q4_SOURCE}")
string(REPLACE "Q16.22 BUILD LABEL:" "Q16.23 BUILD LABEL:"
       Q1950_Q4_SOURCE "${Q1950_Q4_SOURCE}")
string(REPLACE "text=Q16.22 anchor=left-hand" "text=Q16.23 anchor=left-hand"
       Q1950_Q4_SOURCE "${Q1950_Q4_SOURCE}")
string(REPLACE "Q16.22 AUTHORED DOOR FACING" "Q16.23 AUTHORED DOOR FACING"
       Q1950_Q4_SOURCE "${Q1950_Q4_SOURCE}")
string(REPLACE "Q16.22 FAR CLIP:" "Q16.23 FAR CLIP:"
       Q1950_Q4_SOURCE "${Q1950_Q4_SOURCE}")
file(WRITE "${Q1950_Q4_FILE}" "${Q1950_Q4_SOURCE}")

# Configure-time proof. Fail instead of silently shipping a mixed 3x3/5x5 build.
string(FIND "${Q1950_WORLDSPACE_SOURCE}"
       "GRID_RADIUS_Q75 = 2; // Q16.23 vanilla-style 5x5 full-detail visuals"
       Q1950_RADIUS_OK)
string(FIND "${Q1950_NATIVE_SOURCE}" "Q16.23 COLLISION WINDOW:" Q1950_COLLISION_OK)
string(FIND "${Q1950_NATIVE_SOURCE}" "Q16.23 SCENE COLLISION WINDOW:" Q1950_SCENE_COLLISION_OK)
string(FIND "${Q1950_NATIVE_SOURCE}" "Q16.23 TERRAIN RETAIN:" Q1950_TERRAIN_RETAIN_OK)
string(FIND "${Q1950_NATIVE_SOURCE}" "Q16.23 TERRAIN RECENTER:" Q1950_TERRAIN_RECENTER_OK)
string(FIND "${Q1950_NATIVE_SOURCE}" "Q1920_PREFETCH_DISTANCE = 3584.0f" Q1950_PREFETCH_OK)
string(FIND "${Q1950_NATIVE_SOURCE}"
       "rebuildMode=5x5-visual+3x3-collision+terrain-window-retain"
       Q1950_MODE_OK)
string(FIND "${Q1950_Q4_SOURCE}" "Q16.23: 3 = A B C D G" Q1950_LABEL_OK)
if(Q1950_RADIUS_OK EQUAL -1 OR Q1950_COLLISION_OK EQUAL -1 OR
   Q1950_SCENE_COLLISION_OK EQUAL -1 OR Q1950_TERRAIN_RETAIN_OK EQUAL -1 OR
   Q1950_TERRAIN_RECENTER_OK EQUAL -1 OR Q1950_PREFETCH_OK EQUAL -1 OR
   Q1950_MODE_OK EQUAL -1 OR Q1950_LABEL_OK EQUAL -1)
    message(FATAL_ERROR "Q16.23 persistent visual-grid verification failed")
endif()

message(STATUS "Q16.23 enabled: 5x5 retained full-detail visuals + local 3x3 collision + retained 5x5 LAND window + 3584-unit prefetch")
