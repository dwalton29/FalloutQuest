#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fo3terrain::composite {
constexpr int Tile=32, Size=8*Tile;
using Pixel=std::array<uint8_t,4>;
using Source=std::array<Pixel,Tile*Tile>;
using Sources=std::unordered_map<std::string,std::shared_ptr<const Source>>;
inline std::shared_ptr<const Source> Downsample(const std::vector<uint8_t>& bytes,int width,int height) {
    if(width<1||height<1||size_t(width)*size_t(height)>bytes.size()/4)return {};
    auto tile=std::make_shared<Source>();
    for(int y=0;y<Tile;++y)for(int x=0;x<Tile;++x){
        const int x0=x*int64_t(width)/Tile,y0=y*int64_t(height)/Tile;
        const int x1=std::max(x0+1,int((x+1)*int64_t(width)/Tile));
        const int y1=std::max(y0+1,int((y+1)*int64_t(height)/Tile));
        uint64_t sum[4]{};
        for(int sy=y0;sy<y1;++sy)for(int sx=x0;sx<x1;++sx){
            const auto* pixel=bytes.data()+4*(size_t(sy)*width+sx);
            for(int c=0;c<4;++c)sum[c]+=pixel[c];
        }
        const auto count=uint64_t(x1-x0)*(y1-y0);
        for(int c=0;c<4;++c)(*tile)[y*Tile+x][c]=uint8_t((sum[c]+count/2)/count);
    }
    return tile;
}
struct Surface {
    std::vector<Pixel> colour,normal;
    float minX=0,maxX=0,minZ=0,maxZ=0;
    bool valid=false;
};
struct Point {int x,y;float alpha;};
inline int Edge(Point a,Point b,int x,int y){return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
inline bool OwnsEdge(Point a,Point b,int edge) {
    return edge>0||(edge==0&&(a.y<b.y||(a.y==b.y&&a.x>b.x)));
}
// LAND UVs have exactly eight texture repetitions per CELL. Rasterize at
// texel centres in authored layer order, owning shared triangle edges once.
template<class Batch>
Surface Bake(const std::vector<const Batch*>& batches,const Sources& sources) {
    Surface out;
    if(std::none_of(batches.begin(),batches.end(),[](const Batch* b){return b->alphaLayer&&!b->vertices.empty();}))return out;
    for(const auto* b:batches){
        if(!b->vertices.empty()&&!sources.count(b->texturePath))return out;
    }
    std::vector<uint8_t> coverage(Size*Size);
    const Pixel flat{128,128,255,0};
    out.colour.assign(Size*Size,{0,0,0,255});out.normal.assign(Size*Size,flat);
    for(const auto* b:batches){
        if(b->vertices.empty())continue;
        const auto& diffuse=*sources.at(b->texturePath);
        const auto bump=sources.find(b->normalPath);
        const auto n=b->indices.empty()?b->vertices.size()/12:b->indices.size();
        for(size_t i=0;i+2<n;i+=3){
            Point p[3];
            for(unsigned k=0;k<3;++k){
                size_t index=b->indices.empty()?i+k:b->indices[i+k];
                if(index>=b->vertices.size()/12)return {};
                const float* v=b->vertices.data()+index*12;
                const float u=(v[6]/8.f-b->gridX)*Size*2, t=(v[7]/8.f-b->gridY)*Size*2;
                if(!(u>=-.01f&&u<=Size*2+.01f&&t>=-.01f&&t<=Size*2+.01f))return {};
                p[k]={int(std::lround(u)),int(std::lround(t)),v[8]};
                if(!b->alphaLayer){
                    if(!out.valid){out.minX=out.maxX=v[0];out.minZ=out.maxZ=v[2];out.valid=true;}
                    out.minX=std::min(out.minX,v[0]);out.maxX=std::max(out.maxX,v[0]);
                    out.minZ=std::min(out.minZ,v[2]);out.maxZ=std::max(out.maxZ,v[2]);
                }
            }
            int area=Edge(p[0],p[1],p[2].x,p[2].y);
            if(!area)continue;
            if(area<0){std::swap(p[1],p[2]);area=-area;}
            for(int y=std::min({p[0].y,p[1].y,p[2].y})/2;y<std::max({p[0].y,p[1].y,p[2].y})/2;++y)
            for(int x=std::min({p[0].x,p[1].x,p[2].x})/2;x<std::max({p[0].x,p[1].x,p[2].x})/2;++x){
                const int a=Edge(p[1],p[2],2*x+1,2*y+1),bary=Edge(p[2],p[0],2*x+1,2*y+1),c=area-a-bary;
                if(!OwnsEdge(p[1],p[2],a)||!OwnsEdge(p[2],p[0],bary)||!OwnsEdge(p[0],p[1],c))continue;
                float alpha=b->alphaLayer?std::clamp((a*p[0].alpha+bary*p[1].alpha+c*p[2].alpha)/area,0.f,1.f):1.f;
                const size_t texel=(y%Tile)*Tile+x%Tile,offset=y*Size+x;
                if(b->alphaLayer&&!coverage[offset]&&alpha>0.f)return {};
                if(!b->alphaLayer)coverage[offset]=1;
                Pixel normal=bump==sources.end()?flat:(*bump->second)[texel];
                if(!b->specularEnabled)normal[3]=0;
                for(int ch=0;ch<4;++ch){
                    auto blend=[alpha](uint8_t old,uint8_t value){return uint8_t(old+(int(value)-old)*alpha+.5f);};
                    out.colour[offset][ch]=blend(out.colour[offset][ch],diffuse[texel][ch]);
                    out.normal[offset][ch]=blend(out.normal[offset][ch],normal[ch]);
                }
            }
        }
    }
    return out;
}
}
