#include "fo3-transition-q74.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

// Keep the proven Q7.1 input/raycast implementation byte-for-byte in this
// translation unit, but rename its public entry point so Q7.4/Q7.5 can wrap it.
#define ProbeMegatonPlayerHouseDoorQ71 ProbeMegatonPlayerHouseDoorQ71BaseQ74
#include "fo3-cell-spawn.cpp"
#undef ProbeMegatonPlayerHouseDoorQ71

// Q7.5 is included into this translation unit deliberately. This keeps the
// existing CMake/source graph unchanged while letting the render-thread Q7.4
// loader dispatch exterior worldspaces to the XCLC neighborhood assembler.
// The proven Q7.1 source already has an anonymous-namespace TAG symbol, so
// remap Q7.5's logger token while it is textually included here.
#define TAG Q75_TAG
#include "fo3-worldspace-q75.cpp"
#undef TAG

// Q7.6 reuses Q7.5's already-proven ESM/group helpers in this same translation
// unit and decodes the selected exterior CELL LAND/VHGT records.
#include "fo3-terrain-data-q76.cpp"

namespace {

struct Q74Owner {
    uint32_t cellFormId = 0;
    uint32_t worldspaceFormId = 0;
    bool valid = false;
};

Fo3CellTransitionRequestQ74 gPendingTransitionQ74;
bool gHasPendingTransitionQ74 = false;
bool gPlayerResetPendingQ74 = false;
uint32_t gCurrentCellQ74 = TARGET_CELL_FORM_ID;

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
    return out.valid;
}

bool Q74FindDoorHit(float originX, float originY, float originZ,
                    float dirX, float dirY, float dirZ,
                    DoorProbeCandidate& outDoor) {
    const float length = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (length < 1e-5f) return false;
    dirX /= length;
    dirY /= length;
    dirZ /= length;

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
    // For exterior load doors, the linked REFR often belongs to the WRLD's
    // persistent CELL. Rendering only that CELL produces the Q7.4 random-prop
    // soup. Q7.5 merges it with the XCLC cells; Q7.6 now decodes those same
    // cells' LAND records before collision/render rebuild.
    if (gPendingTransitionQ74.valid &&
        gPendingTransitionQ74.cellFormId == cellFormId &&
        gPendingTransitionQ74.worldspaceFormId != 0u) {
        const bool placementsReady =
            LoadFo3WorldspaceNeighborhoodQ75(gPendingTransitionQ74.worldspaceFormId,
                                              cellFormId,
                                              gPendingTransitionQ74.x,
                                              gPendingTransitionQ74.y,
                                              outPlacements);
        if (!placementsReady) return false;

        const bool terrainReady =
            LoadFo3TerrainQ76(gPendingTransitionQ74.worldspaceFormId,
                              gPendingTransitionQ74.x,
                              gPendingTransitionQ74.y,
                              gPendingTransitionQ74.z);
        Q71_LOGI("Q7.6 EXTERIOR ASSEMBLY: worldspace=%08X persistentCell=%08X placements=%zu terrainCells=%zu terrainReady=%d",
                 gPendingTransitionQ74.worldspaceFormId, cellFormId,
                 outPlacements.size(), GetFo3TerrainQ76().size(), terrainReady ? 1 : 0);
        return true;
    }

    Q71_LOGE("Q7.6 CELL LOAD FAILED: cell=%08X reason=no-worldspace-transition-context",
             cellFormId);
    outPlacements.clear();
    return false;
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
    Q71_LOGI("Q7.6 TRANSITION APPLIED: persistentCell=%08X playerReset=NEXT_PHYSICS_FRAME orientation=preserved worldspaceNeighborhood=1 terrain=LAND",
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
        Q71_LOGE("Q7.6 TRANSITION REQUEST FAILED: proven Q7.1 hit could not recover door metadata");
        return true;
    }

    Q74Owner owner;
    if (!Q74ResolveOwner(door.destinationDoorRef, owner) || !owner.valid) {
        Q71_LOGE("Q7.6 TRANSITION REQUEST FAILED: destinationDoor=%08X owner unresolved",
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

    Q71_LOGI("Q7.6 TRANSITION REQUESTED: destinationDoor=%08X persistentCell=%08X worldspace=%08X XTEL=(%.2f %.2f %.2f) R=(%.4f %.4f %.4f) queue=render-thread exteriorNeighborhood=1 terrain=LAND",
             door.destinationDoorRef, owner.cellFormId, owner.worldspaceFormId,
             door.teleportX, door.teleportY, door.teleportZ,
             door.teleportRx, door.teleportRy, door.teleportRz);
    return true;
}
