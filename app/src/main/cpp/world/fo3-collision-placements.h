#pragma once
#include "fo3-megaton-scene.h"
#include <unordered_set>

// Collision candidates retain the original REFR/MODL transform, independent of
// visual support or how many visible parts the renderer expands from a NIF.
inline std::vector<Fo3WorldPlacement> SelectFo3AuthoredCollisionPlacements(
        const std::vector<Fo3WorldPlacement>& placements) {
    std::vector<Fo3WorldPlacement> selected;
    selected.reserve(placements.size());
    std::unordered_set<uint32_t> refs;
    for (const auto& placement : placements)
        if (placement.refFormId != 0u && !placement.modelPath.empty() &&
            refs.insert(placement.refFormId).second) selected.push_back(placement);
    return selected;
}

