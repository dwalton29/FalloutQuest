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
    bool fastMaterial=false;
    float scale=1.0f;
    Diagnostics() {
#ifndef NDEBUG
        char value[PROP_VALUE_MAX]{};
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
struct GpuTimer {
    struct Slot { GLuint id=0; bool pending=false, discard=false; uint64_t frame=0; };
    std::array<Slot,8> slots{};
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
    Average gpu;
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
            ++disjoints;gpu={};
            for(auto& slot:slots) if(slot.pending) slot.discard=true;
        }
        for(auto& slot:slots) {
            if(!slot.pending || frame<=slot.frame+2) continue;
            GLuint ready=0;available(slot.id,GL_QUERY_RESULT_AVAILABLE_EXT,&ready);
            if(!ready) continue; // Never read QUERY_RESULT until available.
            if(!slot.discard) {
                GLuint64 nanos=0;result(slot.id,GL_QUERY_RESULT_EXT,&nanos);
                gpu.Add(double(nanos)/1000.0);
            }
            slot.pending=false;slot.discard=false;
        }
    }
    void Begin(uint64_t frame) {
        if(!supported) return;
        for(size_t i=0;i<slots.size();++i) if(!slots[i].pending) {
            active=static_cast<int>(i);slots[i].frame=frame;
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
inline Average preparation, submission, drawCalls, stateChanges, uniformUploads,
    textureBinds, vaoBinds, stateQueries, vertices, visible;
inline double preparationUs=0;
inline uint64_t passes=0;
inline fqgl::Counters baseline;
inline void Begin(uint64_t frame) {
    timer.Poll(frame);preparationUs=0;baseline=fqgl::counters;timer.Begin(frame);
}
inline void End(double cpuUs,size_t count) {
    timer.End();
    preparation.Add(preparationUs);submission.Add(std::max(0.0,cpuUs-preparationUs));
    const auto& c=fqgl::counters;
    drawCalls.Add(c.draws-baseline.draws);stateChanges.Add(c.state-baseline.state);
    uniformUploads.Add(c.uniforms-baseline.uniforms);textureBinds.Add(c.textures-baseline.textures);
    vaoBinds.Add(c.vaos-baseline.vaos);stateQueries.Add(c.queries-baseline.queries);
    vertices.Add(c.vertices-baseline.vertices);visible.Add(count);++passes;
}
} // namespace fqopaque
