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

struct Fo3CellArrival {
    uint32_t sourceDoorRefFormId = 0;
    uint32_t destinationDoorRefFormId = 0;
    uint32_t flags = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
    bool valid = false;
};

// Reads REFR records from MegatonPlayerHouse (CELL 000151E3), resolves their
// base records, and returns only placed objects with a real MODL path.
bool LoadMegatonPlayerHousePlacements(std::vector<Fo3WorldPlacement>& outPlacements);

// Reads the paired load-door XTEL authored by Bethesda and returns the exact
// destination marker used by the original game when entering MegatonPlayerHouse.
bool LoadMegatonPlayerHouseArrival(Fo3CellArrival& outArrival);
