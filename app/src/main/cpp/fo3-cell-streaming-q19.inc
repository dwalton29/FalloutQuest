// Q19 - persistent exterior CELL streaming.
// Included by fo3-runtime.cpp inside its anonymous namespace after the legacy
// Q18 whole-window pipeline. Wasteland UpdateFo3ExteriorStreamingQ1890 routes
// here; the legacy pipeline remains available for non-Wasteland contexts.

enum class Q1900CellStageQ19 : uint8_t {
    Ready = 0,
    Requested,
    Worker,
    Uploading,
    Failed,
};

struct Q1900TextureRequestQ19 {
    std::string path;
    std::string label;
    std::array<uint8_t, 4> fallback{};
};

struct Q1960TextureUploadQ19 {
    bool active = false;
    GLuint id = 0u;
    std::string cacheKey;
    std::string path;
    std::string label;
    Fo3RgbaTexture texture;
    bool real = false;
    size_t nextRow = 0u;
    size_t chunks = 0u;
    uint64_t totalUs = 0u;
    uint64_t maxStepUs = 0u;
};

struct Q1960ShapeUploadQ19 {
    bool active = false;
    GpuObject gpu;
    std::vector<float> vertices;
    size_t nextByte = 0u;
    size_t chunks = 0u;
    uint64_t totalUs = 0u;
    uint64_t maxStepUs = 0u;
};

struct Q1900CellWorkerTaskQ19 {
    uint64_t contextSerial = 0u;
    uint32_t worldspace = 0u;
    uint32_t persistentCell = 0u;
    int32_t gridX = 0;
    int32_t gridY = 0;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    bool needVisual = false;

    std::vector<Fo3WorldPlacement> placements;
    std::vector<CpuObject> cpu;
    std::vector<Q1900TextureRequestQ19> textures;

    size_t skipped = 0u;
    size_t unsupported = 0u;
    size_t collisionPrimedRefs = 0u;
    size_t collisionTriangles = 0u;
    uint64_t metadataUs = 0u;
    uint64_t cpuUs = 0u;
    uint64_t textureUs = 0u;
    uint64_t collisionUs = 0u;
    uint64_t maxCollisionItemUs = 0u;
    bool success = false;
    std::atomic<bool> ready{false};
};

struct Q1900CellStateQ19 {
    int32_t gridX = 0;
    int32_t gridY = 0;
    Q1900CellStageQ19 stage = Q1900CellStageQ19::Requested;
    bool visualReady = false;
    bool metadataReady = false;
    bool collisionPrimed = false;
    bool wanted = false;
    bool prefetched = false;
    uint64_t lastWantedTick = 0u;
    std::chrono::steady_clock::time_point requestedAt{};

    std::vector<Fo3WorldPlacement> placements;
    std::vector<CpuObject> cpu;
    std::vector<Q1900TextureRequestQ19> textures;
    std::vector<GpuObject> stagedGpu;
    size_t textureCursor = 0u;
    size_t gpuCursor = 0u;
    size_t skipped = 0u;
    size_t unsupported = 0u;
    uint64_t workerUs = 0u;
    uint64_t gpuUs = 0u;
    uint64_t maxTextureGpuUs = 0u;
    uint64_t maxShapeGpuUs = 0u;
    Q1960TextureUploadQ19 q1960TextureUpload;
    Q1960ShapeUploadQ19 q1960ShapeUpload;
};

std::unordered_map<uint64_t, Q1900CellStateQ19> gQ1900CellsQ19;
std::shared_ptr<Q1900CellWorkerTaskQ19> gQ1900WorkerQ19;

struct Q1970LodWorkerTaskQ19;
extern std::shared_ptr<Q1970LodWorkerTaskQ19> gQ1970LodWorkerQ19;

struct Q1930CollisionSnapshotTaskQ19 {
    uint64_t contextSerial = 0u;
    int32_t centreX = 0;
    int32_t centreY = 0;
    std::vector<Fo3WorldPlacement> placements;
    uint64_t snapshotToken = 0u;
    uint64_t prepareUs = 0u;
    bool success = false;
    std::atomic<bool> ready{false};
};
std::shared_ptr<Q1930CollisionSnapshotTaskQ19> gQ1930CollisionTaskQ19;
uint64_t gQ1900ContextSerialQ19 = 0u;
uint64_t gQ1900TickQ19 = 0u;
bool gQ1900ContextReadyQ19 = false;
uint32_t gQ1900WorldspaceQ19 = 0u;
uint32_t gQ1900PersistentQ19 = 0u;
float gQ1900OriginXQ19 = 0.0f;
float gQ1900OriginYQ19 = 0.0f;
float gQ1900OriginZQ19 = 0.0f;
bool gQ1900CollisionCentreValidQ19 = false;
int32_t gQ1900CollisionCentreXQ19 = 0;
int32_t gQ1900CollisionCentreYQ19 = 0;
bool gQ1900TerrainCentreValidQ19 = false;
int32_t gQ1900TerrainCentreXQ19 = 0;
int32_t gQ1900TerrainCentreYQ19 = 0;
int32_t gQ1900LastActualXQ19 = INT32_MIN;
int32_t gQ1900LastActualYQ19 = INT32_MIN;

constexpr int Q1900_RESIDENT_RADIUS_Q19 = 2;
constexpr int Q1900_CACHE_RADIUS_Q19 = 3;
constexpr float Q1900_PREFETCH_EDGE_UNITS_Q19 = 1152.0f;
constexpr uint64_t Q1900_GPU_BUDGET_US_Q19 = 2200u;
constexpr size_t Q1960_GPU_BYTES_PER_FRAME = 256u * 1024u;
constexpr size_t Q1960_TEXTURE_CHUNK_BYTES = 128u * 1024u;
constexpr size_t Q1960_VBO_CHUNK_BYTES = 128u * 1024u;
constexpr uint64_t Q1910_GPU_DELETE_BUDGET_US = 750u;
constexpr uint64_t Q1910_GPU_ITEM_WARN_US = 8000u;
std::deque<GpuObject> gQ1910DeferredGpuDeletesQ19;

void Q1910QueueGpuDeleteQ19(GpuObject& object) {
    if (object.vbo == 0u && object.vao == 0u) return;
    gQ1910DeferredGpuDeletesQ19.push_back(std::move(object));
    // GpuObject is not RAII. Clear the source handles explicitly so moved-from
    // vectors can be compacted/destroyed without double-deleting GL geometry.
    object.vbo = 0u;
    object.vao = 0u;
}

void Q1910DrainDeferredGpuDeletesQ19(bool flushAll = false) {
    if (gQ1910DeferredGpuDeletesQ19.empty()) return;
    const size_t queuedBefore = gQ1910DeferredGpuDeletesQ19.size();
    const auto started = std::chrono::steady_clock::now();
    size_t deleted = 0u;
    uint64_t maxItemUs = 0u;

    while (!gQ1910DeferredGpuDeletesQ19.empty()) {
        const auto itemStarted = std::chrono::steady_clock::now();
        Q1900DeleteGpuShape(gQ1910DeferredGpuDeletesQ19.front());
        gQ1910DeferredGpuDeletesQ19.pop_front();
        const uint64_t itemUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - itemStarted).count());
        maxItemUs = std::max(maxItemUs, itemUs);
        ++deleted;

        if (!flushAll) {
            const uint64_t elapsedUs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - started).count());
            if (elapsedUs >= Q1910_GPU_DELETE_BUDGET_US) break;
        }
    }

    const uint64_t elapsedUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count());
    if (flushAll || gQ1910DeferredGpuDeletesQ19.empty() ||
        maxItemUs >= Q1910_GPU_ITEM_WARN_US ||
        elapsedUs >= Q1910_GPU_DELETE_BUDGET_US * 2u) {
        Q6H_LOGI("Q19.1 GPU DELETE DRAIN: deleted=%zu queuedBefore=%zu remaining=%zu elapsedUs=%llu maxItemUs=%llu budgetUs=%llu flush=%d",
                 deleted, queuedBefore, gQ1910DeferredGpuDeletesQ19.size(),
                 static_cast<unsigned long long>(elapsedUs),
                 static_cast<unsigned long long>(maxItemUs),
                 static_cast<unsigned long long>(Q1910_GPU_DELETE_BUDGET_US),
                 flushAll ? 1 : 0);
    }
}

uint64_t Q1900CellKeyQ19(int32_t x, int32_t y) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32u) |
           static_cast<uint32_t>(y);
}

bool Q1900CellVisualReadyQ19(int32_t x, int32_t y) {
    if (!gQ1900ContextReadyQ19) return false;
    const auto found = gQ1900CellsQ19.find(Q1900CellKeyQ19(x, y));
    return found != gQ1900CellsQ19.end() && found->second.visualReady;
}

Q1900CellStateQ19& Q1900EnsureCellStateQ19(int32_t x, int32_t y) {
    const uint64_t key = Q1900CellKeyQ19(x, y);
    auto found = gQ1900CellsQ19.find(key);
    if (found != gQ1900CellsQ19.end()) return found->second;
    Q1900CellStateQ19 state;
    state.gridX = x;
    state.gridY = y;
    state.stage = Q1900CellStageQ19::Requested;
    state.requestedAt = std::chrono::steady_clock::now();
    auto inserted = gQ1900CellsQ19.emplace(key, std::move(state));
    Q6H_LOGI("Q19 CELL REQUEST: cell=(%d,%d) reason=new-resident-cell", x, y);
    return inserted.first->second;
}

