#include <chrono>
#include <unordered_map>
extern void PumpFo3AndroidEventsQ1860();
#include "fo3-door-prompt-q1840.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include "fo3-cell-traversal-q1700.h"
#include "fo3-transition-q74.h"
#include "fo3-terrain-q76.h"
#include "fo3-cell-traversal-q1700.h"
#include "fo3-loading-state-q1700.h"
#include "fo3-transition-q74.h"
#include "fo3-environment-q1000.h"
#include "fo3-visual-depth-q1010.h"
#include "fo3-authored-color-q1390.h"
#include "fo3-terrain-q76.h"

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
#include "fo3-cell-interior-q1700.inc"
#include "fo3-authored-door-query-q1870.inc"

bool LoadFo3CellPlacements(uint32_t cellFormId,
                           std::vector<Fo3WorldPlacement>& outPlacements) {
    return LoadFo3InteriorCellPlacementsQ1700(cellFormId, outPlacements);
}

// Q7.5 is included into this translation unit deliberately. This keeps the
// existing CMake/source graph unchanged while letting the render-thread Q7.4
// loader dispatch exterior worldspaces to the XCLC neighborhood assembler.
// The proven Q7.1 source already has an anonymous-namespace TAG symbol, so
// remap Q7.5's logger token while it is textually included here.
#define TAG Q75_TAG
#include "fo3-worldspace-runtime.inc"
#undef TAG

bool LookupFo3WastelandDoorTeleportQ1920(
        uint32_t sourceDoorRef,
        Fo3DoorTeleport* outTeleport,
        bool* outIndexReady) {
    if (outTeleport) *outTeleport = {};
    if (outIndexReady) *outIndexReady = false;
    if (sourceDoorRef == 0u || !outTeleport) return false;

    const Q1800WorldspaceIndex* index = GetWastelandIndexQ1800();
    if (!index) return false;
    if (outIndexReady) *outIndexReady = true;

    const auto found = index->doorTeleportsQ1920.find(sourceDoorRef);
    if (found == index->doorTeleportsQ1920.end()) return false;
    *outTeleport = found->second;
    return outTeleport->valid;
}

// Q7.6b reuses the exact same Q7.5 worldspace/group helpers, but only decodes
// CPU LAND/VHGT data. No collision or player-grounding code is touched here.
#include "fo3-terrain-data-runtime.inc"

// Q7.11 resolves LAND BTXT -> LTEX -> TXST -> TX00 so the terrain renderer can
// use Fallout 3's real landscape diffuse DDS instead of the brown debug colour.
#include "fo3-terrain-texture-q711.cpp"
#include "fo3-terrain-material-q1060.cpp"

// Standalone GLES terrain renderer. Its internal symbols are Q76B-prefixed so
// it is safe to include in this translation unit without disturbing Q7.1/Q7.5.
#include "fo3-terrain-render-runtime.inc"

// Q7.7 physical LAND sampler is also kept in this transition translation unit.
// It only becomes active after the exterior terrain upload has succeeded.
#include "fo3-terrain-ground-runtime.inc"

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

// Q6G collision debug rendering is disabled in the proven Q7.5 build. The
// collision header marks only that visual symbol weak, so this strong render
// hook safely supplies Q7.6b LAND instead. All collision/grounding functions
// remain the original Q7.5 implementations apart from Q7.7's explicitly
// guarded terrain-ground merge in the collision resolver.
void RenderFo3CollisionOverlay(const float* mvp16) {
    RenderFo3TerrainQ76(mvp16);
}

bool LoadFo3CellPlacementsQ74(uint32_t cellFormId,
                             std::vector<Fo3WorldPlacement>& outPlacements) {
    // For exterior load doors, the linked REFR often belongs to the WRLD's
    // persistent CELL. Rendering only that CELL produces the Q7.4 random-prop
    // soup. Q7.5 instead merges it with the actual XCLC exterior grid cells.
    ClearFo3CollisionPolicyQ710();
    if (gPendingTransitionQ74.valid &&
        gPendingTransitionQ74.cellFormId == cellFormId &&
        gPendingTransitionQ74.worldspaceFormId != 0u) {
        const bool loaded = LoadFo3WorldspaceNeighborhoodQ75(
            gPendingTransitionQ74.worldspaceFormId,
            cellFormId,
            gPendingTransitionQ74.x,
            gPendingTransitionQ74.y,
            outPlacements);
        if (loaded) {
            const size_t dynamicOnlyModels = ConfigureFo3CollisionPolicyQ710(outPlacements);
            size_t dynamicPlacements = 0u;
            for (const Fo3WorldPlacement& placement : outPlacements) {
                if (IsFo3DynamicCollisionRecordTypeQ710(placement.baseRecordType)) {
                    ++dynamicPlacements;
                }
            }
            Q71_LOGI("Q7.10 COLLISION POLICY: placements=%zu dynamicPlacements=%zu dynamicOnlyModels=%zu staticBhkPolicy=skip-dynamic-models visualPlacementsUntouched=1",
                     outPlacements.size(), dynamicPlacements, dynamicOnlyModels);
        }
        return loaded;
    }

    if (gPendingTransitionQ74.valid &&
        gPendingTransitionQ74.cellFormId == cellFormId &&
        gPendingTransitionQ74.worldspaceFormId == 0u) {
        ShutdownFo3TerrainRenderQ76();
        ActivateFo3TerrainGroundingQ77(0u, 0.0f, 0.0f, 0.0f,
                                       Q71_SCENE_FORWARD, Q71_FLOOR_Y,
                                       Q71_UNITS_PER_METRE);
        const bool loaded = LoadFo3CellPlacements(cellFormId, outPlacements);
        Q71_LOGI("Q16.0 INTERIOR CELL LOAD: cell=%08X placements=%zu loaded=%d source=Fallout3.esm",
                 cellFormId, outPlacements.size(), loaded ? 1 : 0);
        return loaded;
    }

    Q71_LOGE("Q16.0 CELL LOAD FAILED: cell=%08X reason=no-transition-context",
             cellFormId);
    outPlacements.clear();
    return false;
}

