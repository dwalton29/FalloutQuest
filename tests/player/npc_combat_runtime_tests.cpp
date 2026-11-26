// Exercise the production combat and NAVM executor. Only GL/skin attachment,
// audio, and the already separate projectile emitter are recording adapters.
#define FO3_NPC_LOOSE_MESH_HOST_TEST
#define main PackageFixtureMain
#include "npc_package_runtime_tests.cpp"
#undef main
#include "npc/fo3-npc-combat.h"
#include "player/fo3-vr-body.h"
#include "rendering/mesh/fo3-static-nif.h"
#include "data/fo3-bsa-reader.h"
#include "scripting/fo3-script-master-match.h"
#include <unistd.h>
std::vector<Q230ActorVisual> gQ230NpcActors;
bool partialCover=false,captureCoverRays=false;std::vector<float> coverRayOrigins;
bool blocked=false,loading=false,blockSecondSightRay=false;int sightRayCount=0,dialogueEnds=0,shots=0;double emittedDamage=0;
bool IsFo3LoadingVisible(){return loading;}
void Q230UpdateActor(Q230ActorVisual&){}
bool Q230LiveBone(const Q230ActorVisual& actor,int bone,std::array<float,3>& p){p=actor.runtime.position;p[1]+=bone==actor.headBone?1.7f:1.f;return true;}
bool Q230LiveBounds(Q230ActorVisual& actor,std::array<float,3>& lo,std::array<float,3>& hi){
  lo=hi=actor.runtime.position;hi[1]+=1.8f;return true;
}
bool HasFo3InteractionOccluder(float,float y,float,float,float dy,float,float length,uint32_t){if(captureCoverRays)coverRayOrigins.push_back(y);if(partialCover)return y+dy*length<.3f;return blocked||(blockSecondSightRay&&++sightRayCount==2);}
void EndFo3Dialogue(const char*){++dialogueEnds;}
std::vector<std::string> actionSounds;
namespace fo3audio {void SoundEvent(uint32_t){}void NamedSound(const std::string& name) {actionSounds.push_back(name);}}
static fo3anim::Matrix Q230WeaponMatrix(Q230ActorVisual& actor){auto m=fo3anim::Identity();m[12]=actor.runtime.position[0];m[13]=actor.runtime.position[1]+1;m[14]=actor.runtime.position[2];return m;}
namespace weaponruntime {
void EmitShot(uint64_t,uint32_t attacker,uint32_t base,const fo3weapon::Definition&,fo3vr::V,fo3vr::V,fo3vr::V,fo3vr::V,float damage){++shots;emittedDamage=damage;gPlayerSession->player.ApplyAttack(attacker,base,0x14,damage);}
}
#ifdef FO3_NPC_DETECTION_HOST_TEST
float gPlayerDetectionSpeed=0,detectionLight=0;
fo3sneak::Noise gPlayerDetectionShot,gPlayerDetectionStep;
float Q230PlayerLightLevel(const std::array<float,3>&,double){return detectionLight;}
#endif
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
  Q230CombatWeapon weapon;weapon.definition=d;weapon.ready=true; // CPU-ready weapon can fire before its render mesh arrives.
  weapon.clips[1].stop=.3f;weapon.clips[2].stop=.5f;
  actor.combatWeapons[10]=weapon;actor.runtime.yaw=0;actor.runtime.activity=fo3npc::Activity::Package;actor.runtime.package=50;
  actor.runtime.BeginDialogue();gQ230NpcActors={actor};
  gQ210Head=actor.runtime.position;gQ210Head[1]+=1;gQ210Head[2]-=1;
  actionSounds.clear();shots=0;blocked=false;partialCover=captureCoverRays=false;coverRayOrigins.clear();loading=false;dialogueEnds=0;
}
static void ScriptOrderedFists(bool creature=false){
  Prepare();
  auto c=gPlayerSession->player.Definitions();
  auto& actor=gQ230NpcActors.front();
  c.pipboy.dialogueActors[43].aiData[0]=0;
  c.pipboy.dialogueActors[43].aiData[1]=2;
  c.actorInventories[43].entries.clear();
  if(creature){
    actor.source.creature=true;
    c.actorInventories.erase(43);
    c.weapons.actors[43].creature=true;
    c.weapons.actors[43].creatureDamage=5;
    c.weapons.actors[43].creatureReach=65;
  }
  c.pipboy.formNames["fists"]=15;
  auto fists=c.items.at(10);fists.formId=15;fists.weapon.animation=0;fists.weapon.maxRange=65;
  c.items[15]=fists;c.weapons.unarmedBase=.5f;c.weapons.unarmedMult=.05f;c.weapons.actors[43].skills[13]=5;
  Q230CombatWeapon prepared;prepared.definition=fists.weapon;prepared.ready=true;
  prepared.clips[1].stop=.5f;prepared.clips[1].textKeys={{.25f,"Hit"}};
  actor.combatWeapons.emplace(15,prepared);
  gPlayerSession=std::make_unique<Session>(c);
  auto& player=gPlayerSession->player;
  assert(player.SetActorHostile(42,fo3player::PlayerRef));
  Q230SimulateActor(actor,1);
  assert(actor.runtime.activity==fo3npc::Activity::Combat&&actor.runtime.combatWeaponForm==15&&
         !actor.runtime.equippedWeapon&&!player.ActorWeapon(42)&&!actor.runtime.moraleFlee);
  gQ210Head=actor.runtime.position;gQ210Head[1]+=1;gQ210Head[2]-=.5f;
  Q230SimulateActor(actor,1.1);assert(actor.runtime.pendingAttack&&player.Health()==100);
  Q230SimulateActor(actor,1.4);assert(player.Health()==(creature?95:99)&&shots==0);
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
  // These are actual IDLE records in the shipped master, not synthetic
  // stagger KF names. Resource loading occurs on the scene-prep worker.
  for(const char* name:{"mthitheada","mthittorsoa","mthitarmleft","mthitarmright","mthitlegleft","mthitlegright"}) {
    const auto id=catalog.pipboy.formNames.find(name);
    assert(id!=catalog.pipboy.formNames.end());
    const auto asset=catalog.pipboy.idleModels.find(id->second);
    assert(asset!=catalog.pipboy.idleModels.end()&&!asset->second.empty());
  }
  assert(defensive==161&&overrides==17);std::cout<<"Original Defensive packages="<<defensive<<" CNAM combat styles="<<overrides<<'\n';
  // Drive the original ambush trigger/timer, then exercise production enemy
  // acquisition with its resulting memberships. Positions/eyes are the same
  // explicit recording fixtures used by the other combat-runtime tests.
  auto& player=gPlayerSession->player;player.RegisterOriginalActors();
  uint32_t trigger=0;const auto script=catalog.pipboy.formNames.at("mq01cityambushscript");
  for(const auto& ref:catalog.references){const auto owner=catalog.eventBaseScripts.find(ref.second.base);if(owner!=catalog.eventBaseScripts.end()&&owner->second==script)trigger=ref.first;}
  assert(trigger);assert(player.DispatchReferenceEvent(trigger,"OnTriggerEnter",fo3player::PlayerRef,error)==fo3player::ScriptEventResult::Executed);
  for(int i=0;i<24;++i)player.PumpReferenceGameMode(.25,{trigger});
  const auto sarah=catalog.pipboy.formNames.at("mq01sarahref"),mutant=catalog.pipboy.formNames.at("mq01citysupermutant01ref");
  gQ230NpcActors.clear();
  for(uint32_t ref:{sarah,mutant}){Q230ActorVisual live;live.source.refFormId=ref;live.source.baseFormId=catalog.pipboy.targets.at(ref).base;live.stateRestored=true;live.runtime.position={ref==sarah?0.f:1.f,0,0};gQ230NpcActors.push_back(std::move(live));}
  const auto savedHead=gQ210Head;gQ210Head={100,0,100};blocked=false;blockSecondSightRay=false;partialCover=false;
  Q230SimulateActor(gQ230NpcActors[0],10);
  assert(gQ230NpcActors[0].runtime.combatTarget==mutant);
  gQ210Head=savedHead;gQ230NpcActors.clear();
  std::cout<<"Original ambush script faction changes feed production NPC combat target acquisition\n";

}
// Resume the original terminal's post-security save. Production combat
// chooses the enemy and follows the original interior NAVM. The host supplies
// visibility and bone observations, and loads the creature's shipped hit KF.
bool LoadQuestObjectClips(const std::string&,std::vector<fo3script::AnimationSequence>&);
static void HendersonSecurityCombat(const char* master,const char* save,const char* attackPath,const char* meshRoot){
  fo3player::Catalog catalog;std::string error;assert(fo3player::LoadCatalog(master,catalog,error));
  gPlayerSession=std::make_unique<Session>(std::move(catalog));
  auto& player=gPlayerSession->player;assert(player.Restore(save,error));
  const auto& defs=player.Definitions();const auto& ids=defs.pipboy.formNames;
  const auto robot=ids.at("tlhendersonrobotref"),mabel=ids.at("mabelhendersonref"),quest=ids.at("mq04");
  float aggression=0;assert(player.Snapshot().pipboy.quests.at(quest).stage==35&&player.ActorAIValue(robot,0,aggression)&&aggression==3);
  const auto& home=defs.pipboy.targets.at(ids.at("tlhendersonrobotterminalref"));
  gCurrentCellFormId=home.cell;gExteriorWorldspaceQ1890=home.world;
  gSceneCenterXQ1730=home.x;gSceneCenterYQ1730=home.y;gSceneFloorZQ1730=home.z;
  std::vector<Fo3NpcNavMeshQ240> meshes;assert(LoadFo3NpcNavigationQ240(home.cell,home.world,meshes,master));
  auto graph=std::make_shared<Q240NavigationGraph>();
  for(auto& mesh:meshes){
    graph->triangleOffsets.push_back(graph->triangleCount);graph->triangleCount+=mesh.triangles.size();
    graph->byForm[mesh.formId]=graph->meshes.size();graph->meshes.push_back(std::make_shared<Fo3NpcNavMeshQ240>(std::move(mesh)));
  }
  assert(graph->triangleCount);
  gQ230NpcActors.clear();activePackageTargets=&gQ230NpcActors;
  for(auto ref:{robot,mabel}){
    const auto& location=player.Snapshot().actors.at(ref);
    assert(location.cell==home.cell);
    Q230ActorVisual actor;const auto& placed=defs.pipboy.targets.at(ref);
    actor.stateRestored=true;actor.source.refFormId=ref;actor.source.baseFormId=placed.base;actor.source.scale=placed.scale;
    actor.source.creature=ref==robot;actor.navigationGraph=graph;
    actor.runtime.position=Q240ScenePosition(location.position);actor.runtime.yaw=location.yaw;
    gQ230NpcActors.push_back(std::move(actor));
  }
  auto& attacker=gQ230NpcActors.front();
  const auto* stats=player.ActorStatistics(robot);assert(stats&&stats->creature);
  std::ifstream stream(attackPath,std::ios::binary);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(stream),{}};
  Q230CombatWeapon prepared;auto& clip=prepared.clips[1];assert(fo3anim::DecodeClip(bytes,clip)&&clip.frequency>0);
  assert(std::any_of(clip.textKeys.begin(),clip.textKeys.end(),[&](const auto& key){return key.text=="Hit"&&key.time>=clip.start&&key.time<=clip.stop;}));
  const auto fists=ids.at("fists");prepared.definition=defs.items.at(fists).weapon;
  prepared.definition.maxRange=stats->creatureReach*attacker.source.scale;prepared.definition.attackMult=clip.frequency;
  prepared.ready=true;attacker.combatWeapons.emplace(fists,std::move(prepared));
  gQ210Head={1000,1000,1000};blocked=false;partialCover=false;blockSecondSightRay=false;loading=false;
  const auto origin=attacker.runtime.position;
  bool targeted=false,swung=false;
  for(unsigned frame=0;frame<6000&&player.ActorHealth(mabel)>0;++frame){
    Q230SimulateActor(attacker,5+frame*.05);
    targeted|=attacker.runtime.combatTarget==mabel;swung|=attacker.runtime.pendingAttack;
  }
  if(player.ActorHealth(mabel)>0)std::cerr<<"Security combat: target="<<attacker.runtime.combatTarget
    <<" hp="<<player.ActorHealth(mabel)<<" path="<<attacker.aiPathGame.size()<<" action="<<int(attacker.runtime.action)<<std::endl;
  assert(targeted&&swung&&attacker.runtime.position!=origin);
  assert(player.ActorHealth(mabel)==0&&player.Snapshot().pipboy.quests.at(quest).stage==40);
  assert(player.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(defs.pipboy,"MQ04.Task3Complete"))==4);
  std::cout<<"Henderson robot acquired Mabel and delivered the original attack hit over the house NAVM"<<std::endl;
  // Resume each trap before Mabel moves. Stop at the physical trigger edge
  // or at the distance required by the oven's GameMode block.
  for(const auto& route:std::array<std::array<const char*,3>,3>{{
        {".oven","mq04mabelblowupoven","mq04mabelovenref"},
        {".lamp","mq04mabelchandelierpatrol","mq04chandeliertriggerref"},
        {".skate","mq04mabelskatepatrol","mq04skatetriggerref"}}}){
    assert(player.Restore(std::string(save)+route[0],error));
    gQ230NpcActors.clear();
    Q230ActorVisual walker;walker.source.refFormId=mabel;walker.source.baseFormId=defs.pipboy.targets.at(mabel).base;
    const auto initial=player.Snapshot().actors.at(mabel);
    walker.runtime.position=Q240ScenePosition(initial.position);walker.runtime.yaw=initial.yaw;
    walker.navigationGraph=graph;walker.stateRestored=true;
    const auto trigger=ids.at(route[2]),package=ids.at(route[1]);
    const auto& at=defs.pipboy.targets.at(trigger);
    bool entered=false;float nearest=INFINITY;std::array<float,3> closest{};
    for(unsigned i=0;i<3000&&!entered;++i){
      Q240UpdateNpcPackage(walker,10+i*.05);
      if(walker.aiPackage!=package)continue;
      const auto here=Q240GamePosition(walker);
      const float distance=std::hypot(here[0]-at.x,here[1]-at.y,here[2]-at.z);
      if(distance<nearest){nearest=distance;closest=here;}
      if(trigger==ids.at("mq04mabelovenref"))
        entered=std::hypot(here[0]-at.x,here[1]-at.y,here[2]-at.z)<100;
      else{
        const auto changes=player.ScriptActorTriggerChanges(mabel,at.cell,at.world,here,walker.runtime.yaw);
        entered=std::find(changes.begin(),changes.end(),std::make_pair(trigger,true))!=changes.end();
      }
    }
    if(!entered)std::cerr<<route[1]<<" approach: package="<<walker.aiPackage<<" expected="<<package
      <<" procedure="<<int(walker.runtime.procedure)<<" nearest="<<nearest<<" point="<<closest[0]<<","<<closest[1]<<","<<closest[2]<<" trigger="<<at.x<<","<<at.y<<","<<at.z<<" leg="<<walker.aiSequence<<" index="<<walker.aiPathIndex<<" path="<<walker.aiPathGame.size()<<std::endl;
    assert(entered&&walker.runtime.position!=Q240ScenePosition(initial.position));
    if(trigger==ids.at("mq04mabelovenref")){
      Q230PersistActor(walker);
      const std::string ignitionSave=std::string(save)+".ignition";
      assert(player.Save(ignitionSave,error)&&player.Restore(ignitionSave,error));
      const auto& position=player.Snapshot().actors.at(mabel).position;
      assert(std::hypot(position[0]-at.x,position[1]-at.y,position[2]-at.z)<100);
      const auto armed=fo3dialogue::VariableKey(defs.pipboy,"MQ04MabelOvenRef.OvenRigged");
      assert(player.Snapshot().pipboy.dialogueVariables.at(armed)==2);
      const auto& blast=defs.explosions.at(ids.at("mq04ovenexplosion"));
      const auto& enchant=defs.pipboy.enchantments.at(blast.enchantment);
      assert(blast.damage==500 && blast.radius==200 && enchant.type==2 && enchant.effects.size()==2);
      const auto& burn=enchant.effects[0];
      const auto& visual=enchant.effects[1];
      const auto& damage=defs.pipboy.magic.at(burn.id);
      const auto& effect=defs.pipboy.magic.at(visual.id);
      assert(damage.archetype==fo3pipdata::MagicValue && damage.av==16 && (damage.flags&fo3pipdata::MagicDetrimental));
      assert(burn.magnitude==2 && burn.duration==5 && burn.range==1);
      assert(effect.archetype==fo3pipdata::MagicScript && visual.duration==5 &&
             effect.script==ids.at("flamereffectscript") && defs.pipboy.scripts.count(effect.script));
      // Isolate the physical-hit phase. The actual ignition save remains the
      // input to the complete PlaceAtMe transaction, including its enchantment.
      fo3player::Player impact(player);
      const float range=std::hypot(position[0]-at.x,position[1]-at.y,position[2]-at.z);
      const float raw=blast.PlacedHitDamage(range,defs.weapons);
      const float applied=raw*(1-impact.DamageResistance(mabel)/100.f);
      const float health=impact.ActorHealth(mabel);
      assert(range>0 && raw>375 && raw<500 && health>0 && applied>=health);
      assert(blast.PlacedHitDamage(blast.radius,defs.weapons)==0 &&
             blast.PlacedHitDamage(blast.radius+1,defs.weapons)==0);
      float received=0;
      assert(impact.ApplyPlacedExplosionHealthHit(trigger,ids.at("mq04ovenexplosion"),mabel,false,received));
      assert(received==health); // The original EXPL ignores the LOS gate.
      assert(impact.ApplyPlacedExplosionHealthHit(trigger,ids.at("mq04ovenexplosion"),mabel,false,received)&&received==0);
      assert(impact.ActorHealth(mabel)==0 && impact.Snapshot().actors.at(mabel).dead);
      float killer=1;
      assert(impact.IsActorKiller(mabel,fo3player::PlayerRef,killer)&&!killer);
      const std::string impactSave=std::string(save)+".impact";
      assert(impact.Save(impactSave,error)&&impact.Restore(impactSave,error));
      assert(impact.ActorHealth(mabel)==0 && player.ActorHealth(mabel)==health);
      // Execute both original EFITs on the impacted target. The SCPT owns
      // the different start branches and each stop instruction at expiry.
      const auto flames=ids.at("flames01"),smoke=ids.at("smoke01");
      const auto& fire=defs.pipboy.effectShaders.at(flames);
      assert(fire.particleTexture=="Effects\\FXFlameSmallSub02.dds" &&
             defs.pipboy.effectShaders.at(smoke).particleTexture=="Effects\\FXDustSmoke01.dds");
      const auto& models=defs.pipboy.debrisModels.at(fire.addon);
      assert(models.size()==4);
      for(const auto& model:models){
        assert(model.percentage==25&&!model.path.empty());
        if(!meshRoot)continue;
        auto path=fo3script::Lower(model.path);
        std::replace(path.begin(),path.end(),'\\','/');
        const std::string file=std::string(meshRoot)+"/"+path;
        std::vector<Fo3StaticNifMesh> shapes;
        assert(LoadFo3StaticNifMeshes(file,shapes,true)&&shapes.size()==1);
        std::vector<uint8_t> bytes;assert(LoadFalloutMeshFile(file,bytes,nullptr));
        fo3anim::Skeleton hierarchy;std::vector<int> mapping;
        std::vector<fo3anim::Clip> clips;
        assert(fo3anim::DecodeObjectAnimation(bytes,hierarchy,mapping,clips,true)&&clips.size()==1);
        const auto& clip=clips.front();
        assert(clip.DirectControllers()&&clip.tracks.size()==1&&clip.properties.size()==2);
        const auto& shape=shapes.front();
        const int bone=mapping.at(shape.shapeBlock);
        assert(bone>=0&&!shape.indices.empty()&&shape.texcoords.size()*3==shape.positions.size()*2);
        const int parent=hierarchy.bones.at(bone).parent;
        assert(parent>=0&&hierarchy.bones.at(parent).billboardMode==5);
        assert(hierarchy.bones.at(parent).parent>=0);
        fo3anim::Pose start,scroll;assert(fo3anim::Sample(hierarchy,clip,0.,start));
        assert(fo3anim::Sample(hierarchy,clip,2.,scroll)&&scroll.delta.at(bone)!=start.delta.at(bone));
        for(const auto& uv:scroll.properties){
          assert(uv.bone==bone);
          if(uv.kind==fo3anim::PropertyKind::DiffuseOffsetU)assert(std::abs(uv.value[0]+2.f/6.7f)<.001f);
          else {assert(uv.kind==fo3anim::PropertyKind::DiffuseOffsetV);
            assert(std::abs(uv.value[0]-(2.f/(41.f/30.f)-1.f))<.001f);}
        }
        assert(scroll.properties.size()==2);
        auto texture=fo3script::Lower(shape.diffuseTexturePath);
        std::replace(texture.begin(),texture.end(),'\\','/');
        assert(texture.find("effects/fxfirescroll.dds")!=std::string::npos);
      }
      if(meshRoot)std::cout<<"All four original flame addon meshes retained billboard parents, animated geometry and independent U/V scroll periods"<<std::endl;
      if(!impact.BeginPlacedEnchantment(blast.enchantment,mabel,error)){
        std::cerr<<"Could not start original enchantment: "<<error<<std::endl;std::abort();
      }
      auto has=[&](uint32_t shader){return impact.Snapshot().magicShaders.count({mabel,shader})!=0;};
      assert(has(flames)&&has(smoke)&&impact.Snapshot().placedEffects.size()==2);
      assert(impact.AdvancePlacedEffects(2,error)&&impact.Save(impactSave+".burn",error));
      assert(impact.Restore(impactSave+".burn",error)&&has(flames)&&has(smoke));
      assert(impact.Snapshot().magicShaders.at({mabel,smoke}).elapsed==2);
      assert(impact.AdvancePlacedEffects(2.75,error)&&has(flames)&&has(smoke));
      assert(impact.AdvancePlacedEffects(.25,error)&&!has(flames)&&!has(smoke)&&impact.Snapshot().placedEffects.empty());
      std::cout<<"FlamerEffectScript started flame and smoke on dead Mabel; both stopped at five seconds across reload"<<std::endl;
      std::cout<<"Isolated oven health impact at "<<range<<" units: "<<raw<<" raw damage, "
               <<health<<" HP before impact; unowned death persisted across reload"<<std::endl;
      // On a living target the same SCPT starts only smoke and the first
      // EFIT actually drains health for its five-second duration.
      fo3player::Player living(player);
      const auto pc=fo3player::PlayerRef;
      const float initialHealth=living.ActorHealth(pc);
      float resistance=0;assert(living.ActorValue(pc,damage.resistance,resistance));
      const float perSecond=burn.magnitude*(1-resistance/100.f);
      assert(perSecond>0&&initialHealth>perSecond*5);
      assert(living.BeginPlacedEnchantment(blast.enchantment,pc,error));
      assert(living.Snapshot().magicShaders.count({pc,smoke})&&!living.Snapshot().magicShaders.count({pc,flames}));
      assert(living.AdvancePlacedEffects(2,error)&&std::abs(living.ActorHealth(pc)-(initialHealth-2*perSecond))<.001f);
      assert(living.Save(impactSave+".living",error)&&living.Restore(impactSave+".living",error));
      assert(living.AdvancePlacedEffects(4,error)&&std::abs(living.ActorHealth(pc)-(initialHealth-5*perSecond))<.001f);
      assert(living.Snapshot().magicShaders.empty()&&living.Snapshot().placedEffects.empty());
      const auto& stairs=defs.pipboy.targets.at(ids.at("mq04skatetriggerref"));
      assert(stairs.cell==at.cell&&std::hypot(stairs.x-at.x,stairs.y-at.y,stairs.z-at.z)>blast.radius);
      player.SetScriptPlayerLocation(stairs.x,stairs.y,stairs.z,stairs.cell,stairs.world);
      auto oven=player.PrepareReferenceEventTransaction(trigger,"GameMode",0,error,fo3player::ScriptProgramCapture::EnteredPrograms);
      if(!oven)std::cerr<<"Oven GameMode: "<<error<<std::endl;
      assert(oven&&oven->HasReferenceChanges());
      fo3script::ScriptMasterMatch ovenMatch;std::atomic<bool> cancel{false};
      assert(fo3script::MatchScriptProgramsToMaster(master,oven->ExecutedScripts(),ovenMatch,cancel,error));
      assert(player.CommitScriptTransaction(*oven));
      const auto task=fo3dialogue::VariableKey(defs.pipboy,"MQ04.Task3Complete");
      assert(player.Snapshot().pipboy.dialogueVariables.at(task)==1&&player.ActorHealth(mabel)==health);
      for(const auto key:{"mq04ovenfireref","mq04ovensmokeref","tlhendhousesmokecheck"}){
        bool disabled=true;assert(player.ReferenceDisabled(ids.at(key),disabled,error)&&!disabled);
      }
      const auto queued=ignitionSave+".queued";
      assert(player.Save(queued,error)&&player.Restore(queued,error));
      const auto& pending=player.Snapshot().placedBlastHits;
      assert(std::count_if(pending.begin(),pending.end(),[&](const auto& h){return h.target==mabel;})==1);
      const float karma=player.Snapshot().karma;
      if(!player.AdvancePlacedEffects(.01,error)){std::cerr<<"Oven impact: "<<error<<std::endl;std::abort();}
      assert(player.ActorHealth(mabel)==0&&player.IsActorKiller(mabel,fo3player::PlayerRef,killer)&&!killer);
      assert(player.Snapshot().placedBlastHits.empty()&&player.Snapshot().pipboy.quests.at(quest).stage==40);
      // The stage result and Mabel's OnDeath each issue RewardKarma -50.
      const float afterDeath=std::max(-1000.f,karma-100);
      assert(player.Snapshot().karma==afterDeath&&player.Snapshot().magicShaders.count({mabel,flames}));
      assert(player.AdvancePlacedEffects(2,error)&&player.Save(queued+".burn",error)&&player.Restore(queued+".burn",error));
      assert(player.Snapshot().magicShaders.count({mabel,smoke})&&player.AdvancePlacedEffects(3,error));
      assert(player.Snapshot().magicShaders.empty()&&player.Snapshot().placedEffects.empty()&&player.Snapshot().karma==afterDeath);
      std::cout<<"Oven GameMode placed original EXPL, delivered one unowned death after reload, and advanced MQ04 to 40; burn expired across reload"<<std::endl;
    }
    if(meshRoot&&trigger==ids.at("mq04chandeliertriggerref")){
      Q230PersistActor(walker);
      const auto lamp=ids.at("mq04chandelierref");
      auto registry=std::make_shared<fo3script::AnimationRegistry>();
      assert(LoadQuestObjectClips(std::string(meshRoot)+"/clutter/questitems/mq04chandellier01.nif",(*registry)[lamp]));
      player.SetScriptAnimationRegistry(registry);
      auto event=player.PrepareReferenceEventTransaction(trigger,"OnTriggerEnter",mabel,error,fo3player::ScriptProgramCapture::EnteredPrograms);
      assert(event);
      fo3script::ScriptMasterMatch match;std::atomic<bool> cancel{false};
      assert(fo3script::MatchScriptProgramsToMaster(master,event->ExecutedScripts(),match,cancel,error));
      assert(player.CommitScriptTransaction(*event));
      player.AcknowledgeScriptTrigger(trigger,true,mabel);
      const auto* run=player.ScriptAnimation(lamp);
      assert(run&&run->group=="unequip"&&run->duration>0&&!run->loop);
      assert(player.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(defs.pipboy,"MQ04ChandelierTriggerRef.Step"))==1);
      player.SetScriptRuntimeClock(run->started+run->duration*.5,0);
      assert(player.Save(std::string(save)+".fall",error));
      assert(player.ActorHealth(mabel)>0&&player.Snapshot().pipboy.quests.at(quest).stage==35);
      std::cout<<"Original navigation crossing started the chandelier and saved the drop halfway through Unequip"<<std::endl;
    }
    if(trigger==ids.at("mq04skatetriggerref")){
      Q230PersistActor(walker);
      auto event=player.PrepareReferenceEventTransaction(trigger,"OnTriggerEnter",mabel,error,fo3player::ScriptProgramCapture::EnteredPrograms);
      if(!event)std::cerr<<"skate trigger: "<<error<<std::endl;
      assert(event);fo3script::ScriptMasterMatch match;std::atomic<bool> cancel{false};
      assert(fo3script::MatchScriptProgramsToMaster(master,event->ExecutedScripts(),match,cancel,error));
      assert(player.CommitScriptTransaction(*event));
      player.AcknowledgeScriptTrigger(trigger,true,mabel);
      assert(player.ActorHealth(mabel)==0);
      const auto pushes=player.Snapshot().ragdollPushes.at(mabel);
      assert(pushes.size()==1&&pushes.front().speed>0);
      const auto pushSave=std::string(save)+".push";
      assert(player.Save(pushSave,error)&&player.Restore(pushSave,error));
      assert(player.Snapshot().ragdollPushes.at(mabel)==pushes);
      for(unsigned i=0;i<80&&player.Snapshot().pipboy.quests.at(quest).stage==35;++i)
        player.PumpWorldAwareGameMode(.25,{},[&](auto plan){return player.CommitScriptTransaction(*plan);});
      assert(player.Snapshot().pipboy.quests.at(quest).stage==40);
      assert(player.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(defs.pipboy,"MQ04.Task3Complete"))==2);
      std::cout<<"Original skate trigger pushed and killed Mabel; MQ04 advanced to 40 across reload"<<std::endl;
    }
    std::cout<<route[1]<<" selected and reached its authored trigger or interaction radius"<<std::endl;
  }
  activePackageTargets=&packageTargets;
}

