#pragma once
#include <cstddef>
#include <cstdint>

namespace fqspacewarp {
// Weapon parts may share an uploaded shape across several placements in one frame.
enum class PlacementKind : uint8_t { Scene, Weapon, SpareMagazine, EjectedMagazine, ShaderAddon };

struct Placement {
    PlacementKind kind=PlacementKind::Scene;
    uint64_t id=0;
};

struct MotionIdentity {
    uint64_t shape;
    Placement placement;

    constexpr MotionIdentity(uint64_t shapeId=0,Placement instance={})
        :shape(shapeId),placement(instance){}

    bool operator==(const MotionIdentity& other)const{
        return shape==other.shape && placement.id==other.placement.id && placement.kind==other.placement.kind;
    }
};

struct MotionIdentityHash {
    std::size_t operator()(const MotionIdentity& key)const{
        if(key.placement.kind==PlacementKind::Scene)return static_cast<std::size_t>(key.shape);
        uint64_t seed=key.placement.id;
        seed^=key.shape+0x9e3779b97f4a7c15ULL+(seed<<6)+(seed>>2);
        return static_cast<std::size_t>(seed^(uint64_t(key.placement.kind)*0x9e3779b97f4a7c15ULL));
    }
};
}
