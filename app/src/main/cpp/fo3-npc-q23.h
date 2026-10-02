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
    std::string hairModel;

    std::vector<uint32_t> headPartFormIds;
    std::vector<std::string> headPartModels;
    std::vector<Fo3NpcVisualItemQ230> inventory;

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
