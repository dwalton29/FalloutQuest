#pragma once
#include "fo3-door-resources.h"
#include "data/fo3-xtel-index.h"
#include "pipboy/fo3-pipboy-data.h"

// Persistent REFRs belong to the WRLD persistent CELL, even while the player
// occupies a different grid CELL. Use authored DATA/XTEL, with live enables.
template<class Enabled>
std::vector<Fo3DoorAnchor> Fo3WorldDoorAnchors(const fo3xtel::Index& index,
    const fo3pipdata::Definitions& data,uint32_t world,Enabled enabled){
    std::vector<Fo3DoorAnchor> result;
    const auto* doors=index.WorldDoors(world);if(!doors)return result;
    for(uint32_t ref:*doors){
        const auto* link=index.Find(ref);const auto placed=data.targets.find(ref);
        if(!link||placed==data.targets.end()||((link->sourceRecordFlags|link->destinationRecordFlags)&0x20u)||!enabled(ref))continue;
        Fo3DoorAnchor a;a.sourceDoorRef=ref;a.sourceX=placed->second.x;a.sourceY=placed->second.y;a.sourceZ=placed->second.z;
        auto& t=a.teleport;t.valid=true;t.destinationDoorRefFormId=link->destinationRef;t.destinationCellFormId=link->destinationCell;
        t.x=link->arrival[0];t.y=link->arrival[1];t.z=link->arrival[2];
        t.rx=link->rotation[0];t.ry=link->rotation[1];t.rz=link->rotation[2];t.flags=link->teleportFlags;
        result.push_back(std::move(a));
    }
    return result;
}
