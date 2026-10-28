#pragma once
#include "fo3-actor-animation.h"
#include "fo3-npc-state.h"

namespace fo3npc {
inline bool Locomotion(Animation a){
  return a==Animation::Walk||a==Animation::Run||a==Animation::StepLeft||
         a==Animation::StepRight||a==Animation::StepBack;
}
inline Animation WeaponAnimation(const RuntimeState& s,double now){
  return s.reloadUntil>0?Animation::Reload:
      s.pendingAttack||now<s.actionUntil?Animation::Attack:Animation::Aim;
}
inline double ReloadClipTime(const RuntimeState& s,const fo3anim::Clip& clip,double now){
  const double duration=s.reloadUntil-s.actionStart;
  if(duration<=0||clip.frequency<=0)return 0;
  return std::clamp((now-s.actionStart)/duration,0.,1.)*
      std::max(0.f,clip.stop-clip.start)/clip.frequency;
}
struct CombatPose {
  fo3anim::Pose weapon,neutral,direction,pitched;
  std::vector<fo3anim::Transform> from;
  Animation selected=Animation::Count;
  uint64_t instance=0;
  int aimDirection=0;
  double transition=-1;
};
// Quest transition bound retained from the existing actor bridge, not an
// interpretation of the source Blend/BlendIn text-key commands.
inline bool LayerWeapon(const fo3anim::Skeleton& skeleton,fo3anim::Pose& base,
    const std::array<fo3anim::Clip,size_t(Animation::Count)>& animations,
    const RuntimeState& runtime,double now,double idleTime,CombatPose& state){
  const auto selected=WeaponAnimation(runtime,now);
  const auto& clip=animations[size_t(selected)];
  if(clip.tracks.empty())return false;
  const bool instance=state.instance!=runtime.equippedWeapon;
  const bool changed=state.selected!=selected||instance;
  if(changed){
    state.from=state.weapon.local;state.transition=now;
    fo3anim::BindClip(skeleton,clip,state.weapon);
    state.selected=selected;state.instance=runtime.equippedWeapon;
    if(instance){state.neutral={};state.direction={};state.aimDirection=0;}
  }
  // Retriggering a shot restarts only the weapon sample, never the gait or
  // another full-body crossfade. Its clock is the committed action's clock.
  const double time=selected==Animation::Aim?idleTime:selected==Animation::Reload?
      ReloadClipTime(runtime,clip,now):std::max(0.,now-runtime.actionStart);
  if(!fo3anim::Sample(skeleton,clip,time,state.weapon,nullptr,fo3anim::RootPolicy::Locomotion,
      nullptr,selected==Animation::Reload?fo3anim::Playback::Clamp:fo3anim::Playback::Sequence))return false;
  double blend=DialoguePolicy::BlendSeconds;
  if(selected==Animation::Attack)for(const auto& key:clip.textKeys)
    if(key.text=="Hit")blend=std::min(blend,std::max(0.,double(key.time-clip.start)/clip.frequency));
  if(changed&&instance)state.from.clear();
  if(blend>0&&now-state.transition<blend&&state.from.size()==state.weapon.local.size()){
    const float weight=float(std::clamp((now-state.transition)/blend,0.,1.));
    for(size_t i=0;i<state.from.size();++i)
      if(int(i)!=state.weapon.accumulation)
        state.weapon.local[i]=fo3anim::MixTransform(state.from[i],state.weapon.local[i],weight);
  }
  // Original AimUp/AimDown are absolute poses. Apply their delta from Aim to
  // the live weapon action, preserving recoil and its authored event clock.
  const fo3anim::Pose* overlay=&state.weapon;
  if(selected!=Animation::Reload&&std::abs(runtime.aimPitch)>.0001f){
    const int sign=runtime.aimPitch>0?1:-1;
    const auto& directional=animations[size_t(sign>0?Animation::AimUp:Animation::AimDown)];
    const auto& neutral=animations[size_t(Animation::Aim)];
    if(!directional.tracks.empty()&&!neutral.tracks.empty()){
      if(state.neutral.local.empty())fo3anim::BindClip(skeleton,neutral,state.neutral);
      if(sign!=state.aimDirection){fo3anim::BindClip(skeleton,directional,state.direction);state.aimDirection=sign;}
      if(fo3anim::Sample(skeleton,neutral,idleTime,state.neutral)&&
         fo3anim::Sample(skeleton,directional,idleTime,state.direction)){
        state.pitched.local=state.weapon.local;state.pitched.trackBones=state.weapon.trackBones;
        state.pitched.accumulation=state.weapon.accumulation;
        // Fallout3.exe defaults fAimUpDegrees/fAimDownDegrees:Combat = 90.
        fo3anim::ApplyAimDelta(state.pitched,state.neutral,state.direction,
                              std::min(1.f,std::abs(runtime.aimPitch)/1.57079632679f));
        overlay=&state.pitched;
      }
    }
  }
  return fo3anim::OverlayPriority(skeleton,base,clip,*overlay);
}
} // namespace fo3npc
