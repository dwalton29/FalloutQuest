// Q16.3 authored XTEL interaction fallback.
//
// This file is included into the transition translation unit immediately after
// fo3-cell-interior-q1700.inc, so it can reuse Q1700Interior's generic CELL REFR
// collector and enable-parent resolution without adding a second ESM parser.

#include "fo3-authored-door-query-q1730.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

std::unordered_map<uint32_t, CellCache> gCellCaches;

const std::vector<Anchor>& AnchorsForCell(uint32_t cellFormId) {
    CellCache& cache = gCellCaches[cellFormId];
    if (cache.attempted) return cache.anchors;
    cache.attempted = true;

    std::vector<Q1700Interior::RawPlacement> raw;
    if (!Q1700Interior::CollectCellReferences(cellFormId, raw)) {
        Q71_LOGI("Q16.3 XTEL ANCHORS: cell=%08X raw=0 active=0 xtel=0 reason=no-cell-refs",
                 cellFormId);
        return cache.anchors;
    }

    std::unordered_map<uint32_t, const Q1700Interior::RawPlacement*> refs;
    refs.reserve(raw.size());
    for (const auto& p : raw) refs[p.refFormId] = &p;

    std::unordered_map<uint32_t, bool> enabledMemo;
    size_t activeCount = 0u;
    for (const auto& p : raw) {
        std::unordered_set<uint32_t> visiting;
        if (!Q1700Interior::ResolveEnabled(p.refFormId, refs, enabledMemo, visiting)) continue;
        ++activeCount;

        Fo3DoorTeleport teleport;
        if (!ResolveFo3DoorTeleportQ1700(p.refFormId, &teleport) || !teleport.valid) continue;

        Anchor anchor;
        anchor.sourceDoorRef = p.refFormId;
        anchor.sourceX = p.x;
        anchor.sourceY = p.y;
        anchor.sourceZ = p.z;
        anchor.teleport = teleport;
        cache.anchors.push_back(anchor);
    }

    Q71_LOGI("Q16.3 XTEL ANCHORS: cell=%08X raw=%zu active=%zu xtel=%zu source=Fallout3.esm",
             cellFormId, raw.size(), activeCount, cache.anchors.size());
    return cache.anchors;
}

} // namespace Q1730Door

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

    // Invisible/no-model load doors have no useful mesh AABB. Treat the authored
    // REFR position as a generous activation focus instead. The visible-mesh path
    // still wins first in Q16.0; this is used only when that path has no hit.
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

        // Prefer what the controller is pointed most directly at, then nearer.
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
