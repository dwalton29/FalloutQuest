// Q7.2/Q7.3 wrap the proven Q7.1 probe without modifying its implementation.
// Q7.2 resolves the linked destination REFR's owning CELL/WRLD. Q7.3 then
// shadow-builds that destination CELL's authored placed-object set while the
// working Q6K house remains live. No scene teardown or teleport occurs here.
#include "fo3-static-nif.h"

#include <string>
#include <unordered_map>

#define ProbeMegatonPlayerHouseDoorQ71 ProbeMegatonPlayerHouseDoorQ71Base
#include "fo3-cell-spawn.cpp"
#undef ProbeMegatonPlayerHouseDoorQ71

namespace {

constexpr uint32_t Q73_FLAG_INITIALLY_DISABLED = 0x00000800u;
constexpr size_t Q73_NIF_VALIDATION_CAP = 96u;
constexpr size_t Q73_PREVIEW_COUNT = 12u;

struct Q72DestinationOwner {
    uint32_t doorRef = 0;
    uint32_t cellFormId = 0;
    uint32_t worldspaceFormId = 0;
    uint32_t cellChildGroupType = 0;
    bool valid = false;
};

struct Q73Placement {
    uint32_t refFormId = 0;
    uint32_t baseFormId = 0;
    uint32_t recordFlags = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool hasEnableParent = false;
};

struct Q73BaseRecord {
    uint32_t formId = 0;
    std::string recordType;
    std::string editorId;
    std::string modelPath;
};

std::string Q73FourCC(const uint8_t* p) {
    char value[5]{static_cast<char>(p[0]), static_cast<char>(p[1]),
                  static_cast<char>(p[2]), static_cast<char>(p[3]), 0};
    return std::string(value);
}

bool Q73EndsInNif(const std::string& path) {
    if (path.size() < 4u) return false;
    const size_t n = path.size();
    const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(path[n - 3u])));
    const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(path[n - 2u])));
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(path[n - 1u])));
    return path[n - 4u] == '.' && a == 'n' && b == 'i' && c == 'f';
}

bool Q73InCellChildren(const std::vector<GroupFrame>& groups, uint32_t cellFormId) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (it->label == cellFormId &&
            (it->type == 6u || it->type == 8u || it->type == 9u || it->type == 10u)) {
            return true;
        }
    }
    return false;
}

bool ResolveDestinationOwnerQ72(uint32_t targetDoorRef, Q72DestinationOwner& out) {
    out = {};
    out.doorRef = targetDoorRef;

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) {
        Q71_LOGE("Q7.2 DESTINATION RESOLVE FAILED: door=%08X reason=open-esm", targetDoorRef);
        return false;
    }
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        Q71_LOGE("Q7.2 DESTINATION RESOLVE FAILED: door=%08X reason=bad-esm-size", targetDoorRef);
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
                    out.cellChildGroupType = it->type;
                }
                if (out.worldspaceFormId == 0u && it->type == 1u) {
                    out.worldspaceFormId = it->label;
                }
            }
            found = true;
            break;
        }

        if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
    }

    std::fclose(file);
    out.valid = found && out.cellFormId != 0u;
    if (!out.valid) {
        Q71_LOGE("Q7.2 DESTINATION RESOLVE FAILED: door=%08X foundRef=%d cell=%08X worldspace=%08X",
                 targetDoorRef, found ? 1 : 0, out.cellFormId, out.worldspaceFormId);
        return false;
    }

    Q71_LOGI("Q7.2 DESTINATION RESOLVED: door=%08X cell=%08X worldspace=%08X childGroupType=%u transition=DISABLED sceneReload=DISABLED",
             out.doorRef, out.cellFormId, out.worldspaceFormId, out.cellChildGroupType);
    return true;
}

uint32_t DestinationDoorForHitQ72(float originX, float originY, float originZ,
                                  float dirX, float dirY, float dirZ) {
    const float length = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (length < 1e-5f) return 0u;
    dirX /= length;
    dirY /= length;
    dirZ /= length;

    static bool attempted = false;
    static std::vector<DoorProbeCandidate> doors;
    if (!attempted) {
        attempted = true;
        CollectTargetCellDoorProbes(doors);
    }
    if (doors.empty()) return 0u;

    Fo3CellArrival arrival;
    if (!LoadMegatonPlayerHouseArrival(arrival) || !arrival.valid) return 0u;

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
    return best ? best->destinationDoorRef : 0u;
}

