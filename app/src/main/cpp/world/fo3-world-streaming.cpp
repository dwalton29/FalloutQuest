#include "fo3-world-streaming.h"

#include <algorithm>
#include <cmath>

namespace fo3world {

void ResidencyPlanner::Reset() {
    motionValid_ = false;
    motionX_ = 0.0f;
    motionY_ = 0.0f;
}

ResidencyPlan ResidencyPlanner::Update(
        float gameX, float gameY, int32_t gridX, int32_t gridY) {
    ResidencyPlan plan;
    // The complete 7x7 warm buffer includes the normal 5x5 resident set.
    for (int dy = -WarmRadius; dy <= WarmRadius; ++dy) {
        for (int dx = -WarmRadius; dx <= WarmRadius; ++dx) {
            plan.cells[plan.cellCount++] = {gridX + dx, gridY + dy,
                std::max(std::abs(dx), std::abs(dy)) > ResidentRadius};
        }
    }

    if (motionValid_) {
        constexpr float blend = 0.25f;
        motionX_ = motionX_ * (1.0f - blend) + (gameX - lastGameX_) * blend;
        motionY_ = motionY_ * (1.0f - blend) + (gameY - lastGameY_) * blend;
    } else {
        motionValid_ = true;
        motionX_ = 0.0f;
        motionY_ = 0.0f;
    }
    plan.motionX = motionX_;
    plan.motionY = motionY_;
    plan.localX = gameX - static_cast<float>(gridX) * CellSize;
    plan.localY = gameY - static_cast<float>(gridY) * CellSize;
    constexpr float motionEpsilon = 0.20f;
    constexpr float edgeFraction = 0.45f;
    plan.east = motionX_ > motionEpsilon && plan.localX > CellSize * edgeFraction;
    plan.west = motionX_ < -motionEpsilon && plan.localX < CellSize * (1.0f - edgeFraction);
    plan.north = motionY_ > motionEpsilon && plan.localY > CellSize * edgeFraction;
    plan.south = motionY_ < -motionEpsilon && plan.localY < CellSize * (1.0f - edgeFraction);

    // One seven-CELL strip per axis, without adding the diagonal corner.
    if (plan.east || plan.west) {
        const int stripX = gridX + (plan.east ? WarmRadius + 1 : -WarmRadius - 1);
        for (int dy = -WarmRadius; dy <= WarmRadius; ++dy)
            plan.cells[plan.cellCount++] = {stripX, gridY + dy, true};
    }
    if (plan.north || plan.south) {
        const int stripY = gridY + (plan.north ? WarmRadius + 1 : -WarmRadius - 1);
        for (int dx = -WarmRadius; dx <= WarmRadius; ++dx)
            plan.cells[plan.cellCount++] = {gridX + dx, stripY, true};
    }

    lastGameX_ = gameX;
    lastGameY_ = gameY;
    return plan;
}

} // namespace fo3world
