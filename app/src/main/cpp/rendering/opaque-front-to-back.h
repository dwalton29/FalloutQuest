#pragma once
// Q235 opaque packet scheduler. No OpenGL operations, material substitutions,
// changes to geometry or unsafe packet movement.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>
namespace fqopaqueorder {
struct Bounds { float minX,maxX,minY,maxY,minZ,maxZ; };
inline float NearestBoxDistanceSquared(float x,float y,float z,const Bounds& b){
    if(!(b.minX<=b.maxX&&b.minY<=b.maxY&&b.minZ<=b.maxZ)||
       !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||
       !std::isfinite(b.minX)||!std::isfinite(b.maxX)||
       !std::isfinite(b.minY)||!std::isfinite(b.maxY)||
       !std::isfinite(b.minZ)||!std::isfinite(b.maxZ))
       return std::numeric_limits<float>::infinity();
    const float dx=std::max({b.minX-x,0.0f,x-b.maxX});
    const float dy=std::max({b.minY-y,0.0f,y-b.maxY});
    const float dz=std::max({b.minZ-z,0.0f,z-b.maxZ});
    return dx*dx+dy*dy+dz*dz;
}
constexpr float kBucketMetres=4.0f;
struct SortKey {
    bool safe=false;
    uint32_t bucket=0u;
    uint64_t material=0u;
    size_t ordinal=0u;
};
inline SortKey MakeKey(bool depthWritingSafe,float distanceSquared,
                       uint64_t material,size_t ordinal){
    const bool safe=depthWritingSafe&&std::isfinite(distanceSquared)&&distanceSquared>=0.0f;
    const float metres=safe?std::sqrt(distanceSquared):0.0f;
    const uint32_t bucket=safe?static_cast<uint32_t>(
        std::min(65535.0f,std::floor(metres/kBucketMetres))):0u;
    return {safe,bucket,material,ordinal};
}
inline bool LessSafe(const SortKey& a,const SortKey& b){
    if(a.bucket!=b.bucket)return a.bucket<b.bucket;
    if(a.material!=b.material)return a.material<b.material;
    return a.ordinal<b.ordinal;
}
template<class Packet>void SortSafeRuns(std::vector<Packet>& packets){
    size_t first=0u;
    while(first<packets.size()){
        if(!packets[first].key.safe){++first;continue;}
        size_t end=first+1u;
        while(end<packets.size()&&packets[end].key.safe)++end;
        std::sort(packets.begin()+static_cast<std::ptrdiff_t>(first),
                  packets.begin()+static_cast<std::ptrdiff_t>(end),
                  [](const Packet& a,const Packet& b){return LessSafe(a.key,b.key);});
        first=end;
    }
}
} // namespace fqopaqueorder
