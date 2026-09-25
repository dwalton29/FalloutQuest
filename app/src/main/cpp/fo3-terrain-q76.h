#pragma once

#include <cstddef>
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
    // Q20: authored CELL force-hide LAND quadrant mask travels with the
    // decoded cell so background terrain preparation never depends on a
    // mutable process-global selection map.
    uint8_t forceHideMask = 0u;
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

// Q20 persistent per-CELL LAND streaming. Cold boot still uses the proven
// Q7.20 renderer; exterior movement promotes one seven-cell strip at a time.
void ResetFo3TerrainStreamingQ2000();
bool IsFo3TerrainStreamingCpuBusyQ2000();
bool IsFo3TerrainStreamingBusyQ2000();
bool GetFo3TerrainResidentCentreQ2000(int32_t* outGridX, int32_t* outGridY);
const Fo3TerrainCellQ76* FindFo3TerrainGroundCellQ2000(int32_t gridX, int32_t gridY);
size_t GetFo3TerrainGroundCellCountQ2000();
void UpdateFo3TerrainStreamingQ2000(
    uint32_t worldspaceFormId,
    int32_t actualGridX, int32_t actualGridY,
    float arrivalX, float arrivalY, float arrivalZ,
    float sceneForward, float floorY, float unitsPerMetre,
    bool allowCpuWorker);
