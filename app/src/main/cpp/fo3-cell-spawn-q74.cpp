#include "fo3-transition-q74.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Keep the proven Q7.1 input/raycast implementation byte-for-byte in this
// translation unit, but rename its public entry point so Q7.4 can wrap it.
#define ProbeMegatonPlayerHouseDoorQ71 ProbeMegatonPlayerHouseDoorQ71BaseQ74
#include "fo3-cell-spawn.cpp"
#undef ProbeMegatonPlayerHouseDoorQ71

namespace {

constexpr uint32_t Q74_FLAG_INITIALLY_DISABLED = 0x00000800u;
constexpr uint8_t Q74_ENABLE_PARENT_OPPOSITE = 0x01u;

struct Q74Owner {
    uint32_t cellFormId = 0;
    uint32_t worldspaceFormId = 0;
    bool valid = false;
};

struct Q74RawPlacement {
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

struct Q74BaseRecord {
    uint32_t formId = 0;
    std::string recordType;
    std::string editorId;
    std::string modelPath;
};

Fo3CellTransitionRequestQ74 gPendingTransitionQ74;
bool gHasPendingTransitionQ74 = false;
bool gPlayerResetPendingQ74 = false;
uint32_t gCurrentCellQ74 = TARGET_CELL_FORM_ID;

std::string Q74FourCC(const uint8_t* p) {
    char value[5]{static_cast<char>(p[0]), static_cast<char>(p[1]),
                  static_cast<char>(p[2]), static_cast<char>(p[3]), 0};
    return std::string(value);
}

bool Q74EndsInNif(const std::string& path) {
    if (path.size() < 4u) return false;
    const size_t n = path.size();
    const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(path[n - 3u])));
    const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(path[n - 2u])));
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(path[n - 1u])));
    return path[n - 4u] == '.' && a == 'n' && b == 'i' && c == 'f';
}

bool Q74InCellChildren(const std::vector<GroupFrame>& groups, uint32_t cellFormId) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (it->label == cellFormId &&
            (it->type == 6u || it->type == 8u || it->type == 9u || it->type == 10u)) {
            return true;
        }
    }
    return false;
}