bool QueueFo3MegatonEntryQ1000() {
    constexpr uint32_t MEGATON_ENTRANCE_CELL = 0x00002DBDu;
    constexpr uint32_t MEGATON_WORLDSPACE = 0x00000A74u;
    constexpr uint32_t WASTELAND_WORLDSPACE = 0x0000003Cu;

    std::unordered_set<uint32_t> entranceRefs;
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) {
        Q71_LOGE("Q10.0 MEGATON ENTRY FAILED: reason=open-esm");
        return false;
    }
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        Q71_LOGE("Q10.0 MEGATON ENTRY FAILED: reason=bad-esm-size");
        return false;
    }

    // Pass 1: collect every REFR actually owned by MegatonEntrance. XTEL stores
    // a destination reference, so this gives us the authoritative target set
    // without hard-coding a gate reference FormID.
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
            if (sizeField < HEADER_SIZE ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrame{offset + sizeField,
                                        ReadLe32(header + 8u),
                                        ReadLe32(header + 12u)});
            continue;
        }

        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        bool inEntrance = false;
        for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
            if (it->label == MEGATON_ENTRANCE_CELL &&
                (it->type == 6u || it->type == 8u ||
                 it->type == 9u || it->type == 10u)) {
                inEntrance = true;
                break;
            }
        }
        if (inEntrance && std::memcmp(header, "REFR", 4u) == 0) {
            entranceRefs.insert(ReadLe32(header + 12u));
        }
        if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
    }

    if (entranceRefs.empty()) {
        std::fclose(file);
        Q71_LOGE("Q10.0 MEGATON ENTRY FAILED: cell=%08X reason=no-owned-refs",
                 MEGATON_ENTRANCE_CELL);
        return false;
    }

    // Pass 2: find an exterior-world REFR whose XTEL targets one of those refs.
    // Prefer the Capital Wasteland WRLD explicitly; a non-Megaton exterior is a
    // fallback only so this stays data-driven if ownership differs slightly.
    if (fseeko(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }
    groups.clear();

    struct EntryCandidateQ1000 {
        uint32_t sourceRef = 0u;
        uint32_t destinationRef = 0u;
        uint32_t sourceCell = 0u;
        uint32_t sourceWorld = 0u;
        uint32_t flags = 0u;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        float rx = 0.0f, ry = 0.0f, rz = 0.0f;
        int score = -1;
    } best;

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
            if (sizeField < HEADER_SIZE ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrame{offset + sizeField,
                                        ReadLe32(header + 8u),
                                        ReadLe32(header + 12u)});
            continue;
        }

        const uint32_t recordFlags = ReadLe32(header + 8u);
        const uint32_t sourceRef = ReadLe32(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        uint32_t sourceCell = 0u;
        uint32_t sourceWorld = 0u;
        for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
            if (sourceCell == 0u &&
                (it->type == 6u || it->type == 8u ||
                 it->type == 9u || it->type == 10u)) {
                sourceCell = it->label;
            }
            if (sourceWorld == 0u && it->type == 1u) sourceWorld = it->label;
        }

        // Interior doors have no worldspace. Megaton-internal doors should not
        // be mistaken for the town entrance either.
        if (sourceWorld == 0u || sourceWorld == MEGATON_WORLDSPACE) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, recordFlags, payload)) break;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "XTEL", 4u) != 0 || size < 28u) return;
            const uint32_t destinationRef = ReadLe32(bytes + 0u);
            if (entranceRefs.find(destinationRef) == entranceRefs.end()) return;
            const int score = sourceWorld == WASTELAND_WORLDSPACE ? 100 : 10;
            if (score <= best.score) return;
            best.sourceRef = sourceRef;
            best.destinationRef = destinationRef;
            best.sourceCell = sourceCell;
            best.sourceWorld = sourceWorld;
            best.x = ReadLeFloat(bytes + 4u);
            best.y = ReadLeFloat(bytes + 8u);
            best.z = ReadLeFloat(bytes + 12u);
            best.rx = ReadLeFloat(bytes + 16u);
            best.ry = ReadLeFloat(bytes + 20u);
            best.rz = ReadLeFloat(bytes + 24u);
            best.flags = size >= 32u ? ReadLe32(bytes + 28u) : 0u;
            best.score = score;
        });
    }
    std::fclose(file);

    if (best.score < 0 || best.destinationRef == 0u) {
        Q71_LOGE("Q10.0 MEGATON ENTRY FAILED: cell=%08X targetRefs=%zu reason=no-exterior-XTEL",
                 MEGATON_ENTRANCE_CELL, entranceRefs.size());
        return false;
    }

    gPendingTransitionQ74 = {};
    gPendingTransitionQ74.destinationDoorRef = best.destinationRef;
    gPendingTransitionQ74.cellFormId = MEGATON_ENTRANCE_CELL;
    gPendingTransitionQ74.worldspaceFormId = MEGATON_WORLDSPACE;
    gPendingTransitionQ74.x = best.x;
    gPendingTransitionQ74.y = best.y;
    gPendingTransitionQ74.z = best.z;
    gPendingTransitionQ74.rx = best.rx;
    gPendingTransitionQ74.ry = best.ry;
    gPendingTransitionQ74.rz = best.rz;
    gPendingTransitionQ74.valid = true;
    gHasPendingTransitionQ74 = true;

    Q71_LOGI("Q10.0 MEGATON ENTRY: sourceDoor=%08X sourceCell=%08X sourceWorld=%08X destinationDoor=%08X destinationCell=%08X destinationWorld=%08X XTEL=(%.2f %.2f %.2f) R=(%.4f %.4f %.4f) source=Fallout3.esm",
             best.sourceRef, best.sourceCell, best.sourceWorld,
             best.destinationRef, MEGATON_ENTRANCE_CELL, MEGATON_WORLDSPACE,
             best.x, best.y, best.z, best.rx, best.ry, best.rz);
    return true;
}

