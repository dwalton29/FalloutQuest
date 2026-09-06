#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Authored Fallout 3 collision geometry extracted from a NIF's Bethesda Havok
// chain. Positions are returned in ordinary NIF model units (the FO3 Havok
// 7.0 scale has already been applied), ready for the same REFR transform used
// by visible geometry.
struct Fo3NifCollisionMesh {
    std::vector<float> positions;          // xyz, NIF model units
    std::vector<uint32_t> indices;         // triangle list
    std::string modelPath;

    uint32_t collisionObjectBlock = 0xffffffffu;
    uint32_t bodyBlock = 0xffffffffu;
    uint32_t moppBlock = 0xffffffffu;
    uint32_t packedShapeBlock = 0xffffffffu;
    uint32_t dataBlock = 0xffffffffu;

    bool bodyTransformApplied = false;
    bool compressedVertices = false;
    uint16_t subShapeCount = 0;
};

// Decodes the original FO3 bhkCollisionObject -> bhkRigidBody[/T] ->
// bhkMoppBvTreeShape -> bhkPackedNiTriStripsShape ->
// hkPackedNiTriStripsData path. Direct packed shapes are also accepted.
// No render-mesh collision is generated here: this is Bethesda-authored data.
bool LoadFo3NifCollisionMeshes(const std::string& modelPath,
                               std::vector<Fo3NifCollisionMesh>& outMeshes);
