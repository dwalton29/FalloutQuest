#include "fo3-transition-q74.h"

#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <zlib.h>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* ESM_PATH_Q75 =
        "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout3.esm";
constexpr uint32_t FLAG_COMPRESSED_Q75 = 0x00040000u;
constexpr uint32_t FLAG_INITIALLY_DISABLED_Q75 = 0x00000800u;
constexpr uint8_t ENABLE_PARENT_OPPOSITE_Q75 = 0x01u;
constexpr uint64_t HEADER_SIZE_Q75 = 24u;
constexpr uint32_t MAX_RECORD_BYTES_Q75 = 64u * 1024u * 1024u;
constexpr float EXTERIOR_CELL_SIZE_Q75 = 4096.0f;
constexpr int GRID_RADIUS_Q75 = 1; // Q16.7 temporary 3x3 exterior test window
constexpr size_t SMALL_WORLDSPACE_CELL_LIMIT_Q75 = 64u;

#define Q75_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q75_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q75_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

struct GroupFrameQ75 {
    uint64_t end = 0;
    uint32_t label = 0;
    uint32_t type = 0;
};

struct CellInfoQ75 {
    uint32_t formId = 0;
    int32_t gridX = 0;
    int32_t gridY = 0;
    uint32_t forceHideLandQ720 = 0u;
    bool hasGrid = false;
    std::string editorId;
};

struct RawPlacementQ75 {
    uint32_t refFormId = 0;
    uint32_t owningCellFormId = 0; // Q16.7 persistent-cell locality
    uint32_t baseFormId = 0;
    uint32_t recordFlags = 0;
    uint32_t enableParentFormId = 0;
    uint8_t enableParentFlags = 0;
    bool hasEnableParent = false;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
    float scale = 1.0f;
    bool hasTransform = false;
};

struct BaseRecordQ75 {
    uint32_t formId = 0;
    std::string recordType;
    std::string editorId;
    std::string modelPath;
};

uint16_t ReadLe16Q75(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

uint32_t ReadLe32Q75(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

float ReadLeFloatQ75(const uint8_t* p) {
    const uint32_t bits = ReadLe32Q75(p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool ReadExactQ75(FILE* file, void* dst, size_t size) {
    return std::fread(dst, 1, size, file) == size;
}

int64_t FileSizeQ75(FILE* file) {
    const off_t current = ftello(file);
    if (current < 0) return -1;
    if (fseeko(file, 0, SEEK_END) != 0) return -1;
    const off_t end = ftello(file);
    fseeko(file, current, SEEK_SET);
    return static_cast<int64_t>(end);
}

bool InflateRecordQ75(const std::vector<uint8_t>& stored, std::vector<uint8_t>& out) {
    if (stored.size() < 4u) return false;
    const uint32_t inflatedSize = ReadLe32Q75(stored.data());
    if (inflatedSize == 0u || inflatedSize > MAX_RECORD_BYTES_Q75) return false;
    out.resize(inflatedSize);
    uLongf destLen = static_cast<uLongf>(out.size());
    const int result = uncompress(reinterpret_cast<Bytef*>(out.data()), &destLen,
                                  reinterpret_cast<const Bytef*>(stored.data() + 4u),
                                  static_cast<uLong>(stored.size() - 4u));
    if (result != Z_OK || destLen != inflatedSize) {
        out.clear();
        return false;
    }
    return true;
}

bool ReadPayloadQ75(FILE* file, uint32_t storedSize, uint32_t flags,
                    std::vector<uint8_t>& out) {
    if (storedSize == 0u || storedSize > MAX_RECORD_BYTES_Q75) return false;
    std::vector<uint8_t> stored(storedSize);
    if (!ReadExactQ75(file, stored.data(), stored.size())) return false;
    if ((flags & FLAG_COMPRESSED_Q75) == 0u) {
        out.swap(stored);
        return true;
    }
    return InflateRecordQ75(stored, out);
}

void WalkSubrecordsQ75(const uint8_t* data, size_t size,
                       const std::function<void(const char*, const uint8_t*, uint32_t)>& visitor) {
    size_t pos = 0u;
    uint32_t extendedSize = 0u;
    while (pos + 6u <= size) {
        const char* type = reinterpret_cast<const char*>(data + pos);
        const uint16_t size16 = ReadLe16Q75(data + pos + 4u);
        pos += 6u;
        if (std::memcmp(type, "XXXX", 4u) == 0) {
            if (size16 != 4u || pos + 4u > size) return;
            extendedSize = ReadLe32Q75(data + pos);
            pos += 4u;
            continue;
        }
        const uint32_t subSize = extendedSize ? extendedSize : size16;
        extendedSize = 0u;
        if (subSize > size - pos) return;
        visitor(type, data + pos, subSize);
        pos += subSize;
    }
}

std::string FourCCQ75(const uint8_t* p) {
    char value[5]{static_cast<char>(p[0]), static_cast<char>(p[1]),
                  static_cast<char>(p[2]), static_cast<char>(p[3]), 0};
    return std::string(value);
}

bool InWorldspaceQ75(const std::vector<GroupFrameQ75>& groups, uint32_t worldspaceFormId) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (it->type == 1u && it->label == worldspaceFormId) return true;
    }
    return false;
}

uint32_t OwningSelectedCellQ75(const std::vector<GroupFrameQ75>& groups,
                               const std::unordered_set<uint32_t>& selectedCells) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if ((it->type == 6u || it->type == 8u || it->type == 9u || it->type == 10u) &&
            selectedCells.find(it->label) != selectedCells.end()) {
            return it->label;
        }
    }
    return 0u;
}

