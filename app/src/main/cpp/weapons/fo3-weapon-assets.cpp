#include "fo3-weapon-assets.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace fo3weapon {
namespace {
bool Finite(const fo3anim::Matrix &m) {
  for (float f : m) if (!std::isfinite(f)) return false;
  return true;
}
}
void GripFingers::Bind(const fo3anim::Skeleton& source,const fo3anim::Skeleton& avatar){
  joints.clear();wrist=fo3anim::FindBone(avatar,"Bip01 L Hand");
  const int sourceWrist=fo3anim::FindBone(source,"Bip01 L Hand");
  if(wrist<0||sourceWrist<0)return;
  // Visit parent before child even when the two NIFs order blocks differently.
  auto children=[&](auto&& visit,int bone,int parent,int sourceParent)->void{
    for(size_t i=0;i<avatar.bones.size();++i){
      const auto& b=avatar.bones[i];
      if(b.parent!=bone)continue;
      const bool digit=b.name.rfind("Bip01 L Finger",0)==0||b.name.rfind("Bip01 L Thumb",0)==0||
                       b.name.rfind("Bip01 L Hand0",0)==0;
      if(!digit)continue;
      const int match=fo3anim::FindBone(source,b.name);
      if(match<0)continue;
      GripJoint joint{};
      // The first-person NIF inserts metacarpals between Hand and Finger1–4.
      // Fold those joints into the segment represented by the avatar knuckle.
      int ancestor=match;
      for(;ancestor>=0&&ancestor!=sourceParent;ancestor=source.bones[ancestor].parent)
        joint.sourceChain.push_back(ancestor);
      if(ancestor!=sourceParent)continue;
      std::reverse(joint.sourceChain.begin(),joint.sourceChain.end());
      auto rest=fo3anim::Identity();
      for(int index:joint.sourceChain){
        auto bind=source.bones[index].bind;bind.translation={};bind.scale=1;
        rest=fo3anim::Multiply(rest,fo3anim::ToMatrix(bind));
      }
      if(!fo3anim::Inverse(rest,joint.sourceBindInverse))continue;
      joint.avatar=int(i);joint.parent=parent;joint.name=b.name;
      for(char& ch:joint.name)if(ch>='A'&&ch<='Z')ch=char(ch-'A'+'a');
      const int index=int(joints.size());joints.push_back(std::move(joint));visit(visit,int(i),index,match);
    }
  };
  children(children,wrist,-1,sourceWrist);
}
void GripFingers::Sample(const fo3anim::Skeleton& avatar,const std::vector<fo3anim::Transform>& locals){
  for(auto& joint:joints){
    auto segment=fo3anim::Identity();
    for(int index:joint.sourceChain){
      auto rotation=locals.at(index);rotation.translation={};rotation.scale=1;
      segment=fo3anim::Multiply(segment,fo3anim::ToMatrix(rotation));
    }
    const auto change=fo3anim::Multiply(joint.sourceBindInverse,segment);
    const auto local=fo3anim::Multiply(fo3anim::ToMatrix(avatar.bones[joint.avatar].bind),change);
    const auto& parent=joint.parent<0?avatar.bindGlobal[wrist]:joints[joint.parent].posed;
    joint.posed=fo3anim::Multiply(parent,local);
    joint.delta=fo3anim::Multiply(joint.posed,avatar.inverseBind[joint.avatar]);
  }
}
bool BuildAttachment(const fo3anim::Skeleton &skeleton,
                     const fo3anim::Clip &aim, bool longGun, Attachment &out) {
  const int right = fo3anim::FindBone(skeleton, "Bip01 R Hand");
  const int left = fo3anim::FindBone(skeleton, "Bip01 L Hand");
  const int weapon = fo3anim::FindBone(skeleton, "Weapon");
  if (right < 0 || weapon < 0 || (longGun && left < 0)) return false;
  // A missing KF track must not quietly fall back to a fabricated bind offset.
  bool rightTrack = false, weaponTrack = false, leftTrack = false;
  for (const auto &track : aim.tracks) {
    rightTrack |= track.bone == "Bip01 R Hand";
    leftTrack |= track.bone == "Bip01 L Hand";
    weaponTrack |= track.bone == "Weapon";
  }
  if (!rightTrack || !weaponTrack || (longGun && !leftTrack)) return false;
  fo3anim::Pose pose;
  if (!fo3anim::Sample(skeleton, aim, 0, pose)) return false;
  fo3anim::Matrix inverseRight, inverseWeapon;
  if (!fo3anim::Inverse(pose.global.at(right), inverseRight) ||
      !fo3anim::Inverse(pose.global.at(weapon), inverseWeapon)) return false;
  Attachment next;
  next.weaponInRight = fo3anim::Multiply(inverseRight, pose.global.at(weapon));
  next.rightInWeapon = fo3anim::Multiply(inverseWeapon, pose.global.at(right));
  if (longGun) {
    next.leftInWeapon = fo3anim::Multiply(inverseWeapon, pose.global.at(left));
    next.support = true;
  }
  if (!Finite(next.weaponInRight) || !Finite(next.rightInWeapon) ||
      !Finite(next.leftInWeapon)) return false;
  out = next;
  return true;
}
Part ModelNodes::MeshPart(uint32_t shapeBlock) const {
  if (shapeBlock >= blockBones.size()) return Part::Body;
  int bone = blockBones[shapeBlock];
  size_t remaining = hierarchy.bones.size();
  while (bone >= 0 && static_cast<size_t>(bone) < hierarchy.bones.size() && remaining--) {
    if (bone == magazine) return Part::Magazine;
    if (bone == slide) return Part::Slide;
    if (bone == bolt) return Part::Bolt;
    bone = hierarchy.bones[bone].parent;
  }
  return Part::Body;
}
std::array<float, 3> ModelNodes::MuzzleForward() const {
  if (muzzle < 0 || static_cast<size_t>(muzzle) >= hierarchy.bindGlobal.size()) return {};
  auto v = fo3anim::Point(hierarchy.bindGlobal[muzzle], {0, 1, 0}, true);
  const float n = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
  if (!std::isfinite(n) || n < 1e-6f) return {};
  for (float &f : v) f /= n;
  return v;
}
bool DecodeModelNodes(const std::vector<uint8_t> &bytes, ModelNodes &out,bool requireMuzzle) {
  ModelNodes next;
  if (!fo3anim::DecodeModelHierarchy(bytes, next.hierarchy, next.blockBones)) return false;
  next.muzzle = fo3anim::FindBone(next.hierarchy, "ProjectileNode");
  if(next.muzzle<0)next.muzzle=fo3anim::FindBone(next.hierarchy,"##ProjectileNode");
  next.magazine = fo3anim::FindBone(next.hierarchy, "##Clip");
  if(next.magazine<0)next.magazine=fo3anim::FindBone(next.hierarchy,"##Magazine");
  next.slide = fo3anim::FindBone(next.hierarchy, "##Slide");
  next.bolt = fo3anim::FindBone(next.hierarchy, "##Bolt");
  if(!requireMuzzle){out=std::move(next);return true;}
  if (next.muzzle < 0) return false;
  const auto direction = next.MuzzleForward();
  if (direction[0] == 0 && direction[1] == 0 && direction[2] == 0) return false;
  out = std::move(next);
  return true;
}
std::string AimPath(const Definition &definition) {
  const char *prefix = nullptr;
  switch (definition.animation) {
  case 2: prefix = "2hm"; break;
  case 3: case 4: prefix = "1hp"; break;
  case 5: case 7: prefix = "2hr"; break;
  case 6: prefix = "2ha"; break;
  case 8: prefix = "2hh"; break;
  case 9: prefix = "2hl"; break;
  default: return {};
  }
  return std::string("meshes/characters/_1stperson/") + prefix + "aim.kf";
}
}
