#pragma once
#include "performance-capture.h"
#include "opaque-telemetry.h"
#include <sys/system_properties.h>
#include <android/log.h>

namespace fqperf {
inline fqopaque::GpuTimer gpu;
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
        __android_log_print(ANDROID_LOG_INFO,"FQPerf","CAPTURE_END id=%llu frames=%zu saved=%d gpuDisjoint=%llu gpuDropped=%llu",
            (unsigned long long)capture.id,capture.size,ok,(unsigned long long)gpu.disjoints,(unsigned long long)gpu.dropped);
    }
    if(token && token!=lastToken) {
        capture.Start(token);
        gpu.Shutdown();gpu.Initialize();
        gpu.sample=[](uint64_t frame,unsigned eye,unsigned,double us){capture.Gpu(frame,eye,us);};
        __android_log_print(ANDROID_LOG_INFO,"FQPerf","CAPTURE_START id=%llu maxFrames=%zu gpuSupported=%d compositorStats=unavailable",
            (unsigned long long)token,Capacity,gpu.supported);
    }
    lastToken=token;
#endif
}
}