bool DiscoverWorldspaceCellsQ75(uint32_t worldspaceFormId,
                                std::vector<CellInfoQ75>& outCells) {
    outCells.clear();
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

        if (!InWorldspaceQ75(groups, worldspaceFormId) || std::memcmp(header, "CELL", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayloadQ75(file, sizeField, flags, payload)) break;
        CellInfoQ75 cell;
        cell.formId = formId;
        WalkSubrecordsQ75(payload.data(), payload.size(),
                          [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "XCLC", 4u) == 0 && size >= 8u) {
                cell.gridX = static_cast<int32_t>(ReadLe32Q75(bytes + 0u));
                cell.gridY = static_cast<int32_t>(ReadLe32Q75(bytes + 4u));
                cell.forceHideLandQ720 = size >= 12u ? (ReadLe32Q75(bytes + 8u) & 0x0fu) : 0u;
                cell.hasGrid = true;
            } else if (std::memcmp(type, "EDID", 4u) == 0 && cell.editorId.empty()) {
                size_t len = 0u;
                while (len < size && bytes[len] != 0u) ++len;
                cell.editorId.assign(reinterpret_cast<const char*>(bytes), len);
            }
        });
        outCells.push_back(std::move(cell));
    }

    std::fclose(file);
    return !outCells.empty();
}

bool ParsePlacementQ75(const std::vector<uint8_t>& payload, RawPlacementQ75& out) {
    bool haveBase = false;
    WalkSubrecordsQ75(payload.data(), payload.size(),
                      [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "NAME", 4u) == 0 && size >= 4u) {
            out.baseFormId = ReadLe32Q75(bytes);
            haveBase = out.baseFormId != 0u;
        } else if (std::memcmp(type, "DATA", 4u) == 0 && size >= 24u) {
            out.x = ReadLeFloatQ75(bytes + 0u);
            out.y = ReadLeFloatQ75(bytes + 4u);
            out.z = ReadLeFloatQ75(bytes + 8u);
            out.rx = ReadLeFloatQ75(bytes + 12u);
            out.ry = ReadLeFloatQ75(bytes + 16u);
            out.rz = ReadLeFloatQ75(bytes + 20u);
            out.hasTransform = true;
        } else if (std::memcmp(type, "XSCL", 4u) == 0 && size >= 4u) {
            out.scale = ReadLeFloatQ75(bytes);
            if (!(out.scale > 0.0001f && out.scale < 1000.0f)) out.scale = 1.0f;
        } else if (std::memcmp(type, "XESP", 4u) == 0 && size >= 4u) {
            out.hasEnableParent = true;
            out.enableParentFormId = ReadLe32Q75(bytes);
            if (size >= 5u) out.enableParentFlags = bytes[4u];
        }
    });
    return haveBase && out.hasTransform;
}

