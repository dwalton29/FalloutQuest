#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Fo3DoorTeleport {
    uint32_t sourceDoorRefFormId = 0;
    uint32_t destinationDoorRefFormId = 0;
    uint32_t destinationCellFormId = 0;
    uint32_t flags = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
    bool valid = false;
};

struct Fo3WorldPlacement {
    uint32_t owningCellFormId = 0;
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

    Fo3DoorTeleport teleport;
};

struct Fo3CellArrival {
    uint32_t sourceDoorRefFormId = 0;
    uint32_t destinationDoorRefFormId = 0;
    uint32_t destinationCellFormId = 0;
    uint32_t flags = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
    bool valid = false;
};

// Generic Fallout 3 CELL loader. Reads the target CELL's REFR children,
// evaluates the initial enable-parent graph, resolves BASE->MODL, converts
// Bethesda placement rotations, and retains load-door XTEL links.
bool LoadFo3CellPlacements(uint32_t cellFormId,
                           std::vector<Fo3WorldPlacement>& outPlacements);

// Finds the CELL whose child groups own a REFR. Used to resolve XTEL destination
// doors into a loadable destination CELL.
bool FindFo3RefOwningCell(uint32_t refFormId, uint32_t& outCellFormId);

// Finds the authored arrival marker used when entering a CELL through a paired
// XTEL load door. This is the generic form of the Q6K player-house spawn path.
bool LoadFo3CellArrival(uint32_t cellFormId, Fo3CellArrival& outArrival);

// Compatibility wrappers retained while older milestone code is still present.
bool LoadMegatonPlayerHousePlacements(std::vector<Fo3WorldPlacement>& outPlacements);
bool LoadMegatonPlayerHouseArrival(Fo3CellArrival& outArrival);
