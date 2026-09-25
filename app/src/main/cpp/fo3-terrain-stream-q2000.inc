// Q20: persistent per-CELL LAND residency.
//
// This layer deliberately sits *above* the legacy Q7.6/Q7.20 LAND decoder and
// renderer.  The legacy path is still used for cold boot, but once exterior
// gameplay begins Q20 keeps the 7x7 LAND runway resident by CELL:
//   resident 7x7 -> prepare one entering 7-cell strip off-thread ->
//   stage textures/VBOs -> publish strip -> retire outgoing strip.
//
// The critical invariant is that the old terrain remains live until every
// incoming CELL for the next one-cell window is GPU-ready.  No call here asks
// InitializeFo3TerrainRenderQ76() to rebuild a 49-cell window.

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <thread>
#include <unordered_set>
#include <utility>

namespace {

constexpr int Q2000_TERRAIN_RADIUS = 3;
constexpr size_t Q2000_GPU_BYTES_PER_UPDATE = 128u * 1024u;
constexpr uint64_t Q2000_GPU_BUDGET_US = 1500u;
constexpr uint64_t Q2000_GPU_SPIKE_US = 8000u;

struct Q2000PreparedTexture {
    std::string path;
    Fo3RgbaTexture image;
    bool normal = false;
};

struct Q2000CpuCell {
    int32_t gridX = 0;
    int32_t gridY = 0;
    bool hasTerrain = false;
    Fo3TerrainCellQ76 ground;
    std::vector<Q711CpuBatch> batches;
    size_t baseTriangles = 0u;
    size_t alphaTriangles = 0u;
    size_t alphaLayers = 0u;
};

struct Q2000WorkerTask {
    uint64_t serial = 0u;
    uint32_t worldspace = 0u;
    int32_t targetCentreX = 0;
    int32_t targetCentreY = 0;
    float arrivalX = 0.0f;
    float arrivalY = 0.0f;
    float arrivalZ = 0.0f;
    float sceneForward = 0.0f;
    float floorY = 0.0f;
    float unitsPerMetre = 70.0f;
    std::vector<std::pair<int32_t,int32_t>> entering;
    std::unordered_set<std::string> cachedDiffuse;
    std::unordered_set<std::string> cachedNormals;
    std::vector<Q2000CpuCell> cells;
    std::vector<Q2000PreparedTexture> textures;
    uint64_t loadUs = 0u;
    uint64_t buildUs = 0u;
    uint64_t textureUs = 0u;
    uint64_t workerUs = 0u;
    bool success = false;
    std::atomic<bool> ready{false};
};

struct Q2000TextureUploadState {
    bool active = false;
    GLuint texture = 0u;
    size_t textureIndex = 0u;
    int nextRow = 0;
    uint64_t maxStepUs = 0u;
};

struct Q2000VboUploadState {
    bool active = false;
    GLuint vbo = 0u;
    size_t offsetBytes = 0u;
    size_t totalBytes = 0u;
    uint64_t maxStepUs = 0u;
};

struct Q2000DeferredGeometry {
    GLuint vao = 0u;
    GLuint vbo = 0u;
};

uint64_t gQ2000Serial = 1u;
uint32_t gQ2000Worldspace = 0u;
bool gQ2000ResidentValid = false;
int32_t gQ2000ResidentCentreX = 0;
int32_t gQ2000ResidentCentreY = 0;
std::unordered_set<uint64_t> gQ2000WindowKeys;
std::unordered_map<uint64_t, Fo3TerrainCellQ76> gQ2000GroundCells;

std::shared_ptr<Q2000WorkerTask> gQ2000Worker;
std::thread gQ2000WorkerThread;

bool gQ2000TargetActive = false;
int32_t gQ2000TargetCentreX = 0;
int32_t gQ2000TargetCentreY = 0;
std::unordered_set<uint64_t> gQ2000TargetKeys;
std::vector<Q2000CpuCell> gQ2000PendingCells;
std::vector<Q2000PreparedTexture> gQ2000PendingTextures;
size_t gQ2000TextureCursor = 0u;
size_t gQ2000CellCursor = 0u;
size_t gQ2000BatchCursor = 0u;
Q2000TextureUploadState gQ2000TextureUpload;
Q2000VboUploadState gQ2000VboUpload;
std::vector<Q711TerrainBatch> gQ2000CurrentCellBatches;
std::deque<Q2000DeferredGeometry> gQ2000DeferredGeometry;
uint64_t gQ2000TargetWorkerUs = 0u;
uint64_t gQ2000TargetLoadUs = 0u;
uint64_t gQ2000TargetBuildUs = 0u;
uint64_t gQ2000TargetTextureUs = 0u;
uint64_t gQ2000TargetGpuUs = 0u;
uint64_t gQ2000TargetMaxGpuStepUs = 0u;

uint64_t Q2000ElapsedUs(const std::chrono::steady_clock::time_point& start) {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count());
}

std::unordered_set<uint64_t> Q2000WindowKeys(int32_t centreX, int32_t centreY) {
    std::unordered_set<uint64_t> keys;
    keys.reserve(64u);
    for (int dy = -Q2000_TERRAIN_RADIUS; dy <= Q2000_TERRAIN_RADIUS; ++dy) {
        for (int dx = -Q2000_TERRAIN_RADIUS; dx <= Q2000_TERRAIN_RADIUS; ++dx) {
            keys.insert(Q711GridKey(centreX + dx, centreY + dy));
        }
    }
    return keys;
}

void Q2000DeleteBatchGeometry(Q711TerrainBatch& batch) {
    if (batch.vbo) glDeleteBuffers(1, &batch.vbo);
    if (batch.vao) glDeleteVertexArrays(1, &batch.vao);
    batch.vbo = 0u;
    batch.vao = 0u;
}

void Q2000QueueBatchGeometry(Q711TerrainBatch& batch) {
    if (batch.vao || batch.vbo) {
        gQ2000DeferredGeometry.push_back({batch.vao, batch.vbo});
    }
    batch.vao = 0u;
    batch.vbo = 0u;
}

void Q2000DrainDeferredGeometry(bool all) {
    const auto started = std::chrono::steady_clock::now();
    constexpr uint64_t budgetUs = 600u;
    size_t drained = 0u;
    while (!gQ2000DeferredGeometry.empty()) {
        const Q2000DeferredGeometry item = gQ2000DeferredGeometry.front();
        gQ2000DeferredGeometry.pop_front();
        if (item.vbo) glDeleteBuffers(1, &item.vbo);
        if (item.vao) glDeleteVertexArrays(1, &item.vao);
        ++drained;
        if (!all && Q2000ElapsedUs(started) >= budgetUs) break;
    }
    if (drained > 0u && !all) {
        Q76B_LOGI("Q20 TERRAIN DELETE DRAIN: drained=%zu remaining=%zu budgetUs=%llu",
                  drained, gQ2000DeferredGeometry.size(),
                  static_cast<unsigned long long>(budgetUs));
    }
}