void Q1900SeedTextureKnowledgeQ19() {
    std::lock_guard<std::mutex> lock(gQ1900TextureCpuMutexQ19);
    for (const auto& entry : gTextureCache)
        gQ1900KnownGpuTextureKeysQ19.insert(entry.first);
}

void Q1900ResetCellsQ19(int32_t actualGridX, int32_t actualGridY) {
    ++gQ1900ContextSerialQ19;
    gQ1900CellsQ19.clear();
    gQ1900ContextReadyQ19 = true;
    gQ1900WorldspaceQ19 = gExteriorWorldspaceQ1890;
    gQ1900PersistentQ19 = gExteriorPersistentCellQ1890;
    gQ1900OriginXQ19 = gExteriorOriginXQ1890;
    gQ1900OriginYQ19 = gExteriorOriginYQ1890;
    gQ1900OriginZQ19 = gExteriorOriginZQ1890;
    gQ1900CollisionCentreValidQ19 = true;
    gQ1900CollisionCentreXQ19 = gExteriorWindowGridXQ1890;
    gQ1900CollisionCentreYQ19 = gExteriorWindowGridYQ1890;
    gQ1900TerrainCentreValidQ19 = true;
    gQ1900TerrainCentreXQ19 = gExteriorWindowGridXQ1890;
    gQ1900TerrainCentreYQ19 = gExteriorWindowGridYQ1890;
    gQ1900LastActualXQ19 = actualGridX;
    gQ1900LastActualYQ19 = actualGridY;

    // The boot/transition path already built a complete radius-2 visual window.
    // Seed all 25 CELLs as visually ready, including authored empty cells.
    for (int dy = -Q1900_RESIDENT_RADIUS_Q19;
         dy <= Q1900_RESIDENT_RADIUS_Q19; ++dy) {
        for (int dx = -Q1900_RESIDENT_RADIUS_Q19;
             dx <= Q1900_RESIDENT_RADIUS_Q19; ++dx) {
            Q1900CellStateQ19 state;
            state.gridX = gExteriorWindowGridXQ1890 + dx;
            state.gridY = gExteriorWindowGridYQ1890 + dy;
            state.stage = Q1900CellStageQ19::Requested;
            state.visualReady = true;
            state.requestedAt = std::chrono::steady_clock::now();
            gQ1900CellsQ19.emplace(
                Q1900CellKeyQ19(state.gridX, state.gridY),
                std::move(state));
        }
    }
    Q1900SeedTextureKnowledgeQ19();

    // Retire any in-flight legacy Q18 generation. Its completed live objects
    // remain in gObjects and are now owned by the seeded CELL states above.
    if (gPendingStreamQ1900.active)
        Q1900CancelPending("q19-cell-streaming-takeover");
    gExteriorStreamBusyQ1890 = false;

    Q6H_LOGI("Q19 CELL STREAM RESET: context=%llu worldspace=%08X persistent=%08X bootCentre=(%d,%d) actual=(%d,%d) seededCells=25 worker=serialized cpuOffThread=1 textureDecodeOffThread=1 cellCommit=1 cancelOnMovement=0",
             static_cast<unsigned long long>(gQ1900ContextSerialQ19),
             gQ1900WorldspaceQ19, gQ1900PersistentQ19,
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             actualGridX, actualGridY);
}

bool Q1900ContextChangedQ19() {
    return !gQ1900ContextReadyQ19 ||
        gQ1900WorldspaceQ19 != gExteriorWorldspaceQ1890 ||
        gQ1900PersistentQ19 != gExteriorPersistentCellQ1890 ||
        std::fabs(gQ1900OriginXQ19 - gExteriorOriginXQ1890) > 0.01f ||
        std::fabs(gQ1900OriginYQ19 - gExteriorOriginYQ1890) > 0.01f ||
        std::fabs(gQ1900OriginZQ19 - gExteriorOriginZQ1890) > 0.01f;
}

void Q1900MarkWantedQ19(int32_t x, int32_t y, bool prefetch) {
    Q1900CellStateQ19& state = Q1900EnsureCellStateQ19(x, y);
    state.wanted = true;
    state.prefetched = state.prefetched || prefetch;
    state.lastWantedTick = gQ1900TickQ19;
    if (state.stage == Q1900CellStageQ19::Failed)
        state.stage = Q1900CellStageQ19::Requested;
    // Seeded ready cells still need exact metadata/collision chunks eventually.
    if (!state.metadataReady && state.stage == Q1900CellStageQ19::Ready)
        state.stage = Q1900CellStageQ19::Requested;
}

void Q1900EnsureWantedCellsQ19(float gameX, float gameY,
                               int32_t actualGridX, int32_t actualGridY) {
    ++gQ1900TickQ19;
    for (auto& entry : gQ1900CellsQ19) entry.second.wanted = false;

    for (int dy = -Q1900_RESIDENT_RADIUS_Q19;
         dy <= Q1900_RESIDENT_RADIUS_Q19; ++dy) {
        for (int dx = -Q1900_RESIDENT_RADIUS_Q19;
             dx <= Q1900_RESIDENT_RADIUS_Q19; ++dx) {
            Q1900MarkWantedQ19(actualGridX + dx, actualGridY + dy, false);
        }
    }

    const float localX =
        gameX - static_cast<float>(actualGridX) * Q1890_EXTERIOR_CELL_SIZE;
    const float localY =
        gameY - static_cast<float>(actualGridY) * Q1890_EXTERIOR_CELL_SIZE;
    const float farEdge =
        Q1890_EXTERIOR_CELL_SIZE - Q1900_PREFETCH_EDGE_UNITS_Q19;

    auto prefetchVertical = [&](int32_t x) {
        for (int dy = -Q1900_RESIDENT_RADIUS_Q19;
             dy <= Q1900_RESIDENT_RADIUS_Q19; ++dy)
            Q1900MarkWantedQ19(x, actualGridY + dy, true);
    };
    auto prefetchHorizontal = [&](int32_t y) {
        for (int dx = -Q1900_RESIDENT_RADIUS_Q19;
             dx <= Q1900_RESIDENT_RADIUS_Q19; ++dx)
            Q1900MarkWantedQ19(actualGridX + dx, y, true);
    };

    if (localX >= farEdge)
        prefetchVertical(actualGridX + Q1900_RESIDENT_RADIUS_Q19 + 1);
    else if (localX <= Q1900_PREFETCH_EDGE_UNITS_Q19)
        prefetchVertical(actualGridX - Q1900_RESIDENT_RADIUS_Q19 - 1);

    if (localY >= farEdge)
        prefetchHorizontal(actualGridY + Q1900_RESIDENT_RADIUS_Q19 + 1);
    else if (localY <= Q1900_PREFETCH_EDGE_UNITS_Q19)
        prefetchHorizontal(actualGridY - Q1900_RESIDENT_RADIUS_Q19 - 1);
}

int Q1900CellPriorityQ19(const Q1900CellStateQ19& state,
                         int32_t actualGridX, int32_t actualGridY) {
    const int dx = std::abs(state.gridX - actualGridX);
    const int dy = std::abs(state.gridY - actualGridY);
    const int cheb = std::max(dx, dy);
    if (!state.visualReady) {
        if (cheb == 0) return 0;
        if (cheb == 1) return 10 + dx + dy;
        if (cheb == 2) return 30 + dx + dy;
        return 60 + cheb;
    }
    if (!state.metadataReady) {
        if (cheb <= 1) return 100 + dx + dy;
        if (cheb <= 2) return 120 + dx + dy;
        return 160 + cheb;
    }
    return 1000000;
}

void Q1900AddTextureRequestQ19(
        std::vector<Q1900TextureRequestQ19>& requests,
        std::unordered_set<std::string>& seen,
        const std::string& path, const char* label,
        std::array<uint8_t,4> fallback) {
    if (path.empty()) return;
    const std::string key = TextureCacheKey(path, "");
    if (!seen.insert(key).second) return;
    Q1900TextureRequestQ19 request;
    request.path = path;
    request.label = label;
    request.fallback = fallback;
    requests.push_back(std::move(request));
}