static void CreatureInventoryWeapon(){
  Prepare();
  auto catalog=gPlayerSession->player.Definitions();
  catalog.weapons.actors.at(43).creature=true;
  gPlayerSession=std::make_unique<Session>(catalog);
  auto& actor=gQ230NpcActors.front();actor.source.creature=true;
  auto& player=gPlayerSession->player;
  assert(player.PrepareActorInventory(42));
  assert(Q230SelectWeapon(actor)&&player.ActorWeapon(42)->formId==10);
}
static void WeaponCommitAndMeleeTests(){
  Prepare();auto& actor=gQ230NpcActors[0];auto c=gPlayerSession->player.Definitions();
  auto melee=c.items.at(10);melee.formId=20;melee.weapon.animation=1;melee.weapon.ammo=0;melee.weapon.clip=0;
  melee.weapon.maxRange=30;melee.weapon.damage=50;c.items[20]=melee;c.actorInventories[43].entries.push_back({20,0,1,1,1,false});
  auto prepared=actor.combatWeapons.at(10);prepared.definition=melee.weapon;actor.combatWeapons[20]=prepared;
  gPlayerSession=std::make_unique<Session>(c);assert(gPlayerSession->player.PrepareActorInventory(42));
  assert(Q230SelectWeapon(actor,100)&&gPlayerSession->player.ActorWeapon(42)->formId==10);
  actor.runtime.pendingAttack=true;assert(Q230SelectWeapon(actor,10)&&gPlayerSession->player.ActorWeapon(42)->formId==10);
  actor.runtime.pendingAttack=false;actor.runtime.reloadUntil=12;
  assert(Q230SelectWeapon(actor,10)&&gPlayerSession->player.ActorWeapon(42)->formId==10);
  actor.runtime.reloadUntil=0;actor.runtime.actionUntil=12;actor.aiLastUpdate=11;
  assert(Q230SelectWeapon(actor,10)&&gPlayerSession->player.ActorWeapon(42)->formId==10);
  actor.runtime.actionUntil=0;assert(Q230SelectWeapon(actor,10)&&gPlayerSession->player.ActorWeapon(42)->formId==20);

  c.items[20].weapon.maxRange=200;actor.combatWeapons[20].definition.maxRange=200;
  auto& style=c.weapons.styles[200];style.valid=style.meleeValid=true;style.restrictions=1;
  style.flags=1;style.meleeAttackChance=0;style.meleeHoldMin=style.meleeHoldMax=1;c.weapons.defaultCombatStyle=200;
  gPlayerSession=std::make_unique<Session>(c);assert(gPlayerSession->player.PrepareActorInventory(42));
  actor.runtime.equippedWeapon=0;actor.stateRestored=true;actor.runtime.EndDialogue();actor.runtime.BeginCombat(0x14);actor.aiLastUpdate=0;
  Q230SimulateActor(actor,1);assert(actor.runtime.action==fo3npc::CombatAction::Wait&&!actor.runtime.pendingAttack&&actor.runtime.nextMeleeDecision==2);
  Q230SimulateActor(actor,1.5);assert(!actor.runtime.pendingAttack&&actor.runtime.nextMeleeDecision==2);
  c.weapons.styles[200].meleeAttackChance=100;gPlayerSession=std::make_unique<Session>(c);
  assert(gPlayerSession->player.PrepareActorInventory(42));actor.runtime.equippedWeapon=0;
  Q230SimulateActor(actor,2.1);assert(actor.runtime.pendingAttack);
  const float health=gPlayerSession->player.Health();Q230SimulateActor(actor,2.2);
  assert(!actor.runtime.pendingAttack&&gPlayerSession->player.Health()<health&&shots==0);
}
static void CoverGeometryTests(){
  Prepare();auto& actor=gQ230NpcActors[0];const std::array<float,3> threat{1000,2000,120};
  partialCover=true;actor.runtime.nextPath=0;
  assert(!Q230TacticalRoute(actor,threat,0,INFINITY,false,true,1)); // Torso hidden, head exposed.
  partialCover=false;blocked=true;captureCoverRays=true;actor.runtime.nextPath=0;
  assert(Q230TacticalRoute(actor,threat,0,INFINITY,false,true,2));
  const auto hide=actor.runtime.coverPoint;
  assert(actor.runtime.hasCoverPoint);
  actor.runtime.position=Q240ScenePosition(hide);
  actor.runtime.combatMovement=fo3npc::CombatAction::Reposition;actor.runtime.nextPath=0;
  blocked=false;
  assert(Q230TacticalRoute(actor,threat,0,INFINITY,false,false,3)&&actor.runtime.hasFiringPoint);
  actor.runtime.position=Q240ScenePosition(actor.runtime.firingPoint);actor.runtime.nextPath=0;blocked=true;
  assert(Q230TacticalRoute(actor,threat,0,INFINITY,false,true,4)&&actor.runtime.coverPoint==hide);
  assert(!coverRayOrigins.empty());for(float y:coverRayOrigins)assert(y==Q240ScenePosition(threat)[1]);
}
static void ConcurrentWeaponTests(){
  for(int scenario=0;scenario<4;++scenario){
    Prepare();auto& a=gQ230NpcActors[0];auto& player=gPlayerSession->player;
    assert(player.PrepareActorInventory(42)&&Q230SelectWeapon(a));
    a.stateRestored=true;a.runtime.EndDialogue();a.runtime.BeginCombat(0x14);
    a.runtime.lastSeen=1;a.runtime.lastThreat=gQ210Head;a.aiLastUpdate=1;
    a.runtime.combatMovement=fo3npc::CombatAction::Reposition;a.runtime.action=fo3npc::CombatAction::Reload;
    a.aiPathGame={{1140,2040,20}};a.aiPathSurfaces={{0,0}};
    a.runtime.reloadUntil=2;a.runtime.actionStart=1;a.runtime.lastActionTime=1;
    a.animations[size_t(fo3npc::Animation::Reload)].textKeys={{.1f,"Sound: ReloadOut"},{.4f,"Sound: ReloadIn"}};
    if(scenario==3){a.runtime.moraleFlee=true;a.runtime.nextTactic=100;a.runtime.fleeUntil=100;a.runtime.nextPath=100;}
    const auto before=a.runtime.position;Q230SimulateActor(a,1.1);
    assert(a.runtime.position!=before&&a.runtime.reloadUntil==2&&!a.runtime.pendingAttack&&shots==0);
    if(scenario==3)assert(a.runtime.action==fo3npc::CombatAction::Flee);
    assert(actionSounds.empty());
    Q230SimulateActor(a,1.3);Q230SimulateActor(a,1.3);
    assert(actionSounds==std::vector<std::string>{"ReloadOut"});
    if(scenario==1)blocked=true;
    if(scenario==2)gQ210Head[2]-=20;
    Q230SimulateActor(a,2.1);
    assert(a.runtime.reloadUntil==0&&player.ActorWeapon(42)->loadedRounds==3&&shots==0);
    assert((actionSounds==std::vector<std::string>{"ReloadOut","ReloadIn"}));
    const auto contents=*player.ContainerContents(42);
    Q230SimulateActor(a,2.11);
    // No second magazine transaction on the following combat tick.
    const auto* after=player.ContainerContents(42);
    assert(after&&after->size()==contents.size());
    for(size_t j=0;j<after->size();++j)if((*after)[j].formId==11)
      assert((*after)[j].count==contents[j].count);
  }
  // Starting the reload must also precede pursuit/search/cover early returns.
  for(int scenario=0;scenario<4;++scenario){
    Prepare();auto& a=gQ230NpcActors[0];
    assert(gPlayerSession->player.PrepareActorInventory(42)&&Q230SelectWeapon(a));
    a.stateRestored=true;a.runtime.EndDialogue();a.runtime.BeginCombat(0x14);a.aiLastUpdate=1;
    a.runtime.lastSeen=1;a.runtime.lastThreat=gQ210Head;
    a.aiPathGame={{1140,2040,20}};a.aiPathSurfaces={{0,0}};
    if(scenario==0)gQ210Head[2]-=20;
    if(scenario==1)blocked=true;
    if(scenario==2)a.runtime.combatMovement=fo3npc::CombatAction::Cover;
    if(scenario==3){a.runtime.moraleFlee=true;a.runtime.nextTactic=100;a.runtime.fleeUntil=100;a.runtime.nextPath=100;}
    Q230SimulateActor(a,1.1);
    assert(a.runtime.reloadUntil>1.1&&!a.runtime.pendingAttack&&shots==0);
    const double end=a.runtime.reloadUntil;Q230SimulateActor(a,end+.1);
    assert(a.runtime.reloadUntil==0&&gPlayerSession->player.ActorWeapon(42)->loadedRounds==3);
  }
  Prepare();auto c=gPlayerSession->player.Definitions();
  auto& style=c.weapons.styles[200];style.valid=style.manoeuvresValid=true;
  style.dodgeChance=100;style.leftRightChance=100;style.rangeMax=1;
  style.manoeuvreTimers.fill(1);c.weapons.defaultCombatStyle=200;
  gPlayerSession=std::make_unique<Session>(c);auto& a=gQ230NpcActors[0];
  assert(gPlayerSession->player.PrepareActorInventory(42)&&Q230SelectWeapon(a));
  assert(gPlayerSession->player.ReloadActorWeapon(42,a.runtime.equippedWeapon));
  a.stateRestored=true;a.runtime.EndDialogue();a.runtime.BeginCombat(0x14);a.aiLastUpdate=1;
  a.runtime.pendingAttack=true;a.runtime.pendingHit=1.5;a.runtime.actionStart=1;a.runtime.actionUntil=1.6;
  Q230SimulateActor(a,1.1);
  assert(a.runtime.nextManoeuvre>1.1&&a.runtime.pendingAttack&&a.runtime.pendingHit==1.5&&shots==0);
}
#ifdef FO3_NPC_DETECTION_HOST_TEST
static void DetectionIntegration(){
 Prepare();auto& p=gPlayerSession->player;auto& a=gQ230NpcActors[0];a.runtime.EndDialogue();a.stateRestored=true;
 auto special=p.Snapshot().special;auto skills=p.Snapshot().skills;skills[size_t(fo3player::Skill::Sneak)]=100;assert(p.SetPlayerAttributes(special,skills));p.SetSneaking(true);detectionLight=0;
 Q230SimulateActor(a,1);assert(!a.runtime.combatTarget&&a.playerDetection.Hud()==fo3sneak::Indicator::Hidden);
 detectionLight=100;Q230SimulateActor(a,2);assert(a.runtime.combatTarget==0x14&&a.playerDetection.detected&&a.playerDetection.Hud()==fo3sneak::Indicator::Danger);
 const auto known=a.runtime.lastThreat;const auto before=shots;blocked=true;gQ210Head[0]+=10;
 Q230SimulateActor(a,3);assert(!a.playerDetection.detected&&a.playerDetection.Hud()==fo3sneak::Indicator::Caution&&a.runtime.lastThreat==known&&!a.runtime.pendingAttack&&shots==before);
 Q230SimulateActor(a,14);assert(!a.runtime.combatTarget&&a.playerDetection.Hud()==fo3sneak::Indicator::Hidden);
 blocked=false;gQ210Head=a.runtime.position;gQ210Head[1]+=1;gQ210Head[2]-=1;a.playerDetection={};detectionLight=100;
 Q230ObservePlayer(a,false,15);assert(a.playerDetection.Hud()==fo3sneak::Indicator::Detected);
 gQ210Head[2]+=2;a.playerDetection={};Q230ObservePlayer(a,true,16);assert(!a.playerDetection.detected); // Behind the authored 190-degree cone.
 std::cout<<"Production player detection: hide, acquire, lose, last-known search, forget and friendly detection passed\n";
}

