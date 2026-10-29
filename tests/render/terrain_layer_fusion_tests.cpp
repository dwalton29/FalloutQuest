#include "rendering/terrain/terrain-layer-fusion.h"
#include <cassert>
#include <iostream>
struct Batch {
    int gridX=1,gridY=2,layerOrder=-1; bool alphaLayer=false,specularEnabled=false;
    size_t vertexFloats=12; std::string texturePath="base",normalPath="normal";
    std::vector<float> vertices;std::vector<uint32_t> indices;
    std::vector<fo3terrain::PaintMaterial> paintMaterials;
};
Batch Triangle(bool paint,float alpha,float x=0){
    Batch b;b.alphaLayer=paint;b.texturePath=paint?"paint":"base";
    b.vertices={x,0,0, 0,1,0, 0,0,alpha, 1,1,1,
                x+1,0,0, 0,1,0, 1,0,alpha, 1,1,1,
                x,0,1, 0,1,0, 0,1,alpha, 1,1,1};return b;
}
int main(){
    auto ready=[](const Batch& b){return b.texturePath!="missing";};
    for(bool indexed:{false,true})for(int layers=1;layers<=3;++layers){
        std::vector<Batch> b{Triangle(false,1)};
        for(int l=0;l<layers;++l){auto p=Triangle(true,float(l+1)/4);p.specularEnabled=l%2;b.push_back(p);}
        if(indexed)for(auto& v:b)v.indices={0,1,2};
        auto stats=fo3terrain::FuseLayers(b,ready);
        assert(stats.triangles==1 && stats.paintTriangles==size_t(layers) && stats.groups==1);
        for(int l=0;l<=layers;++l)assert(b[l].vertices.empty()&&b[l].indices.empty());
        const auto& f=b.back();assert(f.vertexFloats==15 && f.vertices.size()==45 && !f.alphaLayer);
        assert(f.paintMaterials.size()==size_t(layers));
        for(int c=0;c<3;++c)for(int l=0;l<3;++l)assert(f.vertices[c*15+12+l]==(l<layers?float(l+1)/4:0));
    }
    // Complete stacks only: changed UV/normal/colour, base alpha, reordered
    // corners, duplicate base topology, and >3 layers must preserve all data.
    for(int bad=0;bad<8;++bad){
        std::vector<Batch> b{Triangle(false,1),Triangle(true,.4f)};
        if(bad<3)b[1].vertices[bad==0?6:bad==1?4:9]+=.1f;
        if(bad==3)b[0].vertices[8]=.99f;
        if(bad==4)std::swap_ranges(b[1].vertices.begin(),b[1].vertices.begin()+12,b[1].vertices.begin()+12);
        if(bad==5)b.push_back(b[0]);
        if(bad==6)for(int i=0;i<3;++i)b.push_back(b[1]);
        if(bad==7){auto duplicate=Triangle(true,.7f);b[1].vertices.insert(b[1].vertices.end(),duplicate.vertices.begin(),duplicate.vertices.end());}
        const auto original=b;auto stats=fo3terrain::FuseLayers(b,ready);
        assert(!stats.triangles && b.size()==original.size());
        for(size_t i=0;i<b.size();++i)assert(b[i].vertices==original[i].vertices);
    }
    // Missing textures are not drawn by the original path and cannot become
    // a new visible paint layer. Remaining legacy geometry is retained.
    std::vector<Batch> b{Triangle(false,1),Triangle(true,.4f)};b[1].texturePath="missing";
    assert(!fo3terrain::FuseLayers(b,ready).triangles && b.size()==2);
    // Indexed compaction keeps the unrelated triangle and its original indices.
    b={Triangle(false,1),Triangle(true,.4f)};
    auto extra=Triangle(false,1,10);b[0].vertices.insert(b[0].vertices.end(),extra.vertices.begin(),extra.vertices.end());
    b[0].indices={0,1,2,3,4,5};
    auto stats=fo3terrain::FuseLayers(b,ready);
    assert(stats.triangles==1 && b[0].indices==std::vector<uint32_t>({3,4,5}));
    // Seven sparse subsets of three paints share one material group.
    b={Batch{},Batch{},Batch{},Batch{}};for(size_t i=1;i<4;++i)b[i].alphaLayer=true;
    for(int mask=1;mask<8;++mask)for(int layer=0;layer<4;++layer){
        if(layer && !(mask&(1<<(layer-1))))continue;
        auto t=Triangle(layer!=0,layer?.4f:1,float(mask)*2);
        b[layer].vertices.insert(b[layer].vertices.end(),t.vertices.begin(),t.vertices.end());
    }
    stats=fo3terrain::FuseLayers(b,ready);
    assert(stats.triangles==7 && stats.groups==1 && b.back().vertices.size()==7*45);
    for(int mask=1;mask<8;++mask)for(int c=0;c<3;++c)for(int p=0;p<3;++p)
        assert(b.back().vertices[(mask-1)*45+c*15+12+p]==((mask&(1<<p))?.4f:0));
    // Twenty distinct three-material combinations cannot share four-material
    // groups, so the cell retains its seven original draws.
    b=std::vector<Batch>(7);for(size_t i=1;i<7;++i)b[i].alphaLayer=true;
    for(int mask=1;mask<64;++mask){
        if(__builtin_popcount(unsigned(mask))!=3)continue;
        for(int layer=0;layer<7;++layer){
            if(layer && !(mask&(1<<(layer-1))))continue;
            auto t=Triangle(layer!=0,layer?.4f:1,float(mask)*2);
            b[layer].vertices.insert(b[layer].vertices.end(),t.vertices.begin(),t.vertices.end());
        }
    }
    const auto original=b;stats=fo3terrain::FuseLayers(b,ready);
    assert(!stats.triangles && b.size()==7);
    for(size_t i=0;i<7;++i)assert(b[i].vertices==original[i].vertices);
    // Retained complex geometry must not veto useful large groups elsewhere.
    b=std::vector<Batch>(7);for(size_t i=1;i<7;++i){b[i].alphaLayer=true;b[i].texturePath=std::to_string(i);}
    for(int mask=1;mask<=32;++mask){
        if(__builtin_popcount(unsigned(mask))!=3&&mask!=15&&mask!=32)continue;
        for(int repeat=0;repeat<(mask==32?20:1);++repeat)for(int layer=0;layer<7;++layer){
            if(layer&&!(mask&(1<<(layer-1))))continue;
            auto t=Triangle(layer!=0,layer?.4f:1,float(mask+repeat*64)*2);
            b[layer].vertices.insert(b[layer].vertices.end(),t.vertices.begin(),t.vertices.end());
        }
    }
    stats=fo3terrain::FuseLayers(b,ready);
    size_t draws=0;for(const auto& batch:b)draws+=!batch.vertices.empty();
    assert(stats.triangles>0&&stats.groups>0&&stats.fallback>0&&draws<=7);
    std::map<int,std::pair<int,unsigned>> contributions;
    auto paint=[&](int mask,unsigned layer){auto& value=contributions[mask];assert(!(value.second&(1u<<layer)));value.second|=1u<<layer;};
    for(size_t batch=0;batch<b.size();++batch){const auto& value=b[batch];
        for(size_t at=0;at<value.vertices.size();at+=value.vertexFloats*3){
            const int mask=int(value.vertices[at]/2);auto& contribution=contributions[mask];
            if(value.alphaLayer)paint(mask,unsigned(batch-1));
            else {++contribution.first;
                for(size_t layer=0;layer<value.paintMaterials.size();++layer)
                    if(value.vertices[at+12+layer]>0)paint(mask,unsigned(std::stoi(value.paintMaterials[layer].texturePath)-1));
            }
        }
    }
    assert(contributions.size()==31);
    for(const auto& value:contributions)assert(value.second.first==1&&value.second.second==unsigned(value.first%64));
    std::cout<<"Terrain fusion: exact stacks, ordered alpha, fallback and draw guard passed\n";
}
