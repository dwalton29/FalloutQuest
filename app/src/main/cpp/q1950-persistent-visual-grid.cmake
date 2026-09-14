# Q16.23: persistent exterior residency step.
#
# Q16.22 proved adjacent generations can finish, but the live 3x3 visual window
# still exposed cell boundaries and every shift rebuilt the complete 5x5 LAND
# renderer. Q16.23 makes streamed visual metadata 5x5 while keeping collision
# local to 3x3. Initial scene loading remains on the proven base selector so the
# first Wasteland collision build cannot accidentally feed 25 cells into the
# 1024-placement collision cap. Once streaming starts, a thread-local selector
# override expands only the metadata worker to radius 2.
#
# LAND becomes a 7x7 retained ring. A one-cell visual recenter therefore remains
# fully covered by the existing terrain GPU window; terrain is only rebuilt once
# the streamed centre is two cells from the retained LAND centre. This is still
# an intermediate step before true per-cell terrain GPU ownership/native LOD.

# -----------------------------------------------------------------------------
# A. Add a thread-local object-selection radius override to the final generated
# worldspace loader. Default/scene loads keep the proven radius; stream workers
# opt into radius 2 (5x5) without changing other callers.
# -----------------------------------------------------------------------------
set(Q1950_WORLDSPACE_FILE "${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q720.cpp")
if(NOT EXISTS "${Q1950_WORLDSPACE_FILE}")
    message(FATAL_ERROR "Q16.23 expected generated worldspace source")
endif()
file(READ "${Q1950_WORLDSPACE_FILE}" Q1950_WORLDSPACE_SOURCE)