bool CollectDestinationCellQ73(const Q72DestinationOwner& owner,
                               std::vector<Q73Placement>& placements,
                               std::unordered_map<std::string, size_t>& recordTypes,
                               size_t& landRecords,
                               size_t& initiallyDisabled,
                               size_t& enableParented) {
    placements.clear();
    recordTypes.clear();
    landRecords = 0u;
    initiallyDisabled = 0u;
    enableParented = 0u;

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

        if (!Q73InCellChildren(groups, owner.cellFormId)) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        const std::string recordType = Q73FourCC(header);
        ++recordTypes[recordType];
        if (recordType == "LAND") ++landRecords;

        if (recordType != "REFR") {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;

        Q73Placement placement;
        placement.refFormId = formId;
        placement.recordFlags = flags;
        bool haveBase = false;
        bool havePosition = false;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "NAME", 4u) == 0 && size >= 4u) {
                placement.baseFormId = ReadLe32(bytes);
                haveBase = placement.baseFormId != 0u;
            } else if (std::memcmp(type, "DATA", 4u) == 0 && size >= 12u) {
                placement.x = ReadLeFloat(bytes + 0u);
                placement.y = ReadLeFloat(bytes + 4u);
                placement.z = ReadLeFloat(bytes + 8u);
                havePosition = true;
            } else if (std::memcmp(type, "XESP", 4u) == 0 && size >= 4u) {
                placement.hasEnableParent = true;
            }
        });
        if ((flags & Q73_FLAG_INITIALLY_DISABLED) != 0u) ++initiallyDisabled;
        if (placement.hasEnableParent) ++enableParented;
        if (haveBase && havePosition) placements.push_back(placement);
    }

    std::fclose(file);
    return !placements.empty() || landRecords > 0u;
}

