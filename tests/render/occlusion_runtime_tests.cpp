#include "rendering/conservative-occlusion.h"
#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#define PROP_VALUE_MAX 92
int __system_property_get(const char*,char* value){value[0]=0;return 0;}
#define Q6H_LOGI(...) std::printf(__VA_ARGS__)
struct GpuObject {
    std::shared_ptr<const std::vector<fo3occ::Surface>> occluders;
    mutable uint64_t occlusionGeneration=0;
    mutable bool occlusionHidden=false;
    bool q220LooseObject=false,physicalWeapon=false,q2400SwingDoor=false,
         q210PlayerBody=false,q230NpcActor=false,q1990NativeLod=false;
    int skinMode=0;std::string baseRecordType="STAT";
    float minX=-.03f,maxX=.03f,minY=-.03f,maxY=.03f,minZ=.4f,maxZ=.5f;
    bool q2025LandscapeRock=false,q2025VisibleWhenDistant=false;
    bool stencilDrawModePresent=false;int stencilDrawMode=0;
    uint32_t refFormId=0;uint32_t vertexCount=9000;
    bool live=true;
};
uint64_t gStereoFrame=1;
float gStereoMvp[2][16]{{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},
                      {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}};
bool gStereoMvpReady=true,gExteriorStreamingActiveQ1890=true,gWaterReflectionPassQ2090=false;
uint32_t gExteriorWorldspaceQ1890=0xA74;
struct Publication {uint64_t rev=1;uint64_t Revision()const{return rev;}}gFo3VisualPublication;
struct Environment {bool interior=false;}gFo3Environment;
struct Session {struct Player {bool IsCollected(uint32_t)const{return false;}}player;};
Session* gPlayerSession=nullptr;
bool gQ2021PlayerSceneValid=false;
float gQ2021PlayerSceneX=0,gQ2021PlayerSceneZ=0;
float Q2025_ROCK_FADE_START_M=146,Q2021_DETAIL_FADE_START_M=87;
float gExteriorOriginXQ1890=0,gExteriorOriginYQ1890=0,gExteriorOriginZQ1890=0;
float gFo3EyePosition[3]{};
constexpr float FLOOR_Y=-1.55f,SCENE_FORWARD=0;
struct Fo3TerrainCellQ76 {int gridX=0,gridY=0;std::vector<float> heights;uint8_t forceHideMask=0;};
const Fo3TerrainCellQ76* GetFo3TerrainOcclusionCellQ241(uint32_t,int,int,float,float,float){return nullptr;}
bool Q1970ShouldRenderFullDetail(const GpuObject& o){return o.live;}
bool Q2015AabbVisible(const GpuObject&){return true;}
std::vector<GpuObject>gObjects;
std::vector<const GpuObject*>gOccluderFrameOrder;
bool loading=false;
bool IsFo3LoadingVisible(){return loading;}
#include "rendering/fo3-occlusion-runtime.inc"
int main() {
    gObjects.resize(3);
    const fo3occ::Surface wall{{{-.9f,-.9f,-.5f},{.9f,-.9f,-.5f},
                                {.9f,.9f,-.5f},{-.9f,.9f,-.5f}},4};
    gObjects[0].occluders=std::make_shared<const std::vector<fo3occ::Surface>>(1,wall);
    gObjects[0].minZ=-.6f;gObjects[0].maxZ=-.4f;
    gOccluderFrameOrder.push_back(&gObjects[0]);
    q241::Prepare();assert(q241::active);
    assert(Q241Hidden(gObjects[1]));
    assert(!Q241Hidden(gObjects[0])); // Never use a blocker to hide itself.
    const auto tests=q241::tests;
    assert(Q241Hidden(gObjects[1]));assert(q241::tests==tests); // Shared stereo/batch cache.
    gWaterReflectionPassQ2090=true;assert(!Q241Hidden(gObjects[1]));gWaterReflectionPassQ2090=false;
    loading=true;assert(!Q241Hidden(gObjects[1]));loading=false;
    gObjects[2].q220LooseObject=true;assert(!Q241Hidden(gObjects[2]));gObjects[2].q220LooseObject=false;
    q241::tests=q241::MaxTests;assert(!Q241Hidden(gObjects[2]));assert(q241::stopped);
    // Same-frame camera changes invalidate both buffers and per-object decisions.
    gStereoMvp[1][12]=.99f;q241::Prepare();
    assert(!Q241Hidden(gObjects[1]));
    // An unpublished/replaced scene cannot inherit hidden state.
    ++gStereoFrame;assert(!Q241Hidden(gObjects[1]));
    gStereoMvp[1][12]=0;++gFo3VisualPublication.rev;
    gObjects[0].live=false;q241::cooldown=0;q241::Prepare();
    assert(!Q241Hidden(gObjects[1]));
    // A missed/empty scene causes a bounded retry delay, never historical culls.
    ++gStereoFrame;q241::Prepare();assert(q241::cooldown>0);assert(!q241::active);
    // Interior and unknown stereo cameras retain normal rendering.
    ++gStereoFrame;gFo3Environment.interior=true;q241::Prepare();assert(!Q241Hidden(gObjects[1]));
    gFo3Environment.interior=false;++gStereoFrame;gStereoMvpReady=false;q241::Prepare();
    assert(!Q241Hidden(gObjects[1]));
    std::puts("Occlusion runtime: stereo reuse, mirror isolation, publication, movement and budget passed");
}
