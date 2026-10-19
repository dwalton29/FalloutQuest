#pragma once
// Render-thread owned, bounded session capture. No allocation, locks or file I/O
// during recorded frames. Parent scopes include children; never sum them blindly.
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>

namespace fqperf {
using Clock=std::chrono::steady_clock;
inline double Us(Clock::time_point t) {return std::chrono::duration<double,std::micro>(Clock::now()-t).count();}
enum Metric : unsigned {
    Wait,Begin,Input,Gameplay,Streaming,ActorPrep,Simulation,Packages,Persistence,
    Unloaded,Arrivals,Animation,Facial,Palette,Upload,Target,Sight,Path,Movement,
    WeaponChoice,Eye0,Eye1,End,Work,Cadence,ScenePrep,Clip,Pose,
    Native0,Native1,Detail0,Detail1,NpcDraw0,NpcDraw1,PlayerDraw0,PlayerDraw1,Swapchain,Scripts,Motion,Interaction,NpcGround,NpcPortal,NpcObstruction,NpcCrowd,NpcArrivalPart,Count
};
inline constexpr const char* names[]={"xrWaitUs","xrBeginUs","inputTrackingUs","gameplayUs",
    "streamingUs","actorPrepUs","npcSimulationUs","npcPackagesUs","npcPersistenceUs",
    "npcUnloadedUs","npcArrivalsUs","npcAnimationUs","npcFacialUs","npcPaletteUs",
    "npcUploadUs","npcTargetUs","npcSightUs","npcPathUs","npcMovementUs",
    "npcWeaponChoiceUs","eye0CpuUs","eye1CpuUs","xrEndUs","workUs","cadenceUs",
    "scenePrepUs","npcClipUs","npcPoseUs","native0CpuUs","native1CpuUs",
    "detail0CpuUs","detail1CpuUs","npcDraw0CpuUs","npcDraw1CpuUs","playerDraw0CpuUs","playerDraw1CpuUs","swapchainAcquireWaitUs","scriptsUs","playerMotionUs","interactionUs",
    "npcGroundUs","npcPortalUs","npcObstructionUs","npcCrowdUs","npcArrivalPartUs"};
static_assert(sizeof(names)/sizeof(*names)==Count);
struct Frame {
    uint64_t serial=0;
    std::array<double,Count> us{};
    double gpuEyesUs=-1,periodUs=0;
    double gpuEyeUs[2]{-1,-1};
    uint32_t cell=0,world=0,residents=0,visible=0,animated=0,combat=0,parts=0,rigParts=0,paths=0,expanded=0;
    uint64_t paletteBytes=0,vertices=0,draws=0,states=0,uniforms=0,textures=0,npcDraws=0,paletteSkipped=0;
    bool loading=false,blocked=false,rendered=false,submitted=false,streaming=false;
};
inline constexpr size_t Capacity=4096;
struct Session {
    std::array<Frame,Capacity> frames{};
    Frame current;
    char configuration[512]{};
    size_t size=0;
    uint64_t id=0,serial=0;
    bool active=false,recording=false;
    void Start(uint64_t token) {configuration[0]=0;size=0;id=token;active=true;recording=false;}
    void BeginFrame() {++serial;recording=active&&size<Capacity;if(recording){current={};current.serial=serial;}}
    void FinishFrame() {if(recording)frames[size++]=current;recording=false;}
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
    bool Write(const char* path) const {
        FILE* f=std::fopen(path,"w");if(!f)return false;
        std::fprintf(f,"# v226 session=%llu gpuEyesUs=delayed-both-eyes-excludes-preparation missing=-1 parent-scopes-include-children compositor-drops=unavailable %s\n",(unsigned long long)id,configuration);
        std::fprintf(f,"serial,loading,blocked,rendered,submitted,streaming,cell,world,periodUs,residents,visible,animated,combat,parts,rigParts,paths,expanded,paletteBytes,vertices,draws,states,uniforms,textures,npcDraws,paletteSkipped,gpuEyesUs,gpuEye0Us,gpuEye1Us");
        for(auto name:names)std::fprintf(f,",%s",name);
        std::fputc('\n',f);
        for(size_t i=0;i<size;++i) {
            const auto& v=frames[i];
            std::fprintf(f,"%llu,%d,%d,%d,%d,%d,%u,%u,%.3f,%u,%u,%u,%u,%u,%u,%u,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%.3f,%.3f,%.3f",
                (unsigned long long)v.serial,v.loading,v.blocked,v.rendered,v.submitted,v.streaming,v.cell,v.world,v.periodUs,
                v.residents,v.visible,v.animated,v.combat,v.parts,v.rigParts,v.paths,v.expanded,
                (unsigned long long)v.paletteBytes,(unsigned long long)v.vertices,(unsigned long long)v.draws,
                (unsigned long long)v.states,(unsigned long long)v.uniforms,(unsigned long long)v.textures,(unsigned long long)v.npcDraws,(unsigned long long)v.paletteSkipped,
                v.gpuEyesUs,v.gpuEyeUs[0],v.gpuEyeUs[1]);
            for(double us:v.us)std::fprintf(f,",%.3f",us);
            std::fputc('\n',f);
        }
        return std::fclose(f)==0;
    }
};
inline Session capture;
struct Scope {
    Metric metric;
    bool active=capture.recording;
    Clock::time_point start=active?Clock::now():Clock::time_point{};
    explicit Scope(Metric m):metric(m){}
    ~Scope(){if(active&&capture.recording)capture.current.us[metric]+=Us(start);}
};
} // namespace fqperf
#define FQ_PERF_JOIN_(a,b) a##b
#define FQ_PERF_JOIN(a,b) FQ_PERF_JOIN_(a,b)
#define FQ_PERF_SCOPE(metric) fqperf::Scope FQ_PERF_JOIN(perfScope_,__LINE__)(fqperf::metric)
