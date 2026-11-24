#pragma once
#include "performance-capture.h"
#include "gpu-timestamp-timer.h"
#include "physics/fo3-rigid-world.h"
#include <sys/system_properties.h>
#include <android/log.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace fqperf {
inline fqgpu::TimestampTimer gpu;
inline bool SampleGpuFrame(){return capture.recording&&capture.serial%8==0;}
struct GpuScope {
    int index=-1;
    explicit GpuScope(unsigned pass){if(SampleGpuFrame()){
        if(pass<GpuPassCount)++capture.current.gpuExpected[fqopaque::eye][pass];
        index=gpu.Begin(capture.serial,fqopaque::eye,pass);
    }}
    GpuScope(const GpuScope&)=delete;
    GpuScope& operator=(const GpuScope&)=delete;
    void Finish(){gpu.End(index);index=-1;}
    ~GpuScope(){Finish();}
};
// These envelopes span actual framebuffer work, including its completion at
// the resolve boundary. They are not exclusive per-shader measurements.
inline int hdrIndex=-1;
inline void EndHdrPass(){gpu.End(hdrIndex);hdrIndex=-1;}
inline void BeginHdrSegment(unsigned pass){
    EndHdrPass();
    if(SampleGpuFrame()){
        ++capture.current.gpuExpected[fqopaque::eye][pass];
        hdrIndex=gpu.Begin(capture.serial,fqopaque::eye,pass);
    }
}
inline void BeginHdrPass(){BeginHdrSegment(GpuHdrFirst);}
inline void ContinueHdrPass(){BeginHdrSegment(GpuHdrContinuation);}
inline uint64_t lastToken=0;
inline Clock::time_point nextControl{};
inline constexpr const char* CapturePath="/data/user/0/com.falloutquest.app/files/performance.csv";
inline constexpr const char* CaptureTempPath="/data/user/0/com.falloutquest.app/files/performance.csv.tmp";
inline void Status(const char* state,bool saved=false,int error=0,const char* reason="none") {
    // Pollable even after verbose logcat has wrapped. Called between frames.
    const char* temp="/data/user/0/com.falloutquest.app/files/performance-status.txt.tmp";
    FILE* f=std::fopen(temp,"w");if(!f)return;
    std::fprintf(f,"format=351\nsession=%llu\nstate=%s\nframes=%zu\nsaved=%d\nerror=%d\nreason=%s\nelapsedSeconds=%.3f\n",
        (unsigned long long)capture.id,state,capture.size,saved,error,reason,Us(capture.started)/1e6);
    const bool ok=!std::ferror(f);
    if(std::fclose(f)==0&&ok)std::rename(temp,"/data/user/0/com.falloutquest.app/files/performance-status.txt");
}
inline void Complete(const char* reason) {
    if(!capture.active)return;
    capture.recording=false;
    // Do not wait for outstanding GPU work. Pending samples export as -1.
    errno=0;
    const bool written=capture.Write(CaptureTempPath);
    const bool ok=written&&std::rename(CaptureTempPath,CapturePath)==0;
    const int error=ok?0:errno?errno:EIO;
    Status("complete",ok,error,reason);
    capture.active=false;
    fo3rigid::Live().UseBaselineIterations(false);
    reuseVulkanBufferBinds=true;
    __android_log_print(ANDROID_LOG_INFO,"FQPerf","CAPTURE_END id=%llu frames=%zu saved=%d error=%d reason=%s gpuDisjoint=%llu gpuDropped=%llu",
        (unsigned long long)capture.id,capture.size,ok,error,reason,(unsigned long long)gpu.disjoints,(unsigned long long)gpu.dropped);
}
inline void Control() {
#ifndef NDEBUG
    if(Clock::now()<nextControl)return;
    nextControl=Clock::now()+std::chrono::seconds(1);
    char value[PROP_VALUE_MAX]{};
    __system_property_get("debug.falloutquest.perf_capture",value);
    char* suffix=nullptr;
    const uint64_t token=std::strtoull(value,&suffix,10);
    if(capture.active && (!token || token!=lastToken || capture.size==Capacity || Us(capture.started)>120e6))
        Complete(capture.size==Capacity?"capacity":Us(capture.started)>120e6?"timeout":token?"replaced":"requested");
    if(token && token!=lastToken) {
        // A plain token uses the gameplay default. The comparison applies only to this capture.
        char* binds=std::strstr(suffix,":binds");
        reuseVulkanBufferBinds=!binds||std::strcmp(binds,":binds0")!=0;
        if(binds)*binds='\0';
        fo3rigid::Live().UseBaselineIterations(std::strcmp(suffix,":20")==0);
        capture.Start(token);
        gpu.Shutdown();gpu.Initialize();
        gpu.sample=[](uint64_t frame,unsigned eye,unsigned pass,double us){capture.GpuStage(frame,eye,pass,us);};
        __android_log_print(ANDROID_LOG_INFO,"FQPerf","CAPTURE_START id=%llu maxFrames=%zu gpuSupported=%d gpuBackend=timestamp-pairs timestampBits=%u sampleStride=8 compositorStats=unavailable",
            (unsigned long long)token,Capacity,gpu.supported,gpu.counterBits);
        Status("recording");
    }
    else if(capture.active)Status("recording");
    lastToken=token;
#endif
}
}
