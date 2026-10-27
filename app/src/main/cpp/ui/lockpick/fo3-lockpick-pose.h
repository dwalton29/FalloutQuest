#pragma once
#include "npc/fo3-actor-animation.h"
#include <algorithm>
#include <cmath>

namespace fo3lockpose {
inline const fo3anim::Clip* Find(const std::vector<fo3anim::Clip>& clips,const char* name) {
    const auto i=std::find_if(clips.begin(),clips.end(),[&](const auto& c){return c.name==name;});
    return i==clips.end()?nullptr:&*i;
}
inline float Duration(const fo3anim::Clip* c) {
    return c&&c->frequency>0?std::max(0.f,(c->stop-c->start)/c->frequency):0;
}
// Menu controllers seek to an absolute key time. They do not loop at the end
// of the authored lock-turn or break sequence, despite NIF cycleType=LOOP.
inline bool Seek(const fo3anim::Skeleton& hierarchy,const fo3anim::Clip* source,
                 float seconds,fo3anim::Pose& pose) {
    if(!source)return false;
    fo3anim::BindClip(hierarchy,*source,pose);
    return fo3anim::Sample(hierarchy,*source,std::clamp(seconds,0.f,Duration(source)),pose,
        nullptr,fo3anim::RootPolicy::Locomotion,nullptr,fo3anim::Playback::Clamp);
}
inline bool Sample(const fo3anim::Skeleton& hierarchy,const std::vector<fo3anim::Clip>& clips,
                   bool pin,float intro,float cylinder,float health,float broken,
                   fo3anim::Pose& pose) {
    const auto* forward=Find(clips,"Forward");
    if(intro<Duration(forward))return Seek(hierarchy,forward,intro,pose);
    const auto* motion=Find(clips,broken>=0&&pin?"Left":"Backward");
    const float progress=pin?std::clamp(1-health,0.f,1.f):std::clamp(cylinder/90.f,0.f,1.f);
    if(pin&&broken<0&&health>=1)return Seek(hierarchy,forward,Duration(forward),pose);
    return Seek(hierarchy,motion,pin&&broken>=0?broken:Duration(motion)*progress,pose);
}
inline fo3anim::Matrix PinPlacement(float degrees) {
    // Fallout3.exe 1.7.0.4: 0x943650 places BobbyPin01 at (0,0,-1.6).
    // 0x944210 rotates its whole model about (0,0,0.6), not the animated
    // bobbypin node. Positive input moves the pin to screen-right.
    const float radians=std::clamp(degrees,-90.f,90.f)*.01745329252f;
    fo3anim::Transform turn;turn.rotation={std::cos(radians*.5f),0,std::sin(radians*.5f),0};
    auto matrix=fo3anim::ToMatrix(turn);
    matrix[12]=-2.2f*matrix[8];matrix[13]=-2.2f*matrix[9];matrix[14]=.6f-2.2f*matrix[10];
    return matrix;
}
} // namespace fo3lockpose
