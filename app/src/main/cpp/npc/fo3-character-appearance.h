#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace fo3character {
constexpr size_t ControlLimit = 128;
struct Appearance {
    uint32_t race=0,preset=0,hair=0,eyes=0,beard=0;
    std::array<uint8_t,3> color{};
    std::array<int8_t,ControlLimit> adjustments{};
    bool operator==(const Appearance& o) const {
        return race==o.race&&preset==o.preset&&hair==o.hair&&eyes==o.eyes&&
            beard==o.beard&&color==o.color&&adjustments==o.adjustments;
    }
    bool operator!=(const Appearance& o) const {return !(*this==o);}
};
struct Race {
    std::string name;
    std::vector<uint32_t> hair,eyes;
    std::array<uint32_t,2> defaultHair{};
    float shapeClamp=0,textureClamp=0;
};
struct Preset {
    std::string editor;
    Appearance appearance;
    bool female=false;
    std::vector<uint32_t> parts;
};
struct Option {
    std::string name;
    uint8_t flags=0;
};
struct Catalog {
    std::unordered_map<uint32_t,Race> races;
    std::unordered_map<uint32_t,Preset> presets;
    std::unordered_map<uint32_t,Option> hair,eyes,beards;
    std::vector<uint32_t> Races() const {
        std::vector<uint32_t> out;
        for(const auto& entry:races)out.push_back(entry.first);
        std::sort(out.begin(),out.end());
        return out;
    }
    std::vector<uint32_t> Presets(uint32_t race,bool female) const {
        std::vector<uint32_t> out;
        for(const auto& p:presets)if(p.second.appearance.race==race&&p.second.female==female)out.push_back(p.first);
        std::sort(out.begin(),out.end(),[&](uint32_t a,uint32_t b){return presets.at(a).editor<presets.at(b).editor;});
        return out;
    }
    std::vector<uint32_t> Hair(uint32_t race,bool female) const {
        std::vector<uint32_t> out;
        const auto r=races.find(race);
        if(r==races.end())return out;
        for(uint32_t id:r->second.hair){
            const auto h=hair.find(id);
            if(h!=hair.end()&&(h->second.flags&1)&&!(h->second.flags&(female?4:2)))out.push_back(id);
        }
        return out;
    }
    std::vector<uint32_t> Eyes(uint32_t race) const {
        std::vector<uint32_t> out;
        const auto r=races.find(race);
        if(r!=races.end())for(uint32_t id:r->second.eyes){
            const auto e=eyes.find(id);
            if(e!=eyes.end()&&(e->second.flags&1))out.push_back(id);
        }
        return out;
    }
    static bool Has(const std::vector<uint32_t>& options,uint32_t id) {
        return std::find(options.begin(),options.end(),id)!=options.end();
    }
    Appearance Select(uint32_t preset,bool female) const {
        const auto p=presets.find(preset);
        if(p==presets.end()||p->second.female!=female)return {};
        auto out=p->second.appearance;
        const auto r=races.find(out.race);
        if(r==races.end())return {};
        const auto h=Hair(out.race,female),e=Eyes(out.race);
        if(h.empty()||e.empty())return {};
        if(!Has(h,out.hair)){
            const auto fallback=r->second.defaultHair[female];
            out.hair=Has(h,fallback)?fallback:h.front();
        }
        if(!Has(e,out.eyes))out.eyes=e.front();
        if(!female)for(uint32_t part:p->second.parts)if(beards.count(part)){out.beard=part;break;}
        if(female||!beards.count(out.beard))out.beard=0;
        return out;
    }
    Appearance Default(uint32_t race,bool female) const {
        const auto available=Presets(race,female);
        if(!available.empty())return Select(available.front(),female);
        for(uint32_t id:Races()){
            const auto alternate=Presets(id,female);
            if(!alternate.empty())return Select(alternate.front(),female);
        }
        return {};
    }
    bool Valid(const Appearance& a,bool female) const {
        const auto p=presets.find(a.preset);
        return p!=presets.end()&&p->second.female==female&&p->second.appearance.race==a.race&&
            races.count(a.race)&&Has(Hair(a.race,female),a.hair)&&Has(Eyes(a.race),a.eyes)&&
            (!a.beard||(!female&&beards.count(a.beard)))&&
            std::all_of(a.adjustments.begin(),a.adjustments.end(),[](int v){return v>=-10&&v<=10;});
    }
};

// The installed FRCTL001 file defines linear axes in the same coefficient
// space used by the original EGM/EGT and NPC_ FGGS/FGGA/FGTS.
struct Control {
    std::string label;
    std::vector<float> weights;
    unsigned basis=0;
};
inline bool DecodeControls(const std::vector<uint8_t>& bytes,std::vector<Control>& out) {
    out.clear();
    if(bytes.size()<32||std::string(bytes.begin(),bytes.begin()+8)!="FRCTL001")return false;
    auto word=[&](size_t at){
        return uint32_t(bytes[at])|(uint32_t(bytes[at+1])<<8)|
            (uint32_t(bytes[at+2])<<16)|(uint32_t(bytes[at+3])<<24);
    };
    const std::array<unsigned,4> sizes{word(16),word(20),word(24),word(28)};
    if(sizes!=std::array<unsigned,4>{50,30,50,0})return false;
    size_t at=32;
    std::vector<Control> decoded;
    for(unsigned basis=0;basis<sizes.size();++basis){
        if(bytes.size()-at<4)return false;
        const auto count=word(at);at+=4;
        if(count>ControlLimit-decoded.size()||(count&&!sizes[basis]))return false;
        for(unsigned i=0;i<count;++i){
            if(bytes.size()-at<sizes[basis]*4+4)return false;
            Control c;c.basis=basis;c.weights.resize(sizes[basis]);
            for(float& v:c.weights){
                const uint32_t bits=word(at);at+=4;
                static_assert(sizeof(v)==sizeof(bits));
                std::memcpy(&v,&bits,sizeof(v));
                if(!std::isfinite(v)||std::abs(v)>1.01f)return false;
            }
            const auto len=word(at);at+=4;
            if(!len||len>160||len>bytes.size()-at)return false;
            c.label.assign(reinterpret_cast<const char*>(bytes.data()+at),len);at+=len;
            decoded.push_back(std::move(c));
        }
    }
    if(decoded.empty())return false;
    out=std::move(decoded);
    return true;
}
}
