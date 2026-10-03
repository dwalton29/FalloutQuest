#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Fo3NpcVisualItemQ230 {
    uint32_t formId = 0u;
    int32_t count = 0;
    std::string recordType;
    std::string editorId;
    std::string fullName;
    std::string modelPath;
    uint32_t bipedMask = 0u;
};

struct Fo3NpcActorQ230 {
    uint32_t refFormId = 0u;
    uint32_t baseFormId = 0u;
    uint32_t referenceFlags = 0u;
    uint32_t actorBaseFlags = 0u;
    uint32_t raceFormId = 0u;
    uint32_t hairFormId = 0u;
    uint32_t eyesFormId = 0u;

    std::string editorId;
    std::string fullName;
    std::string skeletonModel;
    std::string raceEditorId;
    std::string raceHeadModel;
    std::vector<std::string> raceHeadModels;
    std::vector<std::string> raceHeadTextures;
    std::vector<std::string> raceBodyModels;
    std::vector<std::string> raceBodyTextures;
    std::string raceBodyTextureModel;
    std::string hairModel;
    std::string hairTexturePath;
    std::string eyeTexturePath;

    std::vector<uint32_t> headPartFormIds;
    std::vector<std::string> headPartModels;
    std::vector<Fo3NpcVisualItemQ230> inventory;
    std::vector<float> faceGenGeometrySymmetric;
    std::vector<float> faceGenGeometryAsymmetric;
    std::vector<float> faceGenTextureSymmetric;
    std::vector<float> raceFaceGenGeometrySymmetric;
    std::vector<float> raceFaceGenGeometryAsymmetric;
    std::vector<float> raceFaceGenTextureSymmetric;
    uint8_t hairColor[4]{0u, 0u, 0u, 0u};

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
    float scale = 1.0f;

    bool female = false;
    bool hasFaceGenGeometry = false;
};

bool LoadFo3CellActors(uint32_t cellFormId,
    std::vector<Fo3NpcActorQ230>& outActors,
    const std::string& esmPath = {});

bool LoadFo3MegatonExteriorActorsQ230(
    std::vector<Fo3NpcActorQ230>& outActors);


struct Fo3FaceGenMorphQ233 {
    std::string egmPath;
    uint32_t vertexCount = 0u;
    uint32_t symmetricModes = 0u;
    uint32_t asymmetricModes = 0u;
    uint32_t geometryBasisVersion = 0u;
    std::vector<float> deltaXYZ;
};

// Resolve the exact companion .egm from Fallout - Meshes.bsa and evaluate the
// NPC's authored FGGS/FGGA coefficients into one XYZ delta per source vertex.
bool LoadFo3FaceGenMorphQ233(
    const std::string& nifPath,
    const std::vector<float>& symmetric,
    const std::vector<float>& asymmetric,
    Fo3FaceGenMorphQ233& out);


struct Fo3FaceGenTextureQ234 {
    std::string egtPath;
    std::string baseTexturePath;
    uint32_t rows = 0u;
    uint32_t columns = 0u;
    uint32_t symmetricModes = 0u;
    uint32_t asymmetricModes = 0u;
    uint32_t textureBasisVersion = 0u;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
};

// Evaluate the authored FaceGen EGT colour basis over the decoded Fallout DDS.
// This mirrors FaceGen's SCM operation in the same 0..255 colour domain:
// base colour + sum(mode * coefficient), preserving authored alpha.
bool LoadFo3FaceGenTextureQ234(
    const std::string& nifPath,
    const std::string& baseTexturePath,
    const std::vector<float>& symmetric,
    Fo3FaceGenTextureQ234& out);
