#include "rendering/terrain/terrain-composite.h"
#include "rendering/vulkan/fo3-world-resolution.h"
#include <cassert>
struct Batch {
    std::string texturePath,normalPath;
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    bool alphaLayer=false,specularEnabled=false;
    int gridX=-2,gridY=1;
};
int main(){
    fqvk::WorldResolution controller;
    for(int i=0;i<100;++i)controller.Observe(24,13.89,controller.step);
    assert(controller.step==6);
    for(int i=0;i<120;++i)controller.Observe(6,13.89,0);
    assert(controller.step==6);
    for(int i=0;i<420;++i)controller.Observe(6,13.89,controller.step);
    assert(controller.step==0);
    using namespace fo3terrain::composite;
    Batch base;base.texturePath="ground";
    for(auto uv:std::array<std::array<float,2>,4>{{{0,0},{1,0},{1,1},{0,1}}}){
        std::array<float,12> v{};v[0]=uv[0];v[2]=uv[1];
        v[6]=(base.gridX+uv[0])*8;v[7]=(base.gridY+uv[1])*8;v[8]=1;
        base.vertices.insert(base.vertices.end(),v.begin(),v.end());
    }
    base.indices={0,1,2,2,3,0};
    auto paint=base;paint.alphaLayer=true;paint.texturePath="path";
    for(size_t i=8;i<paint.vertices.size();i+=12)paint.vertices[i]=.5f;
    Sources sources;sources["ground"]=Downsample({255,0,0,255},1,1);
    sources["path"]=Downsample({0,0,255,255},1,1);
    std::vector<const Batch*> stack{&base,&paint};
    auto cell=Bake(stack,sources);assert(cell.valid);
    for(const auto& p:cell.colour)assert((p==Pixel{128,0,128,255}));
    assert((cell.normal.front()==Pixel{128,128,255,0}));
    sources.erase("path");assert(!Bake(stack,sources).valid);
    assert(!Downsample({1,2,3},1,1));
}
