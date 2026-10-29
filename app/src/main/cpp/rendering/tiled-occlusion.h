#pragma once
#include "conservative-occlusion.h"
#include <vector>
#include <cstring>

namespace fo3occ {
// Each tile contains the uncovered convex pieces of its rectangle. Subtract
// original opaque polygons; declare coverage only when no area remains. This
// combines adjoining surfaces without treating pixel-centre samples as solids.
class TiledBuffer {
    struct V {double x=0,y=0;uint16_t boundary0=0,boundary1=0;};
    static constexpr size_t EdgeCapacity=8192;
    struct Edge {std::array<uint32_t,4> key{};uint32_t generation=0;};
    std::array<Edge,EdgeCapacity> edges{};uint32_t edgeGeneration=1;
    uint16_t EdgeIdentity(V lo,V hi){
        const float points[]{float(lo.x)==0?0.f:float(lo.x),float(lo.y)==0?0.f:float(lo.y),float(hi.x)==0?0.f:float(hi.x),float(hi.y)==0?0.f:float(hi.y)};
        std::array<uint32_t,4> key{};std::memcpy(key.data(),points,sizeof(points));
        uint64_t hash=2166136261u;for(auto v:key)hash=(hash^v)*16777619u;
        hash^=hash>>33;hash*=0xff51afd7ed558ccdull;hash^=hash>>33;
        for(size_t probe=0;probe<EdgeCapacity;++probe){const auto index=(hash+probe)&(EdgeCapacity-1);auto& e=edges[index];
            if(e.generation!=edgeGeneration){e.key=key;e.generation=edgeGeneration;return uint16_t(index+1);}
            if(e.key==key)return uint16_t(index+1);
        }
        return 0; // Registry exhaustion cannot manufacture coverage.
    }
    static V OnBoundary(V v,uint16_t id){
        if(v.boundary0!=id&&v.boundary1!=id){v.boundary1=v.boundary0;v.boundary0=id;}return v;
    }
    static uint16_t SharedBoundary(V a,V b){
        if(a.boundary0&&(a.boundary0==b.boundary0||a.boundary0==b.boundary1))return a.boundary0;
        if(a.boundary1&&(a.boundary1==b.boundary0||a.boundary1==b.boundary1))return a.boundary1;
        return 0;
    }
    static constexpr size_t MaxVertices=12,MaxPieces=12;
    struct Piece {std::array<V,MaxVertices> p{};size_t n=0;};
    struct Plane {double a=0,b=0,c=0;uint16_t identity=0;
        double At(V p)const{return identity&&(p.boundary0==identity||p.boundary1==identity)?0:a*p.x+b*p.y+c;}};
    struct Cell {std::vector<Piece> remaining;float farZ=-1;bool touched=false,full=false,overflow=false;};
    std::array<Cell,64*48> cells;
    std::vector<Piece> scratch;
    static double Area(const Piece& p){
        double area=0;
        for(size_t i=1;i+1<p.n;++i)area+=(p.p[i].x-p.p[0].x)*(p.p[i+1].y-p.p[0].y)-(p.p[i].y-p.p[0].y)*(p.p[i+1].x-p.p[0].x);
        return std::abs(area);
    }
    static Piece Clip(const Piece& in,const Plane& plane,bool inside,bool& overflow){
        Piece out;if(!in.n)return out;
        auto append=[&](V p){
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||out.n>=MaxVertices){overflow=true;return;}
            if(!out.n||p.x!=out.p[out.n-1].x||p.y!=out.p[out.n-1].y)out.p[out.n++]=p;
            else if(p.boundary0)out.p[out.n-1]=OnBoundary(out.p[out.n-1],p.boundary0);
        };
        V a=in.p[in.n-1];double da=plane.At(a);bool keepA=inside?da>=0:da<=0;
        for(size_t i=0;i<in.n;++i){
            const V b=in.p[i];const double db=plane.At(b);const bool keepB=inside?db>=0:db<=0;
            if(keepA!=keepB){
                // A boundary endpoint is already the exact intersection. In
                // particular a+(b-a) can round differently from b at t==1.
                if(da==0)append(OnBoundary(a,plane.identity));
                else if(db==0)append(OnBoundary(b,plane.identity));
                else{const double denominator=da-db;if(denominator==0){overflow=true;return {};}
                    const double t=da/denominator;append({a.x+t*(b.x-a.x),a.y+t*(b.y-a.y),plane.identity,SharedBoundary(a,b)});}
            }
            if(keepB)append(db==0?OnBoundary(b,plane.identity):b);
            a=b;da=db;keepA=keepB;
        }
        if(out.n>1&&out.p[0].x==out.p[out.n-1].x&&out.p[0].y==out.p[out.n-1].y)--out.n;
        if(out.n<3||Area(out)==0)out.n=0;
        return out;
    }
    static Piece Rectangle(int x,int y){
        const double l=-1+2.0*x/Width,r=-1+2.0*(x+1)/Width,b=-1+2.0*y/Height,t=-1+2.0*(y+1)/Height;
        Piece p;p.n=4;p.p[0]={l,b};p.p[1]={r,b};p.p[2]={r,t};p.p[3]={l,t};return p;
    }
public:
    static constexpr int Width=64,Height=48;
    size_t count=0,coveredCells=0,overflowCells=0,rasterCells=0;
    TiledBuffer(){scratch.reserve(MaxPieces);}
    void Clear(){
        if(++edgeGeneration==0){for(auto& edge:edges)edge.generation=0;edgeGeneration=1;}
        if(!count&&!rasterCells)return;
        for(auto& c:cells){c.remaining.clear();c.farZ=-1;c.touched=c.full=c.overflow=false;}
        count=coveredCells=overflowCells=rasterCells=0;
    }
    template<class Allow> void Add(const Polygon& p,Allow allow){
        if(p.count<3||p.count>4||!std::isfinite(p.farZ))return;
        std::array<Plane,4> planes{};
        float left=1,right=-1,bottom=1,top=-1;
        for(size_t i=0;i<p.count;++i){
            const auto a=p.projected[i],b=p.projected[(i+1)%p.count];
            if(!std::isfinite(a.x)||!std::isfinite(a.y))return;
            // Canonical edge arithmetic is shared by opposite sides of a seam.
            const bool forward=a.x<b.x||(a.x==b.x&&a.y<b.y);
            const V lo=forward?V{a.x,a.y}:V{b.x,b.y},hi=forward?V{b.x,b.y}:V{a.x,a.y};
            const double direction=(forward?1.0:-1.0)*p.winding;
            const double nx=-(hi.y-lo.y),ny=hi.x-lo.x;
            const auto identity=EdgeIdentity(lo,hi);if(!identity)return;
            planes[i]={nx*direction,ny*direction,-(nx*lo.x+ny*lo.y)*direction,identity};
            left=std::min(left,a.x);right=std::max(right,a.x);bottom=std::min(bottom,a.y);top=std::max(top,a.y);
        }
        if(right<=-1||left>=1||top<=-1||bottom>=1)return;
        const int x0=std::clamp(int(std::floor((std::max(-1.f,left)+1)*Width*.5)),0,Width-1);
        const int x1=std::clamp(int(std::floor((std::min(1.f,right)+1)*Width*.5)),0,Width-1);
        const int y0=std::clamp(int(std::floor((std::max(-1.f,bottom)+1)*Height*.5)),0,Height-1);
        const int y1=std::clamp(int(std::floor((std::min(1.f,top)+1)*Height*.5)),0,Height-1);
        ++count;
        for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
            if((rasterCells++%32)==0&&!allow())return;
            auto& c=cells[size_t(y*Width+x)];const auto tile=Rectangle(x,y);
            const Rect rect{float(tile.p[0].x),float(tile.p[1].x),float(tile.p[0].y),float(tile.p[2].y),2};
            if(p.Covers(rect)){
                const bool wasFull=c.full;if(!wasFull)++coveredCells;
                c.full=true;c.farZ=wasFull?std::min(c.farZ,p.farZ):p.farZ;c.touched=true;c.remaining.clear();continue;
            }
            if(c.full||c.overflow)continue;
            if(!c.touched){c.remaining.push_back(tile);c.touched=true;}
            scratch.clear();bool overflow=false,changed=false;
            for(const auto& piece:c.remaining){
                auto remainder=piece;
                for(size_t i=0;i<p.count&&remainder.n&&!overflow;++i){
                    auto outside=Clip(remainder,planes[i],false,overflow);
                    if(outside.n){if(scratch.size()>=MaxPieces){overflow=true;break;}scratch.push_back(outside);}
                    remainder=Clip(remainder,planes[i],true,overflow);
                }
                changed|=remainder.n!=0;
                if(overflow)break;
            }
            if(overflow){c.remaining.clear();c.overflow=true;++overflowCells;continue;}
            if(!changed)continue;
            c.farZ=std::max(c.farZ,p.farZ);c.remaining.swap(scratch);
            if(c.remaining.empty()){c.full=true;++coveredCells;}
        }
    }
    void Add(const Polygon& p){Add(p,[]{return true;});}
    bool Hidden(const Rect& source,uint32_t& tests,uint32_t limit)const{
        // Expand the occludee instead of opening cracks between its occluders.
        const Rect r{source.left-.004f,source.right+.004f,source.bottom-.004f,source.top+.004f,source.nearZ};
        if(!std::isfinite(r.left)||!std::isfinite(r.right)||!std::isfinite(r.bottom)||!std::isfinite(r.top)||!std::isfinite(r.nearZ)||
           r.left<-1||r.right>1||r.bottom<-1||r.top>1||r.left>r.right||r.bottom>r.top)return false;
        const int x0=int(std::floor((r.left+1)*Width*.5)),x1=std::min(Width-1,int(std::floor((r.right+1)*Width*.5)));
        const int y0=int(std::floor((r.bottom+1)*Height*.5)),y1=std::min(Height-1,int(std::floor((r.top+1)*Height*.5)));
        for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
            if(tests>=limit)return false;
            ++tests;const auto& c=cells[size_t(y*Width+x)];
            if(!c.full||source.nearZ<=c.farZ+.00001f)return false;
        }
        return true;
    }
};
}
