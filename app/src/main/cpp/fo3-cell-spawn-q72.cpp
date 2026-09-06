// Q7.2 wraps the proven Q7.1 probe without modifying its implementation.
// The original source is included under a renamed symbol; after a confirmed
// door hit we resolve only that linked destination REFR's owning CELL/WRLD.
#define ProbeMegatonPlayerHouseDoorQ71 ProbeMegatonPlayerHouseDoorQ71Base
#include "fo3-cell-spawn.cpp"
#undef ProbeMegatonPlayerHouseDoorQ71

namespace {

struct Q72DestinationOwner {
    uint32_t doorRef = 0;
    uint32_t cellFormId = 0;
    uint32_t worldspaceFormId = 0;
    uint32_t cellChildGroupType = 0;
    bool valid = false;
};

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
            // Cell child groups are labelled by their owning CELL FormID.
            // World children groups are labelled by their owning WRLD FormID.
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
        ResolveDestinationOwnerQ72(destinationDoor, owner);
    } else {
        Q71_LOGI("Q7.2 DESTINATION CACHE HIT: door=%08X transition=DISABLED", destinationDoor);
    }
    return true;
}
