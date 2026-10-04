#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
namespace fqskin {
using Matrix=std::array<float,16>;
using PaletteRow=std::array<float,32>;
inline Matrix Identity() { return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}; }
inline void Pack(PaletteRow& row,const Matrix& position,const Matrix& direction) {
    std::copy(position.begin(),position.end(),row.begin());
    std::copy(direction.begin(),direction.end(),row.begin()+16);
}
struct Mapping {
    std::vector<int> sources; // only mesh-referenced bone ordinals; slot zero is identity
    std::vector<float> attributes; // weights[4], palette indices[4] per expanded vertex
};
inline Mapping Map(const std::vector<uint16_t>& indices,const std::vector<float>& weights,
                   size_t bones,bool player) {
    Mapping out;
    if(indices.size()!=weights.size() || indices.size()%4) return out;
    std::vector<int> slots(bones,-1);
    out.attributes.resize(indices.size()*2);
    for(size_t v=0;v<indices.size()/4;++v) for(size_t j=0;j<4;++j) {
        const size_t at=v*4+j; const float weight=weights[at];
        int slot=0;
        const bool valid=std::isfinite(weight) && weight>(player ? 0.000001f : 0.0f);
        if(valid && indices[at]<bones) {
            int& cached=slots[indices[at]];
            if(cached<0) { cached=static_cast<int>(out.sources.size()+1);out.sources.push_back(indices[at]); }
            slot=cached;
        }
        out.attributes[v*8+j]=valid && (!player || indices[at]<bones) ? weight : 0;
        out.attributes[v*8+4+j]=static_cast<float>(slot);
    }
    return out;
}
// Matches existing weight policies, including player's unnormalised overweight
// vertices and bind remainder only below .999; NPC normalises all positive weights.
inline PaletteRow Blend(const float* attributes,const std::vector<PaletteRow>& palette,bool player) {
    PaletteRow row{};float sum=0;
    for(int j=0;j<4;++j) {
        const float w=attributes[j]; if(w<=0) continue;
        const auto& bone=palette[static_cast<size_t>(attributes[4+j])];
        for(size_t k=0;k<32;++k) row[k]+=w*bone[k];
        sum+=w;
    }
    if(player) { if(sum<.999f) for(size_t k=0;k<32;++k) row[k]+=palette[0][k]*std::max(0.0f,1-sum); }
    else if(sum>1e-8f) for(auto& f:row) f/=sum;
    else row=palette[0];
    return row;
}
inline std::array<float,3> Transform(const float* m,const float* p,bool direction) {
    return {m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+(direction?0:m[12]),
            m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+(direction?0:m[13]),
            m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+(direction?0:m[14])};
}
} // namespace fqskin
