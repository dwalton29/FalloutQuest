# Q16.19: make the Q16.18 streamer keep up with locomotion and remove the two
# large synchronous cache-thrash spikes proven by the device log.
#
# Device evidence from Q16.18:
#   - a generation took ~30-40 seconds because it did not start until XCLC cross;
#   - collision rebuilt/re-read the same NIF bhk models every generation;
#   - LAND rescanned Fallout3.esm and re-uploaded the same terrain DDS textures.
#
# Q16.19 therefore:
#   1) predicts the next XCLC from smoothed locomotion and starts its 3x3 window
#      while the player is still in the current cell;
#   2) raises only the cheap CPU placement budget (GPU remains 1 shape/frame);
#   3) keeps parsed collision NIF/metadata caches alive across window recentres;
#   4) builds a one-time LAND record offset index and reuses decoded LAND cells;
#   5) keeps terrain DDS GL textures cached across terrain-window rebuilds.
#
# Original XTEL render origin, Q16.15 door cache, Q16.16 font baseline, and the
# Q16.17 authored 4096-unit grid semantics are unchanged.

# -----------------------------------------------------------------------------
# A. Collision: Q16.18 recreated these three caches inside every full collision
#    initialization. Make them static so overlapping/new windows reuse parsed bhk
#    shapes and packed Havok metadata instead of reopening the same NIFs.
# -----------------------------------------------------------------------------
set(Q1910_COLLISION_CACHE_OLD [==[
    std::unordered_set<uint32_t> seenRefs;
    std::unordered_map<std::string, std::vector<Fo3NifCollisionShapeQ6F>> modelCache;
    std::unordered_map<std::string, Fo3PackedMetadataMapQ714> metadataCacheQ714;
    std::unordered_set<std::string> noCollisionModelsQ78A;
]==])
set(Q1910_COLLISION_CACHE_NEW [==[
    std::unordered_set<uint32_t> seenRefs;
    // Q16.19: these describe immutable NIF assets, not a CELL instance. Keep
    // them process-lifetime so a recentered 3x3 reuses already parsed bhk data.
    static std::unordered_map<std::string, std::vector<Fo3NifCollisionShapeQ6F>> modelCache;
    static std::unordered_map<std::string, Fo3PackedMetadataMapQ714> metadataCacheQ714;
    static std::unordered_set<std::string> noCollisionModelsQ78A;
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1910_COLLISION_CACHE_OLD}" Q1910_COLLISION_CACHE_POS)
if(Q1910_COLLISION_CACHE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.19 could not find active collision model-cache declarations")
endif()
string(REPLACE "${Q1910_COLLISION_CACHE_OLD}" "${Q1910_COLLISION_CACHE_NEW}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# -----------------------------------------------------------------------------
# B. LAND: index LAND record offsets once per WRLD and cache decoded terrain
#    cells. The first Wasteland index is normally built while the Megaton child
#    worldspace inherits its parent LAND behind the loading screen; later stream
#    shifts seek directly to the 3 entering LAND records instead of walking all
#    ~38k Wasteland LAND records again.
# -----------------------------------------------------------------------------
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "DecodeVnmlQ1010" Q1910_VNML_OK)
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "DecodeVclrQ1060" Q1910_VCLR_OK)
if(Q1910_VNML_OK EQUAL -1 OR Q1910_VCLR_OK EQUAL -1)
    message(FATAL_ERROR "Q16.19 expected final Q10.6 LAND VNML/VCLR decoder stack")
endif()

set(Q1910_LAND_CACHE_HELPERS [==[
struct Q1910LandRecordIndex {
    uint32_t ownerCellFormId = 0u;
    uint32_t landFormId = 0u;
    uint32_t recordFlags = 0u;
    uint32_t storedSize = 0u;
    uint64_t payloadOffset = 0u;
};

std::unordered_map<uint32_t, std::vector<CellInfoQ75>> gQ1910WorldspaceCells;
std::unordered_map<uint32_t, std::vector<Q1910LandRecordIndex>> gQ1910LandIndex;
std::unordered_map<uint64_t, Fo3TerrainCellQ76> gQ1910DecodedLand;

uint64_t Q1910LandCacheKey(uint32_t worldspaceFormId, uint32_t landFormId) {
    return (static_cast<uint64_t>(worldspaceFormId) << 32u) |
           static_cast<uint64_t>(landFormId);
}

const std::vector<CellInfoQ75>* Q1910GetWorldspaceCells(uint32_t worldspaceFormId) {
    const auto cached = gQ1910WorldspaceCells.find(worldspaceFormId);
    if (cached != gQ1910WorldspaceCells.end()) return &cached->second;

    std::vector<CellInfoQ75> cells;
    if (!DiscoverWorldspaceCellsQ75(worldspaceFormId, cells)) return nullptr;
    auto inserted = gQ1910WorldspaceCells.emplace(worldspaceFormId, std::move(cells));
    Q75_LOGI("Q16.19 CELL INDEX READY: worldspace=%08X cells=%zu cacheEntries=%zu",
             worldspaceFormId, inserted.first->second.size(), gQ1910WorldspaceCells.size());
    return &inserted.first->second;
}

uint32_t Q1910OwningCell(const std::vector<GroupFrameQ75>& groups) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (it->type == 6u || it->type == 8u || it->type == 9u || it->type == 10u) {
            return it->label;
        }
    }
    return 0u;
}

const std::vector<Q1910LandRecordIndex>* Q1910GetLandIndex(uint32_t worldspaceFormId) {
    const auto cached = gQ1910LandIndex.find(worldspaceFormId);
    if (cached != gQ1910LandIndex.end()) return &cached->second;

    FILE* file = std::fopen(ESM_PATH_Q75, "rb");
    if (!file) return nullptr;
    const int64_t fileSize = FileSizeQ75(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE_Q75)) {
        std::fclose(file);
        return nullptr;
    }

    std::vector<GroupFrameQ75> groups;
    std::vector<Q1910LandRecordIndex> index;
    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + HEADER_SIZE_Q75 > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE_Q75]{};
        if (!ReadExactQ75(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32Q75(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE_Q75 ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrameQ75{offset + sizeField,
                                           ReadLe32Q75(header + 8u),
                                           ReadLe32Q75(header + 12u)});
            continue;
        }

        const uint64_t payloadEnd = offset + HEADER_SIZE_Q75 + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (InWorldspaceQ75(groups, worldspaceFormId) &&
            std::memcmp(header, "LAND", 4u) == 0) {
            const uint32_t owner = Q1910OwningCell(groups);
            if (owner != 0u) {
                Q1910LandRecordIndex entry;
                entry.ownerCellFormId = owner;
                entry.landFormId = ReadLe32Q75(header + 12u);
                entry.recordFlags = ReadLe32Q75(header + 8u);
                entry.storedSize = sizeField;
                entry.payloadOffset = offset + HEADER_SIZE_Q75;
                index.push_back(entry);
            }
        }
        if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
    }
    std::fclose(file);

    if (index.empty()) return nullptr;
    auto inserted = gQ1910LandIndex.emplace(worldspaceFormId, std::move(index));
    Q75_LOGI("Q16.19 LAND INDEX READY: worldspace=%08X LAND=%zu cacheWorldspaces=%zu",
             worldspaceFormId, inserted.first->second.size(), gQ1910LandIndex.size());
    return &inserted.first->second;
}

bool CollectSelectedLandQ1910(uint32_t worldspaceFormId,
                              const std::unordered_set<uint32_t>& selectedCells,
                              const std::unordered_map<uint32_t, CellInfoQ75>& cellsById,
                              std::vector<Fo3TerrainCellQ76>& out) {
    out.clear();
    const std::vector<Q1910LandRecordIndex>* index = Q1910GetLandIndex(worldspaceFormId);
    if (!index) return false;

    FILE* file = nullptr;
    size_t cacheHits = 0u;
    size_t decoded = 0u;
    size_t missingVhgt = 0u;

    for (const Q1910LandRecordIndex& entry : *index) {
        if (selectedCells.find(entry.ownerCellFormId) == selectedCells.end()) continue;
        const auto cellIt = cellsById.find(entry.ownerCellFormId);
        if (cellIt == cellsById.end() || !cellIt->second.hasGrid) continue;

        const uint64_t cacheKey = Q1910LandCacheKey(worldspaceFormId, entry.landFormId);
        const auto cached = gQ1910DecodedLand.find(cacheKey);
        if (cached != gQ1910DecodedLand.end()) {
            out.push_back(cached->second);
            ++cacheHits;
            continue;
        }

        if (!file) {
            file = std::fopen(ESM_PATH_Q75, "rb");
            if (!file) return false;
        }
        if (fseeko(file, static_cast<off_t>(entry.payloadOffset), SEEK_SET) != 0) continue;
        std::vector<uint8_t> payload;
        if (!ReadPayloadQ75(file, entry.storedSize, entry.recordFlags, payload)) continue;

        Fo3TerrainCellQ76 terrain;
        terrain.cellFormId = entry.ownerCellFormId;
        terrain.landFormId = entry.landFormId;
        terrain.gridX = cellIt->second.gridX;
        terrain.gridY = cellIt->second.gridY;
        bool haveVhgt = false;
        bool haveVnml = false;
        bool haveVclr = false;
        std::vector<float> vertexColorsQ1060;
        WalkSubrecordsQ75(payload.data(), payload.size(),
                          [&](const char* type, const uint8_t* bytes, uint32_t subSize) {
            if (std::memcmp(type, "VHGT", 4u) == 0 && !haveVhgt) {
                haveVhgt = DecodeVhgtQ76(bytes, subSize, terrain.heights);
            } else if (std::memcmp(type, "VNML", 4u) == 0 && !haveVnml) {
                haveVnml = DecodeVnmlQ1010(bytes, subSize, terrain.normals);
            } else if (std::memcmp(type, "VCLR", 4u) == 0 && !haveVclr) {
                haveVclr = DecodeVclrQ1060(bytes, subSize, vertexColorsQ1060);
            }
        });
        if (!haveVhgt) {
            ++missingVhgt;
            continue;
        }
        if (haveVclr) gLandVertexColorsQ1060[entry.landFormId] = std::move(vertexColorsQ1060);

        ++decoded;
        gQ1910DecodedLand.emplace(cacheKey, terrain);
        out.push_back(std::move(terrain));
    }

    if (file) std::fclose(file);
    Q75_LOGI("Q16.19 LAND CACHE: worldspace=%08X selectedCells=%zu returned=%zu cacheHits=%zu decodedNow=%zu missingVHGT=%zu indexLAND=%zu decodedCache=%zu",
             worldspaceFormId, selectedCells.size(), out.size(), cacheHits, decoded,
             missingVhgt, index->size(), gQ1910DecodedLand.size());
    return !out.empty();
}

]==])

