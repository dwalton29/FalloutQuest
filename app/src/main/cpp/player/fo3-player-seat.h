#pragma once
#include "npc/fo3-furniture.h"
namespace fo3seat {
struct Pose {std::array<float,3> eye{};float yaw=0,bodyDrop=0;};
// Quest comfort policy: commit a camera relocation only at full black.
class State {
public:
 enum class Commit {None,Sit,Stand};
 enum class Phase {Idle,Out,In};
 std::shared_ptr<fo3furniture::Scene> scene;
 fo3furniture::Lease lease;
 Pose sitting,standing;
 uint32_t reference=0;size_t marker=SIZE_MAX;
 bool seated=false,entering=false;Phase phase=Phase::Idle;double started=0;
 bool Busy()const{return phase!=Phase::Idle;}
 bool Active()const{return seated||Busy();}
 bool Begin(uint32_t ref,size_t index,const Pose& sit,const Pose& stand,double now){
  if(Active()||!scene||index>=scene->slots.size()||!lease.Acquire(scene->reservations,ref,index,0x14))return false;
  reference=ref;marker=index;sitting=sit;standing=stand;entering=true;phase=Phase::Out;started=now;return true;
 }
 void Leave(double now){if(seated&&!Busy()){entering=false;phase=Phase::Out;started=now;}}
 Commit Step(double now){
  if(phase==Phase::Out&&now-started>=.12){seated=entering;phase=Phase::In;started=now;if(!seated)lease.Release();return seated?Commit::Sit:Commit::Stand;}
  if(phase==Phase::In&&now-started>=.12){phase=Phase::Idle;if(!seated){reference=0;marker=SIZE_MAX;}}
  return Commit::None;
 }
 float Alpha(double now)const{return phase==Phase::Out?float(std::clamp((now-started)/.12,0.,1.)):phase==Phase::In?float(1-std::clamp((now-started)/.12,0.,1.)):0;}
 void Reset(){lease.Release();scene.reset();reference=0;marker=SIZE_MAX;seated=false;phase=Phase::Idle;}
};
// Original marker/KF alignment supplies the camera target, not a guessed chair
// centre or fixed chair height. The eye anchor is the actual player eye midpoint.
inline bool Targets(const fo3furniture::Slot& slot,const fo3anim::Skeleton& skeleton,const std::array<float,3>& bindEye,Pose& sit,Pose& stand){
 if(!slot.program||slot.sleep||slot.idleMarker)return false;
 const int head=fo3anim::FindBone(skeleton,"Bip01 Head");if(head<0)return false;
 const auto& program=*slot.program;if(!program.Valid()||program.enter.frequency<=0||program.loop.frequency<=0||program.exit.frequency<=0)return false;
 const double duration=(program.enter.stop-program.enter.start)/program.enter.frequency;
 const auto retained=fo3furniture::Root(program.enter,duration);
 auto eye=[&](const fo3anim::Clip& clip,double time,const fo3anim::Transform* root,Pose& out){
  fo3anim::Pose pose;fo3anim::BindClip(skeleton,clip,pose);if(!fo3anim::Sample(skeleton,clip,time,pose,nullptr,fo3anim::RootPolicy::Furniture,root,fo3anim::Playback::Clamp))return false;
  const auto local=fo3anim::Point(pose.delta[size_t(head)],bindEye);out.eye=fo3anim::Point(slot.alignment,local);
  const auto orientation=pose.accumulation>=0?fo3anim::Multiply(slot.alignment,fo3anim::ToMatrix(pose.local[size_t(pose.accumulation)])):slot.alignment;
  out.yaw=std::atan2(-orientation[4],orientation[5]);return true;
 };
 if(!eye(program.loop,0,&retained,sit)||!eye(program.exit,(program.exit.stop-program.exit.start)/program.exit.frequency,&retained,stand))return false;
 // Difference in original actor-space eye height drives VR leg IK.
 fo3anim::Pose pose;fo3anim::BindClip(skeleton,program.loop,pose);if(!fo3anim::Sample(skeleton,program.loop,0,pose,nullptr,fo3anim::RootPolicy::Furniture,&retained))return false;
 sit.bodyDrop=std::max(0.f,bindEye[2]-fo3anim::Point(pose.delta[size_t(head)],bindEye)[2]);return true;
}
}
