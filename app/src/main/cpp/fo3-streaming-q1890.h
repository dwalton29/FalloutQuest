#pragma once

#include <cstdint>
#include <vector>

// Q16.17: authored exterior-cell streaming foundation.
//
// This API is intentionally metadata-only. The first pass proves that the live
// player crosses the same 4096-unit XCLC cells Fallout3.esm defines and computes
// the exact entering/leaving edge of the 3x3 window without reading the ESM from
// the interactive frame loop. Q16.18 can consume the same deltas for real cell
// load/unload work once the ownership math is device-verified.

struct Fo3ExteriorGridCellQ1890 {
    uint32_t cellFormId = 0u;
    int32_t gridX = 0;
    int32_t gridY = 0;
};

// Returns the already-cached authored XCLC cell covering a Fallout game-space
// position. This function performs no file IO; the cache is populated while the
// worldspace is loaded through LoadFo3WorldspaceNeighborhoodQ75.
bool GetFo3ExteriorCellAtPositionQ1890(uint32_t worldspaceFormId,
                                       float gameX, float gameY,
                                       Fo3ExteriorGridCellQ1890* outCell);

// Returns the cached square grid window around an authored XCLC centre. No ESM
// scan is permitted here. Missing/nonexistent cells simply do not appear.
bool GetFo3ExteriorWindowQ1890(uint32_t worldspaceFormId,
                               int32_t centerGridX, int32_t centerGridY,
                               int radius,
                               std::vector<Fo3ExteriorGridCellQ1890>& outCells);

// Renderer bridge called once per interactive frame with the player's virtual
// OpenXR body centre. Q16.17 only detects/logs window shifts; it does not mutate
// render, terrain or collision state.
void UpdateFo3ExteriorStreamingProbeQ1890(float virtualX, float virtualZ);
