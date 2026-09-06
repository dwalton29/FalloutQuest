#include "fo3-megaton-scene.h"

#include <android/log.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <zlib.h>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* ESM_PATH =
        "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout3.esm";
constexpr uint32_t TARGET_CELL_FORM_ID = 0x000151E3u;
constexpr uint32_t FLAG_COMPRESSED = 0x00040000u;
constexpr uint32_t FLAG_INITIALLY_DISABLED = 0x00000800u;
constexpr uint8_t ENABLE_PARENT_OPPOSITE = 0x01u;
constexpr uint64_t HEADER_SIZE = 24u;
constexpr uint32_t MAX_RECORD_BYTES = 64u * 1024u * 1024u;

#define Q6A_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6A_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q6A_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define Q6J_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6J_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

struct GroupFrame {
    uint64_t end = 0;
    uint32_t label = 0;
    uint32_t type = 0;
};

struct RawPlacement {
    uint32_t refFormId = 0;
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

struct BaseRecord {
    uint32_t formId = 0;
    std::string recordType;
    std::string editorId;
    std::string modelPath;
};

uint16_t ReadLe16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

uint32_t ReadLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

float ReadLeFloat(const uint8_t* p) {
    const uint32_t bits = ReadLe32(p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool ReadExact(FILE* file, void* dst, size_t size) {
    return std::fread(dst, 1, size, file) == size;
}

int64_t FileSize(FILE* file) {
    const off_t current = ftello(file);
    if (current < 0) return -1;
    if (fseeko(file, 0, SEEK_END) != 0) return -1;
    const off_t end = ftello(file);
    fseeko(file, current, SEEK_SET);
    return static_cast<int64_t>(end);
}

std::string FourCC(const uint8_t* p) {
    char value[5]{static_cast<char>(p[0]), static_cast<char>(p[1]),
                  static_cast<char>(p[2]), static_cast<char>(p[3]), 0};
    return std::string(value);
}

bool InflateRecord(const std::vector<uint8_t>& stored, std::vector<uint8_t>& out) {
    if (stored.size() < 4u) return false;
    const uint32_t inflatedSize = ReadLe32(stored.data());
    if (inflatedSize == 0u || inflatedSize > MAX_RECORD_BYTES) return false;

    out.resize(inflatedSize);
    uLongf destLen = static_cast<uLongf>(out.size());
    const int result = uncompress(reinterpret_cast<Bytef*>(out.data()), &destLen,
                                  reinterpret_cast<const Bytef*>(stored.data() + 4),
                                  static_cast<uLong>(stored.size() - 4u));
    if (result != Z_OK || destLen != inflatedSize) {
        out.clear();
        return false;
    }
    return true;
}

bool ReadPayload(FILE* file, uint32_t storedSize, uint32_t flags,
                 std::vector<uint8_t>& out) {
    if (storedSize == 0u || storedSize > MAX_RECORD_BYTES) return false;
    std::vector<uint8_t> stored(storedSize);
    if (!ReadExact(file, stored.data(), stored.size())) return false;
    if ((flags & FLAG_COMPRESSED) == 0u) {
        out.swap(stored);
        return true;
    }
    return InflateRecord(stored, out);
}

void WalkSubrecords(const uint8_t* data, size_t size,
                    const std::function<void(const char*, const uint8_t*, uint32_t)>& visitor) {
    size_t pos = 0;
    uint32_t extendedSize = 0;
    while (pos + 6u <= size) {
        const char* type = reinterpret_cast<const char*>(data + pos);
        const uint16_t size16 = ReadLe16(data + pos + 4u);
        pos += 6u;

        if (std::memcmp(type, "XXXX", 4) == 0) {
            if (size16 != 4u || pos + 4u > size) return;
            extendedSize = ReadLe32(data + pos);
            pos += 4u;
            continue;
        }

        const uint32_t subSize = extendedSize ? extendedSize : size16;
        extendedSize = 0;
        if (subSize > size - pos) return;
        visitor(type, data + pos, subSize);
        pos += subSize;
    }
}

bool IsMegatonChildGroup(uint32_t label, uint32_t type) {
    return label == TARGET_CELL_FORM_ID &&
           (type == 6u || type == 8u || type == 9u || type == 10u);
}

bool InMegatonChildren(const std::vector<GroupFrame>& groups) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (IsMegatonChildGroup(it->label, it->type)) return true;
    }
    return false;
}

bool ParsePlacement(const std::vector<uint8_t>& payload, RawPlacement& out) {
    bool haveBase = false;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "NAME", 4) == 0 && size >= 4u) {
            out.baseFormId = ReadLe32(bytes);
            haveBase = out.baseFormId != 0u;
        } else if (std::memcmp(type, "DATA", 4) == 0 && size >= 24u) {
            out.x = ReadLeFloat(bytes + 0u);
            out.y = ReadLeFloat(bytes + 4u);
            out.z = ReadLeFloat(bytes + 8u);
            out.rx = ReadLeFloat(bytes + 12u);
            out.ry = ReadLeFloat(bytes + 16u);
            out.rz = ReadLeFloat(bytes + 20u);
            out.hasTransform = true;
        } else if (std::memcmp(type, "XSCL", 4) == 0 && size >= 4u) {
            out.scale = ReadLeFloat(bytes);
            if (!(out.scale > 0.0001f && out.scale < 1000.0f)) out.scale = 1.0f;
        } else if (std::memcmp(type, "XESP", 4) == 0 && size >= 4u) {
            out.hasEnableParent = true;
            out.enableParentFormId = ReadLe32(bytes);
            if (size >= 5u) out.enableParentFlags = bytes[4u];
        }
    });
    return haveBase && out.hasTransform;
}