void Q2000RefreshStats() {
    q76bTerrainCells = gQ2000GroundCells.size();
    q76bTriangles = 0u;
    q718AlphaTriangles = 0u;
    q711RealTextureBatches = 0u;
    q711FallbackBatches = 0u;
    q718AlphaBatches = 0u;
    for (const Q711TerrainBatch& batch : q711TerrainBatches) {
        const size_t triangles =
            static_cast<size_t>(std::max<GLsizei>(0, batch.vertexCount)) / 3u;
        if (batch.alphaLayer) {
            q718AlphaTriangles += triangles;
            ++q718AlphaBatches;
        } else {
            q76bTriangles += triangles;
            if (batch.realTexture) ++q711RealTextureBatches;
            else ++q711FallbackBatches;
        }
        if (batch.alphaLayer && batch.realTexture) ++q711RealTextureBatches;
    }
}

void Q2000Bootstrap(int32_t actualGridX, int32_t actualGridY) {
    if (gQ2000ResidentValid || !q76bReady || q76bWorldspace == 0u) return;

    gQ2000Worldspace = q76bWorldspace;
    gQ2000ResidentCentreX = actualGridX;
    gQ2000ResidentCentreY = actualGridY;
    gQ2000WindowKeys = Q2000WindowKeys(actualGridX, actualGridY);
    gQ2000GroundCells.clear();

    for (const Fo3TerrainCellQ76& cell : GetFo3TerrainQ76()) {
        const uint64_t key = Q711GridKey(cell.gridX, cell.gridY);
        if (gQ2000WindowKeys.find(key) != gQ2000WindowKeys.end()) {
            gQ2000GroundCells[key] = cell;
        }
    }

    gQ2000ResidentValid = true;
    Q2000RefreshStats();
    Q76B_LOGI("Q20 TERRAIN BOOTSTRAP: centre=(%d,%d) windowCells=%zu groundCells=%zu gpuBatches=%zu legacyColdBoot=1 perCellOwnership=1",
              actualGridX, actualGridY, gQ2000WindowKeys.size(),
              gQ2000GroundCells.size(), q711TerrainBatches.size());
}

