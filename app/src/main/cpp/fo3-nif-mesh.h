#pragma once

#include <cstdint>
#include <vector>

struct Fo3StaticMesh {
    std::vector<float> positions;      // xyz triplets, in Fallout/Gamebryo units
    std::vector<uint32_t> indices;     // triangle list
};

bool LoadMegatonChairMesh(Fo3StaticMesh& outMesh);
