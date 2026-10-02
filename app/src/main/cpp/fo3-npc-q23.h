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
    std::string hairModel;
    std::string eyeTexturePath;

    std::vector<uint32_t> headPartFormIds;
    std::vector<std::string> headPartModels;
    std::vector<Fo3NpcVisualItemQ230> inventory;
    std::vector<float> faceGenGeometrySymmetric;
    std::vector<float> faceGenGeometryAsymmetric;
    std::vector<float> faceGenTextureSymmetric;
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
