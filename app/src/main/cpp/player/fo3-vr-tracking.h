#pragma once
#include <cstdint>
#include <cmath>
namespace fo3vr {
struct Tracking {bool valid=false,tracked=false;};
inline Tracking LocationTracking(uint64_t flags,uint64_t validMask,uint64_t trackedMask){
 return {(flags&validMask)==validMask,(flags&trackedMask)==trackedMask};
}
enum class TrackingSource { None, Grip, AimEvidence, Grace };
struct PipboyTracking {
 double lastReliable=-1;TrackingSource source=TrackingSource::None;
 float graceAge=0;
 void Reset(){lastReliable=-1;source=TrackingSource::None;graceAge=0;}
 bool Step(bool ready,bool loading,bool focused,Tracking head,Tracking grip,
           Tracking aim,double now){
  source=TrackingSource::None;graceAge=0;
  // A valid physical grip and head pose are always required for the canonical
  // mount. Aim is tracking evidence only, never substituted into the arm rig.
  if(!ready||loading||!focused||!head.valid||!head.tracked||!grip.valid||!std::isfinite(now)){
   Reset();return false;
  }
  if(grip.tracked||(aim.valid&&aim.tracked)){
   lastReliable=now;source=grip.tracked?TrackingSource::Grip:TrackingSource::AimEvidence;return true;
  }
  if(lastReliable>=0&&now>=lastReliable){
   graceAge=float(now-lastReliable);
   if(graceAge<=.20f){source=TrackingSource::Grace;return true;}
  }
  lastReliable=-1;return false;
 }
};
}
