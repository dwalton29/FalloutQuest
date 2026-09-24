// Textually included after fo3-worldspace-q75.cpp by fo3-cell-spawn-q74.cpp.
// It deliberately reuses the proven Q7.5 ESM/group helpers so terrain and
// object placements are selected from exactly the same exterior CELL domain.

namespace {

std::vector<Fo3TerrainCellQ76> gTerrainDataQ76;
bool gTerrainSelectionOverrideQ1890 = false;
float gTerrainSelectionXQ1890 = 0.0f;
float gTerrainSelectionYQ1890 = 0.0f;

std::unordered_map<uint32_t, uint32_t> gQ1250ParentLandByChildLand;
std::unordered_map<uint32_t, uint32_t> gQ1270MaterialParentLandByChildLand;

uint32_t GetFo3MaterialParentLandQ1270(uint32_t childLandFormId) {
    const auto found = gQ1270MaterialParentLandByChildLand.find(childLandFormId);
    return found == gQ1270MaterialParentLandByChildLand.end() ? 0u : found->second;
}

uint32_t GetFo3RepairedParentLandQ1250(uint32_t childLandFormId) {
    const auto found = gQ1250ParentLandByChildLand.find(childLandFormId);
    return found == gQ1250ParentLandByChildLand.end() ? 0u : found->second;
}
std::unordered_map<uint64_t, uint32_t> gTerrainForceHideQ720;
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

uint32_t GetFo3TerrainForceHideQ720(int32_t gridX, int32_t gridY) {
    const auto found = gTerrainForceHideQ720.find(GridKeyQ79(gridX, gridY));
    return found == gTerrainForceHideQ720.end() ? 0u : (found->second & 0x0fu);
}

bool IsFo3TerrainQuadrantHiddenQ720(int32_t gridX, int32_t gridY, int quadrant) {
    if (quadrant < 0 || quadrant > 3) return false;
    return (GetFo3TerrainForceHideQ720(gridX, gridY) & (1u << quadrant)) != 0u;
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

std::unordered_map<uint32_t, std::vector<float>> gLandVertexColorsQ1060;

bool DecodeVclrQ1060(const uint8_t* bytes, uint32_t size, std::vector<float>& colors) {
    constexpr size_t vertexCount = 33u * 33u;
    constexpr uint32_t expected = static_cast<uint32_t>(vertexCount * 3u);
    if (!bytes || size < expected) return false;
    colors.resize(vertexCount * 3u);
    for (size_t i = 0u; i < vertexCount; ++i) {
        colors[i * 3u + 0u] = static_cast<float>(bytes[i * 3u + 0u]) / 255.0f;
        colors[i * 3u + 1u] = static_cast<float>(bytes[i * 3u + 1u]) / 255.0f;
        colors[i * 3u + 2u] = static_cast<float>(bytes[i * 3u + 2u]) / 255.0f;
    }
    return true;
}

const std::vector<float>& GetFo3TerrainVertexColorsQ1060(uint32_t landFormId) {
    static const std::vector<float> empty;
    const auto found = gLandVertexColorsQ1060.find(landFormId);
    return found == gLandVertexColorsQ1060.end() ? empty : found->second;
}

bool DecodeVnmlQ1010(const uint8_t* bytes, uint32_t size, std::vector<float>& normals) {
    constexpr uint32_t expected = 33u * 33u * 3u;
    if (!bytes || size < expected) return false;
    normals.assign(static_cast<size_t>(33u * 33u * 3u), 0.0f);
    const int8_t* signedBytes = reinterpret_cast<const int8_t*>(bytes);
    for (size_t i = 0u; i < 33u * 33u; ++i) {
        float x = static_cast<float>(signedBytes[i * 3u + 0u]) / 127.0f;
        float y = static_cast<float>(signedBytes[i * 3u + 1u]) / 127.0f;
        float z = static_cast<float>(signedBytes[i * 3u + 2u]) / 127.0f;
        const float length = std::sqrt(x*x + y*y + z*z);
        if (length > 1e-5f) {
            x /= length; y /= length; z /= length;
        } else {
            x = 0.0f; y = 0.0f; z = 1.0f;
        }
        normals[i * 3u + 0u] = x;
        normals[i * 3u + 1u] = y;
        normals[i * 3u + 2u] = z;
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

        if (haveVclr) gLandVertexColorsQ1060[formId] = std::move(vertexColorsQ1060);
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

    const std::vector<CellInfoQ75>* q1910ParentCells = Q1910GetWorldspaceCells(parentInfo.formId);
    if (!q1910ParentCells) {
        Q75_LOGW("Q7.9 LAND PARENT FAILED: child=%08X parent=%08X reason=discover-parent-cells",
                 childWorldspaceFormId, parentInfo.formId);
        return 0u;
    }
    const std::vector<CellInfoQ75>& parentCells = *q1910ParentCells;

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
    CollectSelectedLandQ1910(parentInfo.formId, selectedParentCells, parentCellsById, inherited);

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
        gQ1270MaterialParentLandByChildLand[localCell.landFormId] = parentCell.landFormId;
        Q75_LOGI("Q12.7 MATERIAL PARENT LINK: grid=(%d,%d) childLAND=%08X parentLAND=%08X",
                 localCell.gridX, localCell.gridY,
                 localCell.landFormId, parentCell.landFormId);
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
        gQ1250ParentLandByChildLand[localCell.landFormId] = parentCell.landFormId;
        Q75_LOGI("Q12.5 REPAIRED LAND LINK: grid=(%d,%d) childLAND=%08X parentLAND=%08X",
                 localCell.gridX, localCell.gridY, localCell.landFormId, parentCell.landFormId);
        if (!parentCell.normals.empty()) localCell.normals = std::move(parentCell.normals);
        ++replaced;
    }

    Q75_LOGI("Q7.9 LAND PARENT: child=%08X parent=%08X useLand=1 missingLocalGrids=%zu parentCellsRequested=%zu appended=%zu replaced=%zu parentCellMissing=%zu totalTerrainCells=%zu",
             childWorldspaceFormId, parentInfo.formId,
             missingLocalGrids, selectedParentCells.size(), appended, replaced,
             parentCellMissing, terrain.size());
    return appended + replaced;
}

} // namespace

void SetFo3TerrainSelectionOverrideQ1890(bool enabled, float gameX, float gameY) {
    gTerrainSelectionOverrideQ1890 = enabled;
    gTerrainSelectionXQ1890 = gameX;
    gTerrainSelectionYQ1890 = gameY;
    Q75_LOGI("Q16.17 TERRAIN SELECTION: enabled=%d game=(%.2f %.2f)",
             enabled ? 1 : 0, gameX, gameY);
}

bool LoadFo3TerrainQ76(uint32_t worldspaceFormId,
                       float arrivalX, float arrivalY, float arrivalZ) {
    (void)arrivalZ;
    gTerrainDataQ76.clear();
    gQ1250ParentLandByChildLand.clear();
    gQ1270MaterialParentLandByChildLand.clear();
    gTerrainForceHideQ720.clear();

    const std::vector<CellInfoQ75>* q1910Cells = Q1910GetWorldspaceCells(worldspaceFormId);
    if (!q1910Cells) {
        Q75_LOGE("Q7.6 LAND LOAD FAILED: worldspace=%08X reason=discover-cells", worldspaceFormId);
        return false;
    }
    const std::vector<CellInfoQ75>& cells = *q1910Cells;

    std::vector<const CellInfoQ75*> gridCells;
    std::unordered_map<uint32_t, CellInfoQ75> cellsById;
    for (const CellInfoQ75& cell : cells) {
        cellsById[cell.formId] = cell;
        if (cell.hasGrid) gridCells.push_back(&cell);
    }

    const float selectionXQ1890 = gTerrainSelectionOverrideQ1890
        ? gTerrainSelectionXQ1890 : arrivalX;
    const float selectionYQ1890 = gTerrainSelectionOverrideQ1890
        ? gTerrainSelectionYQ1890 : arrivalY;
    const int32_t targetGridX = static_cast<int32_t>(
        std::floor(selectionXQ1890 / EXTERIOR_CELL_SIZE_Q75));
    const int32_t targetGridY = static_cast<int32_t>(
        std::floor(selectionYQ1890 / EXTERIOR_CELL_SIZE_Q75));
    const bool loadWholeWorldspace = gridCells.size() <= SMALL_WORLDSPACE_CELL_LIMIT_Q75;
    constexpr int Q1931_TERRAIN_GRID_RADIUS = 3; // Q16.27 retained 7x7 LAND runway

    std::unordered_set<uint32_t> selectedCells;
    for (const CellInfoQ75* cell : gridCells) {
        if (loadWholeWorldspace ||
            (std::abs(cell->gridX - targetGridX) <= Q1931_TERRAIN_GRID_RADIUS &&
             std::abs(cell->gridY - targetGridY) <= Q1931_TERRAIN_GRID_RADIUS)) {
            selectedCells.insert(cell->formId);
        }
    }

    size_t forceHiddenCellsQ720 = 0u;
    for (uint32_t cellId : selectedCells) {
        const auto cellIt = cellsById.find(cellId);
        if (cellIt == cellsById.end() || !cellIt->second.hasGrid) continue;
        const uint32_t hide = cellIt->second.forceHideLandQ720 & 0x0fu;
        gTerrainForceHideQ720[GridKeyQ79(cellIt->second.gridX, cellIt->second.gridY)] = hide;
        if (hide != 0u) {
            ++forceHiddenCellsQ720;
            Q75_LOGI("Q7.20 LAND HIDE: cell=%08X grid=(%d,%d) forceHide=0x%X",
                     cellId, cellIt->second.gridX, cellIt->second.gridY, hide);
        }
    }
    Q75_LOGI("Q7.20 LAND HIDE READY: worldspace=%08X selectedGridCells=%zu cellsWithHide=%zu",
             worldspaceFormId, selectedCells.size(), forceHiddenCellsQ720);

    Q75_LOGI("Q16.25 TERRAIN WINDOW: worldspace=%08X targetGrid=(%d,%d) radius=%d selectedCells=%zu wholeWorld=%d",
             worldspaceFormId, targetGridX, targetGridY,
             Q1931_TERRAIN_GRID_RADIUS, selectedCells.size(),
             loadWholeWorldspace ? 1 : 0);
    std::vector<Fo3TerrainCellQ76> localTerrain;

    CollectSelectedLandQ1910(worldspaceFormId, selectedCells, cellsById, localTerrain);
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