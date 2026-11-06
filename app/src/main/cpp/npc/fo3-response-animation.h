#pragma once
#include "fo3-actor-animation.h"
namespace fo3npc {
inline bool RetainsResponsePose(const fo3anim::Clip& clip){return clip.cycle==2||fo3anim::ControlsAccumulationRoot(clip);}
// Consecutive spoken responses are not new animation instances. A full-body
// posture keeps its authored clock until explicitly replaced or locomotion wins.
template<class Actor>
bool SelectResponseAnimation(Actor& actor,uint32_t idle,double now){
  const auto previous=actor.responseClips.find(actor.responseIdle);
  if(idle==actor.responseIdle)return false;
  if(!idle&&previous!=actor.responseClips.end()&&
     (actor.runtime.packagePoseIdle==actor.responseIdle||RetainsResponsePose(previous->second)))return false;
  const auto next=actor.responseClips.find(idle);
  if(idle&&next==actor.responseClips.end())return false;
  actor.runtime.packagePoseIdle=0;actor.runtime.packageEndIdle=0;actor.runtime.packageEndIdleUntil=-1;
  actor.responseIdle=idle;actor.responseLastTime=-1;actor.responseStarted=now;
  actor.runtime.authoredMotionUntil=next!=actor.responseClips.end()&&fo3anim::ControlsAccumulationRoot(next->second)?
    now+(next->second.stop-next->second.start)/next->second.frequency:0;
  return true;
}
}
