#pragma once
#include <cstdint>
#include <limits>

namespace fo3scene {
// Render-thread only, one instance per raw clock domain. Keeps absolute clock
// units/epoch on the normal path; paused intervals never enter gameplay time.
class ResidentPauseClock {
public:
    int64_t Sample(int64_t raw,bool blocked) noexcept {
        if(raw<=0){paused_=true;return value_;}
        if(!valid_){raw_=value_=raw;valid_=true;paused_=blocked;return value_;}
        // A discontinuity starts a new baseline without rewinding deadlines.
        if(raw>=raw_&&!paused_){
            const auto delta=raw-raw_;
            const auto room=std::numeric_limits<int64_t>::max()-value_;
            value_+=delta>room?room:delta;
        }
        raw_=raw;paused_=blocked;return value_;
    }
    // XR's latest sample may precede acquisition. Latch immediately so even a
    // reservation completed between submitted frames suppresses that interval.
    void Pause() noexcept {paused_=true;}
    bool Paused() const noexcept {return paused_;}
private:
    int64_t raw_=0,value_=0;
    bool valid_=false,paused_=false;
};
}