void Q1900RunCellWorkerQ19(
        const std::shared_ptr<Q1900CellWorkerTaskQ19>& task) {
    const auto metadataStarted = std::chrono::steady_clock::now();
    std::vector<Fo3WorldPlacement> neighborhood;
    SetFo3WorldspaceGridRadiusOverrideQ1950(0);
    const float selectionX =
        (static_cast<float>(task->gridX) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
    const float selectionY =
        (static_cast<float>(task->gridY) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
    const bool loaded = LoadFo3WorldspaceNeighborhoodQ75(
        task->worldspace, task->persistentCell,
        selectionX, selectionY, neighborhood);
    SetFo3WorldspaceGridRadiusOverrideQ1950(-1);
    task->metadataUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - metadataStarted).count());

    if (!loaded) {
        task->success = false;
        task->ready.store(true, std::memory_order_release);
        return;
    }

    std::unordered_set<uint32_t> seenRefs;
    for (const Fo3WorldPlacement& placement : neighborhood) {
        const int32_t gridX = placement.hasExteriorGrid
            ? placement.gridX
            : static_cast<int32_t>(
                std::floor(placement.x / Q1890_EXTERIOR_CELL_SIZE));
        const int32_t gridY = placement.hasExteriorGrid
            ? placement.gridY
            : static_cast<int32_t>(
                std::floor(placement.y / Q1890_EXTERIOR_CELL_SIZE));
        if (gridX != task->gridX || gridY != task->gridY) continue;
        if (placement.refFormId != 0u &&
            !seenRefs.insert(placement.refFormId).second) continue;
        task->placements.push_back(placement);
    }

    if (task->needVisual) {
        const auto cpuStarted = std::chrono::steady_clock::now();
        std::unordered_set<std::string> textureSeen;
        for (const Fo3WorldPlacement& placement : task->placements) {
            if (Q74ShouldSkipPlacement(placement)) {
                ++task->skipped;
                continue;
            }
            std::vector<CpuObject> parts;
            if (!BuildCpuObjects(placement, parts)) {
                ++task->unsupported;
                continue;
            }
            for (CpuObject& part : parts) {
                Q1900AddTextureRequestQ19(
                    task->textures, textureSeen,
                    part.mesh.diffuseTexturePath, "DIFFUSE",
                    {190u,170u,135u,255u});
                Q1900AddTextureRequestQ19(
                    task->textures, textureSeen,
                    part.mesh.normalTexturePath, "NORMAL",
                    {128u,128u,255u,0u});
                Q1900AddTextureRequestQ19(
                    task->textures, textureSeen,
                    part.mesh.glowTexturePath, "GLOW",
                    {0u,0u,0u,255u});
                if (!PrepareExpandedVertexStreamQ1960(
                        part,
                        task->originX, task->originY, task->originZ)) {
                    ++task->unsupported;
                    continue;
                }
                task->cpu.push_back(std::move(part));
            }
        }
        task->cpuUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - cpuStarted).count());

        const auto textureStarted = std::chrono::steady_clock::now();
        for (const Q1900TextureRequestQ19& request : task->textures)
            Q1900PrepareTextureCpuQ19(request.path);
        task->textureUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - textureStarted).count());
    }

    // Prime transformed authored collision while still off the render thread.
    // The scheduler does not publish/read the placement cache while this worker
    // is running, so the legacy cache remains single-writer.
    const auto collisionStarted = std::chrono::steady_clock::now();
    for (const Fo3WorldPlacement& placement : task->placements) {
        if (placement.refFormId == 0u || Q74ShouldSkipPlacement(placement))
            continue;
        const auto itemStarted = std::chrono::steady_clock::now();
        size_t triangles = 0u;
        if (PrimeFo3CollisionPlacementCacheQ1820(
                placement,
                task->originX, task->originY, task->originZ,
                SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE,
                &triangles)) {
            ++task->collisionPrimedRefs;
            task->collisionTriangles += triangles;
        }
        const uint64_t itemUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - itemStarted).count());
        task->maxCollisionItemUs =
            std::max(task->maxCollisionItemUs, itemUs);
    }
    task->collisionUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - collisionStarted).count());

    task->success = true;
    task->ready.store(true, std::memory_order_release);
}

void Q1900ConsumeWorkerQ19() {
    if (!gQ1900WorkerQ19 ||
        !gQ1900WorkerQ19->ready.load(std::memory_order_acquire)) return;

    const auto task = gQ1900WorkerQ19;
    gQ1900WorkerQ19.reset();
    if (task->contextSerial != gQ1900ContextSerialQ19 ||
        task->worldspace != gExteriorWorldspaceQ1890 ||
        task->persistentCell != gExteriorPersistentCellQ1890) {
        Q6H_LOGW("Q19 WORKER DROP: cell=(%d,%d) workerContext=%llu liveContext=%llu reason=context-changed",
                 task->gridX, task->gridY,
                 static_cast<unsigned long long>(task->contextSerial),
                 static_cast<unsigned long long>(gQ1900ContextSerialQ19));
        return;
    }

    auto found = gQ1900CellsQ19.find(
        Q1900CellKeyQ19(task->gridX, task->gridY));
    if (found == gQ1900CellsQ19.end()) return;
    Q1900CellStateQ19& state = found->second;

    if (!task->success) {
        state.stage = Q1900CellStageQ19::Failed;
        Q6H_LOGE("Q19 WORKER FAILED: cell=(%d,%d) metadataUs=%llu",
                 state.gridX, state.gridY,
                 static_cast<unsigned long long>(task->metadataUs));
        return;
    }

    state.placements = std::move(task->placements);
    state.metadataReady = true;
    state.collisionPrimed = true;
    state.skipped = task->skipped;
    state.unsupported = task->unsupported;
    state.workerUs = task->metadataUs + task->cpuUs +
                     task->textureUs + task->collisionUs;

    if (task->needVisual) {
        state.cpu = std::move(task->cpu);
        state.textures = std::move(task->textures);
        state.textureCursor = 0u;
        state.gpuCursor = 0u;
        state.stagedGpu.clear();
        state.stagedGpu.reserve(state.cpu.size());
        state.stage = Q1900CellStageQ19::Uploading;
    } else {
        state.stage = Q1900CellStageQ19::Ready;
    }

    Q6H_LOGI("Q19 WORKER READY: cell=(%d,%d) needVisual=%d placements=%zu cpuShapes=%zu textures=%zu skipped=%zu unsupported=%zu metadataUs=%llu cpuUs=%llu textureUs=%llu collisionUs=%llu collisionRefs=%zu collisionTriangles=%zu maxCollisionItemUs=%llu",
             state.gridX, state.gridY, task->needVisual ? 1 : 0,
             state.placements.size(), state.cpu.size(), state.textures.size(),
             state.skipped, state.unsupported,
             static_cast<unsigned long long>(task->metadataUs),
             static_cast<unsigned long long>(task->cpuUs),
             static_cast<unsigned long long>(task->textureUs),
             static_cast<unsigned long long>(task->collisionUs),
             task->collisionPrimedRefs, task->collisionTriangles,
             static_cast<unsigned long long>(task->maxCollisionItemUs));
}

void Q1900LaunchWorkerQ19(int32_t actualGridX, int32_t actualGridY) {
    if (gQ1900WorkerQ19 || gQ1930CollisionTaskQ19 ||
        gQ1970LodWorkerQ19) return;

    Q1900CellStateQ19* best = nullptr;
    int bestPriority = 1000000;
    for (auto& entry : gQ1900CellsQ19) {
        Q1900CellStateQ19& state = entry.second;
        if (state.stage != Q1900CellStageQ19::Requested) continue;
        if (!state.wanted && state.visualReady) continue;
        const int priority =
            Q1900CellPriorityQ19(state, actualGridX, actualGridY);
        if (!best || priority < bestPriority) {
            best = &state;
            bestPriority = priority;
        }
    }
    if (!best || bestPriority >= 1000000) return;

    auto task = std::make_shared<Q1900CellWorkerTaskQ19>();
    task->contextSerial = gQ1900ContextSerialQ19;
    task->worldspace = gExteriorWorldspaceQ1890;
    task->persistentCell = gExteriorPersistentCellQ1890;
    task->gridX = best->gridX;
    task->gridY = best->gridY;
    task->originX = gExteriorOriginXQ1890;
    task->originY = gExteriorOriginYQ1890;
    task->originZ = gExteriorOriginZQ1890;
    task->needVisual = !best->visualReady;

    best->stage = Q1900CellStageQ19::Worker;
    gQ1900WorkerQ19 = task;
    Q6H_LOGI("Q19 WORKER START: cell=(%d,%d) needVisual=%d priority=%d actual=(%d,%d) noCancelOnMovement=1",
             task->gridX, task->gridY, task->needVisual ? 1 : 0,
             bestPriority, actualGridX, actualGridY);
    std::thread([task]() { Q1900RunCellWorkerQ19(task); }).detach();
}

enum class Q1960UploadStepQ19 : uint8_t {
    Pending = 0,
    Complete,
    Failed,
};

uint64_t Q1960ElapsedUsQ19(
        const std::chrono::steady_clock::time_point& started) {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count());
}

void Q1960RecordStepQ19(uint64_t stepUs, uint64_t& totalUs,
                        uint64_t& maxStepUs) {
    totalUs += stepUs;
    maxStepUs = std::max(maxStepUs, stepUs);
}

