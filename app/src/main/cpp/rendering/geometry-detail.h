#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fqdetail {
enum class Family : unsigned { Scenery, Terrain, Actor };
struct Range { uint32_t first=0, count=0; };
struct Levels {
    std::array<Range,3> ranges{};
    std::vector<uint32_t> indices;
    Family family=Family::Scenery;
    uint32_t sourceVertices=0;
    std::array<uint32_t,3> tierVertices{};
};

// Distance to the nearest point, including the case where the viewer is inside.
inline float Distance(const float* eye, float x0,float x1,float y0,float y1,float z0,float z1) {
    const float lo[]{x0,y0,z0},hi[]{x1,y1,z1};
    float squared=0;
    for(unsigned axis=0;axis<3;++axis) {
        if(!std::isfinite(eye[axis])||!std::isfinite(lo[axis])||!std::isfinite(hi[axis])||lo[axis]>hi[axis])return -1;
        const float d=std::max({lo[axis]-eye[axis],0.f,eye[axis]-hi[axis]});
        squared+=d*d;
    }
    return std::sqrt(squared);
}
inline int Tier(float distance,int previous) {
    if(!std::isfinite(distance)||distance<0)return -1;
    if(distance>=(previous==2?55.f:60.f))return 2;
    if(distance>=(previous>=1?11.f:12.f))return 1;
    return 0;
}

// sourceCorners maps each expanded NIF triangle corner to its original NIF vertex.
// The generated indices always address the existing GPU position and skin streams.
Levels Build(Family family,const float* vertices,size_t count,size_t stride,
             const std::vector<uint32_t>& sourceCorners,bool expanded,
             const std::vector<uint16_t>* bones=nullptr,const std::vector<float>* weights=nullptr);
}
