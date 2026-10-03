#include "fo3-megaton-scene.h"
#include "fo3-esm-reader.h"

#include <android/log.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <functional>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* ESM_PATH =
        "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout3.esm";
constexpr uint32_t TARGET_CELL_FORM_ID = 0x000151E3u;
constexpr float Q71_UNITS_PER_METRE = 70.0f;
constexpr float Q71_FLOOR_Y = -1.55f;
constexpr float Q71_SCENE_FORWARD = 0.0f;
constexpr float Q71_DOOR_RADIUS_METRES = 1.15f;
constexpr float Q71_MAX_RAY_METRES = 3.0f;

#define Q6K_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6K_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q6K_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define Q71_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q71_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q71_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

struct GroupFrame {
    uint64_t end = 0;
    uint32_t label = 0;
    uint32_t type = 0;
};

struct ArrivalCandidate {
    uint32_t sourceDoorRef = 0;
    uint32_t destinationDoorRef = 0;
    uint32_t flags = 0;
    bool sourceInsideTargetCell = false;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
};

struct DoorProbeCandidate {
    uint32_t sourceDoorRef = 0;
    uint32_t destinationDoorRef = 0;
    uint32_t flags = 0;
    float gameX = 0.0f;
    float gameY = 0.0f;
    float gameZ = 0.0f;
    float teleportX = 0.0f;
    float teleportY = 0.0f;
    float teleportZ = 0.0f;
    float teleportRx = 0.0f;
    float teleportRy = 0.0f;
    float teleportRz = 0.0f;
};

bool IsTargetChildGroup(uint32_t label, uint32_t type) {
    return label == TARGET_CELL_FORM_ID &&
           (type == 6u || type == 8u || type == 9u || type == 10u);
}

bool InTargetCell(const std::vector<GroupFrame>& groups) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (IsTargetChildGroup(it->label, it->type)) return true;
    }
    return false;
}

bool CollectTargetCellRefs(std::unordered_set<uint32_t>& refs) {
    refs.clear();
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = fo3esm::FileSize(file);
    if (fileSize < static_cast<int64_t>(fo3esm::HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    std::vector<GroupFrame> groups;
    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + fo3esm::HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[fo3esm::HEADER_SIZE]{};
        if (!fo3esm::ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = fo3esm::ReadU32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < fo3esm::HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrame{offset + sizeField,
                                        fo3esm::ReadU32(header + 8u),
                                        fo3esm::ReadU32(header + 12u)});
            continue;
        }

        const uint64_t payloadEnd = offset + fo3esm::HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (InTargetCell(groups) && std::memcmp(header, "REFR", 4u) == 0) {
            refs.insert(fo3esm::ReadU32(header + 12u));
        }
        if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
    }

    std::fclose(file);
    Q6K_LOGI("Q6K XTEL TARGET REFS: cell=%08X refs=%zu", TARGET_CELL_FORM_ID, refs.size());
    return !refs.empty();
}

bool FindArrivalCandidates(const std::unordered_set<uint32_t>& targetRefs,
                           std::vector<ArrivalCandidate>& candidates) {
    candidates.clear();
    FILE* file = std::fopen(ESM_PATH, "rb");
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
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < fo3esm::HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            continue;
        }

        const uint32_t recordFlags = fo3esm::ReadU32(header + 8u);
        const uint32_t sourceRef = fo3esm::ReadU32(header + 12u);
        const uint64_t payloadEnd = offset + fo3esm::HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;

        if (std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!fo3esm::ReadPayloadCurrent(file, sizeField, recordFlags, payload)) break;
        fo3esm::WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "XTEL", 4u) != 0 || size < 28u) return;
            const uint32_t destinationDoor = fo3esm::ReadU32(bytes + 0u);
            if (targetRefs.find(destinationDoor) == targetRefs.end()) return;

            ArrivalCandidate candidate;
            candidate.sourceDoorRef = sourceRef;
            candidate.destinationDoorRef = destinationDoor;
            candidate.sourceInsideTargetCell = targetRefs.find(sourceRef) != targetRefs.end();
            candidate.x = fo3esm::ReadF32(bytes + 4u);
            candidate.y = fo3esm::ReadF32(bytes + 8u);
            candidate.z = fo3esm::ReadF32(bytes + 12u);
            candidate.rx = fo3esm::ReadF32(bytes + 16u);
            candidate.ry = fo3esm::ReadF32(bytes + 20u);
            candidate.rz = fo3esm::ReadF32(bytes + 24u);
            if (size >= 32u) candidate.flags = fo3esm::ReadU32(bytes + 28u);
            candidates.push_back(candidate);
        });
    }

    std::fclose(file);
    return !candidates.empty();
}