Q1960UploadStepQ19 Q1960AdvanceTextureQ19(
        Q1900CellStateQ19& state, size_t& bytesThisFrame) {
    Q1960TextureUploadQ19& upload = state.q1960TextureUpload;

    if (!upload.active) {
        if (state.textureCursor >= state.textures.size())
            return Q1960UploadStepQ19::Complete;

        const Q1900TextureRequestQ19& request =
            state.textures[state.textureCursor];
        const std::string cacheKey =
            TextureCacheKey(request.path, request.label.c_str());
        const auto cached = gTextureCache.find(cacheKey);
        if (cached != gTextureCache.end()) {
            Q1900MarkGpuTextureKnownQ19(cacheKey);
            ++state.textureCursor;
            return Q1960UploadStepQ19::Complete;
        }

        const auto started = std::chrono::steady_clock::now();
        Fo3RgbaTexture texture;
        bool real = false;
        const bool prepared = Q1900TakePreparedTextureQ19(
            TextureCacheKey(request.path, ""), texture, real);
        if (!prepared) {
            // This should be unreachable because one active staged upload owns
            // the uploader until its cache entry is published. Keep a
            // correctness fallback rather than leaving the CELL permanently
            // stuck if an unexpected cache/preparation mismatch occurs.
            GLuint id = 0u;
            std::vector<uint8_t> fallback{
                request.fallback[0], request.fallback[1],
                request.fallback[2], request.fallback[3]};
            const bool ok = UploadTexture(
                request.path, fallback, id, real,
                request.label.c_str(), 0u);
            const uint64_t stepUs = Q1960ElapsedUsQ19(started);
            state.gpuUs += stepUs;
            state.maxTextureGpuUs =
                std::max(state.maxTextureGpuUs, stepUs);
            Q6H_LOGW("Q19.6 TEXTURE PREP MISS: cell=(%d,%d) path=%s fallbackSynchronous=%d itemUs=%llu",
                     state.gridX, state.gridY, request.path.c_str(),
                     ok ? 1 : 0,
                     static_cast<unsigned long long>(stepUs));
            ++state.textureCursor;
            return ok ? Q1960UploadStepQ19::Complete
                      : Q1960UploadStepQ19::Failed;
        }

        if (!real) {
            texture.width = 1;
            texture.height = 1;
            texture.rgba.assign(request.fallback.begin(),
                                request.fallback.end());
            texture.sourcePath = "<fallback>";
            texture.format = "fallback";
        }

        upload = {};
        upload.active = true;
        upload.cacheKey = cacheKey;
        upload.path = request.path;
        upload.label = request.label;
        upload.texture = std::move(texture);
        upload.real = real;

        glGenTextures(1, &upload.id);
        glBindTexture(GL_TEXTURE_2D, upload.id);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        // The texture is not published until complete, so use non-mip
        // filtering during construction and restore authored filtering later.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        Q1070ApplyStaticAnisotropy();
        const GLenum internalFormat =
            upload.label == "DIFFUSE" ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                     upload.texture.width, upload.texture.height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        const uint64_t stepUs = Q1960ElapsedUsQ19(started);
        Q1960RecordStepQ19(
            stepUs, upload.totalUs, upload.maxStepUs);
        state.gpuUs += stepUs;
        state.maxTextureGpuUs =
            std::max(state.maxTextureGpuUs, stepUs);

        if (glGetError() != GL_NO_ERROR || upload.id == 0u) {
            if (upload.id) glDeleteTextures(1, &upload.id);
            upload = {};
            Q6H_LOGE("Q19.6 TEXTURE STAGE FAILED: cell=(%d,%d) phase=allocate path=%s",
                     state.gridX, state.gridY, request.path.c_str());
            ++state.textureCursor;
            return Q1960UploadStepQ19::Failed;
        }
        if (stepUs >= Q1910_GPU_ITEM_WARN_US) {
            Q6H_LOGW("Q19.6 GPU STAGE SPIKE: phase=texture-allocate cell=(%d,%d) itemUs=%llu path=%s size=%dx%d",
                     state.gridX, state.gridY,
                     static_cast<unsigned long long>(stepUs),
                     upload.path.c_str(),
                     upload.texture.width, upload.texture.height);
        }
        return Q1960UploadStepQ19::Pending;
    }

    if (upload.nextRow <
        static_cast<size_t>(std::max(upload.texture.height, 0))) {
        const size_t rowBytes =
            static_cast<size_t>(upload.texture.width) * 4u;
        if (rowBytes == 0u) return Q1960UploadStepQ19::Failed;
        const size_t rowsByBytes =
            std::max<size_t>(1u, Q1960_TEXTURE_CHUNK_BYTES / rowBytes);
        const size_t remainingRows =
            static_cast<size_t>(upload.texture.height) - upload.nextRow;
        const size_t rows = std::min(rowsByBytes, remainingRows);
        const size_t bytes = rows * rowBytes;

        const auto started = std::chrono::steady_clock::now();
        glBindTexture(GL_TEXTURE_2D, upload.id);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(
            GL_TEXTURE_2D, 0,
            0, static_cast<GLint>(upload.nextRow),
            upload.texture.width, static_cast<GLsizei>(rows),
            GL_RGBA, GL_UNSIGNED_BYTE,
            upload.texture.rgba.data() + upload.nextRow * rowBytes);
        glBindTexture(GL_TEXTURE_2D, 0);
        const uint64_t stepUs = Q1960ElapsedUsQ19(started);

        Q1960RecordStepQ19(
            stepUs, upload.totalUs, upload.maxStepUs);
        state.gpuUs += stepUs;
        state.maxTextureGpuUs =
            std::max(state.maxTextureGpuUs, stepUs);
        upload.nextRow += rows;
        ++upload.chunks;
        bytesThisFrame += bytes;

        if (stepUs >= Q1910_GPU_ITEM_WARN_US) {
            Q6H_LOGW("Q19.6 GPU STAGE SPIKE: phase=texture-chunk cell=(%d,%d) itemUs=%llu bytes=%zu path=%s row=%zu/%d",
                     state.gridX, state.gridY,
                     static_cast<unsigned long long>(stepUs),
                     bytes, upload.path.c_str(),
                     upload.nextRow, upload.texture.height);
        }
        return Q1960UploadStepQ19::Pending;
    }

    // Mipmap generation remains one driver operation. Keeping it separate from
    // the final pixel chunk lets telemetry prove whether it is the next stall.
    const auto started = std::chrono::steady_clock::now();
    glBindTexture(GL_TEXTURE_2D, upload.id);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    const uint64_t stepUs = Q1960ElapsedUsQ19(started);
    Q1960RecordStepQ19(stepUs, upload.totalUs, upload.maxStepUs);
    state.gpuUs += stepUs;
    state.maxTextureGpuUs =
        std::max(state.maxTextureGpuUs, stepUs);

    const bool ok = glGetError() == GL_NO_ERROR;
    if (ok) {
        gTextureCache.emplace(
            upload.cacheKey,
            CachedGpuTexture{upload.id, upload.real});
        Q1900MarkGpuTextureKnownQ19(upload.cacheKey);
        Q6H_LOGI("Q19.6 TEXTURE READY: cell=(%d,%d) path=%s size=%dx%d chunks=%zu totalUs=%llu maxStepUs=%llu stagedRows=1 mipFinalizeUs=%llu",
                 state.gridX, state.gridY, upload.path.c_str(),
                 upload.texture.width, upload.texture.height,
                 upload.chunks,
                 static_cast<unsigned long long>(upload.totalUs),
                 static_cast<unsigned long long>(upload.maxStepUs),
                 static_cast<unsigned long long>(stepUs));
        upload.id = 0u; // cache now owns the GL texture.
    } else {
        if (upload.id) glDeleteTextures(1, &upload.id);
        Q6H_LOGE("Q19.6 TEXTURE STAGE FAILED: cell=(%d,%d) phase=mipmap path=%s",
                 state.gridX, state.gridY, upload.path.c_str());
    }
    if (stepUs >= Q1910_GPU_ITEM_WARN_US) {
        Q6H_LOGW("Q19.6 GPU STAGE SPIKE: phase=texture-mipmap cell=(%d,%d) itemUs=%llu path=%s",
                 state.gridX, state.gridY,
                 static_cast<unsigned long long>(stepUs),
                 upload.path.c_str());
    }

    upload = {};
    ++state.textureCursor;
    return ok ? Q1960UploadStepQ19::Complete
              : Q1960UploadStepQ19::Failed;
}

