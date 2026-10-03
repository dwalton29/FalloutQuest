#pragma once
#include "npc/fo3-actor-animation.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace fo3slideshow {
// The original NIF authors movement and sound-key timing. The hold between
// changes is a VR presentation choice; it is not claimed as vanilla timing.
constexpr double HoldSeconds = 6.0;
struct Player {
    int left=-1, forward=-1, backward=-1, current=-1;
    double started=0, lastKeyTime=-1;
    bool running=false, waiting=false;
    size_t visible=0, nextImage=1, slots[2]{0,1};
    fo3anim::Pose pose;
    std::vector<std::string> sounds;

    bool Configure(const fo3anim::Skeleton& hierarchy, const std::vector<fo3anim::Clip>& clips) {
        left=forward=backward=-1;
        for(size_t i=0;i<clips.size();++i) {
            if(clips[i].name=="Left")left=static_cast<int>(i);
            else if(clips[i].name=="Forward")forward=static_cast<int>(i);
            else if(clips[i].name=="Backward")backward=static_cast<int>(i);
        }
        running=left>=0 && forward>=0 && backward>=0;
        if(running) {current=left;fo3anim::BindClip(hierarchy,clips[current],pose);}
        return running;
    }
    void Reset(double now, size_t images) {
        started=now;lastKeyTime=-1;waiting=false;current=left;
        visible=0;nextImage=1;slots[0]=0;slots[1]=images>1?1:0;sounds.clear();
    }
    bool Advance(const fo3anim::Skeleton& hierarchy, const std::vector<fo3anim::Clip>& clips,
                 double now, size_t images) {
        sounds.clear();
        if(!running || current<0 || !std::isfinite(now))return false;
        if(now<started)Reset(now,images);
        const auto& old=clips[current];
        const double duration=(old.stop-old.start)/old.frequency;
        double elapsed=std::max(0.0,now-started);
        if(waiting && elapsed>=duration+HoldSeconds && images>1) {
            current=visible==0?forward:backward;
            const size_t incoming=1-visible;
            slots[incoming]=nextImage++%images;
            // Start from this submitted frame, preventing a resume from firing
            // a backlog of unseen animations/sound cues.
            started=now;lastKeyTime=-1;waiting=false;elapsed=0;
            fo3anim::BindClip(hierarchy,clips[current],pose);
        }
        const auto& clip=clips[current];
        const double time=clip.start+std::min(elapsed*clip.frequency,double(clip.stop-clip.start));
        for(const auto& key:clip.textKeys)
            if(key.time>lastKeyTime && key.time<=time && key.text.rfind("sound: ",0)==0)
                sounds.push_back(key.text.substr(7));
        lastKeyTime=time;
        if(!fo3anim::Sample(hierarchy,clip,elapsed,pose))return false;
        if(!waiting && elapsed>=(clip.stop-clip.start)/clip.frequency) {
            waiting=true;visible=current==forward?1:0;
        }
        return true;
    }
};
} // namespace fo3slideshow