BaseRecord ParseBaseRecord(uint32_t formId, const std::string& recordType,
                           const std::vector<uint8_t>& payload) {
    BaseRecord out;
    out.formId = formId;
    out.recordType = recordType;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "EDID", 4) == 0 && out.editorId.empty()) {
            size_t len = 0;
            while (len < size && bytes[len] != 0) ++len;
            out.editorId.assign(reinterpret_cast<const char*>(bytes), len);
        } else if (std::memcmp(type, "MODL", 4) == 0 && out.modelPath.empty()) {
            size_t len = 0;
            while (len < size && bytes[len] != 0) ++len;
            out.modelPath.assign(reinterpret_cast<const char*>(bytes), len);
        }
    });
    return out;
}

bool CollectReferences(std::vector<RawPlacement>& placements) {
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    std::vector<GroupFrame> groups;
    uint32_t childGroups = 0;

    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32(header + 4u);

        if (std::memcmp(header, "GRUP", 4) == 0) {
            if (sizeField < HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            const uint32_t label = ReadLe32(header + 8u);
            const uint32_t type = ReadLe32(header + 12u);
            groups.push_back(GroupFrame{offset + sizeField, label, type});
            if (IsMegatonChildGroup(label, type)) ++childGroups;
            continue;
        }

        const uint32_t flags = ReadLe32(header + 8u);
        const uint32_t formId = ReadLe32(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;

        const bool wanted = InMegatonChildren(groups) && std::memcmp(header, "REFR", 4) == 0;
        if (!wanted) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;
        RawPlacement p;
        p.refFormId = formId;
        p.recordFlags = flags;
        if (ParsePlacement(payload, p)) placements.push_back(p);
    }

    std::fclose(file);
    Q6A_LOGI("Q6A ESM REFR COLLECT: cell=%08X groups=%u placements=%zu",
             TARGET_CELL_FORM_ID, childGroups, placements.size());
    return childGroups > 0u && !placements.empty();
}

bool ResolveBases(const std::unordered_set<uint32_t>& wanted,
                  std::unordered_map<uint32_t, BaseRecord>& out) {
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32(header + 4u);
        if (std::memcmp(header, "GRUP", 4) == 0) {
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
        out[formId] = ParseBaseRecord(formId, FourCC(header), payload);
        if (out.size() == wanted.size()) break;
    }

    std::fclose(file);
    return true;
}

bool EndsInNif(const std::string& path) {
    if (path.size() < 4u) return false;
    const size_t n = path.size();
    const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(path[n - 3u])));
    const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(path[n - 2u])));
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(path[n - 1u])));
    return path[n - 4u] == '.' && a == 'n' && b == 'i' && c == 'f';
}

