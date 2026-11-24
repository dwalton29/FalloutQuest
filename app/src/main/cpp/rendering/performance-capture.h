#pragma once
// Render-thread owned, bounded session capture. No allocation, locks or file I/O
// during recorded frames. Parent scopes include children; never sum them blindly.
#include "spacewarp/fo3-history-rejection.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>

namespace fqperf {
inline bool reuseVulkanBufferBinds=true;
using Clock=std::chrono::steady_clock;
inline double Us(Clock::time_point t) {return std::chrono::duration<double,std::micro>(Clock::now()-t).count();}
inline double ThreadUs() {
#if defined(CLOCK_THREAD_CPUTIME_ID)
    timespec t{};
    if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)==0)return t.tv_sec*1e6+t.tv_nsec/1000.0;
#endif
    return -1;
}
enum Metric : unsigned {
    Wait,Begin,Input,Gameplay,Streaming,ActorPrep,Simulation,Packages,Persistence,
    Unloaded,Arrivals,Animation,Facial,Palette,Upload,Target,Sight,Path,Movement,
    WeaponChoice,Eye0,Eye1,End,Work,Cadence,ScenePrep,Clip,Pose,
    Native0,Native1,Detail0,Detail1,NpcDraw0,NpcDraw1,PlayerDraw0,PlayerDraw1,Swapchain,Scripts,Motion,Interaction,NpcGround,NpcPortal,NpcObstruction,NpcCrowd,NpcArrivalPart,
    VrInteractionUpdate,PlayerRigUpdate,VatsUpdate,MenuUpdate,PipboyUpdate,DialogueUpdate,
    WeaponUpdate,RigidWorldUpdate,RigidSync,RigidStep,RagdollUpdate,LooseGrab,UnarmedUpdate,ScriptSounds,
    ObjectUpload,DoorResolve,RigidTick,RigidSolve,RigidBodySweep,UploadFenceWait,
    NpcPointerBounds,PlayerPlanar,PlayerGravity,PlayerStance,RigidFullRefresh,RigidPartialRefresh,RigidTerrainSync,CollisionSnapshotPublish,
    RigidPredictiveContacts,RigidUpdateBounds,RigidOverlapPairs,RigidContactDispatch,RigidIslands,RigidConstraints,RigidIntegrate,
    RigidGroupSetup,RigidGroupIterations,RigidGroupFinish,Count
};
inline constexpr const char* names[]={"xrWaitUs","xrBeginUs","inputTrackingUs","gameplayUs",
    "streamingUs","actorPrepUs","npcSimulationUs","npcPackagesUs","npcPersistenceUs",
    "npcUnloadedUs","npcArrivalsUs","npcAnimationUs","npcFacialUs","npcPaletteUs",
    "npcUploadUs","npcTargetUs","npcSightUs","npcPathUs","npcMovementUs",
    "npcWeaponChoiceUs","eye0CpuUs","eye1CpuUs","xrEndUs","workUs","cadenceUs",
    "scenePrepUs","npcClipUs","npcPoseUs","native0CpuUs","native1CpuUs",
    "detail0CpuUs","detail1CpuUs","npcDraw0CpuUs","npcDraw1CpuUs","playerDraw0CpuUs","playerDraw1CpuUs","swapchainAcquireWaitUs","scriptsUs","playerMotionUs","interactionUs",
    "npcGroundUs","npcPortalUs","npcObstructionUs","npcCrowdUs","npcArrivalPartUs",
    "vrInteractionUpdateUs","playerRigUpdateUs","vatsUpdateUs","menuUpdateUs","pipboyUpdateUs","dialogueUpdateUs",
    "weaponUpdateUs","rigidWorldUpdateUs","rigidSyncUs","rigidStepUs","ragdollUpdateUs","looseGrabUs",
    "unarmedUpdateUs","scriptSoundsUs","objectUploadUs","doorResolveUs","rigidTickUs","rigidSolveUs","rigidBodySweepUs","vkUploadFenceWaitUs",
    "npcPointerBoundsUs","playerPlanarUs","playerGravityUs","playerStanceUs",
    "rigidFullRefreshUs","rigidPartialRefreshUs","rigidTerrainSyncUs","collisionSnapshotPublishUs",
    "rigidPredictiveContactsUs","rigidUpdateBoundsUs","rigidOverlapPairsUs","rigidContactDispatchUs","rigidIslandsUs","rigidConstraintsUs","rigidIntegrateUs",
    "rigidGroupSetupUs","rigidGroupIterationsUs","rigidGroupFinishUs"};
