#include "rendering/terrain/fo3-terrain.h"

#include <android/log.h>

#include "physics/fo3-land-surface.h"
#include "physics/fo3-rigid-world.h"
#include <unordered_map>
#include <unordered_set>
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

    const Fo3TerrainCellQ76* cell =
        FindFo3TerrainGroundCellQ2000(gridX, gridY);
    if (!cell || cell->heights.size() != HEIGHT_COUNT_Q77) return false;

    const float localX = gameX - static_cast<float>(gridX) * CELL_SIZE_Q77;
    const float localY = gameY - static_cast<float>(gridY) * CELL_SIZE_Q77;
    const int quadrant = (localY >= CELL_SIZE_Q77 * 0.5f ? 2 : 0) +
                         (localX >= CELL_SIZE_Q77 * 0.5f ? 1 : 0);
    if ((cell->forceHideMask & static_cast<uint8_t>(1u << quadrant)) != 0u)
        return false;
    const float gx = std::clamp(localX / VERTEX_SPACING_Q77, 0.0f, 32.0f);
    const float gy = std::clamp(localY / VERTEX_SPACING_Q77, 0.0f, 32.0f);
    const int x0 = std::clamp(static_cast<int>(std::floor(gx)), 0, 32);
    const int y0 = std::clamp(static_cast<int>(std::floor(gy)), 0, 32);
    const int x1 = std::min(x0 + 1, 32);
    const int y1 = std::min(y0 + 1, 32);
    const float tx = gx - static_cast<float>(x0);
    const float ty = gy - static_cast<float>(y0);

    auto h = [&](int x, int y) {
        return cell->heights[static_cast<size_t>(y * LAND_SIDE_Q77 + x)];
    };
    outGameZ = fo3land::Height(h(x0,y0),h(x1,y0),h(x0,y1),h(x1,y1),tx,ty);
    return std::isfinite(outGameZ);
}

} // namespace

void ActivateFo3TerrainGroundingQ77(uint32_t worldspaceFormId,
                                    float arrivalX, float arrivalY, float arrivalZ,
                                    float sceneForward, float floorY,
                                    float unitsPerMetre) {
    gGroundingActiveQ77 = false;
    if (worldspaceFormId == 0u || unitsPerMetre <= 1e-4f ||
        GetFo3TerrainGroundCellCountQ2000() == 0u) return;

    gGroundingWorldspaceQ77 = worldspaceFormId;
    gArrivalXQ77 = arrivalX;
    gArrivalYQ77 = arrivalY;
    gArrivalZQ77 = arrivalZ;
    gSceneForwardQ77 = sceneForward;
    gFloorYQ77 = floorY;
    gUnitsPerMetreQ77 = unitsPerMetre;
    gSampleCountQ77 = 0u;
    gGroundingActiveQ77 = true;

    Q77_LOGI("Q7.20 LAND PHYSICS READY: worldspace=%08X cells=%zu XTEL=(%.2f %.2f %.2f) visualFrameShared=1 housePathUntouched=1",
             gGroundingWorldspaceQ77, GetFo3TerrainGroundCellCountQ2000(),
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
        Q77_LOGI("Q7.20 LAND SAMPLE: vr=(%.3f %.3f) game=(%.1f %.1f %.1f) playerY=%.3f sample=%llu",
                 virtualX, virtualZ, gameX, gameY, gameZ, *outPlayerYOffset,
                 static_cast<unsigned long long>(gSampleCountQ77));
    }
    return true;
}

bool IsFo3TerrainGroundingActiveQ77() {
    return gGroundingActiveQ77;
}

bool SampleFo3TerrainFootstepMaterial(float virtualX,float virtualZ,uint32_t* material) {
    if(!material||!gGroundingActiveQ77)return false;
    const float x=gArrivalXQ77+virtualX*gUnitsPerMetreQ77;
    const float y=gArrivalYQ77-(virtualZ-gSceneForwardQ77)*gUnitsPerMetreQ77;
    if(!std::isfinite(x)||!std::isfinite(y))return false;
    const int32_t cx=int32_t(std::floor(x/4096.f)),cy=int32_t(std::floor(y/4096.f));
    const auto* cell=FindFo3TerrainGroundCellQ2000(cx,cy);
    if(!cell)return false;
    *material=Fo3TerrainContactMaterial(cell->landFormId,x-cx*4096.f,y-cy*4096.f);
    return true;
}

// Committed, stitched visual heights only. Rebuild entering/changed cells, not
// every frame; hidden quadrants never create invisible floors.
void SynchronizeFo3TerrainPhysics(float x,float z) {
    struct Cached {std::vector<float> heights;uint8_t hidden=0;};
    static std::unordered_map<uint64_t,Cached> cached;
    static float origin[3]{INFINITY,INFINITY,INFINITY};
    auto& world=fo3rigid::Live();
    const bool changed=origin[0]!=gArrivalXQ77||origin[1]!=gArrivalYQ77||origin[2]!=gArrivalZQ77;
    if(changed||!gGroundingActiveQ77){for(const auto& e:cached)world.RemoveStatic(e.first);cached.clear();}
    origin[0]=gArrivalXQ77;origin[1]=gArrivalYQ77;origin[2]=gArrivalZQ77;
    if(!gGroundingActiveQ77)return;
    const int cx=int(std::floor((gArrivalXQ77+x*gUnitsPerMetreQ77)/4096.f));
    const int cy=int(std::floor((gArrivalYQ77-(z-gSceneForwardQ77)*gUnitsPerMetreQ77)/4096.f));
    std::unordered_set<uint64_t> live;
    for(int gy=cy-1;gy<=cy+1;++gy)for(int gx=cx-1;gx<=cx+1;++gx){
        const auto* cell=FindFo3TerrainGroundCellQ2000(gx,gy);if(!cell||cell->heights.size()!=1089)continue;
        const uint64_t id=(uint64_t{1}<<62)|(uint64_t(uint32_t(gx)&0x7fffffff)<<31)|(uint32_t(gy)&0x7fffffff);
        live.insert(id);auto& old=cached[id];
        if(old.heights==cell->heights&&old.hidden==cell->forceHideMask)continue;
        std::vector<fo3rigid::Triangle> triangles;triangles.reserve(2048);
        auto point=[&](int px,int py)->fo3rigid::V{return {
            (gx*4096.f+px*128.f-gArrivalXQ77)/gUnitsPerMetreQ77,
            gFloorYQ77+(cell->heights[py*33+px]-gArrivalZQ77)/gUnitsPerMetreQ77,
            gSceneForwardQ77-(gy*4096.f+py*128.f-gArrivalYQ77)/gUnitsPerMetreQ77};};
        for(int py=0;py<32;++py)for(int px=0;px<32;++px){
            const int q=(py>=16?2:0)+(px>=16?1:0);if(cell->forceHideMask&(1<<q))continue;
            const auto a=point(px,py),b=point(px+1,py),c=point(px,py+1),d=point(px+1,py+1);
            triangles.push_back({a,b,d});triangles.push_back({a,d,c});
        }
        world.Static(id,triangles);old.heights=cell->heights;old.hidden=cell->forceHideMask;
    }
    for(auto i=cached.begin();i!=cached.end();)if(!live.count(i->first)){world.RemoveStatic(i->first);i=cached.erase(i);}else ++i;
}
