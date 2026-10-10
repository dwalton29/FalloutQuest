#include "npc/fo3-npc-combat.h"
#include "npc/fo3-npc-combat-assets.h"
#include "weapons/fo3-weapon-hit.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <zlib.h>
using namespace fo3player;
static Catalog Fixture(bool finite=true) {
  Catalog c;c.initial.baseHealth=100;c.weapons.skillBase=1;c.weapons.conditionBase=1;
  Item w;w.formId=10;w.kind=ItemKind::Weapon;w.name="Authored weapon";w.maxCondition=100;
  auto& d=w.weapon;d.valid=true;d.animation=3;d.ammo=11;d.clip=3;d.ammoUse=1;d.pellets=1;d.damage=20;d.shotsPerSecond=4;d.flags2=finite?2:0;
  c.items[10]=w;Item ammo;ammo.formId=11;ammo.kind=ItemKind::Ammo;ammo.name="Ammo";c.items[11]=ammo;
  Item armour;armour.formId=12;armour.kind=ItemKind::Armour;armour.bipedMask=4;c.items[12]=armour;c.weapons.armourDR[12]=50;
  for(uint32_t ref:{100,101}){c.references[ref].base=ref+100;c.pipboy.targets[ref].base=ref+100;c.pipboy.targets[ref].cell=1;
    c.weapons.actors[ref+100].health=100;
    auto& inventory=c.actorInventories[ref+100];inventory.entries={{10,0,1,1,1,false},{11,0,4,1,1,false},{12,0,1,1,1,false}};
    auto& actor=c.pipboy.dialogueActors[ref+100];actor.aiData.resize(20);actor.aiData[0]=1;actor.aiData[1]=3;actor.aiData[14]=1;actor.factions[500+ref]=0;
  }
  c.pipboy.packages[77].type=6;c.weapons.relations[600][601]=1;return c;
}
static void StateAndPerception() {
  using namespace fo3npc;RuntimeState s;s.activity=Activity::Package;s.package=77;s.position={1,2,3};s.speed=2;
  s.BeginDialogue();assert(s.activity==Activity::Dialogue&&s.speed==0);s.BeginCombat(101);
  assert(s.activity==Activity::Combat&&!s.CanTalk()&&!s.dialogue&&s.suspendedPackage==77);
  s.EndCombat();assert(s.activity==Activity::Package&&s.package==77&&s.position[0]==1);
  s.BeginCombat(101);s.Die(2);assert(s.activity==Activity::Dying&&!s.Alive()&&!s.CanTalk()&&s.combatTarget==0);
  s.BeginDialogue();assert(!s.dialogue);s.BeginCombat(100);assert(s.activity==Activity::Dying);
  assert(fo3weapon::BoneRegion("Bip01 Head")==fo3weapon::Region::Head&&fo3weapon::BoneRegion("Bip01 L Forearm")==fo3weapon::Region::LeftArm&&fo3weapon::BoneRegion("Bip01 R Calf")==fo3weapon::Region::RightLeg&&fo3weapon::BoneRegion("CreatureLeg")==fo3weapon::Region::Unknown);
  auto c=Fixture();auto ai=AI(c.pipboy,200);assert(ai.valid&&ai.aggression==1&&ai.confidence==3);
  assert(Relationship(c,200,201)==Reaction::Enemy&&Acquires(ai,Reaction::Enemy)&&!Acquires(ai,Reaction::Ally)&&!Acquires(ai,Reaction::Neutral));
  ai.aggression=2;assert(Acquires(ai,Reaction::Neutral)&&!Acquires(ai,Reaction::Friend));
  ai.aggression=0;assert(!Acquires(ai,Reaction::Enemy));assert(Assists(ai,Reaction::Ally)&&!Assists(ai,Reaction::Neutral));
  fo3pipdata::PackageDefinition defensive;assert(!Defensive(&defensive)&&!Defensive(nullptr));defensive.flags=1u<<22;assert(Defensive(&defensive));defensive.flags=1u<<26;assert(!Defensive(&defensive));
  c.pipboy.dialogueActors[200].aiData.resize(19);assert(!AI(c.pipboy,200).valid);
  s={};s.BeginCombat(0x14);
  assert(MayPlayHitReaction(s));
  s.pendingAttack=true;assert(!MayPlayHitReaction(s));
  s.pendingAttack=false;s.reloadUntil=2.5;assert(!MayPlayHitReaction(s));
  s.reloadUntil=0;assert(MayPlayHitReaction(s));
  s.activity=Activity::Package;s.pendingAttack=true;assert(MayPlayHitReaction(s));
  s={};s.BeginCombat(101);auto d=c.items.at(10).weapon;
  assert(FireReady(s,d,1)&&!FireReady(s,d,1.1)&&FireReady(s,d,1.25));s.reloadUntil=5;assert(!FireReady(s,d,8));s.reloadUntil=0;
  assert(FireReady(s,d,9)&&!FireReady(s,d,9));
  c.npcCombatSettings={{"fConfidenceCautious",.375f},{"fConfidenceAverage",.1875f},{"fConfidenceBrave",.0375f}};
  for(uint8_t confidence=0;confidence<=4;++confidence){
    ai.confidence=confidence;
    assert(ShouldFlee(ai,c,100,100,true)==(confidence==0));
    assert(ShouldFlee(ai,c,100,100,false)==(confidence<=2));
    assert(ShouldFlee(ai,c,1,100,true)==(confidence<4));
    assert(!ShouldFlee(ai,c,0,100,false));
  }
  ai.confidence=2;assert(!ShouldFlee(ai,c,20,100,true)&&ShouldFlee(ai,c,18,100,true));
  fo3weapon::Definitions::CombatStyle burst;burst.valid=true;burst.fireMin=burst.fireMax=1;burst.pauseMin=burst.pauseMax=2;burst.delayMin=burst.delayMax=1;
  s={};s.BeginCombat(101);
  assert(FireReady(s,d,1,&burst)&&FireReady(s,d,1.25,&burst));
  assert(!FireReady(s,d,2,&burst)&&s.burstWaitUntil==4);
  assert(!FireReady(s,d,3.99,&burst)&&FireReady(s,d,4,&burst));
  s.EndCombat();s.BeginCombat(101);assert(s.burstUntil==0&&s.burstWaitUntil==0);
}
static void CombatAssetCandidates() {
  auto c=Fixture();
  Fo3NpcActorQ230 npc;npc.refFormId=100;npc.baseFormId=200;
  auto addSource=[&](uint32_t form,const char* type){
    Fo3NpcVisualItemQ230 item;
    item.formId=form;item.count=1;item.recordType=type;
    npc.inventory.push_back(std::move(item));
  };
  addSource(10,"WEAP");
  addSource(301,"LVLI");
  addSource(302,"LVLI");
  fo3player::Item ranged=c.items.at(10);ranged.formId=20;c.items[20]=ranged;
  c.lootLists[301].valid=true;c.lootLists[301].entries.push_back({302,0,1,1,1,false});
  c.lootLists[302].valid=true;c.lootLists[302].entries.push_back({20,0,1,1,1,false});
  c.lootLists[302].entries.push_back({301,0,1,1,1,false}); // Cycle must terminate.
  const auto forms=fo3npc::CombatWeaponCandidates(npc,c);
  assert(forms.size()==2&&forms[0]==10&&forms[1]==20);
  assert(fo3npc::CombatWeaponCandidates(npc,c,1).size()==1);
  assert(fo3npc::HitRegionSlot(0)==1&&fo3npc::HitRegionSlot(1)==0&&
         fo3npc::HitRegionSlot(6)==5);
  const std::array<bool,3> all{true,true,true},partial{false,true,true},none{};
  assert(fo3npc::HitVariant(all,0)==0&&fo3npc::HitVariant(all,1)==1&&
         fo3npc::HitVariant(all,2)==2&&fo3npc::HitVariant(all,3)==0);
  assert(fo3npc::HitVariant(partial,0)==1&&fo3npc::HitVariant(partial,2)==2&&
         fo3npc::HitVariant(none,1)==-1);
}
static void InventoryDamagePersistence(bool finite) {
  auto c=Fixture(finite);Player p(c);assert(p.PrepareActorInventory(100)&&p.PrepareActorInventory(101));
  const auto id=p.ContainerContents(100)->at(0).id;assert(!p.FireActorWeapon(100,id));assert(p.EquipActorWeapon(100,id));
  assert(p.ReloadActorWeapon(100,id)&&p.ActorWeapon(100)->loadedRounds==3);
  assert(p.ContainerContents(100)->at(1).count==(finite?1:4));
  for(int i=0;i<3;++i)assert(p.FireActorWeapon(100,id));assert(!p.FireActorWeapon(100,id));
  assert(p.ReloadActorWeapon(100,id)&&p.ActorWeapon(100)->loadedRounds==(finite?1:3));
  assert(p.DamageResistance(101)==50&&p.ApplyAttack(100,10,101,20)&&p.ActorHealth(101)==90);
  assert(p.Snapshot().actors.at(101).hostile==100);
  const float before=p.Health();assert(p.ApplyAttack(100,10,0x14,20)&&p.Health()==before-20);
  assert(!p.ApplyAttack(100,10,100,20)&&!p.ApplyAttack(100,10,101,NAN));
  c.weapons.actors[201].flags=2;Player essential(c);assert(essential.PrepareActorInventory(101));
  assert(essential.ApplyAttack(0x14,10,101,10000)&&essential.ActorHealth(101)==1&&!essential.CanLootContainer(101));
  ActorState a=p.Snapshot().actors.at(100);a.position={123,456,789};a.yaw=.7f;a.package=77;a.sequence=4;a.packageWaitSeconds=7.5f;
  assert(p.UpdateActor(100,a));a.position[1]=NAN;assert(!p.UpdateActor(100,a));a=p.Snapshot().actors.at(100);a.packageWaitSeconds=-1;assert(!p.UpdateActor(100,a));
  const std::string path="/tmp/fq-npc-combat-"+std::to_string(getpid());std::string error;assert(p.Save(path,error));
  Player restored(c);assert(restored.Restore(path,error));assert(restored.ActorWeapon(100)->id==id&&restored.Snapshot().actors.at(100).position[0]==123&&restored.ActorHealth(101)==90&&restored.Snapshot().actors.at(100).packageWaitSeconds==7.5f);
  { // Migrate the initial v7 actor extension without lifecycle flags.
    std::ifstream f(path,std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});f.close();
    const size_t start=bytes.size()-112-20;std::vector<uint8_t> legacy(bytes.begin(),bytes.begin()+start);
    for(size_t i=0;i<2;++i)legacy.insert(legacy.end(),bytes.begin()+start+i*56,bytes.begin()+start+i*56+48);
    auto put=[&](size_t at,uint32_t n){for(int i=0;i<4;++i)legacy[at+i]=(n>>(8*i))&255;};put(4,7);put(12,legacy.size()-20);put(16,crc32(0,legacy.data()+20,legacy.size()-20));
    std::ofstream out(path,std::ios::binary);out.write((const char*)legacy.data(),legacy.size());out.close();
    Player v7(c);assert(v7.Restore(path,error)&&v7.ActorWeapon(100)->id==id&&v7.Snapshot().actors.at(100).position[0]==123);
  }
  { // Revision 8 retained lifecycle but had no package-wait duration.
    assert(p.Save(path,error));std::ifstream f(path,std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});f.close();
    const size_t start=bytes.size()-112-20;std::vector<uint8_t> legacy(bytes.begin(),bytes.begin()+start);
    for(size_t i=0;i<2;++i)legacy.insert(legacy.end(),bytes.begin()+start+i*56,bytes.begin()+start+i*56+52);
    auto put=[&](size_t at,uint32_t n){for(int i=0;i<4;++i)legacy[at+i]=(n>>(8*i))&255;};put(4,8);put(12,legacy.size()-20);put(16,crc32(0,legacy.data()+20,legacy.size()-20));
    std::ofstream out(path,std::ios::binary);out.write((const char*)legacy.data(),legacy.size());out.close();Player v8(c);assert(v8.Restore(path,error)&&v8.Snapshot().actors.at(100).packageWaitSeconds==0);
  }
  { // Valid CRC does not authorize a NaN duration. Restore stays transactional.
    assert(p.Save(path,error));std::ifstream f(path,std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});f.close();
    auto put=[&](size_t at,uint32_t n){for(int i=0;i<4;++i)bytes[at+i]=(n>>(8*i))&255;};put(bytes.size()-24,0x7fc00000);put(16,crc32(0,bytes.data()+20,bytes.size()-20));
    std::ofstream out(path,std::ios::binary);out.write((const char*)bytes.data(),bytes.size());out.close();assert(!restored.Restore(path,error)&&restored.Snapshot().actors.at(100).packageWaitSeconds==7.5f);
  }
  assert(p.ApplyAttack(0x14,10,100,10000)&&p.ActorHealth(100)==0&&p.PrepareContainer(100));
  if(!finite)assert(p.ContainerContents(100)->at(0).loadedRounds==0);
  assert(p.TakeContainerStack(100,id)&&p.Weapon(id)&&!p.Weapon(id)->equipped);
  assert(p.Save(path,error));Player corpse(c);assert(corpse.Restore(path,error));assert(corpse.ActorHealth(100)==0&&corpse.Weapon(id)&&!corpse.ActorWeapon(100)&&corpse.Snapshot().actors.at(100).dead);
  auto higherHealth=c;higherHealth.weapons.actors[200].health=200;Player levelled(higherHealth);assert(levelled.Restore(path,error)&&levelled.ActorHealth(100)==0);
  {
    std::ifstream f(path,std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});f.close();
    auto put=[&](size_t at,uint32_t n){for(int i=0;i<4;++i)bytes[at+i]=(n>>(8*i))&255;};
    put(bytes.size()-24,0x7fc00000);put(16,crc32(0,bytes.data()+20,bytes.size()-20));
    std::ofstream out(path,std::ios::binary);out.write((const char*)bytes.data(),bytes.size());out.close();
    assert(!corpse.Restore(path,error)&&corpse.ActorHealth(100)==0&&corpse.Weapon(id));
  }
  // v6 has no actor extension: preserve older damage/inventory, rebuild routes.
  Player empty(c);assert(empty.Save(path,error));std::ifstream f(path,std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});f.close();bytes.resize(bytes.size()-24);
  auto put=[&](size_t at,uint32_t n){for(int i=0;i<4;++i)bytes[at+i]=(n>>(8*i))&255;};put(4,6);put(12,bytes.size()-20);put(16,crc32(0,bytes.data()+20,bytes.size()-20));
  std::ofstream out(path,std::ios::binary);out.write((const char*)bytes.data(),bytes.size());out.close();assert(corpse.Restore(path,error)&&corpse.Snapshot().actors.empty());
  unlink(path.c_str());
}
static void StyleDecodeValidation(){
  std::vector<uint8_t> standard(82),advanced(84),simple(64),payload;
  const auto put=[](std::vector<uint8_t>& bytes,size_t at,float value){std::memcpy(bytes.data()+at,&value,4);};
  standard[37]=100;standard[80]=1;put(standard,72,.5f);put(standard,76,1.5f);
  for(size_t i=0;i<21;++i)put(advanced,i*4,1);put(advanced,0,-20); // Signed fatigue coefficient is legal.
  const auto decode=[&](){payload.clear();for(const auto& entry:std::array<std::pair<const char*,const std::vector<uint8_t>*>,3>{{{"CSTD",&standard},{"CSAD",&advanced},{"CSSD",&simple}}}){
      payload.insert(payload.end(),entry.first,entry.first+4);const size_t size=entry.second->size();payload.push_back(uint8_t(size));payload.push_back(uint8_t(size>>8));payload.insert(payload.end(),entry.second->begin(),entry.second->end());}
    fo3weapon::Definitions definitions;fo3weapon::Decode(definitions,"CSTY",200,payload);return definitions.styles.at(200);};
  auto style=decode();assert(style.meleeValid&&style.advancedValid&&style.meleeHoldMin==.5f);
  put(standard,72,-1);style=decode();assert(!style.meleeValid&&style.advancedValid);
  put(advanced,16,NAN);assert(!decode().advancedValid);
}
static void SourceContext(){
  fo3weapon::Definitions::CombatStyle style;style.dodgeChance=80;
  style.meleeAttackChance=40;style.flags=1;style.recoilAttackBonus=5;style.unarmedAttackBonus=5;
  style.advancedValid=true;style.advanced[4]=1;style.advanced[5]=.75f;
  style.advanced[6]=1;style.advanced[7]=.7f;style.advanced[8]=1;style.advanced[9]=.5f;
  style.advanced[16]=.75f;style.advanced[17]=1;
  assert(fo3npc::DodgeChance(style,true)==80&&fo3npc::DodgeChance(style,false)==60);
  assert(fo3npc::ForwardDodgeChance(style,true,true)==50);
  assert(fo3npc::ForwardDodgeChance(style,false,false)<50);
  assert(fo3npc::MeleeAttackChance(style,false,true,true)==50);
  assert(fo3npc::MeleeAttackChance(style,true,false,false)==30);
  style.flags=0;assert(fo3npc::MeleeAttackChance(style,true,false,false)==100);
  for(unsigned i=0;i<1000;++i){assert(fo3npc::CombatChance(42,i,100));assert(!fo3npc::CombatChance(42,i,0));}
}
static void Original(const char* path) {
  Catalog c;std::string error;assert(LoadCatalog(path,c,error));assert(c.weapons.styles.size()==48&&c.weapons.detectionDistance==2500&&c.weapons.drMax==85);
  // Original Fallout3.esm has fMoveRunMult=4.0; only active combat Flee uses it.
  assert(std::fabs(c.npcRunMultiplier-4.f)<.001f);
  assert(c.npcBaseSpeed==77&&c.weapons.defaultCombatStyle==0x3d);
  assert(c.npcCombatSettings.at("fCombatFleeNormalDistance")==1400);
  assert(std::fabs(c.npcCombatSettings.at("fConfidenceAverage")-.1875f)<.0001f);
  const auto& style=c.weapons.styles.at(c.weapons.defaultCombatStyle);
  assert(style.manoeuvresValid&&style.dodgeChance==75&&style.leftRightChance==50);
  assert(style.meleeValid&&style.meleeAttackChance==40&&style.meleeHoldMin==.5f&&style.meleeHoldMax==1.5f);
  assert(style.advancedValid&&style.advanced[4]==1&&style.advanced[5]==.75f&&style.advanced[16]==.75f);
  assert((style.manoeuvreTimers==std::array<float,8>{.5f,1.5f,.5f,1.f,.25f,.75f,.5f,1.5f}));
  assert(style.coverRadius==2048&&style.coverChance==100&&style.pauseMin==2&&style.pauseMax==2);
  // Canonical MegatonSettlerWeapon and WithAmmoAssaultRifleNPC LVLI records.
  // These were previously invisible to the combat asset-preparation loop.
  for(uint32_t list:{0x0006C36Bu,0x00029367u}){
    assert(c.lootLists.count(list));
    Fo3NpcActorQ230 source;
    Fo3NpcVisualItemQ230 entry;
    entry.formId=list;entry.count=1;entry.recordType="LVLI";
    source.inventory.push_back(std::move(entry));
    const auto candidates=fo3npc::CombatWeaponCandidates(source,c);
    assert(!candidates.empty());
    for(uint32_t weapon:candidates)assert(c.items.at(weapon).kind==ItemKind::Weapon);
  }
  size_t count=0;for(const auto& r:c.references){if(count==5)break;if(!c.actorInventories.count(r.second.base))continue;
    Player p(c);if(!p.PrepareActorInventory(r.first))continue;
    const auto* contents=p.ContainerContents(r.first);uint64_t id=0;uint32_t form=0;
    for(const auto& s:*contents){const auto& d=c.items.at(s.formId).weapon;if(d.Firearm()&&d.animation<8){id=s.id;form=s.formId;break;}}
    if(!id||!p.EquipActorWeapon(r.first,id)||!p.ReloadActorWeapon(r.first,id))continue;
    assert(p.ActorWeapon(r.first)->loadedRounds<=c.items.at(form).weapon.clip&&p.FireActorWeapon(r.first,id));
    std::cout<<"Original NPC "<<std::hex<<r.first<<" base="<<r.second.base<<" weapon="<<form<<std::dec<<std::endl;
    assert(fo3npc::AI(c.pipboy,r.second.base).valid);++count;
  }assert(count==5);
}
int main(int argc,char** argv){StyleDecodeValidation();SourceContext();StateAndPerception();CombatAssetCandidates();InventoryDamagePersistence(true);InventoryDamagePersistence(false);if(argc>1)Original(argv[1]);std::cout<<"NPC combat state tests passed\n";}
