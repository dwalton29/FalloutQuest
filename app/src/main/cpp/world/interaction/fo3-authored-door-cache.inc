#include "fo3-esm-reader.h"
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

#include "fo3-authored-door-query.h"
#include "fo3-door-resources.h"
#include "fo3-authored-door-cache.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern void PumpFo3AndroidEventsQ1860();

namespace Q1730Door {

using Anchor = Fo3DoorAnchor;
using CellCache = Fo3DoorCellCache;

struct Q1870Frame {
    uint64_t end = 0u;
    uint32_t label = 0u;
    uint32_t type = 0u;
};

auto& gCellCaches = Fo3LiveDoorResources().cells;

// Pure, cancellable CPU preparation. Never touch the live door-resource store
// and never pump Android events from a worker thread.
bool PrepareWorker(uint32_t cellFormId, Fo3AuthoredDoorCpuCache& out,
                   const std::atomic<bool>& cancel) {
    out=Fo3AuthoredDoorCpuCache{};
    out.cellFormId=cellFormId;
    if(cancel.load(std::memory_order_acquire))return false;
    const auto started = std::chrono::steady_clock::now();

    // Preserve Q16.3's exact CELL-child and enable-parent semantics. This is one
    // bounded ESM pass, not one pass per reference.
    std::vector<Q1700Interior::RawPlacement> raw;
    if (!Q1700Interior::CollectCellReferences(cellFormId, raw, &cancel)) {
        Q71_LOGI("Q16.15 XTEL CACHE READY: cell=%08X raw=0 active=0 xtel=0 ownerIndex=0 elapsedMs=0 source=Fallout3.esm passes=1 reason=no-cell-refs",
                 cellFormId);
        return !cancel.load(std::memory_order_acquire);
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

    FILE* file = std::fopen(fo3assets::FalloutMasterPath().c_str(), "rb");
    if (!file) {
        Q71_LOGE("Q16.15 XTEL CACHE FAILED: cell=%08X reason=open-esm", cellFormId);
        return false;
    }
    const int64_t fileSize = fo3esm::FileSize(file);
    if (fileSize < static_cast<int64_t>(fo3esm::HEADER_SIZE)) {
        std::fclose(file);
        Q71_LOGE("Q16.15 XTEL CACHE FAILED: cell=%08X reason=bad-esm-size", cellFormId);
        return false;
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
        if (offset + fo3esm::HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[fo3esm::HEADER_SIZE]{};
        if (!fo3esm::ReadExact(file, header, sizeof(header))) break;
        ++records;
        if ((records & 0x1fffu) == 0u && cancel.load(std::memory_order_acquire)) {
            std::fclose(file);
            return false;
        }

        const uint32_t sizeField = fo3esm::ReadU32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < fo3esm::HEADER_SIZE ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(Q1870Frame{offset + sizeField,
                                        fo3esm::ReadU32(header + 8u),
                                        fo3esm::ReadU32(header + 12u)});
            continue;
        }

        const uint64_t payloadEnd = offset + fo3esm::HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        const uint32_t refFormId = fo3esm::ReadU32(header + 12u);
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

        const uint32_t recordFlags = fo3esm::ReadU32(header + 8u);
        std::vector<uint8_t> payload;
        if (!fo3esm::ReadPayloadCurrent(file, sizeField, recordFlags, payload)) break;

        Fo3DoorTeleport teleport;
        bool foundXtel = false;
        fo3esm::WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (foundXtel || std::memcmp(type, "XTEL", 4u) != 0 || size < 28u) return;
            teleport.destinationDoorRefFormId = fo3esm::ReadU32(bytes + 0u);
            teleport.x = fo3esm::ReadF32(bytes + 4u);
            teleport.y = fo3esm::ReadF32(bytes + 8u);
            teleport.z = fo3esm::ReadF32(bytes + 12u);
            teleport.rx = fo3esm::ReadF32(bytes + 16u);
            teleport.ry = fo3esm::ReadF32(bytes + 20u);
            teleport.rz = fo3esm::ReadF32(bytes + 24u);
            teleport.flags = size >= 32u ? fo3esm::ReadU32(bytes + 28u) : 0u;
            foundXtel = teleport.destinationDoorRefFormId != 0u;
        });
        if (foundXtel) teleports.emplace(refFormId, teleport);
    }
    std::fclose(file);
    if (cancel.load(std::memory_order_acquire)) return false;

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
        out.anchors.push_back(anchor);
    }

    const float elapsedMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    Q71_LOGI("Q16.15 XTEL CACHE READY: cell=%08X raw=%zu active=%zu xtel=%zu ownerIndex=%zu records=%zu elapsedMs=%.1f source=Fallout3.esm passes=2 liveQuery=memory-only",
             cellFormId, raw.size(), activeRefs.size(), out.anchors.size(),
             ownerCells.size(), records, elapsedMs);
    out.rawReferences=raw.size();
    out.activeReferences=activeRefs.size();
    out.scannedRecords=records;
    out.preparationUs=uint64_t(elapsedMs*1000.0f);
    return true;
}