bool QueueFo3MegatonEntryQ1040() {
    constexpr uint32_t WASTELAND_WORLDSPACE = 0x0000003Cu;
    constexpr uint32_t MEGATON_WORLDSPACE = 0x00000A74u;
    constexpr uint32_t PREFERRED_ENTRANCE_CELL = 0x00002DBDu;

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) {
        Q71_LOGE("Q10.4 GATE LOCATOR FAILED: reason=open-esm");
        return false;
    }
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        Q71_LOGE("Q10.4 GATE LOCATOR FAILED: reason=bad-esm-size");
        return false;
    }

    struct GateCandidateQ1040 {
        uint32_t sourceRef = 0u;
        uint32_t sourceCell = 0u;
        uint32_t sourceWorld = 0u;
        uint32_t destinationRef = 0u;
        uint32_t destinationCell = 0u;
        uint32_t destinationWorld = 0u;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        float rx = 0.0f, ry = 0.0f, rz = 0.0f;
        uint32_t flags = 0u;
        int score = -1;
    } best;

    std::vector<GroupFrame> groups;
    size_t exteriorRefs = 0u;
    size_t wastelandXtel = 0u;
    size_t resolvedDestinations = 0u;
    size_t megatonTransitions = 0u;

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

        const uint32_t recordFlags = ReadLe32(header + 8u);
        const uint32_t sourceRef = ReadLe32(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        uint32_t sourceWorld = 0u;
        uint32_t sourceCell = 0u;
        for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
            if (sourceCell == 0u &&
                (it->type == 6u || it->type == 8u || it->type == 9u || it->type == 10u)) {
                sourceCell = it->label;
            }
            if (sourceWorld == 0u && it->type == 1u) sourceWorld = it->label;
        }
        if (sourceWorld != WASTELAND_WORLDSPACE) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }
        ++exteriorRefs;

        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, recordFlags, payload)) break;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "XTEL", 4u) != 0 || size < 28u) return;
            ++wastelandXtel;

            const uint32_t destinationRef = ReadLe32(bytes + 0u);
            Q74Owner destinationOwner;
            if (!Q74ResolveOwner(destinationRef, destinationOwner) || !destinationOwner.valid) return;
            ++resolvedDestinations;

            const bool exactEntranceCell = destinationOwner.cellFormId == PREFERRED_ENTRANCE_CELL;
            const bool megatonWorld = destinationOwner.worldspaceFormId == MEGATON_WORLDSPACE;
            if (!megatonWorld && !exactEntranceCell) return;
            ++megatonTransitions;

            int score = megatonWorld ? 100 : 50;
            if (exactEntranceCell) score += 100;
            if (score <= best.score) return;

            best.sourceRef = sourceRef;
            best.sourceCell = sourceCell;
            best.sourceWorld = sourceWorld;
            best.destinationRef = destinationRef;
            best.destinationCell = destinationOwner.cellFormId != 0u
                ? destinationOwner.cellFormId : PREFERRED_ENTRANCE_CELL;
            best.destinationWorld = MEGATON_WORLDSPACE;
            best.x = ReadLeFloat(bytes + 4u);
            best.y = ReadLeFloat(bytes + 8u);
            best.z = ReadLeFloat(bytes + 12u);
            best.rx = ReadLeFloat(bytes + 16u);
            best.ry = ReadLeFloat(bytes + 20u);
            best.rz = ReadLeFloat(bytes + 24u);
            best.flags = size >= 32u ? ReadLe32(bytes + 28u) : 0u;
            best.score = score;
        });
    }
    std::fclose(file);

    Q71_LOGI("Q10.4 GATE SCAN: wastelandRefs=%zu wastelandXTEL=%zu resolvedDest=%zu toMegaton=%zu",
             exteriorRefs, wastelandXtel, resolvedDestinations, megatonTransitions);

    if (best.score < 0 || best.destinationRef == 0u || best.destinationCell == 0u) {
        Q71_LOGE("Q10.4 GATE LOCATOR FAILED: reason=no-wasteland-to-megaton-transition");
        return false;
    }

    gPendingTransitionQ74 = {};
    gPendingTransitionQ74.destinationDoorRef = best.destinationRef;
    gPendingTransitionQ74.cellFormId = best.destinationCell;
    gPendingTransitionQ74.worldspaceFormId = best.destinationWorld;
    gPendingTransitionQ74.x = best.x;
    gPendingTransitionQ74.y = best.y;
    gPendingTransitionQ74.z = best.z;
    gPendingTransitionQ74.rx = best.rx;
    gPendingTransitionQ74.ry = best.ry;
    gPendingTransitionQ74.rz = best.rz;
    gPendingTransitionQ74.valid = true;
    gHasPendingTransitionQ74 = true;

    Q71_LOGI("Q10.4 GATE READY: sourceDoor=%08X sourceCell=%08X sourceWorld=%08X destinationDoor=%08X destinationCell=%08X destinationWorld=%08X XTEL=(%.2f %.2f %.2f) R=(%.4f %.4f %.4f) score=%d source=Fallout3.esm",
             best.sourceRef, best.sourceCell, best.sourceWorld,
             best.destinationRef, best.destinationCell, best.destinationWorld,
             best.x, best.y, best.z, best.rx, best.ry, best.rz,
             best.score);
    return true;
}

