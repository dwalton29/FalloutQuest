#include "fo3-install-paths.h"
#include "fo3-megaton-scene.h"
#include "fo3-esm-reader.h"

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

namespace {

constexpr const char* TAG = "FalloutQuest";

constexpr uint32_t TARGET_CELL_FORM_ID = 0x000151E3u;
constexpr uint32_t FLAG_INITIALLY_DISABLED = 0x00000800u;
constexpr uint8_t ENABLE_PARENT_OPPOSITE = 0x01u;

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
    fo3esm::WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "NAME", 4) == 0 && size >= 4u) {
            out.baseFormId = fo3esm::ReadU32(bytes);
            haveBase = out.baseFormId != 0u;
        } else if (std::memcmp(type, "DATA", 4) == 0 && size >= 24u) {
            out.x = fo3esm::ReadF32(bytes + 0u);
            out.y = fo3esm::ReadF32(bytes + 4u);
            out.z = fo3esm::ReadF32(bytes + 8u);
            out.rx = fo3esm::ReadF32(bytes + 12u);
            out.ry = fo3esm::ReadF32(bytes + 16u);
            out.rz = fo3esm::ReadF32(bytes + 20u);
            out.hasTransform = true;
        } else if (std::memcmp(type, "XSCL", 4) == 0 && size >= 4u) {
            out.scale = fo3esm::ReadF32(bytes);
            if (!(out.scale > 0.0001f && out.scale < 1000.0f)) out.scale = 1.0f;
        } else if (std::memcmp(type, "XESP", 4) == 0 && size >= 4u) {
            out.hasEnableParent = true;
            out.enableParentFormId = fo3esm::ReadU32(bytes);
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
    fo3esm::WalkSubrecords(payload.data(), payload.size(),
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
    FILE* file = std::fopen(fo3assets::FalloutMasterPath().c_str(), "rb");
    if (!file) return false;
    const int64_t fileSize = fo3esm::FileSize(file);
    if (fileSize < static_cast<int64_t>(fo3esm::HEADER_SIZE)) {
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
        if (offset + fo3esm::HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[fo3esm::HEADER_SIZE]{};
        if (!fo3esm::ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = fo3esm::ReadU32(header + 4u);

        if (std::memcmp(header, "GRUP", 4) == 0) {
            if (sizeField < fo3esm::HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            const uint32_t label = fo3esm::ReadU32(header + 8u);
            const uint32_t type = fo3esm::ReadU32(header + 12u);
            groups.push_back(GroupFrame{offset + sizeField, label, type});
            if (IsMegatonChildGroup(label, type)) ++childGroups;
            continue;
        }

        const uint32_t flags = fo3esm::ReadU32(header + 8u);
        const uint32_t formId = fo3esm::ReadU32(header + 12u);
        const uint64_t payloadEnd = offset + fo3esm::HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;

        const bool wanted = InMegatonChildren(groups) && std::memcmp(header, "REFR", 4) == 0;
        if (!wanted) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!fo3esm::ReadPayloadCurrent(file, sizeField, flags, payload)) break;
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
    FILE* file = std::fopen(fo3assets::FalloutMasterPath().c_str(), "rb");
    if (!file) return false;
    const int64_t fileSize = fo3esm::FileSize(file);
    if (fileSize < static_cast<int64_t>(fo3esm::HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        if (offset + fo3esm::HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[fo3esm::HEADER_SIZE]{};
        if (!fo3esm::ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = fo3esm::ReadU32(header + 4u);
        if (std::memcmp(header, "GRUP", 4) == 0) {
            if (sizeField < fo3esm::HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            continue;
        }

        const uint32_t flags = fo3esm::ReadU32(header + 8u);
        const uint32_t formId = fo3esm::ReadU32(header + 12u);
        const uint64_t payloadEnd = offset + fo3esm::HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;

        if (wanted.find(formId) == wanted.end()) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!fo3esm::ReadPayloadCurrent(file, sizeField, flags, payload)) break;
        out[formId] = ParseBaseRecord(formId, fo3esm::FourCC(header), payload);
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
