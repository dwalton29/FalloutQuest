#pragma once
#include "performance-capture.h"
#include "gpu-timestamp-timer.h"
#include <sys/system_properties.h>
#include <android/log.h>

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
inline void Control() {
#ifndef NDEBUG
    if(Clock::now()<nextControl)return;
    nextControl=Clock::now()+std::chrono::seconds(1);
    char value[PROP_VALUE_MAX]{};
    __system_property_get("debug.falloutquest.perf_capture",value);
    const uint64_t token=std::strtoull(value,nullptr,10);
    if(capture.active && (!token || token!=lastToken || capture.size==Capacity)) {
        capture.active=false;
        const bool ok=capture.Write("/data/user/0/com.falloutquest.app/files/performance.csv");
        __android_log_print(ANDROID_LOG_INFO,"FQPerf","CAPTURE_END id=%llu frames=%zu saved=%d gpuDisjoint=%llu gpuDropped=%llu gpuInvalid=%llu gpuValid=%llu gpuZero=%llu firstBadStart=%llu firstBadEnd=%llu",
            (unsigned long long)capture.id,capture.size,ok,(unsigned long long)gpu.disjoints,(unsigned long long)gpu.dropped,(unsigned long long)gpu.invalid,(unsigned long long)gpu.valid,(unsigned long long)gpu.zero,(unsigned long long)gpu.firstBadStart,(unsigned long long)gpu.firstBadEnd);
    }
    if(token && token!=lastToken) {
        capture.Start(token);
        gpu.Shutdown();gpu.Initialize();
        gpu.sample=[](uint64_t frame,unsigned eye,unsigned pass,double us){capture.GpuStage(frame,eye,pass,us);};
        __android_log_print(ANDROID_LOG_INFO,"FQPerf","CAPTURE_START id=%llu maxFrames=%zu gpuSupported=%d gpuBackend=timestamp-pairs timestampBits=%u sampleStride=8 compositorStats=unavailable",
            (unsigned long long)token,Capacity,gpu.supported,gpu.counterBits);
    }
    lastToken=token;
#endif
}
}
