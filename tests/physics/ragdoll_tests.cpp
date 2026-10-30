#include "physics/fo3-rigid-world.h"
#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
bool LoadFalloutMeshFile(const std::string&,std::vector<uint8_t>&,std::string*){return false;}
#include "npc/fo3-ragdoll-pose.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace fo3rigid;
static M At(float x,float y,float z){auto m=Identity();m[12]=x;m[13]=y;m[14]=z;return m;}
static std::vector<Triangle> Floor(){return {{{-10,0,-10},{10,0,10},{10,0,-10}},{{-10,0,-10},{-10,0,10},{10,0,10}}};}
static void Verify(World& w,uint32_t id,const fo3rag::Definition& d,float scale,bool initial=false,const std::vector<float>& scales={}){
 std::vector<M> pose;assert(w.RagdollPose(id,pose));
 for(const auto& p:pose){for(float v:p)assert(std::isfinite(v));assert(std::abs(p[12])<8&&p[13]>-.5f&&p[13]<4&&std::abs(p[14])<8);}
 for(const auto& j:d.joints){auto a=j.frameA,b=j.frameB;for(int k=0;k<3;++k){a[12+k]*=scales.empty()?scale:scales[j.a];b[12+k]*=scales.empty()?scale:scales[j.b];}
  const auto pa=fo3anim::Point(fo3anim::Multiply(pose[j.a],a),{}),pb=fo3anim::Point(fo3anim::Multiply(pose[j.b],b),{});
  float distance=0;for(int k=0;k<3;++k)distance+=(pa[k]-pb[k])*(pa[k]-pb[k]);if(distance>=(initial?.035f*.035f:.025f*.025f))std::cerr<<"joint drift actor="<<id<<" a="<<d.bodies[j.a].bone<<" b="<<d.bodies[j.b].bone<<" metres="<<std::sqrt(distance)<<std::endl;assert(distance<(initial?.035f*.035f:.025f*.025f));
 }
}
int main(int argc,char** argv){
 fo3rag::Definition d;for(int i=0;i<2;++i){fo3rag::Body b;b.bone=std::to_string(i);b.a={0,-.15f,0};b.b={0,.15f,0};b.radius=.08f;b.mass=3;b.friction=.5f;b.linearDamping=.1f;b.angularDamping=.05f;d.bodies.push_back(b);}
 fo3rag::Joint joint;joint.a=0;joint.b=1;joint.frameA=At(0,.2f,0);joint.frameB=At(0,-.2f,0);joint.minimum={-.5f,-.5f,-.5f};joint.maximum={.5f,.5f,.5f};d.joints.push_back(joint);
 World w;w.Static(1,Floor());assert(w.AddRagdoll(50,d,{At(0,1,0),At(0,1.4f,0)},1));
 for(int i=0;i<1800;++i){w.Step(1.f/120);Verify(w,50,d,1);}
 std::vector<M> settled;bool sleeping=false;assert(w.RagdollPose(50,settled,&sleeping)&&sleeping);
 w.Static(2,{{{100,0,100},{101,0,101},{101,0,100}}});assert(w.RagdollPose(50,settled,&sleeping)&&sleeping);
 // A sleeping corpse remains a physical collider and wakes under an impact.
 assert(w.RagdollSleeping(50));Asset box;box.hulls.resize(1);for(float x:{-.08f,.08f})for(float y:{-.08f,.08f})for(float z:{-.08f,.08f})box.hulls[0].push_back({x,y,z});
 assert(w.Add(900,box,At(settled[0][12],settled[0][13]+.8f,settled[0][14])));bool woke=false;
 for(int tick=0;tick<240;++tick){w.Step(1.f/120);woke|=!w.RagdollSleeping(50);}assert(woke);w.Remove(900);
 for(int tick=0;tick<1200;++tick)w.Step(1.f/120);
 assert(w.RagdollPose(50,settled,&sleeping)&&sleeping);
 w.RemoveRagdoll(50);assert(!w.HasRagdoll(50));assert(w.AddRagdoll(50,d,settled,1));w.SleepRagdoll(50);assert(w.RagdollPose(50,settled,&sleeping)&&sleeping);w.ClearBodies();assert(w.RagdollActors().empty());
 // Fast falling capsules must not tunnel through a one-sided terrain mesh.
 assert(w.AddRagdoll(52,d,{At(2,2,0),At(2,2.4f,0)},1,{0,-20,0}));
 for(int i=0;i<600;++i){w.Step(1.f/120);std::vector<M> falling;assert(w.RagdollPose(52,falling));for(const auto& m:falling)assert(m[13]>-.15f);}
 w.ClearBodies();
 // Child attachment propagation and rigid conversion are shared with rendering.
 fo3anim::Skeleton sk;sk.bones.resize(3);sk.bones[0].name="0";sk.bones[1].name="1";sk.bones[1].parent=0;sk.bones[2].name="eye";sk.bones[2].parent=1;sk.bones[2].bind.translation={0,.1f,0};assert(fo3anim::FinalizeSkeleton(sk));
 fo3anim::Pose p;fo3anim::BindClip(sk,{},p);assert(fo3anim::Sample(sk,{},0,p));fo3player::CorpsePose corpse;corpse.model="test.nif";corpse.bones={{"0",At(1,2,3)},{"1",At(1,2.4f,3)}};assert(fo3rag::ApplyPose(sk,p,corpse));assert(std::abs(p.global[2][13]-2.5f)<1e-5f);
 if(argc>1){
  std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(f),{});std::string error;assert(fo3rag::Read(bytes,d,error));assert(d.bodies.size()==18&&d.joints.size()==17);
  assert(fo3anim::DecodeSkeleton(bytes,sk));std::vector<M> original;std::vector<float> scales;auto scene=Identity();scene[0]=scene[9]=1.f/70;scene[6]=-1.f/70;scene[5]=scene[10]=0;scene[13]=.25f;
  for(const auto& body:d.bodies){const int bone=fo3anim::FindBone(sk,body.bone);assert(bone>=0);auto m=fo3anim::Multiply(scene,sk.bindGlobal[bone]);float length=0;for(int r=0;r<3;++r)length+=m[r]*m[r];scales.push_back(std::sqrt(length));assert(fo3rag::Rigid(m));original.push_back(m);}
  assert(w.AddRagdoll(51,d,original,1.f/70,{},scales));std::cerr<<"original initial bind verification"<<std::endl;Verify(w,51,d,1.f/70,true,scales);std::cerr<<"original dynamic verification"<<std::endl;
  for(int i=0;i<2400;++i){w.Step(1.f/120);Verify(w,51,d,1.f/70,i<30,scales);}
  assert(w.RagdollPose(51,settled,&sleeping));std::cout<<"original rig 18 bodies/17 joints sleeping="<<sleeping<<" pelvisY="<<settled[0][13]<<"\n";assert(sleeping&&settled[0][13]<.6f);
  // Original capsules on a real triangle tread/riser, not a height clamp.
  w.ClearBodies();w.ClearStatics();w.Static(1,Floor());w.Static(2,{{{0,.3f,-10},{10,.3f,10},{10,.3f,-10}},{{0,.3f,-10},{0,.3f,10},{10,.3f,10}},{{0,0,-10},{0,.3f,10},{0,.3f,-10}},{{0,0,-10},{0,0,10},{0,.3f,10}}});
  for(auto& m:original)m[13]+=1.25f;
  assert(w.AddRagdoll(51,d,original,1.f/70,{0,-4,0},scales));
  for(int i=0;i<3600;++i){w.Step(1.f/120);Verify(w,51,d,1.f/70,i<30,scales);}
  assert(w.RagdollPose(51,settled,&sleeping)&&sleeping&&settled[0][13]<1);
  std::cout<<"original rig stair/riser fall settled\n";
  w.ClearBodies();w.ClearStatics();w.Static(1,Floor());
  for(uint32_t actor=60;actor<63;++actor){auto group=original;for(auto& m:group)m[12]+=float(int(actor)-61)*2.f;assert(w.AddRagdoll(actor,d,group,1.f/70,{},scales));}
  for(int tick=0;tick<2400;++tick)w.Step(1.f/120);
  for(uint32_t actor=60;actor<63;++actor)assert(w.RagdollSleeping(actor));
  std::cout<<"Three original human ragdolls settle concurrently and retain sleeping collision islands\n";
  for(size_t size:{size_t(0),size_t(30),bytes.size()/2,bytes.size()-1}){auto cut=bytes;cut.resize(size);fo3rag::Definition invalid;assert(!fo3rag::Read(cut,invalid,error));}
 }
 std::cout<<"ragdoll tests passed\n";
}
