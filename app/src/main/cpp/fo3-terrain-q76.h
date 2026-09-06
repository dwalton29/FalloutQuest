#pragma once

#include <GLES3/gl3.h>
#include <cstdint>

bool InitializeFo3TerrainQ76(uint32_t worldspaceFormId,
                            float arrivalX, float arrivalY, float arrivalZ,
                            float sceneForward, float floorY,
                            float unitsPerMetre);
void RenderFo3TerrainQ76(const float* mvp);
void ShutdownFo3TerrainQ76();
