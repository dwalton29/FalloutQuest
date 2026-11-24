#include "rendering/geometry-detail.h"
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
    auto broken=corners;broken[1]=UINT32_MAX;
    assert(fqdetail::Build(Family::Terrain,grid.data(),side*side,15,broken,false).indices.empty());
    assert(fqdetail::Tier(.3f,-1)==0 && fqdetail::Tier(11.5f,1)==1 && fqdetail::Tier(11.5f,0)==0);
    assert(fqdetail::Tier(90.f,1)==2 && fqdetail::Tier(54.f,2)==1 && fqdetail::Tier(-1.f,2)==-1);
    assert(fqdetail::Tier(std::numeric_limits<float>::quiet_NaN(),2)==-1);
}