set(Q1910_LAND_COLLECT_MARKER "bool CollectSelectedLandQ76(")
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1910_LAND_COLLECT_MARKER}" Q1910_LAND_MARKER_POS)
if(Q1910_LAND_MARKER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.19 could not find LAND collector insertion point")
endif()
string(REPLACE "${Q1910_LAND_COLLECT_MARKER}"
       "${Q1910_LAND_CACHE_HELPERS}${Q1910_LAND_COLLECT_MARKER}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

# Route only the two live terrain collection callsites through the indexed cache;
# keep the old full scanner compiled as a fallback/reference implementation.
string(REPLACE
    "CollectSelectedLandQ76(worldspaceFormId, selectedCells, cellsById, localTerrain);"
    "CollectSelectedLandQ1910(worldspaceFormId, selectedCells, cellsById, localTerrain);"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
string(REPLACE
    "CollectSelectedLandQ76(parentInfo.formId, selectedParentCells, parentCellsById, inherited);"
    "CollectSelectedLandQ1910(parentInfo.formId, selectedParentCells, parentCellsById, inherited);"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

# Cache CELL/XCLC discovery too. It is immutable ESM metadata and was another full
# file walk on every Q16.18 terrain recenter.
set(Q1910_LOAD_CELLS_OLD [==[
    std::vector<CellInfoQ75> cells;
    if (!DiscoverWorldspaceCellsQ75(worldspaceFormId, cells)) {
        Q75_LOGE("Q7.6 LAND LOAD FAILED: worldspace=%08X reason=discover-cells", worldspaceFormId);
        return false;
    }

    std::vector<const CellInfoQ75*> gridCells;
]==])
set(Q1910_LOAD_CELLS_NEW [==[
    const std::vector<CellInfoQ75>* q1910Cells = Q1910GetWorldspaceCells(worldspaceFormId);
    if (!q1910Cells) {
        Q75_LOGE("Q7.6 LAND LOAD FAILED: worldspace=%08X reason=discover-cells", worldspaceFormId);
        return false;
    }
    const std::vector<CellInfoQ75>& cells = *q1910Cells;

    std::vector<const CellInfoQ75*> gridCells;
]==])
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1910_LOAD_CELLS_OLD}" Q1910_LOAD_CELLS_POS)
if(Q1910_LOAD_CELLS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.19 could not find live terrain CELL discovery block")
endif()
string(REPLACE "${Q1910_LOAD_CELLS_OLD}" "${Q1910_LOAD_CELLS_NEW}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

set(Q1910_PARENT_CELLS_OLD [==[
    std::vector<CellInfoQ75> parentCells;
    if (!DiscoverWorldspaceCellsQ75(parentInfo.formId, parentCells)) {
        Q75_LOGW("Q7.9 LAND PARENT FAILED: child=%08X parent=%08X reason=discover-parent-cells",
                 childWorldspaceFormId, parentInfo.formId);
        return 0u;
    }

    std::unordered_map<uint32_t, CellInfoQ75> parentCellsById;
]==])
set(Q1910_PARENT_CELLS_NEW [==[
    const std::vector<CellInfoQ75>* q1910ParentCells = Q1910GetWorldspaceCells(parentInfo.formId);
    if (!q1910ParentCells) {
        Q75_LOGW("Q7.9 LAND PARENT FAILED: child=%08X parent=%08X reason=discover-parent-cells",
                 childWorldspaceFormId, parentInfo.formId);
        return 0u;
    }
    const std::vector<CellInfoQ75>& parentCells = *q1910ParentCells;

    std::unordered_map<uint32_t, CellInfoQ75> parentCellsById;
]==])
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1910_PARENT_CELLS_OLD}" Q1910_PARENT_CELLS_POS)
if(Q1910_PARENT_CELLS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.19 could not find parent LAND CELL discovery block")
endif()
string(REPLACE "${Q1910_PARENT_CELLS_OLD}" "${Q1910_PARENT_CELLS_NEW}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

# VCLR is keyed by immutable LAND FormID. Keep it with the decoded LAND cache
# instead of discarding/redecoding colours every window shift.
string(REPLACE "    gLandVertexColorsQ1060.clear();\n" ""
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

# -----------------------------------------------------------------------------
# C. LAND renderer: persist GL terrain DDS textures. Q16.18's terrain shutdown
#    correctly retired per-window VBO/VAOs but also deleted every texture, forcing
#    DDS decode + mip upload of the same landscape textures at every recenter.
# -----------------------------------------------------------------------------
set(Q1910_TERRAIN_BATCH_MARKER "std::vector<Q711TerrainBatch> q711TerrainBatches;")
set(Q1910_TERRAIN_BATCH_NEW [==[
std::vector<Q711TerrainBatch> q711TerrainBatches;
std::unordered_map<std::string, GLuint> q1910TerrainTextureCache;
constexpr size_t Q1910_TERRAIN_TEXTURE_CACHE_LIMIT = 256u;
]==])
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1910_TERRAIN_BATCH_MARKER}" Q1910_TERRAIN_BATCH_POS)
if(Q1910_TERRAIN_BATCH_POS EQUAL -1)
    message(FATAL_ERROR "Q16.19 could not find terrain batch globals")
endif()
string(REPLACE "${Q1910_TERRAIN_BATCH_MARKER}" "${Q1910_TERRAIN_BATCH_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "        if (batch.texture && batch.ownsTexture) glDeleteTextures(1, &batch.texture);"
    "        // Q16.19: texture is process-cache owned; recenter retires only window geometry."
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    std::unordered_map<std::string, GLuint> uploadedByPath;"
    "    auto& uploadedByPath = q1910TerrainTextureCache;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "            } else if (textureUploads < Q713_MAX_REAL_TEXTURES) {"
    "            } else if (textureUploads < Q713_MAX_REAL_TEXTURES &&\n                       q1910TerrainTextureCache.size() < Q1910_TERRAIN_TEXTURE_CACHE_LIMIT) {"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "                    gpu.ownsTexture = true;"
    "                    gpu.ownsTexture = false; // Q16.19 shared process-lifetime terrain texture cache"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Persist the final post-Q10.6 terrain sources now that their variables contain
# the Q16.19 cache layer. The transition TU already includes these generated paths.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

# -----------------------------------------------------------------------------
# D. Renderer streamer: start one window ahead based on smoothed player motion.
#    Keep GPU at 1 shape/frame (Quest-safe); CPU transformations are cheap enough
#    to do four placements/frame. This attacks latency without creating a new GPU
#    upload spike.
# -----------------------------------------------------------------------------
set(Q1910_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
file(READ "${Q1910_NATIVE_FILE}" Q1910_NATIVE_SOURCE)
string(REPLACE
    "constexpr size_t Q1900_CPU_PLACEMENTS_PER_FRAME = 1u;"
    "constexpr size_t Q1900_CPU_PLACEMENTS_PER_FRAME = 4u; // Q16.19 cheap CPU catch-up"
    Q1910_NATIVE_SOURCE "${Q1910_NATIVE_SOURCE}")

set(Q1910_UPDATE_OLD [==[
void UpdateFo3ExteriorStreamingQ1890(float virtualHeadX, float virtualHeadZ) {
    Q1900AdvanceStream();
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

    Q6H_LOGI("Q16.18 CELL CROSS: from=(%d,%d) to=(%d,%d) virtualHead=(%.3f %.3f) game=(%.2f %.2f) hysteresis=%.1f",
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             targetGridX, targetGridY,
             virtualHeadX, virtualHeadZ, gameX, gameY,
             Q1890_BOUNDARY_HYSTERESIS);
    Q1900BeginStream(gameX, gameY, targetGridX, targetGridY);
}
]==])
set(Q1910_UPDATE_NEW [==[
void UpdateFo3ExteriorStreamingQ1890(float virtualHeadX, float virtualHeadZ) {
    Q1900AdvanceStream();
    if (!gExteriorStreamingActiveQ1890 || gExteriorStreamBusyQ1890 ||
        IsFo3LoadingVisibleQ1700()) return;

    const float gameX = gExteriorOriginXQ1890 +
                        virtualHeadX * FO3_UNITS_PER_METRE;
    const float gameY = gExteriorOriginYQ1890 +
                        (SCENE_FORWARD - virtualHeadZ) * FO3_UNITS_PER_METRE;
    const int32_t actualGridX = static_cast<int32_t>(
        std::floor(gameX / Q1890_EXTERIOR_CELL_SIZE));
    const int32_t actualGridY = static_cast<int32_t>(
        std::floor(gameY / Q1890_EXTERIOR_CELL_SIZE));

    // Q16.19 predicts where the player is travelling and recentres the render
    // window one CELL ahead. This gives the 1-shape/frame uploader almost an
    // entire 4096-unit cell of lead time instead of starting at the boundary.
    static uint32_t q1910MotionWorldspace = 0u;
    static bool q1910MotionReady = false;
    static float q1910PreviousGameX = 0.0f;
    static float q1910PreviousGameY = 0.0f;
    static float q1910VelocityX = 0.0f;
    static float q1910VelocityY = 0.0f;
    if (!q1910MotionReady || q1910MotionWorldspace != gExteriorWorldspaceQ1890) {
        q1910MotionWorldspace = gExteriorWorldspaceQ1890;
        q1910MotionReady = true;
        q1910PreviousGameX = gameX;
        q1910PreviousGameY = gameY;
        q1910VelocityX = 0.0f;
        q1910VelocityY = 0.0f;
        return;
    }

    const float frameDx = gameX - q1910PreviousGameX;
    const float frameDy = gameY - q1910PreviousGameY;
    q1910PreviousGameX = gameX;
    q1910PreviousGameY = gameY;
    q1910VelocityX = q1910VelocityX * 0.86f + frameDx * 0.14f;
    q1910VelocityY = q1910VelocityY * 0.86f + frameDy * 0.14f;

    constexpr float Q1910_PREFETCH_SPEED = 1.15f; // game units/frame; filters head jitter
    int32_t desiredGridX = gExteriorWindowGridXQ1890;
    int32_t desiredGridY = gExteriorWindowGridYQ1890;
    bool predictive = false;
    if (std::fabs(q1910VelocityX) >= Q1910_PREFETCH_SPEED) {
        desiredGridX = actualGridX + (q1910VelocityX > 0.0f ? 1 : -1);
        predictive = true;
    } else {
        desiredGridX = actualGridX;
    }
    if (std::fabs(q1910VelocityY) >= Q1910_PREFETCH_SPEED) {
        desiredGridY = actualGridY + (q1910VelocityY > 0.0f ? 1 : -1);
        predictive = true;
    } else {
        desiredGridY = actualGridY;
    }

    // When stationary after an ahead-of-player prefetch, keep that valid window
    // rather than immediately pulling it backward. If the player has actually
    // escaped the current 3x3 safety margin, force a catch-up recenter.
    if (!predictive) {
        const int dx = std::abs(actualGridX - gExteriorWindowGridXQ1890);
        const int dy = std::abs(actualGridY - gExteriorWindowGridYQ1890);
        if (dx <= 1 && dy <= 1) return;
        desiredGridX = actualGridX;
        desiredGridY = actualGridY;
    }

    if (desiredGridX == gExteriorWindowGridXQ1890 &&
        desiredGridY == gExteriorWindowGridYQ1890) return;

    const float selectionGameX =
        (static_cast<float>(desiredGridX) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
    const float selectionGameY =
        (static_cast<float>(desiredGridY) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;

    Q6H_LOGI("Q16.19 PREFETCH: window=(%d,%d) actual=(%d,%d) desired=(%d,%d) velocity=(%.2f %.2f) playerGame=(%.2f %.2f) selectionCenter=(%.2f %.2f)",
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             actualGridX, actualGridY, desiredGridX, desiredGridY,
             q1910VelocityX, q1910VelocityY, gameX, gameY,
             selectionGameX, selectionGameY);
    Q1900BeginStream(selectionGameX, selectionGameY,
                     desiredGridX, desiredGridY);
}
]==])
string(FIND "${Q1910_NATIVE_SOURCE}" "${Q1910_UPDATE_OLD}" Q1910_UPDATE_POS)
if(Q1910_UPDATE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.19 could not find Q16.18 boundary-only update function")
endif()
string(REPLACE "${Q1910_UPDATE_OLD}" "${Q1910_UPDATE_NEW}"
       Q1910_NATIVE_SOURCE "${Q1910_NATIVE_SOURCE}")

# Stream log version bump. This intentionally changes only diagnostics in the
# native renderer; the visible build label is handled below.
string(REPLACE "Q16.18" "Q16.19"
       Q1910_NATIVE_SOURCE "${Q1910_NATIVE_SOURCE}")
file(WRITE "${Q1910_NATIVE_FILE}" "${Q1910_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# E. Visible Q16.19 label; HUD metrics/door wording remain otherwise untouched.
# -----------------------------------------------------------------------------
set(Q1910_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
file(READ "${Q1910_Q4_FILE}" Q1910_Q4_SOURCE)
set(Q1910_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.18: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x7Fu); // Q16.18: 8 = A B C D E F G
]==])
set(Q1910_LABEL_NEW [==[
        q1600Digit(q1600X, 0x06u); // Q16.19: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x6Fu); // Q16.19: 9 = A B C D F G
]==])
string(FIND "${Q1910_Q4_SOURCE}" "${Q1910_LABEL_OLD}" Q1910_LABEL_POS)
if(Q1910_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.19 could not find Q16.18 build-label digits")
endif()
string(REPLACE "${Q1910_LABEL_OLD}" "${Q1910_LABEL_NEW}"
       Q1910_Q4_SOURCE "${Q1910_Q4_SOURCE}")
string(REPLACE "Q16.18 BUILD LABEL:" "Q16.19 BUILD LABEL:"
       Q1910_Q4_SOURCE "${Q1910_Q4_SOURCE}")
string(REPLACE "text=Q16.18 anchor=left-hand" "text=Q16.19 anchor=left-hand"
       Q1910_Q4_SOURCE "${Q1910_Q4_SOURCE}")
string(REPLACE "Q16.18 AUTHORED DOOR FACING" "Q16.19 AUTHORED DOOR FACING"
       Q1910_Q4_SOURCE "${Q1910_Q4_SOURCE}")
file(WRITE "${Q1910_Q4_FILE}" "${Q1910_Q4_SOURCE}")

# Configure-time verification. Fail rather than silently shipping a partially
# patched cache/prefetch build if one of the mature generated sources drifts.
string(FIND "${Q74_COLLISION_SOURCE}" "static std::unordered_map<std::string, std::vector<Fo3NifCollisionShapeQ6F>> modelCache" Q1910_COLLISION_OK)
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "Q16.19 LAND INDEX READY:" Q1910_LAND_INDEX_OK)
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "CollectSelectedLandQ1910(worldspaceFormId" Q1910_LAND_ROUTE_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "q1910TerrainTextureCache" Q1910_TEX_CACHE_OK)
string(FIND "${Q1910_NATIVE_SOURCE}" "Q16.19 PREFETCH:" Q1910_PREFETCH_OK)
string(FIND "${Q1910_NATIVE_SOURCE}" "Q1900_CPU_PLACEMENTS_PER_FRAME = 4u" Q1910_CPU_BUDGET_OK)
string(FIND "${Q1910_Q4_SOURCE}" "Q16.19: 9 = A B C D F G" Q1910_LABEL_OK)
if(Q1910_COLLISION_OK EQUAL -1 OR Q1910_LAND_INDEX_OK EQUAL -1 OR
   Q1910_LAND_ROUTE_OK EQUAL -1 OR Q1910_TEX_CACHE_OK EQUAL -1 OR
   Q1910_PREFETCH_OK EQUAL -1 OR Q1910_CPU_BUDGET_OK EQUAL -1 OR
   Q1910_LABEL_OK EQUAL -1)
    message(FATAL_ERROR "Q16.19 stream cache/prefetch verification failed")
endif()

message(STATUS "Q16.19 exterior streamer enabled: predictive XCLC prefetch + persistent collision assets + indexed/cached LAND + persistent terrain textures")