bool ResolveFo3DoorTeleportQ1700(uint32_t sourceDoorRef,
                                 Fo3DoorTeleport* outTeleport) {
    if (!outTeleport) return false;
    *outTeleport = {};
    if (sourceDoorRef == 0u) return false;

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    bool foundRef = false;
    bool foundXtel = false;
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

        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        const uint32_t formId = ReadLe32(header + 12u);
        if (formId != sourceDoorRef || std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        foundRef = true;
        const uint32_t recordFlags = ReadLe32(header + 8u);
        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, recordFlags, payload)) break;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (foundXtel || std::memcmp(type, "XTEL", 4u) != 0 || size < 28u) return;
            outTeleport->destinationDoorRefFormId = ReadLe32(bytes + 0u);
            outTeleport->x = ReadLeFloat(bytes + 4u);
            outTeleport->y = ReadLeFloat(bytes + 8u);
            outTeleport->z = ReadLeFloat(bytes + 12u);
            outTeleport->rx = ReadLeFloat(bytes + 16u);
            outTeleport->ry = ReadLeFloat(bytes + 20u);
            outTeleport->rz = ReadLeFloat(bytes + 24u);
            if (size >= 32u) outTeleport->flags = ReadLe32(bytes + 28u);
            foundXtel = outTeleport->destinationDoorRefFormId != 0u;
        });
        break;
    }
    std::fclose(file);

    if (!foundXtel) {
        if (foundRef) {
            Q71_LOGI("Q16.0 DOOR XTEL ABSENT: sourceDoor=%08X", sourceDoorRef);
        }
        *outTeleport = {};
        return false;
    }

    Q74Owner owner;
    if (Q74ResolveOwner(outTeleport->destinationDoorRefFormId, owner) && owner.valid) {
        outTeleport->destinationCellFormId = owner.cellFormId;
    }
    outTeleport->valid = true;
    Q71_LOGI("Q16.0 DOOR XTEL READY: sourceDoor=%08X destinationDoor=%08X destinationCell=%08X XTEL=(%.2f %.2f %.2f)",
             sourceDoorRef, outTeleport->destinationDoorRefFormId,
             outTeleport->destinationCellFormId,
             outTeleport->x, outTeleport->y, outTeleport->z);
    return true;
}

