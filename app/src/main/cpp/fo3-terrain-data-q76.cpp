// Textually included after fo3-worldspace-q75.cpp by fo3-cell-spawn-q74.cpp.
// It deliberately reuses the proven Q7.5 ESM/group helpers so terrain and
// object placements are selected from exactly the same exterior CELL domain.

namespace {

std::vector<Fo3TerrainCellQ76> gTerrainDataQ76;
constexpr int Q76_HEIGHT_SIDE = 33;
constexpr size_t Q76_HEIGHT_COUNT = static_cast<size_t>(Q76_HEIGHT_SIDE * Q76_HEIGHT_SIDE);
constexpr float Q76_HEIGHT_SCALE = 8.0f;
constexpr float Q79_PARENT_LOCAL_MAX_MEAN_DELTA = 4096.0f;

struct ParentWorldspaceQ79 {
    uint32_t formId = 0u;
    uint16_t flags = 0u;
    bool found = false;
    bool useLand = false;
};

uint64_t GridKeyQ79(int32_t x, int32_t y) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32u) |
           static_cast<uint32_t>(y);
}

float MeanHeightQ79(const std::vector<float>& heights) {
    if (heights.empty()) return 0.0f;
    double total = 0.0;
    for (float height : heights) total += static_cast<double>(height);
    return static_cast<float>(total / static_cast<double>(heights.size()));
}

bool DecodeVhgtQ76(const uint8_t* bytes, uint32_t size, std::vector<float>& heights) {
    // FO3/FNV VHGT = float offset + 33 rows x 33 signed delta bytes + 3 unused.
    // The offset itself is vertex (0,0). The first byte of each subsequent
    // row moves that row's first vertex north/south; delta[0] is not applied
    // on top of the offset. One height step is 8 game units.
    constexpr uint32_t expected = 4u + 33u * 33u + 3u;
    if (!bytes || size < expected) return false;

    heights.assign(Q76_HEIGHT_COUNT, 0.0f);
    const int8_t* delta = reinterpret_cast<const int8_t*>(bytes + 4u);
    float rowStart = ReadLeFloatQ75(bytes);

    for (int row = 0; row < Q76_HEIGHT_SIDE; ++row) {
        if (row > 0) {
            rowStart += static_cast<float>(delta[row * Q76_HEIGHT_SIDE]);
        }
        float value = rowStart;
        heights[static_cast<size_t>(row * Q76_HEIGHT_SIDE)] = value * Q76_HEIGHT_SCALE;
        for (int col = 1; col < Q76_HEIGHT_SIDE; ++col) {
            value += static_cast<float>(delta[row * Q76_HEIGHT_SIDE + col]);
            heights[static_cast<size_t>(row * Q76_HEIGHT_SIDE + col)] = value * Q76_HEIGHT_SCALE;
        }
    }
    return true;
}

