#include "player/fo3-player-state.h"
#include "weapons/fo3-weapon-interaction.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace fo3player;
int main(int argc,char**argv) {
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
  pose.cell=123;
  assert(fo3weapon::ValidPose(pose));
  pose.velocity[0]=NAN;
  assert(!fo3weapon::ValidPose(pose));
  if(argc>1) {
    Catalog c;std::string error;
    assert(LoadCatalog(argv[1],c,error));
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
    const auto&w=c.items.at(0x434f);
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