Q1960UploadStepQ19 Q1960AdvanceShapeQ19(
        Q1900CellStateQ19& state, size_t& bytesThisFrame) {
    Q1960ShapeUploadQ19& upload = state.q1960ShapeUpload;

    if (!upload.active) {
        if (state.gpuCursor >= state.cpu.size())
            return Q1960UploadStepQ19::Complete;

        CpuObject& cpu = state.cpu[state.gpuCursor];
        const auto started = std::chrono::steady_clock::now();
        upload = {};
        gQ1960CaptureVboQ19 = true;
        gQ1960CapturedVboVerticesQ19 = &upload.vertices;
        const bool ok = UploadCpuObject(
            cpu,
            gExteriorOriginXQ1890,
            gExteriorOriginYQ1890,
            gExteriorOriginZQ1890,
            upload.gpu);
        gQ1960CaptureVboQ19 = false;
        gQ1960CapturedVboVerticesQ19 = nullptr;
        const uint64_t stepUs = Q1960ElapsedUsQ19(started);

        state.gpuUs += stepUs;
        state.maxShapeGpuUs =
            std::max(state.maxShapeGpuUs, stepUs);
        Q1960RecordStepQ19(
            stepUs, upload.totalUs, upload.maxStepUs);

        if (!ok || upload.vertices.empty() || upload.gpu.vbo == 0u) {
            Q1900DeleteGpuShape(upload.gpu);
            const std::string model = cpu.placement.modelPath;
            cpu = CpuObject{};
            upload = {};
            ++state.gpuCursor;
            Q6H_LOGE("Q19.6 VBO STAGE FAILED: cell=(%d,%d) phase=allocate model=%s",
                     state.gridX, state.gridY, model.c_str());
            return Q1960UploadStepQ19::Failed;
        }

        upload.active = true;
        // All material/texture metadata is now captured. The heavy vertex
        // stream itself remains CPU-side until the chunked sub-data steps.
        cpu = CpuObject{};

        if (stepUs >= Q1910_GPU_ITEM_WARN_US) {
            Q6H_LOGW("Q19.6 GPU STAGE SPIKE: phase=vbo-allocate cell=(%d,%d) itemUs=%llu bytes=%zu model=%s",
                     state.gridX, state.gridY,
                     static_cast<unsigned long long>(stepUs),
                     upload.vertices.size() * sizeof(float),
                     upload.gpu.modelPath.c_str());
        }
        return Q1960UploadStepQ19::Pending;
    }

    const size_t totalBytes = upload.vertices.size() * sizeof(float);
    if (upload.nextByte < totalBytes) {
        const size_t bytes =
            std::min(Q1960_VBO_CHUNK_BYTES,
                     totalBytes - upload.nextByte);
        const auto started = std::chrono::steady_clock::now();
        glBindBuffer(GL_ARRAY_BUFFER, upload.gpu.vbo);
        glBufferSubData(
            GL_ARRAY_BUFFER,
            static_cast<GLintptr>(upload.nextByte),
            static_cast<GLsizeiptr>(bytes),
            reinterpret_cast<const uint8_t*>(upload.vertices.data()) +
                upload.nextByte);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        const uint64_t stepUs = Q1960ElapsedUsQ19(started);

        Q1960RecordStepQ19(
            stepUs, upload.totalUs, upload.maxStepUs);
        state.gpuUs += stepUs;
        state.maxShapeGpuUs =
            std::max(state.maxShapeGpuUs, stepUs);
        upload.nextByte += bytes;
        ++upload.chunks;
        bytesThisFrame += bytes;

        if (stepUs >= Q1910_GPU_ITEM_WARN_US) {
            Q6H_LOGW("Q19.6 GPU STAGE SPIKE: phase=vbo-chunk cell=(%d,%d) itemUs=%llu bytes=%zu uploaded=%zu/%zu model=%s",
                     state.gridX, state.gridY,
                     static_cast<unsigned long long>(stepUs),
                     bytes, upload.nextByte, totalBytes,
                     upload.gpu.modelPath.c_str());
        }
        return Q1960UploadStepQ19::Pending;
    }

    const auto started = std::chrono::steady_clock::now();
    const GLenum error = glGetError();
    const uint64_t stepUs = Q1960ElapsedUsQ19(started);
    Q1960RecordStepQ19(
        stepUs, upload.totalUs, upload.maxStepUs);
    state.gpuUs += stepUs;
    state.maxShapeGpuUs =
        std::max(state.maxShapeGpuUs, stepUs);

    if (error != GL_NO_ERROR) {
        Q6H_LOGE("Q19.6 VBO STAGE FAILED: cell=(%d,%d) phase=finalize glError=0x%X model=%s",
                 state.gridX, state.gridY, error,
                 upload.gpu.modelPath.c_str());
        Q1900DeleteGpuShape(upload.gpu);
        upload = {};
        ++state.gpuCursor;
        return Q1960UploadStepQ19::Failed;
    }

    Q6H_LOGI("Q19.6 VBO READY: cell=(%d,%d) model=%s bytes=%zu chunks=%zu totalUs=%llu maxStepUs=%llu workerExpanded=1",
             state.gridX, state.gridY,
             upload.gpu.modelPath.c_str(), totalBytes,
             upload.chunks,
             static_cast<unsigned long long>(upload.totalUs),
             static_cast<unsigned long long>(upload.maxStepUs));
    state.stagedGpu.push_back(std::move(upload.gpu));
    upload = {};
    ++state.gpuCursor;
    return Q1960UploadStepQ19::Complete;
}

Q1900CellStateQ19* Q1900SelectUploadingCellQ19(
        int32_t actualGridX, int32_t actualGridY) {
    for (auto& entry : gQ1900CellsQ19) {
        Q1900CellStateQ19& state = entry.second;
        if (state.stage == Q1900CellStageQ19::Uploading &&
            (state.q1960TextureUpload.active ||
             state.q1960ShapeUpload.active)) {
            return &state;
        }
    }

    Q1900CellStateQ19* best = nullptr;
    int bestPriority = 1000000;
    for (auto& entry : gQ1900CellsQ19) {
        Q1900CellStateQ19& state = entry.second;
        if (state.stage != Q1900CellStageQ19::Uploading) continue;
        const int priority =
            Q1900CellPriorityQ19(state, actualGridX, actualGridY);
        if (!best || priority < bestPriority) {
            best = &state;
            bestPriority = priority;
        }
    }
    return best;
}

void Q1900CommitCellVisualsQ19(Q1900CellStateQ19& state) {
    const auto retireStarted = std::chrono::steady_clock::now();
    size_t retired = 0u;

    size_t write = 0u;
    const size_t oldLive = gObjects.size();
    for (size_t read = 0u; read < oldLive; ++read) {
        GpuObject& object = gObjects[read];
        const bool replace =
            !object.q1990NativeLod &&
            object.q1970GridX == state.gridX &&
            object.q1970GridY == state.gridY;
        if (replace) {
            Q1910QueueGpuDeleteQ19(object);
            ++retired;
            continue;
        }
        if (write != read) {
            gObjects[write] = std::move(object);
            object.vbo = 0u;
            object.vao = 0u;
        }
        ++write;
    }
    gObjects.resize(write);
    const uint64_t retireQueueUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - retireStarted).count());

    const size_t entering = state.stagedGpu.size();
    for (GpuObject& object : state.stagedGpu) {
        gObjects.push_back(std::move(object));
        object.vbo = 0u;
        object.vao = 0u;
    }
    state.stagedGpu.clear();
    state.cpu.clear();
    state.textures.clear();
    state.visualReady = true;
    state.stage = Q1900CellStageQ19::Ready;
    gSceneReady = !gObjects.empty();
    gLoggedFirstDraw = false;
    gShadowDirtyQ1050 = true;

    const uint64_t totalUs = state.requestedAt.time_since_epoch().count() == 0
        ? 0u
        : static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - state.requestedAt).count());

    Q6H_LOGI("Q19.6 CELL READY: cell=(%d,%d) enteringShapes=%zu replacedShapes=%zu liveShapes=%zu deferredDeletes=%zu placements=%zu workerUs=%llu gpuUs=%llu retireQueueUs=%llu maxTextureStepUs=%llu maxShapeStepUs=%llu totalUs=%llu uploadBytesPerFrame=%zu lodHandoff=cell-specific",
             state.gridX, state.gridY, entering, retired, gObjects.size(),
             gQ1910DeferredGpuDeletesQ19.size(), state.placements.size(),
             static_cast<unsigned long long>(state.workerUs),
             static_cast<unsigned long long>(state.gpuUs),
             static_cast<unsigned long long>(retireQueueUs),
             static_cast<unsigned long long>(state.maxTextureGpuUs),
             static_cast<unsigned long long>(state.maxShapeGpuUs),
             static_cast<unsigned long long>(totalUs),
             Q1960_GPU_BYTES_PER_FRAME);
}

void Q1900AdvanceGpuQ19(int32_t actualGridX, int32_t actualGridY) {
    Q1900CellStateQ19* state =
        Q1900SelectUploadingCellQ19(actualGridX, actualGridY);
    if (!state) return;

    const auto frameStarted = std::chrono::steady_clock::now();
    size_t bytesThisFrame = 0u;
    bool didWork = false;

    while (state->textureCursor < state->textures.size()) {
        const Q1960UploadStepQ19 step =
            Q1960AdvanceTextureQ19(*state, bytesThisFrame);
        didWork = true;
        if (step == Q1960UploadStepQ19::Failed) {
            // Keep progressing; UploadCpuObject will use fallback/cache state.
        }
        if (bytesThisFrame >= Q1960_GPU_BYTES_PER_FRAME ||
            Q1960ElapsedUsQ19(frameStarted) >= Q1900_GPU_BUDGET_US_Q19) {
            return;
        }
        if (state->q1960TextureUpload.active) {
            // Continue the same texture only while both budgets allow.
            continue;
        }
    }

    while (state->gpuCursor < state->cpu.size()) {
        const Q1960UploadStepQ19 step =
            Q1960AdvanceShapeQ19(*state, bytesThisFrame);
        didWork = true;
        if (step == Q1960UploadStepQ19::Failed) {
            // Failed shape is skipped exactly like the legacy upload path.
        }
        if (bytesThisFrame >= Q1960_GPU_BYTES_PER_FRAME ||
            Q1960ElapsedUsQ19(frameStarted) >= Q1900_GPU_BUDGET_US_Q19) {
            return;
        }
        if (state->q1960ShapeUpload.active) {
            continue;
        }
    }

    if (state->textureCursor >= state->textures.size() &&
        state->gpuCursor >= state->cpu.size() &&
        !state->q1960TextureUpload.active &&
        !state->q1960ShapeUpload.active) {
        Q1900CommitCellVisualsQ19(*state);
    } else if (didWork) {
        Q6H_LOGI("Q19.6 CELL GPU: cell=(%d,%d) textures=%zu/%zu shapes=%zu/%zu staged=%zu frameBytes=%zu byteBudget=%zu timeBudgetUs=%llu textureActive=%d shapeActive=%d",
                 state->gridX, state->gridY,
                 state->textureCursor, state->textures.size(),
                 state->gpuCursor, state->cpu.size(),
                 state->stagedGpu.size(),
                 bytesThisFrame, Q1960_GPU_BYTES_PER_FRAME,
                 static_cast<unsigned long long>(Q1900_GPU_BUDGET_US_Q19),
                 state->q1960TextureUpload.active ? 1 : 0,
                 state->q1960ShapeUpload.active ? 1 : 0);
    }
}


// -----------------------------------------------------------------------------
// Q19.7: staged native Level4 LOD.
// CPU extraction/scenegraph/texture decode shares the serialized asset lane with
// CELL/collision work. GPU upload reuses Q19.6's resumable texture/VBO stages.
// -----------------------------------------------------------------------------
struct Q1970LodWorkerTaskQ19 {
    uint64_t contextSerial = 0u;
    int32_t blockX = 0;
    int32_t blockY = 0;
    int ring = 0;
    float centerX = 0.0f;
    float centerY = 0.0f;
    float floorZ = 0.0f;
    std::vector<CpuObject> cpu;
    std::vector<Q1900TextureRequestQ19> textures;
    size_t terrainShapes = 0u;
    size_t objectShapes = 0u;
    size_t terrainTriangles = 0u;
    size_t objectTriangles = 0u;
    uint64_t workerUs = 0u;
    bool success = false;
    std::atomic<bool> ready{false};
};

