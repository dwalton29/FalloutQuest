#include "player/fo3-player-seat.h"
#include "rendering/mesh/fo3-static-nif.cpp"
#include <cassert>
#include <fstream>
#include <iostream>
bool LoadFalloutMeshFile(const std::string&,std::vector<uint8_t>&,std::string*){return false;}
static std::vector<uint8_t> Read(const char* p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
 auto scene=std::make_shared<fo3furniture::Scene>();scene->slots.resize(1);scene->slots[0].reference=100;
 fo3seat::State state;state.scene=scene;fo3furniture::Lease npc;assert(npc.Acquire(scene->reservations,100,0,6));
 fo3seat::Pose sit{{1,.8f,2},1,.6f},stand{{1,1.6f,2},1,0};assert(!state.Begin(100,0,sit,stand,0));npc.Release();
 assert(state.Begin(100,0,sit,stand,0)&&state.Busy()&&!state.seated&&state.Alpha(0)==0);
 assert(state.Step(.1)==fo3seat::State::Commit::None&&state.Alpha(.1)>.8f);
 assert(state.Step(.12)==fo3seat::State::Commit::Sit&&state.seated&&state.Alpha(.12)==1);
 assert(!npc.Acquire(scene->reservations,100,0,6));state.Step(.25);assert(!state.Busy()&&state.seated&&state.Alpha(.25)==0);
 state.Leave(.3);assert(state.Step(.43)==fo3seat::State::Commit::Stand&&!state.seated);assert(npc.Acquire(scene->reservations,100,0,6));npc.Release();state.Step(.56);assert(!state.Active()&&state.reference==0);
 assert(state.Begin(100,0,sit,stand,1));state.Reset();assert(!state.Active()&&!state.scene&&scene->reservations->owners.empty());
 if(argc>5){
  std::vector<fo3furniture::Marker> markers;assert(fo3furniture::DecodeMarkers(Read(argv[1]),markers));
  fo3anim::Skeleton skeleton;assert(fo3anim::DecodeSkeleton(Read(argv[2]),skeleton));
  auto program=std::make_shared<fo3furniture::Program>();assert(fo3anim::DecodeClip(Read(argv[3]),program->enter));assert(fo3anim::DecodeClip(Read(argv[4]),program->loop));assert(fo3anim::DecodeClip(Read(argv[5]),program->exit));assert(program->Valid());
  auto marker=std::find_if(markers.begin(),markers.end(),[](const auto& m){return m.reference==11;});assert(marker!=markers.end());
  const int head=fo3anim::FindBone(skeleton,"Bip01 Head");assert(head>=0);const auto eye=fo3anim::Point(skeleton.bindGlobal[head],{});
  fo3furniture::Slot slot;slot.reference=100;slot.marker=*marker;slot.program=program;slot.alignment=fo3furniture::Alignment(slot.placement,*marker);
  assert(fo3seat::Targets(slot,skeleton,eye,sit,stand));
  std::cout<<"Original chair marker="<<unsigned(marker->reference)<<" sitZ="<<sit.eye[2]<<" standZ="<<stand.eye[2]<<" drop="<<sit.bodyDrop<<"\n";
  assert(sit.eye[2]<stand.eye[2]-10&&sit.bodyDrop>10&&sit.bodyDrop<100);
  auto moved=slot;moved.placement.x=100;moved.placement.y=200;moved.placement.rz=1.57079633f;moved.alignment=fo3furniture::Alignment(moved.placement,moved.marker);
  fo3seat::Pose rotated,exit;assert(fo3seat::Targets(moved,skeleton,eye,rotated,exit));
  const auto inverse=fo3anim::Multiply(fo3furniture::Placement(moved.placement),fo3anim::Identity());const auto expected=fo3anim::Point(inverse,sit.eye);for(int k=0;k<3;++k)assert(std::abs(rotated.eye[k]-expected[k])<.001f);
  assert(std::abs(rotated.bodyDrop-sit.bodyDrop)<.001f);
 }
 std::cout<<"Player seat reservation, blink commit, stand/reset and authored alignment tests passed\n";
}
