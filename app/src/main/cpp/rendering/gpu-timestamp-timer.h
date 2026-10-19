#pragma once
#include "opaque-telemetry.h"

// Paired timestamps do not occupy GL_TIME_ELAPSED_EXT. Independently nested
// owners (eye total, reflection and individual passes) therefore cannot collide.
namespace fqgpu {
inline bool Elapsed(uint64_t start,uint64_t finish,unsigned bits,double& us) {
    if(bits<30 || bits>64)return false;
    const uint64_t mask=bits==64?~uint64_t{0}:(uint64_t{1}<<bits)-1;
    if((start&~mask)||(finish&~mask))return false;
    const uint64_t delta=(finish-start)&mask;
    // Reject backwards/corrupt results; do not turn wrapped negatives positive.
    // At 30 bits the half-period is 536 ms. Long loading stalls remain missing.
    if(delta>(mask>>1)||delta>1000000000ull)return false;
    us=double(delta)/1000.0;return true;
}
struct TimestampTimer {
    struct Slot {GLuint id[2]{};bool used=false,pending=false,discard=false;uint64_t frame=0;unsigned eye=0,phase=0;};
    std::array<Slot,96> slots{};
    PFNGLGENQUERIESEXTPROC gen=nullptr;
    PFNGLDELETEQUERIESEXTPROC del=nullptr;
    PFNGLQUERYCOUNTEREXTPROC counter=nullptr;
    PFNGLGETQUERYOBJECTUIVEXTPROC available=nullptr;
    PFNGLGETQUERYOBJECTUI64VEXTPROC result=nullptr;
    PFNGLGETQUERYIVEXTPROC query=nullptr;
    bool initialized=false,supported=false;
    unsigned counterBits=0;
    uint64_t valid=0,invalid=0,dropped=0,disjoints=0,epoch=0,zero=0,firstBadStart=0,firstBadEnd=0;
    void (*sample)(uint64_t,unsigned,unsigned,double)=nullptr;
    void Initialize() {
        if(initialized)return;
        initialized=true;epoch=fqopaque::gpuDisjointEpoch;
        const auto* ext=reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if(!ext||!std::strstr(ext,"GL_EXT_disjoint_timer_query"))return;
        gen=reinterpret_cast<PFNGLGENQUERIESEXTPROC>(eglGetProcAddress("glGenQueriesEXT"));
        del=reinterpret_cast<PFNGLDELETEQUERIESEXTPROC>(eglGetProcAddress("glDeleteQueriesEXT"));
        counter=reinterpret_cast<PFNGLQUERYCOUNTEREXTPROC>(eglGetProcAddress("glQueryCounterEXT"));
        available=reinterpret_cast<PFNGLGETQUERYOBJECTUIVEXTPROC>(eglGetProcAddress("glGetQueryObjectuivEXT"));
        result=reinterpret_cast<PFNGLGETQUERYOBJECTUI64VEXTPROC>(eglGetProcAddress("glGetQueryObjectui64vEXT"));
        query=reinterpret_cast<PFNGLGETQUERYIVEXTPROC>(eglGetProcAddress("glGetQueryivEXT"));
        if(!gen||!del||!counter||!available||!result||!query)return;
        GLint bits=0;query(GL_TIMESTAMP_EXT,GL_QUERY_COUNTER_BITS_EXT,&bits);
        if(bits<30||bits>64)return;
        counterBits=unsigned(bits);supported=true;
        for(auto& s:slots)gen(2,s.id);
    }
    int Begin(uint64_t frame,unsigned eye,unsigned phase) {
        if(!supported)return -1;
        for(size_t i=0;i<slots.size();++i)if(!slots[i].used){
            auto& s=slots[i];s.used=true;s.pending=false;s.discard=false;
            s.frame=frame;s.eye=eye;s.phase=phase;counter(s.id[0],GL_TIMESTAMP_EXT);return int(i);
        }
        ++dropped;return -1;
    }
    void End(int index) {
        if(index<0)return;
        auto& s=slots[size_t(index)];
        if(!s.used||s.pending)return;
        counter(s.id[1],GL_TIMESTAMP_EXT);s.pending=true;
    }
    void Poll(uint64_t frame) {
        if(!supported)return;
        GLint disjoint=0;glGetIntegerv(GL_GPU_DISJOINT_EXT,&disjoint);
        if(disjoint)++fqopaque::gpuDisjointEpoch;
        if(epoch!=fqopaque::gpuDisjointEpoch){
            epoch=fqopaque::gpuDisjointEpoch;++disjoints;
            for(auto& s:slots)if(s.used)s.discard=true;
        }
        for(auto& s:slots){
            if(!s.pending||frame<=s.frame+2)continue;
            GLuint first=0,last=0;
            available(s.id[0],GL_QUERY_RESULT_AVAILABLE_EXT,&first);
            available(s.id[1],GL_QUERY_RESULT_AVAILABLE_EXT,&last);
            if(!first||!last)continue;
            if(!s.discard){
                GLuint64 start=0,finish=0;result(s.id[0],GL_QUERY_RESULT_EXT,&start);result(s.id[1],GL_QUERY_RESULT_EXT,&finish);
                double us=0;
                if(!Elapsed(start,finish,counterBits,us)){if(!invalid){firstBadStart=start;firstBadEnd=finish;}++invalid;}
                else {++valid;if(us==0)++zero;if(sample)sample(s.frame,s.eye,s.phase,us);}
            }
            s.used=s.pending=s.discard=false;
        }
    }
    void Shutdown(){
        if(supported)for(auto& s:slots)del(2,s.id);
        *this=TimestampTimer{};
    }
};
}
