#include "player/fo3-player-seat.h"
#include "rendering/mesh/fo3-static-nif.cpp"
#include <cassert>
#include <fstream>
#include <iostream>
bool LoadFalloutMeshFile(const std::string&,std::vector<uint8_t>&,std::string*){return false;}
static std::vector<uint8_t> Read(const std::string& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static fo3anim::Pose Sample(const fo3anim::Skeleton& skeleton,const fo3anim::Clip& clip,double time,const fo3anim::Transform* root=nullptr){
 fo3anim::Pose pose;fo3anim::BindClip(skeleton,clip,pose);assert(fo3anim::Sample(skeleton,clip,time,pose,nullptr,fo3anim::RootPolicy::Furniture,root,fo3anim::Playback::Clamp));return pose;
}
static void CloseRotation(const fo3anim::Matrix& a,const fo3anim::Matrix& b,float tolerance){
 for(int col=0;col<3;++col)for(int row=0;row<3;++row)assert(std::abs(a[col*4+row]-b[col*4+row])<tolerance);
}
int main(int argc,char** argv){
 // Non-origin accumulated movement and a shared loop in a different frame.
 fo3anim::Skeleton synthetic;fo3anim::Bone root,child;root.name="Bip01";child.name="Bip01 NonAccum";child.parent=0;synthetic.bones={root,child};assert(fo3anim::FinalizeSkeleton(synthetic));
 fo3furniture::Program test;test.enter.accumulationRoot=test.loop.accumulationRoot="Bip01";test.enter.stop=test.loop.stop=1;test.enter.cycle=2;
 fo3anim::Track move;move.bone="Bip01";move.hasTranslation=true;move.base.translation={20,50,0};test.enter.tracks.push_back(move);
 fo3anim::Track turn;turn.bone="Bip01 NonAccum";turn.hasRotation=true;turn.base.rotation={.70710678f,0,0,.70710678f};test.enter.tracks.push_back(turn);turn.base.rotation={1,0,0,0};test.loop.tracks={turn};test.Prepare();
 auto end=Sample(synthetic,test.enter,1);auto retained=fo3furniture::Root(test.enter,1);const auto corrected=test.LoopRoot(retained);auto loop=Sample(synthetic,test.loop,0,&corrected);
 CloseRotation(end.global[1],loop.global[1],.0001f);assert(corrected.translation==retained.translation&&retained.rotation==fo3anim::Transform{}.rotation);
 for(int k=0;k<3;++k)assert(std::abs(end.global[1][12+k]-loop.global[1][12+k])<.0001f);
 test.loop.tracks.clear();test.Prepare();assert(test.loopHeading==0); // Unrelated/sparse programs get no invented turn.
 if(argc<3){std::cout<<"Furniture continuation frame and accumulated pivot tests passed\n";return 0;}
 const std::string base=argv[2];fo3anim::Skeleton skeleton;assert(fo3anim::DecodeSkeleton(Read(argv[1]),skeleton));
 struct Case{const char* model;const char* enter;const char* loop;const char* exit;unsigned marker;};
 const Case cases[]={
  {"officechairruined01.nif","chair_leftenter.kf","dynamicidle_chairsit.kf","chair_leftexit.kf",11},
  {"officechairruined01.nif","chair_rightenter.kf","dynamicidle_chairsit.kf","chair_rightexit.kf",12},
  {"officechairruined01.nif","chair_forwardenter.kf","dynamicidle_chairsit.kf","chair_forwardexit.kf",14},
  {"bedtwin01.nif","bedleft_enter.kf","dynamicidle_sleep.kf","bedleft_exit.kf",1},
  {"bedtwin01.nif","bedright_enter.kf","dynamicidle_sleep.kf","bedright_exit.kf",2},
  {"floorsitmarker.nif","sitfloorenter.kf","sitfloordynamicidle.kf","sitfloorexit.kf",17}};
 fo3anim::Matrix chairFacing{},bedFacing{};float chairCameraYaw=0;
 for(const auto& c:cases){
  fo3furniture::Program program;assert(fo3anim::DecodeClip(Read(base+"/"+c.enter),program.enter));assert(fo3anim::DecodeClip(Read(base+"/"+c.loop),program.loop));assert(fo3anim::DecodeClip(Read(base+"/"+c.exit),program.exit));assert(program.Valid());program.Prepare();
  std::vector<fo3furniture::Marker> markers;assert(fo3furniture::DecodeMarkers(Read(base+"/"+c.model),markers));const auto marker=std::find_if(markers.begin(),markers.end(),[&](const auto& m){return m.reference==c.marker;});assert(marker!=markers.end());
  const double duration=(program.enter.stop-program.enter.start)/program.enter.frequency;
  const auto entry=Sample(skeleton,program.enter,duration);const auto root=fo3furniture::Root(program.enter,duration);const auto loopRoot=program.LoopRoot(root);
  const auto idle=Sample(skeleton,program.loop,0,&loopRoot),exit=Sample(skeleton,program.exit,0,&root);
  assert(root.translation==loopRoot.translation);const int pivot=fo3anim::FindBone(skeleton,"Bip01 NonAccum"),head=fo3anim::FindBone(skeleton,"Bip01 Head");assert(pivot>=0&&head>=0);
  CloseRotation(entry.global[pivot],idle.global[pivot],.01f);
  // Exit is already authored in the approach frame; applying the continuation
  // heading again here would introduce the same turn on standing up.
  CloseRotation(exit.global[pivot],idle.global[pivot],.02f);
  for(int k=0;k<3;++k)assert(std::abs(entry.global[head][12+k]-idle.global[head][12+k])<1.f);
  for(float yaw:{0.f,.73f,-1.91f}){
   fo3pipdata::Placement placement;placement.x=211;placement.y=-531;placement.z=47;placement.rz=yaw;
   const auto alignment=fo3furniture::Alignment(placement,*marker);CloseRotation(fo3anim::Multiply(alignment,entry.global[pivot]),fo3anim::Multiply(alignment,idle.global[pivot]),.01f);
   const auto before=fo3anim::Point(alignment,fo3anim::Point(entry.global[head],{})),after=fo3anim::Point(alignment,fo3anim::Point(idle.global[head],{}));for(int k=0;k<3;++k)assert(std::abs(before[k]-after[k])<1.f);
  }
  const auto facing=fo3anim::Multiply(fo3furniture::Alignment({},*marker),idle.global[pivot]);
  if(c.marker==11||c.marker==12||c.marker==14){
   fo3furniture::Slot slot;slot.program=std::make_shared<fo3furniture::Program>(program);slot.alignment=fo3furniture::Alignment({},*marker);
   fo3seat::Pose sit,stand;assert(fo3seat::Targets(slot,skeleton,fo3anim::Point(skeleton.bindGlobal[head],{}),sit,stand));
   if(c.marker==11)chairCameraYaw=sit.yaw;else assert(std::abs(std::sin(sit.yaw-chairCameraYaw))<.005f&&std::cos(sit.yaw-chairCameraYaw)>.99f);
   const auto expected=fo3anim::Point(slot.alignment,fo3anim::Point(entry.global[head],{}));for(int k=0;k<3;++k)assert(std::abs(sit.eye[k]-expected[k])<1.f);
  }
  if(c.marker==11)chairFacing=facing;if(c.marker==12||c.marker==14)CloseRotation(chairFacing,facing,.01f);
  if(c.marker==1)bedFacing=facing;if(c.marker==2)CloseRotation(bedFacing,facing,.01f);
  std::cout<<c.enter<<" -> "<<c.loop<<" -> "<<c.exit<<" heading "<<program.loopHeading<<" continuous at marker "<<c.marker<<"\n";
 }
}
