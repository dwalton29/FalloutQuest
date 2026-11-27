#pragma once
#include "player/fo3-vr-body.h"
namespace fo3weapon {
// Manual transfers are the default. Animated button reloads are an opt-in setting.
inline bool& ManualReloadEnabled(){static bool enabled=true;return enabled;}
enum class AmmoGrip { Free, Seated, Removed, Spare };
enum class AmmoTransfer { None, Eject, Insert };

// VR inventory gestures. Progress is tied to the tracked prop's position.
// Only moving an installed prop clear, or releasing a spare at its socket,
// requests a change to the canonical inventory.
struct HandReload {
  AmmoGrip grip=AmmoGrip::Free;
  bool aligned=false;
  explicit operator bool()const{return grip!=AmmoGrip::Free;}
  void Grab(bool spare){grip=spare?AmmoGrip::Spare:AmmoGrip::Seated;aligned=false;}
  AmmoTransfer Update(bool tracked,bool released,float socketDistance){
    if(!tracked||!std::isfinite(socketDistance)){*this={};return AmmoTransfer::None;}
    if(grip==AmmoGrip::Seated&&socketDistance>=.12f){
      grip=released?AmmoGrip::Free:AmmoGrip::Removed;return AmmoTransfer::Eject;
    }
    aligned=grip==AmmoGrip::Spare&&socketDistance<=(aligned?.12f:.09f);
    if(!released)return AmmoTransfer::None;
    const bool insert=aligned;*this={};
    return insert?AmmoTransfer::Insert:AmmoTransfer::None;
  }
};
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
