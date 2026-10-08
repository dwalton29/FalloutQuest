#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include "npc/fo3-furniture.h"
#include <cassert>
#include <fstream>
#include <iostream>
bool LoadFalloutMeshFile(const std::string&,std::vector<uint8_t>&,std::string*){return false;}
static std::vector<uint8_t> Read(const std::string& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
 using namespace fo3furniture;
 auto pool=std::make_shared<Reservations>();
 {Lease a,b;assert(a.Acquire(pool,100,1,10));assert(!b.Acquire(pool,100,1,20));assert(b.Acquire(pool,100,0,20));a.Release();assert(a.Acquire(pool,100,1,10));}
 assert(pool->owners.empty());
 fo3pipdata::Placement p;p.x=100;p.y=200;p.z=30;p.rz=1.57079632679f;p.scale=2;
 Marker marker;marker.position={10,0,5};auto world=Alignment(p,marker);
 assert(std::fabs(world[12]-100)<.001f&&std::fabs(world[13]-220)<.001f&&std::fabs(world[14]-40)<.001f);
 assert(Permitted(0x80000002,1,true)&&!Permitted(0x80000002,0,true)&&!Permitted(0x40000002,1,true));
 std::vector<Marker> markers;assert(!DecodeMarkers({},markers));
 if(argc<2){std::cout<<"furniture core tests passed; original assets not supplied\n";return 0;}
 const std::string root=argv[1];
 assert(DecodeMarkers(Read(root+"/furniture/bedtwin01.nif"),markers)&&markers.size()==2);
 assert(markers[0].reference==1&&markers[1].reference==2&&markers[0].orientation==1570&&markers[1].orientation==4712);
 assert(std::fabs(markers[0].position[0]+69.5973f)<.01f);
 auto truncated=Read(root+"/furniture/bedtwin01.nif");truncated.resize(truncated.size()-8);assert(!DecodeMarkers(truncated,markers));
 assert(DecodeMarkers(Read(root+"/furniture/chair01.nif"),markers)&&markers.size()==3);
 assert(markers[0].reference==11&&markers[1].reference==12&&markers[2].reference==14);
 fo3anim::Skeleton skeleton;assert(fo3anim::DecodeSkeleton(Read(root+"/characters/_male/skeleton.nif"),skeleton));
 for(const auto& side:{"left","right"}) {
   fo3anim::Clip enter,loop,exit;const std::string base=root+"/characters/_male/idleanims/";
   assert(fo3anim::DecodeClip(Read(base+"bed"+side+"_enter.kf"),enter));
   assert(fo3anim::DecodeClip(Read(base+"dynamicidle_sleep.kf"),loop));
   assert(fo3anim::DecodeClip(Read(base+"bed"+side+"_exit.kf"),exit));
   fo3anim::Pose a,b;fo3anim::BindClip(skeleton,enter,a);fo3anim::BindClip(skeleton,enter,b);
   assert(fo3anim::Sample(skeleton,enter,enter.stop-enter.start,a));
   assert(fo3anim::Sample(skeleton,enter,enter.stop-enter.start,b,nullptr,fo3anim::RootPolicy::Furniture));
   const int rootBone=b.accumulation;assert(rootBone>=0);assert((a.local[rootBone].translation==std::array<float,3>{}));
   const auto retained=b.local[rootBone];assert(std::fabs(retained.translation[1])>60);
   fo3anim::BindClip(skeleton,loop,b);assert(fo3anim::Sample(skeleton,loop,2,b,nullptr,fo3anim::RootPolicy::Furniture,&retained));
   assert(b.local[rootBone].translation==retained.translation);
   fo3anim::BindClip(skeleton,exit,b);assert(fo3anim::Sample(skeleton,exit,0,b,nullptr,fo3anim::RootPolicy::Furniture,&retained));
   for(int i=0;i<3;++i)assert(std::fabs(b.local[rootBone].translation[i]-retained.translation[i])<2);
   assert(fo3anim::Sample(skeleton,exit,exit.stop-exit.start,b,nullptr,fo3anim::RootPolicy::Furniture));
   std::cout<<enter.name<<" root end "<<retained.translation[0]<<","<<retained.translation[1]<<","<<retained.translation[2]<<" exit "<<b.local[rootBone].translation[0]<<","<<b.local[rootBone].translation[1]<<" cycle "<<enter.cycle<<"\n";
 }
 std::cout<<"original furniture markers and root-motion tests passed\n";
}
