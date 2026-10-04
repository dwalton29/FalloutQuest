#pragma once
#include "fo3-actor-animation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <functional>
namespace fo3anim {
struct Envelope { double translation=0,scale=1; };
inline double ChannelBound(const Channel& c,unsigned axis,double base) {
    double bound=std::abs(base);
    for(const auto& k:c.keys) {
        // Hermite basis endpoint coefficients <=1, tangent coefficients <1.
        bound=std::max(bound,2*(std::abs(double(k.value[axis]))+std::abs(double(k.forward[axis]))+std::abs(double(k.backward[axis]))));
    }
    for(float v:c.controls) bound=std::max(bound,std::abs(double(v))); // B-spline convex hull
    return bound;
}
inline std::vector<Envelope> ClipEnvelope(const Skeleton& s,const Clip& clip) {
    std::vector<Envelope> local(s.bones.size()),global(s.bones.size());
    std::vector<std::array<double,3>> translations(s.bones.size());
    for(size_t i=0;i<s.bones.size();++i) {
        for(unsigned axis=0;axis<3;++axis) translations[i][axis]=std::abs(double(s.bones[i].bind.translation[axis]));
        local[i].scale=std::abs(double(s.bones[i].bind.scale));
    }
    for(const auto& track:clip.tracks) {
        const int bone=FindBone(s,track.bone);if(bone<0) continue;
        for(unsigned axis=0;axis<3;++axis) if(track.hasTranslation)
            translations[bone][axis]=std::max(translations[bone][axis],ChannelBound(track.translation,axis,track.base.translation[axis]));
        if(track.hasScale) local[bone].scale=std::max(local[bone].scale,ChannelBound(track.scale,0,track.base.scale));
    }
    std::vector<uint8_t> state(s.bones.size());
    std::function<void(size_t)> visit=[&](size_t i) {
        if(state[i]==2) return;
        if(state[i]==1) { global[i]={std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity()};return; }
        state[i]=1;
        const auto& t=translations[i];local[i].translation=std::sqrt(t[0]*t[0]+t[1]*t[1]+t[2]*t[2]);
        const int parent=s.bones[i].parent;
        if(parent>=0 && static_cast<size_t>(parent)<s.bones.size()) {
            visit(parent);global[i]={global[parent].translation+global[parent].scale*local[i].translation,global[parent].scale*local[i].scale};
        } else global[i]=local[i];
        state[i]=2;
    };
    for(size_t i=0;i<s.bones.size();++i) visit(i);
    return global;
}
inline double LinearNormBound(const Matrix& m) {
    double sum=0;for(unsigned c=0;c<3;++c)for(unsigned r=0;r<3;++r)sum+=double(m[c*4+r])*m[c*4+r];return std::sqrt(sum);
}
inline double TranslationLength(const Matrix& m) { return std::sqrt(double(m[12])*m[12]+double(m[13])*m[13]+double(m[14])*m[14]); }
inline double DeltaRadius(const Skeleton& s,const std::vector<Envelope>& envelopes,int bone,double bindRadius) {
    if(bone<0) return bindRadius;
    if(static_cast<size_t>(bone)>=s.inverseBind.size() || static_cast<size_t>(bone)>=envelopes.size()) return std::numeric_limits<double>::infinity();
    return envelopes[bone].translation+envelopes[bone].scale*(LinearNormBound(s.inverseBind[bone])*bindRadius+TranslationLength(s.inverseBind[bone]));
}
} // namespace fo3anim
