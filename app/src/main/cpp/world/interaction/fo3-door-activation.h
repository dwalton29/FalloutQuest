#pragma once
#include "fo3-interaction-ray.h"
#include <array>
#include <cmath>

namespace fo3doorpick {
struct Bounds {
    fo3interaction::Point lo{},hi{};
    bool valid=false;
};
template<class Shapes> Bounds FromShapes(const Shapes& shapes){
    Bounds out;
    for(const auto& shape:shapes)for(size_t i=0;i+2<shape.positions.size();i+=3){
        fo3interaction::Point p{shape.positions[i],shape.positions[i+1],shape.positions[i+2]};
        for(float x:p)if(!std::isfinite(x))return {};
        if(!out.valid){out.lo=out.hi=p;out.valid=true;}
        else for(size_t c=0;c<3;++c){out.lo[c]=std::min(out.lo[c],p[c]);out.hi[c]=std::max(out.hi[c],p[c]);}
    }
    return out;
}
// inversePlacement maps scene metres to model metres. Retain the original
// world-ray parameter when converting to NIF units, including placement scale.
inline bool Ray(const Bounds& bounds,const float inversePlacement[16],float units,
    const fo3interaction::Point& origin,const fo3interaction::Point& direction,
    float range,float& distance){
    if(!bounds.valid||!std::isfinite(units)||units<=0)return false;
    fo3interaction::Point o{},d{};
    for(size_t r=0;r<3;++r){
        o[r]=units*(inversePlacement[r]*origin[0]+inversePlacement[4+r]*origin[1]+inversePlacement[8+r]*origin[2]+inversePlacement[12+r]);
        d[r]=units*(inversePlacement[r]*direction[0]+inversePlacement[4+r]*direction[1]+inversePlacement[8+r]*direction[2]);
    }
    return fo3interaction::Box(o,d,bounds.lo,bounds.hi,range,distance);
}
inline Bounds SceneBounds(const Bounds& bounds,const float placement[16],float units){
    Bounds out;if(!bounds.valid||!std::isfinite(units)||units<=0)return out;
    for(int corner=0;corner<8;++corner){
        const float x=(corner&1?bounds.hi[0]:bounds.lo[0])/units;
        const float y=(corner&2?bounds.hi[1]:bounds.lo[1])/units;
        const float z=(corner&4?bounds.hi[2]:bounds.lo[2])/units;
        fo3interaction::Point p{};for(size_t r=0;r<3;++r)p[r]=placement[r]*x+placement[4+r]*y+placement[8+r]*z+placement[12+r];
        if(!out.valid){out.lo=out.hi=p;out.valid=true;}
        else for(size_t r=0;r<3;++r){out.lo[r]=std::min(out.lo[r],p[r]);out.hi[r]=std::max(out.hi[r],p[r]);}
    }
    return out;
}
}
