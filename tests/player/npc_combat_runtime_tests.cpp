// Exercise the production combat and NAVM executor. Only GL/skin attachment,
// audio, and the already separate projectile emitter are recording adapters.
#define main PackageFixtureMain
#include "npc_package_runtime_tests.cpp"
#undef main
#include "npc/fo3-npc-combat.h"
#include "player/fo3-vr-body.h"
std::vector<Q230ActorVisual> gQ230NpcActors;
bool blocked=false,loading=false;int dialogueEnds=0,shots=0;double emittedDamage=0;
bool IsFo3LoadingVisible(){return loading;}
void Q230UpdateActor(Q230ActorVisual&){}
bool Q230LiveBone(const Q230ActorVisual& actor,int,std::array<float,3>& p){p=actor.runtime.position;p[1]+=1;return true;}
bool HasFo3InteractionOccluder(float,float,float,float,float,float,float,uint32_t){return blocked;}
void EndFo3Dialogue(const char*){++dialogueEnds;}
namespace fo3audio {void SoundEvent(uint32_t){}void NamedSound(const std::string&) {}}
static fo3anim::Matrix Q230WeaponMatrix(Q230ActorVisual& actor){auto m=fo3anim::Identity();m[12]=actor.runtime.position[0];m[13]=actor.runtime.position[1]+1;m[14]=actor.runtime.position[2];return m;}
namespace weaponruntime {
void EmitShot(uint64_t,uint32_t attacker,uint32_t base,const fo3weapon::Definition&,fo3vr::V,fo3vr::V,fo3vr::V,fo3vr::V,float damage){++shots;emittedDamage=damage;gPlayerSession->player.ApplyAttack(attacker,base,0x14,damage);}
}
#define FO3_NPC_COMBAT_HOST_TEST
#include "npc/fo3-npc-combat-runtime.inc"
static void Prepare() {
  auto actor=Actor(6);auto c=gPlayerSession->player.Definitions();c.weapons.skillBase=c.weapons.conditionBase=1;c.weapons.detectionDistance=2500;
  c.references[42].base=43;c.pipboy.targets[42].cell=1;
  auto& a=c.pipboy.dialogueActors[43];a.aiData.resize(20);a.aiData[0]=3;a.aiData[1]=3;
  fo3player::Item w;w.formId=10;w.name="Original weapon";w.kind=fo3player::ItemKind::Weapon;
  auto& d=w.weapon;d.valid=true;d.animation=3;d.clip=3;d.ammo=11;d.ammoUse=1;d.pellets=1;d.damage=10;d.shotsPerSecond=4;d.reloadTime=.5f;d.maxRange=1000;
  c.items[10]=w;fo3player::Item ammo;ammo.formId=11;ammo.kind=fo3player::ItemKind::Ammo;c.items[11]=ammo;
  c.actorInventories[43].entries={{10,0,1,1,1,false},{11,0,1,1,1,false}};
  gPlayerSession=std::make_unique<Session>(std::move(c));
  Q230CombatWeapon weapon;weapon.definition=d;weapon.ready=true;weapon.gpu={0};weapon.clips[1].stop=.3f;weapon.clips[2].stop=.5f;
  actor.combatWeapons[10]=weapon;actor.runtime.yaw=0;actor.runtime.activity=fo3npc::Activity::Package;actor.runtime.package=50;
  actor.runtime.BeginDialogue();gQ230NpcActors={actor};
  gQ210Head=actor.runtime.position;gQ210Head[1]+=1;gQ210Head[2]-=1;
  shots=0;blocked=false;loading=false;dialogueEnds=0;
}
int main(){
  Prepare();auto& a=gQ230NpcActors[0];blocked=true;Q230SimulateActor(a,1);assert(a.runtime.activity==fo3npc::Activity::Dialogue&&shots==0);
  blocked=false;Q230SimulateActor(a,1.3);assert(a.runtime.activity==fo3npc::Activity::Combat&&dialogueEnds==1&&a.runtime.equippedWeapon);
  assert(a.runtime.reloadUntil>0&&!a.runtime.pendingAttack);Q230SimulateActor(a,1.4);assert(shots==0);
  Q230SimulateActor(a,1.9);assert(a.runtime.reloadUntil==0&&gPlayerSession->player.ActorWeapon(42)->loadedRounds==3);
  Q230SimulateActor(a,2);assert(a.runtime.pendingAttack);Q230SimulateActor(a,2.01);assert(shots==1&&emittedDamage==10&&gPlayerSession->player.Health()==90);
  Q230SimulateActor(a,2.1);assert(shots==1);Q230SimulateActor(a,2.3);Q230SimulateActor(a,2.31);assert(shots==2);
  blocked=true;Q230SimulateActor(a,3);assert(a.runtime.action==fo3npc::CombatAction::Search&&!a.aiPathGame.empty());
  const auto position=a.runtime.position;Q230SimulateActor(a,19);assert(a.runtime.activity==fo3npc::Activity::Package&&a.runtime.package==50&&a.runtime.position==position);
  Q230PersistActor(a);auto saved=gPlayerSession->player.Snapshot().actors.at(42);a.stateRestored=false;a.runtime.position={0,0,0};loading=true;Q230SimulateActor(a,20);loading=false;blocked=true;Q230SimulateActor(a,21);assert(Q240GamePosition(a)==saved.position);
  auto c=gPlayerSession->player.Definitions();c.pipboy.dialogueActors[43].aiData[1]=0;gPlayerSession=std::make_unique<Session>(c);a.stateRestored=false;blocked=false;a.runtime.BeginCombat(0x14);a.runtime.nextPath=0;
  Q230SimulateActor(a,22);assert(a.runtime.action==fo3npc::CombatAction::Flee&&!a.aiPathGame.empty());
  assert(gPlayerSession->player.ApplyAttack(0x14,10,42,1000));Q230SimulateActor(a,23);assert(a.runtime.activity==fo3npc::Activity::Dying&&!a.runtime.pendingAttack);Q230SimulateActor(a,24);assert(a.runtime.activity==fo3npc::Activity::Dead&&gPlayerSession->player.CanLootContainer(42));
  a.runtime.position=Q240ScenePosition({2000,3000,20});a.runtime.nextPath=0;a.aiPathGame.clear();
  assert(!Q230CombatRoute(a,{1000,2000,20},true,30)&&a.aiPathGame.empty());
  Prepare();auto& patrol=gQ230NpcActors[0];auto pc=gPlayerSession->player.Definitions();
  pc.pipboy.packages[50].type=13;pc.pipboy.packages[50].patrol={{60,pc.pipboy.targets.at(60)},{61,pc.pipboy.targets.at(60)}};
  gPlayerSession=std::make_unique<Session>(pc);fo3player::ActorState progress;progress.cell=1;progress.world=2;progress.package=50;progress.sequence=2;progress.packageWaitSeconds=5;progress.position=Q240GamePosition(patrol);
  assert(gPlayerSession->player.UpdateActor(42,progress));patrol.stateRestored=false;blocked=true;Q230SimulateActor(patrol,31);assert(patrol.aiSequence==2&&patrol.aiRepathAt==36);
  patrol.runtime.EndDialogue();patrol.runtime.suspendedPackageWait=4;patrol.runtime.BeginCombat(999);Q230SimulateActor(patrol,32);assert(patrol.runtime.activity==fo3npc::Activity::Package&&patrol.aiSequence==2&&patrol.aiRepathAt==36);
  Prepare();auto& escort=gQ230NpcActors[0];auto ec=gPlayerSession->player.Definitions();ec.pipboy.packages[50].type=2;ec.pipboy.packages[50].target.valid=true;ec.pipboy.packages[50].target.type=0;ec.pipboy.packages[50].target.value=0x14;ec.pipboy.packages[50].escortDistance=300;ec.pipboy.packages[50].escortDistanceValid=true;
  gPlayerSession=std::make_unique<Session>(ec);progress.sequence=2;assert(gPlayerSession->player.UpdateActor(42,progress));escort.stateRestored=false;blocked=true;Q230SimulateActor(escort,40);assert(escort.aiSequence==2);
  escort.runtime.EndDialogue();escort.runtime.BeginCombat(999);Q230SimulateActor(escort,41);assert(escort.aiSequence==2&&escort.runtime.activity==fo3npc::Activity::Package);
  Prepare();auto& waiting=gQ230NpcActors[0];auto wc=gPlayerSession->player.Definitions();wc.pipboy.packages[50].type=13;wc.pipboy.packages[50].patrol={{60,wc.pipboy.targets.at(60)}};
  gPlayerSession=std::make_unique<Session>(wc);assert(gPlayerSession->player.PrepareActorInventory(42));waiting.aiPackage=50;waiting.aiSequence=2;waiting.aiRepathAt=10;
  Q230SimulateActor(waiting,2);assert(waiting.runtime.activity==fo3npc::Activity::Combat&&waiting.runtime.suspendedPackageWait==8);
  Q230PersistActor(waiting);assert(gPlayerSession->player.Snapshot().actors.at(42).packageWaitSeconds==8);
  blocked=true;Q230SimulateActor(waiting,20);assert(waiting.runtime.activity==fo3npc::Activity::Package&&waiting.aiRepathAt==28);
  Q230PersistActor(waiting);assert(gPlayerSession->player.Snapshot().actors.at(42).packageWaitSeconds==8);
  std::cout<<"Production NPC combat pursuit/flee, LOS, reload, firing, death and restore passed\n";
}
