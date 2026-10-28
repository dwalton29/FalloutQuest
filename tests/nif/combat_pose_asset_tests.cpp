#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include "../../app/src/main/cpp/npc/fo3-combat-animation.h"
#include <cassert>
#include <fstream>
#include <iostream>
bool LoadFalloutMeshFile(const std::string& path,std::vector<uint8_t>& out,std::string*){
  std::ifstream f(path,std::ios::binary);out.assign(std::istreambuf_iterator<char>(f),{});return !out.empty();
}
static std::vector<uint8_t> Read(const std::string& p){std::vector<uint8_t> b;assert(LoadFalloutMeshFile(p,b,nullptr));return b;}
int main(int argc,char** argv){
  if(argc!=2)return 77;
  using namespace fo3anim;using namespace fo3npc;
  const std::string root=std::string(argv[1])+"/";
  Skeleton skeleton;assert(DecodeSkeleton(Read(root+"skeleton.nif"),skeleton));
  std::array<Clip,size_t(Animation::Count)> clips;
  for(const auto& entry:{std::pair<Animation,const char*>{Animation::Walk,"mtforward.kf"},
      {Animation::Aim,"1hpaim.kf"},{Animation::AimUp,"1hpaimup.kf"},
      {Animation::AimDown,"1hpaimdown.kf"},{Animation::Reload,"1hpreloada.kf"},
      {Animation::Attack,"1hpattackleft.kf"}})
    assert(DecodeClip(Read(root+entry.second),clips[size_t(entry.first)]));
  const int pelvis=FindBone(skeleton,"Bip01 Pelvis"),spine=FindBone(skeleton,"Bip01 Spine2");
  const int hand=FindBone(skeleton,"Bip01 R Hand");assert(pelvis>=0&&spine>=0&&hand>=0);
  Pose walk;const auto& gait=clips[size_t(Animation::Walk)];assert(Sample(skeleton,gait,.4,walk));
  assert(walk.priorities[pelvis]==30);
  Pose reload;const auto& loading=clips[size_t(Animation::Reload)];assert(Sample(skeleton,loading,.4,reload));
  assert(loading.weight==1&&reload.priorities[pelvis]==27&&reload.priorities[spine]==65);
  RuntimeState runtime;runtime.activity=Activity::Combat;runtime.equippedWeapon=1;runtime.reloadUntil=(loading.stop-loading.start)/loading.frequency;
  CombatPose state;assert(LayerWeapon(skeleton,walk,clips,runtime,.4,.4,state));
  Pose expected;assert(Sample(skeleton,gait,.4,expected));
  assert(walk.local[pelvis].translation==expected.local[pelvis].translation);
  assert(walk.local[pelvis].rotation==expected.local[pelvis].rotation);
  assert(walk.local[hand].rotation==reload.local[hand].rotation);
  // Actual KF AimUp reaches the authored pose at the executable's 90 degree default.
  runtime.reloadUntil=0;runtime.aimPitch=1.57079632679f;CombatPose aimedState;
  assert(Sample(skeleton,gait,.4,walk)&&LayerWeapon(skeleton,walk,clips,runtime,.4,.4,aimedState));
  Pose up;assert(Sample(skeleton,clips[size_t(Animation::AimUp)],.4,up));
  float dot=0;for(int i=0;i<4;++i)dot+=walk.local[hand].rotation[i]*up.local[hand].rotation[i];
  assert(std::abs(std::abs(dot)-1)<.0001f);
  assert(walk.local[pelvis].rotation==expected.local[pelvis].rotation);
  assert(walk.local[walk.accumulation].translation==Transform{}.translation);
  std::cout<<"Original pistol/locomotion KF priorities, concurrent reload, source AimUp and in-place root passed\n";
}
