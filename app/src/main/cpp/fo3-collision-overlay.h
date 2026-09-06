#pragma once

#include "fo3-megaton-scene.h"

#include <vector>

// Builds the shared authored-collision world from Bethesda NIF bhk shapes for
// selected Megaton structural placements. Q6F can render it as a debug overlay;
// Q6G consumes the exact same transformed triangles for locomotion.
bool InitializeFo3CollisionOverlay(const std::vector<Fo3WorldPlacement>& placements,
                                   float centerX, float centerY, float floorZ,
                                   float sceneForward, float floorY,
                                   float unitsPerMetre);

// Resolves a standing player capsule against the Q6F-authored Fallout collision
// world. X/Z are the virtual headset/body centre in metres. playerYOffset is the
// locomotion-space Y translation; grounding updates it relative to floorY.
// Returns false only when the collision world is unavailable.
bool ResolveFo3PlayerMotionQ6G(float currentX, float currentZ,
                               float desiredX, float desiredZ,
                               float currentPlayerYOffset,
                               float* outX, float* outZ,
                               float* outPlayerYOffset);

bool IsFo3PlayerCollisionReadyQ6G();

// Q6F debug draw. Q6G keeps this disabled by default while retaining the same
// collision data for physical movement.
void RenderFo3CollisionOverlay(const float* mvp16);

void ShutdownFo3CollisionOverlay();
