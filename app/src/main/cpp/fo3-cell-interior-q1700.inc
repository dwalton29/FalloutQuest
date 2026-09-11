// Q16.0 generic interior CELL placement loader.
//
// Included into the Q7.4 transition translation unit immediately after the
// proven fo3-cell-spawn.cpp parser, so it reuses that TU's compressed-record,
// subrecord and endian helpers. Unlike the original MegatonPlayerHouse loader,
// the target CELL FormID is supplied at runtime and there are no house-specific
// model-path/spawn hacks.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Q1700Interior {

struct Frame {
    uint64_t end = 0u;
    uint32_t label = 0u;
    uint32_t type = 0u;
};

struct RawPlacement {
    uint32_t refFormId = 0u;
    uint32_t baseFormId = 0u;
    uint32_t recordFlags = 0u;
    uint32_t enableParentFormId = 0u;
    uint8_t enableParentFlags = 0u;
    bool hasEnableParent = false;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float rx = 0.0f, ry = 0.0f, rz = 0.0f;
    float scale = 1.0f;
    bool hasTransform = false;
};

struct BaseRecord {
    uint32_t formId = 0u;
    std::string recordType;
    std::string editorId;
    std::string modelPath;
};

constexpr uint32_t Q1700_INITIALLY_DISABLED = 0x00000800u;
constexpr uint8_t Q1700_ENABLE_PARENT_OPPOSITE = 0x01u;

bool InCellChildren(const std::vector<Frame>& groups, uint32_t cellFormId) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (it->label == cellFormId &&
            (it->type == 6u || it->type == 8u || it->type == 9u || it->type == 10u)) {
            return true;
        }
    }
    return false;
}

bool ParsePlacementPayload(const std::vector<uint8_t>& payload, RawPlacement& out) {
    bool haveBase = false;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "NAME", 4u) == 0 && size >= 4u) {
            out.baseFormId = ReadLe32(bytes);
            haveBase = out.baseFormId != 0u;
        } else if (std::memcmp(type, "DATA", 4u) == 0 && size >= 24u) {
            out.x = ReadLeFloat(bytes + 0u);
            out.y = ReadLeFloat(bytes + 4u);
            out.z = ReadLeFloat(bytes + 8u);
            out.rx = ReadLeFloat(bytes + 12u);
            out.ry = ReadLeFloat(bytes + 16u);
            out.rz = ReadLeFloat(bytes + 20u);
            out.hasTransform = true;
        } else if (std::memcmp(type, "XSCL", 4u) == 0 && size >= 4u) {
            out.scale = ReadLeFloat(bytes);
            if (!(out.scale > 0.0001f && out.scale < 1000.0f)) out.scale = 1.0f;
        } else if (std::memcmp(type, "XESP", 4u) == 0 && size >= 4u) {
            out.hasEnableParent = true;
            out.enableParentFormId = ReadLe32(bytes);
            if (size >= 5u) out.enableParentFlags = bytes[4u];
        }
    });
    return haveBase && out.hasTransform;
}

bool CollectCellReferences(uint32_t cellFormId, std::vector<RawPlacement>& out) {
    out.clear();
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    std::vector<Frame> groups;
    uint32_t matchingGroups = 0u;
    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            const uint32_t label = ReadLe32(header + 8u);
            const uint32_t type = ReadLe32(header + 12u);
            groups.push_back(Frame{offset + sizeField, label, type});
            if (label == cellFormId &&
                (type == 6u || type == 8u || type == 9u || type == 10u)) {
                ++matchingGroups;
            }
            continue;
        }

        const uint32_t flags = ReadLe32(header + 8u);
        const uint32_t formId = ReadLe32(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (!InCellChildren(groups, cellFormId) || std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;
        RawPlacement placement;
        placement.refFormId = formId;
        placement.recordFlags = flags;
        if (ParsePlacementPayload(payload, placement)) out.push_back(placement);
    }

    std::fclose(file);
    Q71_LOGI("Q16.0 INTERIOR REFR COLLECT: cell=%08X groups=%u placements=%zu",
             cellFormId, matchingGroups, out.size());
    return matchingGroups > 0u && !out.empty();
}

BaseRecord ParseBase(uint32_t formId, const char* recordType,
                     const std::vector<uint8_t>& payload) {
    BaseRecord out;
    out.formId = formId;
    out.recordType.assign(recordType, 4u);
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "EDID", 4u) == 0 && out.editorId.empty()) {
            size_t n = 0u;
            while (n < size && bytes[n] != 0u) ++n;
            out.editorId.assign(reinterpret_cast<const char*>(bytes), n);
        } else if (std::memcmp(type, "MODL", 4u) == 0 && out.modelPath.empty()) {
            size_t n = 0u;
            while (n < size && bytes[n] != 0u) ++n;
            out.modelPath.assign(reinterpret_cast<const char*>(bytes), n);
        }
    });
    return out;
}

