#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Fo3WorldPlacement {
    uint32_t refFormId = 0;
    uint32_t baseFormId = 0;
    std::string baseRecordType;
    std::string editorId;
    std::string modelPath;

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
    float scale = 1.0f;
};

// Reads REFR records from MegatonPlayerHouse (CELL 000151E3), resolves their
// base records, and returns only placed objects with a real MODL path.
bool LoadMegatonPlayerHousePlacements(std::vector<Fo3WorldPlacement>& outPlacements);
