#pragma once
#include "player/fo3-vr-body.h"
namespace fo3weapon {
// VR presentation/access policy, not Fallout data. Manual loading is the
// default. Supported long guns also offer a button reload while gripped.
inline bool& ManualReloadEnabled(){static bool enabled=true;return enabled;}
// A button press starts one presentation interval. Ammunition is committed by
// the caller only when Finish succeeds; dropping the instance cancels it.
struct ReloadPlayback {
  double start=0,seconds=0;
  explicit operator bool()const{return seconds>0;}
  bool Begin(double now,float duration){
    if(seconds>0||!std::isfinite(now)||!std::isfinite(duration)||duration<=0)return false;
    start=now;seconds=duration;return true;
  }
  float PoseWeight(double elapsed)const{
    return std::clamp(float(std::min(elapsed,seconds-elapsed))/.10f,0.f,1.f);
  }
  bool Finish(double now){
    if(!seconds||!std::isfinite(now)||now-start<seconds)return false;
    seconds=0;return true;
  }
};
inline bool InsertionZone(fo3vr::V hand,fo3vr::V centre,fo3vr::V half){
  const float radius=fo3vr::Length(half)+.30f;
  return fo3vr::Finite(hand)&&fo3vr::Finite(centre)&&fo3vr::Finite(half)&&fo3vr::Length(hand-centre)<=radius;
}
}
