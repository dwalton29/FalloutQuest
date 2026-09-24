# Q16.27: remove the remaining cell-turnover stalls without growing the full-detail world.
#
# Q16.26 device evidence isolated three costs:
#   1) every adjacent collision prefetch rebuilt the whole 3x3 transformed triangle
#      soup synchronously (~0.6-1.1s), even though six of nine authored cells overlap;
#   2) retained LAND eventually recentred by rebuilding all 25 cells synchronously;
#   3) the full-detail object streamer can still be outrun, exposing an empty horizon.
#
# Q16.27 therefore keeps the proven 5x5 resident / actual-centred 3x3 draw hierarchy,
# caches transformed authored collision per REFR across rolling 3x3 windows, widens
# cheap LAND backing to a 7x7 runway, and renders Bethesda's real Level4 Wasteland
# terrain/object macromeshes outside the near world. Level4 is stored separately from
# gObjects so the full-detail stream cannot retire it.

# -----------------------------------------------------------------------------
# A. Persistent transformed collision placement cache.
# -----------------------------------------------------------------------------
set(Q1990_COLLISION_GLOBAL_OLD [==[
std::vector<CollisionTriangle> gWorldTriangles;
std::unordered_map<uint64_t, CollisionSurfaceSourceQ722> gSurfaceSourcesQ722;
]==])
set(Q1990_COLLISION_GLOBAL_NEW [==[
std::vector<CollisionTriangle> gWorldTriangles;
std::unordered_map<uint64_t, CollisionSurfaceSourceQ722> gSurfaceSourcesQ722;

// Q16.27: collision is represented by transformed authored triangles rather than
// persistent physics-engine bodies. Cache the expensive per-REFR transform result;
// each rolling 3x3 rebuild can then memcpy the six overlapping cells and transform
// only the entering strip. The cache is scoped to one immutable exterior origin.
struct Q1990CachedCollisionPlacement {
    std::vector<CollisionTriangle> triangles;
    size_t shapeCount = 0u;
    std::array<size_t, 5> kindCounts{};
    uint64_t lastUse = 0u;
};
std::unordered_map<uint32_t, Q1990CachedCollisionPlacement> gQ1990CollisionPlacementCache;
uint64_t gQ1990CollisionCacheSerial = 0u;
bool gQ1990CollisionCacheContextValid = false;
float gQ1990CollisionCacheCenterX = 0.0f;
float gQ1990CollisionCacheCenterY = 0.0f;
float gQ1990CollisionCacheFloorZ = 0.0f;
float gQ1990CollisionCacheSceneForward = 0.0f;
float gQ1990CollisionCacheFloorY = 0.0f;
float gQ1990CollisionCacheUnitsPerMetre = 0.0f;
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1990_COLLISION_GLOBAL_OLD}" Q1990_COLLISION_GLOBAL_POS)
if(Q1990_COLLISION_GLOBAL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find Q7.22 collision world/source globals")
endif()
string(REPLACE "${Q1990_COLLISION_GLOBAL_OLD}" "${Q1990_COLLISION_GLOBAL_NEW}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

set(Q1990_COLLISION_CONTEXT_OLD [==[
    gCollisionFloorY = floorY;

    const bool exteriorAllBhksQ78A = IsExteriorMegatonPlacementSetQ78A(placements);
]==])
set(Q1990_COLLISION_CONTEXT_NEW [==[
    gCollisionFloorY = floorY;

    const bool q1990SameCollisionContext = gQ1990CollisionCacheContextValid &&
        std::fabs(gQ1990CollisionCacheCenterX - centerX) < 0.01f &&
        std::fabs(gQ1990CollisionCacheCenterY - centerY) < 0.01f &&
        std::fabs(gQ1990CollisionCacheFloorZ - floorZ) < 0.01f &&
        std::fabs(gQ1990CollisionCacheSceneForward - sceneForward) < 0.0001f &&
        std::fabs(gQ1990CollisionCacheFloorY - floorY) < 0.0001f &&
        std::fabs(gQ1990CollisionCacheUnitsPerMetre - unitsPerMetre) < 0.0001f;
    if (!q1990SameCollisionContext) {
        const size_t q1990OldEntries = gQ1990CollisionPlacementCache.size();
        gQ1990CollisionPlacementCache.clear();
        gQ1990CollisionCacheContextValid = true;
        gQ1990CollisionCacheCenterX = centerX;
        gQ1990CollisionCacheCenterY = centerY;
        gQ1990CollisionCacheFloorZ = floorZ;
        gQ1990CollisionCacheSceneForward = sceneForward;
        gQ1990CollisionCacheFloorY = floorY;
        gQ1990CollisionCacheUnitsPerMetre = unitsPerMetre;
        Q6F_LOGI("Q16.27 COLLISION CACHE RESET: oldEntries=%zu origin=(%.2f %.2f %.2f) unitsPerMetre=%.2f",
                 q1990OldEntries, centerX, centerY, floorZ, unitsPerMetre);
    }

    const bool exteriorAllBhksQ78A = IsExteriorMegatonPlacementSetQ78A(placements);
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1990_COLLISION_CONTEXT_OLD}" Q1990_COLLISION_CONTEXT_POS)
if(Q1990_COLLISION_CONTEXT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find collision initialization context")
endif()
string(REPLACE "${Q1990_COLLISION_CONTEXT_OLD}" "${Q1990_COLLISION_CONTEXT_NEW}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

set(Q1990_COLLISION_COUNTER_OLD [==[
    size_t misses = 0;
    size_t cacheHits = 0;
]==])
set(Q1990_COLLISION_COUNTER_NEW [==[
    size_t misses = 0;
    size_t cacheHits = 0;
    size_t q1990PlacementCacheHits = 0u;
    size_t q1990PlacementCacheMisses = 0u;
    size_t q1990PlacementCacheTriangles = 0u;
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1990_COLLISION_COUNTER_OLD}" Q1990_COLLISION_COUNTER_POS)
if(Q1990_COLLISION_COUNTER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find collision cache counters")
endif()
string(REPLACE "${Q1990_COLLISION_COUNTER_OLD}" "${Q1990_COLLISION_COUNTER_NEW}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

set(Q1990_COLLISION_LOOKUP_OLD [==[
        ++bhkAttemptsQ78A;

        if (noCollisionModelsQ78A.find(placement.modelPath) != noCollisionModelsQ78A.end()) {
]==])
set(Q1990_COLLISION_LOOKUP_NEW [==[
        ++bhkAttemptsQ78A;

        if (exteriorAllBhksQ78A) {
            auto q1990CachedPlacement = gQ1990CollisionPlacementCache.find(placement.refFormId);
            if (q1990CachedPlacement != gQ1990CollisionPlacementCache.end()) {
                Q1990CachedCollisionPlacement& q1990Entry = q1990CachedPlacement->second;
                if (gWorldTriangles.size() + q1990Entry.triangles.size() >
                    MAX_EXTERIOR_COLLISION_TRIANGLES_Q78A) {
                    capped = true;
                    break;
                }
                gWorldTriangles.insert(gWorldTriangles.end(),
                                       q1990Entry.triangles.begin(), q1990Entry.triangles.end());
                for (const CollisionTriangle& q1990Tri : q1990Entry.triangles) {
                    if (gSurfaceSourcesQ722.find(q1990Tri.surfaceKeyQ714) == gSurfaceSourcesQ722.end()) {
                        gSurfaceSourcesQ722.emplace(
                            q1990Tri.surfaceKeyQ714,
                            CollisionSurfaceSourceQ722{placement.refFormId,
                                                       placement.editorId,
                                                       placement.modelPath});
                    }
                }
                ++gPlacementCount;
                gCollisionShapeCount += q1990Entry.shapeCount;
                gTriangleCount += q1990Entry.triangles.size();
                for (size_t q1990Kind = 0u; q1990Kind < gKindCounts.size(); ++q1990Kind)
                    gKindCounts[q1990Kind] += q1990Entry.kindCounts[q1990Kind];
                ++q1990PlacementCacheHits;
                q1990PlacementCacheTriangles += q1990Entry.triangles.size();
                q1990Entry.lastUse = ++gQ1990CollisionCacheSerial;
                continue;
            }
            ++q1990PlacementCacheMisses;
        }

        if (noCollisionModelsQ78A.find(placement.modelPath) != noCollisionModelsQ78A.end()) {
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1990_COLLISION_LOOKUP_OLD}" Q1990_COLLISION_LOOKUP_POS)
if(Q1990_COLLISION_LOOKUP_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find collision placement traversal entry")
endif()
string(REPLACE "${Q1990_COLLISION_LOOKUP_OLD}" "${Q1990_COLLISION_LOOKUP_NEW}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

set(Q1990_COLLISION_TRI_START_OLD [==[
        size_t placementTriangles = 0;
        size_t placementShapes = 0;
]==])
set(Q1990_COLLISION_TRI_START_NEW [==[
        const size_t q1990PlacementTriangleStart = gWorldTriangles.size();
        size_t placementTriangles = 0;
        size_t placementShapes = 0;
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1990_COLLISION_TRI_START_OLD}" Q1990_COLLISION_TRI_START_POS)
if(Q1990_COLLISION_TRI_START_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find collision placement triangle counters")
endif()
string(REPLACE "${Q1990_COLLISION_TRI_START_OLD}" "${Q1990_COLLISION_TRI_START_NEW}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

set(Q1990_COLLISION_STORE_OLD [==[
            for (size_t i = 0; i < gKindCounts.size(); ++i) gKindCounts[i] += placementKinds[i];
            if (gPlacementCount <= 80u) {
]==])
set(Q1990_COLLISION_STORE_NEW [==[
            for (size_t i = 0; i < gKindCounts.size(); ++i) gKindCounts[i] += placementKinds[i];
            if (exteriorAllBhksQ78A && !capped && placement.refFormId != 0u &&
                q1990PlacementTriangleStart < gWorldTriangles.size()) {
                Q1990CachedCollisionPlacement q1990Entry;
                q1990Entry.triangles.assign(
                    gWorldTriangles.begin() + static_cast<std::ptrdiff_t>(q1990PlacementTriangleStart),
                    gWorldTriangles.end());
                q1990Entry.shapeCount = placementShapes;
                q1990Entry.kindCounts = placementKinds;
                q1990Entry.lastUse = ++gQ1990CollisionCacheSerial;
                gQ1990CollisionPlacementCache[placement.refFormId] = std::move(q1990Entry);
            }
            if (gPlacementCount <= 80u) {
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1990_COLLISION_STORE_OLD}" Q1990_COLLISION_STORE_POS)
if(Q1990_COLLISION_STORE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find successful collision placement accounting")
endif()
string(REPLACE "${Q1990_COLLISION_STORE_OLD}" "${Q1990_COLLISION_STORE_NEW}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

set(Q1990_COLLISION_LOG_OLD [==[
    if (gWorldTriangles.empty()) {
]==])
set(Q1990_COLLISION_LOG_NEW [==[
    // Bound the persistent transformed cache. A few thousand refs cover several
    // neighbouring 3x3 windows/backtracking without turning traversal into an
    // unbounded memory sink.
    if (gQ1990CollisionPlacementCache.size() > 4096u) {
        std::vector<std::pair<uint64_t, uint32_t>> q1990Ages;
        q1990Ages.reserve(gQ1990CollisionPlacementCache.size());
        for (const auto& q1990Pair : gQ1990CollisionPlacementCache)
            q1990Ages.emplace_back(q1990Pair.second.lastUse, q1990Pair.first);
        std::sort(q1990Ages.begin(), q1990Ages.end());
        const size_t q1990Remove = gQ1990CollisionPlacementCache.size() - 3072u;
        for (size_t q1990Index = 0u; q1990Index < q1990Remove; ++q1990Index)
            gQ1990CollisionPlacementCache.erase(q1990Ages[q1990Index].second);
    }
    Q6F_LOGI("Q16.27 COLLISION CACHE: placementHits=%zu placementMisses=%zu reusedTriangles=%zu entries=%zu mode=persistent-transformed-REFR-chunks",
             q1990PlacementCacheHits, q1990PlacementCacheMisses,
             q1990PlacementCacheTriangles, gQ1990CollisionPlacementCache.size());

    if (gWorldTriangles.empty()) {
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1990_COLLISION_LOG_OLD}" Q1990_COLLISION_LOG_POS)
if(Q1990_COLLISION_LOG_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find stable empty-collision guard")
endif()
string(REPLACE "${Q1990_COLLISION_LOG_OLD}" "${Q1990_COLLISION_LOG_NEW}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# Force the coherent/weld/spatial layers to repoint at the new copied triangle
# vector even in the rare case std::vector reuses the same allocation and count.
set(Q1990_COLLISION_READY_OLD [==[
    gPlayerCollisionReady = true;
]==])
set(Q1990_COLLISION_READY_NEW [==[
    gHkWeldAdjacencyReadyQ801 = false;
    gHkShapesReadyQ900 = false;
    gQ950Ready = false;
    gPlayerCollisionReady = true;
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1990_COLLISION_READY_OLD}" Q1990_COLLISION_READY_POS)
if(Q1990_COLLISION_READY_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find collision ready publication")
endif()
string(REPLACE "${Q1990_COLLISION_READY_OLD}" "${Q1990_COLLISION_READY_NEW}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# -----------------------------------------------------------------------------
# B. LAND runway: restore a cheap 7x7 backing ring and keep it centred by ACTUAL
# player position. This does not pretend the legacy global terrain VBO is already
# per-cell owned; it deliberately reduces how often that expensive rebuild occurs.
# -----------------------------------------------------------------------------
set(Q1990_TERRAIN_RADIUS_OLD
    "constexpr int Q1931_TERRAIN_GRID_RADIUS = 2; // Q16.26 retained 5x5 LAND backing")
set(Q1990_TERRAIN_RADIUS_NEW
    "constexpr int Q1931_TERRAIN_GRID_RADIUS = 3; // Q16.27 retained 7x7 LAND runway")
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1990_TERRAIN_RADIUS_OLD}" Q1990_TERRAIN_RADIUS_POS)
if(Q1990_TERRAIN_RADIUS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find Q16.26 LAND radius")
endif()
string(REPLACE "${Q1990_TERRAIN_RADIUS_OLD}" "${Q1990_TERRAIN_RADIUS_NEW}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
string(REPLACE "Q16.26 TERRAIN WINDOW:" "Q16.27 TERRAIN WINDOW:"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")

set(Q1990_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1990_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.27 expected Q16.26 generated renderer")
endif()
file(READ "${Q1990_NATIVE_FILE}" Q1990_NATIVE_SOURCE)

set(Q1990_TERRAIN_DELTA_OLD [==[
    const int q1950TerrainDx = std::abs(gPendingStreamQ1900.targetGridX - q1950TerrainGridX);
    const int q1950TerrainDy = std::abs(gPendingStreamQ1900.targetGridY - q1950TerrainGridY);
    if (q1950TerrainDx <= 1 && q1950TerrainDy <= 1) {
]==])
set(Q1990_TERRAIN_DELTA_NEW [==[
    const int32_t q1990TerrainReferenceGridX = gQ1920LatestGridValid
        ? gQ1920LatestGridX : gPendingStreamQ1900.targetGridX;
    const int32_t q1990TerrainReferenceGridY = gQ1920LatestGridValid
        ? gQ1920LatestGridY : gPendingStreamQ1900.targetGridY;
    const int q1950TerrainDx = std::abs(q1990TerrainReferenceGridX - q1950TerrainGridX);
    const int q1950TerrainDy = std::abs(q1990TerrainReferenceGridY - q1950TerrainGridY);
    // Radius 3 can keep an actual-centred radius-1 near world fully backed while
    // actualGrid moves two cells from the retained LAND centre.
    if (q1950TerrainDx <= 2 && q1950TerrainDy <= 2) {
]==])
string(FIND "${Q1990_NATIVE_SOURCE}" "${Q1990_TERRAIN_DELTA_OLD}" Q1990_TERRAIN_DELTA_POS)
if(Q1990_TERRAIN_DELTA_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find retained terrain delta test")
endif()
string(REPLACE "${Q1990_TERRAIN_DELTA_OLD}" "${Q1990_TERRAIN_DELTA_NEW}"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")

# If a full rebuild is finally necessary, centre the new 7x7 ring on actualGrid
# rather than on a stale predictive resident target.
set(Q1990_TERRAIN_SELECT_OLD [==[
    SetFo3TerrainSelectionOverrideQ1890(
        true,
        gPendingStreamQ1900.selectionGameX,
        gPendingStreamQ1900.selectionGameY);
]==])
set(Q1990_TERRAIN_SELECT_NEW [==[
    const float q1990TerrainSelectionGameX =
        (static_cast<float>(q1990TerrainReferenceGridX) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
    const float q1990TerrainSelectionGameY =
        (static_cast<float>(q1990TerrainReferenceGridY) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
    SetFo3TerrainSelectionOverrideQ1890(
        true, q1990TerrainSelectionGameX, q1990TerrainSelectionGameY);
]==])
string(FIND "${Q1990_NATIVE_SOURCE}" "${Q1990_TERRAIN_SELECT_OLD}" Q1990_TERRAIN_SELECT_POS)
if(Q1990_TERRAIN_SELECT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find staged terrain selection override")
endif()
string(REPLACE "${Q1990_TERRAIN_SELECT_OLD}" "${Q1990_TERRAIN_SELECT_NEW}"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")

set(Q1990_TERRAIN_RECENTER_OLD [==[
        q1950TerrainGridX = gPendingStreamQ1900.targetGridX;
        q1950TerrainGridY = gPendingStreamQ1900.targetGridY;
]==])
set(Q1990_TERRAIN_RECENTER_NEW [==[
        q1950TerrainGridX = q1990TerrainReferenceGridX;
        q1950TerrainGridY = q1990TerrainReferenceGridY;
]==])
string(FIND "${Q1990_NATIVE_SOURCE}" "${Q1990_TERRAIN_RECENTER_OLD}" Q1990_TERRAIN_RECENTER_POS)
if(Q1990_TERRAIN_RECENTER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find terrain retained-centre publication")
endif()
string(REPLACE "${Q1990_TERRAIN_RECENTER_OLD}" "${Q1990_TERRAIN_RECENTER_NEW}"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")
string(REPLACE "centre=(%d,%d) radius=2" "centre=(%d,%d) radius=3"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")
string(REPLACE "delta=(%d,%d) radius=2 fullGpuRebuild=0"
       "delta=(%d,%d) radius=3 fullGpuRebuild=0"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")
string(REPLACE "centre=(%d,%d) radius=2 fullGpuRebuild=1"
       "centre=(%d,%d) radius=3 fullGpuRebuild=1"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# C. Native Bethesda Level4 GPU residency + draw path.
# -----------------------------------------------------------------------------
# Mark LOD GPU shapes so they can reuse the mature material draw helper without
# being rejected by Q16.25/26's full-detail authored-cell predicate.
set(Q1990_GPU_FIELD_OLD [==[
    int32_t q1970GridY = 0;
]==])
set(Q1990_GPU_FIELD_NEW [==[
    int32_t q1970GridY = 0;
    bool q1990NativeLod = false;
]==])
string(FIND "${Q1990_NATIVE_SOURCE}" "${Q1990_GPU_FIELD_OLD}" Q1990_GPU_FIELD_POS)
if(Q1990_GPU_FIELD_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find q1970 GPU grid metadata")
endif()
string(REPLACE "${Q1990_GPU_FIELD_OLD}" "${Q1990_GPU_FIELD_NEW}"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")

set(Q1990_ACTIVE_PRED_OLD [==[
bool Q1970ShouldRenderFullDetail(const GpuObject& object) {
    int32_t activeGridX = 0;
]==])
set(Q1990_ACTIVE_PRED_NEW [==[
bool Q1970ShouldRenderFullDetail(const GpuObject& object) {
    if (object.q1990NativeLod) return true;
    int32_t activeGridX = 0;
]==])
string(FIND "${Q1990_NATIVE_SOURCE}" "${Q1990_ACTIVE_PRED_OLD}" Q1990_ACTIVE_PRED_POS)
if(Q1990_ACTIVE_PRED_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find active full-detail predicate")
endif()
string(REPLACE "${Q1990_ACTIVE_PRED_OLD}" "${Q1990_ACTIVE_PRED_NEW}"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")

# Transition/stream code is earlier in the monolithic TU than the LOD helper body.
set(Q1990_FORWARD_OLD [==[
extern int32_t gQ1920LatestGridY;
]==])
set(Q1990_FORWARD_NEW [==[
extern int32_t gQ1920LatestGridY;
void Q1990EnsureNativeLodForCell(int32_t cellX, int32_t cellY,
                                 float centerX, float centerY, float floorZ);
]==])
string(FIND "${Q1990_NATIVE_SOURCE}" "${Q1990_FORWARD_OLD}" Q1990_FORWARD_POS)
if(Q1990_FORWARD_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find actual-grid forward declarations")
endif()
string(REPLACE "${Q1990_FORWARD_OLD}" "${Q1990_FORWARD_NEW}"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")

set(Q1990_RENDER_MARKER [==[
void RenderScene() {
]==])
set(Q1990_LOD_HELPERS [==[
struct Q1990NativeLodBlock {
    int32_t blockX = 0;
    int32_t blockY = 0;
    uint64_t lastUse = 0u;
    std::vector<GpuObject> terrain;
    std::vector<GpuObject> objects;
};

std::vector<Q1990NativeLodBlock> gQ1990NativeLodBlocks;
uint64_t gQ1990NativeLodSerial = 0u;
bool gQ1990NativeLodOriginValid = false;
float gQ1990NativeLodCenterX = 0.0f;
float gQ1990NativeLodCenterY = 0.0f;
float gQ1990NativeLodFloorZ = 0.0f;
int32_t gQ1990NativeLodCentreBlockX = 0;
int32_t gQ1990NativeLodCentreBlockY = 0;
bool gQ1990NativeLodDrawLogged = false;

int32_t Q1990FloorToLevel4Block(int32_t cell) {
    int32_t quotient = cell / 4;
    if (cell < 0 && (cell % 4) != 0) --quotient;
    return quotient * 4;
}

void Q1990DeleteLodGpu(GpuObject& object) {
    if (object.vbo) glDeleteBuffers(1, &object.vbo);
    if (object.vao) glDeleteVertexArrays(1, &object.vao);
    object.vbo = 0u;
    object.vao = 0u;
}

void Q1990ClearNativeLodGeometry() {
    for (Q1990NativeLodBlock& block : gQ1990NativeLodBlocks) {
        for (GpuObject& object : block.terrain) Q1990DeleteLodGpu(object);
        for (GpuObject& object : block.objects) Q1990DeleteLodGpu(object);
    }
    gQ1990NativeLodBlocks.clear();
    gQ1990NativeLodDrawLogged = false;
}

bool Q1990UploadNativeLodNif(const std::string& path,
                             float centerX, float centerY, float floorZ,
                             std::vector<GpuObject>& output,
                             size_t& outTriangles) {
    std::vector<Fo3StaticNifMesh> meshes;
    if (!LoadFo3StaticNifMeshes(path, meshes) || meshes.empty()) return false;
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
                cpu.mesh.positions[i * 3u], cpu.mesh.positions[i * 3u + 1u], cpu.mesh.positions[i * 3u + 2u]});
            cpu.normalsGame.push_back(Vec3{
                cpu.mesh.normals[i * 3u], cpu.mesh.normals[i * 3u + 1u], cpu.mesh.normals[i * 3u + 2u]});
            cpu.tangentsGame.push_back(Vec3{
                cpu.mesh.tangents[i * 3u], cpu.mesh.tangents[i * 3u + 1u], cpu.mesh.tangents[i * 3u + 2u]});
            cpu.bitangentsGame.push_back(Vec3{
                cpu.mesh.bitangents[i * 3u], cpu.mesh.bitangents[i * 3u + 1u], cpu.mesh.bitangents[i * 3u + 2u]});
        }
        GpuObject gpu;
        if (!UploadCpuObject(cpu, centerX, centerY, floorZ, gpu)) {
            Q1990DeleteLodGpu(gpu);
            continue;
        }
        gpu.q1990NativeLod = true;
        outTriangles += static_cast<size_t>(gpu.vertexCount / 3);
        output.push_back(std::move(gpu));
        any = true;
    }
    return any;
}

Q1990NativeLodBlock* Q1990FindLodBlock(int32_t blockX, int32_t blockY) {
    for (Q1990NativeLodBlock& block : gQ1990NativeLodBlocks)
        if (block.blockX == blockX && block.blockY == blockY) return &block;
    return nullptr;
}

bool Q1990LodBlockDesired(int32_t blockX, int32_t blockY) {
    return std::abs(blockX - gQ1990NativeLodCentreBlockX) <= 4 &&
           std::abs(blockY - gQ1990NativeLodCentreBlockY) <= 4;
}

void Q1990EnsureNativeLodForCell(int32_t cellX, int32_t cellY,
                                 float centerX, float centerY, float floorZ) {
    const bool sameOrigin = gQ1990NativeLodOriginValid &&
        std::fabs(gQ1990NativeLodCenterX - centerX) < 0.01f &&
        std::fabs(gQ1990NativeLodCenterY - centerY) < 0.01f &&
        std::fabs(gQ1990NativeLodFloorZ - floorZ) < 0.01f;
    if (!sameOrigin) {
        Q1990ClearNativeLodGeometry();
        gQ1990NativeLodOriginValid = true;
        gQ1990NativeLodCenterX = centerX;
        gQ1990NativeLodCenterY = centerY;
        gQ1990NativeLodFloorZ = floorZ;
    }

    const int32_t centreBlockX = Q1990FloorToLevel4Block(cellX);
    const int32_t centreBlockY = Q1990FloorToLevel4Block(cellY);
    gQ1990NativeLodCentreBlockX = centreBlockX;
    gQ1990NativeLodCentreBlockY = centreBlockY;
    ++gQ1990NativeLodSerial;

    size_t newBlocks = 0u;
    size_t totalTriangles = 0u;
    for (int32_t by = centreBlockY - 4; by <= centreBlockY + 4; by += 4) {
        for (int32_t bx = centreBlockX - 4; bx <= centreBlockX + 4; bx += 4) {
            Q1990NativeLodBlock* existing = Q1990FindLodBlock(bx, by);
            if (existing) {
                existing->lastUse = gQ1990NativeLodSerial;
                continue;
            }
            Q1990NativeLodBlock block;
            block.blockX = bx;
            block.blockY = by;
            block.lastUse = gQ1990NativeLodSerial;
            const std::string suffix = "Wasteland.Level4.X" + std::to_string(bx) +
                                       ".Y" + std::to_string(by) + ".NIF";
            const std::string terrainPath = "Landscape\\LOD\\Wasteland\\" + suffix;
            const std::string objectPath = "Landscape\\LOD\\Wasteland\\Blocks\\" + suffix;
            size_t terrainTriangles = 0u;
            size_t objectTriangles = 0u;
            const bool terrainReady = Q1990UploadNativeLodNif(
                terrainPath, centerX, centerY, floorZ, block.terrain, terrainTriangles);
            const bool objectsReady = Q1990UploadNativeLodNif(
                objectPath, centerX, centerY, floorZ, block.objects, objectTriangles);
            totalTriangles += terrainTriangles + objectTriangles;
            Q6H_LOGI("Q16.27 NATIVE LOD BLOCK READY: block=(%d,%d) terrainReady=%d terrainShapes=%zu terrainTriangles=%zu objectsReady=%d objectShapes=%zu objectTriangles=%zu",
                     bx, by, terrainReady ? 1 : 0, block.terrain.size(), terrainTriangles,
                     objectsReady ? 1 : 0, block.objects.size(), objectTriangles);
            gQ1990NativeLodBlocks.push_back(std::move(block));
            ++newBlocks;
        }
    }

    // Keep a bounded warm macroblock cache. Nine are visible; seven additional
    // recently-used blocks make reversing direction cheap without unbounded VAOs.
    while (gQ1990NativeLodBlocks.size() > 16u) {
        size_t victim = gQ1990NativeLodBlocks.size();
        uint64_t oldest = UINT64_MAX;
        for (size_t i = 0u; i < gQ1990NativeLodBlocks.size(); ++i) {
            const Q1990NativeLodBlock& block = gQ1990NativeLodBlocks[i];
            if (Q1990LodBlockDesired(block.blockX, block.blockY)) continue;
            if (block.lastUse < oldest) { oldest = block.lastUse; victim = i; }
        }
        if (victim >= gQ1990NativeLodBlocks.size()) break;
        for (GpuObject& object : gQ1990NativeLodBlocks[victim].terrain) Q1990DeleteLodGpu(object);
        for (GpuObject& object : gQ1990NativeLodBlocks[victim].objects) Q1990DeleteLodGpu(object);
        gQ1990NativeLodBlocks.erase(gQ1990NativeLodBlocks.begin() + static_cast<std::ptrdiff_t>(victim));
    }

    Q6H_LOGI("Q16.27 NATIVE LOD WINDOW: playerCell=(%d,%d) centreBlock=(%d,%d) visibleBlocks=9 cachedBlocks=%zu newBlocks=%zu newTriangles=%zu mode=Bethesda-Level4-authored-world-coordinates",
             cellX, cellY, centreBlockX, centreBlockY,
             gQ1990NativeLodBlocks.size(), newBlocks, totalTriangles);
}

void Q1990RenderNativeLod(bool alphaPass) {
    if (gExteriorWorldspaceQ1890 != 0x0000003Cu || gQ1990NativeLodBlocks.empty()) return;
    size_t drawnShapes = 0u;
    size_t drawnTriangles = 0u;
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 6.0f);
    for (Q1990NativeLodBlock& block : gQ1990NativeLodBlocks) {
        if (!Q1990LodBlockDesired(block.blockX, block.blockY)) continue;
        // The current 4x4 macroblock is completely covered by the actual-centred
        // 7x7 LAND runway. Do not double-render coarse terrain there. Object LOD
        // remains visible so structures survive beyond the 3x3 detailed objects.
        if (block.blockX != gQ1990NativeLodCentreBlockX ||
            block.blockY != gQ1990NativeLodCentreBlockY) {
            for (const GpuObject& object : block.terrain) {
                if (object.alphaBlend != alphaPass) continue;
                DrawSceneObject(object);
                ++drawnShapes;
                drawnTriangles += static_cast<size_t>(object.vertexCount / 3);
            }
        }
        for (const GpuObject& object : block.objects) {
            if (object.alphaBlend != alphaPass) continue;
            DrawSceneObject(object);
            ++drawnShapes;
            drawnTriangles += static_cast<size_t>(object.vertexCount / 3);
        }
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    if (!alphaPass && !gQ1990NativeLodDrawLogged) {
        gQ1990NativeLodDrawLogged = true;
        Q6H_LOGI("Q16.27 NATIVE LOD DRAW: shapes=%zu triangles=%zu currentTerrainBlockSuppressed=1 polygonOffset=1 shadows=0",
                 drawnShapes, drawnTriangles);
    }
}

void RenderScene() {
]==])
string(FIND "${Q1990_NATIVE_SOURCE}" "${Q1990_RENDER_MARKER}" Q1990_RENDER_POS)
if(Q1990_RENDER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find static RenderScene")
endif()
string(REPLACE "${Q1990_RENDER_MARKER}" "${Q1990_LOD_HELPERS}"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")

set(Q1990_OPAQUE_OLD [==[
    glDisable(GL_BLEND);
    for (const GpuObject& object : gObjects) {
]==])
set(Q1990_OPAQUE_NEW [==[
    glDisable(GL_BLEND);
    Q1990RenderNativeLod(false);
    for (const GpuObject& object : gObjects) {
]==])
string(FIND "${Q1990_NATIVE_SOURCE}" "${Q1990_OPAQUE_OLD}" Q1990_OPAQUE_POS)
if(Q1990_OPAQUE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find current opaque render pass")
endif()
string(REPLACE "${Q1990_OPAQUE_OLD}" "${Q1990_OPAQUE_NEW}"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")

set(Q1990_ALPHA_OLD [==[
    glEnable(GL_BLEND);
    for (const GpuObject& object : gObjects) {
]==])
set(Q1990_ALPHA_NEW [==[
    glEnable(GL_BLEND);
    Q1990RenderNativeLod(true);
    for (const GpuObject& object : gObjects) {
]==])
string(FIND "${Q1990_NATIVE_SOURCE}" "${Q1990_ALPHA_OLD}" Q1990_ALPHA_POS)
if(Q1990_ALPHA_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find current alpha render pass")
endif()
string(REPLACE "${Q1990_ALPHA_OLD}" "${Q1990_ALPHA_NEW}"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")

# Initial Wasteland entry runs under loading comfort; build the first nine cheap
# macroblocks there. Subsequent calls are a no-op until the player enters another
# four-cell Level4 block.
set(Q1990_INITIAL_LOD_OLD [==[
        Q1970ProbeNativeLod(request.x, request.y);
]==])
set(Q1990_INITIAL_LOD_NEW [==[
        Q1970ProbeNativeLod(request.x, request.y);
        Q1990EnsureNativeLodForCell(
            static_cast<int32_t>(std::floor(request.x / Q1890_EXTERIOR_CELL_SIZE)),
            static_cast<int32_t>(std::floor(request.y / Q1890_EXTERIOR_CELL_SIZE)),
            request.x, request.y, request.z);
]==])
string(FIND "${Q1990_NATIVE_SOURCE}" "${Q1990_INITIAL_LOD_OLD}" Q1990_INITIAL_LOD_POS)
if(Q1990_INITIAL_LOD_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find Q16.25 native LOD probe hook")
endif()
string(REPLACE "${Q1990_INITIAL_LOD_OLD}" "${Q1990_INITIAL_LOD_NEW}"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")

set(Q1990_ACTUAL_GRID_OLD [==[
    gQ1920LatestGridY = actualGridY;

    static bool q1970ActiveLogReady = false;
]==])
set(Q1990_ACTUAL_GRID_NEW [==[
    gQ1920LatestGridY = actualGridY;
    if (gExteriorWorldspaceQ1890 == 0x0000003Cu) {
        Q1990EnsureNativeLodForCell(actualGridX, actualGridY,
                                    gExteriorOriginXQ1890,
                                    gExteriorOriginYQ1890,
                                    gExteriorOriginZQ1890);
    }

    static bool q1970ActiveLogReady = false;
]==])
string(FIND "${Q1990_NATIVE_SOURCE}" "${Q1990_ACTUAL_GRID_OLD}" Q1990_ACTUAL_GRID_POS)
if(Q1990_ACTUAL_GRID_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find actual-grid publication hook")
endif()
string(REPLACE "${Q1990_ACTUAL_GRID_OLD}" "${Q1990_ACTUAL_GRID_NEW}"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# D. Remove thousands of render-thread Android log writes from entering shapes.
# Keep Q16.x generation summaries and cache misses available; suppress hot per-
# shape/cache-hit diagnostics only while exterior streaming is active.
# -----------------------------------------------------------------------------
string(REPLACE "Q6H_LOGI(\"Q6H GPU "
       "if (!gExteriorStreamingActiveQ1890) Q6H_LOGI(\"Q6H GPU "
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")
string(REPLACE "Q6H_LOGI(\"Q6H CPU OBJECT:"
       "if (!gExteriorStreamingActiveQ1890) Q6H_LOGI(\"Q6H CPU OBJECT:"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")
string(REPLACE "Q6H_LOGI(\"Q12.0 Z STATE:"
       "if (!gExteriorStreamingActiveQ1890) Q6H_LOGI(\"Q12.0 Z STATE:"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")
string(REPLACE "Q6H_LOGI(\"Q11.2 VCOLOR GATE:"
       "if (!gExteriorStreamingActiveQ1890) Q6H_LOGI(\"Q11.2 VCOLOR GATE:"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")

# Diagnostics describe the real hierarchy. Full-detail remains intentionally 3x3.
string(REPLACE
    "residentRadius=2 activeRadius=1 collisionRadius=1 terrainRadius=2 rebuildMode=5x5-resident+3x3-active+3x3-collision-first+5x5-terrain"
    "residentRadius=2 activeRadius=1 collisionRadius=1 terrainRadius=3 rebuildMode=5x5-resident+3x3-active+cached-3x3-collision+7x7-terrain+Level4-LOD"
    Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")
string(REPLACE "Q16.26" "Q16.27"
       Q1990_NATIVE_SOURCE "${Q1990_NATIVE_SOURCE}")
file(WRITE "${Q1990_NATIVE_FILE}" "${Q1990_NATIVE_SOURCE}")

string(REPLACE "Q16.26 COLLISION MODE OVERRIDE" "Q16.27 COLLISION MODE OVERRIDE"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# -----------------------------------------------------------------------------
# E. Headset-visible Q16.27 label.
# -----------------------------------------------------------------------------
set(Q1990_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1990_Q4_FILE}")
    message(FATAL_ERROR "Q16.27 expected Q16.26 OpenXR source")
endif()
file(READ "${Q1990_Q4_FILE}" Q1990_Q4_SOURCE)
set(Q1990_LABEL_OLD [==[
        q1600Digit(q1600X, 0x5Bu); // Q16.26: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x7Du); // Q16.26: 6 = A F G E D C
]==])
set(Q1990_LABEL_NEW [==[
        q1600Digit(q1600X, 0x5Bu); // Q16.27: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x07u); // Q16.27: 7 = A B C
]==])
string(FIND "${Q1990_Q4_SOURCE}" "${Q1990_LABEL_OLD}" Q1990_LABEL_POS)
if(Q1990_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 could not find Q16.26 build-label digits")
endif()
string(REPLACE "${Q1990_LABEL_OLD}" "${Q1990_LABEL_NEW}"
       Q1990_Q4_SOURCE "${Q1990_Q4_SOURCE}")
string(REPLACE "Q16.26 BUILD LABEL:" "Q16.27 BUILD LABEL:"
       Q1990_Q4_SOURCE "${Q1990_Q4_SOURCE}")
string(REPLACE "text=Q16.26 anchor=left-hand" "text=Q16.27 anchor=left-hand"
       Q1990_Q4_SOURCE "${Q1990_Q4_SOURCE}")
string(REPLACE "Q16.26 AUTHORED DOOR FACING" "Q16.27 AUTHORED DOOR FACING"
       Q1990_Q4_SOURCE "${Q1990_Q4_SOURCE}")
string(REPLACE "Q16.26 FAR CLIP:" "Q16.27 FAR CLIP:"
       Q1990_Q4_SOURCE "${Q1990_Q4_SOURCE}")
file(WRITE "${Q1990_Q4_FILE}" "${Q1990_Q4_SOURCE}")

# -----------------------------------------------------------------------------
# F. Configure-time proof.
# -----------------------------------------------------------------------------
file(READ "${Q1990_NATIVE_FILE}" Q1990_NATIVE_VERIFY)
string(FIND "${Q74_COLLISION_SOURCE}" "Q16.27 COLLISION CACHE:" Q1990_COLLISION_CACHE_OK)
string(FIND "${Q74_COLLISION_SOURCE}" "gQ1990CollisionPlacementCache" Q1990_COLLISION_STORAGE_OK)
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "Q1931_TERRAIN_GRID_RADIUS = 3" Q1990_TERRAIN_RADIUS_OK)
string(FIND "${Q1990_NATIVE_VERIFY}" "q1950TerrainDx <= 2" Q1990_TERRAIN_RETAIN_OK)
string(FIND "${Q1990_NATIVE_VERIFY}" "Q16.27 NATIVE LOD BLOCK READY:" Q1990_LOD_LOAD_OK)
string(FIND "${Q1990_NATIVE_VERIFY}" "Q1990RenderNativeLod(false)" Q1990_LOD_DRAW_OK)
string(FIND "${Q1990_NATIVE_VERIFY}" "q1990NativeLod" Q1990_LOD_FLAG_OK)
string(FIND "${Q1990_NATIVE_VERIFY}"
       "cached-3x3-collision+7x7-terrain+Level4-LOD" Q1990_MODE_OK)
string(FIND "${Q1990_Q4_SOURCE}" "Q16.27: 7 = A B C" Q1990_LABEL_OK)
if(Q1990_COLLISION_CACHE_OK EQUAL -1 OR Q1990_COLLISION_STORAGE_OK EQUAL -1 OR
   Q1990_TERRAIN_RADIUS_OK EQUAL -1 OR Q1990_TERRAIN_RETAIN_OK EQUAL -1 OR
   Q1990_LOD_LOAD_OK EQUAL -1 OR Q1990_LOD_DRAW_OK EQUAL -1 OR
   Q1990_LOD_FLAG_OK EQUAL -1 OR Q1990_MODE_OK EQUAL -1 OR
   Q1990_LABEL_OK EQUAL -1)
    message(FATAL_ERROR "Q16.27 persistent-collision/native-LOD verification failed")
endif()

message(STATUS "Q16.27 enabled: transformed-REFR collision cache + actual-centred retained 7x7 LAND runway + visible native Level4 LOD + reduced stream log pressure")