const std::vector<Anchor>& AnchorsForCell(uint32_t cellFormId) {
    const auto cached = gCellCaches.find(cellFormId);
    if (cached != gCellCaches.end() && cached->second.attempted) return cached->second.anchors;
    MarkFo3DoorResourcesChanged();
    CellCache& cache = gCellCaches[cellFormId];
    cache.attempted = true;
    // Retain original synchronous fallback, normally unused after scene load.
    Fo3AuthoredDoorCpuCache built;
    const std::atomic<bool> cancel{false};
    if (PrepareWorker(cellFormId,built,cancel)) cache.anchors=std::move(built.anchors);
    return cache.anchors;
}

} // namespace Q1730Door

bool HasFo3AuthoredDoorAnchorsCachedQ2190(uint32_t cellFormId) {
    const auto cached=Q1730Door::gCellCaches.find(cellFormId);
    return cached!=Q1730Door::gCellCaches.end()&&cached->second.attempted;
}

bool PrepareFo3AuthoredDoorAnchorsQ2190(uint32_t cellFormId,
    Fo3AuthoredDoorCpuCache& out, const std::atomic<bool>& cancel) {
    return Q1730Door::PrepareWorker(cellFormId,out,cancel);
}

void PublishFo3AuthoredDoorAnchorsQ2190(Fo3AuthoredDoorCpuCache&& prepared) {
    const uint32_t cellFormId=prepared.cellFormId;
    const auto cached=Q1730Door::gCellCaches.find(cellFormId);
    if(cached!=Q1730Door::gCellCaches.end()&&cached->second.attempted)return;
    MarkFo3DoorResourcesChanged();
    Q1730Door::CellCache& cache=Q1730Door::gCellCaches[cellFormId];
    cache.attempted=true;
    cache.anchors=std::move(prepared.anchors);
    Q71_LOGI("Q21.9 XTEL WORKER CACHE PUBLISHED: cell=%08X anchors=%zu raw=%zu active=%zu scanned=%zu workerMs=%.1f",
        cellFormId,cache.anchors.size(),prepared.rawReferences,prepared.activeReferences,
        prepared.scannedRecords,prepared.preparationUs/1000.0);
}


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

    // Reject invalid rays before a first-use cache build, as the legacy path did.
    for (float value : {sceneCenterX,sceneCenterY,sceneFloorZ,unitsPerMetre,floorY,
            sceneForward,originX,originY,originZ,dirX,dirY,dirZ}) {
        if (!std::isfinite(value)) return false;
    }
    const float length = std::sqrt(dirX*dirX+dirY*dirY+dirZ*dirZ);
    if (!std::isfinite(length) || length < 1e-5f) return false;
    return QueryFo3DoorAnchorResources(Q1730Door::AnchorsForCell(cellFormId),
        sceneCenterX,sceneCenterY,sceneFloorZ,unitsPerMetre,floorY,sceneForward,
        originX,originY,originZ,dirX,dirY,dirZ,outAim);
}