std::shared_ptr<Q1970LodWorkerTaskQ19> gQ1970LodWorkerQ19;
std::unique_ptr<Q1900CellStateQ19> gQ1970LodUploadQ19;
int32_t gQ1970LodUploadBlockXQ19 = 0;
int32_t gQ1970LodUploadBlockYQ19 = 0;
int gQ1970LodUploadRingQ19 = 0;
uint64_t gQ1970LodUploadWorkerUsQ19 = 0u;

bool Q1970LodBlockPendingQ19(int32_t blockX, int32_t blockY) {
    if (gQ1970LodWorkerQ19 &&
        gQ1970LodWorkerQ19->blockX == blockX &&
        gQ1970LodWorkerQ19->blockY == blockY) return true;
    return gQ1970LodUploadQ19 &&
        gQ1970LodUploadBlockXQ19 == blockX &&
        gQ1970LodUploadBlockYQ19 == blockY;
}

bool Q1970NearDetailSafeQ19(int32_t actualGridX, int32_t actualGridY) {
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const auto found = gQ1900CellsQ19.find(
                Q1900CellKeyQ19(actualGridX + dx, actualGridY + dy));
            if (found == gQ1900CellsQ19.end() ||
                !found->second.visualReady) {
                return false;
            }
        }
    }
    return true;
}

bool Q1970AppendLodNifCpuQ19(
        const std::string& path,
        const std::shared_ptr<Q1970LodWorkerTaskQ19>& task,
        bool terrain,
        std::unordered_set<std::string>& textureSeen) {
    std::vector<Fo3StaticNifMesh> meshes;
    if (!LoadFo3StaticNifMeshes(path, meshes) || meshes.empty())
        return false;

    bool any = false;
    for (Fo3StaticNifMesh& mesh : meshes) {
        const size_t vertexCount = mesh.positions.size() / 3u;
        if (vertexCount == 0u || mesh.indices.empty() ||
            mesh.normals.size() / 3u != vertexCount ||
            mesh.tangents.size() / 3u != vertexCount ||
            mesh.bitangents.size() / 3u != vertexCount ||
            mesh.texcoords.size() / 2u != vertexCount) {
            continue;
        }

        CpuObject cpu;
        cpu.placement.refFormId = 0u;
        cpu.placement.baseFormId = 0u;
        cpu.placement.scale = 1.0f;
        cpu.placement.modelPath = path;
        cpu.mesh = std::move(mesh);
        cpu.positionsGame.reserve(vertexCount);
        cpu.normalsGame.reserve(vertexCount);
        cpu.tangentsGame.reserve(vertexCount);
        cpu.bitangentsGame.reserve(vertexCount);
        for (size_t i = 0u; i < vertexCount; ++i) {
            cpu.positionsGame.push_back(Vec3{
                cpu.mesh.positions[i * 3u],
                cpu.mesh.positions[i * 3u + 1u],
                cpu.mesh.positions[i * 3u + 2u]});
            cpu.normalsGame.push_back(Vec3{
                cpu.mesh.normals[i * 3u],
                cpu.mesh.normals[i * 3u + 1u],
                cpu.mesh.normals[i * 3u + 2u]});
            cpu.tangentsGame.push_back(Vec3{
                cpu.mesh.tangents[i * 3u],
                cpu.mesh.tangents[i * 3u + 1u],
                cpu.mesh.tangents[i * 3u + 2u]});
            cpu.bitangentsGame.push_back(Vec3{
                cpu.mesh.bitangents[i * 3u],
                cpu.mesh.bitangents[i * 3u + 1u],
                cpu.mesh.bitangents[i * 3u + 2u]});
        }

        if (!PrepareExpandedVertexStreamQ1960(
                cpu, task->centerX, task->centerY, task->floorZ)) {
            continue;
        }

        Q1900AddTextureRequestQ19(
            task->textures, textureSeen,
            cpu.mesh.diffuseTexturePath, "DIFFUSE",
            {180u,180u,180u,255u});
        Q1900AddTextureRequestQ19(
            task->textures, textureSeen,
            cpu.mesh.normalTexturePath, "NORMAL",
            {128u,128u,255u,255u});
        Q1900AddTextureRequestQ19(
            task->textures, textureSeen,
            cpu.mesh.glowTexturePath, "GLOW",
            {0u,0u,0u,255u});

        const size_t triangles = cpu.mesh.indices.size() / 3u;
        if (terrain) {
            ++task->terrainShapes;
            task->terrainTriangles += triangles;
        } else {
            ++task->objectShapes;
            task->objectTriangles += triangles;
        }
        task->cpu.push_back(std::move(cpu));
        any = true;
    }
    return any;
}

void Q1970RunLodWorkerQ19(
        const std::shared_ptr<Q1970LodWorkerTaskQ19>& task) {
    const auto started = std::chrono::steady_clock::now();
    std::unordered_set<std::string> textureSeen;

    const std::string suffix =
        "Wasteland.Level4.X" + std::to_string(task->blockX) +
        ".Y" + std::to_string(task->blockY) + ".NIF";
    const std::string terrainPath =
        "Landscape\\LOD\\Wasteland\\" + suffix;
    const std::string objectPath =
        "Landscape\\LOD\\Wasteland\\Blocks\\" + suffix;

    const bool terrainReady =
        Q1970AppendLodNifCpuQ19(
            terrainPath, task, true, textureSeen);
    const bool objectsReady =
        Q1970AppendLodNifCpuQ19(
            objectPath, task, false, textureSeen);

    for (const Q1900TextureRequestQ19& request : task->textures) {
        Q1900PrepareTextureCpuQ19(request.path);
    }

    task->success = terrainReady || objectsReady || task->cpu.empty();
    task->workerUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count());
    task->ready.store(true, std::memory_order_release);
}

void Q1970ConsumeLodWorkerQ19() {
    if (!gQ1970LodWorkerQ19 ||
        !gQ1970LodWorkerQ19->ready.load(std::memory_order_acquire)) {
        return;
    }

    const auto task = gQ1970LodWorkerQ19;
    gQ1970LodWorkerQ19.reset();

    const bool stale =
        task->contextSerial != gQ1900ContextSerialQ19 ||
        std::fabs(task->centerX - gExteriorOriginXQ1890) > 0.01f ||
        std::fabs(task->centerY - gExteriorOriginYQ1890) > 0.01f ||
        std::fabs(task->floorZ - gExteriorOriginZQ1890) > 0.01f ||
        !Q1990LodBlockDesired(task->blockX, task->blockY);
    if (stale || Q1990FindLodBlock(task->blockX, task->blockY)) {
        Q6H_LOGI("Q19.7 LOD CPU STALE: block=(%d,%d) ring=%d workerUs=%llu action=discard",
                 task->blockX, task->blockY, task->ring,
                 static_cast<unsigned long long>(task->workerUs));
        return;
    }

    auto upload = std::make_unique<Q1900CellStateQ19>();
    upload->gridX = task->blockX;
    upload->gridY = task->blockY;
    upload->stage = Q1900CellStageQ19::Uploading;
    upload->textures = std::move(task->textures);
    upload->cpu = std::move(task->cpu);
    upload->placements.clear();
    gQ1970LodUploadBlockXQ19 = task->blockX;
    gQ1970LodUploadBlockYQ19 = task->blockY;
    gQ1970LodUploadRingQ19 = task->ring;
    gQ1970LodUploadWorkerUsQ19 = task->workerUs;
    gQ1970LodUploadQ19 = std::move(upload);

    Q6H_LOGI("Q19.7 LOD CPU READY: block=(%d,%d) ring=%d terrainShapes=%zu terrainTriangles=%zu objectShapes=%zu objectTriangles=%zu textures=%zu cpuShapes=%zu workerUs=%llu gpuDeferred=1",
             task->blockX, task->blockY, task->ring,
             task->terrainShapes, task->terrainTriangles,
             task->objectShapes, task->objectTriangles,
             gQ1970LodUploadQ19->textures.size(),
             gQ1970LodUploadQ19->cpu.size(),
             static_cast<unsigned long long>(task->workerUs));
}