bool Q2000BuildCpuCell(
        const Fo3TerrainCellQ76& source,
        const std::vector<float>& heights,
        float arrivalX, float arrivalY, float arrivalZ,
        float sceneForward, float floorY, float unitsPerMetre,
        Q2000CpuCell& out) {
    out = {};
    out.gridX = source.gridX;
    out.gridY = source.gridY;
    out.hasTerrain = heights.size() == Q76B_HEIGHT_COUNT;
    if (!out.hasTerrain) return true;

    out.ground = source;
    out.ground.heights = heights;

    std::unordered_map<std::string, size_t> batchByTexture;
    auto getBatch = [&](const std::string& texturePath,
                        bool alphaLayer,
                        int layerOrder) -> Q711CpuBatch& {
        const std::string key =
            (alphaLayer ? "A" : "B") + std::to_string(layerOrder) + "|" +
            (texturePath.empty() ? std::string("<fallback>") : texturePath);
        const auto found = batchByTexture.find(key);
        if (found != batchByTexture.end()) return out.batches[found->second];
        const size_t index = out.batches.size();
        batchByTexture.emplace(key, index);
        Q711CpuBatch batch;
        batch.gridX = source.gridX;
        batch.gridY = source.gridY;
        batch.texturePath = texturePath;
        batch.normalPath = ResolveFo3TerrainNormalTextureQ1060(texturePath);
        batch.specularEnabled = IsFo3TerrainSpecularEnabledQ1060(texturePath);
        batch.alphaLayer = alphaLayer;
        batch.layerOrder = layerOrder;
        out.batches.push_back(std::move(batch));
        return out.batches.back();
    };

    const float originX = static_cast<float>(source.gridX) * Q76B_CELL_SIZE;
    const float originY = static_cast<float>(source.gridY) * Q76B_CELL_SIZE;

    auto gameHeight = [&](int x, int y) -> float {
        return heights[static_cast<size_t>(y * Q76B_LAND_SIDE + x)];
    };
    auto point = [&](int x, int y) -> Q76BV3 {
        const float gameX = originX + static_cast<float>(x) * Q76B_VERTEX_SPACING;
        const float gameY = originY + static_cast<float>(y) * Q76B_VERTEX_SPACING;
        return Q76BToVr(gameX, gameY, gameHeight(x, y),
                        arrivalX, arrivalY, arrivalZ,
                        sceneForward, floorY, unitsPerMetre);
    };

    const bool hasAuthoredNormals =
        source.normals.size() == Q76B_HEIGHT_COUNT * 3u;
    auto authoredNormal = [&](int x, int y) -> Q76BV3 {
        if (!hasAuthoredNormals) return {};
        const size_t index =
            static_cast<size_t>(y * Q76B_LAND_SIDE + x) * 3u;
        return Q76BNormalize({
            source.normals[index + 0u],
            source.normals[index + 2u],
           -source.normals[index + 1u]});
    };

    const std::vector<float>& childColors =
        GetFo3TerrainVertexColorsQ1060(source.landFormId);
    const uint32_t parentLand = GetFo3MaterialParentLandQ1270(source.landFormId);
    const std::vector<float>& parentColors =
        GetFo3TerrainVertexColorsQ1060(parentLand);
    const std::vector<float>& colors =
        childColors.size() == Q76B_HEIGHT_COUNT * 3u ? childColors :
        (parentColors.size() == Q76B_HEIGHT_COUNT * 3u ? parentColors : childColors);
    const bool hasColors = colors.size() == Q76B_HEIGHT_COUNT * 3u;
    auto authoredColor = [&](int x, int y) -> Q76BV3 {
        if (!hasColors) return {1.0f, 1.0f, 1.0f};
        const size_t index =
            static_cast<size_t>(y * Q76B_LAND_SIDE + x) * 3u;
        return {colors[index], colors[index + 1u], colors[index + 2u]};
    };

    std::string quadrantTextures[4];
    for (int q = 0; q < 4; ++q)
        quadrantTextures[q] = ResolveFo3TerrainBaseTextureQ711(source.landFormId, q);

    if (parentLand != 0u) {
        for (int q = 0; q < 4; ++q) {
            if (!quadrantTextures[q].empty()) continue;
            quadrantTextures[q] =
                ResolveFo3TerrainBaseTextureQ711(parentLand, q);
        }
    }

    const uint8_t hideMask = source.forceHideMask;
    for (int y = 0; y < Q76B_LAND_QUADS; ++y) {
        for (int x = 0; x < Q76B_LAND_QUADS; ++x) {
            const int quadrant = Q711QuadrantForQuad(x, y);
            if ((hideMask & static_cast<uint8_t>(1u << quadrant)) != 0u)
                continue;

            Q711CpuBatch& batch =
                getBatch(quadrantTextures[quadrant], false, -1);
            const float gameX0 =
                originX + static_cast<float>(x) * Q76B_VERTEX_SPACING;
            const float gameY0 =
                originY + static_cast<float>(y) * Q76B_VERTEX_SPACING;
            const float gameX1 = gameX0 + Q76B_VERTEX_SPACING;
            const float gameY1 = gameY0 + Q76B_VERTEX_SPACING;
            const float u0 = gameX0 / Q711_TEXTURE_REPEAT_GAME_UNITS;
            const float v0 = gameY0 / Q711_TEXTURE_REPEAT_GAME_UNITS;
            const float u1 = gameX1 / Q711_TEXTURE_REPEAT_GAME_UNITS;
            const float v1 = gameY1 / Q711_TEXTURE_REPEAT_GAME_UNITS;

            const Q76BV3 p00 = point(x, y);
            const Q76BV3 p10 = point(x + 1, y);
            const Q76BV3 p01 = point(x, y + 1);
            const Q76BV3 p11 = point(x + 1, y + 1);
            Q718PushTriangle(batch.vertices,
                             p00,u0,v0,p10,u1,v0,p11,u1,v1,
                             1.0f,1.0f,1.0f,
                             authoredNormal(x,y), authoredNormal(x+1,y),
                             authoredNormal(x+1,y+1),
                             authoredColor(x,y), authoredColor(x+1,y),
                             authoredColor(x+1,y+1));
            Q718PushTriangle(batch.vertices,
                             p00,u0,v0,p11,u1,v1,p01,u0,v1,
                             1.0f,1.0f,1.0f,
                             authoredNormal(x,y), authoredNormal(x+1,y+1),
                             authoredNormal(x,y+1),
                             authoredColor(x,y), authoredColor(x+1,y+1),
                             authoredColor(x,y+1));
            out.baseTriangles += 2u;
        }
    }

    const std::vector<Q718TerrainAlphaLayer>& childAlpha =
        GetFo3TerrainAlphaLayersQ718(source.landFormId);
    std::vector<Q718TerrainAlphaLayer> mergedAlpha;
    const std::vector<Q718TerrainAlphaLayer>* alpha = &childAlpha;
    if (parentLand != 0u) {
        const std::vector<Q718TerrainAlphaLayer>& parentAlpha =
            GetFo3TerrainAlphaLayersQ718(parentLand);
        mergedAlpha.reserve(parentAlpha.size() + childAlpha.size());
        mergedAlpha.insert(mergedAlpha.end(), parentAlpha.begin(), parentAlpha.end());
        constexpr uint32_t childBias = 1024u;
        for (const Q718TerrainAlphaLayer& sourceLayer : childAlpha) {
            Q718TerrainAlphaLayer layer = sourceLayer;
            layer.layer = static_cast<uint16_t>(std::min<uint32_t>(
                static_cast<uint32_t>(layer.layer) + childBias, 65535u));
            mergedAlpha.push_back(std::move(layer));
        }
        alpha = &mergedAlpha;
    }

    for (const Q718TerrainAlphaLayer& layer : *alpha) {
        if (layer.quadrant > 3u || layer.authoredVertices == 0u) continue;
        if ((hideMask & static_cast<uint8_t>(1u << layer.quadrant)) != 0u)
            continue;
        const std::string texturePath =
            ResolveFo3TerrainAlphaTextureQ718(layer);
        if (texturePath.empty()) continue;

        ++out.alphaLayers;
        const int xStart = (layer.quadrant & 1u) ? 16 : 0;
        const int yStart = (layer.quadrant & 2u) ? 16 : 0;
        for (int localY = 0; localY < 16; ++localY) {
            for (int localX = 0; localX < 16; ++localX) {
                const size_t i00 = static_cast<size_t>(localY * 17 + localX);
                const size_t i10 = i00 + 1u;
                const size_t i01 = i00 + 17u;
                const size_t i11 = i01 + 1u;
                const float a00 = layer.opacity[i00];
                const float a10 = layer.opacity[i10];
                const float a01 = layer.opacity[i01];
                const float a11 = layer.opacity[i11];
                if (std::max({a00,a10,a01,a11}) <= Q718_ALPHA_EPS) continue;

                const int x = xStart + localX;
                const int y = yStart + localY;
                const float gameX0 =
                    originX + static_cast<float>(x) * Q76B_VERTEX_SPACING;
                const float gameY0 =
                    originY + static_cast<float>(y) * Q76B_VERTEX_SPACING;
                const float gameX1 = gameX0 + Q76B_VERTEX_SPACING;
                const float gameY1 = gameY0 + Q76B_VERTEX_SPACING;
                const float u0 = gameX0 / Q711_TEXTURE_REPEAT_GAME_UNITS;
                const float v0 = gameY0 / Q711_TEXTURE_REPEAT_GAME_UNITS;
                const float u1 = gameX1 / Q711_TEXTURE_REPEAT_GAME_UNITS;
                const float v1 = gameY1 / Q711_TEXTURE_REPEAT_GAME_UNITS;
                const Q76BV3 p00 = point(x,y);
                const Q76BV3 p10 = point(x+1,y);
                const Q76BV3 p01 = point(x,y+1);
                const Q76BV3 p11 = point(x+1,y+1);
                Q711CpuBatch& batch =
                    getBatch(texturePath, true, static_cast<int>(layer.layer));
                Q718PushTriangle(batch.vertices,
                                 p00,u0,v0,p10,u1,v0,p11,u1,v1,
                                 a00,a10,a11,
                                 authoredNormal(x,y), authoredNormal(x+1,y),
                                 authoredNormal(x+1,y+1),
                                 authoredColor(x,y), authoredColor(x+1,y),
                                 authoredColor(x+1,y+1));
                Q718PushTriangle(batch.vertices,
                                 p00,u0,v0,p11,u1,v1,p01,u0,v1,
                                 a00,a11,a01,
                                 authoredNormal(x,y), authoredNormal(x+1,y+1),
                                 authoredNormal(x,y+1),
                                 authoredColor(x,y), authoredColor(x+1,y+1),
                                 authoredColor(x,y+1));
                out.alphaTriangles += 2u;
            }
        }
    }

    std::stable_sort(out.batches.begin(), out.batches.end(),
                     [](const Q711CpuBatch& a, const Q711CpuBatch& b) {
        if (a.alphaLayer != b.alphaLayer) return !a.alphaLayer;
        if (a.alphaLayer && a.layerOrder != b.layerOrder)
            return a.layerOrder < b.layerOrder;
        return false;
    });
    return true;
}

