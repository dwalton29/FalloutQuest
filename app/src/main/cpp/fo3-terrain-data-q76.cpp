// Textually included after fo3-worldspace-q75.cpp by fo3-cell-spawn-q74.cpp.
// It deliberately reuses the proven Q7.5 ESM/group helpers so terrain and
// object placements are selected from exactly the same exterior CELL domain.

namespace {

std::vector<Fo3TerrainCellQ76> gTerrainDataQ76;
constexpr int Q76_HEIGHT_SIDE = 33;
constexpr size_t Q76_HEIGHT_COUNT = static_cast<size_t>(Q76_HEIGHT_SIDE * Q76_HEIGHT_SIDE);
constexpr float Q76_HEIGHT_SCALE = 8.0f;

bool DecodeVhgtQ76(const uint8_t* bytes, uint32_t size, std::vector<float>& heights) {
    // FO3/FNV VHGT = float offset + 33 rows x 33 signed delta bytes + 3 unused.
    // Values are stored in eighths of a Bethesda game unit height step.
    constexpr uint32_t expected = 4u + 33u * 33u + 3u;
    if (!bytes || size < expected) return false;

    heights.assign(Q76_HEIGHT_COUNT, 0.0f);
    const int8_t* delta = reinterpret_cast<const int8_t*>(bytes + 4u);
    float rowStart = ReadLeFloatQ75(bytes);

    for (int row = 0; row < Q76_HEIGHT_SIDE; ++row) {
        rowStart += static_cast<float>(delta[row * Q76_HEIGHT_SIDE]);
        float value = rowStart;
        heights[static_cast<size_t>(row * Q76_HEIGHT_SIDE)] = value * Q76_HEIGHT_SCALE;
        for (int col = 1; col < Q76_HEIGHT_SIDE; ++col) {
            value += static_cast<float>(delta[row * Q76_HEIGHT_SIDE + col]);
            heights[static_cast<size_t>(row * Q76_HEIGHT_SIDE + col)] = value * Q76_HEIGHT_SCALE;
        }
    }
    return true;
}

bool CollectSelectedLandQ76(uint32_t worldspaceFormId,
                            const std::unordered_set<uint32_t>& selectedCells,
                            const std::unordered_map<uint32_t, CellInfoQ75>& cellsById,
                            std::vector<Fo3TerrainCellQ76>& out) {
    out.clear();
    FILE* file = std::fopen(ESM_PATH_Q75, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSizeQ75(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE_Q75)) {
        std::fclose(file);
        return false;
    }

    std::vector<GroupFrameQ75> groups;
    size_t landSeen = 0u;
    size_t selectedLand = 0u;
    size_t missingVhgt = 0u;

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

        if (!InWorldspaceQ75(groups, worldspaceFormId) || std::memcmp(header, "LAND", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }
        ++landSeen;

        const uint32_t owner = OwningSelectedCellQ75(groups, selectedCells);
        if (owner == 0u) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }
        const auto cellIt = cellsById.find(owner);
        if (cellIt == cellsById.end() || !cellIt->second.hasGrid) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayloadQ75(file, sizeField, flags, payload)) break;
        Fo3TerrainCellQ76 terrain;
        terrain.cellFormId = owner;
        terrain.landFormId = formId;
        terrain.gridX = cellIt->second.gridX;
        terrain.gridY = cellIt->second.gridY;
        bool haveVhgt = false;
        WalkSubrecordsQ75(payload.data(), payload.size(),
                          [&](const char* type, const uint8_t* bytes, uint32_t subSize) {
            if (std::memcmp(type, "VHGT", 4u) == 0 && !haveVhgt) {
                haveVhgt = DecodeVhgtQ76(bytes, subSize, terrain.heights);
            }
        });

        if (!haveVhgt) {
            ++missingVhgt;
            continue;
        }
        ++selectedLand;
        Q75_LOGI("Q7.6 LAND CELL: LAND=%08X cell=%08X grid=(%d,%d) vertices=%zu minHeight=%.1f maxHeight=%.1f",
                 terrain.landFormId, terrain.cellFormId, terrain.gridX, terrain.gridY,
                 terrain.heights.size(),
                 *std::min_element(terrain.heights.begin(), terrain.heights.end()),
                 *std::max_element(terrain.heights.begin(), terrain.heights.end()));
        out.push_back(std::move(terrain));
    }

    std::fclose(file);
    Q75_LOGI("Q7.6 LAND DECODE: worldspace=%08X LANDseen=%zu selected=%zu decoded=%zu missingVHGT=%zu",
             worldspaceFormId, landSeen, selectedCells.size(), out.size(), missingVhgt);
    return !out.empty();
}

} // namespace

bool LoadFo3TerrainQ76(uint32_t worldspaceFormId,
                       float arrivalX, float arrivalY, float arrivalZ) {
    (void)arrivalZ;
    gTerrainDataQ76.clear();

    std::vector<CellInfoQ75> cells;
    if (!DiscoverWorldspaceCellsQ75(worldspaceFormId, cells)) {
        Q75_LOGE("Q7.6 LAND LOAD FAILED: worldspace=%08X reason=discover-cells", worldspaceFormId);
        return false;
    }

    std::vector<const CellInfoQ75*> gridCells;
    std::unordered_map<uint32_t, CellInfoQ75> cellsById;
    for (const CellInfoQ75& cell : cells) {
        cellsById[cell.formId] = cell;
        if (cell.hasGrid) gridCells.push_back(&cell);
    }

    const int32_t targetGridX = static_cast<int32_t>(std::floor(arrivalX / EXTERIOR_CELL_SIZE_Q75));
    const int32_t targetGridY = static_cast<int32_t>(std::floor(arrivalY / EXTERIOR_CELL_SIZE_Q75));
    const bool loadWholeWorldspace = gridCells.size() <= SMALL_WORLDSPACE_CELL_LIMIT_Q75;

    std::unordered_set<uint32_t> selectedCells;
    for (const CellInfoQ75* cell : gridCells) {
        if (loadWholeWorldspace ||
            (std::abs(cell->gridX - targetGridX) <= GRID_RADIUS_Q75 &&
             std::abs(cell->gridY - targetGridY) <= GRID_RADIUS_Q75)) {
            selectedCells.insert(cell->formId);
        }
    }

    if (!CollectSelectedLandQ76(worldspaceFormId, selectedCells, cellsById, gTerrainDataQ76)) {
        Q75_LOGE("Q7.6 LAND LOAD FAILED: worldspace=%08X selectedCells=%zu targetGrid=(%d,%d)",
                 worldspaceFormId, selectedCells.size(), targetGridX, targetGridY);
        return false;
    }

    size_t vertices = 0u;
    for (const Fo3TerrainCellQ76& cell : gTerrainDataQ76) vertices += cell.heights.size();
    Q75_LOGI("Q7.6 LAND READY: worldspace=%08X terrainCells=%zu heightVertices=%zu targetGrid=(%d,%d) mode=%s",
             worldspaceFormId, gTerrainDataQ76.size(), vertices,
             targetGridX, targetGridY,
             loadWholeWorldspace ? "full-small-worldspace" : "arrival-neighborhood");
    return true;
}

const std::vector<Fo3TerrainCellQ76>& GetFo3TerrainQ76() {
    return gTerrainDataQ76;
}
