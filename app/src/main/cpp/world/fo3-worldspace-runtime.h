#pragma once

#include "fo3-megaton-scene.h"

// Negative radius preserves the initial-arrival policy: whole small worldspaces
// or a 5x5 window. Streaming workers explicitly request one CELL (radius 0).
// Selection belongs to the request, so simultaneous callers cannot affect it.
struct Fo3WorldspaceSelection {
    uint32_t worldspace = 0u;
    uint32_t persistentCell = 0u;
    float gameX = 0.0f;
    float gameY = 0.0f;
    int gridRadius = -1;
};

bool LoadFo3WorldspacePlacements(
    const Fo3WorldspaceSelection& selection,
    std::vector<Fo3WorldPlacement>& outPlacements);
