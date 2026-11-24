#include "rendering/geometry-detail.h"
#include "rendering/terrain/terrain-geometry-detail.h"
#include <cassert>
#include <set>
#include <limits>
using fqdetail::Family;
int main(){
    constexpr unsigned side=17;
    std::vector<float> grid(side*side*15);
    std::vector<uint32_t> corners;
    for(unsigned y=0;y<side;++y)for(unsigned x=0;x<side;++x){
        const unsigned i=y*side+x;
        grid[i*15]=x;grid[i*15+2]=y;grid[i*15+4]=1;
        grid[i*15+6]=x*.125f;grid[i*15+7]=y*.125f;
        grid[i*15+12]=x/float(side-1);
        if(y+1<side&&x+1<side)corners.insert(corners.end(),{i,i+1,i+side,i+1,i+side+1,i+side});
    }
    auto terrain=fqdetail::Build(Family::Terrain,grid.data(),side*side,15,corners,false);
    for(auto range:terrain.ranges){
        assert(range.count>0 && range.count<corners.size());
        std::set<unsigned> kept(terrain.indices.begin()+range.first,terrain.indices.begin()+range.first+range.count);
        for(unsigned i=0;i<side;++i)for(auto b:{i,i+side*(side-1),i*side,i*side+side-1})assert(kept.count(b));
    }
    std::vector<float> expanded(corners.size()*18);
    for(size_t i=0;i<corners.size();++i){
        expanded[i*18]=float(corners[i]%side);expanded[i*18+2]=float(corners[i]/side);
    }
    std::vector<uint16_t> bones(side*side*4);
    std::vector<float> weights(side*side*4);
    for(unsigned i=0;i<side*side;++i){bones[4*i]=i%side>8;weights[4*i]=1;}
    auto skin=fqdetail::Build(Family::Actor,expanded.data(),corners.size(),18,corners,true,&bones,&weights);
    assert(skin.ranges[2].count>0);
    std::set<unsigned> retained;
    for(size_t i=skin.ranges[2].first;i<skin.indices.size();++i)retained.insert(corners.at(skin.indices[i]));
    for(unsigned y=0;y<side;++y){assert(retained.count(y*side+8));assert(retained.count(y*side+9));}
    // Both draws cover the same LAND, with different source order, opposite
    // winding, and a different alpha attribute. All three reduced meshes must
    // coincide in XYZ and keep the complete cell boundary.
    struct Batch {
        size_t vertexFloats=15;
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        fqdetail::Levels geometryLevels;
        fo3terrain::Bounds detailBounds;
    };
    Batch land,paint;
    land.vertices=grid;land.indices=corners;
    for(size_t i=side*side;i>0;--i){
        paint.vertices.insert(paint.vertices.end(),grid.begin()+(i-1)*15,grid.begin()+i*15);
        paint.vertices[paint.vertices.size()-7]=float(i%side)/side;
    }
    for(size_t i=0;i<corners.size();i+=3)
        for(unsigned j:{0u,2u,1u})paint.indices.push_back(side*side-1-corners[i+j]);
    fo3terrain::PrepareCellDetail(std::vector<Batch*>{&land,&paint});
    const unsigned keep[]{90,60,30};
    for(unsigned tier=0;tier<3;++tier){
        const unsigned target=(side*side*keep[tier]+50)/100;
        auto l=land.geometryLevels.ranges[tier],p=paint.geometryLevels.ranges[tier];
        assert(l.count>0&&p.count==l.count);
        assert(land.geometryLevels.tierVertices[tier]<=target && land.geometryLevels.tierVertices[tier]+2>=target);
        assert(paint.geometryLevels.tierVertices[tier]==land.geometryLevels.tierVertices[tier]);
        for(size_t i=0;i<l.count;i+=3)for(unsigned j=0;j<3;++j)
            assert(land.geometryLevels.indices[l.first+i+j]==side*side-1-paint.geometryLevels.indices[p.first+i+(j?3-j:0)]);
        assert(land.detailBounds.boundsValid && land.detailBounds.maxX==paint.detailBounds.maxX);
    }
    // Long narrow paint strips have no interior vertices. Reaching the
    // far budget requires moving both sides of a material boundary together.
    Batch ground,stripes;
    ground.vertices=grid;ground.indices=corners;stripes.vertices=grid;
    for(size_t i=0;i<corners.size();i+=6)
        if((corners[i]%side)%3==1)stripes.indices.insert(stripes.indices.end(),corners.begin()+i,corners.begin()+i+6);
    fo3terrain::PrepareCellDetail(std::vector<Batch*>{&ground,&stripes});
    for(unsigned tier=0;tier<3;++tier){
        const auto total=ground.geometryLevels.sourceVertices+stripes.geometryLevels.sourceVertices;
        const auto used=ground.geometryLevels.tierVertices[tier]+stripes.geometryLevels.tierVertices[tier];
        assert(used<=(total*keep[tier]+50)/100);
        auto faces=[&](const Batch& b){
            std::set<std::array<uint32_t,3>> out;
            const auto r=b.geometryLevels.ranges[tier];assert(r.count>0);
            for(size_t i=r.first;i<size_t(r.first)+r.count;i+=3){
                const auto* vertex=b.geometryLevels.indices.data()+i;
                std::array<uint32_t,3> face{vertex[0],vertex[1],vertex[2]};
                std::sort(face.begin(),face.end());out.insert(face);
            }
            return out;
        };
        auto base=faces(ground);
        for(auto face:faces(stripes))assert(base.count(face));
        unsigned twiceArea=0;
        std::map<std::array<uint32_t,2>,unsigned> edges;
        for(auto f:base){
            int x[3],z[3];
            for(unsigned j=0;j<3;++j){
                x[j]=int(f[j]%side);z[j]=int(f[j]/side);
                auto a=f[j],b=f[(j+1)%3];++edges[{std::min(a,b),std::max(a,b)}];
            }
            twiceArea+=std::abs((x[1]-x[0])*(z[2]-z[0])-(x[2]-x[0])*(z[1]-z[0]));
        }
        assert(twiceArea==2*(side-1)*(side-1));
        unsigned boundary=0;
        for(const auto& edge:edges){assert(edge.second<=2);boundary+=edge.second==1;}
        assert(boundary==4*(side-1));
    }
    // The last three vertices form a detached decoration. Its triangle must
    // survive even the far tier and a remaining one-triangle mesh stays drawable.
    std::vector<float> prop((side*side+3)*18);
    for(unsigned i=0;i<side*side;++i){prop[i*18]=float(i%side);prop[i*18+2]=float(i/side);}
    const unsigned trim=side*side;
    prop[trim*18]=30;prop[(trim+1)*18]=31;prop[(trim+2)*18]=30;prop[(trim+2)*18+2]=1;
    auto shapes=corners;shapes.insert(shapes.end(),{trim,trim+1,trim+2});
    auto lod=fqdetail::Build(Family::Scenery,prop.data(),side*side+3,18,shapes,false);
    for(auto r:lod.ranges){
        bool intact=false;
        for(size_t i=r.first;i<size_t(r.first)+r.count;i+=3){
            std::array<uint32_t,3> face{lod.indices[i],lod.indices[i+1],lod.indices[i+2]};
            std::sort(face.begin(),face.end());
            if(face==std::array<uint32_t,3>{trim,trim+1,trim+2})intact=true;
        }
        assert(intact);
    }
    const float eye[]{0,0,0};
    const float moved[]{0,2,0,0,-1,0,0,0,0,0,1,0,20,0,0,1};
    assert(fqdetail::MovedDistance(eye,moved,{-1,-3,-1},{1,3,1})==17.f);
    auto broken=corners;broken[1]=UINT32_MAX;
    assert(fqdetail::Build(Family::Terrain,grid.data(),side*side,15,broken,false).indices.empty());
    assert(fqdetail::Tier(.3f,-1)==0 && fqdetail::Tier(11.5f,1)==1 && fqdetail::Tier(11.5f,0)==0);
    assert(fqdetail::Tier(90.f,1)==2 && fqdetail::Tier(54.f,2)==1 && fqdetail::Tier(-1.f,2)==-1);
    assert(fqdetail::Tier(std::numeric_limits<float>::quiet_NaN(),2)==-1);
}
