#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Fo3WaterTypeQ2070 {
    uint32_t formId = 0u;
    std::string editorId;
    std::string noiseTexturePath;
    uint8_t opacity = 0u;
    uint8_t flags = 0u;
    float sunPower = 0.0f;
    float reflectivity = 0.0f;
    float fresnelAmount = 0.0f;
    float aboveFogNear = 0.0f;
    float aboveFogFar = 0.0f;
    float shallowColor[4]{0.0f, 0.0f, 0.0f, 1.0f};
    float deepColor[4]{0.0f, 0.0f, 0.0f, 1.0f};
    float reflectionColor[4]{0.0f, 0.0f, 0.0f, 1.0f};
    float noiseScale = 0.0f;
    float windDirection[3]{0.0f, 0.0f, 0.0f};
    float windSpeed[3]{0.0f, 0.0f, 0.0f};
    float depthFalloffStart = 0.0f;
    float depthFalloffEnd = 0.0f;
    float aboveFogAmount = 0.0f;
    float normalUvScale = 0.0f;
    float distortionAmount = 0.0f;
    float shininess = 0.0f;
    float reflectionHdrMultiplier = 0.0f;
    float amplitudeScale[3]{0.0f, 0.0f, 0.0f};
    float uvScale[3]{0.0f, 0.0f, 0.0f};
    bool valid = false;
};

struct Fo3WaterCellQ2070 {
    uint32_t cellFormId = 0u;
    int32_t gridX = 0;
    int32_t gridY = 0;
    float waterHeightGame = 0.0f;
    uint32_t waterTypeFormId = 0u;
    std::string noiseTexturePath;
    Fo3WaterTypeQ2070 type;
};

bool LoadFo3WaterSceneQ2070(uint32_t worldspaceFormId,
                            float arrivalX, float arrivalY);
void ClearFo3WaterSceneQ2070();
const std::vector<Fo3WaterCellQ2070>& GetFo3WaterCellsQ2070();

// Select the relevant authored horizontal water plane for this eye. Exterior
// water can contain multiple CELL heights in the resident window; prefer the
// CELL containing the eye's game-space XY, otherwise the nearest CELL centre.
bool GetFo3DominantWaterPlaneQ2070(float eyeSceneX, float eyeSceneZ,
                                   float originGameX, float originGameY,
                                   float originGameZ, float floorY,
                                   float sceneForward, float unitsPerMetre,
                                   float* outPlaneSceneY,
                                   float* outHeightGame,
                                   uint32_t* outCellFormId,
                                   uint32_t* outWaterTypeFormId);

size_t GetFo3ExposedWaterCellCountQ209B();
bool IsFo3WaterPotentiallyVisibleQ209B(const float* mvp16,
                                       float originGameX,
                                       float originGameY,
                                       float originGameZ,
                                       float floorY,
                                       float sceneForward,
                                       float unitsPerMetre);

// Q20.9 WATER000 adds the PC planar ReflectionMap on top of Q20.8A's
// source-backed WATER001 refraction/depth/noise path.
void RenderFo3WaterSurfaceQ2070(const float* mvp16,
                                float originGameX,
                                float originGameY,
                                float originGameZ,
                                float floorY,
                                float sceneForward,
                                float unitsPerMetre,
                                uint32_t sceneColorTexture,
                                uint32_t sceneDepthTexture,
                                int sceneWidth,
                                int sceneHeight,
                                bool sceneSnapshotReady,
                                const float eyePosition[3],
                                const float sunDirection[3],
                                const float sunColor[3],
                                const float sceneFogColor[3],
                                float sceneFogNearMetres,
                                float sceneFogFarMetres,
                                float sceneFogPower,
                                float nearClipMetres,
                                float farClipMetres,
                                uint32_t reflectionTexture,
                                bool reflectionReady,
                                const float reflectionMvp16[16]);
void ShutdownFo3WaterQ2070();