bool CollectTargetCellDoorProbes(std::vector<DoorProbeCandidate>& doors) {
    doors.clear();
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = fo3esm::FileSize(file);
    if (fileSize < static_cast<int64_t>(fo3esm::HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    std::vector<GroupFrame> groups;
    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + fo3esm::HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[fo3esm::HEADER_SIZE]{};
        if (!fo3esm::ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = fo3esm::ReadU32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < fo3esm::HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrame{offset + sizeField,
                                        fo3esm::ReadU32(header + 8u),
                                        fo3esm::ReadU32(header + 12u)});
            continue;
        }

        const uint32_t recordFlags = fo3esm::ReadU32(header + 8u);
        const uint32_t sourceRef = fo3esm::ReadU32(header + 12u);
        const uint64_t payloadEnd = offset + fo3esm::HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (!InTargetCell(groups) || std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!fo3esm::ReadPayloadCurrent(file, sizeField, recordFlags, payload)) break;
        DoorProbeCandidate candidate;
        candidate.sourceDoorRef = sourceRef;
        bool haveData = false;
        bool haveTeleport = false;
        fo3esm::WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "DATA", 4u) == 0 && size >= 12u) {
                candidate.gameX = fo3esm::ReadF32(bytes + 0u);
                candidate.gameY = fo3esm::ReadF32(bytes + 4u);
                candidate.gameZ = fo3esm::ReadF32(bytes + 8u);
                haveData = true;
            } else if (std::memcmp(type, "XTEL", 4u) == 0 && size >= 28u) {
                candidate.destinationDoorRef = fo3esm::ReadU32(bytes + 0u);
                candidate.teleportX = fo3esm::ReadF32(bytes + 4u);
                candidate.teleportY = fo3esm::ReadF32(bytes + 8u);
                candidate.teleportZ = fo3esm::ReadF32(bytes + 12u);
                candidate.teleportRx = fo3esm::ReadF32(bytes + 16u);
                candidate.teleportRy = fo3esm::ReadF32(bytes + 20u);
                candidate.teleportRz = fo3esm::ReadF32(bytes + 24u);
                if (size >= 32u) candidate.flags = fo3esm::ReadU32(bytes + 28u);
                haveTeleport = candidate.destinationDoorRef != 0u;
            }
        });
        if (haveData && haveTeleport) doors.push_back(candidate);
    }

    std::fclose(file);
    return !doors.empty();
}

bool RaySphere(float ox, float oy, float oz,
               float dx, float dy, float dz,
               float cx, float cy, float cz,
               float radius, float& outT) {
    const float ocx = ox - cx;
    const float ocy = oy - cy;
    const float ocz = oz - cz;
    const float b = ocx * dx + ocy * dy + ocz * dz;
    const float c = ocx * ocx + ocy * ocy + ocz * ocz - radius * radius;
    const float discriminant = b * b - c;
    if (discriminant < 0.0f) return false;
    const float root = std::sqrt(discriminant);
    float t = -b - root;
    if (t < 0.0f) t = -b + root;
    if (t < 0.0f || t > Q71_MAX_RAY_METRES) return false;
    outT = t;
    return true;
}

} // namespace

bool LoadMegatonPlayerHouseArrival(Fo3CellArrival& outArrival) {
    static bool attempted = false;
    static bool ready = false;
    static Fo3CellArrival cached;
    if (attempted) {
        if (ready) outArrival = cached;
        return ready;
    }
    attempted = true;
    outArrival = {};

    std::unordered_set<uint32_t> targetRefs;
    if (!CollectTargetCellRefs(targetRefs)) {
        Q6K_LOGE("Q6K XTEL ARRIVAL FAILED: could not collect target-cell references");
        return false;
    }

    std::vector<ArrivalCandidate> candidates;
    if (!FindArrivalCandidates(targetRefs, candidates)) {
        Q6K_LOGE("Q6K XTEL ARRIVAL FAILED: no REFR XTEL links into cell=%08X", TARGET_CELL_FORM_ID);
        return false;
    }

    const ArrivalCandidate* chosen = nullptr;
    for (const ArrivalCandidate& candidate : candidates) {
        Q6K_LOGI("Q6K XTEL CANDIDATE: source=%08X destinationDoor=%08X sourceInsideTarget=%d P=(%.2f %.2f %.2f) R=(%.4f %.4f %.4f) flags=%08X",
                 candidate.sourceDoorRef, candidate.destinationDoorRef,
                 candidate.sourceInsideTargetCell ? 1 : 0,
                 candidate.x, candidate.y, candidate.z,
                 candidate.rx, candidate.ry, candidate.rz, candidate.flags);
        if (!chosen || (chosen->sourceInsideTargetCell && !candidate.sourceInsideTargetCell)) {
            chosen = &candidate;
        }
    }
    if (!chosen) return false;

    cached.sourceDoorRefFormId = chosen->sourceDoorRef;
    cached.destinationDoorRefFormId = chosen->destinationDoorRef;
    cached.x = chosen->x;
    cached.y = chosen->y;
    cached.z = chosen->z;
    cached.rx = chosen->rx;
    cached.ry = chosen->ry;
    cached.rz = chosen->rz;
    cached.flags = chosen->flags;
    cached.valid = true;
    outArrival = cached;
    ready = true;

    Q6K_LOGI("Q6K XTEL ARRIVAL: source=%08X destinationDoor=%08X candidates=%zu P=(%.2f %.2f %.2f) R=(%.4f %.4f %.4f) source=Fallout3.esm/XTEL exactGameSpawn=1",
             cached.sourceDoorRefFormId, cached.destinationDoorRefFormId,
             candidates.size(), cached.x, cached.y, cached.z,
             cached.rx, cached.ry, cached.rz);
    return true;
}

