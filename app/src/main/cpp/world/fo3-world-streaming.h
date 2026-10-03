#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace fo3world {

inline constexpr float CellSize = 4096.0f;
inline constexpr int ResidentRadius = 2;
inline constexpr int WarmRadius = 3;
inline constexpr int CacheRadius = 3;

struct WantedCell {
    int32_t x = 0;
    int32_t y = 0;
    bool prefetch = false;
};

struct ResidencyPlan {
    // At most 49 warm CELLs plus two seven-CELL strips, with no frame allocation.
    std::array<WantedCell, 63> cells{};
    size_t cellCount = 0u;
    float localX = 0.0f;
    float localY = 0.0f;
    float motionX = 0.0f;
    float motionY = 0.0f;
    bool east = false;
    bool west = false;
    bool north = false;
    bool south = false;

    const WantedCell* begin() const { return cells.data(); }
    const WantedCell* end() const { return cells.data() + cellCount; }
};

// Render-thread planner. It owns motion history; CELL payloads, workers and GL
// handles remain with the live streaming runtime. No ESM, Android or GL access.
class ResidencyPlanner {
public:
    void Reset();
    ResidencyPlan Update(float gameX, float gameY, int32_t gridX, int32_t gridY);

private:
    bool motionValid_ = false;
    float lastGameX_ = 0.0f;
    float lastGameY_ = 0.0f;
    float motionX_ = 0.0f;
    float motionY_ = 0.0f;
};

} // namespace fo3world
