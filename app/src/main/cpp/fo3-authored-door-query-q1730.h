#pragma once

#include "fo3-cell-traversal-q1700.h"

#include <cstdint>

// Q16.3 fallback for authored load doors that are not represented by a drawable
// GPU DOOR mesh. The transition translation unit caches enabled XTEL-bearing
// REFRs for the current CELL and ray-tests their authored DATA positions in the
// same virtual OpenXR frame used by the static renderer.
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
                                     Fo3DoorAimQ1700* outAim);
