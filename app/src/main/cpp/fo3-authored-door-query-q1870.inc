// Q16.15 authored XTEL interaction cache.
//
// Q16.3's first controller-ray miss called AnchorsForCell(), which collected the
// CELL and then invoked ResolveFo3DoorTeleportQ1700 once for every active REFR.
// That resolver linearly scans Fallout3.esm, so Megaton performed ~166 full ESM
// scans on the render thread when the player merely moved the controller off the
// gate. Build the same authored anchor set with two bounded passes instead:
//   1) the existing generic CELL collector (preserves enable-parent semantics),
//   2) one ESM pass that resolves every active source XTEL and every destination
//      REFR owner in memory.
// The result is primed at scene activation and every live query is memory-only.

#include "fo3-authored-door-query-q1730.h"
#include "fo3-authored-door-query-q1870.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern void PumpFo3AndroidEventsQ1860();

namespace Q1730Door {

struct Anchor {
    uint32_t sourceDoorRef = 0u;
    float sourceX = 0.0f;
    float sourceY = 0.0f;
    float sourceZ = 0.0f;
    Fo3DoorTeleport teleport;
};

struct CellCache {
    bool attempted = false;
    std::vector<Anchor> anchors;
};

struct Q1870Frame {
    uint64_t end = 0u;
    uint32_t label = 0u;
    uint32_t type = 0u;
};

std::unordered_map<uint32_t, CellCache> gCellCaches;

const std::vector<Anchor>& AnchorsForCell(uint32_t cellFormId) {
    CellCache& cache = gCellCaches[cellFormId];
    if (cache.attempted) return cache.anchors;
    cache.attempted = true;

    const auto started = std::chrono::steady_clock::now();

    // Preserve Q16.3's exact CELL-child and enable-parent semantics. This is one
    // bounded ESM pass, not one pass per reference.
    std::vector<Q1700Interior::RawPlacement> raw;
    if (!Q1700Interior::CollectCellReferences(cellFormId, raw)) {
        Q71_LOGI("Q16.15 XTEL CACHE READY: cell=%08X raw=0 active=0 xtel=0 ownerIndex=0 elapsedMs=0 source=Fallout3.esm passes=1 reason=no-cell-refs",
                 cellFormId);
        return cache.anchors;
    }

    std::unordered_map<uint32_t, const Q1700Interior::RawPlacement*> refs;
    refs.reserve(raw.size());
    for (const auto& p : raw) refs[p.refFormId] = &p;

    std::unordered_map<uint32_t, bool> enabledMemo;
    std::unordered_set<uint32_t> activeRefs;
    activeRefs.reserve(raw.size());
    for (const auto& p : raw) {
        std::unordered_set<uint32_t> visiting;
        if (Q1700Interior::ResolveEnabled(p.refFormId, refs, enabledMemo, visiting)) {
            activeRefs.insert(p.refFormId);
        }
    }

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) {
        Q71_LOGE("Q16.15 XTEL CACHE FAILED: cell=%08X reason=open-esm", cellFormId);
        return cache.anchors;
    }
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        Q71_LOGE("Q16.15 XTEL CACHE FAILED: cell=%08X reason=bad-esm-size", cellFormId);
        return cache.anchors;
    }

    std::vector<Q1870Frame> groups;
    std::unordered_map<uint32_t, uint32_t> ownerCells;
    ownerCells.reserve(32768u);
    std::unordered_map<uint32_t, Fo3DoorTeleport> teleports;
    teleports.reserve(activeRefs.size());

    size_t records = 0u;
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
            groups.push_back(Q1870Frame{offset + sizeField,
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

        const uint32_t refFormId = ReadLe32(header + 12u);
        uint32_t ownerCell = 0u;
        for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
            if (it->type == 6u || it->type == 8u ||
                it->type == 9u || it->type == 10u) {
                ownerCell = it->label;
                break;
            }
        }
        if (ownerCell != 0u) ownerCells[refFormId] = ownerCell;

        if (activeRefs.find(refFormId) == activeRefs.end()) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        const uint32_t recordFlags = ReadLe32(header + 8u);
        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, recordFlags, payload)) break;

        Fo3DoorTeleport teleport;
        bool foundXtel = false;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (foundXtel || std::memcmp(type, "XTEL", 4u) != 0 || size < 28u) return;
            teleport.destinationDoorRefFormId = ReadLe32(bytes + 0u);
            teleport.x = ReadLeFloat(bytes + 4u);
            teleport.y = ReadLeFloat(bytes + 8u);
            teleport.z = ReadLeFloat(bytes + 12u);
            teleport.rx = ReadLeFloat(bytes + 16u);
            teleport.ry = ReadLeFloat(bytes + 20u);
            teleport.rz = ReadLeFloat(bytes + 24u);
            teleport.flags = size >= 32u ? ReadLe32(bytes + 28u) : 0u;
            foundXtel = teleport.destinationDoorRefFormId != 0u;
        });
        if (foundXtel) teleports.emplace(refFormId, teleport);
    }
    std::fclose(file);
    PumpFo3AndroidEventsQ1860();

    for (const auto& entry : teleports) {
        const auto placementIt = refs.find(entry.first);
        if (placementIt == refs.end() || !placementIt->second) continue;

        Fo3DoorTeleport teleport = entry.second;
        const auto ownerIt = ownerCells.find(teleport.destinationDoorRefFormId);
        if (ownerIt != ownerCells.end()) teleport.destinationCellFormId = ownerIt->second;
        teleport.valid = teleport.destinationDoorRefFormId != 0u;
        if (!teleport.valid) continue;

        const Q1700Interior::RawPlacement& p = *placementIt->second;
        Anchor anchor;
        anchor.sourceDoorRef = p.refFormId;
        anchor.sourceX = p.x;
        anchor.sourceY = p.y;
        anchor.sourceZ = p.z;
        anchor.teleport = teleport;
        cache.anchors.push_back(anchor);
    }

    const float elapsedMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    Q71_LOGI("Q16.15 XTEL CACHE READY: cell=%08X raw=%zu active=%zu xtel=%zu ownerIndex=%zu records=%zu elapsedMs=%.1f source=Fallout3.esm passes=2 liveQuery=memory-only",
             cellFormId, raw.size(), activeRefs.size(), cache.anchors.size(),
             ownerCells.size(), records, elapsedMs);
    return cache.anchors;
}

} // namespace Q1730Door

