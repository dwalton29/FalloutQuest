#include "fo3-world-streaming.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <utility>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool HasCell(const fo3world::ResidencyPlan& plan, int x, int y) {
    return std::any_of(plan.begin(), plan.end(),
        [=](const fo3world::WantedCell& cell) { return cell.x == x && cell.y == y; });
}

void CheckBuffer(const fo3world::ResidencyPlan& plan, int x, int y) {
    std::set<std::pair<int, int>> unique;
    size_t resident = 0u;
    for (const auto& cell : plan) {
        Require(unique.emplace(cell.x, cell.y).second, "duplicate wanted CELL");
        if (!cell.prefetch) ++resident;
    }
    Require(resident == 25u, "normal residency must contain exactly 25 CELLs");
    for (int dy = -3; dy <= 3; ++dy)
        for (int dx = -3; dx <= 3; ++dx)
            Require(HasCell(plan, x + dx, y + dy), "warm buffer has a hole");
}
} // namespace

int main() {
    fo3world::ResidencyPlanner planner;
    auto first = planner.Update(2048.0f, 2048.0f, 0, 0);
    CheckBuffer(first, 0, 0);
    Require(first.cellCount == 49u, "first frame must have no lookahead");
    auto east = planner.Update(2052.0f, 2048.0f, 0, 0);
    CheckBuffer(east, 0, 0);
    Require(east.east && !east.west && east.cellCount == 56u,
            "east motion must add one seven-CELL strip");
    Require(HasCell(east, 4, -3) && HasCell(east, 4, 3), "east strip bounds");

    auto diagonal = planner.Update(2056.0f, 2052.0f, 0, 0);
    CheckBuffer(diagonal, 0, 0);
    Require(diagonal.east && diagonal.north && diagonal.cellCount == 63u,
            "diagonal motion must add two strips");
    Require(!HasCell(diagonal, 4, 4), "diagonal corner is outside the current runway");

    planner.Reset();
    auto transition = planner.Update(-4097.0f, -1.0f, -2, -1);
    CheckBuffer(transition, -2, -1);
    Require(transition.cellCount == 49u && transition.motionX == 0.0f &&
            transition.motionY == 0.0f, "reset must discard motion across transitions");
    auto southwest = planner.Update(-4101.0f, -5.0f, -2, -1);
    CheckBuffer(southwest, -2, -1);
    Require(!southwest.west && !southwest.south,
            "motion away from the relevant edge must not prefetch");

    planner.Reset();
    planner.Update(-6144.0f, -2048.0f, -2, -1);
    southwest = planner.Update(-6148.0f, -2052.0f, -2, -1);
    CheckBuffer(southwest, -2, -1);
    Require(southwest.west && southwest.south && southwest.cellCount == 63u,
            "negative grids must support west and south lookahead");
    Require(HasCell(southwest, -6, -1) && HasCell(southwest, -2, -5),
            "negative-grid strip coordinates");

    planner.Reset();
    planner.Update(1800.0f, 2048.0f, 0, 0);
    auto belowEdge = planner.Update(1804.0f, 2048.0f, 0, 0);
    Require(!belowEdge.east && belowEdge.cellCount == 49u,
            "east lookahead must wait for the existing 45-percent edge threshold");

    // Independent planners must never share player/scene motion history.
    fo3world::ResidencyPlanner other;
    auto independent = other.Update(3000.0f, 3000.0f, 0, 0);
    Require(independent.cellCount == 49u && independent.motionX == 0.0f,
            "planner instances leaked motion history");
    for (int frame = 0; frame < 1000; ++frame) {
        const float x = -8000.0f + frame * 17.0f;
        const float y = 6000.0f - frame * 13.0f;
        const int gx = static_cast<int>(std::floor(x / 4096.0f));
        const int gy = static_cast<int>(std::floor(y / 4096.0f));
        CheckBuffer(other.Update(x, y, gx, gy), gx, gy);
    }
    std::cout << "world streaming tests passed\n";
}