void Q1970AdvanceLodGpuQ19() {
    if (!gQ1970LodUploadQ19) return;
    Q1900CellStateQ19& state = *gQ1970LodUploadQ19;

    const auto frameStarted = std::chrono::steady_clock::now();
    // Far LOD gets a deliberately smaller share than detailed CELL uploads.
    constexpr size_t Q1970_LOD_GPU_BYTES_PER_UPDATE = 128u * 1024u;
    constexpr uint64_t Q1970_LOD_GPU_BUDGET_US = 1200u;
    size_t bytesThisFrame = 0u;

    while (state.textureCursor < state.textures.size()) {
        Q1960AdvanceTextureQ19(state, bytesThisFrame);
        if (bytesThisFrame >= Q1970_LOD_GPU_BYTES_PER_UPDATE ||
            Q1960ElapsedUsQ19(frameStarted) >= Q1970_LOD_GPU_BUDGET_US ||
            state.q1960TextureUpload.active) {
            return;
        }
    }

    while (state.gpuCursor < state.cpu.size()) {
        Q1960AdvanceShapeQ19(state, bytesThisFrame);
        if (bytesThisFrame >= Q1970_LOD_GPU_BYTES_PER_UPDATE ||
            Q1960ElapsedUsQ19(frameStarted) >= Q1970_LOD_GPU_BUDGET_US ||
            state.q1960ShapeUpload.active) {
            return;
        }
    }

    if (state.textureCursor < state.textures.size() ||
        state.gpuCursor < state.cpu.size() ||
        state.q1960TextureUpload.active ||
        state.q1960ShapeUpload.active) {
        return;
    }

    Q1990NativeLodBlock block;
    block.blockX = gQ1970LodUploadBlockXQ19;
    block.blockY = gQ1970LodUploadBlockYQ19;
    block.lastUse = gQ1990NativeLodSerial;
    size_t terrainTriangles = 0u;
    size_t objectTriangles = 0u;

    for (GpuObject& gpu : state.stagedGpu) {
        gpu.q1990NativeLod = true;
        const std::string lower = TextureCacheKey(gpu.modelPath, "");
        const bool objectLod =
            lower.find("landscape\\lod\\wasteland\\blocks\\") !=
            std::string::npos;
        if (objectLod) {
            objectTriangles += static_cast<size_t>(gpu.vertexCount / 3);
            block.objects.push_back(std::move(gpu));
        } else {
            terrainTriangles += static_cast<size_t>(gpu.vertexCount / 3);
            block.terrain.push_back(std::move(gpu));
        }
        gpu.vbo = 0u;
        gpu.vao = 0u;
    }
    state.stagedGpu.clear();

    // Cache authored empty blocks as completed too.
    gQ1990NativeLodBlocks.push_back(std::move(block));
    Q6H_LOGI("Q19.7 LOD BLOCK READY: block=(%d,%d) ring=%d terrainShapes=%zu terrainTriangles=%zu objectShapes=%zu objectTriangles=%zu workerUs=%llu gpuUs=%llu maxTextureStepUs=%llu maxShapeStepUs=%llu stagedGpu=1 fairScheduler=1",
             gQ1970LodUploadBlockXQ19, gQ1970LodUploadBlockYQ19,
             gQ1970LodUploadRingQ19,
             gQ1990NativeLodBlocks.back().terrain.size(), terrainTriangles,
             gQ1990NativeLodBlocks.back().objects.size(), objectTriangles,
             static_cast<unsigned long long>(gQ1970LodUploadWorkerUsQ19),
             static_cast<unsigned long long>(state.gpuUs),
             static_cast<unsigned long long>(state.maxTextureGpuUs),
             static_cast<unsigned long long>(state.maxShapeGpuUs));

    gQ1970LodUploadQ19.reset();
}

bool Q1970ChooseMissingLodQ19(
        int32_t& nextBlockX, int32_t& nextBlockY, int& bestRing) {
    int bestManhattan = 1000000;
    bestRing = 1000000;
    bool foundMissing = false;
    for (int dy = -Q1840_DISTANT_BLOCK_RADIUS;
         dy <= Q1840_DISTANT_BLOCK_RADIUS; ++dy) {
        for (int dx = -Q1840_DISTANT_BLOCK_RADIUS;
             dx <= Q1840_DISTANT_BLOCK_RADIUS; ++dx) {
            const int32_t bx =
                gQ1990NativeLodCentreBlockX +
                dx * Q1840_LEVEL4_BLOCK_CELLS;
            const int32_t by =
                gQ1990NativeLodCentreBlockY +
                dy * Q1840_LEVEL4_BLOCK_CELLS;
            if (Q1990FindLodBlock(bx, by) ||
                Q1970LodBlockPendingQ19(bx, by)) continue;
            const int ring = std::max(std::abs(dx), std::abs(dy));
            const int manhattan = std::abs(dx) + std::abs(dy);
            if (!foundMissing || ring < bestRing ||
                (ring == bestRing && manhattan < bestManhattan)) {
                foundMissing = true;
                bestRing = ring;
                bestManhattan = manhattan;
                nextBlockX = bx;
                nextBlockY = by;
            }
        }
    }
    return foundMissing;
}

void Q1970AdvanceNativeLodQ19(int32_t cellX, int32_t cellY,
                              float centerX, float centerY, float floorZ) {
    if (!gQ1900ContextReadyQ19 ||
        gExteriorWorldspaceQ1890 != 0x0000003Cu) {
        return;
    }

    Q1970ConsumeLodWorkerQ19();

    // Q19.7 exterior scheduling calls this after Q1900UpdateCellStreaming, so
    // detailed CELL uploads always receive the first frame-budget opportunity.
    // LOD only progresses one bounded leftover slice here.
    Q1970AdvanceLodGpuQ19();
    if (gQ1970LodUploadQ19 || gQ1970LodWorkerQ19) return;

    // Share the CPU asset lane safely, but do not wait for all radius-2/prefetch
    // cells. As soon as the active 3x3 is visually covered, LOD may take one turn.
    if (gQ1900WorkerQ19 || gQ1930CollisionTaskQ19 ||
        !Q1970NearDetailSafeQ19(cellX, cellY)) {
        return;
    }

    int32_t blockX = 0, blockY = 0;
    int ring = 0;
    if (!Q1970ChooseMissingLodQ19(blockX, blockY, ring)) return;

    auto task = std::make_shared<Q1970LodWorkerTaskQ19>();
    task->contextSerial = gQ1900ContextSerialQ19;
    task->blockX = blockX;
    task->blockY = blockY;
    task->ring = ring;
    task->centerX = centerX;
    task->centerY = centerY;
    task->floorZ = floorZ;
    gQ1970LodWorkerQ19 = task;

    Q6H_LOGI("Q19.7 LOD CPU START: block=(%d,%d) ring=%d active3x3Ready=1 serializedAssetLane=1 outerRingStarvation=0",
             blockX, blockY, ring);
    std::thread([task]() { Q1970RunLodWorkerQ19(task); }).detach();
}

bool Q1900CollectCollisionWindowQ19(
        int32_t centreX, int32_t centreY,
        std::vector<Fo3WorldPlacement>& placements) {
    placements.clear();
    std::unordered_set<uint32_t> refs;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const auto found = gQ1900CellsQ19.find(
                Q1900CellKeyQ19(centreX + dx, centreY + dy));
            if (found == gQ1900CellsQ19.end() ||
                !found->second.metadataReady ||
                !found->second.collisionPrimed) {
                return false;
            }
            for (const Fo3WorldPlacement& placement :
                 found->second.placements) {
                if (placement.refFormId == 0u ||
                    !refs.insert(placement.refFormId).second ||
                    Q74ShouldSkipPlacement(placement)) continue;
                placements.push_back(placement);
            }
        }
    }
    return true;
}

void Q1930RunCollisionSnapshotTaskQ19(
        const std::shared_ptr<Q1930CollisionSnapshotTaskQ19>& task) {
    const auto started = std::chrono::steady_clock::now();
    task->success = PrepareFo3CollisionSnapshotQ1930(
        task->placements,
        gQ1900OriginXQ19,
        gQ1900OriginYQ19,
        gQ1900OriginZQ19,
        SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE,
        &task->snapshotToken);
    task->prepareUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count());
    task->ready.store(true, std::memory_order_release);
}

void Q1900TryPublishCollisionQ19(int32_t actualGridX, int32_t actualGridY) {
    if (gQ1900CollisionCentreValidQ19 &&
        gQ1900CollisionCentreXQ19 == actualGridX &&
        gQ1900CollisionCentreYQ19 == actualGridY) {
        return;
    }

    if (gQ1930CollisionTaskQ19) {
        if (!gQ1930CollisionTaskQ19->ready.load(std::memory_order_acquire))
            return;

        const auto task = gQ1930CollisionTaskQ19;
        gQ1930CollisionTaskQ19.reset();

        const bool stale =
            task->contextSerial != gQ1900ContextSerialQ19 ||
            task->centreX != actualGridX ||
            task->centreY != actualGridY;
        if (stale) {
            DiscardFo3CollisionSnapshotQ1930(task->snapshotToken);
            Q6H_LOGW("Q19.3 COLLISION SNAPSHOT STALE: preparedCentre=(%d,%d) actual=(%d,%d) prepareUs=%llu action=discard oldCollisionRetained=1",
                     task->centreX, task->centreY,
                     actualGridX, actualGridY,
                     static_cast<unsigned long long>(task->prepareUs));
        } else if (task->success && task->snapshotToken != 0u) {
            uint64_t swapUs = 0u;
            const bool ready = PublishFo3CollisionSnapshotQ1930(
                task->snapshotToken, &swapUs);
            if (ready) {
                gQ1900CollisionCentreValidQ19 = true;
                gQ1900CollisionCentreXQ19 = actualGridX;
                gQ1900CollisionCentreYQ19 = actualGridY;
            }
            Q6H_LOGI("Q19.3 COLLISION PUBLISH: centre=(%d,%d) placements=%zu ready=%d prepareUs=%llu swapUs=%llu source=prepared-snapshot derivedReadyAtPublish=1 oldCollisionLiveDuringPrepare=1",
                     actualGridX, actualGridY, task->placements.size(),
                     ready ? 1 : 0,
                     static_cast<unsigned long long>(task->prepareUs),
                     static_cast<unsigned long long>(swapUs));
            return;
        } else {
            Q6H_LOGE("Q19.3 COLLISION SNAPSHOT FAILED: centre=(%d,%d) placements=%zu prepareUs=%llu oldCollisionRetained=1",
                     task->centreX, task->centreY,
                     task->placements.size(),
                     static_cast<unsigned long long>(task->prepareUs));
        }
    }

    // Keep one serialized background asset job at a time. The transformed REFR
    // cache is shared with CELL prewarming and was designed for single-writer use.
    if (gQ1900WorkerQ19 || gQ1930CollisionTaskQ19 ||
        gQ1970LodWorkerQ19) return;

    std::vector<Fo3WorldPlacement> placements;
    if (!Q1900CollectCollisionWindowQ19(
            actualGridX, actualGridY, placements)) return;

    auto task = std::make_shared<Q1930CollisionSnapshotTaskQ19>();
    task->contextSerial = gQ1900ContextSerialQ19;
    task->centreX = actualGridX;
    task->centreY = actualGridY;
    task->placements = std::move(placements);
    gQ1930CollisionTaskQ19 = task;

    Q6H_LOGI("Q19.3 COLLISION SNAPSHOT START: centre=(%d,%d) placements=%zu oldCollisionRetained=1 thread=serialized-asset-worker",
             actualGridX, actualGridY, task->placements.size());
    std::thread([task]() { Q1930RunCollisionSnapshotTaskQ19(task); }).detach();
}

