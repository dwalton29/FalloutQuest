#pragma once

#include <cstdint>
#include <vector>

struct Fo3TerrainCellQ76 {
    uint32_t cellFormId = 0;
    uint32_t landFormId = 0;
    int32_t gridX = 0;
    int32_t gridY = 0;
    std::vector<float> heights;
    // Q10.1: authored LAND/VNML normals in Fallout game axes, xyz triples.
    // Grounding remains height-only; this is consumed by the visual renderer.
    std::vector<float> normals;
};

// CPU LAND/VHGT decode. This is deliberately independent of player collision.
bool LoadFo3TerrainQ76(uint32_t worldspaceFormId,
                       float arrivalX, float arrivalY, float arrivalZ);
const std::vector<Fo3TerrainCellQ76>& GetFo3TerrainQ76();

// Q7.6b visual-only terrain layer. It is initialized only after a successful
// exterior scene swap and never participates in house spawn or locomotion.
bool InitializeFo3TerrainRenderQ76(uint32_t worldspaceFormId,
                                   float arrivalX, float arrivalY, float arrivalZ,
                                   float sceneForward, float floorY,
                                   float unitsPerMetre);
void RenderFo3TerrainQ76(const float* mvp);
void ShutdownFo3TerrainRenderQ76();

// Q7.7 exterior-only physical LAND grounding. Activation happens only after
// the exterior scene/terrain swap succeeds, so the proven Q7.5 house path does
// not consult LAND at startup.
void ActivateFo3TerrainGroundingQ77(uint32_t worldspaceFormId,
                                    float arrivalX, float arrivalY, float arrivalZ,
                                    float sceneForward, float floorY,
                                    float unitsPerMetre);
bool SampleFo3TerrainGroundQ77(float virtualX, float virtualZ,
                               float* outPlayerYOffset);
bool IsFo3TerrainGroundingActiveQ77();
