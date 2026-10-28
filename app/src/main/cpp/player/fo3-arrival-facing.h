#pragma once
#include <cmath>
#include <cstdint>

namespace fo3arrival {
// Render-thread publication: only a completed CELL transaction changes facing.
struct Facing {
    uint64_t serial = 0;
    uint32_t destination = 0;
    float gameYaw = 0;
    void Publish(uint32_t door, float yaw) {
        if (!std::isfinite(yaw)) return;
        destination = door; gameYaw = yaw; ++serial;
    }
    bool Apply(uint64_t& consumed, float physicalYaw, float& virtualYaw) const {
        if (!serial || serial == consumed || !std::isfinite(physicalYaw)) return false;
        // FO3 +Y forward / clockwise heading -> XR -Z forward / CCW yaw.
        const float offset = -gameYaw - physicalYaw;
        virtualYaw = std::atan2(std::sin(offset), std::cos(offset));
        consumed = serial;
        return true;
    }
};
} // namespace fo3arrival