bool CollectSelectedRefsQ75(uint32_t worldspaceFormId,
                            const std::unordered_set<uint32_t>& selectedCells,
                            std::vector<RawPlacementQ75>& out,
                            size_t& landRecords) {
    out.clear();
    landRecords = 0u;
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
        const uint32_t cell = OwningSelectedCellQ75(groups, selectedCells);
        if (cell == 0u) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        if (std::memcmp(header, "LAND", 4u) == 0) {
            ++landRecords;
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }
        if (std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayloadQ75(file, sizeField, flags, payload)) break;
        RawPlacementQ75 placement;
        placement.refFormId = formId;
        placement.owningCellFormId = cell;
        placement.recordFlags = flags;
        if (ParsePlacementQ75(payload, placement)) out.push_back(placement);
    }

    std::fclose(file);
    return !out.empty();
}

bool ResolveInitialEnabledQ75(uint32_t refFormId,
                              const std::unordered_map<uint32_t, const RawPlacementQ75*>& refs,
                              std::unordered_map<uint32_t, bool>& memo,
                              std::unordered_set<uint32_t>& visiting) {
    const auto memoIt = memo.find(refFormId);
    if (memoIt != memo.end()) return memoIt->second;
    const auto it = refs.find(refFormId);
    if (it == refs.end()) return true;
    if (!visiting.insert(refFormId).second) return true;
    const RawPlacementQ75& p = *it->second;
    bool enabled = true;
    if (p.hasEnableParent && p.enableParentFormId != 0u) {
        enabled = ResolveInitialEnabledQ75(p.enableParentFormId, refs, memo, visiting);
        if ((p.enableParentFlags & ENABLE_PARENT_OPPOSITE_Q75) != 0u) enabled = !enabled;
    } else {
        enabled = (p.recordFlags & FLAG_INITIALLY_DISABLED_Q75) == 0u;
    }
    visiting.erase(refFormId);
    memo[refFormId] = enabled;
    return enabled;
}

bool ResolveBasesQ75(const std::unordered_set<uint32_t>& wanted,
                     std::unordered_map<uint32_t, BaseRecordQ75>& out) {
    out.clear();
    FILE* file = std::fopen(ESM_PATH_Q75, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSizeQ75(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE_Q75)) {
        std::fclose(file);
        return false;
    }

    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        if (offset + HEADER_SIZE_Q75 > static_cast<uint64_t>(fileSize)) break;
        uint8_t header[HEADER_SIZE_Q75]{};
        if (!ReadExactQ75(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32Q75(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE_Q75 || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            continue;
        }

        const uint32_t flags = ReadLe32Q75(header + 8u);
        const uint32_t formId = ReadLe32Q75(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE_Q75 + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (wanted.find(formId) == wanted.end()) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayloadQ75(file, sizeField, flags, payload)) break;
        BaseRecordQ75 base;
        base.formId = formId;
        base.recordType = FourCCQ75(header);
        WalkSubrecordsQ75(payload.data(), payload.size(),
                          [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "EDID", 4u) == 0 && base.editorId.empty()) {
                size_t len = 0u;
                while (len < size && bytes[len] != 0u) ++len;
                base.editorId.assign(reinterpret_cast<const char*>(bytes), len);
            } else if (std::memcmp(type, "MODL", 4u) == 0 && base.modelPath.empty()) {
                size_t len = 0u;
                while (len < size && bytes[len] != 0u) ++len;
                base.modelPath.assign(reinterpret_cast<const char*>(bytes), len);
            }
        });
        out[formId] = std::move(base);
        if (out.size() == wanted.size()) break;
    }
    std::fclose(file);
    return true;
}


constexpr uint32_t WASTELAND_WORLDSPACE_Q1800 = 0x0000003Cu;

struct Q1800WorldspaceIndex {
    std::vector<CellInfoQ75> cells;
    std::unordered_map<uint64_t, std::vector<uint32_t>> gridCells;
    std::unordered_map<uint32_t, size_t> cellByFormId;
    std::unordered_map<uint32_t, std::vector<RawPlacementQ75>> refsByCell;
    std::unordered_map<uint32_t, size_t> landCountByCell;
    std::unordered_map<uint32_t, BaseRecordQ75> bases;
    size_t gridCellCount = 0u;
    size_t rawRefCount = 0u;
    size_t landRecordCount = 0u;
};

