#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace fo3terrain {
// The same renderer-space positions consumed by aPosition, for BOTH cold
// upload and streamed-cell publication. Unknown bounds always draw.
struct Bounds {
    float minX=0,maxX=0,minY=0,maxY=0,minZ=0,maxZ=0;
    bool boundsValid=false;
};
inline Bounds BuildBounds(const float* vertices,size_t floats) {
    Bounds b;
    constexpr size_t stride=12;
    if(!vertices||!floats||floats%stride)return b;
    b.minX=b.maxX=vertices[0];b.minY=b.maxY=vertices[1];b.minZ=b.maxZ=vertices[2];
    for(size_t i=0;i<floats;i+=stride){
        const float x=vertices[i],y=vertices[i+1],z=vertices[i+2];
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return Bounds{};
        b.minX=std::min(b.minX,x);b.maxX=std::max(b.maxX,x);
        b.minY=std::min(b.minY,y);b.maxY=std::max(b.maxY,y);
        b.minZ=std::min(b.minZ,z);b.maxZ=std::max(b.maxZ,z);
    }
    b.boundsValid=true;return b;
}
inline bool MayIntersectClip(const Bounds& b,const float* m) {
    if(!m||!b.boundsValid)return true;
    for(size_t i=0;i<16;++i)if(!std::isfinite(m[i]))return true;
    // Six metres of conservative expansion. Transform all corners exactly as
    // the shader does; no plane normalization, matrix inversion or w division.
    // A convex box is rejected only if every corner is outside ONE plane.
    unsigned outsideAll=63;
    for(unsigned corner=0;corner<8;++corner){
        const double x=(corner&1)?double(b.maxX)+6:double(b.minX)-6;
        const double y=(corner&2)?double(b.maxY)+6:double(b.minY)-6;
        const double z=(corner&4)?double(b.maxZ)+6:double(b.minZ)-6;
        const double cx=m[0]*x+m[4]*y+m[8]*z+m[12];
        const double cy=m[1]*x+m[5]*y+m[9]*z+m[13];
        const double cz=m[2]*x+m[6]*y+m[10]*z+m[14];
        const double cw=m[3]*x+m[7]*y+m[11]*z+m[15];
        const double epsilon=1e-4*std::max({1.0,std::abs(cx),std::abs(cy),std::abs(cz),std::abs(cw)});
        unsigned outside=0;
        if(cx < -cw-epsilon)outside|=1;
        if(cx >  cw+epsilon)outside|=2;
        if(cy < -cw-epsilon)outside|=4;
        if(cy >  cw+epsilon)outside|=8;
        if(cz < -cw-epsilon)outside|=16;
        if(cz >  cw+epsilon)outside|=32;
        outsideAll&=outside;
        if(!outsideAll)return true;
    }
    return !outsideAll;
}
}