bool IsMegatonArchitecturePath(const std::string& path) {
    std::string lower = path;
    for (char& ch : lower) {
        if (ch == '/') ch = '\\';
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lower.find("architecture\\megaton\\interior\\shackinteriors") != std::string::npos;
}

bool ResolveInitialEnabled(uint32_t refFormId,
                           const std::unordered_map<uint32_t, const RawPlacement*>& refs,
                           std::unordered_map<uint32_t, bool>& memo,
                           std::unordered_set<uint32_t>& visiting,
                           size_t& unresolvedParents,
                           size_t& cycles) {
    const auto memoIt = memo.find(refFormId);
    if (memoIt != memo.end()) return memoIt->second;
    const auto it = refs.find(refFormId);
    if (it == refs.end()) {
        ++unresolvedParents;
        return true;
    }
    if (!visiting.insert(refFormId).second) {
        ++cycles;
        return true;
    }

    const RawPlacement& p = *it->second;
    bool enabled = true;
    if (p.hasEnableParent && p.enableParentFormId != 0u) {
        enabled = ResolveInitialEnabled(p.enableParentFormId, refs, memo, visiting,
                                        unresolvedParents, cycles);
        if ((p.enableParentFlags & ENABLE_PARENT_OPPOSITE) != 0u) enabled = !enabled;
    } else {
        enabled = (p.recordFlags & FLAG_INITIALLY_DISABLED) == 0u;
    }

    visiting.erase(refFormId);
    memo[refFormId] = enabled;
    return enabled;
}

void ConvertBethesdaRotation(float rx, float ry, float rz,
                             float& outRx, float& outRy, float& outRz) {
    // Bethesda REFR angles are clockwise-positive. The runtime renderer's
    // ApplyEsmRotation builds the conventional Rz*Ry*Rx matrix, so decompose
    // transpose(Rz(rz)*Ry(ry)*Rx(rx)) back into that same representation.
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

bool LoadMegatonPlayerHousePlacements(std::vector<Fo3WorldPlacement>& outPlacements) {
    outPlacements.clear();

    std::vector<RawPlacement> raw;
    if (!CollectReferences(raw)) {
        Q6A_LOGE("Q6A ESM: failed to collect MegatonPlayerHouse REFR records");
        return false;
    }

    std::unordered_map<uint32_t, const RawPlacement*> refs;
    refs.reserve(raw.size());
    for (const RawPlacement& p : raw) refs[p.refFormId] = &p;

    // Evaluate the ESM's initial enable-parent graph rather than dropping every
    // XESP child. This reproduces the cell's authored initial state: theme groups
    // whose parent marker starts disabled remain hidden, while ordinary house
    // contents controlled by enabled parents stay visible.
    std::unordered_map<uint32_t, bool> enabledMemo;
    enabledMemo.reserve(raw.size());
    std::vector<const RawPlacement*> activeRaw;
    activeRaw.reserve(raw.size());
    size_t disabledStandalone = 0;
    size_t disabledByParent = 0;
    size_t enabledParented = 0;
    size_t unresolvedParents = 0;
    size_t cycles = 0;
    for (const RawPlacement& p : raw) {
        std::unordered_set<uint32_t> visiting;
        const bool enabled = ResolveInitialEnabled(p.refFormId, refs, enabledMemo, visiting,
                                                   unresolvedParents, cycles);
        if (!enabled) {
            if (p.hasEnableParent) ++disabledByParent;
            else ++disabledStandalone;
            continue;
        }
        if (p.hasEnableParent) ++enabledParented;
        activeRaw.push_back(&p);
    }
    Q6J_LOGI("Q6J ESM STATE: raw=%zu activeInitial=%zu disabledStandalone=%zu disabledByParent=%zu enabledParented=%zu unresolvedParents=%zu cycles=%zu policy=initial-enable-graph",
             raw.size(), activeRaw.size(), disabledStandalone, disabledByParent,
             enabledParented, unresolvedParents, cycles);

    std::unordered_set<uint32_t> wanted;
    wanted.reserve(activeRaw.size());
    for (const RawPlacement* p : activeRaw) wanted.insert(p->baseFormId);

    std::unordered_map<uint32_t, BaseRecord> bases;
    if (!ResolveBases(wanted, bases)) {
        Q6A_LOGE("Q6A ESM: failed to resolve base records");
        return false;
    }

    bool loggedDoorAnchor = false;
    size_t rotationConverted = 0;
    for (const RawPlacement* rawPlacement : activeRaw) {
        const RawPlacement& p = *rawPlacement;
        const auto it = bases.find(p.baseFormId);
        if (it == bases.end() || it->second.modelPath.empty() || !EndsInNif(it->second.modelPath)) continue;
        const BaseRecord& base = it->second;

        Fo3WorldPlacement world;
        world.refFormId = p.refFormId;
        world.baseFormId = p.baseFormId;
        world.baseRecordType = base.recordType;
        world.editorId = base.editorId;
        world.modelPath = base.modelPath;
        world.x = p.x;
        world.y = p.y;
        world.z = p.z;
        ConvertBethesdaRotation(p.rx, p.ry, p.rz, world.rx, world.ry, world.rz);
        ++rotationConverted;
        world.scale = p.scale;

        // Q6H derives VR (0,0) from model paths classified as Megaton structure.
        // Keep the exit door in that classifier and use forward slashes for the
        // remaining architecture (the BSA/collision loaders normalize them).
        // This makes the authored ground-floor entrance the sole structural
        // spawn anchor without changing any world-space relationships.
        if (IsMegatonArchitecturePath(world.modelPath) &&
            world.editorId != "ShackExitDoorReg01") {
            for (char& ch : world.modelPath) if (ch == '\\') ch = '/';
        } else if (world.editorId == "ShackExitDoorReg01" && !loggedDoorAnchor) {
            loggedDoorAnchor = true;
            Q6J_LOGI("Q6J SPAWN ANCHOR: ref=%08X EDID=%s P=(%.1f %.1f %.1f) source=ground-floor-exit-door",
                     world.refFormId, world.editorId.c_str(), world.x, world.y, world.z);
        }

        outPlacements.push_back(std::move(world));
    }

    Q6J_LOGI("Q6J ROTATION CONVENTION: placements=%zu bethesdaClockwise=1 rendererZYXDecomposition=1",
             rotationConverted);
    if (!loggedDoorAnchor) {
        Q6J_LOGW("Q6J SPAWN ANCHOR: ShackExitDoorReg01 not found; Q6H structural-centre fallback will be used");
    }

    Q6A_LOGI("Q6A ESM READY: resolvedBases=%zu/%zu modelPlacements=%zu",
             bases.size(), wanted.size(), outPlacements.size());

    const size_t preview = std::min<size_t>(outPlacements.size(), 12u);
    for (size_t i = 0; i < preview; ++i) {
        const Fo3WorldPlacement& p = outPlacements[i];
        Q6A_LOGI("Q6A ESM OBJECT[%zu]: ref=%08X base=%08X type=%s EDID=%s MODL=%s P=(%.1f %.1f %.1f) R=(%.3f %.3f %.3f) S=%.3f",
                 i, p.refFormId, p.baseFormId, p.baseRecordType.c_str(),
                 p.editorId.empty() ? "<none>" : p.editorId.c_str(),
                 p.modelPath.c_str(), p.x, p.y, p.z, p.rx, p.ry, p.rz, p.scale);
    }

    return !outPlacements.empty();
}