bool QueueFo3DoorTransitionQ1700(uint32_t sourceDoorRef,
                                 uint32_t destinationDoorRef,
                                 float x, float y, float z,
                                 float rx, float ry, float rz) {
    if (destinationDoorRef == 0u) return false;
    if (gHasPendingTransitionQ74) {
        Q71_LOGI("Q16.0 DOOR QUEUE BUSY: sourceDoor=%08X destinationDoor=%08X",
                 sourceDoorRef, destinationDoorRef);
        return false;
    }

    Q74Owner owner;
    if (!Q74ResolveOwner(destinationDoorRef, owner) || !owner.valid) {
        Q71_LOGE("Q16.0 DOOR OWNER FAIL: sourceDoor=%08X destinationDoor=%08X",
                 sourceDoorRef, destinationDoorRef);
        return false;
    }

    gPendingTransitionQ74 = {};
    gPendingTransitionQ74.destinationDoorRef = destinationDoorRef;
    gPendingTransitionQ74.cellFormId = owner.cellFormId;
    gPendingTransitionQ74.worldspaceFormId = owner.worldspaceFormId;
    gPendingTransitionQ74.x = x;
    gPendingTransitionQ74.y = y;
    gPendingTransitionQ74.z = z;
    gPendingTransitionQ74.rx = rx;
    gPendingTransitionQ74.ry = ry;
    gPendingTransitionQ74.rz = rz;
    gPendingTransitionQ74.valid = true;
    gHasPendingTransitionQ74 = true;

    BeginFo3LoadingQ1700(owner.cellFormId, owner.worldspaceFormId);
    Q71_LOGI("Q16.0 DOOR QUEUED: sourceDoor=%08X destinationDoor=%08X cell=%08X worldspace=%08X kind=%s XTEL=(%.2f %.2f %.2f) R=(%.4f %.4f %.4f) loading=PRESENT",
             sourceDoorRef, destinationDoorRef, owner.cellFormId,
             owner.worldspaceFormId,
             owner.worldspaceFormId == 0u ? "INTERIOR" : "EXTERIOR",
             x, y, z, rx, ry, rz);
    return true;
}

namespace Q1840Prompt {

struct GroupFrame {
    uint64_t end = 0u;
    uint32_t label = 0u;
    uint32_t type = 0u;
};

struct Metadata {
    uint32_t sourceBase = 0u;
    Fo3DoorTeleport teleport;
};

bool gIndexAttempted = false;
bool gIndexReady = false;
std::unordered_map<uint32_t, std::string> gFullNames;
std::unordered_map<uint32_t, uint32_t> gCellWorldspaces;
std::unordered_map<uint32_t, uint32_t> gRefBases;
std::unordered_map<uint32_t, Metadata> gDoorMetadata;
std::unordered_map<uint32_t, std::string> gPrompts;

std::string SubrecordString(const uint8_t* bytes, uint32_t size) {
    if (!bytes || size == 0u) return {};
    size_t n = 0u;
    while (n < size && bytes[n] != 0u) ++n;
    return std::string(reinterpret_cast<const char*>(bytes), n);
}

uint32_t CurrentWorldspace(const std::vector<GroupFrame>& groups) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (it->type == 1u) return it->label;
    }
    return 0u;
}

bool EnsureIndex() {
    if (gIndexAttempted) return gIndexReady;
    gIndexAttempted = true;

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) {
        Q71_LOGE("Q16.12 PROMPT INDEX FAILED: reason=open-esm");
        return false;
    }
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        Q71_LOGE("Q16.12 PROMPT INDEX FAILED: reason=bad-esm-size");
        return false;
    }

    std::vector<GroupFrame> groups;
    size_t doorNames = 0u;
    size_t cellNames = 0u;
    size_t worldNames = 0u;
    size_t refBases = 0u;

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
            if (sizeField < HEADER_SIZE ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrame{offset + sizeField,
                                        ReadLe32(header + 8u),
                                        ReadLe32(header + 12u)});
            continue;
        }

        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        const bool isDoor = std::memcmp(header, "DOOR", 4u) == 0;
        const bool isCell = std::memcmp(header, "CELL", 4u) == 0;
        const bool isWorld = std::memcmp(header, "WRLD", 4u) == 0;
        const bool isRef = std::memcmp(header, "REFR", 4u) == 0;
        if (!isDoor && !isCell && !isWorld && !isRef) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        const uint32_t formId = ReadLe32(header + 12u);
        const uint32_t recordFlags = ReadLe32(header + 8u);
        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, recordFlags, payload)) break;

        std::string full;
        uint32_t refBase = 0u;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (!isRef && full.empty() &&
                std::memcmp(type, "FULL", 4u) == 0) {
                full = SubrecordString(bytes, size);
            } else if (isRef && refBase == 0u &&
                       std::memcmp(type, "NAME", 4u) == 0 && size >= 4u) {
                refBase = ReadLe32(bytes);
            }
        });

        if (!full.empty()) {
            gFullNames[formId] = full;
            if (isDoor) ++doorNames;
            else if (isCell) ++cellNames;
            else if (isWorld) ++worldNames;
        }
        if (isRef && refBase != 0u) {
            gRefBases[formId] = refBase;
            ++refBases;
        }
        if (isCell) {
            const uint32_t world = CurrentWorldspace(groups);
            if (world != 0u) gCellWorldspaces[formId] = world;
        }
    }

    std::fclose(file);
    gIndexReady = !gFullNames.empty() && !gRefBases.empty();
    Q71_LOGI("Q16.12 PROMPT INDEX READY: ready=%d fullNames=%zu door=%zu cell=%zu world=%zu refs=%zu cellWorldLinks=%zu source=Fallout3.esm",
             gIndexReady ? 1 : 0, gFullNames.size(), doorNames, cellNames,
             worldNames, refBases, gCellWorldspaces.size());
    return gIndexReady;
}

