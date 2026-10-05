#include "fo3-weapon-assets.h"
#include <cmath>

namespace fo3weapon {
namespace {
bool Finite(const fo3anim::Matrix &m) {
  for (float f : m) if (!std::isfinite(f)) return false;
  return true;
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
bool DecodeModelNodes(const std::vector<uint8_t> &bytes, ModelNodes &out) {
  ModelNodes next;
  if (!fo3anim::DecodeModelHierarchy(bytes, next.hierarchy, next.blockBones)) return false;
  next.muzzle = fo3anim::FindBone(next.hierarchy, "ProjectileNode");
  next.magazine = fo3anim::FindBone(next.hierarchy, "##Clip");
  next.slide = fo3anim::FindBone(next.hierarchy, "##Slide");
  next.bolt = fo3anim::FindBone(next.hierarchy, "##Bolt");
  if (next.muzzle < 0) return false;
  const auto direction = next.MuzzleForward();
  if (direction[0] == 0 && direction[1] == 0 && direction[2] == 0) return false;
  out = std::move(next);
  return true;
}
std::string AimPath(const Definition &definition) {
  const char *prefix = nullptr;
  switch (definition.animation) {
  case 3: case 4: prefix = "1hp"; break;
  case 5: case 7: prefix = "2hr"; break;
  case 6: prefix = "2ha"; break;
  default: return {};
  }
  return std::string("meshes/characters/_1stperson/") + prefix + "aim.kf";
}
}
