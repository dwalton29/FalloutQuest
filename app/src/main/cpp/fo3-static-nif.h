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

// Loads every fully renderable NiTriStrips/NiTriShape geometry block in an
// arbitrary Fallout 3 NIF, preserving each shape as its own material draw.
// Unsupported shapes are skipped without discarding the rest of the model.
bool LoadFo3StaticNifMeshes(const std::string& modelPath,
                            std::vector<Fo3StaticNifMesh>& outMeshes);

// Compatibility helper used by earlier milestones: returns the first supported
// renderable shape from the same generalized loader.
bool LoadFo3StaticNif(const std::string& modelPath, Fo3StaticNifMesh& outMesh);
