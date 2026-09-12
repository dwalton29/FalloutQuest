# Q16.17: first continuous exterior CELL streaming milestone.
#
# Q16.15 proved the 3x3 Capital Wasteland window is practical on Quest once the
# live XTEL fallback is indexed. The remaining limitation is lifetime: Q7.5/Q16.7
# select that 3x3 only once from the load-door XTEL. Q16.17 keeps the exact same
# authored CELL/LAND selection and the exact same render/collision paths, but
# recentres the selection window when the player's absolute Fallout position
# crosses an XCLC boundary.
#
# This first pass intentionally rebuilds the 3x3 window synchronously at a cell
# boundary. It proves world-coordinate continuity before Q16.18 makes ownership
# incremental (incoming row/outgoing row). The permanent render origin remains
# the original exterior XTEL, so a stream update never resets or snaps the VR
# player coordinate frame.

# -----------------------------------------------------------------------------
# 1. LAND: split "which cells are selected" from "where game zero is rendered".
#    All prior Q7.20/Q10.x generated terrain modifications are already present in
#    Q720_TERRAIN_DATA_SOURCE here; patch that final generated source rather than
#    falling back to an older terrain implementation.
# -----------------------------------------------------------------------------
if(NOT DEFINED Q720_TERRAIN_DATA_SOURCE)
    message(FATAL_ERROR "Q16.17 expected final generated terrain-data source")
endif()

set(Q1890_TERRAIN_GLOBAL_OLD
    "std::vector<Fo3TerrainCellQ76> gTerrainDataQ76;")
set(Q1890_TERRAIN_GLOBAL_NEW [==[
std::vector<Fo3TerrainCellQ76> gTerrainDataQ76;
bool gTerrainSelectionOverrideQ1890 = false;
float gTerrainSelectionXQ1890 = 0.0f;
float gTerrainSelectionYQ1890 = 0.0f;
]==])
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1890_TERRAIN_GLOBAL_OLD}"
       Q1890_TERRAIN_GLOBAL_POS)
if(Q1890_TERRAIN_GLOBAL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.17 could not find final terrain-data globals")
endif()
string(REPLACE "${Q1890_TERRAIN_GLOBAL_OLD}" "${Q1890_TERRAIN_GLOBAL_NEW}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

set(Q1890_TERRAIN_PUBLIC_MARKER [==[
} // namespace

bool LoadFo3TerrainQ76(uint32_t worldspaceFormId,
]==])
set(Q1890_TERRAIN_PUBLIC_NEW [==[
} // namespace

void SetFo3TerrainSelectionOverrideQ1890(bool enabled, float gameX, float gameY) {
    gTerrainSelectionOverrideQ1890 = enabled;
    gTerrainSelectionXQ1890 = gameX;
    gTerrainSelectionYQ1890 = gameY;
    Q75_LOGI("Q16.17 TERRAIN SELECTION: enabled=%d game=(%.2f %.2f)",
             enabled ? 1 : 0, gameX, gameY);
}

bool LoadFo3TerrainQ76(uint32_t worldspaceFormId,
]==])
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1890_TERRAIN_PUBLIC_MARKER}"
       Q1890_TERRAIN_PUBLIC_POS)
if(Q1890_TERRAIN_PUBLIC_POS EQUAL -1)
    message(FATAL_ERROR "Q16.17 could not find final terrain public-function marker")
endif()
string(REPLACE "${Q1890_TERRAIN_PUBLIC_MARKER}" "${Q1890_TERRAIN_PUBLIC_NEW}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

set(Q1890_TERRAIN_GRID_OLD [==[
    const int32_t targetGridX = static_cast<int32_t>(std::floor(arrivalX / EXTERIOR_CELL_SIZE_Q75));
    const int32_t targetGridY = static_cast<int32_t>(std::floor(arrivalY / EXTERIOR_CELL_SIZE_Q75));
]==])
set(Q1890_TERRAIN_GRID_NEW [==[
    const float selectionXQ1890 = gTerrainSelectionOverrideQ1890
        ? gTerrainSelectionXQ1890 : arrivalX;
    const float selectionYQ1890 = gTerrainSelectionOverrideQ1890
        ? gTerrainSelectionYQ1890 : arrivalY;
    const int32_t targetGridX = static_cast<int32_t>(
        std::floor(selectionXQ1890 / EXTERIOR_CELL_SIZE_Q75));
    const int32_t targetGridY = static_cast<int32_t>(
        std::floor(selectionYQ1890 / EXTERIOR_CELL_SIZE_Q75));
]==])
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1890_TERRAIN_GRID_OLD}"
       Q1890_TERRAIN_GRID_POS)
