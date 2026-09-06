#include "fo3-terrain-q76.h"

#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

constexpr const char* TAG_Q77 = "FalloutQuest";
constexpr float CELL_SIZE_Q77 = 4096.0f;
constexpr int LAND_SIDE_Q77 = 33;
constexpr int LAND_QUADS_Q77 = 32;
constexpr float VERTEX_SPACING_Q77 = CELL_SIZE_Q77 / static_cast<float>(LAND_QUADS_Q77);
constexpr size_t HEIGHT_COUNT_Q77 = static_cast<size_t>(LAND_SIDE_Q77 * LAND_SIDE_Q77);

#define Q77_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG_Q77, __VA_ARGS__)

bool gGroundingActiveQ77 = false;
uint32_t gGroundingWorldspaceQ77 = 0u;
float gArrivalXQ77 = 0.0f;
float gArrivalYQ77 = 0.0f;
float gArrivalZQ77 = 0.0f;
float gSceneForwardQ77 = 0.0f;
float gFloorYQ77 = -1.55f;
float gUnitsPerMetreQ77 = 70.0f;
uint64_t gSampleCountQ77 = 0u;

bool SampleGameHeightQ77(float gameX, float gameY, float& outGameZ) {
    const int32_t gridX = static_cast<int32_t>(std::floor(gameX / CELL_SIZE_Q77));
    const int32_t gridY = static_cast<int32_t>(std::floor(gameY / CELL_SIZE_Q77));

    const auto& terrain = GetFo3TerrainQ76();
    for (const Fo3TerrainCellQ76& cell : terrain) {
        if (cell.gridX != gridX || cell.gridY != gridY ||
            cell.heights.size() != HEIGHT_COUNT_Q77) continue;

        const float localX = gameX - static_cast<float>(gridX) * CELL_SIZE_Q77;
        const float localY = gameY - static_cast<float>(gridY) * CELL_SIZE_Q77;
        const float gx = std::clamp(localX / VERTEX_SPACING_Q77, 0.0f, 32.0f);
        const float gy = std::clamp(localY / VERTEX_SPACING_Q77, 0.0f, 32.0f);
        const int x0 = std::clamp(static_cast<int>(std::floor(gx)), 0, 32);
        const int y0 = std::clamp(static_cast<int>(std::floor(gy)), 0, 32);
        const int x1 = std::min(x0 + 1, 32);
        const int y1 = std::min(y0 + 1, 32);
        const float tx = gx - static_cast<float>(x0);
        const float ty = gy - static_cast<float>(y0);

        auto h = [&](int x, int y) {
            return cell.heights[static_cast<size_t>(y * LAND_SIDE_Q77 + x)];
        };
        const float h0 = h(x0, y0) * (1.0f - tx) + h(x1, y0) * tx;
        const float h1 = h(x0, y1) * (1.0f - tx) + h(x1, y1) * tx;
        outGameZ = h0 * (1.0f - ty) + h1 * ty;
        return std::isfinite(outGameZ);
    }
    return false;
}

} // namespace

void ActivateFo3TerrainGroundingQ77(uint32_t worldspaceFormId,
                                    float arrivalX, float arrivalY, float arrivalZ,
                                    float sceneForward, float floorY,
                                    float unitsPerMetre) {
    gGroundingActiveQ77 = false;
    if (worldspaceFormId == 0u || unitsPerMetre <= 1e-4f || GetFo3TerrainQ76().empty()) return;

    gGroundingWorldspaceQ77 = worldspaceFormId;
    gArrivalXQ77 = arrivalX;
    gArrivalYQ77 = arrivalY;
    gArrivalZQ77 = arrivalZ;
    gSceneForwardQ77 = sceneForward;
    gFloorYQ77 = floorY;
    gUnitsPerMetreQ77 = unitsPerMetre;
    gSampleCountQ77 = 0u;
    gGroundingActiveQ77 = true;

    Q77_LOGI("Q7.7 LAND PHYSICS READY: worldspace=%08X cells=%zu XTEL=(%.2f %.2f %.2f) visualFrameShared=1 housePathUntouched=1",
             gGroundingWorldspaceQ77, GetFo3TerrainQ76().size(),
             gArrivalXQ77, gArrivalYQ77, gArrivalZQ77);
}

bool SampleFo3TerrainGroundQ77(float virtualX, float virtualZ,
                               float* outPlayerYOffset) {
    if (!gGroundingActiveQ77 || !outPlayerYOffset || gUnitsPerMetreQ77 <= 1e-4f) return false;

    const float gameX = gArrivalXQ77 + virtualX * gUnitsPerMetreQ77;
    const float gameY = gArrivalYQ77 - (virtualZ - gSceneForwardQ77) * gUnitsPerMetreQ77;
    float gameZ = 0.0f;
    if (!SampleGameHeightQ77(gameX, gameY, gameZ)) return false;

    *outPlayerYOffset = (gameZ - gArrivalZQ77) / gUnitsPerMetreQ77;
    ++gSampleCountQ77;
    if (gSampleCountQ77 <= 4u || (gSampleCountQ77 % 720u) == 0u) {
        Q77_LOGI("Q7.7 LAND SAMPLE: vr=(%.3f %.3f) game=(%.1f %.1f %.1f) playerY=%.3f sample=%llu",
                 virtualX, virtualZ, gameX, gameY, gameZ, *outPlayerYOffset,
                 static_cast<unsigned long long>(gSampleCountQ77));
    }
    return true;
}

bool IsFo3TerrainGroundingActiveQ77() {
    return gGroundingActiveQ77;
}

// Textually compile the Q7.8a supplemental authored-bhk layer in the same
// transition translation unit as Q7.7. It wraps (rather than replaces) the
// proven Q7.7 collision resolver and stays dormant until exterior LAND is active.
#include "fo3-collision-extra-q78.cpp"
