#pragma once

#include "fo3-megaton-scene.h"

#include <cstdint>
#include <vector>

struct Fo3CellTransitionRequestQ74 {
    uint32_t destinationDoorRef = 0;
    uint32_t cellFormId = 0;
    uint32_t worldspaceFormId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
    bool valid = false;
};

// Decoded Fallout 3 LAND geometry for one exterior CELL. Heights are absolute
// Bethesda game-Z coordinates in a bottom-up 33x33 grid; X/Y are derived from
// the CELL's XCLC coordinate at 128 game units per vertex.
struct Fo3TerrainCellQ76 {
    uint32_t cellFormId = 0;
    uint32_t landFormId = 0;
    int32_t gridX = 0;
    int32_t gridY = 0;
    std::vector<float> heights;
};

// Renderer-side transition queue. The input callback only enqueues; GL/collision
// teardown and rebuild happen later from the render thread.
bool ConsumeFo3CellTransitionRequestQ74(Fo3CellTransitionRequestQ74& outRequest);
void CompleteFo3CellTransitionQ74(uint32_t cellFormId);

// Consumed by the authored collision resolver on the frame after a successful
// scene swap. Returning the body centre to VR (0,0) maps the player to XTEL.
bool ConsumeFo3PlayerResetQ74();

// Generic ESM4 CELL loader used by Q7.4 after the door has resolved its owning
// destination CELL. Only active, model-bearing REFRs are returned.
bool LoadFo3CellPlacementsQ74(uint32_t cellFormId,
                             std::vector<Fo3WorldPlacement>& outPlacements);

// Q7.5 exterior loader. The XTEL-linked CELL can be a worldspace persistent
// cell, so this discovers the WRLD's XCLC grid cells, merges the persistent
// refs with the relevant exterior cells around the arrival point (or all cells
// for a small dedicated worldspace), and returns one combined placement set.
bool LoadFo3WorldspaceNeighborhoodQ75(uint32_t worldspaceFormId,
                                     uint32_t persistentCellFormId,
                                     float arrivalX, float arrivalY,
                                     std::vector<Fo3WorldPlacement>& outPlacements);

// Q7.6 LAND loader. Uses the same Q7.5 selected worldspace/XCLC domain, decodes
// the VHGT delta height maps, and retains them for the render/collision rebuild.
bool LoadFo3TerrainQ76(uint32_t worldspaceFormId,
                       float arrivalX, float arrivalY, float arrivalZ);
const std::vector<Fo3TerrainCellQ76>& GetFo3TerrainQ76();