bool Q74ParsePlacement(const std::vector<uint8_t>& payload, Q74RawPlacement& out) {
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

bool Q74CollectCellRefs(uint32_t cellFormId, std::vector<Q74RawPlacement>& out) {
    out.clear();
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    std::vector<GroupFrame> groups;
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
            groups.push_back(GroupFrame{offset + sizeField,
                                        ReadLe32(header + 8u),
                                        ReadLe32(header + 12u)});
            continue;
        }

        const uint32_t flags = ReadLe32(header + 8u);
        const uint32_t formId = ReadLe32(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (!Q74InCellChildren(groups, cellFormId) || std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;
        Q74RawPlacement placement;
        placement.refFormId = formId;
        placement.recordFlags = flags;
        if (Q74ParsePlacement(payload, placement)) out.push_back(placement);
    }

    std::fclose(file);
    return !out.empty();
}

bool Q74ResolveInitialEnabled(uint32_t refFormId,
                              const std::unordered_map<uint32_t, const Q74RawPlacement*>& refs,
                              std::unordered_map<uint32_t, bool>& memo,
                              std::unordered_set<uint32_t>& visiting) {
    const auto memoIt = memo.find(refFormId);
    if (memoIt != memo.end()) return memoIt->second;
    const auto it = refs.find(refFormId);
    if (it == refs.end()) return true;
    if (!visiting.insert(refFormId).second) return true;

    const Q74RawPlacement& p = *it->second;
    bool enabled = true;
    if (p.hasEnableParent && p.enableParentFormId != 0u) {
        enabled = Q74ResolveInitialEnabled(p.enableParentFormId, refs, memo, visiting);
        if ((p.enableParentFlags & Q74_ENABLE_PARENT_OPPOSITE) != 0u) enabled = !enabled;
    } else {
        enabled = (p.recordFlags & Q74_FLAG_INITIALLY_DISABLED) == 0u;
    }
    visiting.erase(refFormId);
    memo[refFormId] = enabled;
    return enabled;
}

bool Q74ResolveBases(const std::unordered_set<uint32_t>& wanted,
                     std::unordered_map<uint32_t, Q74BaseRecord>& out) {
    out.clear();
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
        Q74BaseRecord base;
        base.formId = formId;
        base.recordType = Q74FourCC(header);
        WalkSubrecords(payload.data(), payload.size(),
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

void Q74ConvertBethesdaRotation(float rx, float ry, float rz,
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

bool Q74ResolveOwner(uint32_t targetDoorRef, Q74Owner& out) {
    out = {};
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    std::vector<GroupFrame> groups;
    bool found = false;
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
            groups.push_back(GroupFrame{offset + sizeField,
                                        ReadLe32(header + 8u),
                                        ReadLe32(header + 12u)});
            continue;
        }
        const uint32_t formId = ReadLe32(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (formId == targetDoorRef && std::memcmp(header, "REFR", 4u) == 0) {
            for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
                if (out.cellFormId == 0u &&
                    (it->type == 6u || it->type == 8u || it->type == 9u || it->type == 10u)) {
                    out.cellFormId = it->label;
                }
                if (out.worldspaceFormId == 0u && it->type == 1u) out.worldspaceFormId = it->label;
            }
            found = true;
            break;
        }
        if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
    }
    std::fclose(file);
    out.valid = found && out.cellFormId != 0u;
    return out.valid;
}

bool Q74FindDoorHit(float originX, float originY, float originZ,
                    float dirX, float dirY, float dirZ,
                    DoorProbeCandidate& outDoor) {
    const float length = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (length < 1e-5f) return false;
    dirX /= length; dirY /= length; dirZ /= length;

    static bool attempted = false;
    static std::vector<DoorProbeCandidate> doors;
    if (!attempted) {
        attempted = true;
        CollectTargetCellDoorProbes(doors);
    }
    if (doors.empty()) return false;

    Fo3CellArrival arrival;
    if (!LoadMegatonPlayerHouseArrival(arrival) || !arrival.valid) return false;

    const DoorProbeCandidate* best = nullptr;
    float bestT = Q71_MAX_RAY_METRES + 1.0f;
    for (const DoorProbeCandidate& door : doors) {
        const float vrX = (door.gameX - arrival.x) / Q71_UNITS_PER_METRE;
        const float vrY = Q71_FLOOR_Y + (door.gameZ - arrival.z) / Q71_UNITS_PER_METRE;
        const float vrZ = Q71_SCENE_FORWARD - (door.gameY - arrival.y) / Q71_UNITS_PER_METRE;
        float t = 0.0f;
        if (RaySphere(originX, originY, originZ, dirX, dirY, dirZ,
                      vrX, vrY, vrZ, Q71_DOOR_RADIUS_METRES, t) && t < bestT) {
            best = &door;
            bestT = t;
        }
    }
    if (!best) return false;
    outDoor = *best;
    return true;
}

} // namespace

bool LoadFo3CellPlacementsQ74(uint32_t cellFormId,
                             std::vector<Fo3WorldPlacement>& outPlacements) {
    outPlacements.clear();
    std::vector<Q74RawPlacement> raw;
    if (!Q74CollectCellRefs(cellFormId, raw)) {
        Q71_LOGE("Q7.4 CELL LOAD FAILED: cell=%08X reason=collect-refs", cellFormId);
        return false;
    }

    std::unordered_map<uint32_t, const Q74RawPlacement*> refs;
    refs.reserve(raw.size());
    for (const Q74RawPlacement& p : raw) refs[p.refFormId] = &p;

    std::unordered_map<uint32_t, bool> memo;
    std::vector<const Q74RawPlacement*> active;
    active.reserve(raw.size());
    size_t disabled = 0u;
    for (const Q74RawPlacement& p : raw) {
        std::unordered_set<uint32_t> visiting;
        if (Q74ResolveInitialEnabled(p.refFormId, refs, memo, visiting)) active.push_back(&p);
        else ++disabled;
    }

    std::unordered_set<uint32_t> wanted;
    for (const Q74RawPlacement* p : active) wanted.insert(p->baseFormId);
    std::unordered_map<uint32_t, Q74BaseRecord> bases;
    if (!Q74ResolveBases(wanted, bases)) {
        Q71_LOGE("Q7.4 CELL LOAD FAILED: cell=%08X reason=resolve-bases", cellFormId);
        return false;
    }

    for (const Q74RawPlacement* p : active) {
        const auto it = bases.find(p->baseFormId);
        if (it == bases.end()) continue;
        const Q74BaseRecord& base = it->second;
        if (base.modelPath.empty() || !Q74EndsInNif(base.modelPath)) continue;
        Fo3WorldPlacement world;
        world.refFormId = p->refFormId;
        world.baseFormId = p->baseFormId;
        world.baseRecordType = base.recordType;
        world.editorId = base.editorId;
        world.modelPath = base.modelPath;
        world.x = p->x; world.y = p->y; world.z = p->z;
        Q74ConvertBethesdaRotation(p->rx, p->ry, p->rz, world.rx, world.ry, world.rz);
        world.scale = p->scale;
        outPlacements.push_back(std::move(world));
    }

    Q71_LOGI("Q7.4 CELL PLACEMENTS READY: cell=%08X rawRefs=%zu activeRefs=%zu disabledInitial=%zu bases=%zu modelPlacements=%zu",
             cellFormId, raw.size(), active.size(), disabled, bases.size(), outPlacements.size());
    return !outPlacements.empty();
}

bool ConsumeFo3CellTransitionRequestQ74(Fo3CellTransitionRequestQ74& outRequest) {
    if (!gHasPendingTransitionQ74 || !gPendingTransitionQ74.valid) return false;
    outRequest = gPendingTransitionQ74;
    gHasPendingTransitionQ74 = false;
    return true;
}

void CompleteFo3CellTransitionQ74(uint32_t cellFormId) {
    gCurrentCellQ74 = cellFormId;
    gPlayerResetPendingQ74 = true;
    Q71_LOGI("Q7.4 TRANSITION APPLIED: cell=%08X playerReset=NEXT_PHYSICS_FRAME orientation=preserved",
             cellFormId);
}

bool ConsumeFo3PlayerResetQ74() {
    if (!gPlayerResetPendingQ74) return false;
    gPlayerResetPendingQ74 = false;
    return true;
}

bool ProbeMegatonPlayerHouseDoorQ71(float originX, float originY, float originZ,
                                    float dirX, float dirY, float dirZ) {
    const bool hit = ProbeMegatonPlayerHouseDoorQ71BaseQ74(originX, originY, originZ,
                                                           dirX, dirY, dirZ);
    if (!hit) return false;
    if (gHasPendingTransitionQ74) return true;

    DoorProbeCandidate door;
    if (!Q74FindDoorHit(originX, originY, originZ, dirX, dirY, dirZ, door)) {
        Q71_LOGE("Q7.4 TRANSITION REQUEST FAILED: proven Q7.1 hit could not recover door metadata");
        return true;
    }

    Q74Owner owner;
    if (!Q74ResolveOwner(door.destinationDoorRef, owner) || !owner.valid) {
        Q71_LOGE("Q7.4 TRANSITION REQUEST FAILED: destinationDoor=%08X owner unresolved",
                 door.destinationDoorRef);
        return true;
    }
    if (owner.cellFormId == gCurrentCellQ74) return true;

    gPendingTransitionQ74 = {};
    gPendingTransitionQ74.destinationDoorRef = door.destinationDoorRef;
    gPendingTransitionQ74.cellFormId = owner.cellFormId;
    gPendingTransitionQ74.worldspaceFormId = owner.worldspaceFormId;
    gPendingTransitionQ74.x = door.teleportX;
    gPendingTransitionQ74.y = door.teleportY;
    gPendingTransitionQ74.z = door.teleportZ;
    gPendingTransitionQ74.rx = door.teleportRx;
    gPendingTransitionQ74.ry = door.teleportRy;
    gPendingTransitionQ74.rz = door.teleportRz;
    gPendingTransitionQ74.valid = true;
    gHasPendingTransitionQ74 = true;

    Q71_LOGI("Q7.4 TRANSITION REQUESTED: destinationDoor=%08X cell=%08X worldspace=%08X XTEL=(%.2f %.2f %.2f) R=(%.4f %.4f %.4f) queue=render-thread",
             door.destinationDoorRef, owner.cellFormId, owner.worldspaceFormId,
             door.teleportX, door.teleportY, door.teleportZ,
             door.teleportRx, door.teleportRy, door.teleportRz);
    return true;
}