void Q2000PrepareTexture(
        const std::string& path, bool normal,
        const std::shared_ptr<Q2000WorkerTask>& task,
        std::unordered_set<std::string>& seen) {
    if (path.empty()) return;
    const std::string dedupe = (normal ? "N|" : "D|") + path;
    if (!seen.insert(dedupe).second) return;
    if (normal) {
        if (task->cachedNormals.find(path) != task->cachedNormals.end()) return;
    } else {
        if (task->cachedDiffuse.find(path) != task->cachedDiffuse.end()) return;
    }

    Fo3RgbaTexture image;
    if (!LoadFalloutTextureRgba(path, image) ||
        image.width <= 0 || image.height <= 0 || image.rgba.empty()) {
        Q76B_LOGW("Q20 TERRAIN TEXTURE CPU MISS: type=%s path=%s",
                  normal ? "normal" : "diffuse", path.c_str());
        return;
    }
    Q2000PreparedTexture prepared;
    prepared.path = path;
    prepared.image = std::move(image);
    prepared.normal = normal;
    task->textures.push_back(std::move(prepared));
}

void Q2000RunWorker(const std::shared_ptr<Q2000WorkerTask>& task) {
    const auto workerStarted = std::chrono::steady_clock::now();

    const auto loadStarted = std::chrono::steady_clock::now();
    const float selectionX =
        (static_cast<float>(task->targetCentreX) + 0.5f) * Q76B_CELL_SIZE;
    const float selectionY =
        (static_cast<float>(task->targetCentreY) + 0.5f) * Q76B_CELL_SIZE;
    SetFo3TerrainSelectionOverrideQ1890(true, selectionX, selectionY);
    const bool loaded = LoadFo3TerrainQ76(
        task->worldspace, task->arrivalX, task->arrivalY, task->arrivalZ);
    SetFo3TerrainSelectionOverrideQ1890(false, 0.0f, 0.0f);
    task->loadUs = Q2000ElapsedUs(loadStarted);

    if (!loaded) {
        task->workerUs = Q2000ElapsedUs(workerStarted);
        task->ready.store(true, std::memory_order_release);
        return;
    }

    const auto buildStarted = std::chrono::steady_clock::now();
    const std::vector<Fo3TerrainCellQ76> terrain = GetFo3TerrainQ76();
    std::unordered_map<uint64_t, size_t> byGrid;
    std::vector<std::vector<float>> stitched;
    byGrid.reserve(terrain.size() * 2u);
    stitched.reserve(terrain.size());
    for (size_t i = 0u; i < terrain.size(); ++i) {
        stitched.push_back(terrain[i].heights);
        if (terrain[i].heights.size() == Q76B_HEIGHT_COUNT)
            byGrid[Q711GridKey(terrain[i].gridX, terrain[i].gridY)] = i;
    }

    // Reproduce the legacy seam policy off-thread.  Only the entering CELL
    // geometry is emitted, but its heightfield is derived from the same 7x7
    // neighborhood as the legacy renderer.
    for (size_t i = 0u; i < terrain.size(); ++i) {
        if (stitched[i].size() != Q76B_HEIGHT_COUNT) continue;
        const Fo3TerrainCellQ76& cell = terrain[i];
        auto stitch = [&](size_t j, bool east) {
            if (j >= terrain.size() || stitched[j].size() != Q76B_HEIGHT_COUNT)
                return;
            for (int p = 0; p < Q76B_LAND_SIDE; ++p) {
                const size_t a = east
                    ? static_cast<size_t>(p * Q76B_LAND_SIDE + 32)
                    : static_cast<size_t>(32 * Q76B_LAND_SIDE + p);
                const size_t b = east
                    ? static_cast<size_t>(p * Q76B_LAND_SIDE)
                    : static_cast<size_t>(p);
                const float delta = std::fabs(stitched[i][a] - stitched[j][b]);
                if (delta <= Q718_MAX_SEAM_STITCH_DELTA) {
                    const float average = (stitched[i][a] + stitched[j][b]) * 0.5f;
                    stitched[i][a] = average;
                    stitched[j][b] = average;
                }
            }
        };
        const auto east = byGrid.find(Q711GridKey(cell.gridX + 1, cell.gridY));
        if (east != byGrid.end()) stitch(east->second, true);
        const auto north = byGrid.find(Q711GridKey(cell.gridX, cell.gridY + 1));
        if (north != byGrid.end()) stitch(north->second, false);
    }

    task->cells.reserve(task->entering.size());
    for (const auto& coord : task->entering) {
        Q2000CpuCell cell;
        cell.gridX = coord.first;
        cell.gridY = coord.second;
        const auto found = byGrid.find(Q711GridKey(coord.first, coord.second));
        if (found != byGrid.end()) {
            const size_t index = found->second;
            Q2000BuildCpuCell(
                terrain[index], stitched[index],
                task->arrivalX, task->arrivalY, task->arrivalZ,
                task->sceneForward, task->floorY, task->unitsPerMetre,
                cell);
        }
        task->cells.push_back(std::move(cell));
    }
    task->buildUs = Q2000ElapsedUs(buildStarted);

    const auto textureStarted = std::chrono::steady_clock::now();
    std::unordered_set<std::string> seen;
    for (const Q2000CpuCell& cell : task->cells) {
        for (const Q711CpuBatch& batch : cell.batches) {
            Q2000PrepareTexture(batch.texturePath, false, task, seen);
            Q2000PrepareTexture(batch.normalPath, true, task, seen);
        }
    }
    task->textureUs = Q2000ElapsedUs(textureStarted);
    task->success = true;
    task->workerUs = Q2000ElapsedUs(workerStarted);
    task->ready.store(true, std::memory_order_release);
}

void Q2000JoinFinishedWorker() {
    if (gQ2000WorkerThread.joinable()) gQ2000WorkerThread.join();
}

