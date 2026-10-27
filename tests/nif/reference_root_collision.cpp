#include "physics/fo3-nif-collision.h"
#include <cassert>
void TestReferenceDoorCollision(const std::string& path) {
    std::vector<Fo3NifCollisionShapeQ6F> collision;
    assert(LoadFo3NifCollisionShapesComplete(path,collision));
    assert(!collision.empty());
    for(const auto& part:collision) {
        float ymin=1e9,ymax=-1e9;
        for(size_t j=1;j<part.positions.size();j+=3){ymin=std::min(ymin,part.positions[j]);ymax=std::max(ymax,part.positions[j]);}
        assert(ymax-ymin>140.f);
    }
}