Q1800WorldspaceIndex gQ1800WastelandIndex;
std::once_flag gQ1800WastelandIndexOnce;
bool gQ1800WastelandIndexReady = false;

uint64_t GridKeyQ1800(int32_t x, int32_t y) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32u) |
           static_cast<uint32_t>(y);
}

uint32_t OwningWorldspaceCellQ1800(const std::vector<GroupFrameQ75>& groups) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (it->type == 6u || it->type == 8u || it->type == 9u || it->type == 10u) {
            return it->label;
        }
    }
    return 0u;
}

bool BuildWastelandIndexQ1800(Q1800WorldspaceIndex& out) {
    const auto started = std::chrono::steady_clock::now();
    out = Q1800WorldspaceIndex{};

    FILE* file = std::fopen(ESM_PATH_Q75, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSizeQ75(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE_Q75)) {
        std::fclose(file);
        return false;
    }

    std::unordered_set<uint32_t> wantedBases;
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
            if (sizeField < HEADER_SIZE_Q75 ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrameQ75{offset + sizeField,
                                           ReadLe32Q75(header + 8u),
                                           ReadLe32Q75(header + 12u)});
            continue;
        }

        const uint32_t flags = ReadLe32Q75(header + 8u);
        const uint32_t formId = ReadLe32Q75(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE_Q75 + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;

        if (!InWorldspaceQ75(groups, WASTELAND_WORLDSPACE_Q1800)) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        if (std::memcmp(header, "CELL", 4u) == 0) {
            std::vector<uint8_t> payload;
            if (!ReadPayloadQ75(file, sizeField, flags, payload)) break;

            CellInfoQ75 cell;
            cell.formId = formId;
            WalkSubrecordsQ75(payload.data(), payload.size(),
                              [&](const char* type, const uint8_t* bytes, uint32_t size) {
                if (std::memcmp(type, "XCLC", 4u) == 0 && size >= 8u) {
                    cell.gridX = static_cast<int32_t>(ReadLe32Q75(bytes + 0u));
                    cell.gridY = static_cast<int32_t>(ReadLe32Q75(bytes + 4u));
                    cell.forceHideLandQ720 =
                        size >= 12u ? (ReadLe32Q75(bytes + 8u) & 0x0fu) : 0u;
                    cell.hasGrid = true;
                } else if (std::memcmp(type, "EDID", 4u) == 0 && cell.editorId.empty()) {
                    size_t len = 0u;
                    while (len < size && bytes[len] != 0u) ++len;
                    cell.editorId.assign(reinterpret_cast<const char*>(bytes), len);
                }
            });

            const size_t cellIndex = out.cells.size();
            out.cellByFormId[cell.formId] = cellIndex;
            if (cell.hasGrid) {
                out.gridCells[GridKeyQ1800(cell.gridX, cell.gridY)].push_back(cell.formId);
                ++out.gridCellCount;
            }
            out.cells.push_back(std::move(cell));
            continue;
        }

        const uint32_t ownerCell = OwningWorldspaceCellQ1800(groups);
        if (ownerCell != 0u && std::memcmp(header, "LAND", 4u) == 0) {
            ++out.landCountByCell[ownerCell];
            ++out.landRecordCount;
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        if (ownerCell != 0u && std::memcmp(header, "REFR", 4u) == 0) {
            std::vector<uint8_t> payload;
            if (!ReadPayloadQ75(file, sizeField, flags, payload)) break;
            RawPlacementQ75 placement;
            placement.refFormId = formId;
            placement.owningCellFormId = ownerCell;
            placement.recordFlags = flags;
            if (ParsePlacementQ75(payload, placement)) {
                wantedBases.insert(placement.baseFormId);
                out.refsByCell[ownerCell].push_back(std::move(placement));
                ++out.rawRefCount;
            }
            continue;
        }

        if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
    }

    std::fclose(file);

    if (out.cells.empty() || out.rawRefCount == 0u) return false;
    if (!ResolveBasesQ75(wantedBases, out.bases)) return false;

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    Q75_LOGI("Q18 WASTELAND INDEX READY: cells=%zu gridCells=%zu refs=%zu LAND=%zu bases=%zu buildMs=%lld source=Fallout3.esm",
             out.cells.size(), out.gridCellCount, out.rawRefCount,
             out.landRecordCount, out.bases.size(),
             static_cast<long long>(elapsed));
    return true;
}

