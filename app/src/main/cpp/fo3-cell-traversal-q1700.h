#pragma once

#include <cstdint>

// Minimal authored XTEL payload shared by the active renderer and transition TU.
// This deliberately lives in Q16 rather than depending on the retired q7-runtime
// renderer transform.
struct Fo3DoorTeleport {
    uint32_t destinationDoorRefFormId = 0u;
    uint32_t destinationCellFormId = 0u;
    uint32_t flags = 0u;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
    bool valid = false;
};

struct Fo3DoorAimQ1700 {
    bool valid = false;
    uint32_t sourceDoorRef = 0u;
    uint32_t destinationDoorRef = 0u;
    float distance = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
};

// Resolve one rendered REFR's authored XTEL directly from Fallout3.esm. The
// renderer calls this only while uploading DOOR placements and caches the result;
// it is never an every-frame ESM scan.
bool ResolveFo3DoorTeleportQ1700(uint32_t sourceDoorRef,
                                 Fo3DoorTeleport* outTeleport);

// Renderer-side authored DOOR targeting. Coordinates/direction are in the same
// virtual OpenXR scene frame as the static NIF renderer.
bool QueryFo3DoorAimQ1700(float originX, float originY, float originZ,
                          float dirX, float dirY, float dirZ,
                          Fo3DoorAimQ1700* outAim);

// Queue the currently aimed authored XTEL door. The actual scene replacement is
// performed by the existing Q7.4 render-thread CELL swap after one loading frame.
bool ActivateFo3DoorQ1700(float originX, float originY, float originZ,
                          float dirX, float dirY, float dirZ);

// Transition-TU entry point used by ActivateFo3DoorQ1700. Destination ownership
// (interior CELL vs exterior WRLD/CELL) is resolved from Fallout3.esm here.
bool QueueFo3DoorTransitionQ1700(uint32_t sourceDoorRef,
                                 uint32_t destinationDoorRef,
                                 float x, float y, float z,
                                 float rx, float ry, float rz);