bool ResolveBases(const std::unordered_set<uint32_t>& wanted,
                  std::unordered_map<uint32_t, BaseRecord>& out) {
    out.clear();
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    while (out.size() < wanted.size()) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;
        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            continue;
        }

        const uint32_t flags = ReadLe32(header + 8u);
        const uint32_t formId = ReadLe32(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (wanted.find(formId) == wanted.end()) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;
        out[formId] = ParseBase(formId, reinterpret_cast<const char*>(header), payload);
    }
    std::fclose(file);
    return !out.empty();
}

bool ResolveEnabled(uint32_t refFormId,
                    const std::unordered_map<uint32_t, const RawPlacement*>& refs,
                    std::unordered_map<uint32_t, bool>& memo,
                    std::unordered_set<uint32_t>& visiting) {
    const auto memoIt = memo.find(refFormId);
    if (memoIt != memo.end()) return memoIt->second;
    const auto refIt = refs.find(refFormId);
    if (refIt == refs.end()) return true;
    if (!visiting.insert(refFormId).second) return true;

    const RawPlacement& p = *refIt->second;
    bool enabled = true;
    if (p.hasEnableParent && p.enableParentFormId != 0u) {
        enabled = ResolveEnabled(p.enableParentFormId, refs, memo, visiting);
        if ((p.enableParentFlags & Q1700_ENABLE_PARENT_OPPOSITE) != 0u) enabled = !enabled;
    } else {
        enabled = (p.recordFlags & Q1700_INITIALLY_DISABLED) == 0u;
    }
    visiting.erase(refFormId);
    memo[refFormId] = enabled;
    return enabled;
}

bool EndsInNif(const std::string& path) {
    if (path.size() < 4u) return false;
    const size_t n = path.size();
    return path[n - 4u] == '.' &&
           std::tolower(static_cast<unsigned char>(path[n - 3u])) == 'n' &&
           std::tolower(static_cast<unsigned char>(path[n - 2u])) == 'i' &&
           std::tolower(static_cast<unsigned char>(path[n - 1u])) == 'f';
}

void ConvertBethesdaRotation(float rx, float ry, float rz,
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

} // namespace Q1700Interior

bool LoadFo3InteriorCellPlacementsQ1700(uint32_t cellFormId,
                                        std::vector<Fo3WorldPlacement>& outPlacements) {
    outPlacements.clear();
    std::vector<Q1700Interior::RawPlacement> raw;
    if (!Q1700Interior::CollectCellReferences(cellFormId, raw)) {
        Q71_LOGE("Q16.0 INTERIOR CELL FAILED: cell=%08X reason=no-REFR-children", cellFormId);
        return false;
    }

    std::unordered_map<uint32_t, const Q1700Interior::RawPlacement*> refs;
    refs.reserve(raw.size());
    for (const auto& p : raw) refs[p.refFormId] = &p;

    std::unordered_map<uint32_t, bool> enabledMemo;
    std::vector<const Q1700Interior::RawPlacement*> active;
    active.reserve(raw.size());
    for (const auto& p : raw) {
        std::unordered_set<uint32_t> visiting;
        if (Q1700Interior::ResolveEnabled(p.refFormId, refs, enabledMemo, visiting)) {
            active.push_back(&p);
        }
    }

    std::unordered_set<uint32_t> wanted;
    wanted.reserve(active.size());
    for (const auto* p : active) wanted.insert(p->baseFormId);
    std::unordered_map<uint32_t, Q1700Interior::BaseRecord> bases;
    if (!Q1700Interior::ResolveBases(wanted, bases)) {
        Q71_LOGE("Q16.0 INTERIOR CELL FAILED: cell=%08X reason=base-resolve", cellFormId);
        return false;
    }

    for (const auto* p : active) {
        const auto baseIt = bases.find(p->baseFormId);
        if (baseIt == bases.end()) continue;
        const auto& base = baseIt->second;
        if (base.modelPath.empty() || !Q1700Interior::EndsInNif(base.modelPath)) continue;

        Fo3WorldPlacement world;
        world.refFormId = p->refFormId;
        world.baseFormId = p->baseFormId;
        world.baseRecordType = base.recordType;
        world.editorId = base.editorId;
        world.modelPath = base.modelPath;
        world.x = p->x; world.y = p->y; world.z = p->z;
        Q1700Interior::ConvertBethesdaRotation(p->rx, p->ry, p->rz,
                                               world.rx, world.ry, world.rz);
        world.scale = p->scale;
        outPlacements.push_back(std::move(world));
    }

    Q71_LOGI("Q16.0 INTERIOR CELL READY: cell=%08X raw=%zu active=%zu modelPlacements=%zu source=Fallout3.esm",
             cellFormId, raw.size(), active.size(), outPlacements.size());
    return !outPlacements.empty();
}
