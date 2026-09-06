#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Fo3TexturedChairMesh {
    std::vector<float> positions;   // xyz, Fallout/Gamebryo units
    std::vector<float> texcoords;   // uv pairs
    std::vector<uint32_t> indices;  // expanded triangle list
    std::string diffuseTexturePath;
};

bool LoadMegatonChairTexturedMesh(Fo3TexturedChairMesh& outMesh);
