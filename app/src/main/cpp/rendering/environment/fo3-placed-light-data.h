#pragma once
#include "fo3-interior-lighting.h"
struct Fo3PlacedLight {
    uint32_t refFormId=0,baseFormId=0;
    std::string editorId;
    float position[3]{0,0,0},color[3]{1,1,1};
    float radius=1,falloff=1,fade=1;
    uint32_t flags=0;
};
constexpr int FO3_SHADER_LIGHTS=8;
inline std::vector<Fo3PlacedLight> PrepareFo3PlacedInteriorLights(const fo3interior::Snapshot& snapshot) {
    std::vector<Fo3PlacedLight> next;next.reserve(snapshot.lights.size());
    for(const auto& light:snapshot.lights) {
        Fo3PlacedLight p;p.refFormId=light.ref;p.baseFormId=light.base;p.editorId=light.editorId;
        for(int i=0;i<3;++i){p.position[i]=light.position[i];p.color[i]=light.colour[i];}
        p.radius=light.radius;p.fade=light.fade;p.falloff=light.falloff;p.flags=light.flags;
        next.push_back(std::move(p));
    }
    return next;
}
inline bool MatchesFo3PlacedInteriorLights(const fo3interior::Snapshot& snapshot,
        const std::vector<Fo3PlacedLight>& placed) {
    if(snapshot.lights.size()!=placed.size())return false;
    for(size_t n=0;n<placed.size();++n) {
        const auto& l=snapshot.lights[n];const auto& p=placed[n];
        if(l.ref!=p.refFormId||l.base!=p.baseFormId||l.editorId!=p.editorId||l.flags!=p.flags||
            l.radius!=p.radius||l.fade!=p.fade||l.falloff!=p.falloff)return false;
        for(int i=0;i<3;++i)if(l.position[i]!=p.position[i]||l.colour[i]!=p.color[i])return false;
    }
    return true;
}
