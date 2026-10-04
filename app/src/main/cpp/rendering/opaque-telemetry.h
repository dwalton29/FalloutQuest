#pragma once
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <sys/system_properties.h>
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include "gl-state-cache.h"

namespace fqopaque {
using Clock=std::chrono::steady_clock;
inline double Micros(Clock::time_point start) {
    return std::chrono::duration<double,std::micro>(Clock::now()-start).count();
}
struct Diagnostics {
    bool fastMaterial=false, lodRadius20=false, lodMinimal=false, lodClipBypass=false;
    float scale=1.0f;
    Diagnostics() {
#ifndef NDEBUG
        char value[PROP_VALUE_MAX]{};
        __system_property_get("debug.falloutquest.lod_radius20",value);
        lodRadius20=std::atoi(value)==1;
        __system_property_get("debug.falloutquest.lod_minimal",value);
        lodMinimal=std::atoi(value)==1;
        __system_property_get("debug.falloutquest.lod_clip_bypass",value);
        lodClipBypass=std::atoi(value)==1;
        __system_property_get("debug.falloutquest.fast_material",value);
        fastMaterial=std::atoi(value)==1;
        __system_property_get("debug.falloutquest.render_scale",value);
        // Explicit opt-in 0.7 diagnostic; default and production remain 1.0.
        if(std::atof(value)>0.69 && std::atof(value)<0.71) scale=0.7f;
#endif
    }
};
inline const Diagnostics& Options() { static const Diagnostics options;return options; }
struct Average {
    std::array<double,120> samples{};
    unsigned count=0, next=0;
    double sum=0;
    void Add(double v) { sum-=samples[next];samples[next]=v;sum+=v;next=(next+1)%samples.size();if(count<samples.size()) ++count; }
    double Mean() const { return count ? sum/count : 0; }
};
enum Phase : unsigned { NativeLod, DetailedWorld, Npc, Player, PhaseCount };
struct PhaseStats { Average cpu, draws, vertices; double lastCpu=0; uint64_t lastDraws=0,lastVertices=0; };
inline std::array<std::array<PhaseStats,PhaseCount>,2> phases;
inline Average totalCpu;
struct NativeWork {
    uint64_t level32=0, level16=0, level8=0, level4Terrain=0, level4Objects=0, high=0;
    uint64_t resident=0, considered=0, windowRejected=0, refinementRejected=0,
        frustumRejectedBlocks=0, frustumRejectedShapes=0, eligibleShapes=0;
};
inline NativeWork nativeWork;
inline bool collectingNative=false, nativeObjectMaterial=false;
inline unsigned eye=0;
struct DetailedWork { size_t visible=0, instanced=0, instancedDraws=0, fallbackDraws=0; };
inline DetailedWork detailedWork;
struct GpuTimer {
    struct Slot { GLuint id=0; bool pending=false, discard=false; uint64_t frame=0; unsigned phase=0, eye=0; };
    std::array<Slot,32> slots{};
    PFNGLGENQUERIESEXTPROC gen=nullptr;
    PFNGLDELETEQUERIESEXTPROC del=nullptr;
    PFNGLBEGINQUERYEXTPROC begin=nullptr;
    PFNGLENDQUERYEXTPROC end=nullptr;
    PFNGLGETQUERYOBJECTUIVEXTPROC available=nullptr;
    PFNGLGETQUERYOBJECTUI64VEXTPROC result=nullptr;
    PFNGLGETQUERYIVEXTPROC bits=nullptr;
    bool initialized=false, supported=false;
    int active=-1;
    uint64_t disjoints=0, dropped=0;
    Average gpu; // Legacy aggregate of individual query samples, not total opaque time.
    std::array<Average,PhaseCount> phaseGpu;
    std::array<std::array<Average,PhaseCount>,2> eyeGpu;
    void Initialize() {
        if(initialized) return;
        initialized=true;
        const char* extensions=reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if(!extensions || !std::strstr(extensions,"GL_EXT_disjoint_timer_query")) return;
        gen=reinterpret_cast<PFNGLGENQUERIESEXTPROC>(eglGetProcAddress("glGenQueriesEXT"));
        del=reinterpret_cast<PFNGLDELETEQUERIESEXTPROC>(eglGetProcAddress("glDeleteQueriesEXT"));
        begin=reinterpret_cast<PFNGLBEGINQUERYEXTPROC>(eglGetProcAddress("glBeginQueryEXT"));
        end=reinterpret_cast<PFNGLENDQUERYEXTPROC>(eglGetProcAddress("glEndQueryEXT"));
        available=reinterpret_cast<PFNGLGETQUERYOBJECTUIVEXTPROC>(eglGetProcAddress("glGetQueryObjectuivEXT"));
        result=reinterpret_cast<PFNGLGETQUERYOBJECTUI64VEXTPROC>(eglGetProcAddress("glGetQueryObjectui64vEXT"));
        bits=reinterpret_cast<PFNGLGETQUERYIVEXTPROC>(eglGetProcAddress("glGetQueryivEXT"));
        if(!gen || !del || !begin || !end || !available || !result || !bits) return;
        GLint counterBits=0;bits(GL_TIME_ELAPSED_EXT,GL_QUERY_COUNTER_BITS_EXT,&counterBits);
        if(!counterBits) return;
        supported=true; for(auto& slot:slots) gen(1,&slot.id);
    }
    void Poll(uint64_t frame) {
        Initialize(); if(!supported) return;
        GLint disjoint=0;glGetIntegerv(GL_GPU_DISJOINT_EXT,&disjoint);
        if(disjoint) {
            ++disjoints;gpu={};phaseGpu={};eyeGpu={};
            for(auto& slot:slots) if(slot.pending) slot.discard=true;
        }
        for(auto& slot:slots) {
            if(!slot.pending || frame<=slot.frame+2) continue;
            GLuint ready=0;available(slot.id,GL_QUERY_RESULT_AVAILABLE_EXT,&ready);
            if(!ready) continue; // Never read QUERY_RESULT until available.
            if(!slot.discard) {
                GLuint64 nanos=0;result(slot.id,GL_QUERY_RESULT_EXT,&nanos);
                gpu.Add(double(nanos)/1000.0);
                phaseGpu[slot.phase].Add(double(nanos)/1000.0);
                eyeGpu[slot.eye][slot.phase].Add(double(nanos)/1000.0);
            }
            slot.pending=false;slot.discard=false;
        }
    }
    void Begin(uint64_t frame, unsigned phase=NativeLod) {
        if(!supported) return;
        for(size_t i=0;i<slots.size();++i) if(!slots[i].pending) {
            active=static_cast<int>(i);slots[i].frame=frame;slots[i].phase=phase;slots[i].eye=eye;
            begin(GL_TIME_ELAPSED_EXT,slots[i].id);return;
        }
        ++dropped;
    }
    void End() {
        if(active<0) return;
        end(GL_TIME_ELAPSED_EXT);slots[active].pending=true;active=-1;
    }
    void Shutdown() {
        if(active>=0) End();
        if(supported) for(auto& slot:slots) del(1,&slot.id);
        *this=GpuTimer{};
    }
};
inline GpuTimer timer;
inline Average preparation, submission, otherCpu, drawCalls, stateChanges, uniformUploads,
    textureBinds, vaoBinds, stateQueries, vertices, visible;
inline double preparationUs=0, submissionUs=0;
inline bool measuring=false;
struct SubmissionScope {
    bool active=measuring;
    Clock::time_point started=active ? Clock::now() : Clock::time_point{};
    ~SubmissionScope() { if(active) submissionUs+=Micros(started); }
};
struct PhaseScope {
    unsigned phase;
    Clock::time_point started;
    fqgl::Counters before;
    PhaseScope(unsigned p, uint64_t frame):phase(p),started(Clock::now()),before(fqgl::counters) {
        timer.Begin(frame,p);
    }
    ~PhaseScope() {
        timer.End();
        auto& stats=phases[eye][phase];
        stats.lastCpu=Micros(started);
        stats.lastDraws=fqgl::counters.draws-before.draws;
        stats.lastVertices=fqgl::counters.vertices-before.vertices;
        stats.cpu.Add(stats.lastCpu);
        stats.draws.Add(stats.lastDraws);
        stats.vertices.Add(stats.lastVertices);
    }
};
inline uint64_t passes=0;
inline fqgl::Counters baseline;
inline void Begin(uint64_t frame) {
    timer.Poll(frame);preparationUs=0;submissionUs=0;measuring=true;
    baseline=fqgl::counters;
}
inline void End(double cpuUs,size_t count) {
    measuring=false;
    totalCpu.Add(cpuUs);
    preparation.Add(preparationUs);submission.Add(submissionUs);
    otherCpu.Add(std::max(0.0,cpuUs-preparationUs-submissionUs));
    const auto& c=fqgl::counters;
    drawCalls.Add(c.draws-baseline.draws);stateChanges.Add(c.state-baseline.state);
    uniformUploads.Add(c.uniforms-baseline.uniforms);textureBinds.Add(c.textures-baseline.textures);
    vaoBinds.Add(c.vaos-baseline.vaos);stateQueries.Add(c.queries-baseline.queries);
    vertices.Add(c.vertices-baseline.vertices);visible.Add(count);++passes;
}
} // namespace fqopaque