const Q1800WorldspaceIndex* GetWastelandIndexQ1800() {
    std::call_once(gQ1800WastelandIndexOnce, []() {
        gQ1800WastelandIndexReady = BuildWastelandIndexQ1800(gQ1800WastelandIndex);
        if (!gQ1800WastelandIndexReady) {
            Q75_LOGE("Q18 WASTELAND INDEX FAILED: source=Fallout3.esm");
        }
    });
    return gQ1800WastelandIndexReady ? &gQ1800WastelandIndex : nullptr;
}

bool CollectIndexedRefsQ1800(const Q1800WorldspaceIndex& index,
                             const std::unordered_set<uint32_t>& selectedCells,
                             std::vector<RawPlacementQ75>& out,
                             size_t& landRecords) {
    out.clear();
    landRecords = 0u;

    size_t reserveRefs = 0u;
    for (uint32_t cellFormId : selectedCells) {
        const auto refs = index.refsByCell.find(cellFormId);
        if (refs != index.refsByCell.end()) reserveRefs += refs->second.size();
        const auto land = index.landCountByCell.find(cellFormId);
        if (land != index.landCountByCell.end()) landRecords += land->second;
    }
    out.reserve(reserveRefs);

    for (uint32_t cellFormId : selectedCells) {
        const auto refs = index.refsByCell.find(cellFormId);
        if (refs == index.refsByCell.end()) continue;
        out.insert(out.end(), refs->second.begin(), refs->second.end());
    }
    return !out.empty();
}

void SelectIndexedCellsQ1800(const Q1800WorldspaceIndex& index,
                             uint32_t persistentCellFormId,
                             int32_t targetGridX, int32_t targetGridY,
                             int radius, bool loadWholeWorldspace,
                             std::unordered_set<uint32_t>& selectedCells) {
    selectedCells.clear();
    if (persistentCellFormId != 0u) selectedCells.insert(persistentCellFormId);

    if (loadWholeWorldspace) {
        for (const CellInfoQ75& cell : index.cells) {
            if (cell.hasGrid) selectedCells.insert(cell.formId);
        }
        return;
    }

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const auto found =
                index.gridCells.find(GridKeyQ1800(targetGridX + dx, targetGridY + dy));
            if (found == index.gridCells.end()) continue;
            for (uint32_t cellFormId : found->second) selectedCells.insert(cellFormId);
        }
    }
}

void ResolveIndexedBasesQ1800(const Q1800WorldspaceIndex& index,
                              const std::unordered_set<uint32_t>& wanted,
                              std::unordered_map<uint32_t, BaseRecordQ75>& out) {
    out.clear();
    out.reserve(wanted.size());
    for (uint32_t formId : wanted) {
        const auto found = index.bases.find(formId);
        if (found != index.bases.end()) out.emplace(formId, found->second);
    }
}


bool EndsInNifQ75(const std::string& path) {
    if (path.size() < 4u) return false;
    std::string tail = path.substr(path.size() - 4u);
    for (char& c : tail) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return tail == ".nif";
}

void ConvertBethesdaRotationQ75(float rx, float ry, float rz,
                                float& outRx, float& outRy, float& outRz) {
    const float sx = std::sin(rx), cx = std::cos(rx);
    const float sy = std::sin(ry), cy = std::cos(ry);
    const float sz = std::sin(rz), cz = std::cos(rz);
    const float m00 = cy * cz;
    const float m01 = sz * cy;
    const float m10 = sx * sy * cz - sz * cx;
    const float m11 = sx * sy * sz + cx * cz;
    const float m20 = sx * sz + sy * cx * cz;
    const float m21 = -sx * cz + sy * sz * cx;
    const float m22 = cx * cy;
    outRy = std::asin(std::clamp(-m20, -1.0f, 1.0f));
    const float cosY = std::cos(outRy);
    if (std::fabs(cosY) > 1e-5f) {
        outRx = std::atan2(m21, m22);
        outRz = std::atan2(m10, m00);
    } else {
        outRx = 0.0f;
        outRz = std::atan2(-m01, m11);
    }
}

} // namespace

