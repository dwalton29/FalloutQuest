#pragma once
#include "fo3-frame-history.h"
#include <unordered_set>
namespace fqmotion {
// Commands contain values and GL resource names, never scene-vector pointers.
// The scene cannot publish/delete resources between an eye draw and its replay.
struct Draw {
    uint64_t identity=0;
    uint32_t mesh=0,vbo=0,indexBuffer=0,skinAttributes=0,paletteTexture=0,diffuse=0;
    int count=0,vertices=0,skinMode=0;
    bool indexed=false,cull=true,clockwise=false,decal=false,alphaTest=false,vertexAlpha=false;
    float alpha=1,threshold=.5f;
};
enum Reason : uint32_t { MissingHistory=1,Deformation=2,Transparency=4,Water=8,Ui=16,Resources=32,Capacity=64,Instancing=128 };
struct Capture {
    fqspacewarp::History* history=nullptr;
    std::vector<Draw> draws;
    std::unordered_set<uint64_t> identities;
    uint32_t reasons=0;
    bool collecting=false,mainDrawn=false;
    void Begin(fqspacewarp::History& h){history=h.Active()?&h:nullptr;draws.clear();identities.clear();reasons=0;collecting=false;mainDrawn=false;}
    void Main(){collecting=history!=nullptr;mainDrawn=collecting;}
    void Block(uint32_t reason){if(history)reasons|=reason;}
    bool Record(const Draw& draw,const fqspacewarp::Drawable& shape){
        if(!collecting||!history)return false;
        if(!history->Record(draw.identity,shape)){Block(Capacity);return false;}
        if(!identities.insert(draw.identity).second)return true;
        if(draws.size()>=fqspacewarp::History::MaxDrawables){Block(Capacity);return false;}
        draws.push_back(draw);
        const auto* previous=history->Previous(draw.identity,shape.geometry);
        if(!previous)Block(MissingHistory);
        else if(previous->palette.size()!=shape.palette.size()||previous->deformedPositions.size()!=shape.deformedPositions.size())Block(Deformation);
        return true;
    }
    fqspacewarp::Coverage Coverage(bool rendered)const{
        fqspacewarp::Coverage result;if(!history)return result;
        result.serial=history->CurrentFrame().serial;
        result.motion=result.depth=rendered;
        result.staticWorld=result.cutouts=result.hands=result.weapons=rendered&&mainDrawn&&!reasons;
        result.dynamicExpected=static_cast<uint32_t>(draws.size());
        result.dynamicWritten=rendered?result.dynamicExpected:0;
        return result;
    }
};
inline bool requested=false;
inline Capture eye;
inline uint64_t nextIdentity=0;
// Called only by the actor upload owner; preserve facial bind deformation when
// GPU skinning updates a palette without uploading the position stream.
inline void Positions(fqspacewarp::Drawable& d,const std::vector<float>& stream){
    d.deformedPositions.resize(stream.size()/18*3);
    for(size_t v=0;v<stream.size()/18;++v)std::copy_n(stream.data()+v*18,3,d.deformedPositions.data()+v*3);
}
} // namespace fqmotion