bool ProbeMegatonPlayerHouseDoorQ71(float originX, float originY, float originZ,
                                    float dirX, float dirY, float dirZ) {
    const float length = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (length < 1e-5f) {
        Q71_LOGW("Q7.1 DOOR MISS: invalid controller ray");
        return false;
    }
    dirX /= length;
    dirY /= length;
    dirZ /= length;

    static bool attempted = false;
    static std::vector<DoorProbeCandidate> cachedDoors;
    if (!attempted) {
        attempted = true;
        if (!CollectTargetCellDoorProbes(cachedDoors)) {
            Q71_LOGE("Q7.1 DOOR CACHE FAILED: no XTEL-bearing REFRs in cell=%08X",
                     TARGET_CELL_FORM_ID);
            return false;
        }
        Q71_LOGI("Q7.1 DOOR CACHE READY: cell=%08X doors=%zu lazyScan=firstTrigger-only",
                 TARGET_CELL_FORM_ID, cachedDoors.size());
        for (const DoorProbeCandidate& door : cachedDoors) {
            Q71_LOGI("Q7.1 DOOR CANDIDATE: source=%08X destinationDoor=%08X sourceP=(%.2f %.2f %.2f) XTEL=(%.2f %.2f %.2f)",
                     door.sourceDoorRef, door.destinationDoorRef,
                     door.gameX, door.gameY, door.gameZ,
                     door.teleportX, door.teleportY, door.teleportZ);
        }
    }
    if (cachedDoors.empty()) return false;

    Fo3CellArrival arrival;
    if (!LoadMegatonPlayerHouseArrival(arrival) || !arrival.valid) {
        Q71_LOGE("Q7.1 DOOR MISS: Q6K arrival origin unavailable");
        return false;
    }

    const DoorProbeCandidate* best = nullptr;
    float bestT = Q71_MAX_RAY_METRES + 1.0f;
    float bestVrX = 0.0f, bestVrY = 0.0f, bestVrZ = 0.0f;
    for (const DoorProbeCandidate& door : cachedDoors) {
        const float vrX = (door.gameX - arrival.x) / Q71_UNITS_PER_METRE;
        const float vrY = Q71_FLOOR_Y + (door.gameZ - arrival.z) / Q71_UNITS_PER_METRE;
        const float vrZ = Q71_SCENE_FORWARD - (door.gameY - arrival.y) / Q71_UNITS_PER_METRE;
        float t = 0.0f;
        if (RaySphere(originX, originY, originZ,
                      dirX, dirY, dirZ,
                      vrX, vrY, vrZ,
                      Q71_DOOR_RADIUS_METRES, t) && t < bestT) {
            best = &door;
            bestT = t;
            bestVrX = vrX;
            bestVrY = vrY;
            bestVrZ = vrZ;
        }
    }

    if (!best) {
        Q71_LOGI("Q7.1 DOOR MISS: candidates=%zu maxDistance=%.1fm radius=%.2fm rayOrigin=(%.2f %.2f %.2f)",
                 cachedDoors.size(), Q71_MAX_RAY_METRES, Q71_DOOR_RADIUS_METRES,
                 originX, originY, originZ);
        return false;
    }

    Q71_LOGI("Q7.1 DOOR HIT: source=%08X destinationDoor=%08X distance=%.2fm doorGame=(%.2f %.2f %.2f) doorVr=(%.2f %.2f %.2f) XTEL=(%.2f %.2f %.2f) R=(%.4f %.4f %.4f) flags=%08X transition=DISABLED",
             best->sourceDoorRef, best->destinationDoorRef, bestT,
             best->gameX, best->gameY, best->gameZ,
             bestVrX, bestVrY, bestVrZ,
             best->teleportX, best->teleportY, best->teleportZ,
             best->teleportRx, best->teleportRy, best->teleportRz,
             best->flags);
    return true;
}