const std::string& FullName(uint32_t formId) {
    static const std::string empty;
    const auto it = gFullNames.find(formId);
    return it == gFullNames.end() ? empty : it->second;
}

std::string DestinationName(const Fo3DoorTeleport& teleport) {
    if (teleport.destinationCellFormId == 0u) return {};
    const std::string& cell = FullName(teleport.destinationCellFormId);
    if (!cell.empty()) return cell;
    const auto worldIt = gCellWorldspaces.find(teleport.destinationCellFormId);
    if (worldIt == gCellWorldspaces.end()) return {};
    return FullName(worldIt->second);
}

bool EnsureDoorMetadata(uint32_t sourceDoorRef) {
    if (gDoorMetadata.find(sourceDoorRef) != gDoorMetadata.end()) return true;
    if (!EnsureIndex()) return false;

    const auto baseIt = gRefBases.find(sourceDoorRef);
    if (baseIt == gRefBases.end() || baseIt->second == 0u) return false;

    Fo3DoorTeleport teleport;
    if (!ResolveFo3DoorTeleportQ1700(sourceDoorRef, &teleport) || !teleport.valid) {
        return false;
    }

    Metadata meta;
    meta.sourceBase = baseIt->second;
    meta.teleport = teleport;
    gDoorMetadata[sourceDoorRef] = meta;
    Q71_LOGI("Q16.12 PROMPT METADATA LAZY: sourceDoor=%08X sourceBase=%08X destinationDoor=%08X destinationCell=%08X source=Fallout3.esm",
             sourceDoorRef, meta.sourceBase,
             teleport.destinationDoorRefFormId,
             teleport.destinationCellFormId);
    return true;
}

bool BuildPrompt(uint32_t sourceDoorRef, std::string& out) {
    out.clear();
    if (!EnsureDoorMetadata(sourceDoorRef)) return false;
    if (!EnsureIndex()) return false;

    const Metadata& meta = gDoorMetadata[sourceDoorRef];
    const std::string doorName = FullName(meta.sourceBase);
    const std::string destination = DestinationName(meta.teleport);
    if (doorName.empty() && destination.empty()) {
        Q71_LOGE("Q16.12 AUTHORED DOOR PROMPT MISS: sourceDoor=%08X sourceBase=%08X destinationCell=%08X reason=no-FULL-fields",
                 sourceDoorRef, meta.sourceBase, meta.teleport.destinationCellFormId);
        return false;
    }

    out = "Open";
    if (!doorName.empty()) {
        out += " ";
        out += doorName;
    }
    if (!destination.empty()) {
        out += " to ";
        out += destination;
    }

    Q71_LOGI("Q16.12 AUTHORED DOOR PROMPT: sourceDoor=%08X sourceBase=%08X destinationDoor=%08X destinationCell=%08X door=\"%s\" destination=\"%s\" text=\"%s\"",
             sourceDoorRef, meta.sourceBase,
             meta.teleport.destinationDoorRefFormId,
             meta.teleport.destinationCellFormId,
             doorName.c_str(), destination.c_str(), out.c_str());
    return true;
}

} // namespace Q1840Prompt

void CacheFo3DoorPromptQ1840(uint32_t sourceDoorRef,
                             uint32_t sourceDoorBase,
                             const Fo3DoorTeleport& teleport) {
    if (sourceDoorRef == 0u || sourceDoorBase == 0u || !teleport.valid) return;
    Q1840Prompt::Metadata meta;
    meta.sourceBase = sourceDoorBase;
    meta.teleport = teleport;
    Q1840Prompt::gDoorMetadata[sourceDoorRef] = meta;
    Q1840Prompt::gPrompts.erase(sourceDoorRef);
    // Build the one-time authored-name index during scene preparation rather
    // than on the first visible HUD frame when possible.
    Q1840Prompt::EnsureIndex();
}

bool GetFo3DoorPromptQ1840(uint32_t sourceDoorRef,
                           char* outText,
                           size_t outCapacity) {
    if (!outText || outCapacity == 0u || sourceDoorRef == 0u) return false;
    outText[0] = '\0';

    auto cached = Q1840Prompt::gPrompts.find(sourceDoorRef);
    if (cached == Q1840Prompt::gPrompts.end()) {
        std::string prompt;
        if (!Q1840Prompt::BuildPrompt(sourceDoorRef, prompt) || prompt.empty()) {
            return false;
        }
        cached = Q1840Prompt::gPrompts.emplace(sourceDoorRef, std::move(prompt)).first;
    }

    const std::string& prompt = cached->second;
    const size_t count = std::min(prompt.size(), outCapacity - 1u);
    std::memcpy(outText, prompt.data(), count);
    outText[count] = '\0';
    return count > 0u;
}