static void OccludedEvidence(const std::unordered_map<std::string,float>& settings){
 Prepare();
 auto catalog=gPlayerSession->player.Definitions();
 catalog.sneakSettings=settings;
 catalog.weapons.actors[43].perception=5;
 gPlayerSession=std::make_unique<Session>(std::move(catalog));
 auto& actor=gQ230NpcActors.front();
 actor.runtime.EndDialogue();
 auto& player=gPlayerSession->player;
 blocked=true;detectionLight=0;
 gPlayerDetectionShot=gPlayerDetectionStep={};
 Q230SimulateActor(actor,1);
 assert(!actor.runtime.combatTarget&&!actor.runtime.search.active&&
        actor.playerDetection.Hud()==fo3sneak::Indicator::Hidden);
 const fo3sneak::Point gunshot{.8f,.1f,-4.f};
 gPlayerDetectionShot={gunshot,2,settings.at("iSoundLevelLoud")};
 gQ210Head[0]+=10;
 Q230SimulateActor(actor,2.1);
 assert(actor.runtime.search.investigating&&!actor.runtime.combatTarget&&shots==0&&
        actor.playerDetection.Hud()==fo3sneak::Indicator::Caution&&
        actor.playerDetection.lastKnown==gunshot&&actor.runtime.search.origin==Q230GameThreat(gunshot));
 const auto searchOrigin=actor.runtime.search.origin;
 Q240UpdateNpcPackage(actor,2.1);
 assert(actor.runtime.search.origin==searchOrigin&&actor.runtime.search.investigating);
 gQ210Head[0]+=2;
 Q230SimulateActor(actor,3);
 assert(actor.playerDetection.lastKnown==gunshot&&actor.playerDetection.expires==12);
 Q230SimulateActor(actor,13);
 assert(!actor.runtime.search.active&&!actor.runtime.combatTarget&&actor.runtime.CanTalk());

 gQ210Head[0]-=12;
 gPlayerDetectionStep={{.5f,.1f,-4.f},14,2};
 Q230SimulateActor(actor,14.1);
 assert(actor.runtime.search.investigating&&actor.playerDetection.lastKnown==gPlayerDetectionStep.position);
 const fo3sneak::Point attackOrigin{1.2f,.2f,-4.6f};
 assert(player.ApplyAttack(0x14,10,42,5));
 actor.playerDetection.Hit(settings,15,attackOrigin);
 Q230SimulateActor(actor,15.1);
 assert(actor.runtime.combatTarget==0x14&&!actor.runtime.search.investigating&&
        actor.runtime.lastThreat==attackOrigin&&shots==0);
 gPlayerDetectionShot=gPlayerDetectionStep={};
}

