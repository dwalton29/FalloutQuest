// Exercise the production combat and NAVM executor. Only GL/skin attachment,
// audio, and the already separate projectile emitter are recording adapters.
#define main PackageFixtureMain
#include "npc_package_runtime_tests.cpp"
#undef main
#include "npc/fo3-npc-combat.h"
#include "player/fo3-vr-body.h"
#include <unistd.h>
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
static void AddNpc(fo3player::Catalog& c,uint32_t ref,uint32_t base,uint32_t target=0) {
  auto actor=gQ230NpcActors.front();actor.source.refFormId=ref;actor.source.baseFormId=base;actor.runtime={};actor.runtime.position=gQ230NpcActors.front().runtime.position;actor.runtime.position[2]-=.5f;
  actor.aiPackage=actor.aiSequence=0;actor.stateRestored=false;if(target)actor.runtime.BeginCombat(target);
  c.references[ref].base=base;c.pipboy.targets[ref].base=base;c.pipboy.targets[ref].cell=1;c.pipboy.targets[ref].world=2;const auto root=Q240GamePosition(actor);c.pipboy.targets[ref].x=root[0];c.pipboy.targets[ref].y=root[1];c.pipboy.targets[ref].z=root[2];c.weapons.actors[base].health=100;
  c.pipboy.dialogueActors[base]=c.pipboy.dialogueActors[43];c.pipboy.dialogueActors[base].packages.clear();
  gQ230NpcActors.push_back(actor);packageTargets.push_back(actor);
}
static void PackageCombatPolicyTests() {
  Prepare();auto c=gPlayerSession->player.Definitions();c.pipboy.packages[50].flags=1u<<22;gPlayerSession=std::make_unique<Session>(c);
  gQ230NpcActors[0].runtime.EndDialogue();Q230SimulateActor(gQ230NpcActors[0],1);assert(gQ230NpcActors[0].runtime.activity!=fo3npc::Activity::Combat); // No first-frame aggression before package movement.
  Q240UpdateNpcPackage(gQ230NpcActors[0],1.1);gQ230NpcActors[0].runtime.BeginDialogue();Q230SimulateActor(gQ230NpcActors[0],1.5);assert(gQ230NpcActors[0].runtime.dialogue&&dialogueEnds==0);
  assert(gPlayerSession->player.ApplyAttack(0x14,10,42,10));Q230SimulateActor(gQ230NpcActors[0],2);assert(gQ230NpcActors[0].runtime.activity==fo3npc::Activity::Combat&&gQ230NpcActors[0].runtime.combatTarget==0x14&&dialogueEnds==1); // Canonical damage still interrupts dialogue.

  Prepare();c=gPlayerSession->player.Definitions();c.pipboy.packages[50].flags=1u<<22;c.pipboy.dialogueActors[43].aiData[14]=2;c.pipboy.dialogueActors[43].factions[100]=0;AddNpc(c,70,71,0x14);gPlayerSession=std::make_unique<Session>(c);
  gQ230NpcActors[0].runtime.EndDialogue();Q230SimulateActor(gQ230NpcActors[0],1);assert(gQ230NpcActors[0].runtime.activity!=fo3npc::Activity::Combat); // Shared-faction assistance does not bypass a defensive Travel package.
  assert(gPlayerSession->player.ApplyAttack(70,10,42,5));Q230SimulateActor(gQ230NpcActors[0],2);assert(gQ230NpcActors[0].runtime.combatTarget==70); // NPC damage remains a valid retaliation.

  for(uint8_t type:{1,2}) {
    Prepare();c=gPlayerSession->player.Definitions();auto& p=c.pipboy.packages[50];p.type=type;p.flags=1u<<22;p.target.valid=true;p.target.type=0;p.target.value=type==1?70:0x14;p.target.radius=100;p.escortDistanceValid=true;p.escortDistance=300;
    if(type==1)AddNpc(c,70,71);AddNpc(c,80,81,p.target.value);gPlayerSession=std::make_unique<Session>(c);
    gQ230NpcActors[0].runtime.EndDialogue();Q230SimulateActor(gQ230NpcActors[0],1);assert(gQ230NpcActors[0].runtime.activity==fo3npc::Activity::Combat&&gQ230NpcActors[0].runtime.combatTarget==80&&gQ230NpcActors[0].aiPackage==50);
  }
  Prepare();c=gPlayerSession->player.Definitions();auto& p=c.pipboy.packages[50];p.type=1;p.flags=1u<<22;p.target.valid=true;p.target.type=0;p.target.value=70;p.target.radius=100;AddNpc(c,70,71);gPlayerSession=std::make_unique<Session>(c);
  assert(gPlayerSession->player.ApplyAttack(0x14,10,70,5));gQ230NpcActors[0].runtime.EndDialogue();blocked=true;Q230SimulateActor(gQ230NpcActors[0],1);assert(gQ230NpcActors[0].runtime.activity!=fo3npc::Activity::Combat);blocked=false;Q230SimulateActor(gQ230NpcActors[0],2);assert(gQ230NpcActors[0].runtime.combatTarget==0x14); // Leader damage evidence, still bounded by LOS.

  Prepare();c=gPlayerSession->player.Definitions();c.pipboy.packages[50].flags=1u<<22;c.pipboy.packages[50].schedule.valid=true;c.pipboy.packages[50].schedule.hour=12;c.pipboy.packages[50].schedule.duration=1;
  c.pipboy.packages[52]=c.pipboy.packages[50];c.pipboy.packages[52].flags=0;c.pipboy.packages[52].schedule.valid=false;c.pipboy.dialogueActors[43].packages={50,52};gPlayerSession=std::make_unique<Session>(c);gQ230NpcActors[0].runtime.EndDialogue();
  Q240UpdateNpcPackage(gQ230NpcActors[0],1);packageHour=14;Q230SimulateActor(gQ230NpcActors[0],2);assert(gQ230NpcActors[0].runtime.activity==fo3npc::Activity::Combat&&gQ230NpcActors[0].aiPackage==52&&gQ230NpcActors[0].runtime.suspendedPackage==52);packageHour=12; // Policy resolves a newly scheduled package before perception.

  Prepare();c=gPlayerSession->player.Definitions();c.pipboy.packages[50].combatStyle=200;c.pipboy.dialogueActors[43].combatStyle=201;
  c.weapons.styles[200].valid=true;c.weapons.styles[200].restrictions=1;c.weapons.styles[200].delayMin=2;
  c.weapons.styles[201].valid=true;c.weapons.styles[201].restrictions=2;
  auto melee=c.items[10];melee.formId=20;melee.weapon.animation=1;melee.weapon.ammo=0;melee.weapon.damage=1;c.items[20]=melee;c.actorInventories[43].entries.push_back({20,0,1,1,1,false});
  auto prepared=gQ230NpcActors[0].combatWeapons.at(10);prepared.definition=melee.weapon;gQ230NpcActors[0].combatWeapons[20]=prepared;gQ230NpcActors[0].aiPackage=50;gPlayerSession=std::make_unique<Session>(c);assert(gPlayerSession->player.PrepareActorInventory(42));
  assert(Q230Style(gQ230NpcActors[0])==&gPlayerSession->player.Definitions().weapons.styles.at(200));assert(Q230SelectWeapon(gQ230NpcActors[0])&&gPlayerSession->player.ActorWeapon(42)->formId==20);
  fo3player::ActorState progress;progress.world=2;progress.cell=1;progress.package=50;progress.position=Q240GamePosition(gQ230NpcActors[0]);assert(gPlayerSession->player.UpdateActor(42,progress));
  const auto save="/tmp/fq-package-style-"+std::to_string(getpid());std::string error;assert(gPlayerSession->player.Save(save,error));gPlayerSession=std::make_unique<Session>(c);assert(gPlayerSession->player.Restore(save,error));std::remove(save.c_str());gQ230NpcActors[0].stateRestored=false;gQ230NpcActors[0].runtime.EndDialogue();blocked=true;Q230SimulateActor(gQ230NpcActors[0],4);assert(Q230Style(gQ230NpcActors[0])==&gPlayerSession->player.Definitions().weapons.styles.at(200));gQ230NpcActors[0].runtime.BeginCombat(999);Q230SimulateActor(gQ230NpcActors[0],5);assert(gQ230NpcActors[0].runtime.activity!=fo3npc::Activity::Combat&&Q230Style(gQ230NpcActors[0])==&gPlayerSession->player.Definitions().weapons.styles.at(200));
  auto d=c.items[10].weapon;d.delayMin=.25f;fo3npc::RuntimeState timing;timing.BeginCombat(0x14);assert(fo3npc::FireReady(timing,d,1,Q230Style(gQ230NpcActors[0]))&&!fo3npc::FireReady(timing,d,1.25,Q230Style(gQ230NpcActors[0]))&&fo3npc::FireReady(timing,d,1.5,Q230Style(gQ230NpcActors[0])));
  c.pipboy.packages[50].combatStyle=0;gPlayerSession=std::make_unique<Session>(c);assert(gPlayerSession->player.PrepareActorInventory(42));assert(Q230SelectWeapon(gQ230NpcActors[0])&&gPlayerSession->player.ActorWeapon(42)->formId==10); // Null CNAM falls back to NPC ZNAM.
  c.pipboy.packages[50].combatStyle=999;gPlayerSession=std::make_unique<Session>(c);assert(gPlayerSession->player.PrepareActorInventory(42));assert(!Q230SelectWeapon(gQ230NpcActors[0])); // Unavailable override never becomes a default style.
}
static void OriginalCombatPolicies(const char* path) {
  fo3player::Catalog c;std::string error;assert(fo3player::LoadCatalog(path,c,error));gPlayerSession=std::make_unique<Session>(std::move(c));const auto& catalog=gPlayerSession->player.Definitions();
  size_t defensive=0,overrides=0;Q230ActorVisual actor;actor.source.baseFormId=7;
  for(const auto& entry:catalog.pipboy.packages){const auto& p=entry.second;if(fo3npc::Defensive(&p))++defensive;if(!p.combatStyle)continue;
    ++overrides;actor.aiPackage=entry.first;assert(p.combatStyleValid);const auto style=catalog.weapons.styles.find(p.combatStyle);assert(style!=catalog.weapons.styles.end()&&style->second.valid);assert(Q230Style(actor)==&style->second);
  }
  assert(defensive==161&&overrides==17);std::cout<<"Original Defensive packages="<<defensive<<" CNAM combat styles="<<overrides<<'\n';
}
int main(int argc,char** argv){
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
  Prepare();auto& fleeing=gQ230NpcActors[0];auto fc=gPlayerSession->player.Definitions();auto& fp=fc.pipboy.packages[50];fp.type=10;fp.location={};fp.target.valid=true;fp.target.type=0;fp.target.value=0x14;fp.target.radius=500;
  fo3pipdata::Condition falseGate;falseGate.function=72;falseGate.a=44;falseGate.value=1;fp.conditions={falseGate};
  gPlayerSession=std::make_unique<Session>(fc);progress.packageWaitSeconds=0;progress.sequence=1;progress.position=Q240GamePosition(fleeing);assert(gPlayerSession->player.UpdateActor(42,progress));
  const char* save="/tmp/fq-flee-package-runtime-save";std::string error;assert(gPlayerSession->player.Save(save,error));gPlayerSession=std::make_unique<Session>(fc);assert(gPlayerSession->player.Restore(save,error));std::remove(save);
  fleeing.runtime.EndDialogue();fleeing.stateRestored=false;blocked=true;Q230SimulateActor(fleeing,50);assert(fleeing.aiPackage==50&&fleeing.aiSequence==1);Q240UpdateNpcPackage(fleeing,50.1);assert(fleeing.runtime.activity==fo3npc::Activity::Package&&fleeing.aiPackage==50);
  fleeing.runtime.BeginCombat(999);Q230SimulateActor(fleeing,51);assert(fleeing.runtime.activity==fo3npc::Activity::Package&&fleeing.aiSequence==1);Q240UpdateNpcPackage(fleeing,51.1);assert(fleeing.aiPackage==50); // False entry condition does not undo a resumed Flee phase.
  PackageCombatPolicyTests();if(argc>1)OriginalCombatPolicies(argv[1]);
  std::cout<<"Production NPC combat pursuit/flee, LOS, reload, firing, death and restore passed\n";
}
