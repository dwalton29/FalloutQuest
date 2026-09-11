#pragma once

#include <cstdint>

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
