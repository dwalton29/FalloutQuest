#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include "fo3-actor-animation.h"
#include <cassert>
#include <fstream>
#include <iostream>
bool LoadFalloutMeshFile(const std::string& path,std::vector<uint8_t>& out,std::string*){std::ifstream f(path,std::ios::binary);out.assign(std::istreambuf_iterator<char>(f),{});return !out.empty();}
static std::vector<uint8_t> Read(const std::string& path){std::vector<uint8_t> out;LoadFalloutMeshFile(path,out,nullptr);return out;}
int main(int argc,char** argv){
 if(argc!=2)return 77;const std::string root=std::string(argv[1])+"/";
 fo3anim::Skeleton skeleton;assert(fo3anim::DecodeSkeleton(Read(root+"skeleton.nif"),skeleton));
 const std::pair<const char*,const char*> expected[]={
 {"1hpaim.kf","Aim"},{"1hpattackright.kf","AttackRight"},{"1hpreloada.kf","ReloadA"},
 {"2hraim.kf","Aim"},{"2hrattackright.kf","AttackRight"},{"2hrreloada.kf","ReloadA"},
 {"2haaim.kf","Aim"},{"2haattackloop.kf","AttackLoop"},{"2hareloada.kf","ReloadA"},
 {"1hmattackright_a.kf","AttackRight_A"},{"deathpose1.kf","SpecialIdle_DeathPose1"}};
 for(const auto& e:expected){fo3anim::Clip clip;assert(fo3anim::DecodeClip(Read(root+e.first),clip)&&clip.name==e.second);
  fo3anim::Pose pose;fo3anim::BindClip(skeleton,clip,pose);size_t matched=0;
  for(size_t i=0;i<pose.trackBones.size();++i){if(pose.trackBones[i]>=0)++matched;else std::cout<<"Weapon-only track "<<clip.tracks[i].bone<<"\n";}
  assert(matched>=50);for(int frame=0;frame<120;++frame)assert(fo3anim::Sample(skeleton,clip,frame/30.,pose));
  std::cout<<e.first<<" matched="<<matched<<" tracks="<<clip.tracks.size()<<" duration="<<clip.stop-clip.start<<"\n";
 }std::cout<<"Original NPC combat/death animation assets passed\n";
}
