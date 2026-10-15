#pragma once

namespace fo3scene {
// Sampling continues during a reservation, but held gameplay controls must be
// released before a later frame can interpret them as a fresh action.
class ResidentInputBarrier {
public:
    bool Allow(bool blocked, bool neutral) noexcept {
        if(blocked){awaitNeutral_=true;return false;}
        if(awaitNeutral_){if(neutral)awaitNeutral_=false;return false;}
        return true;
    }
private:
    bool awaitNeutral_=false;
};
} // namespace fo3scene
