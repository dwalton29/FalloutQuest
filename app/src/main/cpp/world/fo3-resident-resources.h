#pragma once

#include "fo3-collision-placements.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace fo3scene {
// Render-thread capture for workers: value identities only, never GPU handles,
// pointers to live shapes, Player state, or mutable scene/collision callbacks.
struct ResidentVisualRecord { uint32_t reference = 0; bool nativeLod = false; };
struct ResidentResourceSelection {
    std::vector<size_t> retainedSlots;
    std::vector<uint32_t> removedReferences, incomingReferences;
    std::vector<Fo3WorldPlacement> incomingPlacements, collisionPlacements;
};

inline bool SameResidentPlacement(const Fo3WorldPlacement& a, const Fo3WorldPlacement& b) {
    return a.refFormId == b.refFormId && a.baseFormId == b.baseFormId &&
        a.referenceRecordFlags == b.referenceRecordFlags && a.baseRecordFlags == b.baseRecordFlags &&
        a.baseRecordType == b.baseRecordType && a.editorId == b.editorId && a.modelPath == b.modelPath &&
        a.x == b.x && a.y == b.y && a.z == b.z && a.rx == b.rx && a.ry == b.ry && a.rz == b.rz &&
        a.scale == b.scale && a.owningCellFormId == b.owningCellFormId &&
        a.gridX == b.gridX && a.gridY == b.gridY && a.hasExteriorGrid == b.hasExteriorGrid &&
        a.persistentExteriorRef == b.persistentExteriorRef;
}

// Both placement sets must describe the identical residency scope under the
// published and candidate immutable reference projections. This layer admits
// only enable-state changes; transforms/relocation remain separate operations.
inline bool SelectResidentResources(const std::vector<Fo3WorldPlacement>& before,
        const std::vector<Fo3WorldPlacement>& candidate,
        const std::vector<ResidentVisualRecord>& live,
        const std::vector<uint32_t>& affected, ResidentResourceSelection& out, std::string& error) {
    error.clear();
    auto fail = [&](const char* why) { error = why; return false; };
    std::unordered_set<uint32_t> changed;
    for (size_t i = 0; i < affected.size(); ++i) {
        if (!affected[i] || (i && affected[i] <= affected[i-1])) return fail("invalid affected reference list");
        changed.insert(affected[i]);
    }
    using Index = std::unordered_map<uint32_t,const Fo3WorldPlacement*>;
    Index oldIndex, newIndex;
    auto index = [](const std::vector<Fo3WorldPlacement>& placements, Index& ids) {
        for (const auto& p : placements) {
            if (!p.refFormId || !p.baseFormId || p.modelPath.empty() ||
                !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
                !std::isfinite(p.rx) || !std::isfinite(p.ry) || !std::isfinite(p.rz) ||
                !std::isfinite(p.scale) || p.scale <= 0 || !ids.emplace(p.refFormId,&p).second) return false;
        }
        return true;
    };
    if (!index(before,oldIndex) || !index(candidate,newIndex)) return fail("invalid or duplicate resident placement");
    ResidentResourceSelection next;
    std::unordered_set<uint32_t> removed, incoming;
    for (const auto& p : before) {
        const auto after = newIndex.find(p.refFormId);
        if (after != newIndex.end()) {
            if (!SameResidentPlacement(p,*after->second)) return fail("resident transform or identity changed");
        } else {
            if (!changed.count(p.refFormId)) return fail("unaffected resident placement removed");
            removed.insert(p.refFormId); next.removedReferences.push_back(p.refFormId);
        }
    }
    for (const auto& p : candidate) if (!oldIndex.count(p.refFormId)) {
        if (!changed.count(p.refFormId)) return fail("unaffected resident placement added");
        incoming.insert(p.refFormId); next.incomingReferences.push_back(p.refFormId);
        next.incomingPlacements.push_back(p);
    }
    for (size_t i = 0; i < live.size(); ++i) {
        const auto& shape = live[i];
        if (shape.nativeLod) { next.retainedSlots.push_back(i); continue; }
        if (!shape.reference) return fail("live visual has no reference identity");
        if (incoming.count(shape.reference)) return fail("incoming reference already has live visuals");
        if (changed.count(shape.reference) && !oldIndex.count(shape.reference) && !newIndex.count(shape.reference))
            return fail("affected live reference outside placement scope");
        if (!removed.count(shape.reference)) next.retainedSlots.push_back(i);
    }
    std::sort(next.removedReferences.begin(),next.removedReferences.end());
    std::sort(next.incomingReferences.begin(),next.incomingReferences.end());
    // Authored collision is independent of visual filtering/part expansion.
    next.collisionPlacements = SelectFo3AuthoredCollisionPlacements(candidate);
    out = std::move(next);
    return true;
}

// CPU worker orchestration used by the renderer. Every incoming renderable REFR
// must decode completely. The caller's output survives rejection/cancellation;
// partial results remain private and are destroyed. Build must enforce its full
// part contract, never return success after omitting unsupported parts.
template<class Part, class Build, class Skip>
bool PrepareResidentCpuParts(const ResidentResourceSelection& selection,
        std::vector<Part>& out, Build build, Skip skip,
        const std::atomic<bool>& cancelled, std::string& error) {
    error.clear();
    std::vector<Part> next;
    for (const auto& placement : selection.incomingPlacements) {
        if (cancelled.load(std::memory_order_acquire)) { error="resident preparation cancelled"; return false; }
        if (skip(placement)) { error="incoming visual policy unsupported"; return false; }
        std::vector<Part> parts;
        if (!build(placement,parts) || parts.empty()) { error="incoming visual decode incomplete"; return false; }
        for (auto& part : parts) {
            if (cancelled.load(std::memory_order_acquire)) { error="resident preparation cancelled"; return false; }
            next.push_back(std::move(part));
        }
    }
    if (cancelled.load(std::memory_order_acquire)) { error="resident preparation cancelled"; return false; }
    out = std::move(next);
    return true;
}
} // namespace fo3scene
