#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Fo3NifTransform {
    float translation[3]{0.0f, 0.0f, 0.0f};
    float rotation[9]{
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
    };
    float scale = 1.0f;
    bool valid = false;
};

struct Fo3MaterialChairMesh {
    std::vector<float> positions;    // xyz, transformed NIF model space
    std::vector<float> normals;      // xyz
    std::vector<float> tangents;     // xyz
    std::vector<float> bitangents;   // xyz
    std::vector<float> texcoords;    // uv pairs
    std::vector<uint32_t> indices;   // expanded non-degenerate triangle list

    std::string diffuseTexturePath;
    std::string normalTexturePath;

    float glossiness = 10.0f;
    float alpha = 1.0f;
    float emitMultiplier = 1.0f;

    Fo3NifTransform rootTransform;
    Fo3NifTransform shapeTransform;
};

bool LoadMegatonChairMaterialMesh(Fo3MaterialChairMesh& outMesh);