void Q1900MaintainTerrainQ19(int32_t actualGridX, int32_t actualGridY) {
    if (!gQ1900TerrainCentreValidQ19) {
        gQ1900TerrainCentreValidQ19 = true;
        gQ1900TerrainCentreXQ19 = actualGridX;
        gQ1900TerrainCentreYQ19 = actualGridY;
        return;
    }
    const int dx = std::abs(actualGridX - gQ1900TerrainCentreXQ19);
    const int dy = std::abs(actualGridY - gQ1900TerrainCentreYQ19);
    if (dx <= 2 && dy <= 2) return;

    const auto started = std::chrono::steady_clock::now();
    const float selectionX =
        (static_cast<float>(actualGridX) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
    const float selectionY =
        (static_cast<float>(actualGridY) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
    SetFo3TerrainSelectionOverrideQ1890(true, selectionX, selectionY);
    const bool ready = InitializeFo3TerrainRenderQ76(
        gExteriorWorldspaceQ1890,
        gExteriorOriginXQ1890,
        gExteriorOriginYQ1890,
        gExteriorOriginZQ1890,
        SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
    SetFo3TerrainSelectionOverrideQ1890(false, 0.0f, 0.0f);
    if (ready) {
        ActivateFo3TerrainGroundingQ77(
            gExteriorWorldspaceQ1890,
            gExteriorOriginXQ1890,
            gExteriorOriginYQ1890,
            gExteriorOriginZQ1890,
            SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
        gQ1900TerrainCentreXQ19 = actualGridX;
        gQ1900TerrainCentreYQ19 = actualGridY;
    }
    const uint64_t elapsedUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count());
    Q6H_LOGI("Q19 TERRAIN LEGACY RECENTER: centre=(%d,%d) ready=%d elapsedUs=%llu radius=3 note=next-q19-terrain-cell-residency",
             actualGridX, actualGridY, ready ? 1 : 0,
             static_cast<unsigned long long>(elapsedUs));
}

void Q1900RetireFarCellsQ19(int32_t actualGridX, int32_t actualGridY) {
    if (actualGridX == gQ1900LastActualXQ19 &&
        actualGridY == gQ1900LastActualYQ19) return;
    gQ1900LastActualXQ19 = actualGridX;
    gQ1900LastActualYQ19 = actualGridY;

    const auto retireStarted = std::chrono::steady_clock::now();
    size_t retiredShapes = 0u;
    const size_t oldLive = gObjects.size();
    size_t write = 0u;

    // Q19.1: one O(n) compaction. Outgoing shapes stop drawing immediately,
    // but their GL objects are destroyed later under a tiny frame budget.
    for (size_t read = 0u; read < oldLive; ++read) {
        GpuObject& object = gObjects[read];
        const int dx = std::abs(object.q1970GridX - actualGridX);
        const int dy = std::abs(object.q1970GridY - actualGridY);
        const bool retire =
            !object.q1990NativeLod &&
            std::max(dx, dy) > Q1900_CACHE_RADIUS_Q19;
        if (retire) {
            Q1910QueueGpuDeleteQ19(object);
            ++retiredShapes;
            continue;
        }
        if (write != read) {
            gObjects[write] = std::move(object);
            object.vbo = 0u;
            object.vao = 0u;
        }
        ++write;
    }
    gObjects.resize(write);

    const uint64_t workerKey = gQ1900WorkerQ19
        ? Q1900CellKeyQ19(gQ1900WorkerQ19->gridX, gQ1900WorkerQ19->gridY)
        : UINT64_MAX;
    size_t retiredStaged = 0u;
    for (auto it = gQ1900CellsQ19.begin(); it != gQ1900CellsQ19.end();) {
        const int dx = std::abs(it->second.gridX - actualGridX);
        const int dy = std::abs(it->second.gridY - actualGridY);
        const bool uploading =
            it->second.stage == Q1900CellStageQ19::Uploading;
        if (it->first != workerKey && !uploading &&
            std::max(dx, dy) > Q1900_CACHE_RADIUS_Q19 + 1) {
            for (GpuObject& object : it->second.stagedGpu) {
                Q1910QueueGpuDeleteQ19(object);
                ++retiredStaged;
            }
            it = gQ1900CellsQ19.erase(it);
        } else {
            ++it;
        }
    }

    const uint64_t retireQueueUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - retireStarted).count());
    if (retiredShapes > 0u || retiredStaged > 0u) {
        gSceneReady = !gObjects.empty();
        gShadowDirtyQ1050 = true;
        Q6H_LOGI("Q19.1 CELL RETIRE QUEUED: actual=(%d,%d) retiredLiveShapes=%zu retiredStagedShapes=%zu liveShapes=%zu deferredDeletes=%zu cacheRadius=%d queueUs=%llu algorithm=single-pass-compact+deferred-gl-delete",
                 actualGridX, actualGridY, retiredShapes, retiredStaged,
                 gObjects.size(), gQ1910DeferredGpuDeletesQ19.size(),
                 Q1900_CACHE_RADIUS_Q19,
                 static_cast<unsigned long long>(retireQueueUs));
    }
}

const char* Q1900CellStageNameQ19(Q1900CellStageQ19 stage) {
    switch (stage) {
        case Q1900CellStageQ19::Ready: return "ready";
        case Q1900CellStageQ19::Requested: return "requested";
        case Q1900CellStageQ19::Worker: return "worker";
        case Q1900CellStageQ19::Uploading: return "uploading";
        case Q1900CellStageQ19::Failed: return "failed";
    }
    return "unknown";
}

void Q1900UpdateCellStreamingQ19(
        float gameX, float gameY,
        int32_t actualGridX, int32_t actualGridY) {
    if (Q1900ContextChangedQ19())
        Q1900ResetCellsQ19(actualGridX, actualGridY);

    Q1900ConsumeWorkerQ19();
    Q1900EnsureWantedCellsQ19(gameX, gameY, actualGridX, actualGridY);
    Q1910DrainDeferredGpuDeletesQ19(false);
    Q1900AdvanceGpuQ19(actualGridX, actualGridY);
    Q1900TryPublishCollisionQ19(actualGridX, actualGridY);
    Q1900MaintainTerrainQ19(actualGridX, actualGridY);
    Q1900RetireFarCellsQ19(actualGridX, actualGridY);
    Q1900LaunchWorkerQ19(actualGridX, actualGridY);

    bool pendingVisual = false;
    size_t readyCells = 0u;
    size_t requestedCells = 0u;
    for (const auto& entry : gQ1900CellsQ19) {
        const Q1900CellStateQ19& state = entry.second;
        if (state.visualReady) ++readyCells;
        if (state.stage == Q1900CellStageQ19::Requested ||
            state.stage == Q1900CellStageQ19::Worker ||
            state.stage == Q1900CellStageQ19::Uploading) {
            ++requestedCells;
            if (!state.visualReady) pendingVisual = true;
        }
    }
    gExteriorStreamBusyQ1890 =
        gQ1900WorkerQ19 != nullptr ||
        gQ1930CollisionTaskQ19 != nullptr ||
        pendingVisual;

    // Logical centre follows the player; residency is now CELL-specific rather
    // than represented by a single 5x5 generation centre.
    gExteriorWindowGridXQ1890 = actualGridX;
    gExteriorWindowGridYQ1890 = actualGridY;

    static int32_t lastStatusX = INT32_MIN;
    static int32_t lastStatusY = INT32_MIN;
    static bool lastReady = true;
    const auto active = gQ1900CellsQ19.find(
        Q1900CellKeyQ19(actualGridX, actualGridY));
    const bool activeReady =
        active != gQ1900CellsQ19.end() && active->second.visualReady;
    if (actualGridX != lastStatusX || actualGridY != lastStatusY ||
        activeReady != lastReady) {
        const char* stage = active == gQ1900CellsQ19.end()
            ? "absent" : Q1900CellStageNameQ19(active->second.stage);
        Q6H_LOGI("Q19 ACTIVE CELL: cell=(%d,%d) ready=%d stage=%s readyCells=%zu trackedCells=%zu pendingCells=%zu lodKeptUntilReady=%d noGenerationCancel=1",
                 actualGridX, actualGridY, activeReady ? 1 : 0, stage,
                 readyCells, gQ1900CellsQ19.size(), requestedCells,
                 activeReady ? 0 : 1);
        lastStatusX = actualGridX;
        lastStatusY = actualGridY;
        lastReady = activeReady;
    }
}
