#pragma once

#include "fo3-megaton-scene.h"

#include <vector>

// Builds a batched debug-wireframe from Bethesda-authored NIF collision for the
// selected Megaton structural placements. Failure is non-fatal to the scene.
bool InitializeFo3CollisionOverlay(const std::vector<Fo3WorldPlacement>& placements,
                                   float centerX, float centerY, float floorZ,
                                   float sceneForward, float floorY,
                                   float unitsPerMetre);

// Draws the original Fallout collision as a bright cyan x-ray wireframe using
// the same per-eye MVP as the visible scene.
void RenderFo3CollisionOverlay(const float* mvp16);

void ShutdownFo3CollisionOverlay();
