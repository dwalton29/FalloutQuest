#pragma once
#include "world/fo3-cell-traversal.h"
#include "world/fo3-reference-state.h"
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>

struct Fo3DoorAnchor {
    uint32_t sourceDoorRef=0;
    float sourceX=0,sourceY=0,sourceZ=0;
    Fo3DoorTeleport teleport;
};
struct Fo3DoorCellCache {bool attempted=false;std::vector<Fo3DoorAnchor> anchors;};
struct Fo3DoorPromptMetadata {uint32_t sourceBase=0;Fo3DoorTeleport teleport;};
struct Fo3DoorResources {
    std::unordered_map<uint32_t,Fo3DoorCellCache> cells;
    std::unordered_map<uint32_t,Fo3DoorPromptMetadata> metadata;
    std::unordered_map<uint32_t,std::string> prompts;
};
// Render-thread legacy accesses must mark before structural mutation. The CPU
// worker receives an owned copy and never accesses this live store/publication.
Fo3DoorResources& Fo3LiveDoorResources();
uint64_t GetFo3DoorResourceRevision();
void MarkFo3DoorResourcesChanged();
Fo3DoorResources CaptureFo3DoorResources();
bool FormatFo3DoorPrompt(const std::string& door,const std::string& destination,std::string& out);
bool PrepareFo3ResidentDoorResources(const std::string& path,uint32_t cell,
    const fo3world::ReferenceStateSnapshot& published,const fo3world::ReferenceStateSnapshot& candidate,
    const std::vector<uint32_t>& affected,const Fo3DoorResources& captured,Fo3DoorResources& out,
    const std::atomic<bool>& cancelled,std::string& error);
bool QueryFo3DoorAnchorResources(const std::vector<Fo3DoorAnchor>& anchors,
    float centerX,float centerY,float floorZ,float units,float floorY,float forward,
    float ox,float oy,float oz,float dx,float dy,float dz,Fo3DoorAimQ1700* out);
bool PrepareFo3ResidentDoorPlan(uint64_t revision,Fo3DoorResources& candidate);
bool CanCommitFo3ResidentDoorPlan();
bool CanRollbackFo3ResidentDoorPlan();
bool CanFinalizeFo3ResidentDoorPlan(bool committed);
bool CommitFo3ResidentDoorPlan();
bool RollbackFo3ResidentDoorPlan();
void DiscardFo3ResidentDoorPlan();
bool HasFo3ResidentDoorPlan();
bool Fo3ResidentDoorPlanApplied();
