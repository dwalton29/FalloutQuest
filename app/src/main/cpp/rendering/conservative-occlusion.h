#pragma once
#include "quest-render-policy.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Current-camera analytic occlusion. No depth readback, query wait, historical
// visibility, synthetic solid bounding boxes or raster coverage assumptions.
namespace fo3occ {
struct Point { float x=0,y=0,z=0; };
struct Triangle { Point p[3]; };
struct Surface { Point p[4];unsigned count=3; };
inline bool Same(Point a,Point b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
// Union of two original triangles sharing the same oppositely directed edge.
// Projection still has to prove the resulting quad convex before it is used.
inline bool Merge(const Triangle& a,const Triangle& b,Surface& out) {
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)
        if(Same(a.p[i],b.p[(j+1)%3])&&Same(a.p[(i+1)%3],b.p[j])) {
            out={{a.p[(i+1)%3],a.p[(i+2)%3],a.p[i],b.p[(j+2)%3]},4};
            return true;
        }
    return false;
}
struct Rect { float left=0,right=0,bottom=0,top=0,nearZ=0; };
struct Polygon {
    float nx[4]{},ny[4]{},c[4]{},farZ=1,area=0;
    unsigned count=0;
    bool Covers(const Rect& r) const {
        // Two NDC margins: shrink real occluders by >2 pixels at the normal
        // eye resolution; require a strictly separated depth interval.
        if(r.nearZ<=farZ+0.00001f)return false;
        for(unsigned i=0;i<count;++i) {
            const float x=nx[i]>=0?r.left:r.right;
            const float y=ny[i]>=0?r.bottom:r.top;
            if(nx[i]*x+ny[i]*y+c[i]<=0.004f)return false;
        }
        return true;
    }
};
inline bool Project(const float* m,Point p,Point& out) {
    const double w=double(m[3])*p.x+double(m[7])*p.y+double(m[11])*p.z+m[15];
    const double z=double(m[2])*p.x+double(m[6])*p.y+double(m[10])*p.z+m[14];
    if(!std::isfinite(w)||!std::isfinite(z)||w<=0.0001||z<=-w+0.00001||z>=w)return false;
    out={float((double(m[0])*p.x+double(m[4])*p.y+double(m[8])*p.z+m[12])/w),
         float((double(m[1])*p.x+double(m[5])*p.y+double(m[9])*p.z+m[13])/w),float(z/w)};
    return std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.z);
}
inline bool ProjectPolygon(const float* m,const Point* points,unsigned n,
                           bool twoSided,bool clockwise,Polygon& out) {
    if(n<3||n>4)return false;
    Point p[4];
    for(unsigned i=0;i<n;++i)if(!Project(m,points[i],p[i]))return false;
    const double cross=double(p[1].x-p[0].x)*(p[2].y-p[0].y)-
                       double(p[1].y-p[0].y)*(p[2].x-p[0].x);
    if(!std::isfinite(cross)||std::fabs(cross)<1e-10)return false;
    if(!twoSided && ((cross<0)!=clockwise))return false;
    const float sign=cross>0?1.f:-1.f;
    Polygon result;result.count=n;result.farZ=-1;
    float left=1,right=-1,bottom=1,top=-1;
    for(unsigned i=0;i<n;++i) {
        const auto a=p[i],b=p[(i+1)%n];
        const float dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);
        if(!std::isfinite(len)||len<1e-6f)return false;
        result.nx[i]=-dy*sign/len;result.ny[i]=dx*sign/len;
        result.c[i]=-(result.nx[i]*a.x+result.ny[i]*a.y);
        // In particular, reject folded/concave terrain quads. For a convex
        // projected original quad, its two authored triangles cover the hull.
        for(unsigned j=0;j<n;++j)
            if(result.nx[i]*p[j].x+result.ny[i]*p[j].y+result.c[i]<-1e-6f)return false;
        result.farZ=std::max(result.farZ,a.z);
        left=std::min(left,a.x);right=std::max(right,a.x);
        bottom=std::min(bottom,a.y);top=std::max(top,a.y);
    }
    result.area=std::max(0.f,std::min(1.f,right)-std::max(-1.f,left))*
                std::max(0.f,std::min(1.f,top)-std::max(-1.f,bottom));
    if(result.area<0.0004f)return false;
    out=result;return true;
}
inline bool ProjectBounds(const float* m,const questrender::Bounds& b,Rect& out) {
    if(b.minX>b.maxX||b.minY>b.maxY||b.minZ>b.maxZ)return false;
    Rect r{1e30f,-1e30f,1e30f,-1e30f,1};
    for(unsigned i=0;i<8;++i) {
        Point p;
        if(!Project(m,{i&1?b.maxX+0.15f:b.minX-0.15f,
                       i&2?b.maxY+0.15f:b.minY-0.15f,
                       i&4?b.maxZ+0.15f:b.minZ-0.15f},p))return false;
        r.left=std::min(r.left,p.x);r.right=std::max(r.right,p.x);
        r.bottom=std::min(r.bottom,p.y);r.top=std::max(r.top,p.y);
        r.nearZ=std::min(r.nearZ,p.z);
    }
    if(r.left<-1||r.right>1||r.bottom<-1||r.top>1)return false;
    out=r;return true;
}
struct Buffer {
    static constexpr size_t Capacity=64;
    std::array<Polygon,Capacity> polygons{};
    size_t count=0;
    void Clear(){count=0;}
    void Add(const Polygon& p) {
        if(count<Capacity){polygons[count++]=p;return;}
        auto it=std::min_element(polygons.begin(),polygons.end(),
                                [](const auto& a,const auto& b){return a.area<b.area;});
        if(p.area>it->area)*it=p;
    }
    bool Hidden(const Rect& r,uint32_t& tests,uint32_t limit) const {
        for(size_t i=0;i<count && tests<limit;++i) {
            ++tests;if(polygons[i].Covers(r))return true;
        }
        return false;
    }
};
inline bool OpaquePixels(const uint8_t* rgba,size_t bytes) {
    if(!rgba||bytes==0||bytes%4)return false;
    for(size_t i=3;i<bytes;i+=4)if(rgba[i]!=255)return false;
    return true;
}
} // namespace fo3occ