bool QueueFo3MegatonEntryQ1860() {
    constexpr uint32_t WASTELAND_WORLDSPACE = 0x0000003Cu;
    constexpr uint32_t MEGATON_WORLDSPACE = 0x00000A74u;
    constexpr uint32_t PREFERRED_ENTRANCE_CELL = 0x00002DBDu;

    struct OwnerQ1860 {
        uint32_t cell = 0u;
        uint32_t world = 0u;
    };
    struct CandidateQ1860 {
        uint32_t sourceRef = 0u;
        uint32_t sourceCell = 0u;
        uint32_t destinationRef = 0u;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        float rx = 0.0f, ry = 0.0f, rz = 0.0f;
        uint32_t flags = 0u;
    };

    const auto started = std::chrono::steady_clock::now();
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) {
        Q71_LOGE("Q16.14 FAST GATE SCAN FAILED: reason=open-esm");
        return false;
    }
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        Q71_LOGE("Q16.14 FAST GATE SCAN FAILED: reason=bad-esm-size");
        return false;
    }

    std::vector<GroupFrame> groups;
    std::unordered_map<uint32_t, OwnerQ1860> megatonOwners;
    std::vector<CandidateQ1860> candidates;
    size_t records = 0u;
    size_t wastelandRefs = 0u;
    size_t wastelandXtel = 0u;

    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        ++records;
        if ((records & 0x1fffu) == 0u) PumpFo3AndroidEventsQ1860();

        const uint32_t sizeField = ReadLe32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrame{offset + sizeField,
                                        ReadLe32(header + 8u),
                                        ReadLe32(header + 12u)});
            continue;
        }

        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        uint32_t sourceWorld = 0u;
        uint32_t sourceCell = 0u;
        for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
            if (sourceCell == 0u &&
                (it->type == 6u || it->type == 8u || it->type == 9u || it->type == 10u)) {
                sourceCell = it->label;
            }
            if (sourceWorld == 0u && it->type == 1u) sourceWorld = it->label;
        }

        const uint32_t sourceRef = ReadLe32(header + 12u);
        if (sourceWorld == MEGATON_WORLDSPACE || sourceCell == PREFERRED_ENTRANCE_CELL) {
            megatonOwners[sourceRef] = OwnerQ1860{sourceCell, sourceWorld};
        }

        if (sourceWorld != WASTELAND_WORLDSPACE) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }
        ++wastelandRefs;

        const uint32_t recordFlags = ReadLe32(header + 8u);
        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, recordFlags, payload)) break;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "XTEL", 4u) != 0 || size < 28u) return;
            ++wastelandXtel;
            CandidateQ1860 c;
            c.sourceRef = sourceRef;
            c.sourceCell = sourceCell;
            c.destinationRef = ReadLe32(bytes + 0u);
            c.x = ReadLeFloat(bytes + 4u);
            c.y = ReadLeFloat(bytes + 8u);
            c.z = ReadLeFloat(bytes + 12u);
            c.rx = ReadLeFloat(bytes + 16u);
            c.ry = ReadLeFloat(bytes + 20u);
            c.rz = ReadLeFloat(bytes + 24u);
            c.flags = size >= 32u ? ReadLe32(bytes + 28u) : 0u;
            candidates.push_back(c);
        });
    }
    std::fclose(file);
    PumpFo3AndroidEventsQ1860();

    CandidateQ1860 best{};
    OwnerQ1860 bestOwner{};
    int bestScore = -1;
    size_t resolvedDestinations = 0u;
    size_t megatonTransitions = 0u;
    for (const CandidateQ1860& c : candidates) {
        const auto ownerIt = megatonOwners.find(c.destinationRef);
        if (ownerIt == megatonOwners.end()) continue;
        ++resolvedDestinations;
        const OwnerQ1860& owner = ownerIt->second;
        const bool exactEntranceCell = owner.cell == PREFERRED_ENTRANCE_CELL;
        const bool megatonWorld = owner.world == MEGATON_WORLDSPACE;
        if (!megatonWorld && !exactEntranceCell) continue;
        ++megatonTransitions;

        int score = megatonWorld ? 100 : 50;
        if (exactEntranceCell) score += 100;
        if (score <= bestScore) continue;
        best = c;
        bestOwner = owner;
        bestScore = score;
    }

    const float elapsedMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    Q71_LOGI("Q16.14 FAST GATE SCAN: records=%zu wastelandRefs=%zu wastelandXTEL=%zu ownerIndex=%zu resolvedDest=%zu toMegaton=%zu elapsedMs=%.1f passes=1",
             records, wastelandRefs, wastelandXtel, megatonOwners.size(),
             resolvedDestinations, megatonTransitions, elapsedMs);

    if (bestScore < 0 || best.destinationRef == 0u || bestOwner.cell == 0u) {
        Q71_LOGE("Q16.14 FAST GATE SCAN FAILED: reason=no-wasteland-to-megaton-transition");
        return false;
    }

    gPendingTransitionQ74 = {};
    gPendingTransitionQ74.destinationDoorRef = best.destinationRef;
    gPendingTransitionQ74.cellFormId = bestOwner.cell;
    gPendingTransitionQ74.worldspaceFormId = MEGATON_WORLDSPACE;
    gPendingTransitionQ74.x = best.x;
    gPendingTransitionQ74.y = best.y;
    gPendingTransitionQ74.z = best.z;
    gPendingTransitionQ74.rx = best.rx;
    gPendingTransitionQ74.ry = best.ry;
    gPendingTransitionQ74.rz = best.rz;
    gPendingTransitionQ74.valid = true;
    gHasPendingTransitionQ74 = true;

    Q71_LOGI("Q16.14 FAST GATE READY: sourceDoor=%08X sourceCell=%08X sourceWorld=%08X destinationDoor=%08X destinationCell=%08X destinationWorld=%08X XTEL=(%.2f %.2f %.2f) R=(%.4f %.4f %.4f) score=%d source=Fallout3.esm",
             best.sourceRef, best.sourceCell, WASTELAND_WORLDSPACE,
             best.destinationRef, bestOwner.cell, MEGATON_WORLDSPACE,
             best.x, best.y, best.z, best.rx, best.ry, best.rz, bestScore);
    return true;
}