static void SharedSighting(){
 Prepare();
 auto catalog=gPlayerSession->player.Definitions();
 catalog.pipboy.dialogueActors[43].factions[55]=0;
 AddNpc(catalog,70,71);
 catalog.pipboy.dialogueActors[71].aiData[14]=1;
 gPlayerSession=std::make_unique<Session>(std::move(catalog));
 auto& scout=gQ230NpcActors[0];auto& ally=gQ230NpcActors[1];
 ally.runtime.position[2]=scout.runtime.position[2]+1;
 scout.runtime.EndDialogue();detectionLight=100;
 Q230SimulateActor(scout,1);
 const auto sight=scout.playerDetection.lastKnown;
 assert(scout.runtime.combatTarget==fo3player::PlayerRef&&scout.playerDetection.detected&&ally.playerDetection.phase==fo3sneak::Phase::Alert);
 blocked=true;gQ210Head[0]+=4;
 Q230SimulateActor(ally,1.1);
 assert(ally.runtime.search.investigating&&ally.playerDetection.lastKnown==sight&&
        !ally.playerDetection.detected&&!ally.runtime.combatTarget);
 // The investigating ally is checking the original evidence. The scout that
 // loses sight of the same player should immediately probe a different place.
 Q230SimulateActor(scout,2);
 assert(scout.runtime.search.active&&scout.runtime.search.nextDirection>0&&
        (scout.aiPathGame.empty()||scout.aiPathGame.back()!=ally.runtime.search.origin));
}
static void ChosenTargetAndSearch(){
 Prepare();
 auto catalog=gPlayerSession->player.Definitions();
 AddNpc(catalog,70,71);AddNpc(catalog,80,81);
 gQ230NpcActors[2].runtime.position[2]-=2;
 gPlayerSession=std::make_unique<Session>(std::move(catalog));
 auto& actor=gQ230NpcActors.front();
 actor.runtime.EndDialogue();
 gQ210Head[2]-=100;
 sightRayCount=0;blockSecondSightRay=true;
 Q230SimulateActor(actor,1);
 blockSecondSightRay=false;
 auto firstTarget=gQ230NpcActors[1].runtime.position;firstTarget[1]+=1;
 assert(actor.runtime.combatTarget==70&&actor.runtime.lastThreat==firstTarget&&actor.runtime.search.active);

 Prepare();
 auto& searcher=gQ230NpcActors.front();
 searcher.runtime.EndDialogue();
 searcher.stateRestored=true;searcher.runtime.BeginCombat(0x14);
 searcher.runtime.lastThreat=searcher.runtime.position;
 searcher.runtime.lastSeen=searcher.aiLastUpdate=1;
 searcher.playerDetection.phase=fo3sneak::Phase::Lost;
 searcher.playerDetection.lastKnown=searcher.runtime.lastThreat;
 searcher.playerDetection.expires=11;
 assert(gPlayerSession->player.PrepareActorInventory(42));
 blocked=true;
 const auto queries=gQ240PathsPlanned;
 Q230SimulateActor(searcher,2);Q230SimulateActor(searcher,2.1);Q230SimulateActor(searcher,2.9);
 assert(gQ240PathsPlanned-queries==1&&searcher.runtime.speed==0);
 Q230SimulateActor(searcher,3.2);
 assert(searcher.runtime.search.checkedCount==1&&searcher.aiPathIndex<searcher.aiPathGame.size()&&
        searcher.aiPathGame.back()!=searcher.runtime.search.origin);
 blocked=false;detectionLight=100;
 Q230SimulateActor(searcher,4);
 assert(searcher.runtime.combatTarget==0x14&&searcher.playerDetection.detected&&!searcher.runtime.search.active);
}
#endif
int main(int argc,char** argv){
#ifdef FO3_NPC_DETECTION_HOST_TEST
 auto settings=fo3sneak::NativeSettings();
 if(argc>1){
   fo3player::Catalog original;std::string error;
   assert(fo3player::LoadCatalog(argv[1],original,error));
   settings=original.sneakSettings;
 }
 DetectionIntegration();
 OccludedEvidence(settings);
 SharedSighting();
 ChosenTargetAndSearch();
 std::cout<<"Silent occlusion, sound and hit origins, target selection, local search and reacquisition passed\n";
 return 0;
#endif
  EndIdleRetirement();
  ScriptOrderedFists();
  ScriptOrderedFists(true);
  CreatureInventoryWeapon();WeaponCommitAndMeleeTests();CoverGeometryTests();ConcurrentWeaponTests();
  {
    Prepare();auto& actor=gQ230NpcActors[0];actor.runtime.EndDialogue();actor.runtime.BeginCombat(0x14);
    actor.runtime.pendingAttack=true;actor.runtime.reloadUntil=20;actor.runtime.navigationDoor=77;
    actor.runtime.packageEndIdle=701;actor.runtime.packageEndIdleUntil=4;
    actor.runtime.suspendedPackageWait=3;actor.aiPathGame={{1,2,3}};actor.aiPathSurfaces={{0,0}};actor.aiPathIndex=1;
    Q230StopScriptCombat(actor,5);
    assert(actor.runtime.activity==fo3npc::Activity::Package&&actor.runtime.package==50&&actor.runtime.combatTarget==0);
    assert(!actor.runtime.pendingAttack&&actor.runtime.reloadUntil==0&&actor.runtime.navigationDoor==0);
    assert(actor.runtime.packageEndIdle==0&&actor.runtime.packageEndIdleUntil<0);
    assert(actor.aiPathGame.empty()&&actor.aiPathSurfaces.empty()&&actor.aiPathIndex==0&&actor.aiRepathAt==8);
    actor.runtime.activity=fo3npc::Activity::Dead;Q230StopScriptCombat(actor,6);assert(actor.runtime.activity==fo3npc::Activity::Dead);
  }
  {
    Prepare();auto& actor=gQ230NpcActors[0];assert(gPlayerSession->player.PrepareActorInventory(42));
    assert(Q230SelectWeapon(actor));assert(gPlayerSession->player.ReloadActorWeapon(42,actor.runtime.equippedWeapon));
    actor.stateRestored=true;actor.runtime.EndDialogue();actor.runtime.BeginCombat(0x14);
    actor.runtime.lastSeen=1;actor.runtime.lastThreat=gQ210Head;actor.aiLastUpdate=1;
    actor.runtime.combatMovement=fo3npc::CombatAction::Reposition;actor.runtime.action=fo3npc::CombatAction::Reposition;
    actor.aiPathGame={{1140,2040,20}};actor.aiPathSurfaces={{0,0}};
    const auto before=actor.runtime.position;Q230SimulateActor(actor,1.1);
    assert(actor.runtime.position!=before&&actor.runtime.pendingAttack&&shots==0);
    const auto windup=actor.runtime.position;Q230SimulateActor(actor,1.2);
    assert(actor.runtime.position!=windup&&shots==1&&actor.runtime.speed>0);
    blocked=true;actor.runtime.pendingAttack=false;actor.runtime.combatMovement=fo3npc::CombatAction::Cover;
    actor.runtime.action=fo3npc::CombatAction::Cover;Q230SimulateActor(actor,1.3);
    assert(actor.runtime.action==fo3npc::CombatAction::Cover);
    actor.aiPathIndex=actor.aiPathGame.size();actor.runtime.coverUntil=20;Q230SimulateActor(actor,2);
    assert(actor.runtime.combatMovement==fo3npc::CombatAction::Cover&&actor.runtime.action!=fo3npc::CombatAction::Search);
    actor.runtime.combatMovement=fo3npc::CombatAction::Acquire;actor.runtime.moraleFlee=true;actor.runtime.fleeUntil=30;
    actor.runtime.lastSeen=0;Q230SimulateActor(actor,16);assert(actor.runtime.activity==fo3npc::Activity::Combat);
    auto departed=actor;departed.source.refFormId=60;departed.source.baseFormId=61;departed.runtime.offScene=true;
    auto c=gPlayerSession->player.Definitions();c.references[60].base=61;c.pipboy.targets[60].base=61;c.weapons.actors[61].health=100;
    gPlayerSession=std::make_unique<Session>(c);gQ230NpcActors.push_back(departed);
    std::array<float,3> target{};uint32_t base=0;assert(!Q230Target(60,target,base));
  }
  {
    Prepare();auto& actor=gQ230NpcActors[0];actor.runtime.EndDialogue();actor.runtime.BeginCombat(0x14);
    const auto start=Q240GamePosition(actor);const std::array<float,3> threat{1000,2000,20};
    assert(Q230TacticalRoute(actor,threat,800,INFINITY,true,false,1));
    assert(!actor.aiPathGame.empty()&&Q240PlanarDistance(actor.aiPathGame.back(),threat)>Q240PlanarDistance(start,threat));
    actor.runtime.action=fo3npc::CombatAction::Flee;
    const auto route=actor.aiPathGame;assert(Q230TacticalRoute(actor,threat,800,INFINITY,true,false,1.1)&&actor.aiPathGame==route);
    actor.runtime.nextPath=0;
    assert(!Q230TacticalRoute(actor,threat,0,INFINITY,false,true,2)&&actor.aiPathGame.empty()); // Clear sight is not cover.
    blocked=true;actor.runtime.nextPath=0;
    assert(Q230TacticalRoute(actor,threat,0,INFINITY,false,true,3)&&!actor.aiPathGame.empty());
  }
  {
    Prepare();auto& actor=gQ230NpcActors[0];actor.runtime.position=Q240ScenePosition({1500,2500,20});
    const std::array<float,3> threat{1500,3000,20};
    for(int direction:{-1,1}){
      actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;actor.runtime.nextPath=0;
      assert(Q230TacticalRoute(actor,threat,0,INFINITY,false,false,1,direction));
      const auto endpoint=actor.aiPathGame.back();assert((endpoint[0]-1500)*direction>0);
    }
    blocked=true;actor.aiPathGame.clear();actor.aiPathIndex=0;actor.runtime.nextPath=0;
    assert(!Q230TacticalRoute(actor,threat,0,INFINITY,false,false,2)); // A firing endpoint needs sight.
  }
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
  {
    Prepare();auto c=gPlayerSession->player.Definitions();c.weapons.actors[43].templates=2;c.weapons.actors[43].templateId=999;
    gPlayerSession=std::make_unique<Session>(c);auto& unknown=gQ230NpcActors[0];unknown.runtime.EndDialogue();
    Q230SimulateActor(unknown,1);Q230SimulateActor(unknown,2);
    assert(gPlayerSession->player.ActorHealth(42)<0&&unknown.runtime.Alive());
    assert(!gPlayerSession->player.Snapshot().actors.at(42).dead&&shots==0);
    // Pure persistent movement dirties the save but does not invalidate the
    // entire population's package conditions each rendered frame.
    auto saved=gPlayerSession->player.Snapshot().actors.at(42);
    const auto saveRevision=gPlayerSession->player.Revision(),packageRevision=gPlayerSession->player.PackageRevision();
    saved.position[0]+=1;assert(gPlayerSession->player.UpdateActor(42,saved));
    assert(gPlayerSession->player.Revision()>saveRevision&&gPlayerSession->player.PackageRevision()==packageRevision);
    saved.hostile=0x14;assert(gPlayerSession->player.UpdateActor(42,saved));
    assert(gPlayerSession->player.PackageRevision()>packageRevision);
  }
  // An attack started before losing LOS must not fire later as a stale hit.
  Prepare();auto& interrupted=gQ230NpcActors[0];Q230SimulateActor(interrupted,1.3);
  Q230SimulateActor(interrupted,1.9);Q230SimulateActor(interrupted,2);
  assert(interrupted.runtime.pendingAttack&&shots==0);
  blocked=true;Q230SimulateActor(interrupted,2.01);
  assert(!interrupted.runtime.pendingAttack&&shots==0&&interrupted.runtime.action==fo3npc::CombatAction::Search);
  blocked=false;Q230SimulateActor(interrupted,2.02);
  assert(shots==0&&!interrupted.runtime.pendingAttack);
  Q230SimulateActor(interrupted,2.3);assert(interrupted.runtime.pendingAttack);
  Q230SimulateActor(interrupted,2.31);assert(shots==1);
  // The eye may still see the target while the actual firing muzzle is
  // obstructed at the animation hit event; do not spend ammo or emit a shot.
  Prepare();auto& muzzleInterrupted=gQ230NpcActors[0];
  Q230SimulateActor(muzzleInterrupted,1.3);Q230SimulateActor(muzzleInterrupted,1.9);
  Q230SimulateActor(muzzleInterrupted,2);assert(muzzleInterrupted.runtime.pendingAttack);
  const auto roundsBefore=gPlayerSession->player.ActorWeapon(42)->loadedRounds;
  blockSecondSightRay=true;sightRayCount=0;
  Q230SimulateActor(muzzleInterrupted,2.01);
  assert(shots==0&&!muzzleInterrupted.runtime.pendingAttack&&
         gPlayerSession->player.ActorWeapon(42)->loadedRounds==roundsBefore);
  blockSecondSightRay=false;sightRayCount=0;
  // A nonlethal authored hit owns the pose temporarily; navigation and AI
  // must not instantly overwrite the reaction, and must resume afterwards.
  Prepare();auto& reacted=gQ230NpcActors[0];
  reacted.runtime.EndDialogue();reacted.runtime.hitUntil=2.5;
  reacted.runtime.animation=fo3npc::Animation::Hit;
  Q230SimulateActor(reacted,2);
  assert(reacted.runtime.animation==fo3npc::Animation::Hit&&shots==0);
  Q240UpdateNpcPackage(reacted,2.1);
  assert(reacted.runtime.animation==fo3npc::Animation::Hit&&reacted.runtime.speed==0);
  Q230SimulateActor(reacted,2.6);
  assert(reacted.runtime.animation!=fo3npc::Animation::Hit);
  // Perception may evaluate a different package but cannot clear the active
  // XTEL movement intent. Package execution alone adopts that intent.
  Prepare();auto& policyActor=gQ230NpcActors.front();policyActor.runtime.EndDialogue();
  policyActor.runtime.xtelDoor=0xabc;policyActor.runtime.xtelCell=0xdef;
  uint32_t policyId=0;std::array<float,3> policyAnchor{};
  (void)Q230PerceptionPackage(policyActor,policyId,policyAnchor,1);
  assert(policyActor.runtime.xtelDoor==0xabc&&policyActor.runtime.xtelCell==0xdef);
  PackageCombatPolicyTests();if(argc>1)OriginalCombatPolicies(argv[1]);
  if(argc==4||argc==5)HendersonSecurityCombat(argv[1],argv[2],argv[3],argc==5?argv[4]:nullptr);
  std::cout<<"Production NPC combat pursuit/flee, LOS, reload, firing, death and restore passed\n";
}
