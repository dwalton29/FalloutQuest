#include "player/fo3-player-state.h"
#include "weapons/fo3-weapon-interaction.h"
#include "weapons/fo3-weapon-hit.h"
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
  Catalog c;
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
  assert(U32(good,4)==6);
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
  bad=seededSave;Put(bad,bad.size()-8,0x7fc00000);Seal(bad);Write(path,bad);
  assert(!resumed.Restore(path,error)&&resumed.Revision()==seedRevision);
  bad=seededSave;
  const auto ownedId=resumed.Snapshot().inventory.front().id;
  Put(bad,bad.size()-84,static_cast<uint32_t>(ownedId));
  Put(bad,bad.size()-80,static_cast<uint32_t>(ownedId>>32));
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
  Player p(c);assert(p.BootstrapDevelopmentWeapon());const auto id=p.EquippedWeapon()->id;
  assert(p.ActorHealth(200)==100&&p.ActorHealth(201)==100);
  const float amount=p.WeaponDamage(id);assert(amount>=4.5f&&amount<=9);
  assert(!p.ApplyWeaponHit(0,200,amount)&&!p.ApplyWeaponHit(0x434f,0,amount));
  assert(!p.ApplyWeaponHit(0x434f,200,NAN)&&!p.ApplyWeaponHit(0x434f,200,-1));
  assert(p.WeaponHit(id,200));assert(std::fabs(p.ActorHealth(200)-(100-amount))<1e-5f);
  fo3weapon::WorldPose pose;pose.cell=123;assert(p.DropWeapon(id,pose));
  // A travelling shot retains the damage captured when fired after ownership changes.
  assert(p.ApplyWeaponHit(0x434f,200,amount));assert(!p.WeaponHit(id,200));
  assert(p.ApplyWeaponHit(0x434f,201,1000)&&p.ActorHealth(201)==1);
  assert(p.ApplyWeaponHit(0x434f,200,1000)&&p.ActorHealth(200)==0);
  assert(!p.ApplyWeaponHit(0x434f,200,1));
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
int main(int argc,char**argv) {
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