set(Q1950_WORLDSPACE_PUBLIC_OLD [==[
} // namespace

bool LoadFo3WorldspaceNeighborhoodQ75(uint32_t worldspaceFormId,
]==])
set(Q1950_WORLDSPACE_PUBLIC_NEW [==[
} // namespace

thread_local int gQ1950WorldspaceGridRadiusOverride = -1;

void SetFo3WorldspaceGridRadiusOverrideQ1950(int radius) {
    gQ1950WorldspaceGridRadiusOverride = radius;
}

bool LoadFo3WorldspaceNeighborhoodQ75(uint32_t worldspaceFormId,
]==])
string(FIND "${Q1950_WORLDSPACE_SOURCE}" "${Q1950_WORLDSPACE_PUBLIC_OLD}" Q1950_WORLDSPACE_PUBLIC_POS)
if(Q1950_WORLDSPACE_PUBLIC_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find worldspace public loader marker")
endif()
string(REPLACE "${Q1950_WORLDSPACE_PUBLIC_OLD}" "${Q1950_WORLDSPACE_PUBLIC_NEW}"
       Q1950_WORLDSPACE_SOURCE "${Q1950_WORLDSPACE_SOURCE}")

set(Q1950_WORLDSPACE_RADIUS_DECL_OLD [==[
    const bool loadWholeWorldspace = gridCells.size() <= SMALL_WORLDSPACE_CELL_LIMIT_Q75;

    std::unordered_set<uint32_t> selectedCells;
]==])
set(Q1950_WORLDSPACE_RADIUS_DECL_NEW [==[
    const bool loadWholeWorldspace = gridCells.size() <= SMALL_WORLDSPACE_CELL_LIMIT_Q75;
    const int q1950SelectionRadius = gQ1950WorldspaceGridRadiusOverride >= 0
        ? gQ1950WorldspaceGridRadiusOverride : GRID_RADIUS_Q75;

    std::unordered_set<uint32_t> selectedCells;
]==])
string(FIND "${Q1950_WORLDSPACE_SOURCE}" "${Q1950_WORLDSPACE_RADIUS_DECL_OLD}" Q1950_WORLDSPACE_RADIUS_DECL_POS)
if(Q1950_WORLDSPACE_RADIUS_DECL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find worldspace selection declaration")
endif()
string(REPLACE "${Q1950_WORLDSPACE_RADIUS_DECL_OLD}" "${Q1950_WORLDSPACE_RADIUS_DECL_NEW}"
       Q1950_WORLDSPACE_SOURCE "${Q1950_WORLDSPACE_SOURCE}")

set(Q1950_WORLDSPACE_RADIUS_TEST_OLD [==[
            (std::abs(cell->gridX - targetGridX) <= GRID_RADIUS_Q75 &&
             std::abs(cell->gridY - targetGridY) <= GRID_RADIUS_Q75)) {
]==])
set(Q1950_WORLDSPACE_RADIUS_TEST_NEW [==[
            (std::abs(cell->gridX - targetGridX) <= q1950SelectionRadius &&
             std::abs(cell->gridY - targetGridY) <= q1950SelectionRadius)) {
]==])
string(FIND "${Q1950_WORLDSPACE_SOURCE}" "${Q1950_WORLDSPACE_RADIUS_TEST_OLD}" Q1950_WORLDSPACE_RADIUS_TEST_POS)
if(Q1950_WORLDSPACE_RADIUS_TEST_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find worldspace radius test")
endif()
string(REPLACE "${Q1950_WORLDSPACE_RADIUS_TEST_OLD}" "${Q1950_WORLDSPACE_RADIUS_TEST_NEW}"
       Q1950_WORLDSPACE_SOURCE "${Q1950_WORLDSPACE_SOURCE}")

string(REPLACE "Q7.5 WORLDSPACE CELLS:" "Q16.23 VISUAL WINDOW:"
       Q1950_WORLDSPACE_SOURCE "${Q1950_WORLDSPACE_SOURCE}")
set(Q1950_WORLDSPACE_LOG_RADIUS_OLD [==[
             GRID_RADIUS_Q75);
]==])
set(Q1950_WORLDSPACE_LOG_RADIUS_NEW [==[
             q1950SelectionRadius);
]==])
string(FIND "${Q1950_WORLDSPACE_SOURCE}" "${Q1950_WORLDSPACE_LOG_RADIUS_OLD}" Q1950_WORLDSPACE_LOG_RADIUS_POS)
if(Q1950_WORLDSPACE_LOG_RADIUS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find worldspace radius diagnostic")
endif()
string(REPLACE "${Q1950_WORLDSPACE_LOG_RADIUS_OLD}" "${Q1950_WORLDSPACE_LOG_RADIUS_NEW}"
       Q1950_WORLDSPACE_SOURCE "${Q1950_WORLDSPACE_SOURCE}")
file(WRITE "${Q1950_WORLDSPACE_FILE}" "${Q1950_WORLDSPACE_SOURCE}")

# -----------------------------------------------------------------------------
# B. Stream worker: request 5x5 visual metadata. The override is thread-local so
# it cannot leak into the render thread or scene-transition loader.
# -----------------------------------------------------------------------------
set(Q1950_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1950_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.23 expected Q16.22 generated renderer")
endif()
file(READ "${Q1950_NATIVE_FILE}" Q1950_NATIVE_SOURCE)
string(PREPEND Q1950_NATIVE_SOURCE
       "void SetFo3WorldspaceGridRadiusOverrideQ1950(int radius);\n")

set(Q1950_METADATA_WORKER_OLD [==[
    std::thread([task]() {
        task->success = LoadFo3WorldspaceNeighborhoodQ75(
            task->worldspace, task->persistentCell,
            task->selectionGameX, task->selectionGameY,
            task->placements);
        task->ready.store(true, std::memory_order_release);
    }).detach();
]==])
set(Q1950_METADATA_WORKER_NEW [==[
    std::thread([task]() {
        SetFo3WorldspaceGridRadiusOverrideQ1950(2);
        task->success = LoadFo3WorldspaceNeighborhoodQ75(
            task->worldspace, task->persistentCell,
            task->selectionGameX, task->selectionGameY,
            task->placements);
        SetFo3WorldspaceGridRadiusOverrideQ1950(-1);
        task->ready.store(true, std::memory_order_release);
    }).detach();
]==])
string(FIND "${Q1950_NATIVE_SOURCE}" "${Q1950_METADATA_WORKER_OLD}" Q1950_METADATA_WORKER_POS)
if(Q1950_METADATA_WORKER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find metadata worker")
endif()
string(REPLACE "${Q1950_METADATA_WORKER_OLD}" "${Q1950_METADATA_WORKER_NEW}"
       Q1950_NATIVE_SOURCE "${Q1950_NATIVE_SOURCE}")

# Keep streamed collision spatially local even though targetPlacements is now
# 5x5. This avoids multiplying the already-heavy BHK rebuild and protects the
# 1024-placement cap.
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
string(FIND "${Q1950_NATIVE_SOURCE}" "${Q1950_STREAM_COLLISION_LOOP_OLD}" Q1950_STREAM_COLLISION_LOOP_POS)
if(Q1950_STREAM_COLLISION_LOOP_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find staged collision placement loop")
endif()
string(REPLACE "${Q1950_STREAM_COLLISION_LOOP_OLD}" "${Q1950_STREAM_COLLISION_LOOP_NEW}"
       Q1950_NATIVE_SOURCE "${Q1950_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# C. Make LAND radius 3 (7x7), then retain that GPU window across one-cell visual
# shifts. A radius-2 visual window shifted by one cell still fits entirely inside
# the retained radius-3 terrain ring.
# -----------------------------------------------------------------------------
set(Q1950_TERRAIN_RADIUS_OLD "constexpr int Q1931_TERRAIN_GRID_RADIUS = 2;")
set(Q1950_TERRAIN_RADIUS_NEW "constexpr int Q1931_TERRAIN_GRID_RADIUS = 3; // Q16.23 retained 7x7 LAND ring")
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1950_TERRAIN_RADIUS_OLD}" Q1950_TERRAIN_RADIUS_POS)
if(Q1950_TERRAIN_RADIUS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find Q16.21 terrain radius")
endif()
string(REPLACE "${Q1950_TERRAIN_RADIUS_OLD}" "${Q1950_TERRAIN_RADIUS_NEW}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
string(REPLACE "Q16.22 TERRAIN WINDOW" "Q16.23 TERRAIN WINDOW"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")

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
        q1950TerrainWindowValid = true;
        q1950TerrainWorldspace = gExteriorWorldspaceQ1890;
        q1950TerrainPersistent = gExteriorPersistentCellQ1890;
        q1950TerrainGridX = gExteriorWindowGridXQ1890;
        q1950TerrainGridY = gExteriorWindowGridYQ1890;
        Q6H_LOGI("Q16.23 TERRAIN RESIDENCY RESET: worldspace=%08X persistent=%08X centre=(%d,%d) radius=3",
                 q1950TerrainWorldspace, q1950TerrainPersistent,
                 q1950TerrainGridX, q1950TerrainGridY);
    }

    const int q1950TerrainDx = std::abs(gPendingStreamQ1900.targetGridX - q1950TerrainGridX);
    const int q1950TerrainDy = std::abs(gPendingStreamQ1900.targetGridY - q1950TerrainGridY);
    if (q1950TerrainDx <= 1 && q1950TerrainDy <= 1) {
        gPendingStreamQ1900.terrainReady = true;
        gPendingStreamQ1900.phase = Q1900StreamPhase::Commit;
        Q6H_LOGI("Q16.23 TERRAIN RETAIN: generation=%llu retainedCentre=(%d,%d) target=(%d,%d) delta=(%d,%d) radius=3 fullGpuRebuild=0",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 q1950TerrainGridX, q1950TerrainGridY,
                 gPendingStreamQ1900.targetGridX, gPendingStreamQ1900.targetGridY,
                 q1950TerrainDx, q1950TerrainDy);
        return;
    }

    PumpFo3AndroidEventsQ1860();
]==])
string(FIND "${Q1950_NATIVE_SOURCE}" "${Q1950_TERRAIN_ENTRY_OLD}" Q1950_TERRAIN_ENTRY_POS)
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
        Q6H_LOGI("Q16.23 TERRAIN RECENTER: generation=%llu centre=(%d,%d) radius=3 fullGpuRebuild=1",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 q1950TerrainGridX, q1950TerrainGridY);
    }
    if (gPendingStreamQ1900.terrainReady) {
]==])
string(FIND "${Q1950_NATIVE_SOURCE}" "${Q1950_TERRAIN_RESULT_OLD}" Q1950_TERRAIN_RESULT_POS)
if(Q1950_TERRAIN_RESULT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find staged terrain result")
endif()
string(REPLACE "${Q1950_TERRAIN_RESULT_OLD}" "${Q1950_TERRAIN_RESULT_NEW}"
       Q1950_NATIVE_SOURCE "${Q1950_NATIVE_SOURCE}")

# Five-cell entering strips need more lead time, not a larger per-frame GL burst.
set(Q1950_PREFETCH_OLD "constexpr float Q1920_PREFETCH_DISTANCE = 3072.0f; // Q16.22 early adjacent prefetch")
set(Q1950_PREFETCH_NEW "constexpr float Q1920_PREFETCH_DISTANCE = 3584.0f; // Q16.23 5x5 visual lead time")
string(FIND "${Q1950_NATIVE_SOURCE}" "${Q1950_PREFETCH_OLD}" Q1950_PREFETCH_POS)
if(Q1950_PREFETCH_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 could not find Q16.22 prefetch distance")
endif()
string(REPLACE "${Q1950_PREFETCH_OLD}" "${Q1950_PREFETCH_NEW}"
       Q1950_NATIVE_SOURCE "${Q1950_NATIVE_SOURCE}")

string(REPLACE "rebuildMode=early-prefetch+finish-inflight+cached-assets"
       "rebuildMode=5x5-visual+3x3-collision+7x7-terrain-retain"
       Q1950_NATIVE_SOURCE "${Q1950_NATIVE_SOURCE}")
string(REPLACE "Q16.22" "Q16.23" Q1950_NATIVE_SOURCE "${Q1950_NATIVE_SOURCE}")
file(WRITE "${Q1950_NATIVE_FILE}" "${Q1950_NATIVE_SOURCE}")

# Keep collision diagnostics aligned.
string(REPLACE "Q16.22 COLLISION MODE OVERRIDE" "Q16.23 COLLISION MODE OVERRIDE"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# -----------------------------------------------------------------------------
# D. Visible Q16.23 label. Far clip intentionally remains 300m until native FO3
# distant terrain/object LOD is wired in.
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
string(REPLACE "Q16.22 BUILD LABEL:" "Q16.23 BUILD LABEL:" Q1950_Q4_SOURCE "${Q1950_Q4_SOURCE}")
string(REPLACE "text=Q16.22 anchor=left-hand" "text=Q16.23 anchor=left-hand" Q1950_Q4_SOURCE "${Q1950_Q4_SOURCE}")
string(REPLACE "Q16.22 AUTHORED DOOR FACING" "Q16.23 AUTHORED DOOR FACING" Q1950_Q4_SOURCE "${Q1950_Q4_SOURCE}")
string(REPLACE "Q16.22 FAR CLIP:" "Q16.23 FAR CLIP:" Q1950_Q4_SOURCE "${Q1950_Q4_SOURCE}")
file(WRITE "${Q1950_Q4_FILE}" "${Q1950_Q4_SOURCE}")

# Configure-time proof. Fail closed rather than silently ship a mixed-radius APK.
string(FIND "${Q1950_WORLDSPACE_SOURCE}" "SetFo3WorldspaceGridRadiusOverrideQ1950" Q1950_WORLD_OVERRIDE_OK)
string(FIND "${Q1950_WORLDSPACE_SOURCE}" "q1950SelectionRadius" Q1950_WORLD_RADIUS_OK)
string(FIND "${Q1950_NATIVE_SOURCE}" "SetFo3WorldspaceGridRadiusOverrideQ1950(2);" Q1950_WORKER_OK)
string(FIND "${Q1950_NATIVE_SOURCE}" "Q16.23 COLLISION WINDOW:" Q1950_COLLISION_OK)
string(FIND "${Q1950_NATIVE_SOURCE}" "Q16.23 TERRAIN RETAIN:" Q1950_TERRAIN_RETAIN_OK)
string(FIND "${Q1950_NATIVE_SOURCE}" "Q16.23 TERRAIN RECENTER:" Q1950_TERRAIN_RECENTER_OK)
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "Q1931_TERRAIN_GRID_RADIUS = 3" Q1950_TERRAIN_RADIUS_OK)
string(FIND "${Q1950_NATIVE_SOURCE}" "Q1920_PREFETCH_DISTANCE = 3584.0f" Q1950_PREFETCH_OK)
string(FIND "${Q1950_NATIVE_SOURCE}" "rebuildMode=5x5-visual+3x3-collision+7x7-terrain-retain" Q1950_MODE_OK)
string(FIND "${Q1950_Q4_SOURCE}" "Q16.23: 3 = A B C D G" Q1950_LABEL_OK)
if(Q1950_WORLD_OVERRIDE_OK EQUAL -1 OR Q1950_WORLD_RADIUS_OK EQUAL -1 OR
   Q1950_WORKER_OK EQUAL -1 OR Q1950_COLLISION_OK EQUAL -1 OR
   Q1950_TERRAIN_RETAIN_OK EQUAL -1 OR Q1950_TERRAIN_RECENTER_OK EQUAL -1 OR
   Q1950_TERRAIN_RADIUS_OK EQUAL -1 OR Q1950_PREFETCH_OK EQUAL -1 OR
   Q1950_MODE_OK EQUAL -1 OR Q1950_LABEL_OK EQUAL -1)
    message(FATAL_ERROR "Q16.23 persistent visual-grid verification failed")
endif()

message(STATUS "Q16.23 enabled: stream-only 5x5 visuals + local 3x3 collision + retained 7x7 LAND + 3584-unit prefetch")