bool ConsumeFo3CellTransitionRequestQ74(Fo3CellTransitionRequestQ74& outRequest) {
    if (!gHasPendingTransitionQ74 || !gPendingTransitionQ74.valid) return false;
    // Initial direct boot has no loading state and remains immediate. User door
    // transitions wait until one LSCR frame has actually reached xrEndFrame.
    if (ShouldDelayFo3TransitionConsumeQ1700()) return false;
    outRequest = gPendingTransitionQ74;
    gHasPendingTransitionQ74 = false;
    return true;
}

void CompleteFo3CellTransitionQ74(uint32_t cellFormId) {
    gCurrentCellQ74 = cellFormId;
    gPlayerResetPendingQ74 = true;
    NotifyFo3TransitionCompleteQ1700();

    if (gPendingTransitionQ74.valid &&
        gPendingTransitionQ74.cellFormId == cellFormId &&
        gPendingTransitionQ74.worldspaceFormId == 0u) {
        ShutdownFo3TerrainRenderQ76();
        ActivateFo3TerrainGroundingQ77(0u, 0.0f, 0.0f, 0.0f,
                                       Q71_SCENE_FORWARD, Q71_FLOOR_Y,
                                       Q71_UNITS_PER_METRE);
    }

    bool terrainReady = false;

    if (gPendingTransitionQ74.valid && gPendingTransitionQ74.worldspaceFormId != 0u) {
        LoadFo3EnvironmentQ1390(gPendingTransitionQ74.worldspaceFormId);
        LoadFo3PlacedLightsQ1390(gPendingTransitionQ74.worldspaceFormId,
                                 gPendingTransitionQ74.x,
                                 gPendingTransitionQ74.y,
                                 gPendingTransitionQ74.z);
    } else {
        ResetFo3EnvironmentQ1390();
        ResetFo3PlacedLightsQ1010();
    }

    if (gPendingTransitionQ74.valid &&
        gPendingTransitionQ74.cellFormId == cellFormId &&
        gPendingTransitionQ74.worldspaceFormId != 0u) {
        terrainReady = InitializeFo3TerrainRenderQ76(
            gPendingTransitionQ74.worldspaceFormId,
            gPendingTransitionQ74.x,
            gPendingTransitionQ74.y,
            gPendingTransitionQ74.z,
            Q71_SCENE_FORWARD,
            Q71_FLOOR_Y,
            Q71_UNITS_PER_METRE);
        if (terrainReady) {
            ActivateFo3TerrainGroundingQ77(
                gPendingTransitionQ74.worldspaceFormId,
                gPendingTransitionQ74.x,
                gPendingTransitionQ74.y,
                gPendingTransitionQ74.z,
                Q71_SCENE_FORWARD,
                Q71_FLOOR_Y,
                Q71_UNITS_PER_METRE);
        }
    }

    Q71_LOGI("Q7.7 TERRAIN POST-SWAP: persistentCell=%08X worldspace=%08X ready=%d physicalGround=%d housePathUntouched=1",
             cellFormId,
             gPendingTransitionQ74.worldspaceFormId,
             terrainReady ? 1 : 0,
             IsFo3TerrainGroundingActiveQ77() ? 1 : 0);
    Q71_LOGI("Q7.5 TRANSITION APPLIED: persistentCell=%08X playerReset=NEXT_PHYSICS_FRAME orientation=preserved worldspaceNeighborhood=1",
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
        Q71_LOGE("Q7.5 TRANSITION REQUEST FAILED: proven Q7.1 hit could not recover door metadata");
        return true;
    }

    Q74Owner owner;
    if (!Q74ResolveOwner(door.destinationDoorRef, owner) || !owner.valid) {
        Q71_LOGE("Q7.5 TRANSITION REQUEST FAILED: destinationDoor=%08X owner unresolved",
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

    Q71_LOGI("Q7.5 TRANSITION REQUESTED: destinationDoor=%08X persistentCell=%08X worldspace=%08X XTEL=(%.2f %.2f %.2f) R=(%.4f %.4f %.4f) queue=render-thread exteriorNeighborhood=1",
             door.destinationDoorRef, owner.cellFormId, owner.worldspaceFormId,
             door.teleportX, door.teleportY, door.teleportZ,
             door.teleportRx, door.teleportRy, door.teleportRz);
    return true;
}
