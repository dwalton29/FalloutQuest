#pragma once
#include "fo3-weapon-assets.h"
#include "rendering/mesh/fo3-static-nif.h"
#include "fo3-weapon-interaction.h"

namespace fo3weapon {
// Authored anchors in NIF coordinates. Conversion to metres belongs to the
// runtime boundary; neither model origin nor controller origin is a grip.
enum class ReloadFamily { DetachablePistol, DetachableRifle, Cylinder, Tube, InternalMagazine, EnergyCell, Heavy, RequiresAudit };
struct Asset {
  fo3anim::Skeleton nodes;
  std::vector<int> blocks;
  fo3anim::Matrix handToWeapon=fo3anim::Identity();
  std::array<float,3> muzzle{},muzzleForward{},clip{},slide{},support{};
  fo3anim::Matrix weaponToLeft=fo3anim::Identity();
  std::array<std::string,3> reloadSounds{};
  int clipNode=-1,slideNode=-1;
  bool valid=false,hasSupport=false;
  bool Decode(const std::vector<uint8_t>& model,
              const std::vector<uint8_t>& skeleton,
              const std::vector<uint8_t>& aim) {
    *this={};
    fo3anim::Skeleton fp;fo3anim::Clip a;fo3anim::Pose p;
    ModelNodes authored;
    if(!DecodeModelNodes(model,authored)||
       !fo3anim::DecodeSkeleton(skeleton,fp)||!fo3anim::DecodeClip(aim,a)||
       !fo3anim::Sample(fp,a,.1,p))return false;
    nodes=std::move(authored.hierarchy);blocks=std::move(authored.blockBones);
    const int hand=fo3anim::FindBone(fp,"Bip01 R Hand"),weapon=fo3anim::FindBone(fp,"Weapon");
    const int muzzleNode=fo3anim::FindBone(nodes,"ProjectileNode");
    fo3anim::Matrix inverse;
    if(hand<0||weapon<0||muzzleNode<0||!fo3anim::Inverse(p.global[hand],inverse))return false;
    handToWeapon=fo3anim::Multiply(inverse,p.global[weapon]);
    muzzle=fo3anim::Point(nodes.bindGlobal[muzzleNode],{});
    muzzleForward=fo3anim::Point(nodes.bindGlobal[muzzleNode],{0,1,0},true);
    clipNode=fo3anim::FindBone(nodes,"##Clip");
    if(clipNode<0)clipNode=fo3anim::FindBone(nodes,"##Magazine");
    slideNode=fo3anim::FindBone(nodes,"##Slide");
    if(slideNode<0)slideNode=fo3anim::FindBone(nodes,"##Bolt");
    if(clipNode>=0)clip=fo3anim::Point(nodes.bindGlobal[clipNode],{});
    if(slideNode>=0)slide=fo3anim::Point(nodes.bindGlobal[slideNode],{});
    const int left=fo3anim::FindBone(fp,"Bip01 L Hand");
    if(left>=0&&fo3anim::Inverse(p.global[weapon],inverse)) {
      weaponToLeft=fo3anim::Multiply(inverse,p.global[left]);
      // Metacarpal origins give a palm centre, not a wrist through the barrel.
      std::array<float,3> palm{};int count=0;
      for(const char* n:{"Bip01 L Hand01","Bip01 L Hand02","Bip01 L Hand03","Bip01 L Hand04"}) {
        const int i=fo3anim::FindBone(fp,n);if(i<0)continue;
        auto q=fo3anim::Point(p.global[i],{});for(int c=0;c<3;c++)palm[c]+=q[c];++count;
      }
      if(count){for(float&v:palm)v/=count;support=fo3anim::Point(inverse,palm);hasSupport=true;}
    }
    valid=true;return true;
  }
  bool Reload(const std::vector<uint8_t>& bytes) {
    fo3anim::Clip animation;if(!fo3anim::DecodeClip(bytes,animation))return false;
    size_t i=0;for(const auto&key:animation.textKeys) {
      if(key.text.compare(0,7,"Sound: ")==0&&i<reloadSounds.size())reloadSounds[i++]=key.text.substr(7);
    }
    return i==3;
  }
  ReloadFamily ReloadInteraction(const Definition&d)const {
    if(d.animation==4||d.animation==7)return ReloadFamily::EnergyCell;
    if(d.animation>=8)return ReloadFamily::Heavy;
    // Reload A plus authored detachable component and charging-action node.
    // Other A/E/etc animations alone do not prove a magazine/cylinder family.
    if(d.reload==0&&clipNode>=0&&slideNode>=0){
      if(d.animation==3)return ReloadFamily::DetachablePistol;
      if(d.animation==6)return ReloadFamily::DetachableRifle;
    }
    return ReloadFamily::RequiresAudit;
  }
  bool Descendant(uint32_t block,int parent)const {
    if(parent<0||block>=blocks.size())return false;
    for(int i=blocks[block];i>=0;i=nodes.bones[i].parent)if(i==parent)return true;
    return false;
  }
};
// VR access policy, documented centrally in body space (Y up, Z rear).
// Hip follows the authored right thigh origin; shoulder follows solved clavicle.
inline Zone Hip(V thigh,const R&body){return {thigh+fo3vr::Rotate(body,{.10f,0,.035f}),body,{.16f,.20f,.16f}};}
inline Zone Back(V shoulder,const R&body){return {shoulder+fo3vr::Rotate(body,{.04f,-.04f,.15f}),body,{.20f,.22f,.20f}};}
inline Zone Pouch(V pelvis,const R&body){return {pelvis+fo3vr::Rotate(body,{-.22f,-.06f,.015f}),body,{.18f,.18f,.18f}};}
}