static_assert(sizeof(names)/sizeof(*names)==Count);
enum GpuPass : unsigned { GpuNative, GpuDetailed, GpuNpc, GpuPlayer,
    GpuAlpha, GpuEnvironment, GpuTerrain, GpuReflection, GpuWater,
    GpuResolve, GpuExposure, GpuPost, GpuBloom, GpuDepthCopy, GpuSky, GpuMirrorTerrain, GpuHdrFirst, GpuHdrContinuation, GpuPassCount,
    GpuEyeTotal=GpuPassCount };
inline constexpr const char* gpuPassNames[]={"native","detailed","npc","player",
    "alpha","environment","terrain","reflection","water","resolve","exposure",
    "post","bloom","depthCopy","sky","mirrorTerrain","hdrFirstPass","hdrContinuation"};
static_assert(sizeof(gpuPassNames)/sizeof(*gpuPassNames)==GpuPassCount);
inline constexpr const char* vkPassNames[]={"scene","sceneResume","ui","adapt","bloom","film","output","snapshot","depthCopy","overlay","motion","composition","motionGeometry"};
inline constexpr unsigned VkPassCount=sizeof(vkPassNames)/sizeof(*vkPassNames);
// Offset the optional census from the GPU timing frames (serial % 8 == 0).
inline bool SampleScenery(uint64_t serial){return serial%32==4;}
struct SceneryCounts {
    uint32_t detailedDraws=0,instancedDraws=0,alreadySaved=0,eligibleDraws=0,additionalMatches=0;
};
struct Frame {
    bool scenerySampled=false;
    SceneryCounts scenery;
    uint64_t serial=0;
    std::array<double,Count> us{};
    std::array<uint32_t,Count> calls{};
    std::array<double,Count> maxUs{};
    double threadCpuUs=-1,elapsedUs=0,slowObjectUs=0;
    uint32_t slowObjectRef=0,slowObjectMetric=0;
    uint32_t movableBoundsUpdated=0,staticBoundsReused=0;
    float ragdollRequestHeight=0,ragdollRequestVelocityY=0;
    uint32_t staticRefsReused=0,staticRefsBuilt=0;
    uint32_t solverVelocityIterations=0;
    // Per-frame peak subdivision requests; bone indices refer to the actor's ragdoll definition.
    uint32_t ragdollRequested=0,ragdollRequestActor=0,ragdollRequestBone=0;
    uint32_t ragdollApplied=0,ragdollApplyActor=0,ragdollApplyBone=0;
    uint32_t paletteStagedCopies=0,paletteStagingPages=0,npcGroundFullScans=0;
    uint64_t npcGroundScannedTriangles=0;
    fqspacewarp::HistoryRejection motionRejection{};
    uint32_t worldRenderWidth=0,worldRenderHeight=0,matteDraws=0,compositeLandDraws=0;
    uint64_t vkActualCorners=0;std::array<uint64_t,3> geometrySaved{};
    uint32_t motionReplayPasses=0,motionReplayDraws=0;
    bool spacewarp=false,focused=false;
    double vkWorldUs=0,vkUiUs=0;
    std::array<double,VkPassCount> vkPassUs{};
    uint32_t vkSceneDraws=0,vkBufferBindsIssued=0,vkBufferBindsSkipped=0;
    uint32_t vkExpected=0,vkReceived=0,vkTimed=0,vkCpuTimed=0,vkTimingDropped=0;
    double vkRecordUs=0,vkUploadUs=0,vkDrawUs=0,vkQueueUs=0,vkReuseWaitUs=0,vkFinishWaitUs=0,vkXrWaitUs=0;
    double gpuEyesUs=-1,periodUs=0;
    double gpuEyeUs[2]{-1,-1};
    std::array<std::array<double,GpuPassCount>,2> gpuPassUs{};
    std::array<std::array<unsigned,GpuPassCount>,2> gpuExpected{},gpuReceived{};
    Frame(){for(auto& eye:gpuPassUs)eye.fill(-1);}

