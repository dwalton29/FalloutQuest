#pragma once

#include "fo3-reference-state.h"
#include <memory>
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
    // Call only in the final scene/collision swap, never on worker completion
    // or on a tentative script-state edit. Workers own the returned snapshot.
    void Publish(std::shared_ptr<const fo3world::ReferenceStateSnapshot> state) {
        state_ = std::move(state);
        if (++serial_ == 0) ++serial_;
    }
    PublicationStamp Stamp() const {
        return {serial_, state_ ? state_->revision : 0};
    }
    std::shared_ptr<const fo3world::ReferenceStateSnapshot> Snapshot() const { return state_; }
    bool Accepts(PublicationStamp stamp, uint64_t liveReferenceRevision) const {
        return serial_ != 0 && stamp == Stamp() &&
               stamp.references == liveReferenceRevision;
    }
private:
    uint64_t serial_ = 0;
    std::shared_ptr<const fo3world::ReferenceStateSnapshot> state_;
};
} // namespace fo3scene
