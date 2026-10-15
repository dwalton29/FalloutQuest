#pragma once

#include "fo3-reference-state.h"
#include <memory>
#include <limits>
#include <utility>

namespace fo3scene {
// Render-thread publication identity. Context/origin alone cannot distinguish
// two successive loads of the same CELL, nor two reference-state projections.
struct PublicationStamp {
    uint64_t scene = 0;
    uint64_t references = 0;
    bool operator==(const PublicationStamp& other) const {
        return scene == other.scene && references == other.references;
    }
};

class ReferencePublication {
public:
    explicit ReferencePublication(uint64_t initial=0):serial_(initial){}
    ReferencePublication(const ReferencePublication&)=delete;
    ReferencePublication& operator=(const ReferencePublication&)=delete;
    bool CanAdvance(uint64_t count) const noexcept {
        return !exhausted_&&count<=std::numeric_limits<uint64_t>::max()-serial_;
    }
    // Call only in the final scene/collision swap, never on worker completion
    // or on a tentative script-state edit. Workers own the returned snapshot.
    void Publish(std::shared_ptr<const fo3world::ReferenceStateSnapshot> state) noexcept {
        state_ = std::move(state);
        // Never recycle a scene identity. Legacy publication still replaces
        // its snapshot at exhaustion, but no worker may accept that epoch.
        if(!CanAdvance(1))exhausted_=true;
        else ++serial_;
    }
    PublicationStamp Stamp() const {
        return {exhausted_?0:serial_, state_ ? state_->revision : 0};
    }
    std::shared_ptr<const fo3world::ReferenceStateSnapshot> Snapshot() const { return state_; }
    bool Accepts(PublicationStamp stamp, uint64_t liveReferenceRevision) const {
        return !exhausted_ && serial_ != 0 && stamp == Stamp() &&
               stamp.references == liveReferenceRevision;
    }
private:
    friend class ReferenceExchange;
    std::shared_ptr<const uint8_t> identity_=std::make_shared<const uint8_t>(0);
    uint64_t serial_ = 0;
    bool exhausted_=false;
    std::shared_ptr<const fo3world::ReferenceStateSnapshot> state_;
};
} // namespace fo3scene
