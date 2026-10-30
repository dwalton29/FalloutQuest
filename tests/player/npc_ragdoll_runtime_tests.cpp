#include "player/fo3-player-state.h"
#include "physics/fo3-rigid-world.h"
#include "npc/fo3-npc.h"
#include "npc/fo3-npc-state.h"
#include "npc/fo3-ragdoll-pose.h"
#include "rendering/quest-render-policy.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <unistd.h>
#include <cstdio>
#define Q6H_LOGI(...) do {if(false)std::printf(__VA_ARGS__);}while(false)
struct Rig {fo3anim::Matrix scenePlacement=fo3anim::Identity();};
struct Q230ActorVisual {
 Fo3NpcActorQ230 source;fo3npc::RuntimeState runtime;fo3anim::Skeleton skeleton;fo3anim::Pose pose;
 std::vector<Rig> rigs;fo3anim::Matrix dialogueRoot=fo3anim::Identity();
 std::shared_ptr<const fo3rag::Definition> ragdollDefinition;std::shared_ptr<const fo3player::CorpsePose> corpsePose;
 mutable std::shared_ptr<const fo3player::CorpsePose> boundsCorpse;mutable fo3anim::Matrix corpseBoundsScene{};mutable questrender::Bounds cachedCorpseBounds{};
 std::vector<int> ragdollBones;std::vector<float> ragdollScales;bool ragdollStarted=false,stateRestored=true;
 std::array<float,3> ragdollPreviousPosition{},ragdollVelocity{};double ragdollMotionAt=-1;uint64_t lastFrame=0;
};
std::vector<Q230ActorVisual> gQ230NpcActors;
struct RagdollTestSession {fo3player::Player player;explicit RagdollTestSession(const fo3player::Catalog& c):player(c){}};
std::unique_ptr<RagdollTestSession> gPlayerSession;
#include "npc/fo3-npc-ragdoll-runtime.inc"
static fo3anim::Matrix At(float x,float y,float z){auto m=fo3anim::Identity();m[12]=x;m[13]=y;m[14]=z;return m;}
static Q230ActorVisual Actor(){
 Q230ActorVisual actor;actor.source.refFormId=100;actor.source.skeletonModel="OriginalFixture.nif";actor.rigs.resize(1);actor.runtime.position={1,2,3};actor.rigs[0].scenePlacement=At(1,2,3);
 // Use a non-unit placement scale, as the actual NIF->metre bridge does.
 for(int k=0;k<3;++k)actor.rigs[0].scenePlacement[k*5]=.5f;
 actor.skeleton.bones.resize(3);actor.skeleton.bones[0].name="pelvis";actor.skeleton.bones[1].name="chest";actor.skeleton.bones[1].parent=0;actor.skeleton.bones[1].bind.translation={0,.4f,0};actor.skeleton.bones[2].name="eyes";actor.skeleton.bones[2].parent=1;actor.skeleton.bones[2].bind.translation={0,.2f,0};assert(fo3anim::FinalizeSkeleton(actor.skeleton));fo3anim::BindClip(actor.skeleton,{},actor.pose);assert(fo3anim::Sample(actor.skeleton,{},0,actor.pose));
 auto d=std::make_shared<fo3rag::Definition>();for(const char* name:{"pelvis","chest"}){fo3rag::Body b;b.bone=name;b.a={0,-.15f,0};b.b={0,.15f,0};b.radius=.1f;b.mass=5;b.friction=.5f;d->bodies.push_back(b);}fo3rag::Joint j;j.a=0;j.b=1;j.frameA=At(0,.2f,0);j.frameB=At(0,-.2f,0);j.minimum={-.5f,-.5f,-.5f};j.maximum={.5f,.5f,.5f};d->joints.push_back(j);actor.ragdollDefinition=d;actor.ragdollBones={0,1};return actor;
}
int main(){
 using namespace fo3player;Catalog c;c.initial.baseHealth=100;c.initial.special.fill(1);c.weapons.actors[200].health=100;c.pipboy.targets[100].base=c.references[100].base=200;c.pipboy.targets[100].cell=1;
 Item item;item.formId=10;item.kind=ItemKind::Misc;item.name="Fixture loot";c.items[10]=item;c.actorInventories[200].entries={{10,0,1,1,1,false}};
 Item gun;gun.formId=20;gun.kind=ItemKind::Weapon;gun.weapon.valid=true;gun.weapon.damage=1000;c.items[20]=gun;
 gPlayerSession=std::make_unique<RagdollTestSession>(c);auto& p=gPlayerSession->player;ActorState state;state.cell=1;state.position={1,2,3};assert(p.UpdateActor(100,state));gQ230NpcActors.push_back(Actor());auto& actor=gQ230NpcActors[0];auto& world=fo3rigid::Live();world.ClearBodies();world.ClearStatics();world.Static(1,{{{-10,0,-10},{10,0,10},{10,0,-10}},{{-10,0,-10},{-10,0,10},{10,0,10}}});
 Q230PrepareRagdolls();assert(!actor.ragdollStarted);assert(p.ApplyWeaponHit(20,100,1000));Q230TrackRagdollMotion(actor,1);Q230PrepareRagdolls();assert(actor.ragdollStarted&&world.HasRagdoll(100));
 auto frozen=actor.pose;Q230CaptureRagdolls();assert(actor.corpsePose);assert(fo3rag::ApplyPose(actor.skeleton,actor.pose,*actor.corpsePose));for(size_t i=0;i<actor.pose.global.size();++i)for(int k=0;k<16;++k)assert(std::abs(actor.pose.global[i][k]-frozen.global[i][k])<1e-5f); // No pose discontinuity on handoff.
 for(int n=0;n<1200;++n){world.Step(1.f/120);Q230CaptureRagdolls();}
 assert(actor.corpsePose->sleeping&&p.Snapshot().actors.at(100).corpse);assert(fo3rag::ApplyPose(actor.skeleton,actor.pose,*actor.corpsePose));assert(actor.pose.global[0][13]<-3); // Fell relative to the original 2m root.
 questrender::Bounds bounds;assert(Q230RagdollBounds(actor,bounds)&&bounds.minY<.1f&&bounds.maxY<1);
 assert(p.CanLootContainer(100)&&p.PrepareContainer(100));const auto* loot=p.ContainerContents(100);assert(loot&&!loot->empty());assert(p.TakeContainerStack(100,loot->front().id));assert(p.ItemCount(10)==1);
 const auto path="/tmp/fq-corpse-runtime-"+std::to_string(getpid());std::string error;assert(p.Save(path,error));const auto saved=*actor.corpsePose;
 world.ClearBodies();gQ230NpcActors.clear();assert(p.Restore(path,error));gQ230NpcActors.push_back(Actor());auto& returned=gQ230NpcActors[0];returned.corpsePose=p.Snapshot().actors.at(100).corpse;returned.ragdollStarted=true;Q230PrepareRagdolls();Q230CaptureRagdolls();assert(*returned.corpsePose==saved);assert(fo3rag::ApplyPose(returned.skeleton,returned.pose,*returned.corpsePose));assert(p.ItemCount(10)==1&&p.ContainerContents(100)->empty());
 const auto revision=p.Revision();const auto frozenPose=returned.corpsePose;for(int frame=0;frame<180;++frame){world.Step(1.f/60);Q230CaptureRagdolls();assert(world.RagdollSleeping(100)&&returned.corpsePose==frozenPose&&p.Revision()==revision);} // Sleeping pose does not dirty saves/packages every frame.
 // Resuming an airborne save preserves actual body velocity, not just pose.
 world.RemoveRagdoll(100);auto moving=saved;moving.sleeping=false;for(auto& b:moving.bones){b.global[13]+=4;b.linear={2,0,0};b.angular={0,.3f,0};}returned.corpsePose=std::make_shared<const CorpsePose>(moving);Q230PrepareRagdolls();Q230CaptureRagdolls();assert(std::abs(returned.corpsePose->bones[0].linear[0]-2)<1e-5f);world.Step(1.f/120);Q230CaptureRagdolls();assert(!world.RagdollSleeping(100)&&returned.corpsePose->bones[0].global[12]>moving.bones[0].global[12]);
 gQ230NpcActors.clear();Q230PrepareRagdolls();assert(world.RagdollActors().empty());unlink(path.c_str());std::cout<<"production NPC ragdoll handoff, loot, return/save and motion tests passed\n";
}