void PrimeFo3AuthoredDoorAnchorsQ1870(uint32_t cellFormId) {
    const std::vector<Q1730Door::Anchor>& anchors = Q1730Door::AnchorsForCell(cellFormId);
    Q71_LOGI("Q16.15 XTEL CACHE PRIME: cell=%08X anchors=%zu liveRayMiss=memory-only",
             cellFormId, anchors.size());
}

bool QueryFo3AuthoredDoorAnchorQ1730(uint32_t cellFormId,
                                     float sceneCenterX,
                                     float sceneCenterY,
                                     float sceneFloorZ,
                                     float unitsPerMetre,
                                     float floorY,
                                     float sceneForward,
                                     float originX,
                                     float originY,
                                     float originZ,
                                     float dirX,
                                     float dirY,
                                     float dirZ,
                                     Fo3DoorAimQ1700* outAim) {
    if (outAim) *outAim = {};
    if (cellFormId == 0u || unitsPerMetre <= 1e-4f) return false;

    const float length = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (length < 1e-5f) return false;
    dirX /= length;
    dirY /= length;
    dirZ /= length;

    const std::vector<Q1730Door::Anchor>& anchors = Q1730Door::AnchorsForCell(cellFormId);
    if (anchors.empty()) return false;

    // Preserve Q16.3's authored focus geometry exactly; only cache construction
    // changed. Rendered DOOR AABBs still win before this fallback is queried.
    constexpr float kMaxDistance = 4.0f;
    constexpr float kAnchorRadius = 1.35f;
    constexpr float kRadiusSq = kAnchorRadius * kAnchorRadius;

    const Q1730Door::Anchor* best = nullptr;
    float bestScore = 1e30f;
    float bestT = 0.0f;
    for (const Q1730Door::Anchor& anchor : anchors) {
        const float px = (anchor.sourceX - sceneCenterX) / unitsPerMetre;
        const float py = floorY + (anchor.sourceZ - sceneFloorZ) / unitsPerMetre;
        const float pz = sceneForward - (anchor.sourceY - sceneCenterY) / unitsPerMetre;

        const float vx = px - originX;
        const float vy = py - originY;
        const float vz = pz - originZ;
        const float t = vx * dirX + vy * dirY + vz * dirZ;
        if (t < 0.05f || t > kMaxDistance) continue;

        const float distanceSq = vx * vx + vy * vy + vz * vz;
        const float perpendicularSq = std::max(0.0f, distanceSq - t * t);
        if (perpendicularSq > kRadiusSq) continue;

        const float score = perpendicularSq * 3.0f + t * 0.05f;
        if (score >= bestScore) continue;
        bestScore = score;
        bestT = t;
        best = &anchor;
    }

    if (!best) return false;
    if (outAim) {
        outAim->valid = true;
        outAim->sourceDoorRef = best->sourceDoorRef;
        outAim->destinationDoorRef = best->teleport.destinationDoorRefFormId;
        outAim->distance = bestT;
        outAim->x = best->teleport.x;
        outAim->y = best->teleport.y;
        outAim->z = best->teleport.z;
        outAim->rx = best->teleport.rx;
        outAim->ry = best->teleport.ry;
        outAim->rz = best->teleport.rz;
    }
    return true;
}