bool ResolveDestinationBasesQ73(const std::unordered_set<uint32_t>& wanted,
                                std::unordered_map<uint32_t, Q73BaseRecord>& out) {
    out.clear();
    if (wanted.empty()) return true;

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

        Q73BaseRecord base;
        base.formId = formId;
        base.recordType = Q73FourCC(header);
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

bool ProbeDestinationCellQ73(const Q72DestinationOwner& owner) {
    Q71_LOGI("Q7.3 DESTINATION CELL BEGIN: door=%08X cell=%08X worldspace=%08X shadowBuild=1 renderSwap=DISABLED teleport=DISABLED",
             owner.doorRef, owner.cellFormId, owner.worldspaceFormId);

    std::vector<Q73Placement> placements;
    std::unordered_map<std::string, size_t> cellRecordTypes;
    size_t landRecords = 0u;
    size_t initiallyDisabled = 0u;
    size_t enableParented = 0u;
    if (!CollectDestinationCellQ73(owner, placements, cellRecordTypes,
                                   landRecords, initiallyDisabled, enableParented)) {
        Q71_LOGE("Q7.3 DESTINATION CELL FAILED: cell=%08X reason=collect-cell-records",
                 owner.cellFormId);
        return false;
    }

    std::unordered_set<uint32_t> wantedBases;
    wantedBases.reserve(placements.size());
    for (const Q73Placement& placement : placements) {
        if (placement.baseFormId != 0u) wantedBases.insert(placement.baseFormId);
    }

    std::unordered_map<uint32_t, Q73BaseRecord> bases;
    if (!ResolveDestinationBasesQ73(wantedBases, bases)) {
        Q71_LOGE("Q7.3 DESTINATION CELL FAILED: cell=%08X reason=resolve-bases wanted=%zu",
                 owner.cellFormId, wantedBases.size());
        return false;
    }

    size_t modelPlacements = 0u;
    size_t unresolvedBases = 0u;
    std::unordered_map<std::string, size_t> baseTypes;
    std::unordered_set<std::string> uniqueModelSet;
    for (const Q73Placement& placement : placements) {
        const auto baseIt = bases.find(placement.baseFormId);
        if (baseIt == bases.end()) {
            ++unresolvedBases;
            continue;
        }
        const Q73BaseRecord& base = baseIt->second;
        ++baseTypes[base.recordType.empty() ? "<none>" : base.recordType];
        if (!base.modelPath.empty() && Q73EndsInNif(base.modelPath)) {
            ++modelPlacements;
            uniqueModelSet.insert(base.modelPath);
        }
    }

    std::vector<std::string> uniqueModels(uniqueModelSet.begin(), uniqueModelSet.end());
    std::sort(uniqueModels.begin(), uniqueModels.end());

    const size_t validateCount = std::min(uniqueModels.size(), Q73_NIF_VALIDATION_CAP);
    size_t supportedModels = 0u;
    size_t unsupportedModels = 0u;
    size_t visibleShapes = 0u;
    size_t triangles = 0u;
    for (size_t i = 0u; i < validateCount; ++i) {
        std::vector<Fo3StaticNifMesh> meshes;
        if (!LoadFo3StaticNifMeshes(uniqueModels[i], meshes)) {
            ++unsupportedModels;
            continue;
        }
        ++supportedModels;
        visibleShapes += meshes.size();
        for (const Fo3StaticNifMesh& mesh : meshes) triangles += mesh.indices.size() / 3u;
    }

    Q71_LOGI("Q7.3 CELL RECORDS: cell=%08X REFR=%zu LAND=%zu initiallyDisabled=%zu enableParented=%zu resolvedBases=%zu/%zu unresolvedPlacements=%zu",
             owner.cellFormId, placements.size(), landRecords,
             initiallyDisabled, enableParented, bases.size(), wantedBases.size(), unresolvedBases);
    for (const auto& entry : cellRecordTypes) {
        Q71_LOGI("Q7.3 CELL RECORD TYPE: type=%s count=%zu", entry.first.c_str(), entry.second);
    }
    for (const auto& entry : baseTypes) {
        Q71_LOGI("Q7.3 BASE TYPE: type=%s placements=%zu", entry.first.c_str(), entry.second);
    }

    size_t previewed = 0u;
    for (const Q73Placement& placement : placements) {
        if (previewed >= Q73_PREVIEW_COUNT) break;
        const auto baseIt = bases.find(placement.baseFormId);
        if (baseIt == bases.end()) continue;
        const Q73BaseRecord& base = baseIt->second;
        if (base.modelPath.empty() || !Q73EndsInNif(base.modelPath)) continue;
        Q71_LOGI("Q7.3 OBJECT[%zu]: ref=%08X base=%08X type=%s EDID=%s MODL=%s P=(%.1f %.1f %.1f)",
                 previewed, placement.refFormId, placement.baseFormId,
                 base.recordType.empty() ? "<none>" : base.recordType.c_str(),
                 base.editorId.empty() ? "<none>" : base.editorId.c_str(),
                 base.modelPath.c_str(), placement.x, placement.y, placement.z);
        ++previewed;
    }

    Q71_LOGI("Q7.3 DESTINATION CELL READY: door=%08X cell=%08X worldspace=%08X refs=%zu modelPlacements=%zu uniqueModels=%zu LAND=%zu nifValidated=%zu/%zu supported=%zu unsupported=%zu visibleShapes=%zu triangles=%zu validationCapped=%d shadowBuild=1 renderSwap=DISABLED teleport=DISABLED",
             owner.doorRef, owner.cellFormId, owner.worldspaceFormId,
             placements.size(), modelPlacements, uniqueModels.size(), landRecords,
             validateCount, uniqueModels.size(), supportedModels, unsupportedModels,
             visibleShapes, triangles,
             uniqueModels.size() > Q73_NIF_VALIDATION_CAP ? 1 : 0);
    return true;
}

} // namespace

bool ProbeMegatonPlayerHouseDoorQ71(float originX, float originY, float originZ,
                                    float dirX, float dirY, float dirZ) {
    const bool hit = ProbeMegatonPlayerHouseDoorQ71Base(originX, originY, originZ,
                                                        dirX, dirY, dirZ);
    if (!hit) return false;

    const uint32_t destinationDoor = DestinationDoorForHitQ72(originX, originY, originZ,
                                                               dirX, dirY, dirZ);
    if (destinationDoor == 0u) {
        Q71_LOGE("Q7.2 DESTINATION RESOLVE FAILED: Q7.1 hit had no matching destination door");
        return true;
    }

    static std::unordered_set<uint32_t> resolvedDoors;
    if (resolvedDoors.insert(destinationDoor).second) {
        Q72DestinationOwner owner;
        if (ResolveDestinationOwnerQ72(destinationDoor, owner) && owner.valid) {
            ProbeDestinationCellQ73(owner);
        }
    } else {
        Q71_LOGI("Q7.2 DESTINATION CACHE HIT: door=%08X transition=DISABLED", destinationDoor);
    }
    return true;
}
