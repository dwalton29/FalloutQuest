#include "player/fo3-player-state.h"
#include "dialogue/fo3-dialogue-session.h"
#include "weapons/fo3-weapon-interaction.h"
#include "weapons/fo3-weapon-hit.h"
#include "weapons/fo3-weapon-holsters.h"
#include "weapons/fo3-reload-policy.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <fstream>
#include <iterator>
#include <unistd.h>
#include <zlib.h>
using namespace fo3player;
using Bytes = std::vector<uint8_t>;
Bytes Read(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  return Bytes(std::istreambuf_iterator<char>(f), {});
}
void Write(const std::string &path, const Bytes &bytes) {
  std::ofstream f(path, std::ios::binary);
  f.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  assert(f.good());
}
uint32_t U32(const Bytes &b, size_t at) {
  return uint32_t(b.at(at)) | uint32_t(b.at(at+1))<<8 |
      uint32_t(b.at(at+2))<<16 | uint32_t(b.at(at+3))<<24;
}
void Put(Bytes &b, size_t at, uint32_t n) {
  for (unsigned i=0;i<4;++i) b.at(at+i)=(n>>(8*i))&255;
}
void Seal(Bytes &b) {
  Put(b,12,static_cast<uint32_t>(b.size()-20));
  Put(b,16,crc32(0,b.data()+20,b.size()-20));
}
Catalog Fixture() {
  Catalog c;c.initial.baseHealth=100;c.initial.special.fill(1);
  Item item;item.formId=0x434f;item.kind=ItemKind::Weapon;
  item.editorId="Weap10mmPistol";item.name="10mm Pistol";item.maxCondition=150;
  auto &d=item.weapon;d.valid=true;d.animation=3;d.ammo=0x4241;d.projectile=0x2cd5f;
  d.clip=12;d.ammoUse=1;d.pellets=1;d.damage=9;d.shotsPerSecond=6;
  c.items.emplace(item.formId,item);
  item={};item.formId=0x4241;item.kind=ItemKind::Ammo;item.name="10mm Round";
  c.items.emplace(item.formId,item);c.weapons.damageGun=.03f;
  c.references[10].base=0x434f;
  c.references[20].base=30;
  c.containers[30].entries.push_back({0x434f,0,2,1,.5f,false});
  return c;
}
void ReloadTests() {
  Player p(Fixture());assert(p.Add(0x434f,1));const auto id=p.Snapshot().inventory.front().id;
  assert(p.Equip(id)&&p.Add(0x4241,18));
  assert(p.EjectMagazine(id));const auto ejected=p.Revision();
  assert(!p.EjectMagazine(id)&&p.Revision()==ejected&&p.AmmoReserve(0x4241)==18);
  std::string error;const auto path="/tmp/fq-reload-"+std::to_string(getpid());
  assert(p.Save(path,error));Player restored(Fixture());assert(restored.Restore(path,error));unlink(path.c_str());
  assert(!restored.EjectMagazine(id)&&restored.CanReloadWeapon(id)&&restored.ReloadWeapon(id));
  assert(restored.Weapon(id)->loadedRounds==12&&!restored.Weapon(id)->needsAction&&restored.AmmoReserve(0x4241)==6);
  const auto full=restored.Revision();assert(!restored.ReloadWeapon(id)&&restored.Revision()==full);
  for(int i=0;i<12;++i)assert(restored.FireWeapon(id));
  assert(!restored.Weapon(id)->needsAction&&restored.EjectMagazine(id));const auto empty=restored.Revision();
  assert(!restored.EjectMagazine(id)&&restored.Revision()==empty);
  assert(restored.ReloadWeapon(id)&&restored.Weapon(id)->loadedRounds==6&&restored.AmmoReserve(0x4241)==0);
  for(int i=0;i<6;++i)assert(restored.FireWeapon(id));
  const auto dry=restored.Revision();assert(!restored.ReloadWeapon(id)&&restored.Revision()==dry);
  assert(fo3weapon::InsertionZone({.4f,0,0},{0,0,0},{.2f,.1f,.1f}));
  assert(fo3weapon::InsertionZone({0,.4f,0},{0,0,0},{.2f,.1f,.1f}));
  assert(!fo3weapon::InsertionZone({0,.8f,0},{0,0,0},{.2f,.1f,.1f}));
  assert(!fo3weapon::InsertionZone({NAN,0,0},{0,0,0},{.2f,.1f,.1f}));
}
void Ownership(Catalog c) {
  // Isolate the test inventory, preserving original item/projectile/rule definitions.
  c.initial.inventory.clear();c.initial.nextStackId=1;
  c.initial.worldWeapons.clear();c.initial.containers.clear();c.initial.collected.clear();
  c.initial.developmentWeaponGranted=false;
  Player p(c);std::string error;
  assert(p.Add(0x434f,2,.5f));
  assert(p.Snapshot().inventory.size()==2);
  const auto id=p.Snapshot().inventory[0].id;
  const auto other=p.Snapshot().inventory[1].id;
  assert(id!=other&&p.Equip(id));
  assert(p.Add(0x4241,13)&&p.Add(0x4241,4));
  assert(p.Snapshot().inventory.size()==3&&p.AmmoReserve(0x4241)==17);
  assert(!p.FireWeapon(id)&&!p.LoadMagazine(id));
  assert(p.EjectMagazine(id)&&p.LoadMagazine(id));
  assert(p.Weapon(id)->loadedRounds==12&&p.AmmoReserve(0x4241)==5);
  assert(p.Weapon(id)->needsAction&&!p.FireWeapon(id));
  assert(p.ChamberWeapon(id)&&p.FireWeapon(id));
  assert(p.Weapon(id)->loadedRounds==11&&p.Weapon(id)->condition<.5f);
  const float condition=p.Weapon(id)->condition;
  assert(p.EjectMagazine(id)&&p.AmmoReserve(0x4241)==16);
  assert(p.LoadMagazine(id)&&p.AmmoReserve(0x4241)==4);
  fo3weapon::WorldPose pose;pose.cell=123;pose.position={100,200,300};
  pose.velocity={1,2,3};pose.angularVelocity={.1f,.2f,.3f};
  const auto rev=p.Revision();
  auto invalid=pose;invalid.rotation[0]=NAN;
  assert(!p.DropWeapon(id,invalid)&&p.Revision()==rev);
  assert(p.DropWeapon(id,pose)&&!p.Weapon(id)&&!p.EquippedWeapon());
  assert(p.Snapshot().worldWeapons[0].instance.id==id);
  assert(p.Snapshot().worldWeapons[0].instance.loadedRounds==12);
  assert(p.Snapshot().worldWeapons[0].instance.needsAction);
  const std::string path="/tmp/fq-weapon-state-"+std::to_string(getpid())+".fqps";
  assert(p.Save(path,error));
  Player loaded(c);assert(loaded.Restore(path,error));
  assert(loaded.Snapshot().worldWeapons.size()==1);
  assert(loaded.Snapshot().worldWeapons[0].pose.position==pose.position);
  assert(loaded.Snapshot().worldWeapons[0].pose.velocity==pose.velocity);
  assert(loaded.PickupWorldWeapon(id)&&!loaded.PickupWorldWeapon(id));
  assert(loaded.Weapon(id)->condition==condition&&loaded.Weapon(id)->loadedRounds==12);
  assert(loaded.Weapon(id)->needsAction&&!loaded.FireWeapon(id));
  assert(loaded.ChamberWeapon(id)&&loaded.FireWeapon(id));
  assert(loaded.Equip(other)&&!loaded.Weapon(id)->equipped&&loaded.Weapon(other)->equipped);
  assert(loaded.Equip(id)&&loaded.Save(path,error));
  const auto good=Read(path);
  assert(U32(good,4)==14);
  Player overflow=loaded;
  assert(overflow.Add(0x4241,INT32_MAX-overflow.AmmoReserve(0x4241)));
  const auto overflowRevision=overflow.Revision();
  assert(!overflow.EjectMagazine(id)&&overflow.Revision()==overflowRevision);
  assert(overflow.Weapon(id)->loadedRounds==11&&overflow.AmmoReserve(0x4241)==INT32_MAX);
  auto bad=good;
  // Collected/container tables are empty in this isolated fixture.
  const size_t pipLength=40+21*U32(good,36)+16;
  const size_t extension=pipLength+4+U32(good,pipLength);
  Put(bad,extension+12+8,0xffffu);Seal(bad);Write(path,bad);
  const auto unchanged=loaded.Revision();
  assert(!loaded.Restore(path,error)&&loaded.Revision()==unchanged);
  assert(loaded.Weapon(id)->loadedRounds==11);
  // Real v4 layout: same legacy prefix, followed directly by the Pip-Boy blob.
  Bytes v4(good.begin(),good.begin()+pipLength);
  v4.insert(v4.end(),good.begin()+pipLength+4,good.begin()+extension);
  Put(v4,4,4);Seal(v4);Write(path,v4);
  Player migrated(c);assert(migrated.Restore(path,error));
  assert(migrated.Weapon(id)&&!migrated.Weapon(id)->loadedRounds);
  assert(migrated.AmmoReserve(0x4241)==4);
  // Old, unequipped weapons could be count stacks. Split them on migration,
  // preserving the original instance and assigning collision-free new IDs.
  const size_t legacyWeapon=40; // The dropped/picked-up weapon moved to the end.
  assert(U32(v4,legacyWeapon+8)==0x434f&&!v4.at(legacyWeapon+20));
  Put(v4,legacyWeapon+12,3);Seal(v4);Write(path,v4);
  assert(migrated.Restore(path,error));
  assert(migrated.Snapshot().inventory.size()==loaded.Snapshot().inventory.size()+2);
  assert(migrated.Weapon(other)->count==1);
  assert(migrated.Snapshot().nextStackId==loaded.Snapshot().nextStackId+2);
  Player seeded(c);assert(seeded.BootstrapDevelopmentWeapon());
  const auto seedId=seeded.EquippedWeapon()->id;
  assert(seeded.Weapon(seedId)->loadedRounds==12&&seeded.AmmoReserve(0x4241)==48);
  assert(seeded.DropWeapon(seedId,pose)&&seeded.Save(path,error));
  Player resumed(c);assert(resumed.Restore(path,error));
  const auto seedRevision=resumed.Revision();
  assert(resumed.BootstrapDevelopmentWeapon()&&resumed.Revision()==seedRevision);
  assert(!resumed.EquippedWeapon()&&resumed.Snapshot().worldWeapons[0].instance.id==seedId);
  // Malformed world pose and duplicate ownership are rejected atomically.
  const auto seededSave=Read(path);
  bad=seededSave;Put(bad,bad.size()-48,0x7fc00000);Seal(bad);Write(path,bad);
  assert(!resumed.Restore(path,error)&&resumed.Revision()==seedRevision);
  bad=seededSave;
  const auto ownedId=resumed.Snapshot().inventory.front().id;
  Put(bad,bad.size()-124,static_cast<uint32_t>(ownedId));
  Put(bad,bad.size()-120,static_cast<uint32_t>(ownedId>>32));
  Seal(bad);Write(path,bad);
  assert(!resumed.Restore(path,error)&&resumed.Revision()==seedRevision);
  std::remove(path.c_str());
}
void AmmoIsolation() {
  auto c=Fixture();Item wrong=c.items.at(0x4241);wrong.formId=0x999;c.items.emplace(wrong.formId,wrong);
  Player p(c);assert(p.Add(0x434f,1));const auto id=p.Snapshot().inventory.front().id;assert(p.Equip(id));
  assert(p.Add(0x999,100)&&p.EjectMagazine(id));assert(!p.LoadMagazine(id));
  assert(p.AmmoReserve(0x999)==100&&p.Weapon(id)->loadedRounds==0);
  assert(p.Add(0x4241,7)&&p.LoadMagazine(id));assert(p.Weapon(id)->loadedRounds==7&&p.AmmoReserve(0x4241)==0);
  assert(!p.LoadMagazine(id)&&!p.FireWeapon(id));assert(p.ChamberWeapon(id));
  for(int shot=0;shot<7;shot++)assert(p.FireWeapon(id));
  const auto revision=p.Revision();assert(!p.FireWeapon(id)&&p.Revision()==revision);
  assert(p.Weapon(id)->loadedRounds==0&&p.AmmoReserve(0x999)==100);
}
void Combat() {
  auto c=Fixture();c.weapons.skillBase=.5f;c.weapons.skillMult=.5f;
  c.weapons.conditionBase=.66f;c.weapons.conditionMult=.34f;
  c.weapons.actors[100].health=100;c.pipboy.targets[200].base=100;
  c.weapons.actors[101].health=100;c.weapons.actors[101].flags=2;c.pipboy.targets[201].base=101;
  c.pipboy.formNames["testactor"]=200;
  Player p(c);assert(p.BootstrapDevelopmentWeapon());const auto id=p.EquippedWeapon()->id;
  assert(p.ActorHealth(200)==100&&p.ActorHealth(201)==100);
  const float amount=p.WeaponDamage(id);assert(amount>=4.5f&&amount<=9);
  assert(!p.ApplyWeaponHit(0,200,amount)&&!p.ApplyWeaponHit(0x434f,0,amount));
  assert(!p.ApplyWeaponHit(0x434f,200,NAN)&&!p.ApplyWeaponHit(0x434f,200,-1));
  assert(p.WeaponHit(id,200));assert(std::fabs(p.ActorHealth(200)-(100-amount))<1e-5f);
  fo3pipdata::ResultScript heal;heal.source="testactor.ResetHealth";std::string healError;
  const auto hurt=p.ActorHealth(200);
  assert(p.PreviewDialogueResult(heal,healError)&&p.ActorHealth(200)==hurt);
  assert(p.ExecuteDialogueResult(heal,healError)&&p.ActorHealth(200)==100);
  assert(!p.ResetActorHealth(0xffffffff));
  assert(p.WeaponHit(id,200));
  fo3weapon::WorldPose pose;pose.cell=123;assert(p.DropWeapon(id,pose));
  // A travelling shot retains the damage captured when fired after ownership changes.
  assert(p.ApplyWeaponHit(0x434f,200,amount));assert(!p.WeaponHit(id,200));
  assert(p.ApplyWeaponHit(0x434f,201,1000)&&p.ActorHealth(201)==1);
  assert(p.ApplyWeaponHit(0x434f,200,1000)&&p.ActorHealth(200)==0);
  assert(!p.ApplyWeaponHit(0x434f,200,1));
  assert(p.ExecuteDialogueResult(heal,healError)&&p.ActorHealth(200)==0);
  std::string error;const auto path="/tmp/fq-weapon-combat-"+std::to_string(getpid())+".fqps";
  assert(p.Save(path,error));Player restored(c);assert(restored.Restore(path,error));
  assert(restored.ActorHealth(200)==0&&restored.ActorHealth(201)==1);
  auto bytes=Read(path);Put(bytes,bytes.size()-4,0x7fc00000);Seal(bytes);Write(path,bytes);
  const auto revision=restored.Revision();assert(!restored.Restore(path,error)&&restored.Revision()==revision);
  unlink(path.c_str());
  float distance=10;
  assert(fo3weapon::Surface({0,0,0},{0,0,1},{-1,-1,2},{1,-1,2},{0,1,2},distance)&&distance==2);
  assert(!fo3weapon::Surface({2,0,0},{0,0,1},{-1,-1,2},{1,-1,2},{0,1,2},distance));
  assert(!fo3weapon::Surface({0,0,0},{0,0,1},{0,0,2},{0,0,2},{0,0,2},distance));
}
void BodyAccess(){
  const auto body=fo3vr::Axis({0,1,0},1.2f);
  const auto hip=fo3weapon::Hip({2,1,3},body);
  for(const auto local:{fo3vr::V{.27f,0,0},fo3vr::V{0,.27f,0},fo3vr::V{0,-.27f,0},fo3vr::V{0,0,.27f}})
    assert(hip.Contains(hip.center+fo3vr::Rotate(body,local)));
  assert(!hip.Contains(hip.center+fo3vr::Rotate(body,{.29f,0,0})));
  assert(hip.Contains(hip.center+fo3vr::Rotate(body,{.29f,0,0}),true));
  const auto back=fo3weapon::Back({2,1.4f,3},body);
  assert(back.Contains(back.center+fo3vr::Rotate(body,{0,.36f,0})));
  assert(back.Contains(back.center+fo3vr::Rotate(body,{.31f,.10f,0})));
  assert(!back.Contains(back.center+fo3vr::Rotate(body,{0,0,-.26f}))); // In front of the shoulder.
  auto overlapHip=fo3weapon::Hip({0,1,0},fo3vr::Identity());
  auto overlapBack=fo3weapon::Back({0,1.4f,0},fo3vr::Identity());
  const fo3vr::V overlapping{.10f,1.24f,.08f};
  assert(overlapHip.Contains(overlapping)&&overlapBack.Contains(overlapping));
  assert(fo3weapon::BodySlot(overlapping,overlapHip,overlapBack,true,true)==fo3weapon::HolsterSlot::Back);
  assert(fo3weapon::BodySlot(overlapping,overlapHip,overlapBack,true,false)==fo3weapon::HolsterSlot::Hip);
  fo3weapon::StowAccess access;
  assert(access.Update(back,true,back.center,1));
  assert(access.Update(back,false,{},1.15)); // Brief occlusion can finish an already-entered stow.
  assert(!access.Update(back,false,{},1.21));
  assert(!access.Update(back,false,back.center,2)); // Untracked coordinates cannot enter a holster.
  assert(access.Update(back,true,back.center,3));
  assert(!access.Update(back,true,back.center+fo3vr::Rotate(body,{0,0,-1}),3.01));
  Catalog c=Fixture();auto rifle=c.items.at(0x434f);rifle.formId=99;rifle.weapon.animation=6;c.items[99]=rifle;
  Player player(c);assert(player.Add(0x434f,1,.7f)&&player.Add(99,1,.4f));
  const auto pistol=player.Snapshot().inventory[0].id,gun=player.Snapshot().inventory[1].id;
  auto ready=[](uint32_t){return true;};
  assert(player.Equip(pistol));
  assert(fo3weapon::Holstered(player,true,pistol,0,ready)->id==gun); // Rifle accessible with pistol active.
  assert(player.Equip(gun));
  assert(fo3weapon::Holstered(player,false,gun,pistol,ready)->id==pistol);
  assert(player.Weapon(pistol)->condition==.7f&&player.Weapon(gun)->condition==.4f);
  assert(!fo3weapon::Holstered(player,true,pistol,0,[](uint32_t){return false;}));
}
int main(int argc,char**argv) {
  ReloadTests();
  BodyAccess();
  // Gameplay bullets do not pause merely because VR weapon input is blocked.
  const auto normal=fo3weapon::FrameAccessFor(true,false,false,false,true);
  assert(normal.advanceProjectiles&&normal.weaponControls);
  for(const auto suppressed:{
    fo3weapon::FrameAccessFor(true,false,false,true,true), // Pip-Boy
    fo3weapon::FrameAccessFor(true,false,false,false,false) // Shoulder interaction disabled
  })assert(suppressed.advanceProjectiles&&!suppressed.weaponControls);
  for(const auto paused:{
    fo3weapon::FrameAccessFor(false,false,false,false,true), // No active scene
    fo3weapon::FrameAccessFor(true,true,false,false,true),  // Loading
    fo3weapon::FrameAccessFor(true,false,true,false,true)  // Save blocked
  })assert(!paused.advanceProjectiles&&!paused.weaponControls);
  // Left-wrist Pip-Boy cannot steal a detached spare or slide cycle.
  assert(!fo3weapon::ReloadOwnsLeftHand(false,false,true,false));
  assert(fo3weapon::ReloadOwnsLeftHand(true,false,false,false));
  assert(fo3weapon::ReloadOwnsLeftHand(false,true,false,false));
  assert(fo3weapon::ReloadOwnsLeftHand(false,false,true,true));
  assert(!fo3weapon::ReloadOwnsLeftHand(false,false,false,true));
  Combat();
  AmmoIsolation();
  Ownership(Fixture());
  {
    Player p(Fixture());
    assert(p.PickupWeapon(10)&&!p.PickupWeapon(10)&&p.EquippedWeapon());
    assert(p.PrepareContainer(20)&&p.ContainerContents(20)->size()==2);
    const auto id=p.ContainerContents(20)->front().id;
    assert(p.TakeContainerStack(20,id)&&p.Weapon(id)&&p.Weapon(id)->condition==.5f);
    const auto rev=p.Revision();assert(!p.Add(0x434f,10000)&&p.Revision()==rev);
  }
  {
    Player p(Fixture());assert(p.Add(0x434f,1,.25f));
    const auto id=p.Snapshot().inventory.front().id;
    assert(p.Add(0x4241,3)&&p.BootstrapDevelopmentWeapon());
    assert(p.Weapon(id)->condition==.25f&&p.Weapon(id)->loadedRounds==3);
    assert(p.Snapshot().inventory.size()==1&&!p.AmmoReserve(0x4241));
    assert(p.FireWeapon(id)&&p.FireWeapon(id)&&p.FireWeapon(id)&&!p.FireWeapon(id));
    assert(p.EjectMagazine(id)&&!p.LoadMagazine(id));
  }
  {
    auto c=Fixture();c.items.at(0x434f).cannotDrop=true;
    Player p(c);assert(p.PickupWeapon(10));
    fo3weapon::WorldPose pose;pose.cell=123;
    const auto rev=p.Revision();assert(!p.DropWeapon(p.EquippedWeapon()->id,pose));
    assert(p.Revision()==rev&&p.Snapshot().worldWeapons.empty());
  }
  fo3weapon::Definition malformed;
  assert(!fo3weapon::DecodeWeapon({},malformed));
  assert(!malformed.Firearm());
  fo3weapon::WorldPose pose;
  assert(!fo3weapon::ValidPose(pose));
  fo3weapon::Inputs input;
  fo3weapon::Definition semi;semi.shotsPerSecond=6;
  fo3weapon::Trigger trigger;
  input.Update(false,1,1,1,true);
  assert(!input.r&&!input.l&&!trigger.Ready(semi,input,1,0));
  input.Update(false,0,0,0,false);
  input.Update(false,1,1,1,true);
  assert(input.r&&input.l&&input.eject&&trigger.Ready(semi,input,1,1));
  input.Update(false,1,1,1,true);
  assert(!input.r&&!input.l&&!input.eject&&!trigger.Ready(semi,input,1,2));
  fo3weapon::Definition automatic;automatic.flags=2;automatic.rate=8;
  input.Update(true,1,1,1,true);
  input.Update(false,1,1,1,true);
  assert(!trigger.Ready(automatic,input,1,3)); // focus exit requires neutral
  input.Update(false,0,0,0,false);
  input.Update(false,1,1,1,false);
  assert(trigger.Ready(automatic,input,1,4));
  assert(!trigger.Ready(automatic,input,1,4.1));
  assert(trigger.Ready(automatic,input,1,4.126));
  fo3weapon::Zone zone;zone.center={2,1,3};
  assert(zone.Contains({2,1,3}));
  assert(!zone.Contains({2.18f,1,3})&&zone.Contains({2.18f,1,3},true));
  zone.body=fo3vr::Axis({0,1,0},fo3vr::Pi*.5f);
  assert(zone.Contains(zone.center+fo3vr::Rotate(zone.body,{.15f,0,0})));
  const auto one=fo3vr::Identity();
  assert(fo3weapon::TwoHand(one,{0,0,0},{.01f,0,0},{1,0,0})==one);
  assert(fo3weapon::TwoHand(one,{0,0,0},{-1,0,0},{1,0,0})==one);
  auto two=fo3weapon::TwoHand(one,{0,0,0},{.5f,.5f,0},{1,0,0});
  auto axis=fo3vr::Rotate(two,{1,0,0});
  assert(fo3vr::Dot(axis,fo3vr::Unit({1,1,0}))>.9999f);
  {
    fo3weapon::WorldPose location;location.position={4095,0,0};assert(fo3weapon::CollisionResident(location,0,0));
    location.position[0]=8192;assert(!fo3weapon::CollisionResident(location,0,0));
    location.position[0]=-4096;assert(fo3weapon::CollisionResident(location,0,0));
    location.position[0]=-4096.5f;assert(!fo3weapon::CollisionResident(location,0,0));
    location.position[0]=NAN;assert(!fo3weapon::CollisionResident(location,0,0));
    const auto rotation=fo3vr::Axis({0,1,0},fo3vr::Pi*.5f);const fo3vr::V origin{5,2,3},local{.1f,.2f,.3f};
    const auto hand=origin+fo3vr::Rotate(rotation,local);
    assert(fo3weapon::RearwardPull(rotation,origin,hand,local)<1e-5f);
    assert(fo3weapon::RearwardPull(rotation,origin+fo3vr::V{1,2,3},hand+fo3vr::V{1,2,3},local)<1e-5f);
    assert(std::fabs(fo3weapon::RearwardPull(rotation,origin,hand+fo3vr::Rotate(rotation,{-.05f,0,0}),local)-.045f)<1e-5f);
    assert(fo3weapon::BoxDistance(rotation,origin,hand,local,{.1f,.1f,.1f})<1e-5f);
    assert(std::fabs(fo3weapon::BoxDistance(rotation,origin,hand+fo3vr::Rotate(rotation,{.2f,0,0}),local,{.1f,.1f,.1f})-.1f)<1e-5f);
  }
  pose.cell=123;
  assert(fo3weapon::ValidPose(pose));
  pose.velocity[0]=NAN;
  assert(!fo3weapon::ValidPose(pose));
  if(argc>1) {
    Catalog c;std::string error;
    assert(LoadCatalog(argv[1],c,error));
    Ownership(c);
    // Original-data checkpoint: valid MQ01 entry snapshot, original paid topic.
    {auto start=c;auto& q=start.initial.pipboy.quests[0x14e87];q.stage=10;q.running=true;q.stages.insert(10);
     Player mq(start);mq.RegisterOriginalActors();assert(mq.Add(0xf,100));
     const auto placed=c.pipboy.targets.at(0x3b3c);
     mq.SetScriptPlayerLocation(placed.x,placed.y,placed.z,placed.cell,placed.world);
     fo3dialogue::Context ctx;ctx.player=&mq;
     ctx.speaker={0x3b3c,0xa61,placed.cell,placed.world,0,placed.x,placed.y,placed.z,false,false};
     ctx.target={PlayerRef,7,placed.cell,placed.world,0,placed.x,placed.y+40,placed.z,false,false};
     fo3dialogue::Session conversation;
     auto finish=[&](){while(conversation.phase==fo3dialogue::Phase::Speaking)
       (void)conversation.AudioDone(conversation.audioToken,true,ctx,mq);};
     assert(conversation.Start(ctx,mq));finish();
     for(uint32_t topic:{0x3b7cu,0x14f3eu,0x151b6u}){
       const auto choice=std::find_if(conversation.choices.begin(),conversation.choices.end(),[&](const auto& row){return row.topic==topic;});
       assert(choice!=conversation.choices.end());
       assert(conversation.Choose(size_t(choice-conversation.choices.begin()),ctx,mq));
       if(topic==0x151b6)assert(conversation.info==0x151bc);
       finish();
     }
     assert(mq.ItemCount(0xf)==0&&mq.Snapshot().pipboy.quests.at(0x14e87).stage==40);
     assert(mq.Snapshot().pipboy.quests.at(0x14e87).objectives.at(40).displayed);
     mq.RegisterOriginalActors();
     // Execute the complete original pre-ambush result, without advancing a stage.
     for(const auto& item:c.pipboy.quests.at(0x14e87).stages.at(80).items)
       assert(mq.ExecuteDialogueResult(item.result,error));
     for(uint16_t stage:{55,142,150})for(const auto& item:c.pipboy.quests.at(0x14e87).stages.at(stage).items)
       assert(mq.ExecuteDialogueResult(item.result,error));
     const auto vargas=c.pipboy.formNames.at("mq01vargasref");
     assert(!mq.Essential(vargas)&&mq.QuestScriptDelay(0x14e87)==5);
     const auto sarah=c.pipboy.formNames.at("mq01sarahref");float ai=-1;
     assert(mq.ActorAIValue(sarah,0,ai)&&ai==0);
     assert(mq.ActorAIValue(sarah,57,ai)&&ai==0);
     assert(!mq.SetActorAIValue(sarah,57,3)&&!mq.SetActorAIValue(sarah,0,NAN));
     auto save="/tmp/fq-mq01-paid-"+std::to_string(getpid());assert(mq.Save(save,error));
     Player returned(start);assert(returned.Restore(save,error));
     assert(returned.ItemCount(0xf)==0&&returned.Snapshot().pipboy.quests.at(0x14e87).stage==40);
     assert(returned.Snapshot().pipboy.dialogueVariables==mq.Snapshot().pipboy.dialogueVariables);
     assert(returned.ActorAIValue(sarah,0,ai)&&ai==0&&returned.ActorAIValue(sarah,57,ai)&&ai==0);
     assert(!returned.Essential(vargas)&&returned.QuestScriptDelay(0x14e87)==5);
     assert(returned.Snapshot().factionRelations==mq.Snapshot().factionRelations);
     auto corrupt=Read(save);size_t aiTag=0;
     for(size_t i=20;i+20<corrupt.size();++i)if(corrupt[i]=='N'&&corrupt[i+1]=='A'&&corrupt[i+2]=='I'&&corrupt[i+3]=='1'){aiTag=i;break;}
     assert(aiTag);Put(corrupt,aiTag+16,0x7fc00000);Seal(corrupt);Write(save,corrupt);
     const auto beforeBadAI=returned.Revision();assert(!returned.Restore(save,error)&&returned.Revision()==beforeBadAI);
     unlink(save.c_str());
     std::cout<<"Original MQ01 paid Moriarty response, completion token, caps, GNR objective and reload passed\n";
    }
    struct Expected {uint32_t id,ammo,projectile;const char*edid;uint8_t clip,animation;uint16_t damage;int health;};
    const Expected expected[]={
      {0x434f,0x4241,0x2cd5f,"Weap10mmPistol",12,3,9,150},
      {0x4333,0x207f7,0x21404,"WeapHuntingRifle",5,5,25,500},
      {0x1ffec,0x4240,0x426d,"WeapAssaultRifle",24,6,8,300},
      {0x4327,0x28eea,0x3bf07,"WeapShotgunCombat",12,5,55,240},
      {0x4335,0x20772,0x14b0f,"WeapLaserPistol",30,4,12,350},
      {0x4336,0x4485,0x14b0f,"WeapLaserRifle",24,7,23,1000}
    };
    for(const auto&e:expected) {
      const auto&item=c.items.at(e.id);const auto&w=item.weapon;
      assert(item.editorId==e.edid&&item.maxCondition==e.health);
      assert(w.Firearm()&&w.ammo==e.ammo&&w.projectile==e.projectile);
      assert(w.clip==e.clip&&w.animation==e.animation&&w.damage==e.damage);
      assert(c.items.at(w.ammo).kind==ItemKind::Ammo);
      assert(c.weapons.models.at(w.firstPerson)==item.model);
      const auto&q=c.weapons.projectiles.at(w.projectile);
      assert(q.speed==10000&&q.gravity==0&&q.range==10000);
      assert(q.Hitscan()==(w.animation!=4&&w.animation!=7));
      assert(w.fire2D&&w.fire3D&&w.dry&&w.ammoUse==1&&w.pellets>0);
      std::cout<<item.editorId<<" clip="<<unsigned(w.clip)<<" damage="<<w.damage<<" ammo="<<std::hex<<w.ammo<<std::dec<<" hitscan="<<q.Hitscan()<<"\n";
    }
    {
      auto rifleCatalog=c;rifleCatalog.initial.inventory.clear();rifleCatalog.initial.nextStackId=1;Player rifle(rifleCatalog);
      assert(rifle.Add(0x1ffec,1,.63f));const auto id=rifle.Snapshot().inventory.front().id;
      assert(rifle.Equip(id)&&rifle.Add(0x4240,31)&&rifle.EjectMagazine(id)&&rifle.LoadMagazine(id));
      assert(rifle.ChamberWeapon(id)&&rifle.FireWeapon(id)&&rifle.Weapon(id)->loadedRounds==23&&rifle.AmmoReserve(0x4240)==7);
      const auto condition=rifle.Weapon(id)->condition;fo3weapon::WorldPose pose;pose.cell=0xa96;pose.velocity={1,2,3};
      assert(rifle.DropWeapon(id,pose)&&rifle.PickupWorldWeapon(id));assert(rifle.Weapon(id)->condition==condition&&rifle.Weapon(id)->loadedRounds==23);
    }
    const auto&w=c.items.at(0x434f);
    std::cout<<"10mm weight="<<w.weight<<" value="<<w.value<<" condition="<<w.maxCondition<<" ammoName="<<c.items.at(0x4241).name<<"\n";
    const auto&d=w.weapon;std::cout<<"rate="<<d.rate<<" shots="<<d.shotsPerSecond<<" minSpread="<<d.minSpread<<" spread="<<d.spread<<" reload="<<unsigned(d.reload)<<" reloadTime="<<d.reloadTime<<" delay="<<d.delayMin<<"/"<<d.delayMax<<" sounds="<<std::hex<<d.fire3D<<"/"<<d.fire2D<<"/"<<d.dry<<"/"<<d.equip<<"/"<<d.unequip<<std::dec<<"\n";
    assert(w.model=="Weapons\\1HandPistol\\10mmPistol.NIF");
    assert(w.weapon.fire2D==223962&&w.weapon.dry==127516);
    assert(std::fabs(w.weapon.shotsPerSecond-6.f)<.0001f);
    assert(!w.weapon.Automatic());
    assert(c.items.at(0x4241).editorId=="Ammo10mm");
    assert(c.items.at(0x1ffec).weapon.Automatic()&&c.items.at(0x1ffec).weapon.rate==8);
    assert(std::fabs(c.weapons.damageGun-.03f)<1e-6f);
    assert(std::fabs(c.weapons.damageEnergy-.04f)<1e-6f);
    assert(std::fabs(c.weapons.damageLauncher-.06f)<1e-6f);
  }
  std::cout<<"Weapon record tests passed\n";
}
