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

// Q7.5 exterior loader. The XTEL-linked CELL can be a worldspace persistent
// cell, so this discovers the WRLD's XCLC grid cells, merges the persistent
// refs with the relevant exterior cells around the arrival point (or all cells
// for a small dedicated worldspace), and returns one combined placement set.
bool LoadFo3WorldspaceNeighborhoodQ75(uint32_t worldspaceFormId,
                                     uint32_t persistentCellFormId,
                                     float arrivalX, float arrivalY,
                                     std::vector<Fo3WorldPlacement>& outPlacements);

// Q7.8a collision wrappers. The generated Q6H/Q7 renderer includes this header
// after fo3-collision-overlay.h, so these macros route only runtime call sites;
// the original Q7.7 collision implementation remains compiled and is called by
// the wrappers first. Supplemental bhk collision is dormant until exterior LAND
// is active, preserving the proven player-house path.
bool InitializeFo3CollisionOverlayQ78(const std::vector<Fo3WorldPlacement>& placements,
                                      float centerX, float centerY, float floorZ,
                                      float sceneForward, float floorY,
                                      float unitsPerMetre);
bool ResolveFo3PlayerMotionQ78(float currentX, float currentZ,
                               float desiredX, float desiredZ,
                               float currentPlayerYOffset,
                               float* outX, float* outZ,
                               float* outPlayerYOffset);

#define InitializeFo3CollisionOverlay InitializeFo3CollisionOverlayQ78
#define ResolveFo3PlayerMotionQ6G ResolveFo3PlayerMotionQ78