if(Q1890_TERRAIN_GRID_POS EQUAL -1)
    message(FATAL_ERROR "Q16.17 could not find final terrain target-grid calculation")
endif()
string(REPLACE "${Q1890_TERRAIN_GRID_OLD}" "${Q1890_TERRAIN_GRID_NEW}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")

# -----------------------------------------------------------------------------
# 2. Mature renderer: maintain exterior stream context and rebuild the proven
#    3x3 object/collision/LAND set against the ORIGINAL exterior XTEL origin.
# -----------------------------------------------------------------------------
set(Q1890_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1890_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.17 expected final mature renderer source")
endif()
file(READ "${Q1890_NATIVE_FILE}" Q1890_NATIVE_SOURCE)
string(PREPEND Q1890_NATIVE_SOURCE
       "#include \"fo3-terrain-q76.h\"\nextern void SetFo3TerrainSelectionOverrideQ1890(bool enabled, float gameX, float gameY);\n")

set(Q1890_STREAM_HELPERS [==[
constexpr float Q1890_EXTERIOR_CELL_SIZE = 4096.0f;
constexpr float Q1890_BOUNDARY_HYSTERESIS = 96.0f;
bool gExteriorStreamingActiveQ1890 = false;
bool gExteriorStreamBusyQ1890 = false;
uint32_t gExteriorWorldspaceQ1890 = 0u;
uint32_t gExteriorPersistentCellQ1890 = 0u;
float gExteriorOriginXQ1890 = 0.0f;
float gExteriorOriginYQ1890 = 0.0f;
float gExteriorOriginZQ1890 = 0.0f;
int32_t gExteriorWindowGridXQ1890 = 0;
int32_t gExteriorWindowGridYQ1890 = 0;
uint64_t gExteriorWindowGenerationQ1890 = 0u;

bool Q1890InsideCandidatePastHysteresis(float gameX, float gameY,
                                        int32_t oldX, int32_t oldY,
                                        int32_t newX, int32_t newY) {
    const float localX = gameX - static_cast<float>(newX) * Q1890_EXTERIOR_CELL_SIZE;
    const float localY = gameY - static_cast<float>(newY) * Q1890_EXTERIOR_CELL_SIZE;
    if (newX > oldX && localX < Q1890_BOUNDARY_HYSTERESIS) return false;
    if (newX < oldX && localX > Q1890_EXTERIOR_CELL_SIZE - Q1890_BOUNDARY_HYSTERESIS) return false;
    if (newY > oldY && localY < Q1890_BOUNDARY_HYSTERESIS) return false;
    if (newY < oldY && localY > Q1890_EXTERIOR_CELL_SIZE - Q1890_BOUNDARY_HYSTERESIS) return false;
    return true;
}

bool RebuildExteriorWindowQ1890(float selectionGameX, float selectionGameY,
                                int32_t targetGridX, int32_t targetGridY) {
    if (!gExteriorStreamingActiveQ1890 || gExteriorStreamBusyQ1890) return false;
    gExteriorStreamBusyQ1890 = true;
    const uint64_t generation = ++gExteriorWindowGenerationQ1890;

    Q6H_LOGI("Q16.17 WINDOW BUILD BEGIN: generation=%llu worldspace=%08X persistent=%08X fromGrid=(%d,%d) toGrid=(%d,%d) selection=(%.2f %.2f) originXTEL=(%.2f %.2f %.2f)",
             static_cast<unsigned long long>(generation),
             gExteriorWorldspaceQ1890, gExteriorPersistentCellQ1890,
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             targetGridX, targetGridY, selectionGameX, selectionGameY,
             gExteriorOriginXQ1890, gExteriorOriginYQ1890, gExteriorOriginZQ1890);

    PumpFo3AndroidEventsQ1860();
    std::vector<Fo3WorldPlacement> placements;
    if (!LoadFo3WorldspaceNeighborhoodQ75(
            gExteriorWorldspaceQ1890,
            gExteriorPersistentCellQ1890,
            selectionGameX, selectionGameY,
            placements)) {
        Q6H_LOGE("Q16.17 WINDOW BUILD FAILED: generation=%llu phase=worldspace-load targetGrid=(%d,%d) oldSceneRetained=1",
                 static_cast<unsigned long long>(generation), targetGridX, targetGridY);
        gExteriorStreamBusyQ1890 = false;
        return false;
    }

    const size_t dynamicOnlyModels = ConfigureFo3CollisionPolicyQ710(placements);
    std::vector<CpuObject> selected;
    selected.reserve(placements.size() * 2u);
    size_t skipped = 0u;
    size_t unsupported = 0u;
    size_t cpuBoundaries = 0u;
    for (const Fo3WorldPlacement& placement : placements) {
        PumpFo3AndroidEventsQ1860();
        ++cpuBoundaries;
        if (Q74ShouldSkipPlacement(placement)) {
            ++skipped;
            continue;
        }
        std::vector<CpuObject> parts;
        if (!BuildCpuObjects(placement, parts)) {
            ++unsupported;
            continue;
        }
        for (CpuObject& part : parts) selected.push_back(std::move(part));
    }
    if (selected.size() < 2u) {
        Q6H_LOGE("Q16.17 WINDOW BUILD FAILED: generation=%llu phase=cpu placements=%zu shapes=%zu skipped=%zu unsupported=%zu oldSceneRetained=1",
                 static_cast<unsigned long long>(generation), placements.size(),
                 selected.size(), skipped, unsupported);
        gExteriorStreamBusyQ1890 = false;
        return false;
    }

    std::vector<GpuObject> replacement;
    replacement.reserve(selected.size());
    size_t gpuBoundaries = 0u;
    for (CpuObject& cpu : selected) {
        PumpFo3AndroidEventsQ1860();
        ++gpuBoundaries;
        GpuObject gpu;
        if (UploadCpuObject(cpu,
                            gExteriorOriginXQ1890,
                            gExteriorOriginYQ1890,
                            gExteriorOriginZQ1890,
                            gpu)) {
            replacement.push_back(std::move(gpu));
        } else {
            if (gpu.vbo) glDeleteBuffers(1, &gpu.vbo);
            if (gpu.vao) glDeleteVertexArrays(1, &gpu.vao);
        }
    }
    if (replacement.size() < 2u) {
        Q74DeleteGpuObjects(replacement);
        Q6H_LOGE("Q16.17 WINDOW BUILD FAILED: generation=%llu phase=gpu shapes=%zu oldSceneRetained=1",
                 static_cast<unsigned long long>(generation), replacement.size());
        gExteriorStreamBusyQ1890 = false;
        return false;
    }

    std::vector<Fo3WorldPlacement> collisionPlacements;
    collisionPlacements.reserve(selected.size());
    for (const CpuObject& cpu : selected) collisionPlacements.push_back(cpu.placement);

    PumpFo3AndroidEventsQ1860();
    const bool collisionReady = InitializeFo3CollisionOverlay(
        collisionPlacements,
        gExteriorOriginXQ1890,
        gExteriorOriginYQ1890,
        gExteriorOriginZQ1890,
        SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
    PumpFo3AndroidEventsQ1860();

    // Select LAND around the player's NEW grid but retain the original XTEL as
    // the render/grounding origin. This is the key no-snap distinction.
    SetFo3TerrainSelectionOverrideQ1890(true, selectionGameX, selectionGameY);
    const bool terrainReady = InitializeFo3TerrainRenderQ76(
        gExteriorWorldspaceQ1890,
        gExteriorOriginXQ1890,
        gExteriorOriginYQ1890,
        gExteriorOriginZQ1890,
        SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
    SetFo3TerrainSelectionOverrideQ1890(false, 0.0f, 0.0f);
    if (terrainReady) {
        ActivateFo3TerrainGroundingQ77(
            gExteriorWorldspaceQ1890,
            gExteriorOriginXQ1890,
            gExteriorOriginYQ1890,
            gExteriorOriginZQ1890,
            SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
    }
    PumpFo3AndroidEventsQ1860();

    const size_t oldObjects = gObjects.size();
    Q74DeleteGpuObjects(gObjects);
    gObjects = std::move(replacement);
    gSceneReady = !gObjects.empty();
    gLoggedFirstDraw = false;
    gExteriorWindowGridXQ1890 = targetGridX;
    gExteriorWindowGridYQ1890 = targetGridY;

    size_t triangles = 0u;
    for (const GpuObject& object : gObjects) {
        triangles += static_cast<size_t>(object.vertexCount / 3);
    }

    Q6H_LOGI("Q16.17 WINDOW BUILD READY: generation=%llu grid=(%d,%d) oldObjects=%zu newObjects=%zu triangles=%zu placements=%zu cpuBoundaries=%zu gpuBoundaries=%zu dynamicOnlyModels=%zu collisionReady=%d terrainReady=%d originPreserved=1 playerReset=0 loadingScreen=0",
             static_cast<unsigned long long>(generation),
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             oldObjects, gObjects.size(), triangles, placements.size(),
             cpuBoundaries, gpuBoundaries, dynamicOnlyModels,
             collisionReady ? 1 : 0, terrainReady ? 1 : 0);

    gExteriorStreamBusyQ1890 = false;
    return true;
}

void UpdateFo3ExteriorStreamingQ1890(float virtualHeadX, float virtualHeadZ) {
    if (!gExteriorStreamingActiveQ1890 || gExteriorStreamBusyQ1890 ||
        IsFo3LoadingVisibleQ1700()) return;

    const float gameX = gExteriorOriginXQ1890 +
                        virtualHeadX * FO3_UNITS_PER_METRE;
    const float gameY = gExteriorOriginYQ1890 +
                        (SCENE_FORWARD - virtualHeadZ) * FO3_UNITS_PER_METRE;
    const int32_t targetGridX = static_cast<int32_t>(
        std::floor(gameX / Q1890_EXTERIOR_CELL_SIZE));
    const int32_t targetGridY = static_cast<int32_t>(
        std::floor(gameY / Q1890_EXTERIOR_CELL_SIZE));

    if (targetGridX == gExteriorWindowGridXQ1890 &&
        targetGridY == gExteriorWindowGridYQ1890) return;
    if (!Q1890InsideCandidatePastHysteresis(
            gameX, gameY,
            gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
            targetGridX, targetGridY)) return;

    Q6H_LOGI("Q16.17 CELL CROSS: from=(%d,%d) to=(%d,%d) virtualHead=(%.3f %.3f) game=(%.2f %.2f) hysteresis=%.1f",
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             targetGridX, targetGridY,
             virtualHeadX, virtualHeadZ, gameX, gameY,
             Q1890_BOUNDARY_HYSTERESIS);
    RebuildExteriorWindowQ1890(gameX, gameY, targetGridX, targetGridY);
}

]==])

set(Q1890_DRAW_MARKER "void DrawSceneObject(const GpuObject& object) {")
string(FIND "${Q1890_NATIVE_SOURCE}" "${Q1890_DRAW_MARKER}" Q1890_DRAW_POS)
if(Q1890_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q16.17 could not find final DrawSceneObject anchor")
endif()
string(REPLACE "${Q1890_DRAW_MARKER}"
       "${Q1890_STREAM_HELPERS}${Q1890_DRAW_MARKER}"
       Q1890_NATIVE_SOURCE "${Q1890_NATIVE_SOURCE}")

# Arm/disarm streaming only after a successful mature scene swap. Exterior scene
# origin is the actual authored XTEL and never changes during window recentres.
set(Q1890_CONTEXT_OLD [==[
    CompleteFo3CellTransitionQ74(request.cellFormId);
    gCurrentCellFormId = request.cellFormId;
    PrimeFo3AuthoredDoorAnchorsQ1870(gCurrentCellFormId);
]==])
set(Q1890_CONTEXT_NEW [==[
    CompleteFo3CellTransitionQ74(request.cellFormId);
    gCurrentCellFormId = request.cellFormId;
    PrimeFo3AuthoredDoorAnchorsQ1870(gCurrentCellFormId);

    if (request.worldspaceFormId != 0u) {
        gExteriorStreamingActiveQ1890 = true;
        gExteriorWorldspaceQ1890 = request.worldspaceFormId;
        gExteriorPersistentCellQ1890 = request.cellFormId;
        gExteriorOriginXQ1890 = request.x;
        gExteriorOriginYQ1890 = request.y;
        gExteriorOriginZQ1890 = request.z;
        gExteriorWindowGridXQ1890 = static_cast<int32_t>(
            std::floor(request.x / Q1890_EXTERIOR_CELL_SIZE));
        gExteriorWindowGridYQ1890 = static_cast<int32_t>(
            std::floor(request.y / Q1890_EXTERIOR_CELL_SIZE));
        gExteriorWindowGenerationQ1890 = 0u;
        Q6H_LOGI("Q16.17 STREAM CONTEXT: active=1 worldspace=%08X persistent=%08X grid=(%d,%d) originXTEL=(%.2f %.2f %.2f) radius=1 rebuildMode=boundary-sync",
                 gExteriorWorldspaceQ1890, gExteriorPersistentCellQ1890,
                 gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
                 gExteriorOriginXQ1890, gExteriorOriginYQ1890,
                 gExteriorOriginZQ1890);
    } else {
        gExteriorStreamingActiveQ1890 = false;
        gExteriorWorldspaceQ1890 = 0u;
        gExteriorPersistentCellQ1890 = 0u;
        Q6H_LOGI("Q16.17 STREAM CONTEXT: active=0 reason=interior");
    }
]==])
string(FIND "${Q1890_NATIVE_SOURCE}" "${Q1890_CONTEXT_OLD}" Q1890_CONTEXT_POS)
if(Q1890_CONTEXT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.17 could not find Q16.15 mature completion/cache-prime block")
endif()
string(REPLACE "${Q1890_CONTEXT_OLD}" "${Q1890_CONTEXT_NEW}"
       Q1890_NATIVE_SOURCE "${Q1890_NATIVE_SOURCE}")
file(WRITE "${Q1890_NATIVE_FILE}" "${Q1890_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# 3. OpenXR host: after locomotion resolves, convert the virtual head position
#    back to absolute Fallout game X/Y and ask the streamer whether XCLC changed.
# -----------------------------------------------------------------------------
set(Q1890_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1890_Q4_FILE}")
    message(FATAL_ERROR "Q16.17 expected final OpenXR source")
endif()
file(READ "${Q1890_Q4_FILE}" Q1890_Q4_SOURCE)

set(Q1890_PLAYER_TICK_OLD [==[
                UpdatePlayer(views[0], frameState.predictedDisplayTime);
]==])
set(Q1890_PLAYER_TICK_NEW [==[
                UpdatePlayer(views[0], frameState.predictedDisplayTime);
                const XrPosef q1890VirtualHead = ToVirtualPose(views[0].pose);
                UpdateFo3ExteriorStreamingQ1890(q1890VirtualHead.position.x,
                                                q1890VirtualHead.position.z);
]==])
string(FIND "${Q1890_Q4_SOURCE}" "${Q1890_PLAYER_TICK_OLD}" Q1890_PLAYER_TICK_POS)
if(Q1890_PLAYER_TICK_POS EQUAL -1)
    message(FATAL_ERROR "Q16.17 could not find live post-locomotion streaming hook")
endif()
string(REPLACE "${Q1890_PLAYER_TICK_OLD}" "${Q1890_PLAYER_TICK_NEW}"
       Q1890_Q4_SOURCE "${Q1890_Q4_SOURCE}")

# Headset-visible proof Q16.16 -> Q16.17.
set(Q1890_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.16: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x7Du); // Q16.16: 6 = A F G E D C
]==])
set(Q1890_LABEL_NEW [==[
        q1600Digit(q1600X, 0x06u); // Q16.17: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x07u); // Q16.17: 7 = A B C
]==])
string(FIND "${Q1890_Q4_SOURCE}" "${Q1890_LABEL_OLD}" Q1890_LABEL_POS)
if(Q1890_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.17 could not find Q16.16 build-label digits")
endif()
string(REPLACE "${Q1890_LABEL_OLD}" "${Q1890_LABEL_NEW}"
       Q1890_Q4_SOURCE "${Q1890_Q4_SOURCE}")
string(REPLACE "Q16.16 BUILD LABEL:" "Q16.17 BUILD LABEL:"
       Q1890_Q4_SOURCE "${Q1890_Q4_SOURCE}")
string(REPLACE "text=Q16.16 anchor=left-hand" "text=Q16.17 anchor=left-hand"
       Q1890_Q4_SOURCE "${Q1890_Q4_SOURCE}")
string(REPLACE "Q16.16 AUTHORED DOOR FACING" "Q16.17 AUTHORED DOOR FACING"
       Q1890_Q4_SOURCE "${Q1890_Q4_SOURCE}")
file(WRITE "${Q1890_Q4_FILE}" "${Q1890_Q4_SOURCE}")

# -----------------------------------------------------------------------------
# Configure-time proof: streaming was added on top of, not instead of, the
# currently proven Q16.15 door cache / Q16.16 real HUD / mature environment path.
# -----------------------------------------------------------------------------
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "Q16.17 TERRAIN SELECTION:" Q1890_TERRAIN_OK)
string(FIND "${Q1890_NATIVE_SOURCE}" "Q16.17 CELL CROSS:" Q1890_CROSS_OK)
string(FIND "${Q1890_NATIVE_SOURCE}" "Q16.17 WINDOW BUILD READY:" Q1890_READY_OK)
string(FIND "${Q1890_NATIVE_SOURCE}" "Q16.15 XTEL CACHE" Q1890_CACHE_OK)
string(FIND "${Q1890_NATIVE_SOURCE}" "Q16.11 MATURE SCENE SWAP BEGIN:" Q1890_MATURE_OK)
string(FIND "${Q1890_NATIVE_SOURCE}" "LoadFo3CellEnvironmentQ1410(" Q1890_ENV_OK)
string(FIND "${Q1890_Q4_SOURCE}" "RenderFo3InteractionHudQ1880" Q1890_HUD_OK)
string(FIND "${Q1890_Q4_SOURCE}" "Q16.17: 7 = A B C" Q1890_LABEL_OK)
string(FIND "${Q1890_Q4_SOURCE}" "UpdateFo3ExteriorStreamingQ1890" Q1890_TICK_OK)
if(Q1890_TERRAIN_OK EQUAL -1 OR Q1890_CROSS_OK EQUAL -1 OR
   Q1890_READY_OK EQUAL -1 OR Q1890_CACHE_OK EQUAL -1 OR
   Q1890_MATURE_OK EQUAL -1 OR Q1890_ENV_OK EQUAL -1 OR
   Q1890_HUD_OK EQUAL -1 OR Q1890_LABEL_OK EQUAL -1 OR
   Q1890_TICK_OK EQUAL -1)
    message(FATAL_ERROR "Q16.17 cell-streaming verification failed")
endif()

message(STATUS "Q16.17 exterior streaming enabled: XCLC boundary -> recentered authored 3x3 objects/LAND/collision; original XTEL render origin preserved")
