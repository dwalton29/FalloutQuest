#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace fqloading {
// Render-thread policy; CPU durations are deliberately not GPU completion.
// A separate asynchronous timer supplies gpuUs, fences bound queued work.
struct FrameBudget {
    uint64_t start=0, allowance=2000, spent=0, operations=0, frame=0;
    bool loading=false,gpuReady=true;
    void Begin(uint64_t now, uint64_t period, uint64_t previousCpu,
               uint64_t cadence, uint64_t gpuUs, bool visible) {
        gpuReady=true;start=now;spent=operations=0;++frame;loading=visible;
        period=std::max<uint64_t>(period,1000);
        if(previousCpu>period || cadence>period*3/2 || gpuUs>period/2)
            allowance=std::max<uint64_t>(500,allowance/2);
        else allowance=std::min<uint64_t>(3000,allowance+125);
        allowance=std::min(allowance,std::max<uint64_t>(500,period/4));
    }
    bool Allow(uint64_t now) const {
        if(!loading)return true;
        if(!gpuReady)return false;
        // One bounded operation can always make progress. Further work is
        // admitted only against both the elapsed deadline and cumulative cost.
        return operations==0 || (spent<allowance && now-start<allowance);
    }
    void Charge(uint64_t before,uint64_t after) {
        spent+=after>=before?after-before:0;++operations;
    }
};
inline FrameBudget budget;
constexpr size_t TransferBytes=256u*1024u;
inline size_t TransferRows(size_t width,size_t remaining) {
    if(!width || !remaining)return 0;
    return std::min(remaining,std::max<size_t>(1,TransferBytes/(width*4)));
}
} // namespace fqloading
