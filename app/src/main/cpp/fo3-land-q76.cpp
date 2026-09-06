// Q7.6 is textually included by fo3-cell-spawn-q74.cpp immediately after the
// Q7.5 worldspace parser. Reuse that parser's proven ESM4/group helpers so LAND
// stays in exactly the same selected exterior CELL set as the visible REFRs.

namespace {

constexpr int Q76_LAND_VERTS = 33;
constexpr int Q76_LAND_QUADS = 32;
constexpr float Q76_CELL_SIZE = 4096.0f;
constexpr float Q76_VERTEX_SPACING = Q76_CELL_SIZE / static_cast<float>(Q76_LAND_QUADS);
constexpr float Q76_HEIGHT_SCALE = 8.0f;
constexpr size_t Q76_VHGT_DELTA_COUNT = static_cast<size_t>(Q76_LAND_VERTS * Q76_LAND_VERTS);
constexpr size_t Q76_VHGT_MIN_BYTES = 4u + Q76_VHGT_DELTA_COUNT;

#define Q76_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FalloutQuest", __VA_ARGS__)
#define Q76_LOGW(...) __android_log_print(ANDROID_LOG_WARN, "FalloutQuest", __VA_ARGS__)
#define Q76_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "FalloutQuest", __VA_ARGS__)

std::vector<Fo3TerrainCellQ76> gTerrainCellsQ76;

int SignedDeltaQ76(uint8_t value) {
    return value < 128u ? static_cast<int>(value) : static_cast<int>(value) - 256;
}

bool DecodeLandQ76(const std::vector<uint8_t>& payload,
                   const CellInfoQ75& cell,
                   uint32_t landFormId,
                   Fo3TerrainCellQ76& out) {
    const uint8_t* vhgt = nullptr;
    uint32_t vhgtSize = 0u;
    WalkSubrecordsQ75(payload.data(), payload.size(),
                      [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "VHGT", 4u) == 0 && !vhgt) {
            vhgt = bytes;
            vhgtSize = size;
        }
    });
    if (!vhgt || vhgtSize < Q76_VHGT_MIN_BYTES) return false;

    const float offset = ReadLeFloatQ75(vhgt);
    if (!std::isfinite(offset)) return false;
    const uint8_t* deltas = vhgt + 4u;

    out = {};
    out.cellFormId = cell.formId;
    out.landFormId = landFormId;
    out.gridX = cell.gridX;
    out.gridY = cell.gridY;
    out.heights.resize(Q76_VHGT_DELTA_COUNT);

    // TES4/Fallout LAND VHGT: Offset is the southwest vertex in height units;
    // each later row's first byte is a north/south delta, and remaining bytes
    // on that row are east/west deltas. One height unit is 8 game units.
    out.heights[0] = offset * Q76_HEIGHT_SCALE;
    for (int row = 0; row < Q76_LAND_VERTS; ++row) {
        const size_t rowBase = static_cast<size_t>(row * Q76_LAND_VERTS);
        if (row > 0) {
            const size_t previousRow = static_cast<size_t>((row - 1) * Q76_LAND_VERTS);
            out.heights[rowBase] = out.heights[previousRow]
                + static_cast<float>(SignedDeltaQ76(deltas[rowBase])) * Q76_HEIGHT_SCALE;
        }
        for (int col = 1; col < Q76_LAND_VERTS; ++col) {
            const size_t index = rowBase + static_cast<size_t>(col);
            out.heights[index] = out.heights[index - 1u]
                + static_cast<float>(SignedDeltaQ76(deltas[index])) * Q76_HEIGHT_SCALE;
        }
    }
    return true;
}

