#pragma once
#include "physics/fo3-nif-collision.h"
#include <algorithm>
#include <cmath>
#include <map>

// Fallout 3 controller-driven particle bursts. Each sprite belongs to a
// NiParticleSystem and uses that system's own shader, emitter and birth keys.
namespace fo3burst {
struct Layer {
    std::string texture;
    std::array<float,3> origin{},volume{};
    float speed=0,speedVariation=0,declination=0,declinationVariation=0,angle=0,angleVariation=0;
    float radius=0,radiusVariation=0,life=0,lifeVariation=0,begin=0,window=0;
    float fadeIn=0,fadeOut=1,grow=0,shrink=0;
    uint32_t count=0;uint8_t sourceBlend=6,destBlend=7;
    bool sphere=false;
};
inline std::vector<Layer> Decode(const std::vector<uint8_t>& bytes){
    using namespace fo3_q6f_collision_detail;
    NifHeader h;if(!ParseHeader(bytes,h)||h.userVersion!=11||h.bsVersion!=34)return {};
    auto get=[&](uint32_t id,const char* type,size_t n)->const uint8_t*{
        return id<h.numBlocks&&h.blockSizes[id]>=n&&BlockType(h,id)==type?BlockData(bytes,h,id):nullptr;
    };
    struct Birth {float rate=0;std::vector<std::pair<float,float>> windows;};
    std::map<uint32_t,Birth> births;
    for(uint32_t i=0;i<h.numBlocks;++i)if(const auto seq=get(i,"NiControllerSequence",12)){
        const auto count=ReadLe32(seq+4);if(count>(h.blockSizes[i]-12)/29)continue;
        for(uint32_t j=0;j<count;++j){
            const auto row=seq+12+j*29;const uint32_t interp=ReadLe32(row);
            const auto ctl=get(ReadLe32(row+4),"NiPSysEmitterCtlr",38);if(!ctl)continue;
            auto& birth=births[ReadLe32(ctl+22)];
            if(const auto rate=get(interp,"NiFloatInterpolator",8)){
                const float v=ReadLeFloat(rate);if(std::isfinite(v)&&v>=0)birth.rate=v;
            }else if(const auto gate=get(interp,"NiBoolInterpolator",5)){
                const auto dataId=ReadLe32(gate+1);const auto data=get(dataId,"NiBoolData",8);if(!data)continue;
                const auto keys=ReadLe32(data);if(keys>(h.blockSizes[dataId]-8)/5)continue;
                float start=-1;
                for(uint32_t k=0;k<keys;++k){
                    const auto key=data+8+5*k;const float t=ReadLeFloat(key);
                    if(!std::isfinite(t)||t<0)break;
                    if(key[4]&&start<0)start=t;
                    if(!key[4]&&start>=0){if(t>start)birth.windows.push_back({start,t-start});start=-1;}
                }
            }
        }
        if(!births.empty())break;
    }
    std::vector<Layer> layers;
    for(uint32_t i=0;i<h.numBlocks;++i){
        const bool sphere=BlockType(h,i)=="NiPSysSphereEmitter";
        const auto emitter=get(i,sphere?"NiPSysSphereEmitter":"NiPSysBoxEmitter",sphere?77:85);
        if(!emitter||!emitter[12])continue;
        const auto system=ReadLe32(emitter+8);const auto ps=get(system,"NiParticleSystem",72);
        const auto birth=births.find(system);if(!ps||birth==births.end())continue;
        const size_t extras=ReadLe32(ps+4),size=h.blockSizes[system];
        if(extras>(size-72)/4)continue;
        const size_t off=68+extras*4;const uint32_t props=ReadLe32(ps+off);
        if(size-off<12||props>(size-off-12)/4)continue;
        Layer l;l.sphere=sphere;
        for(uint32_t j=0;j<props;++j){
            const auto prop=ReadLe32(ps+off+4+j*4);
            if(const auto shader=get(prop,"BSShaderNoLightingProperty",38)){
                if(ReadLe32(shader+4)!=0)continue;
                const uint32_t len=ReadLe32(shader+34);
                if(len&&len<=h.blockSizes[prop]-38)l.texture.assign(reinterpret_cast<const char*>(shader+38),len);
            }else if(const auto alpha=get(prop,"NiAlphaProperty",15)){
                if(ReadLe32(alpha+4)!=0)continue;
                const uint16_t flags=ReadLe16(alpha+12);l.sourceBlend=(flags>>1)&15;l.destBlend=(flags>>5)&15;
            }
        }
        const auto data=get(ReadLe32(ps+off+8+4*props),"NiPSysData",6);
        if(!data||l.texture.empty())continue;
        const unsigned capacity=ReadLe16(data+4);
        float values[14];bool valid=true;
        for(size_t k=0;k<14;++k){values[k]=ReadLeFloat(emitter+13+k*4);valid=valid&&std::isfinite(values[k]);}
        if(!valid||values[10]<=0||values[12]<=0)continue;
        l.speed=values[0];l.speedVariation=values[1];l.declination=values[2];l.declinationVariation=values[3];
        l.angle=values[4];l.angleVariation=values[5];l.radius=values[10];l.radiusVariation=values[11];
        l.life=values[12];l.lifeVariation=values[13];
        for(unsigned a=0;a<3;++a)l.volume[a]=ReadLeFloat(emitter+73+(sphere?0:a*4));
        for(uint32_t k=0;k<h.numBlocks;++k){
            if(const auto color=get(k,"BSPSysSimpleColorModifier",85);color&&ReadLe32(color+8)==system){
                l.fadeIn=std::clamp(ReadLeFloat(color+13),0.f,1.f);
                l.fadeOut=std::clamp(ReadLeFloat(color+17),l.fadeIn,1.f);
            }
            if(const auto grow=get(k,"NiPSysGrowFadeModifier",29);grow&&ReadLe32(grow+8)==system){
                l.grow=ReadLeFloat(grow+13);l.shrink=ReadLeFloat(grow+19);
            }
        }
        for(const auto& window:birth->second.windows){
            l.begin=window.first;l.window=window.second;
            const double n=std::round(double(birth->second.rate)*l.window);
            l.count=uint32_t(std::clamp(n,0.,double(std::min(capacity,1024u))));
            if(l.count)layers.push_back(l);
        }
    }
    return layers;
}
}
