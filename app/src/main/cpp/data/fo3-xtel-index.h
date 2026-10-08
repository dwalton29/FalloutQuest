#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Original-data-only, immutable after Build. This is a topological index:
// the next NPC milestone must still enforce locks, enable parents, scripts,
// local NAVM approach and actual scene residency before traversing a door.
namespace fo3xtel {

struct DoorLink {
    uint32_t sourceRef=0, sourceBase=0, sourceCell=0, sourceWorld=0;
    uint32_t destinationRef=0, destinationBase=0, destinationCell=0, destinationWorld=0;
    uint32_t sourceRecordFlags=0, destinationRecordFlags=0, teleportFlags=0;
    std::array<float,3> arrival{};     // XTEL destination game coordinates
    std::array<float,3> rotation{};    // XTEL destination authored radians
    bool reciprocal=false;
};

struct Statistics {
    size_t records=0, referenceOwners=0, authoredXtels=0;
    size_t resolved=0, missingDestination=0, nonDoor=0, malformed=0;
    size_t crossCell=0, crossWorld=0, reciprocal=0;
};

class Index {
public:
    // One off-render-thread ESM pass. Builds into a temporary index and
    // publishes only after the full scan/validation succeeds.
    bool Build(const std::string& esmPath,std::string& error);
    const DoorLink* Find(uint32_t sourceDoorRef) const;
    const std::vector<uint32_t>* Outgoing(uint32_t sourceCell) const;

    // Returns authored source-door FormIDs on a directed CELL route. This
    // proves graph connectivity only, not that the NPC can use those doors.
    bool CellRoute(uint32_t fromCell,uint32_t toCell,
                   std::vector<uint32_t>& sourceDoorRefs) const;
    const Statistics& Stats() const {return stats_;}
    size_t Size() const {return links_.size();}
private:
    std::unordered_map<uint32_t,DoorLink> links_;
    std::unordered_map<uint32_t,std::vector<uint32_t>> byCell_;
    Statistics stats_;
};

} // namespace fo3xtel
