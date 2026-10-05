#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include "player/fo3-vr-body.h"
#include "ui/pipboy/fo3-pipboy-mesh.h"
#include <cassert>
#include <fstream>
#include <iostream>
bool LoadFalloutMeshFile(const std::string&p,std::vector<uint8_t>&b,std::string*r){
 std::ifstream f(p,std::ios::binary);b.assign(std::istreambuf_iterator<char>(f),{});if(r)*r=p;return !b.empty();
}
using namespace fo3vr;
static V Convert(V p){return {p.x/70,p.z/70-1.55f,-p.y/70};}
static V BonePoint(const fo3anim::Skeleton&s,int i){const auto&m=s.bindGlobal[i];return Convert({m[12],m[13],m[14]});}
static V Palm(const char*path,const char*name){
 std::vector<Fo3StaticNifMesh>meshes;assert(LoadFo3StaticNifMeshes(path,meshes));V total{};float weight=0;
 for(const auto&m:meshes)for(uint32_t vertex:m.indices){
  float w=0;for(int k=0;k<4;k++){size_t at=vertex*4+k;auto bone=m.skinBoneIndices[at];
   if(bone<m.skinBones.size()&&m.skinBones[bone].name==name)w+=m.skinBoneWeights[at];}
  V p{m.positions[vertex*3],m.positions[vertex*3+1],m.positions[vertex*3+2]};total=total+Convert(p)*w;weight+=w;
 }
 assert(weight>0);return total*(1/weight);
}
int main(int argc,char**argv){
 if(argc!=7){std::cout<<"Supply skeleton, left glove, right hand, PipBoyArm, left eye, right eye for original-asset validation\n";return 77;}
 std::vector<uint8_t>bytes;assert(LoadFalloutMeshFile(argv[1],bytes,nullptr));fo3anim::Skeleton skeleton;assert(fo3anim::DecodeSkeleton(bytes,skeleton));
 int head=fo3anim::FindBone(skeleton,"Bip01 Head");assert(head>=0);V eyeSum{};
 for(int a=5;a<=6;a++){
  std::vector<Fo3StaticNifMesh>eyes;assert(LoadFo3StaticNifMeshes(argv[a],eyes));
  assert(eyes.size()==1);const auto&m=eyes[0];V lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
  for(size_t i=0;i<m.positions.size();i+=3){V p{m.positions[i],m.positions[i+1],m.positions[i+2]};lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
  V c=(lo+hi)*.5f;auto eye=fo3anim::Point(skeleton.bindGlobal[head],{c.x,c.y,c.z});eyeSum=eyeSum+Convert({eye[0],eye[1],eye[2]});
 }
 V anchor=eyeSum*.5f;assert(anchor.y>BonePoint(skeleton,head).y+.08f);
 std::vector<Fo3StaticNifMesh>pip;assert(LoadFo3StaticNifMeshes(argv[4],pip));fo3pip::Surface screen;
 for(const auto&m:pip)if(fo3pip::Screen(m))screen=fo3pip::Inspect(m);
 assert(screen.valid);int mount=fo3anim::FindBone(skeleton,"Bip01 L ForeTwist");assert(mount>=0);
 auto bind=fo3pip::BindInRenderCoordinates(skeleton.bindGlobal[mount],70,-1.55f,0);
 V centre=Convert({screen.center.x,screen.center.y,screen.center.z});auto bc=fo3anim::Point(bind,{centre.x,centre.y,centre.z});V bindCentre{bc[0],bc[1],bc[2]};
 for(int side=0;side<2;side++){
  std::string prefix=side==0?"Bip01 L ":"Bip01 R ";int clav=fo3anim::FindBone(skeleton,prefix+"Clavicle"),upper=fo3anim::FindBone(skeleton,prefix+"UpperArm"),fore=fo3anim::FindBone(skeleton,prefix+"Forearm"),hand=fo3anim::FindBone(skeleton,prefix+"Hand"),twist=fo3anim::FindBone(skeleton,prefix+"ForeTwist");
  assert(clav>=0&&upper>=0&&fore>=0&&hand>=0&&twist>=0);
  assert(skeleton.bones[upper].parent==clav&&skeleton.bones[fore].parent==upper&&skeleton.bones[hand].parent==fore&&skeleton.bones[twist].parent==fore);
  assert(Length(BonePoint(skeleton,twist)-BonePoint(skeleton,fore))<1e-4f);
  ArmRig rig{BonePoint(skeleton,clav),BonePoint(skeleton,upper),BonePoint(skeleton,fore),BonePoint(skeleton,hand),Palm(argv[side+2],(prefix+"Hand").c_str()),side==0,true};ArmState state;
  assert(Length(rig.palm-rig.wrist)<.15f);
  int middle=fo3anim::FindBone(skeleton,prefix+"Finger2"),little=fo3anim::FindBone(skeleton,prefix+"Finger4");assert(middle>=0&&little>=0);
  V across=Unit(BonePoint(skeleton,middle)-BonePoint(skeleton,little));
  V inward=Unit(Cross(across,BonePoint(skeleton,middle)-rig.wrist));if(side==1)inward=inward*-1;
  V gripAcross{0,0,-1},gripInward{side==0?-1.f:1.f,0,0};
  R mapping=FrameRotation(across,inward,gripAcross,gripInward);
  assert(Length(Rotate(mapping,across)-gripAcross)<1e-4f);
  assert(Length(Rotate(mapping,inward)-gripInward)<1e-4f);
  V gripTarget{side==0?-.2f:.2f,anchor.y-.25f,anchor.z-.25f};
  auto gripPose=SolveArm(rig,gripTarget,mapping,state,.014f);assert(gripPose.valid&&gripPose.error<2e-4f);
  for(float angle:{-Pi,-Pi/2,0.f,Pi/2,Pi}){
   R handRotation=Axis({0,0,1},angle);V target{side==0?-.2f:.2f,anchor.y-.25f,anchor.z-.3f};
   auto pose=SolveArm(rig,target,handRotation,state,.014f);assert(pose.valid&&pose.error<2e-4f);
   auto device=RigidMount(pose.foreTwist,bindCentre);assert(Length(device.Point(bindCentre)-pose.foreTwist.Point(bindCentre))<1e-5f);
   // The Pip-Boy is rigid even when near-extension forearm stretch is active.
   V a=device.Point(bindCentre),b=device.Point(bindCentre+V{0,.1f,0});assert(std::fabs(Length(b-a)-.1f)<1e-5f);
  }
  std::cout<<"Original "<<prefix<<" upper="<<Length(rig.elbow-rig.shoulder)<<" fore="<<Length(rig.wrist-rig.elbow)<<" palmOffset="<<Length(rig.palm-rig.wrist)<<" passed\n";
 }
 std::cout<<"Original eyes anchor=("<<anchor.x<<","<<anchor.y<<","<<anchor.z<<") and PipBoyArm attachment passed\n";
}