void Q2000ConsumeWorker() {
    if (!gQ2000Worker ||
        !gQ2000Worker->ready.load(std::memory_order_acquire)) return;

    const auto task = gQ2000Worker;
    Q2000JoinFinishedWorker();
    gQ2000Worker.reset();

    if (task->serial != gQ2000Serial || !task->success ||
        task->worldspace != gQ2000Worldspace) {
        Q76B_LOGW("Q20 TERRAIN WORKER STALE: target=(%d,%d) serial=%llu currentSerial=%llu success=%d workerUs=%llu action=discard",
                  task->targetCentreX, task->targetCentreY,
                  static_cast<unsigned long long>(task->serial),
                  static_cast<unsigned long long>(gQ2000Serial),
                  task->success ? 1 : 0,
                  static_cast<unsigned long long>(task->workerUs));
        return;
    }

    gQ2000TargetActive = true;
    gQ2000TargetCentreX = task->targetCentreX;
    gQ2000TargetCentreY = task->targetCentreY;
    gQ2000TargetKeys = Q2000WindowKeys(
        gQ2000TargetCentreX, gQ2000TargetCentreY);
    gQ2000PendingCells = std::move(task->cells);
    gQ2000PendingTextures = std::move(task->textures);
    gQ2000TextureCursor = 0u;
    gQ2000CellCursor = 0u;
    gQ2000BatchCursor = 0u;
    gQ2000TextureUpload = {};
    gQ2000VboUpload = {};
    gQ2000CurrentCellBatches.clear();
    gQ2000TargetWorkerUs = task->workerUs;
    gQ2000TargetLoadUs = task->loadUs;
    gQ2000TargetBuildUs = task->buildUs;
    gQ2000TargetTextureUs = task->textureUs;
    gQ2000TargetGpuUs = 0u;
    gQ2000TargetMaxGpuStepUs = 0u;

    Q76B_LOGI("Q20 TERRAIN CPU READY: targetCentre=(%d,%d) enteringCells=%zu preparedCells=%zu textures=%zu loadUs=%llu buildUs=%llu textureUs=%llu workerUs=%llu oldTerrainLive=1",
              gQ2000TargetCentreX, gQ2000TargetCentreY,
              task->entering.size(), gQ2000PendingCells.size(),
              gQ2000PendingTextures.size(),
              static_cast<unsigned long long>(gQ2000TargetLoadUs),
              static_cast<unsigned long long>(gQ2000TargetBuildUs),
              static_cast<unsigned long long>(gQ2000TargetTextureUs),
              static_cast<unsigned long long>(gQ2000TargetWorkerUs));
}

bool Q2000AdvanceOneTexture(size_t& bytesThisUpdate) {
    if (gQ2000TextureCursor >= gQ2000PendingTextures.size()) return true;
    Q2000PreparedTexture& prepared =
        gQ2000PendingTextures[gQ2000TextureCursor];
    auto& cache = prepared.normal
        ? q1810TerrainNormalTextureCache
        : q1910TerrainTextureCache;
    const size_t limit = prepared.normal
        ? Q1810_TERRAIN_NORMAL_CACHE_LIMIT
        : Q1910_TERRAIN_TEXTURE_CACHE_LIMIT;

    if (!gQ2000TextureUpload.active) {
        if (cache.find(prepared.path) != cache.end()) {
            ++gQ2000TextureCursor;
            return true;
        }
        if (cache.size() >= limit) {
            Q76B_LOGW("Q20 TERRAIN TEXTURE CACHE FULL: type=%s path=%s entries=%zu limit=%zu",
                      prepared.normal ? "normal" : "diffuse",
                      prepared.path.c_str(), cache.size(), limit);
            ++gQ2000TextureCursor;
            return true;
        }

        const auto stepStarted = std::chrono::steady_clock::now();
        GLuint texture = 0u;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        Q1070ApplyTerrainAnisotropy();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     prepared.image.width, prepared.image.height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        const uint64_t stepUs = Q2000ElapsedUs(stepStarted);
        gQ2000TargetMaxGpuStepUs =
            std::max(gQ2000TargetMaxGpuStepUs, stepUs);
        gQ2000TextureUpload.active = true;
        gQ2000TextureUpload.texture = texture;
        gQ2000TextureUpload.textureIndex = gQ2000TextureCursor;
        gQ2000TextureUpload.nextRow = 0;
        gQ2000TextureUpload.maxStepUs = stepUs;
        if (stepUs >= Q2000_GPU_SPIKE_US) {
            Q76B_LOGW("Q20 TERRAIN GPU STEP SPIKE: stage=texture-allocate path=%s us=%llu",
                      prepared.path.c_str(),
                      static_cast<unsigned long long>(stepUs));
        }
    }

    const size_t rowBytes =
        static_cast<size_t>(prepared.image.width) * 4u;
    if (rowBytes == 0u) {
        ++gQ2000TextureCursor;
        gQ2000TextureUpload = {};
        return true;
    }
    const size_t available =
        bytesThisUpdate < Q2000_GPU_BYTES_PER_UPDATE
            ? Q2000_GPU_BYTES_PER_UPDATE - bytesThisUpdate : 0u;
    size_t rows = std::max<size_t>(1u, available / rowBytes);
    rows = std::min<size_t>(
        rows,
        static_cast<size_t>(prepared.image.height -
                            gQ2000TextureUpload.nextRow));

    const auto stepStarted = std::chrono::steady_clock::now();
    glBindTexture(GL_TEXTURE_2D, gQ2000TextureUpload.texture);
    const uint8_t* pixels = prepared.image.rgba.data() +
        static_cast<size_t>(gQ2000TextureUpload.nextRow) * rowBytes;
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, gQ2000TextureUpload.nextRow,
                    prepared.image.width, static_cast<GLsizei>(rows),
                    GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    const uint64_t stepUs = Q2000ElapsedUs(stepStarted);
    gQ2000TextureUpload.maxStepUs =
        std::max(gQ2000TextureUpload.maxStepUs, stepUs);
    gQ2000TargetMaxGpuStepUs =
        std::max(gQ2000TargetMaxGpuStepUs, stepUs);
    bytesThisUpdate += rows * rowBytes;
    gQ2000TextureUpload.nextRow += static_cast<int>(rows);
    if (stepUs >= Q2000_GPU_SPIKE_US) {
        Q76B_LOGW("Q20 TERRAIN GPU STEP SPIKE: stage=texture-chunk path=%s us=%llu rows=%zu",
                  prepared.path.c_str(),
                  static_cast<unsigned long long>(stepUs), rows);
    }

    if (gQ2000TextureUpload.nextRow < prepared.image.height) return false;

    const auto mipStarted = std::chrono::steady_clock::now();
    glGenerateMipmap(GL_TEXTURE_2D);
    const uint64_t mipUs = Q2000ElapsedUs(mipStarted);
    gQ2000TargetMaxGpuStepUs =
        std::max(gQ2000TargetMaxGpuStepUs, mipUs);
    const GLenum error = glGetError();
    if (error == GL_NO_ERROR) {
        cache[prepared.path] = gQ2000TextureUpload.texture;
        Q76B_LOGI("Q20 TERRAIN TEXTURE READY: type=%s path=%s size=%dx%d maxStepUs=%llu mipUs=%llu staged=1",
                  prepared.normal ? "normal" : "diffuse",
                  prepared.path.c_str(),
                  prepared.image.width, prepared.image.height,
                  static_cast<unsigned long long>(
                      gQ2000TextureUpload.maxStepUs),
                  static_cast<unsigned long long>(mipUs));
    } else {
        Q76B_LOGW("Q20 TERRAIN TEXTURE GPU MISS: type=%s path=%s glError=0x%X",
                  prepared.normal ? "normal" : "diffuse",
                  prepared.path.c_str(), error);
        if (gQ2000TextureUpload.texture)
            glDeleteTextures(1, &gQ2000TextureUpload.texture);
    }
    gQ2000TextureUpload = {};
    ++gQ2000TextureCursor;
    return true;
}

