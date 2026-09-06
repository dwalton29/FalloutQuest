#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Fo3StaticNifMesh {
    std::vector<float> positions;    // xyz in NIF model space after NIF transforms
    std::vector<float> normals;      // xyz
    std::vector<float> tangents;     // xyz
    std::vector<float> bitangents;   // xyz
    std::vector<float> texcoords;    // uv
    std::vector<uint32_t> indices;   // non-degenerate GL_TRIANGLES list

    std::string modelPath;
    std::string diffuseTexturePath;
    std::string normalTexturePath;
    float glossiness = 10.0f;
    float alpha = 1.0f;
};

// Loads the first fully renderable static geometry in an arbitrary FO3 NIF.
// Q6A deliberately targets ordinary static REFR clutter/props; unsupported
// geometry is skipped so the ESM-driven scene can select another real object.
bool LoadFo3StaticNif(const std::string& modelPath, Fo3StaticNifMesh& outMesh);
