# Q16.18: convert Q16.17's synchronous correctness proof into a staged exterior
# streamer suitable for standalone VR.
#
# Q16.17 proved the authored 4096-unit XCLC math and original-XTEL origin are
# correct, but rebuilt the entire 3x3 render/collision/LAND window on the frame
# that crossed a CELL boundary. Q16.18 keeps that coordinate math unchanged and:
#   - moves the expensive Fallout3.esm neighbourhood scan off the VR render thread,
#   - retains every already-live GPU shape whose REFR is still in the new window,
#   - builds only entering REFRs, one placement per interactive frame,
#   - uploads only entering visual shapes, one shape per interactive frame,
#   - gives collision and LAND their own isolated frames,
#   - commits the new window only after all staged work is ready.
#
# Texture objects remain owned by the renderer's shared texture cache. Retiring a
# streamed shape deletes only its VAO/VBO, exactly like the mature Q7 scene clear.

set(Q1900_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1900_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.18 expected Q16.17 generated renderer")
endif()
file(READ "${Q1900_NATIVE_FILE}" Q1900_NATIVE_SOURCE)

# Detached metadata jobs use only LoadFo3WorldspaceNeighborhoodQ75's local file
# state. Their result is published with release/acquire ordering and consumed on
# the renderer thread; no GL, collision or terrain state is touched by the worker.
string(PREPEND Q1900_NATIVE_SOURCE
       "#include <atomic>\n#include <memory>\n#include <thread>\n")

set(Q1900_UPDATE_MARKER
    "void UpdateFo3ExteriorStreamingQ1890(float virtualHeadX, float virtualHeadZ) {")
string(FIND "${Q1900_NATIVE_SOURCE}" "${Q1900_UPDATE_MARKER}" Q1900_UPDATE_POS)
if(Q1900_UPDATE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.18 could not find Q16.17 streaming update hook")
endif()

set(Q1900_HELPERS [==[
enum class Q1900StreamPhase : uint8_t {
    Metadata = 0,
    Cpu,
    Gpu,
    Collision,
    Terrain,
    Commit,
};

struct Q1900MetadataTask {
    uint32_t worldspace = 0u;
    uint32_t persistentCell = 0u;
    float selectionGameX = 0.0f;
    float selectionGameY = 0.0f;
    std::vector<Fo3WorldPlacement> placements;
    bool success = false;
    std::atomic<bool> ready{false};
};

struct Q1900PendingStream {
    bool active = false;
    uint64_t generation = 0u;
    int32_t targetGridX = 0;
    int32_t targetGridY = 0;
    float selectionGameX = 0.0f;
    float selectionGameY = 0.0f;
    Q1900StreamPhase phase = Q1900StreamPhase::Metadata;
    std::shared_ptr<Q1900MetadataTask> metadata;
    std::vector<Fo3WorldPlacement> targetPlacements;
    std::vector<Fo3WorldPlacement> incomingPlacements;
    std::vector<CpuObject> incomingCpu;
    std::vector<GpuObject> incomingGpu;
    std::unordered_map<uint32_t, uint8_t> targetRefs;
    std::unordered_map<uint32_t, uint8_t> visibleRefs;
    size_t cpuCursor = 0u;
    size_t gpuCursor = 0u;
    size_t retainedShapes = 0u;
    size_t skippedPlacements = 0u;
    size_t unsupportedPlacements = 0u;
    size_t dynamicOnlyModels = 0u;
    size_t frames = 0u;
    bool collisionReady = false;
    bool terrainReady = false;
};

Q1900PendingStream gPendingStreamQ1900;
constexpr size_t Q1900_CPU_PLACEMENTS_PER_FRAME = 1u;
constexpr size_t Q1900_GPU_SHAPES_PER_FRAME = 1u;

void Q1900DeleteGpuShape(GpuObject& object) {
    // diffuse/normal are shared texture-cache handles and must survive a REFR
    // leaving the live window. Only per-shape geometry belongs to this object.
    if (object.vbo) {
        glDeleteBuffers(1, &object.vbo);
        object.vbo = 0u;
    }
    if (object.vao) {
        glDeleteVertexArrays(1, &object.vao);
        object.vao = 0u;
    }
}

void Q1900CancelPending(const char* reason) {
    if (!gPendingStreamQ1900.active) {
        gExteriorStreamBusyQ1890 = false;
        return;
    }
    const uint64_t generation = gPendingStreamQ1900.generation;
    for (GpuObject& object : gPendingStreamQ1900.incomingGpu) {
        Q1900DeleteGpuShape(object);
    }
    Q6H_LOGW("Q16.18 STREAM CANCELLED: generation=%llu reason=%s oldWindowRetained=1",
             static_cast<unsigned long long>(generation),
             reason ? reason : "unknown");
    gPendingStreamQ1900 = Q1900PendingStream{};
    gExteriorStreamBusyQ1890 = false;
}

bool Q1900BeginStream(float selectionGameX, float selectionGameY,
                      int32_t targetGridX, int32_t targetGridY) {
    if (!gExteriorStreamingActiveQ1890 || gExteriorStreamBusyQ1890 ||
        gPendingStreamQ1900.active) return false;

    gExteriorStreamBusyQ1890 = true;
    gPendingStreamQ1900 = Q1900PendingStream{};
    gPendingStreamQ1900.active = true;
    gPendingStreamQ1900.generation = ++gExteriorWindowGenerationQ1890;
    gPendingStreamQ1900.targetGridX = targetGridX;
    gPendingStreamQ1900.targetGridY = targetGridY;
    gPendingStreamQ1900.selectionGameX = selectionGameX;
    gPendingStreamQ1900.selectionGameY = selectionGameY;
    gPendingStreamQ1900.phase = Q1900StreamPhase::Metadata;

    auto task = std::make_shared<Q1900MetadataTask>();
    task->worldspace = gExteriorWorldspaceQ1890;
    task->persistentCell = gExteriorPersistentCellQ1890;
    task->selectionGameX = selectionGameX;
    task->selectionGameY = selectionGameY;
    gPendingStreamQ1900.metadata = task;

    Q6H_LOGI("Q16.18 STREAM QUEUED: generation=%llu worldspace=%08X persistent=%08X fromGrid=(%d,%d) toGrid=(%d,%d) selection=(%.2f %.2f) retainedWindowLive=1 metadataThread=worker cpuBudget=%zu gpuBudget=%zu",
             static_cast<unsigned long long>(gPendingStreamQ1900.generation),
             task->worldspace, task->persistentCell,
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             targetGridX, targetGridY, selectionGameX, selectionGameY,
             Q1900_CPU_PLACEMENTS_PER_FRAME, Q1900_GPU_SHAPES_PER_FRAME);

    std::thread([task]() {
        task->success = LoadFo3WorldspaceNeighborhoodQ75(
            task->worldspace, task->persistentCell,
            task->selectionGameX, task->selectionGameY,
            task->placements);
        task->ready.store(true, std::memory_order_release);
    }).detach();
    return true;
}

void Q1900PrepareMetadata() {
    auto task = gPendingStreamQ1900.metadata;
    if (!task || !task->ready.load(std::memory_order_acquire)) return;

    if (!task->success || task->placements.empty()) {
        Q6H_LOGE("Q16.18 STREAM FAILED: generation=%llu phase=metadata placements=%zu oldWindowRetained=1",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 task ? task->placements.size() : 0u);
        Q1900CancelPending("metadata-failed");
        return;
    }

    gPendingStreamQ1900.targetPlacements = std::move(task->placements);
    gPendingStreamQ1900.metadata.reset();
    gPendingStreamQ1900.dynamicOnlyModels =
        ConfigureFo3CollisionPolicyQ710(gPendingStreamQ1900.targetPlacements);

    std::unordered_map<uint32_t, uint8_t> existingRefs;
    existingRefs.reserve(gObjects.size());
    for (const GpuObject& object : gObjects) {
        if (object.refFormId != 0u) existingRefs[object.refFormId] = 1u;
    }

    gPendingStreamQ1900.targetRefs.reserve(
        gPendingStreamQ1900.targetPlacements.size());
    std::unordered_map<uint32_t, uint8_t> queuedEnteringRefs;
    queuedEnteringRefs.reserve(gPendingStreamQ1900.targetPlacements.size());

    for (const Fo3WorldPlacement& placement : gPendingStreamQ1900.targetPlacements) {
        if (placement.refFormId == 0u) continue;
        gPendingStreamQ1900.targetRefs[placement.refFormId] = 1u;
        if (existingRefs.find(placement.refFormId) == existingRefs.end() &&
            queuedEnteringRefs.emplace(placement.refFormId, 1u).second) {
            gPendingStreamQ1900.incomingPlacements.push_back(placement);
        }
    }

    for (const GpuObject& object : gObjects) {
        if (gPendingStreamQ1900.targetRefs.find(object.refFormId) !=
            gPendingStreamQ1900.targetRefs.end()) {
            ++gPendingStreamQ1900.retainedShapes;
            gPendingStreamQ1900.visibleRefs[object.refFormId] = 1u;
        }
    }

    gPendingStreamQ1900.incomingCpu.reserve(
        gPendingStreamQ1900.incomingPlacements.size() * 2u);
    gPendingStreamQ1900.phase = gPendingStreamQ1900.incomingPlacements.empty()
        ? Q1900StreamPhase::Collision : Q1900StreamPhase::Cpu;

    Q6H_LOGI("Q16.18 METADATA READY: generation=%llu targetPlacements=%zu targetRefs=%zu enteringPlacements=%zu retainedShapes=%zu dynamicOnlyModels=%zu oldWindowStillLive=1",
             static_cast<unsigned long long>(gPendingStreamQ1900.generation),
             gPendingStreamQ1900.targetPlacements.size(),
             gPendingStreamQ1900.targetRefs.size(),
             gPendingStreamQ1900.incomingPlacements.size(),
             gPendingStreamQ1900.retainedShapes,
             gPendingStreamQ1900.dynamicOnlyModels);
}

void Q1900AdvanceCpu() {
    size_t budget = Q1900_CPU_PLACEMENTS_PER_FRAME;
    while (budget-- > 0u &&
           gPendingStreamQ1900.cpuCursor < gPendingStreamQ1900.incomingPlacements.size()) {
        const Fo3WorldPlacement& placement =
            gPendingStreamQ1900.incomingPlacements[gPendingStreamQ1900.cpuCursor++];
        if (Q74ShouldSkipPlacement(placement)) {
            ++gPendingStreamQ1900.skippedPlacements;
            continue;
        }
        std::vector<CpuObject> parts;
        if (!BuildCpuObjects(placement, parts)) {
            ++gPendingStreamQ1900.unsupportedPlacements;
            continue;
        }
        for (CpuObject& part : parts) {
            gPendingStreamQ1900.incomingCpu.push_back(std::move(part));
        }
    }

    if (gPendingStreamQ1900.cpuCursor >=
        gPendingStreamQ1900.incomingPlacements.size()) {
        gPendingStreamQ1900.phase = gPendingStreamQ1900.incomingCpu.empty()
            ? Q1900StreamPhase::Collision : Q1900StreamPhase::Gpu;
        Q6H_LOGI("Q16.18 CPU READY: generation=%llu enteringPlacements=%zu cpuShapes=%zu skipped=%zu unsupported=%zu frames=%zu",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.incomingPlacements.size(),
                 gPendingStreamQ1900.incomingCpu.size(),
                 gPendingStreamQ1900.skippedPlacements,
                 gPendingStreamQ1900.unsupportedPlacements,
                 gPendingStreamQ1900.frames);
    }
}

void Q1900AdvanceGpu() {
    size_t budget = Q1900_GPU_SHAPES_PER_FRAME;
    while (budget-- > 0u &&
           gPendingStreamQ1900.gpuCursor < gPendingStreamQ1900.incomingCpu.size()) {
        CpuObject& cpu = gPendingStreamQ1900.incomingCpu[gPendingStreamQ1900.gpuCursor++];
        const uint32_t refFormId = cpu.placement.refFormId;
        GpuObject gpu;
        if (UploadCpuObject(cpu,
                            gExteriorOriginXQ1890,
                            gExteriorOriginYQ1890,
                            gExteriorOriginZQ1890,
                            gpu)) {
            gPendingStreamQ1900.visibleRefs[refFormId] = 1u;
            gPendingStreamQ1900.incomingGpu.push_back(std::move(gpu));
        } else {
            Q1900DeleteGpuShape(gpu);
        }
        cpu = CpuObject{};
    }

    if (gPendingStreamQ1900.gpuCursor >= gPendingStreamQ1900.incomingCpu.size()) {
        gPendingStreamQ1900.phase = Q1900StreamPhase::Collision;
        Q6H_LOGI("Q16.18 GPU READY: generation=%llu uploadedShapes=%zu visibleRefs=%zu frames=%zu textureCachePreserved=1",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.incomingGpu.size(),
                 gPendingStreamQ1900.visibleRefs.size(),
                 gPendingStreamQ1900.frames);
    }
}

void Q1900AdvanceCollision() {
    std::vector<Fo3WorldPlacement> collisionPlacements;
    collisionPlacements.reserve(gPendingStreamQ1900.targetPlacements.size());
    std::unordered_map<uint32_t, uint8_t> seenRefs;
    seenRefs.reserve(gPendingStreamQ1900.targetPlacements.size());
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
    gPendingStreamQ1900.collisionReady = InitializeFo3CollisionOverlay(
        collisionPlacements,
        gExteriorOriginXQ1890,
        gExteriorOriginYQ1890,
        gExteriorOriginZQ1890,
        SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
    PumpFo3AndroidEventsQ1860();
    gPendingStreamQ1900.phase = Q1900StreamPhase::Terrain;

    Q6H_LOGI("Q16.18 COLLISION READY: generation=%llu uniqueRenderableRefs=%zu ready=%d isolatedFrame=1",
             static_cast<unsigned long long>(gPendingStreamQ1900.generation),
             collisionPlacements.size(),
             gPendingStreamQ1900.collisionReady ? 1 : 0);
}

void Q1900AdvanceTerrain() {
    PumpFo3AndroidEventsQ1860();
    SetFo3TerrainSelectionOverrideQ1890(
        true,
        gPendingStreamQ1900.selectionGameX,
        gPendingStreamQ1900.selectionGameY);
    gPendingStreamQ1900.terrainReady = InitializeFo3TerrainRenderQ76(
        gExteriorWorldspaceQ1890,
        gExteriorOriginXQ1890,
        gExteriorOriginYQ1890,
        gExteriorOriginZQ1890,
        SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
    SetFo3TerrainSelectionOverrideQ1890(false, 0.0f, 0.0f);
    if (gPendingStreamQ1900.terrainReady) {
        ActivateFo3TerrainGroundingQ77(
            gExteriorWorldspaceQ1890,
            gExteriorOriginXQ1890,
            gExteriorOriginYQ1890,
            gExteriorOriginZQ1890,
            SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
    }
    PumpFo3AndroidEventsQ1860();
    gPendingStreamQ1900.phase = Q1900StreamPhase::Commit;

    Q6H_LOGI("Q16.18 TERRAIN READY: generation=%llu selection=(%.2f %.2f) ready=%d isolatedFrame=1 originPreserved=1",
             static_cast<unsigned long long>(gPendingStreamQ1900.generation),
             gPendingStreamQ1900.selectionGameX,
             gPendingStreamQ1900.selectionGameY,
             gPendingStreamQ1900.terrainReady ? 1 : 0);
}

void Q1900CommitWindow() {
    const size_t oldShapes = gObjects.size();
    const size_t enteringShapes = gPendingStreamQ1900.incomingGpu.size();
    std::vector<GpuObject> replacement;
    replacement.reserve(gPendingStreamQ1900.retainedShapes + enteringShapes);

    size_t retiredShapes = 0u;
    for (GpuObject& object : gObjects) {
        if (gPendingStreamQ1900.targetRefs.find(object.refFormId) !=
            gPendingStreamQ1900.targetRefs.end()) {
            replacement.push_back(std::move(object));
            // GpuObject is not an owning RAII type; clear moved geometry handles
            // so no later cleanup path can accidentally delete retained shapes.
            object.vbo = 0u;
            object.vao = 0u;
        } else {
            Q1900DeleteGpuShape(object);
            ++retiredShapes;
        }
    }
    gObjects.clear();

    for (GpuObject& object : gPendingStreamQ1900.incomingGpu) {
        replacement.push_back(std::move(object));
        object.vbo = 0u;
        object.vao = 0u;
    }

    gObjects = std::move(replacement);
    gSceneReady = !gObjects.empty();
    gLoggedFirstDraw = false;
    gExteriorWindowGridXQ1890 = gPendingStreamQ1900.targetGridX;
    gExteriorWindowGridYQ1890 = gPendingStreamQ1900.targetGridY;

    size_t triangles = 0u;
    for (const GpuObject& object : gObjects) {
        triangles += static_cast<size_t>(object.vertexCount / 3);
    }

    const uint64_t generation = gPendingStreamQ1900.generation;
    const size_t frames = gPendingStreamQ1900.frames;
    const size_t retainedShapes = gPendingStreamQ1900.retainedShapes;
    const bool collisionReady = gPendingStreamQ1900.collisionReady;
    const bool terrainReady = gPendingStreamQ1900.terrainReady;

    Q6H_LOGI("Q16.18 WINDOW READY: generation=%llu grid=(%d,%d) oldShapes=%zu retainedShapes=%zu enteringShapes=%zu retiredShapes=%zu liveShapes=%zu triangles=%zu stagedFrames=%zu collisionReady=%d terrainReady=%d originPreserved=1 playerReset=0 fullSceneRebuild=0",
             static_cast<unsigned long long>(generation),
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             oldShapes, retainedShapes, enteringShapes, retiredShapes,
             gObjects.size(), triangles, frames,
             collisionReady ? 1 : 0, terrainReady ? 1 : 0);

    gPendingStreamQ1900 = Q1900PendingStream{};
    gExteriorStreamBusyQ1890 = false;
}

void Q1900AdvanceStream() {
    if (!gPendingStreamQ1900.active) return;
    ++gPendingStreamQ1900.frames;

    // Let explicit door/cell loading own the renderer while its loading state is
    // visible. A detached metadata read may finish meanwhile, but is not consumed.
    if (IsFo3LoadingVisibleQ1700()) return;

    const auto task = gPendingStreamQ1900.metadata;
    if (!gExteriorStreamingActiveQ1890 ||
        (task && (task->worldspace != gExteriorWorldspaceQ1890 ||
                  task->persistentCell != gExteriorPersistentCellQ1890))) {
        Q1900CancelPending("scene-context-changed");
        return;
    }

    switch (gPendingStreamQ1900.phase) {
        case Q1900StreamPhase::Metadata:
            Q1900PrepareMetadata();
            break;
        case Q1900StreamPhase::Cpu:
            Q1900AdvanceCpu();
            break;
        case Q1900StreamPhase::Gpu:
            Q1900AdvanceGpu();
            break;
        case Q1900StreamPhase::Collision:
            Q1900AdvanceCollision();
            break;
        case Q1900StreamPhase::Terrain:
            Q1900AdvanceTerrain();
            break;
        case Q1900StreamPhase::Commit:
            Q1900CommitWindow();
            break;
    }
}

]==])

string(REPLACE "${Q1900_UPDATE_MARKER}"
       "${Q1900_HELPERS}${Q1900_UPDATE_MARKER}"
       Q1900_NATIVE_SOURCE "${Q1900_NATIVE_SOURCE}")

# Advance one staged unit before evaluating another boundary. While a stream is
# pending gExteriorStreamBusyQ1890 remains true, preventing duplicate queues.
set(Q1900_UPDATE_GUARD_OLD [==[
void UpdateFo3ExteriorStreamingQ1890(float virtualHeadX, float virtualHeadZ) {
    if (!gExteriorStreamingActiveQ1890 || gExteriorStreamBusyQ1890 ||
        IsFo3LoadingVisibleQ1700()) return;
]==])
set(Q1900_UPDATE_GUARD_NEW [==[
void UpdateFo3ExteriorStreamingQ1890(float virtualHeadX, float virtualHeadZ) {
    Q1900AdvanceStream();
    if (!gExteriorStreamingActiveQ1890 || gExteriorStreamBusyQ1890 ||
        IsFo3LoadingVisibleQ1700()) return;
]==])
string(FIND "${Q1900_NATIVE_SOURCE}" "${Q1900_UPDATE_GUARD_OLD}" Q1900_GUARD_POS)
if(Q1900_GUARD_POS EQUAL -1)
    message(FATAL_ERROR "Q16.18 could not find Q16.17 synchronous stream guard")
endif()
string(REPLACE "${Q1900_UPDATE_GUARD_OLD}" "${Q1900_UPDATE_GUARD_NEW}"
       Q1900_NATIVE_SOURCE "${Q1900_NATIVE_SOURCE}")

string(FIND "${Q1900_NATIVE_SOURCE}"
       "RebuildExteriorWindowQ1890(gameX, gameY, targetGridX, targetGridY);"
       Q1900_SYNC_REBUILD_POS)
if(Q1900_SYNC_REBUILD_POS EQUAL -1)
    message(FATAL_ERROR "Q16.18 could not find Q16.17 synchronous rebuild call")
endif()
string(REPLACE
    "RebuildExteriorWindowQ1890(gameX, gameY, targetGridX, targetGridY);"
    "Q1900BeginStream(gameX, gameY, targetGridX, targetGridY);"
    Q1900_NATIVE_SOURCE "${Q1900_NATIVE_SOURCE}")

string(REPLACE "Q16.17 CELL CROSS:" "Q16.18 CELL CROSS:"
       Q1900_NATIVE_SOURCE "${Q1900_NATIVE_SOURCE}")
string(REPLACE "Q16.17 STREAM CONTEXT:" "Q16.18 STREAM CONTEXT:"
       Q1900_NATIVE_SOURCE "${Q1900_NATIVE_SOURCE}")
string(REPLACE "rebuildMode=boundary-sync"
       "rebuildMode=async-metadata+incremental-edge"
       Q1900_NATIVE_SOURCE "${Q1900_NATIVE_SOURCE}")

file(WRITE "${Q1900_NATIVE_FILE}" "${Q1900_NATIVE_SOURCE}")

# Headset-visible proof Q16.17 -> Q16.18. No HUD layout, metrics or wording
# changes are made here; only the tiny left-hand build label advances.
set(Q1900_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1900_Q4_FILE}")
    message(FATAL_ERROR "Q16.18 expected final OpenXR source")
endif()
file(READ "${Q1900_Q4_FILE}" Q1900_Q4_SOURCE)
set(Q1900_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.17: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x07u); // Q16.17: 7 = A B C
]==])
set(Q1900_LABEL_NEW [==[
        q1600Digit(q1600X, 0x06u); // Q16.18: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x7Fu); // Q16.18: 8 = A B C D E F G
]==])
string(FIND "${Q1900_Q4_SOURCE}" "${Q1900_LABEL_OLD}" Q1900_LABEL_POS)
if(Q1900_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.18 could not find Q16.17 build-label digits")
endif()
string(REPLACE "${Q1900_LABEL_OLD}" "${Q1900_LABEL_NEW}"
       Q1900_Q4_SOURCE "${Q1900_Q4_SOURCE}")
string(REPLACE "Q16.17 BUILD LABEL:" "Q16.18 BUILD LABEL:"
       Q1900_Q4_SOURCE "${Q1900_Q4_SOURCE}")
string(REPLACE "text=Q16.17 anchor=left-hand" "text=Q16.18 anchor=left-hand"
       Q1900_Q4_SOURCE "${Q1900_Q4_SOURCE}")
string(REPLACE "Q16.17 AUTHORED DOOR FACING" "Q16.18 AUTHORED DOOR FACING"
       Q1900_Q4_SOURCE "${Q1900_Q4_SOURCE}")
file(WRITE "${Q1900_Q4_FILE}" "${Q1900_Q4_SOURCE}")

# Configure-time proof: synchronous Q16.17 fallback remains compiled but the live
# boundary hook must now queue Q16.18 and advance it frame-by-frame. Also verify
# the already-proven Q16.15 door cache and Q16.16 baseline HUD survived.
string(FIND "${Q1900_NATIVE_SOURCE}" "Q16.18 STREAM QUEUED:" Q1900_QUEUE_OK)
string(FIND "${Q1900_NATIVE_SOURCE}" "Q16.18 WINDOW READY:" Q1900_READY_OK)
string(FIND "${Q1900_NATIVE_SOURCE}" "Q1900AdvanceStream();" Q1900_PUMP_OK)
string(FIND "${Q1900_NATIVE_SOURCE}" "Q1900BeginStream(gameX, gameY" Q1900_BEGIN_OK)
string(FIND "${Q1900_NATIVE_SOURCE}" "Q16.15 XTEL CACHE:" Q1900_CACHE_OK)
string(FIND "${Q1900_Q4_SOURCE}" "RenderFo3InteractionHudQ1880" Q1900_HUD_OK)
string(FIND "${Q1900_Q4_SOURCE}" "Q16.18: 8 = A B C D E F G" Q1900_LABEL_OK)
if(Q1900_QUEUE_OK EQUAL -1 OR Q1900_READY_OK EQUAL -1 OR
   Q1900_PUMP_OK EQUAL -1 OR Q1900_BEGIN_OK EQUAL -1 OR
   Q1900_CACHE_OK EQUAL -1 OR Q1900_HUD_OK EQUAL -1 OR
   Q1900_LABEL_OK EQUAL -1)
    message(FATAL_ERROR "Q16.18 incremental streaming verification failed")
endif()

message(STATUS "Q16.18 incremental exterior streaming enabled: async ESM metadata + retained overlap + 1 placement/frame CPU + 1 shape/frame GPU + isolated collision/LAND frames")