bool ReadParentWorldspaceQ79(uint32_t worldspaceFormId, ParentWorldspaceQ79& out) {
    out = {};
    FILE* file = std::fopen(ESM_PATH_Q75, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSizeQ75(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE_Q75)) {
        std::fclose(file);
        return false;
    }

    bool recordFound = false;
    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        if (offset + HEADER_SIZE_Q75 > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE_Q75]{};
        if (!ReadExactQ75(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32Q75(header + 4u);

        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE_Q75 ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            // Pointer already sits at the first child record. Do not seek to
            // group end; WRLD records are inside the top-level WRLD group.
            continue;
        }

        const uint64_t payloadEnd = offset + HEADER_SIZE_Q75 + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        const uint32_t formId = ReadLe32Q75(header + 12u);
        if (std::memcmp(header, "WRLD", 4u) != 0 || formId != worldspaceFormId) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        const uint32_t flags = ReadLe32Q75(header + 8u);
        std::vector<uint8_t> payload;
        if (!ReadPayloadQ75(file, sizeField, flags, payload)) break;

        uint32_t parent = 0u;
        uint16_t parentFlags = 0u;
        WalkSubrecordsQ75(payload.data(), payload.size(),
                          [&](const char* type, const uint8_t* bytes, uint32_t subSize) {
            if (std::memcmp(type, "WNAM", 4u) == 0 && subSize >= 4u) {
                parent = ReadLe32Q75(bytes);
            } else if (std::memcmp(type, "PNAM", 4u) == 0 && subSize >= 1u) {
                parentFlags = bytes[0];
                if (subSize >= 2u) {
                    parentFlags |= static_cast<uint16_t>(bytes[1]) << 8u;
                }
            }
        });

        out.formId = parent;
        out.flags = parentFlags;
        out.found = true;
        out.useLand = parent != 0u && (parentFlags & 0x0001u) != 0u;
        recordFound = true;
        break;
    }

    std::fclose(file);
    if (recordFound) {
        Q75_LOGI("Q7.9 WRLD PARENT: child=%08X parent=%08X PNAM=%04X useLand=%d",
                 worldspaceFormId, out.formId, out.flags, out.useLand ? 1 : 0);
    } else {
        Q75_LOGW("Q7.9 WRLD PARENT: child=%08X recordNotFound=1", worldspaceFormId);
    }
    return recordFound;
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

size_t AppendInheritedParentLandQ79(
    uint32_t childWorldspaceFormId,
    const std::unordered_set<uint32_t>& selectedChildCells,
    const std::unordered_map<uint32_t, CellInfoQ75>& childCellsById,
    std::vector<Fo3TerrainCellQ76>& terrain) {

    ParentWorldspaceQ79 parentInfo;
    if (!ReadParentWorldspaceQ79(childWorldspaceFormId, parentInfo) ||
        !parentInfo.useLand || parentInfo.formId == 0u) {
        return 0u;
    }

    std::unordered_set<uint64_t> selectedGridKeys;
    for (uint32_t cellId : selectedChildCells) {
        const auto it = childCellsById.find(cellId);
        if (it == childCellsById.end() || !it->second.hasGrid) continue;
        selectedGridKeys.insert(GridKeyQ79(it->second.gridX, it->second.gridY));
    }

    std::unordered_map<uint64_t, size_t> localIndexByGrid;
    for (size_t index = 0u; index < terrain.size(); ++index) {
        const Fo3TerrainCellQ76& cell = terrain[index];
        localIndexByGrid[GridKeyQ79(cell.gridX, cell.gridY)] = index;
    }

    std::vector<CellInfoQ75> parentCells;
    if (!DiscoverWorldspaceCellsQ75(parentInfo.formId, parentCells)) {
        Q75_LOGW("Q7.9 LAND PARENT FAILED: child=%08X parent=%08X reason=discover-parent-cells",
                 childWorldspaceFormId, parentInfo.formId);
        return 0u;
    }

    std::unordered_map<uint32_t, CellInfoQ75> parentCellsById;
    std::unordered_map<uint64_t, uint32_t> parentCellByGrid;
    for (const CellInfoQ75& cell : parentCells) {
        parentCellsById[cell.formId] = cell;
        if (cell.hasGrid) {
            parentCellByGrid[GridKeyQ79(cell.gridX, cell.gridY)] = cell.formId;
        }
    }

    // Ask for the parent counterpart for every selected child grid, not only
    // grids with no local LAND. Some FO3 child worldspaces contain local LAND
    // records whose VHGT is a low/default island even though Use Land Data is
    // enabled; comparing the overlapping parent cell lets us reject only the
    // catastrophic vertical outliers without hardcoding a worldspace or grid.
    std::unordered_set<uint32_t> selectedParentCells;
    size_t missingLocalGrids = 0u;
    size_t parentCellMissing = 0u;
    for (uint64_t key : selectedGridKeys) {
        if (localIndexByGrid.find(key) == localIndexByGrid.end()) ++missingLocalGrids;
        const auto parentIt = parentCellByGrid.find(key);
        if (parentIt == parentCellByGrid.end()) {
            ++parentCellMissing;
            continue;
        }
        selectedParentCells.insert(parentIt->second);
    }

    if (selectedParentCells.empty()) {
        Q75_LOGI("Q7.9 LAND PARENT: child=%08X parent=%08X useLand=1 missingLocalGrids=%zu parentCellsRequested=0 appended=0 replaced=0 parentCellMissing=%zu",
                 childWorldspaceFormId, parentInfo.formId,
                 missingLocalGrids, parentCellMissing);
        return 0u;
    }

    std::vector<Fo3TerrainCellQ76> inherited;
    CollectSelectedLandQ76(parentInfo.formId, selectedParentCells, parentCellsById, inherited);

    size_t appended = 0u;
    size_t replaced = 0u;
    for (Fo3TerrainCellQ76& parentCell : inherited) {
        const uint64_t key = GridKeyQ79(parentCell.gridX, parentCell.gridY);
        const auto localIt = localIndexByGrid.find(key);
        if (localIt == localIndexByGrid.end()) {
            const size_t newIndex = terrain.size();
            terrain.push_back(std::move(parentCell));
            localIndexByGrid[key] = newIndex;
            ++appended;
            continue;
        }

        Fo3TerrainCellQ76& localCell = terrain[localIt->second];
        const float localMean = MeanHeightQ79(localCell.heights);
        const float parentMean = MeanHeightQ79(parentCell.heights);
        const float meanDelta = std::fabs(localMean - parentMean);
        if (meanDelta <= Q79_PARENT_LOCAL_MAX_MEAN_DELTA) continue;

        // Preserve the child LAND/CELL identity so child-world texture and
        // material lookups remain authored locally; only repair the implausible
        // heightfield from the matching parent grid.
        Q75_LOGW("Q7.9 LAND PARENT REPLACE: child=%08X parent=%08X grid=(%d,%d) localLAND=%08X parentLAND=%08X localMean=%.1f parentMean=%.1f delta=%.1f threshold=%.1f",
                 childWorldspaceFormId, parentInfo.formId,
                 localCell.gridX, localCell.gridY,
                 localCell.landFormId, parentCell.landFormId,
                 localMean, parentMean, meanDelta,
                 Q79_PARENT_LOCAL_MAX_MEAN_DELTA);
        localCell.heights = std::move(parentCell.heights);
        ++replaced;
    }

    Q75_LOGI("Q7.9 LAND PARENT: child=%08X parent=%08X useLand=1 missingLocalGrids=%zu parentCellsRequested=%zu appended=%zu replaced=%zu parentCellMissing=%zu totalTerrainCells=%zu",
             childWorldspaceFormId, parentInfo.formId,
             missingLocalGrids, selectedParentCells.size(), appended, replaced,
             parentCellMissing, terrain.size());
    return appended + replaced;
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

    std::vector<Fo3TerrainCellQ76> localTerrain;
    CollectSelectedLandQ76(worldspaceFormId, selectedCells, cellsById, localTerrain);
    gTerrainDataQ76 = std::move(localTerrain);

    const size_t localCount = gTerrainDataQ76.size();
    const size_t inheritedCount = AppendInheritedParentLandQ79(
        worldspaceFormId, selectedCells, cellsById, gTerrainDataQ76);

    if (gTerrainDataQ76.empty()) {
        Q75_LOGE("Q7.6 LAND LOAD FAILED: worldspace=%08X selectedCells=%zu targetGrid=(%d,%d) local=0 inherited=0",
                 worldspaceFormId, selectedCells.size(), targetGridX, targetGridY);
        return false;
    }

    size_t vertices = 0u;
    for (const Fo3TerrainCellQ76& cell : gTerrainDataQ76) vertices += cell.heights.size();
    Q75_LOGI("Q7.9 LAND READY: worldspace=%08X terrainCells=%zu local=%zu inherited=%zu heightVertices=%zu selectedGridCells=%zu targetGrid=(%d,%d) mode=%s",
             worldspaceFormId, gTerrainDataQ76.size(), localCount, inheritedCount,
             vertices, selectedCells.size(), targetGridX, targetGridY,
             loadWholeWorldspace ? "full-small-worldspace" : "arrival-neighborhood");
    return true;
}

const std::vector<Fo3TerrainCellQ76>& GetFo3TerrainQ76() {
    return gTerrainDataQ76;
}