bool Q2000AdvanceOneBatch(
        Q2000CpuCell& cell,
        Q711CpuBatch& cpu,
        size_t& bytesThisUpdate) {
    GLuint diffuse = 0u;
    GLuint normal = 0u;
    if (!cpu.texturePath.empty()) {
        const auto found = q1910TerrainTextureCache.find(cpu.texturePath);
        if (found != q1910TerrainTextureCache.end()) diffuse = found->second;
    }
    if (!cpu.normalPath.empty()) {
        const auto found = q1810TerrainNormalTextureCache.find(cpu.normalPath);
        if (found != q1810TerrainNormalTextureCache.end()) normal = found->second;
    }

    // An unresolved alpha texture must not become a brown fallback layer.
    if (cpu.alphaLayer && diffuse == 0u) return true;
    if (cpu.vertices.empty()) return true;

    if (!gQ2000VboUpload.active) {
        const auto stepStarted = std::chrono::steady_clock::now();
        GLuint vbo = 0u;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        const size_t bytes = cpu.vertices.size() * sizeof(float);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(bytes),
                     nullptr, GL_STATIC_DRAW);
        const uint64_t stepUs = Q2000ElapsedUs(stepStarted);
        gQ2000TargetMaxGpuStepUs =
            std::max(gQ2000TargetMaxGpuStepUs, stepUs);
        gQ2000VboUpload.active = true;
        gQ2000VboUpload.vbo = vbo;
        gQ2000VboUpload.offsetBytes = 0u;
        gQ2000VboUpload.totalBytes = bytes;
        gQ2000VboUpload.maxStepUs = stepUs;
        if (stepUs >= Q2000_GPU_SPIKE_US) {
            Q76B_LOGW("Q20 TERRAIN GPU STEP SPIKE: stage=vbo-allocate cell=(%d,%d) bytes=%zu us=%llu",
                      cell.gridX, cell.gridY, bytes,
                      static_cast<unsigned long long>(stepUs));
        }
    }

    const size_t available =
        bytesThisUpdate < Q2000_GPU_BYTES_PER_UPDATE
            ? Q2000_GPU_BYTES_PER_UPDATE - bytesThisUpdate : 0u;
    const size_t remaining =
        gQ2000VboUpload.totalBytes - gQ2000VboUpload.offsetBytes;
    const size_t chunk =
        std::min(remaining, std::max<size_t>(1u, available));
    const auto stepStarted = std::chrono::steady_clock::now();
    glBindBuffer(GL_ARRAY_BUFFER, gQ2000VboUpload.vbo);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        static_cast<GLintptr>(gQ2000VboUpload.offsetBytes),
        static_cast<GLsizeiptr>(chunk),
        reinterpret_cast<const uint8_t*>(cpu.vertices.data()) +
            gQ2000VboUpload.offsetBytes);
    const uint64_t stepUs = Q2000ElapsedUs(stepStarted);
    gQ2000VboUpload.maxStepUs =
        std::max(gQ2000VboUpload.maxStepUs, stepUs);
    gQ2000TargetMaxGpuStepUs =
        std::max(gQ2000TargetMaxGpuStepUs, stepUs);
    gQ2000VboUpload.offsetBytes += chunk;
    bytesThisUpdate += chunk;
    if (stepUs >= Q2000_GPU_SPIKE_US) {
        Q76B_LOGW("Q20 TERRAIN GPU STEP SPIKE: stage=vbo-chunk cell=(%d,%d) bytes=%zu us=%llu",
                  cell.gridX, cell.gridY, chunk,
                  static_cast<unsigned long long>(stepUs));
    }

    if (gQ2000VboUpload.offsetBytes < gQ2000VboUpload.totalBytes)
        return false;

    const auto finalizeStarted = std::chrono::steady_clock::now();
    Q711TerrainBatch gpu;
    gpu.gridX = cell.gridX;
    gpu.gridY = cell.gridY;
    gpu.texturePath = cpu.texturePath;
    gpu.normalPath = cpu.normalPath;
    gpu.specularEnabled = cpu.specularEnabled;
    gpu.alphaLayer = cpu.alphaLayer;
    gpu.layerOrder = cpu.layerOrder;
    gpu.texture = diffuse;
    gpu.normalTexture = normal;
    gpu.realTexture = diffuse != 0u;
    gpu.realNormal = normal != 0u;
    gpu.ownsTexture = false;
    gpu.ownsNormal = false;
    gpu.vbo = gQ2000VboUpload.vbo;
    glGenVertexArrays(1, &gpu.vao);
    glBindVertexArray(gpu.vao);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    constexpr GLsizei stride = static_cast<GLsizei>(12 * sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(8 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(9 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    gpu.vertexCount =
        static_cast<GLsizei>(cpu.vertices.size() / 12u);
    gQ2000CurrentCellBatches.push_back(std::move(gpu));
    const uint64_t finalizeUs = Q2000ElapsedUs(finalizeStarted);
    gQ2000TargetMaxGpuStepUs =
        std::max(gQ2000TargetMaxGpuStepUs, finalizeUs);

    gQ2000VboUpload.vbo = 0u;
    gQ2000VboUpload = {};
    return true;
}

void Q2000PublishCurrentCell(Q2000CpuCell& cell) {
    for (Q711TerrainBatch& batch : gQ2000CurrentCellBatches)
        q711TerrainBatches.push_back(std::move(batch));
    const size_t batches = gQ2000CurrentCellBatches.size();
    gQ2000CurrentCellBatches.clear();

    if (cell.hasTerrain) {
        gQ2000GroundCells[Q711GridKey(cell.gridX, cell.gridY)] =
            std::move(cell.ground);
    }

    Q76B_LOGI("Q20 TERRAIN CELL READY: cell=(%d,%d) hasLAND=%d batches=%zu baseTriangles=%zu alphaTriangles=%zu oldTerrainLive=1",
              cell.gridX, cell.gridY, cell.hasTerrain ? 1 : 0,
              batches, cell.baseTriangles, cell.alphaTriangles);
}

void Q2000CommitTarget() {
    const size_t oldWindowCells = gQ2000WindowKeys.size();
    size_t retiredBatches = 0u;
    size_t write = 0u;
    for (size_t read = 0u; read < q711TerrainBatches.size(); ++read) {
        Q711TerrainBatch& batch = q711TerrainBatches[read];
        const uint64_t key = Q711GridKey(batch.gridX, batch.gridY);
        if (gQ2000TargetKeys.find(key) == gQ2000TargetKeys.end()) {
            Q2000QueueBatchGeometry(batch);
            ++retiredBatches;
            continue;
        }
        if (write != read) {
            q711TerrainBatches[write] = std::move(batch);
            batch.vao = 0u;
            batch.vbo = 0u;
        }
        ++write;
    }
    q711TerrainBatches.resize(write);

    for (auto it = gQ2000GroundCells.begin();
         it != gQ2000GroundCells.end();) {
        if (gQ2000TargetKeys.find(it->first) == gQ2000TargetKeys.end())
            it = gQ2000GroundCells.erase(it);
        else
            ++it;
    }

    size_t retained = 0u;
    for (uint64_t key : gQ2000TargetKeys)
        if (gQ2000WindowKeys.find(key) != gQ2000WindowKeys.end()) ++retained;
    const size_t entering = gQ2000TargetKeys.size() - retained;
    const size_t retiredCells =
        oldWindowCells > retained ? oldWindowCells - retained : 0u;

    gQ2000WindowKeys = gQ2000TargetKeys;
    gQ2000ResidentCentreX = gQ2000TargetCentreX;
    gQ2000ResidentCentreY = gQ2000TargetCentreY;
    gQ2000TargetKeys.clear();
    gQ2000TargetActive = false;
    gQ2000PendingCells.clear();
    gQ2000PendingTextures.clear();
    gQ2000TextureCursor = gQ2000CellCursor = gQ2000BatchCursor = 0u;
    Q2000RefreshStats();

    Q76B_LOGI("Q20 TERRAIN WINDOW READY: centre=(%d,%d) retainedCells=%zu enteringCells=%zu retiredCells=%zu retiredBatches=%zu liveBatches=%zu groundCells=%zu workerUs=%llu loadUs=%llu buildUs=%llu textureUs=%llu gpuUs=%llu maxGpuStepUs=%llu full49Rebuild=0 oneCellShift=1 oldTerrainLiveUntilComplete=1",
              gQ2000ResidentCentreX, gQ2000ResidentCentreY,
              retained, entering, retiredCells, retiredBatches,
              q711TerrainBatches.size(), gQ2000GroundCells.size(),
              static_cast<unsigned long long>(gQ2000TargetWorkerUs),
              static_cast<unsigned long long>(gQ2000TargetLoadUs),
              static_cast<unsigned long long>(gQ2000TargetBuildUs),
              static_cast<unsigned long long>(gQ2000TargetTextureUs),
              static_cast<unsigned long long>(gQ2000TargetGpuUs),
              static_cast<unsigned long long>(gQ2000TargetMaxGpuStepUs));
}

void Q2000AdvanceGpu() {
    Q2000DrainDeferredGeometry(false);
    if (!gQ2000TargetActive) return;

    const auto updateStarted = std::chrono::steady_clock::now();
    size_t bytesThisUpdate = 0u;

    while (gQ2000TextureCursor < gQ2000PendingTextures.size()) {
        const bool complete = Q2000AdvanceOneTexture(bytesThisUpdate);
        if (!complete ||
            bytesThisUpdate >= Q2000_GPU_BYTES_PER_UPDATE ||
            Q2000ElapsedUs(updateStarted) >= Q2000_GPU_BUDGET_US) {
            gQ2000TargetGpuUs += Q2000ElapsedUs(updateStarted);
            return;
        }
    }

    while (gQ2000CellCursor < gQ2000PendingCells.size()) {
        Q2000CpuCell& cell = gQ2000PendingCells[gQ2000CellCursor];
        if (!cell.hasTerrain || cell.batches.empty()) {
            Q2000PublishCurrentCell(cell);
            ++gQ2000CellCursor;
            gQ2000BatchCursor = 0u;
            continue;
        }

        while (gQ2000BatchCursor < cell.batches.size()) {
            Q711CpuBatch& batch = cell.batches[gQ2000BatchCursor];
            const bool complete =
                Q2000AdvanceOneBatch(cell, batch, bytesThisUpdate);
            if (!complete) {
                gQ2000TargetGpuUs += Q2000ElapsedUs(updateStarted);
                return;
            }
            ++gQ2000BatchCursor;
            if (bytesThisUpdate >= Q2000_GPU_BYTES_PER_UPDATE ||
                Q2000ElapsedUs(updateStarted) >= Q2000_GPU_BUDGET_US) {
                gQ2000TargetGpuUs += Q2000ElapsedUs(updateStarted);
                return;
            }
        }

        Q2000PublishCurrentCell(cell);
        ++gQ2000CellCursor;
        gQ2000BatchCursor = 0u;
        if (Q2000ElapsedUs(updateStarted) >= Q2000_GPU_BUDGET_US) {
            gQ2000TargetGpuUs += Q2000ElapsedUs(updateStarted);
            return;
        }
    }

    gQ2000TargetGpuUs += Q2000ElapsedUs(updateStarted);
    Q2000CommitTarget();
}

void Q2000LaunchWorker(
        uint32_t worldspaceFormId,
        int32_t targetCentreX, int32_t targetCentreY,
        float arrivalX, float arrivalY, float arrivalZ,
        float sceneForward, float floorY, float unitsPerMetre) {
    auto task = std::make_shared<Q2000WorkerTask>();
    task->serial = gQ2000Serial;
    task->worldspace = worldspaceFormId;
    task->targetCentreX = targetCentreX;
    task->targetCentreY = targetCentreY;
    task->arrivalX = arrivalX;
    task->arrivalY = arrivalY;
    task->arrivalZ = arrivalZ;
    task->sceneForward = sceneForward;
    task->floorY = floorY;
    task->unitsPerMetre = unitsPerMetre;

    const auto target = Q2000WindowKeys(targetCentreX, targetCentreY);
    for (int dy = -Q2000_TERRAIN_RADIUS; dy <= Q2000_TERRAIN_RADIUS; ++dy) {
        for (int dx = -Q2000_TERRAIN_RADIUS; dx <= Q2000_TERRAIN_RADIUS; ++dx) {
            const int32_t x = targetCentreX + dx;
            const int32_t y = targetCentreY + dy;
            const uint64_t key = Q711GridKey(x, y);
            if (gQ2000WindowKeys.find(key) == gQ2000WindowKeys.end())
                task->entering.emplace_back(x, y);
        }
    }

    for (const auto& entry : q1910TerrainTextureCache)
        task->cachedDiffuse.insert(entry.first);
    for (const auto& entry : q1810TerrainNormalTextureCache)
        task->cachedNormals.insert(entry.first);

    gQ2000Worker = task;
    Q76B_LOGI("Q20 TERRAIN CPU START: fromCentre=(%d,%d) targetCentre=(%d,%d) retainedCells=%zu enteringCells=%zu thread=serialized-asset-worker full49GpuRebuild=0",
              gQ2000ResidentCentreX, gQ2000ResidentCentreY,
              targetCentreX, targetCentreY,
              target.size() - task->entering.size(),
              task->entering.size());
    gQ2000WorkerThread =
        std::thread([task]() { Q2000RunWorker(task); });
}

} // namespace

void ResetFo3TerrainStreamingQ2000() {
    ++gQ2000Serial;
    if (gQ2000WorkerThread.joinable()) gQ2000WorkerThread.join();
    gQ2000Worker.reset();

    if (gQ2000TextureUpload.texture) {
        glDeleteTextures(1, &gQ2000TextureUpload.texture);
    }
    gQ2000TextureUpload = {};
    if (gQ2000VboUpload.vbo) {
        glDeleteBuffers(1, &gQ2000VboUpload.vbo);
    }
    gQ2000VboUpload = {};
    for (Q711TerrainBatch& batch : gQ2000CurrentCellBatches)
        Q2000DeleteBatchGeometry(batch);
    gQ2000CurrentCellBatches.clear();
    Q2000DrainDeferredGeometry(true);

    gQ2000Worldspace = 0u;
    gQ2000ResidentValid = false;
    gQ2000ResidentCentreX = gQ2000ResidentCentreY = 0;
    gQ2000WindowKeys.clear();
    gQ2000GroundCells.clear();
    gQ2000TargetActive = false;
    gQ2000TargetKeys.clear();
    gQ2000PendingCells.clear();
    gQ2000PendingTextures.clear();
    gQ2000TextureCursor = gQ2000CellCursor = gQ2000BatchCursor = 0u;
}

bool IsFo3TerrainStreamingCpuBusyQ2000() {
    return gQ2000Worker != nullptr;
}

bool IsFo3TerrainStreamingBusyQ2000() {
    return gQ2000Worker != nullptr || gQ2000TargetActive;
}

bool GetFo3TerrainResidentCentreQ2000(
        int32_t* outGridX, int32_t* outGridY) {
    if (!gQ2000ResidentValid) return false;
    if (outGridX) *outGridX = gQ2000ResidentCentreX;
    if (outGridY) *outGridY = gQ2000ResidentCentreY;
    return true;
}

const Fo3TerrainCellQ76* FindFo3TerrainGroundCellQ2000(
        int32_t gridX, int32_t gridY) {
    const auto found =
        gQ2000GroundCells.find(Q711GridKey(gridX, gridY));
    if (found != gQ2000GroundCells.end()) return &found->second;

    // Cold boot / pre-Q20 bootstrap fallback.
    const std::vector<Fo3TerrainCellQ76>& legacy = GetFo3TerrainQ76();
    for (const Fo3TerrainCellQ76& cell : legacy)
        if (cell.gridX == gridX && cell.gridY == gridY) return &cell;
    return nullptr;
}

size_t GetFo3TerrainGroundCellCountQ2000() {
    return gQ2000ResidentValid
        ? gQ2000GroundCells.size()
        : GetFo3TerrainQ76().size();
}

void UpdateFo3TerrainStreamingQ2000(
        uint32_t worldspaceFormId,
        int32_t actualGridX, int32_t actualGridY,
        float arrivalX, float arrivalY, float arrivalZ,
        float sceneForward, float floorY, float unitsPerMetre,
        bool allowCpuWorker) {
    if (!q76bReady || worldspaceFormId == 0u) return;

    if (!gQ2000ResidentValid) Q2000Bootstrap(actualGridX, actualGridY);
    if (!gQ2000ResidentValid) return;

    if (gQ2000Worldspace != worldspaceFormId) {
        ResetFo3TerrainStreamingQ2000();
        Q2000Bootstrap(actualGridX, actualGridY);
        if (!gQ2000ResidentValid) return;
    }

    Q2000ConsumeWorker();
    Q2000AdvanceGpu();

    if (gQ2000Worker || gQ2000TargetActive || !allowCpuWorker) return;
    if (actualGridX == gQ2000ResidentCentreX &&
        actualGridY == gQ2000ResidentCentreY) return;

    // Never jump a multi-cell catch-up.  Move one axis by one CELL, so a normal
    // update retains exactly 42/49 cells and prepares only one seven-cell strip.
    const int dx = actualGridX - gQ2000ResidentCentreX;
    const int dy = actualGridY - gQ2000ResidentCentreY;
    int32_t nextX = gQ2000ResidentCentreX;
    int32_t nextY = gQ2000ResidentCentreY;
    if (std::abs(dx) >= std::abs(dy) && dx != 0)
        nextX += dx > 0 ? 1 : -1;
    else if (dy != 0)
        nextY += dy > 0 ? 1 : -1;

    Q2000LaunchWorker(
        worldspaceFormId, nextX, nextY,
        arrivalX, arrivalY, arrivalZ,
        sceneForward, floorY, unitsPerMetre);
}