bool CollectTerrainRecordsQ76(uint32_t worldspaceFormId,
                              const std::unordered_set<uint32_t>& selectedCells,
                              const std::unordered_map<uint32_t, const CellInfoQ75*>& cellsById,
                              std::vector<Fo3TerrainCellQ76>& outTerrain,
                              size_t& outLandSeen) {
    outTerrain.clear();
    outLandSeen = 0u;
    FILE* file = std::fopen(ESM_PATH_Q75, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSizeQ75(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE_Q75)) {
        std::fclose(file);
        return false;
    }

    std::vector<GroupFrameQ75> groups;
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
            if (sizeField < HEADER_SIZE_Q75 || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrameQ75{offset + sizeField,
                                           ReadLe32Q75(header + 8u),
                                           ReadLe32Q75(header + 12u)});
            continue;
        }

        const uint32_t flags = ReadLe32Q75(header + 8u);
        const uint32_t formId = ReadLe32Q75(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE_Q75 + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;

        if (!InWorldspaceQ75(groups, worldspaceFormId)) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }
        const uint32_t cellFormId = OwningSelectedCellQ75(groups, selectedCells);
        if (cellFormId == 0u || std::memcmp(header, "LAND", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        ++outLandSeen;
        std::vector<uint8_t> payload;
        if (!ReadPayloadQ75(file, sizeField, flags, payload)) break;
        const auto cellIt = cellsById.find(cellFormId);
        if (cellIt == cellsById.end() || !cellIt->second || !cellIt->second->hasGrid) continue;

        Fo3TerrainCellQ76 terrain;
        if (!DecodeLandQ76(payload, *cellIt->second, formId, terrain)) {
            Q76_LOGW("Q7.6 LAND DECODE MISS: LAND=%08X cell=%08X grid=(%d,%d)",
                     formId, cellFormId, cellIt->second->gridX, cellIt->second->gridY);
            continue;
        }
        outTerrain.push_back(std::move(terrain));
    }

    std::fclose(file);
    std::sort(outTerrain.begin(), outTerrain.end(),
              [](const Fo3TerrainCellQ76& a, const Fo3TerrainCellQ76& b) {
        if (a.gridY != b.gridY) return a.gridY < b.gridY;
        if (a.gridX != b.gridX) return a.gridX < b.gridX;
        return a.cellFormId < b.cellFormId;
    });
    return !outTerrain.empty();
}

bool SampleTerrainQ76(const std::vector<Fo3TerrainCellQ76>& terrain,
                      float gameX, float gameY, float& outHeight) {
    const int32_t gridX = static_cast<int32_t>(std::floor(gameX / Q76_CELL_SIZE));
    const int32_t gridY = static_cast<int32_t>(std::floor(gameY / Q76_CELL_SIZE));
    for (const Fo3TerrainCellQ76& cell : terrain) {
        if (cell.gridX != gridX || cell.gridY != gridY ||
            cell.heights.size() != Q76_VHGT_DELTA_COUNT) continue;
        const float localX = gameX - static_cast<float>(gridX) * Q76_CELL_SIZE;
        const float localY = gameY - static_cast<float>(gridY) * Q76_CELL_SIZE;
        const float gx = std::clamp(localX / Q76_VERTEX_SPACING, 0.0f, 32.0f);
        const float gy = std::clamp(localY / Q76_VERTEX_SPACING, 0.0f, 32.0f);
        const int x0 = std::clamp(static_cast<int>(std::floor(gx)), 0, 32);
        const int y0 = std::clamp(static_cast<int>(std::floor(gy)), 0, 32);
        const int x1 = std::min(x0 + 1, 32);
        const int y1 = std::min(y0 + 1, 32);
        const float tx = gx - static_cast<float>(x0);
        const float ty = gy - static_cast<float>(y0);
        auto h = [&](int x, int y) {
            return cell.heights[static_cast<size_t>(y * Q76_LAND_VERTS + x)];
        };
        const float h0 = h(x0, y0) * (1.0f - tx) + h(x1, y0) * tx;
        const float h1 = h(x0, y1) * (1.0f - tx) + h(x1, y1) * tx;
        outHeight = h0 * (1.0f - ty) + h1 * ty;
        return true;
    }
    return false;
}

} // namespace