    uint32_t cell=0,world=0,residents=0,visible=0,animated=0,combat=0,parts=0,rigParts=0,paths=0,expanded=0,sceneSamples=1;
    uint64_t paletteBytes=0,vertices=0,draws=0,states=0,uniforms=0,textures=0,npcDraws=0,paletteSkipped=0,submittedVertices=0;
    uint64_t colorResolvePixels=0,depthResolvePixels=0;
    uint32_t reflectionPasses=0,waterSnapshots=0;
    uint32_t hdrRttEyes=0;
    uint64_t implicitColorResolvePixels=0;
    bool loading=false,blocked=false,rendered=false,submitted=false,streaming=false;
};
inline constexpr size_t Capacity=4096;
struct Session {
    std::array<Frame,Capacity> frames{};
    Frame current;
    char configuration[768]{};
    char graphics[768]{};
    size_t size=0;
    uint64_t id=0,serial=0;
    bool active=false,recording=false;
    Clock::time_point started{};
    void Start(uint64_t token) {configuration[0]=graphics[0]=0;size=0;id=token;active=true;recording=false;started=Clock::now();}
    void BeginFrame() {++serial;recording=active&&size<Capacity;if(recording){current={};current.serial=serial;}}
    void FinishFrame() {if(recording){current.elapsedUs=Us(started);frames[size++]=current;}recording=false;}
    Frame* Find(uint64_t token,uint64_t frame) {
        if(!active||token!=id)return nullptr;
        if(recording&&current.serial==frame)return &current;
        for(size_t i=size;i>0;--i){if(frames[i-1].serial==frame)return &frames[i-1];if(frames[i-1].serial<frame)break;}
        return nullptr;
    }
    void Gpu(uint64_t frame,unsigned eye,double us) {
        if(!active || eye>1)return;
        for(size_t i=size;i>0;--i) {
            if(frames[i-1].serial==frame){
                auto& f=frames[i-1];f.gpuEyeUs[eye]=us;
                if(f.gpuEyeUs[0]>=0&&f.gpuEyeUs[1]>=0)f.gpuEyesUs=f.gpuEyeUs[0]+f.gpuEyeUs[1];
                return;
            }
            if(frames[i-1].serial<frame)break;
        }
    }
    void GpuStage(uint64_t frame,unsigned eye,unsigned pass,double us) {
        if(pass==GpuEyeTotal){Gpu(frame,eye,us);return;}
        if(!active||eye>1||pass>=GpuPassCount)return;
        for(size_t i=size;i>0;--i){
            if(frames[i-1].serial==frame){
                ++frames[i-1].gpuReceived[eye][pass];
                auto& value=frames[i-1].gpuPassUs[eye][pass];
                value=value<0?us:value+us;return;
            }
            if(frames[i-1].serial<frame)break;
        }
    }
    bool Write(const char* path) const {
        FILE* f=std::fopen(path,"w");if(!f)return false;
        std::fprintf(f,"# format=351 gpuBackend=renderer-specific gpuSampleStride=8 gpuScopes=whole-passes shaderAttribution=unreliable-on-tilers session=%llu gpuEyesUs=GLES-only vkGpu=completed-submissions missing=-1 parent-scopes-include-children threadCpu=render-thread-only compositor-drops=unavailable %s %s\n",(unsigned long long)id,configuration,graphics);
        std::fprintf(f,"serial,loading,blocked,rendered,submitted,streaming,cell,world,periodUs,residents,visible,animated,combat,parts,rigParts,paths,expanded,paletteBytes,vertices,draws,states,uniforms,textures,npcDraws,paletteSkipped,gpuEyesUs,gpuEye0Us,gpuEye1Us");
        for(auto name:names)std::fprintf(f,",%s",name);
        std::fprintf(f,",sceneSamples,submittedVertices");
        for(unsigned eye=0;eye<2;++eye)for(auto name:gpuPassNames)std::fprintf(f,",gpu%u_%sUs",eye,name);
        std::fprintf(f,",reflectionPasses,waterSnapshots,colorResolvePixels,depthResolvePixels,hdrRttEyes,implicitColorResolvePixels");
        std::fprintf(f,",elapsedUs,threadCpuUs,spacewarp,focused,slowObjectUs,slowObjectRef,slowObjectMetric,vkSubmissions,vkCompleted,vkTimed,vkTimingDropped,vkWorldGpuUs,vkUiGpuUs");
        for(auto name:vkPassNames)std::fprintf(f,",vk_%sUs",name);
        std::fprintf(f,",vkRecordUs,vkUploadUs,vkDrawUs,vkQueueUs,vkReuseWaitUs,vkFinishWaitUs,vkXrWaitUs");
        for(auto name:names)std::fprintf(f,",%sCalls,%sMaxUs",name,name);
        std::fprintf(f,",ragdollRequested,ragdollRequestActor,ragdollRequestBone,ragdollApplied,ragdollApplyActor,ragdollApplyBone,paletteStagedCopies,paletteStagingPages,npcGroundFullScans,npcGroundScannedTriangles,motionReplayPasses,motionReplayDraws,motionRejectReason,motionRejectObject,motionRejectRef,motionRejectMesh,motionRejectObjects,motionRejectUsedBytes,motionRejectRecordBytes,staticRefsReused,staticRefsBuilt,rigidConstraintRemainderUs,solverVelocityIterations,vkSceneDraws,vkBufferBindsIssued,vkBufferBindsSkipped,scenerySampled,sceneryDetailedDraws,sceneryInstancedDraws,sceneryAlreadySaved,sceneryEligibleDraws,sceneryAdditionalMatches,movableBoundsUpdated,staticBoundsReused,ragdollRequestHeight,ragdollRequestVelocityY,vkActualCorners,lodScenerySaved,lodTerrainSaved,lodActorsSaved,worldRenderWidth,worldRenderHeight,matteDraws,compositeLandDraws\n");
        for(size_t i=0;i<size;++i) {
            const auto& v=frames[i];
            std::fprintf(f,"%llu,%d,%d,%d,%d,%d,%u,%u,%.3f,%u,%u,%u,%u,%u,%u,%u,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%.3f,%.3f,%.3f",
                (unsigned long long)v.serial,v.loading,v.blocked,v.rendered,v.submitted,v.streaming,v.cell,v.world,v.periodUs,
                v.residents,v.visible,v.animated,v.combat,v.parts,v.rigParts,v.paths,v.expanded,
                (unsigned long long)v.paletteBytes,(unsigned long long)v.vertices,(unsigned long long)v.draws,
                (unsigned long long)v.states,(unsigned long long)v.uniforms,(unsigned long long)v.textures,(unsigned long long)v.npcDraws,(unsigned long long)v.paletteSkipped,
                v.gpuEyesUs,v.gpuEyeUs[0],v.gpuEyeUs[1]);
            for(double us:v.us)std::fprintf(f,",%.3f",us);
            std::fprintf(f,",%u,%llu",v.sceneSamples,(unsigned long long)v.submittedVertices);
            for(unsigned eye=0;eye<2;++eye)for(unsigned pass=0;pass<GpuPassCount;++pass){
                const bool complete=v.gpuExpected[eye][pass]>0&&v.gpuExpected[eye][pass]==v.gpuReceived[eye][pass];
                std::fprintf(f,",%.3f",complete?v.gpuPassUs[eye][pass]:-1.0);
            }
            std::fprintf(f,",%u,%u,%llu,%llu,%u,%llu",v.reflectionPasses,v.waterSnapshots,(unsigned long long)v.colorResolvePixels,(unsigned long long)v.depthResolvePixels,v.hdrRttEyes,(unsigned long long)v.implicitColorResolvePixels);
            const bool vkComplete=v.vkExpected>0&&v.vkExpected==v.vkReceived;
            const bool vkPassComplete=vkComplete&&v.vkTimed==v.vkExpected&&!v.vkTimingDropped;
            std::fprintf(f,",%.3f,%.3f,%d,%d,%.3f,%u,%u,%u,%u,%u,%u,%.3f,%.3f",v.elapsedUs,v.threadCpuUs,v.spacewarp,v.focused,v.slowObjectUs,v.slowObjectRef,v.slowObjectMetric,v.vkExpected,v.vkReceived,v.vkTimed,v.vkTimingDropped,vkComplete?v.vkWorldUs:-1,vkComplete?v.vkUiUs:-1);
            for(double value:v.vkPassUs)std::fprintf(f,",%.3f",vkPassComplete?value:-1);
            const bool vkCpuComplete=v.vkExpected>0&&v.vkCpuTimed==v.vkExpected;
            std::fprintf(f,",%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f",v.vkExpected?v.vkRecordUs:-1,vkCpuComplete?v.vkUploadUs:-1,vkCpuComplete?v.vkDrawUs:-1,vkCpuComplete?v.vkQueueUs:-1,v.vkExpected?v.vkReuseWaitUs:-1,v.vkExpected?v.vkFinishWaitUs:-1,vkComplete?v.vkXrWaitUs:-1);
            for(unsigned m=0;m<Count;++m)std::fprintf(f,",%u,%.3f",v.calls[m],v.maxUs[m]);
            std::fprintf(f,",%u,%u,%u,%u,%u,%u,%u,%u,%u,%llu,%u,%u",v.ragdollRequested,v.ragdollRequestActor,v.ragdollRequestBone,
                v.ragdollApplied,v.ragdollApplyActor,v.ragdollApplyBone,v.paletteStagedCopies,v.paletteStagingPages,v.npcGroundFullScans,
                (unsigned long long)v.npcGroundScannedTriangles,v.motionReplayPasses,v.motionReplayDraws);
            const auto& reject=v.motionRejection;
            // Derive this per frame before rounding. It includes island grouping,
            // sorting, dispatch and timing overhead inside solveConstraints.
            const double remainder=v.us[RigidConstraints]-v.us[RigidGroupSetup]
                -v.us[RigidGroupIterations]-v.us[RigidGroupFinish];
            std::fprintf(f,",%s,%llu,%u,%u,%zu,%zu,%zu,%u,%u,%.3f,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%.4f,%.4f",reject.reason?reject.reason:"none",
                (unsigned long long)reject.identity,reject.reference,reject.mesh,reject.objects,reject.usedBytes,reject.recordBytes,
                v.staticRefsReused,v.staticRefsBuilt,remainder,v.solverVelocityIterations,v.vkSceneDraws,v.vkBufferBindsIssued,v.vkBufferBindsSkipped,
                v.scenerySampled,v.scenery.detailedDraws,v.scenery.instancedDraws,v.scenery.alreadySaved,
                v.scenery.eligibleDraws,v.scenery.additionalMatches,v.movableBoundsUpdated,v.staticBoundsReused,
                v.ragdollRequestHeight,v.ragdollRequestVelocityY);
            std::fprintf(f,",%llu,%llu,%llu,%llu,%u,%u,%u,%u\n",(unsigned long long)v.vkActualCorners,(unsigned long long)v.geometrySaved[0],
                (unsigned long long)v.geometrySaved[1],(unsigned long long)v.geometrySaved[2],
                v.worldRenderWidth,v.worldRenderHeight,v.matteDraws,v.compositeLandDraws);
        }
        const bool ok=std::ferror(f)==0;
        return std::fclose(f)==0&&ok;
    }
};
inline Session capture;
struct Scope {
    Metric metric;
    bool active=capture.recording;
    Clock::time_point start=active?Clock::now():Clock::time_point{};
    uint64_t token=capture.id,frame=capture.serial;
    uint32_t ref=0;
    explicit Scope(Metric m,uint32_t objectRef=0):metric(m),ref(objectRef){}
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
    void Finish() {
        if(active&&capture.recording&&token==capture.id&&frame==capture.serial){
            const double elapsed=Us(start);auto& v=capture.current;
            v.us[metric]+=elapsed;++v.calls[metric];
            if(elapsed>v.maxUs[metric])v.maxUs[metric]=elapsed;
            if(ref&&elapsed>v.slowObjectUs){v.slowObjectUs=elapsed;v.slowObjectRef=ref;v.slowObjectMetric=metric;}
        }
        active=false;
    }
    ~Scope(){Finish();}
};
struct FrameGuard {~FrameGuard(){capture.FinishFrame();}};
} // namespace fqperf
#define FQ_PERF_JOIN_(a,b) a##b
#define FQ_PERF_JOIN(a,b) FQ_PERF_JOIN_(a,b)
#define FQ_PERF_SCOPE(metric) fqperf::Scope FQ_PERF_JOIN(perfScope_,__LINE__)(fqperf::metric)
