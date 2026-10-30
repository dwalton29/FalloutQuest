#include "audio/fo3-footsteps.h"
#include "player/fo3-vr-body.h"
#include "rendering/mesh/fo3-static-nif.cpp"
#include <fstream>
#include <cassert>
#include <iostream>
bool LoadFalloutMeshFile(const std::string&,std::vector<uint8_t>&,std::string*){return false;}
static std::vector<uint8_t> Read(const char* path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
 if(argc<2)return 77;
 fo3anim::Clip clip;assert(fo3anim::DecodeClip(Read(argv[1]),clip));fo3audio::FootstepGait gait;assert(gait.Load(clip));
 fo3audio::FootstepClock clock;int left=0,right=0;for(int n=0;n<480;++n){const int foot=clock.Advance(gait.stride/120,gait.duration/120,true,gait,{});left+=foot==0;right+=foot==1;}assert(left==4&&right==4);
 std::cout<<"Original sneak gait "<<clip.name<<" stride="<<gait.stride<<" duration="<<gait.duration<<" contacts="<<left<<"/"<<right<<"\n";
 if(argc>2){fo3anim::Skeleton skeleton;assert(fo3anim::DecodeSkeleton(Read(argv[2]),skeleton));for(const char* side:{"L","R"}){const std::string prefix=std::string("Bip01 ")+side+" ";auto point=[&](const char* name){const int bone=fo3anim::FindBone(skeleton,prefix+name);assert(bone>=0);const auto& m=skeleton.bindGlobal[bone];return fo3vr::V{m[12]/70,m[14]/70,-m[13]/70};};const auto h=point("Thigh"),k=point("Calf"),a=point("Foot");for(float drop:{.2f,.4f,.6f}){const auto solved=fo3vr::SolveCrouchLeg(h,k,a,drop);assert(solved.valid);assert(fo3vr::Length(solved.thigh.Point(k)-solved.knee)<.001f&&fo3vr::Length(solved.calf.Point(a)-solved.ankle)<.001f);assert(std::abs(solved.ankle.y-a.y-drop)<.001f);}}std::cout<<"Original human leg segments preserve joint continuity and feet during crouch\n";}
}