thread_local int gQ1950WorldspaceGridRadiusOverride = -1;

void SetFo3WorldspaceGridRadiusOverrideQ1950(int radius) {
    gQ1950WorldspaceGridRadiusOverride = radius;
}

bool LoadFo3WorldspaceNeighborhoodQ75(uint32_t worldspaceFormId,
                                     uint32_t persistentCellFormId,
                                     float arrivalX, float arrivalY,
                                     std::vector<Fo3WorldPlacement>& outPlacements) {
    outPlacements.clear();

    const auto lookupStartedQ1800 = std::chrono::steady_clock::now();
    const Q1800WorldspaceIndex* q1800Index =
        worldspaceFormId == WASTELAND_WORLDSPACE_Q1800 ? GetWastelandIndexQ1800() : nullptr;

    std::vector<CellInfoQ75> legacyCellsQ1800;
    const std::vector<CellInfoQ75>* cellsQ1800 = nullptr;
    if (q1800Index) {
        cellsQ1800 = &q1800Index->cells;
    } else {
        if (!DiscoverWorldspaceCellsQ75(worldspaceFormId, legacyCellsQ1800)) {
            Q75_LOGE("Q7.5 WORLDSPACE LOAD FAILED: worldspace=%08X reason=discover-cells",
                     worldspaceFormId);
            return false;
        }
        cellsQ1800 = &legacyCellsQ1800;
    }
    const std::vector<CellInfoQ75>& cells = *cellsQ1800;

    std::vector<const CellInfoQ75*> gridCells;
    if (!q1800Index) {
        for (const CellInfoQ75& cell : cells) if (cell.hasGrid) gridCells.push_back(&cell);
    }
    const size_t gridCellCountQ1800 =
        q1800Index ? q1800Index->gridCellCount : gridCells.size();
    const int32_t targetGridX = static_cast<int32_t>(std::floor(arrivalX / EXTERIOR_CELL_SIZE_Q75));
    const int32_t targetGridY = static_cast<int32_t>(std::floor(arrivalY / EXTERIOR_CELL_SIZE_Q75));
    const bool loadWholeWorldspace = gridCellCountQ1800 <= SMALL_WORLDSPACE_CELL_LIMIT_Q75;
    const int q1950SelectionRadius = gQ1950WorldspaceGridRadiusOverride >= 0
        ? gQ1950WorldspaceGridRadiusOverride : 2; // Q16.26 initial exterior 5x5 resident

    std::unordered_set<uint32_t> selectedCells;
    if (q1800Index) {
        SelectIndexedCellsQ1800(*q1800Index, persistentCellFormId,
                                targetGridX, targetGridY, q1950SelectionRadius,
                                loadWholeWorldspace, selectedCells);
    } else {
        if (persistentCellFormId != 0u) selectedCells.insert(persistentCellFormId);
        for (const CellInfoQ75* cell : gridCells) {
            if (loadWholeWorldspace ||
                (std::abs(cell->gridX - targetGridX) <= q1950SelectionRadius &&
                 std::abs(cell->gridY - targetGridY) <= q1950SelectionRadius)) {
                selectedCells.insert(cell->formId);
            }
        }
    }

    Q75_LOGI("Q16.26 VISUAL WINDOW: worldspace=%08X persistent=%08X discovered=%zu gridCells=%zu selected=%zu targetGrid=(%d,%d) mode=%s radius=%d source=%s",
             worldspaceFormId, persistentCellFormId, cells.size(), gridCellCountQ1800,
             selectedCells.size(), targetGridX, targetGridY,
             loadWholeWorldspace ? "full-small-worldspace" : "arrival-neighborhood",
             q1950SelectionRadius, q1800Index ? "Q18-index" : "ESM-scan");
    if (q1800Index) {
        for (uint32_t cellFormId : selectedCells) {
            const auto cellIndex = q1800Index->cellByFormId.find(cellFormId);
            if (cellIndex == q1800Index->cellByFormId.end()) continue;
            const CellInfoQ75& cell = q1800Index->cells[cellIndex->second];
            if (!cell.hasGrid) continue;
            Q75_LOGI("Q7.5 GRID CELL: cell=%08X grid=(%d,%d) EDID=%s selected=1",
                     cell.formId, cell.gridX, cell.gridY,
                     cell.editorId.empty() ? "<none>" : cell.editorId.c_str());
        }
    } else {
        for (const CellInfoQ75* cell : gridCells) {
            if (selectedCells.find(cell->formId) == selectedCells.end()) continue;
            Q75_LOGI("Q7.5 GRID CELL: cell=%08X grid=(%d,%d) EDID=%s selected=1",
                     cell->formId, cell->gridX, cell->gridY,
                     cell->editorId.empty() ? "<none>" : cell->editorId.c_str());
        }
    }

    std::vector<RawPlacementQ75> raw;
    size_t landRecords = 0u;
    const bool refsReadyQ1800 = q1800Index
        ? CollectIndexedRefsQ1800(*q1800Index, selectedCells, raw, landRecords)
        : CollectSelectedRefsQ75(worldspaceFormId, selectedCells, raw, landRecords);
    if (!refsReadyQ1800) {
        Q75_LOGE("Q7.5 WORLDSPACE LOAD FAILED: worldspace=%08X reason=collect-refs selectedCells=%zu source=%s",
                 worldspaceFormId, selectedCells.size(),
                 q1800Index ? "Q18-index" : "ESM-scan");
        return false;
    }

    size_t q1770PersistentBefore = 0u;
    for (const RawPlacementQ75& p : raw) {
        if (p.owningCellFormId == persistentCellFormId) ++q1770PersistentBefore;
    }
    raw.erase(std::remove_if(raw.begin(), raw.end(),
                             [&](const RawPlacementQ75& p) {
        if (p.owningCellFormId != persistentCellFormId) return false;
        const int32_t gx = static_cast<int32_t>(std::floor(p.x / EXTERIOR_CELL_SIZE_Q75));
        const int32_t gy = static_cast<int32_t>(std::floor(p.y / EXTERIOR_CELL_SIZE_Q75));
        return std::abs(gx - targetGridX) > GRID_RADIUS_Q75 ||
               std::abs(gy - targetGridY) > GRID_RADIUS_Q75;
    }), raw.end());
    size_t q1770PersistentAfter = 0u;
    for (const RawPlacementQ75& p : raw) {
        if (p.owningCellFormId == persistentCellFormId) ++q1770PersistentAfter;
    }
    Q75_LOGI("Q16.7 PERSISTENT LOCAL CULL: worldspace=%08X persistent=%08X before=%zu kept=%zu removed=%zu targetGrid=(%d,%d) radius=%d",
             worldspaceFormId, persistentCellFormId,
             q1770PersistentBefore, q1770PersistentAfter,
             q1770PersistentBefore - q1770PersistentAfter,
             targetGridX, targetGridY, GRID_RADIUS_Q75);

    std::unordered_map<uint32_t, const RawPlacementQ75*> refs;
    refs.reserve(raw.size());
    for (const RawPlacementQ75& p : raw) refs[p.refFormId] = &p;
    std::unordered_map<uint32_t, bool> memo;
    std::vector<const RawPlacementQ75*> active;
    active.reserve(raw.size());
    size_t disabled = 0u;
    for (const RawPlacementQ75& p : raw) {
        std::unordered_set<uint32_t> visiting;
        if (ResolveInitialEnabledQ75(p.refFormId, refs, memo, visiting)) active.push_back(&p);
        else ++disabled;
    }

    std::unordered_set<uint32_t> wanted;
    for (const RawPlacementQ75* p : active) if (p->baseFormId != 0u) wanted.insert(p->baseFormId);
    std::unordered_map<uint32_t, BaseRecordQ75> bases;
    if (q1800Index) {
        ResolveIndexedBasesQ1800(*q1800Index, wanted, bases);
    } else if (!ResolveBasesQ75(wanted, bases)) {
        Q75_LOGE("Q7.5 WORLDSPACE LOAD FAILED: worldspace=%08X reason=resolve-bases wanted=%zu",
                 worldspaceFormId, wanted.size());
        return false;
    }

    std::unordered_map<uint32_t, const CellInfoQ75*> cellInfoByFormQ1800;
    cellInfoByFormQ1800.reserve(cells.size());
    for (const CellInfoQ75& cell : cells) cellInfoByFormQ1800[cell.formId] = &cell;

    std::unordered_map<std::string, size_t> baseTypes;
    std::unordered_set<std::string> uniqueModels;
    size_t q1770MegatonEntranceSkipped = 0u;
    for (const RawPlacementQ75* p : active) {
        const auto it = bases.find(p->baseFormId);
        if (it == bases.end()) continue;
        const BaseRecordQ75& base = it->second;

        const bool q1770SuppressMegatonEntrance =
            worldspaceFormId == 0x0000003Cu &&
            (p->refFormId == 0x00003B24u ||
             p->refFormId == 0x0001D55Au ||
             p->refFormId == 0x0006D4ADu);
        if (q1770SuppressMegatonEntrance) {
            ++q1770MegatonEntranceSkipped;
            Q75_LOGI("Q16.7 MEGATON ENTRANCE SUPPRESSED: ref=%08X base=%08X EDID=%s model=%s",
                     p->refFormId, p->baseFormId,
                     base.editorId.empty() ? "<none>" : base.editorId.c_str(),
                     base.modelPath.empty() ? "<none>" : base.modelPath.c_str());
            continue;
        }

        ++baseTypes[base.recordType.empty() ? "<none>" : base.recordType];
        if (base.modelPath.empty() || !EndsInNifQ75(base.modelPath)) continue;
        Fo3WorldPlacement world;
        world.refFormId = p->refFormId;
        world.baseFormId = p->baseFormId;
        world.baseRecordType = base.recordType;
        world.editorId = base.editorId;
        world.modelPath = base.modelPath;
        world.x = p->x;
        world.y = p->y;
        world.z = p->z;
        ConvertBethesdaRotationQ75(p->rx, p->ry, p->rz, world.rx, world.ry, world.rz);
        world.scale = p->scale;

        world.owningCellFormId = p->owningCellFormId;
        if (p->owningCellFormId == persistentCellFormId) {
            // Persistent-CELL REFRs are spatially resident according to their
            // authored DATA position, matching the moving exterior window.
            world.gridX = static_cast<int32_t>(std::floor(p->x / EXTERIOR_CELL_SIZE_Q75));
            world.gridY = static_cast<int32_t>(std::floor(p->y / EXTERIOR_CELL_SIZE_Q75));
            world.hasExteriorGrid = true;
            world.persistentExteriorRef = true;
        } else {
            const auto owningCell = cellInfoByFormQ1800.find(p->owningCellFormId);
            if (owningCell != cellInfoByFormQ1800.end() && owningCell->second->hasGrid) {
                world.gridX = owningCell->second->gridX;
                world.gridY = owningCell->second->gridY;
                world.hasExteriorGrid = true;
            }
        }

        uniqueModels.insert(world.modelPath);
        outPlacements.push_back(std::move(world));
    }

    Q75_LOGI("Q16.7 EXTERIOR TEST READY: entranceSkipped=%zu persistentKept=%zu radius=%d", q1770MegatonEntranceSkipped, q1770PersistentAfter, GRID_RADIUS_Q75);
    const auto lookupElapsedQ1800 = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - lookupStartedQ1800).count();
    Q75_LOGI("Q7.5 WORLDSPACE READY: worldspace=%08X persistent=%08X selectedCells=%zu LAND=%zu rawRefs=%zu activeRefs=%zu disabledInitial=%zu bases=%zu modelPlacements=%zu uniqueModels=%zu XTEL=(%.2f %.2f) source=%s lookupUs=%lld",
             worldspaceFormId, persistentCellFormId, selectedCells.size(), landRecords,
             raw.size(), active.size(), disabled, bases.size(), outPlacements.size(), uniqueModels.size(),
             arrivalX, arrivalY, q1800Index ? "Q18-index" : "ESM-scan",
             static_cast<long long>(lookupElapsedQ1800));
    for (const auto& entry : baseTypes) {
        Q75_LOGI("Q7.5 BASE TYPE: type=%s placements=%zu", entry.first.c_str(), entry.second);
    }
    return !outPlacements.empty();
}
