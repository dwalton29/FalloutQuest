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
