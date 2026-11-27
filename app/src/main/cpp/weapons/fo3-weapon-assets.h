#pragma once
#include "fo3-weapon-data.h"
#include "npc/fo3-actor-animation.h"

namespace fo3weapon {
// Copy the first-person finger rotations onto the avatar's own joint lengths.
// Deltas are in avatar bind space with the wrist fixed; arm IK moves it later.
struct GripJoint {
  int source=-1,avatar=-1,parent=-1;
  std::string name;
  fo3anim::Matrix sourceBindInverse,posed,delta;
};
struct GripFingers {
  int wrist=-1;
  std::vector<GripJoint> joints;
  void Bind(const fo3anim::Skeleton& source,const fo3anim::Skeleton& avatar);
  void Sample(const fo3anim::Skeleton& avatar,const std::vector<fo3anim::Transform>& locals);
};
struct Attachment {
  fo3anim::Matrix weaponInRight = fo3anim::Identity();
  fo3anim::Matrix rightInWeapon = fo3anim::Identity();
  fo3anim::Matrix leftInWeapon = fo3anim::Identity();
  bool support = false;
};
// Samples the original aim KF against its original first-person skeleton.
// Matrices remain in game coordinates; renderer conjugates them exactly once.
bool BuildAttachment(const fo3anim::Skeleton &skeleton,
                     const fo3anim::Clip &aim, bool longGun, Attachment &out);
enum class Part { Body, Magazine, Slide, Bolt };
struct ModelNodes {
  fo3anim::Skeleton hierarchy;
  std::vector<int> blockBones;
  int muzzle = -1, magazine = -1, slide = -1, bolt = -1;
  Part MeshPart(uint32_t shapeBlock) const;
  // ProjectileNode's authored local +Y is forward, not a grip-to-muzzle ray.
  std::array<float, 3> MuzzleForward() const;
};
bool DecodeModelNodes(const std::vector<uint8_t> &bytes, ModelNodes &out,bool requireMuzzle=true);
// No filename/model-name classification: these are FO3 WEAP animation types.
// Require the corresponding first-person aim clip when preparing the model.
std::string AimPath(const Definition &definition);
}