bool LoadFo3TerrainQ76(uint32_t worldspaceFormId,
                       float arrivalX, float arrivalY, float arrivalZ) {
    gTerrainCellsQ76.clear();

    std::vector<CellInfoQ75> cells;
    if (!DiscoverWorldspaceCellsQ75(worldspaceFormId, cells)) {
        Q76_LOGE("Q7.6 TERRAIN FAILED: worldspace=%08X reason=discover-cells", worldspaceFormId);
        return false;
    }

    std::vector<const CellInfoQ75*> gridCells;
    std::unordered_map<uint32_t, const CellInfoQ75*> cellsById;
    for (const CellInfoQ75& cell : cells) {
        cellsById[cell.formId] = &cell;
        if (cell.hasGrid) gridCells.push_back(&cell);
    }

    const int32_t targetGridX = static_cast<int32_t>(std::floor(arrivalX / Q76_CELL_SIZE));
    const int32_t targetGridY = static_cast<int32_t>(std::floor(arrivalY / Q76_CELL_SIZE));
    const bool loadWholeWorldspace = gridCells.size() <= SMALL_WORLDSPACE_CELL_LIMIT_Q75;
    std::unordered_set<uint32_t> selectedCells;
    for (const CellInfoQ75* cell : gridCells) {
        if (loadWholeWorldspace ||
            (std::abs(cell->gridX - targetGridX) <= GRID_RADIUS_Q75 &&
             std::abs(cell->gridY - targetGridY) <= GRID_RADIUS_Q75)) {
            selectedCells.insert(cell->formId);
        }
    }

    size_t landSeen = 0u;
    if (!CollectTerrainRecordsQ76(worldspaceFormId, selectedCells, cellsById,
                                  gTerrainCellsQ76, landSeen)) {
        Q76_LOGE("Q7.6 TERRAIN FAILED: worldspace=%08X LANDseen=%zu selectedCells=%zu",
                 worldspaceFormId, landSeen, selectedCells.size());
        return false;
    }

    float minimum = 1e30f;
    float maximum = -1e30f;
    for (const Fo3TerrainCellQ76& cell : gTerrainCellsQ76) {
        for (float height : cell.heights) {
            minimum = std::min(minimum, height);
            maximum = std::max(maximum, height);
        }
        Q76_LOGI("Q7.6 LAND CELL: LAND=%08X cell=%08X grid=(%d,%d) vertices=%zu triangles=%d",
                 cell.landFormId, cell.cellFormId, cell.gridX, cell.gridY,
                 cell.heights.size(), Q76_LAND_QUADS * Q76_LAND_QUADS * 2);
    }

    float arrivalGround = 0.0f;
    const bool sampledArrival = SampleTerrainQ76(gTerrainCellsQ76, arrivalX, arrivalY, arrivalGround);
    Q76_LOGI("Q7.6 TERRAIN READY: worldspace=%08X LANDseen=%zu decodedCells=%zu vertices=%zu triangles=%zu heightRange=[%.1f,%.1f] targetGrid=(%d,%d) XTEL=(%.2f %.2f %.2f) arrivalGroundZ=%s%.2f deltaToXTEL=%s%.2f",
             worldspaceFormId, landSeen, gTerrainCellsQ76.size(),
             gTerrainCellsQ76.size() * Q76_VHGT_DELTA_COUNT,
             gTerrainCellsQ76.size() * static_cast<size_t>(Q76_LAND_QUADS * Q76_LAND_QUADS * 2),
             minimum, maximum, targetGridX, targetGridY,
             arrivalX, arrivalY, arrivalZ,
             sampledArrival ? "" : "N/A ", sampledArrival ? arrivalGround : 0.0f,
             sampledArrival ? "" : "N/A ", sampledArrival ? (arrivalZ - arrivalGround) : 0.0f);
    return true;
}

const std::vector<Fo3TerrainCellQ76>& GetFo3TerrainQ76() {
    return gTerrainCellsQ76;
}
