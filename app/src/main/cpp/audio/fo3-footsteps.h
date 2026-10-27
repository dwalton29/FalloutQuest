#pragma once
#include "npc/fo3-actor-animation.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace fo3audio {
// Fallout3.exe 1.7.0.4: material dispatch 0x8e945a, tables 0x1114928,
// 0x11149a8, 0x11149d0, 0x11149f8; sound initialization 0x8e84f0.
inline constexpr std::array<unsigned,32> FootstepMaterials{
    0,0,7,0,8,2,0,0,9,5,0,2,5,2,0,0,3,4,6,1,2,2,3,4,6,4,2,2,5,5,2,5};
inline constexpr std::array<const char*,10> FootstepSurfaces{
    "ConcSolid","ConcBroken","MetalSolid","MetalHollow","MetalSheet",
    "Wood","Sand","Dirt","Grass","Water"};
inline constexpr std::array<unsigned,10> FootstepFallback{10,0,0,2,3,0,7,0,7,0};
inline constexpr std::array<const char*,9> FootstepActions{
    "WalkL","WalkR","RunR","RunL","SneakL","SneakR","StairsL","StairsR","Land"};
struct FootstepGait {
    float duration=0, stride=0;
    std::array<float,2> contacts{}; // normalized m:L / m:R contact markers
    bool Load(const fo3anim::Clip& clip) {
        *this={};
        if(!std::isfinite(clip.start)||!std::isfinite(clip.stop)||
           !std::isfinite(clip.frequency)||clip.frequency<=0)return false;
        const float length=clip.stop-clip.start;
        if(length<=0)return false;
        bool left=false,right=false;
        for(const auto& key:clip.textKeys) {
            if(!std::isfinite(key.time)||key.time<clip.start||key.time>clip.stop)continue;
            if(key.text=="m:L"){contacts[0]=(key.time-clip.start)/length;left=true;}
            if(key.text=="m:R"){contacts[1]=(key.time-clip.start)/length;right=true;}
        }
        for(const auto& track:clip.tracks)if(track.bone==clip.accumulationRoot && track.translation.keys.size()>=2) {
            const auto& a=track.translation.keys.front().value;
            const auto& b=track.translation.keys.back().value;
            stride=std::hypot(b[0]-a[0],b[1]-a[1])/70.f;
            break;
        }
        if(!left||!right||!(contacts[0]<contacts[1])||!std::isfinite(stride)||stride<.1f)return false;
        duration=length/clip.frequency;return true;
    }
};
struct FootstepClock {
    float phase=0;
    void Reset(){phase=0;}
    // Travel after capsule resolution, not stick input. Native KF root distance
    // scales the contact cadence continuously with VR locomotion speed.
    int Advance(float distance,float dt,bool enabled,const FootstepGait& walk,const FootstepGait& run) {
        if(!enabled||!std::isfinite(distance)||!std::isfinite(dt)||dt<=0||dt>.1f||
           distance<=.0001f||distance>1.f||walk.duration<=0){Reset();return -1;}
        const bool fast=run.duration>0 && distance/dt>(walk.stride/walk.duration+run.stride/run.duration)*.5f;
        const auto& gait=fast?run:walk;
        const float end=phase+distance/gait.stride;
        int result=-1;float earliest=end+1.f;
        for(int foot=0;foot<2;++foot) {
            float contact=gait.contacts[foot];
            if(contact<=phase)contact+=1.f;
            if(contact<=end && contact<earliest){result=foot;earliest=contact;}
        }
        phase=std::fmod(end,1.f);
        return result<0?-1:fast?(result==0?3:2):result;
    }
};
} // namespace fo3audio
