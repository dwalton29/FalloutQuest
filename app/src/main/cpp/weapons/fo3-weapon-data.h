#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
namespace fo3weapon {
enum class Family { Unarmed, Melee, Pistol, EnergyPistol, Rifle, EnergyRifle, Heavy, Thrown };
struct Definition {
  uint32_t animation=0,ammo=0,projectile=0,firstPerson=0,flags2=0,skill=0;
  uint32_t fire3D=0,fire2D=0,dry=0,equip=0,unequip=0,criticalEffect=0;
  uint8_t flags=0,grip=255,reload=255,ammoUse=0,pellets=0,clip=0;
  uint16_t damage=0,criticalDamage=0;
  float animationMult=1,attackMult=1,minSpread=0,spread=0,minRange=0,maxRange=0;
  float rate=0,shotsPerSecond=0,reloadTime=0,delayMin=0,delayMax=0;
  float rumbleLeft=0,rumbleRight=0,rumbleDuration=0,degradationOverride=0,criticalMult=0;
  std::string embeddedNode;
  bool valid=false;
  bool Automatic() const {return flags&2;}
  bool Firearm() const {return valid&&animation>=3&&animation<=9&&ammo&&clip&&ammoUse&&pellets;}
  bool LongGun() const {return animation>=5&&animation<=9;}
  Family Classification() const {
    switch(animation){case 0:return Family::Unarmed;case 1:case 2:return Family::Melee;
    case 3:return Family::Pistol;case 4:return Family::EnergyPistol;case 5:case 6:return Family::Rifle;
    case 7:return Family::EnergyRifle;case 8:case 9:return Family::Heavy;default:return Family::Thrown;}
  }
};
struct Projectile {
  uint16_t flags=0,type=0;
  float gravity=0,speed=0,range=0,flashDuration=0,impactForce=0;
  uint32_t explosion=0;
  std::string model,flashModel;
  bool Hitscan() const {return flags&1;}
};
struct Definitions {
  std::unordered_map<uint32_t,Projectile> projectiles;
  std::unordered_map<uint32_t,std::string> models;
  float damageGun=0,damageEnergy=0,damageLauncher=0;
};
struct WorldPose {
  uint32_t cell=0,world=0;
  std::array<float,3> position{},velocity{},angularVelocity{};
  std::array<float,4> rotation{0,0,0,1};
};
// Additional immutable records must not change the legacy catalog fingerprint.
bool Relevant(const std::string& type);
bool DecodeWeapon(const std::vector<uint8_t>& payload,Definition& out);
void Decode(Definitions& out,const std::string& type,uint32_t id,const std::vector<uint8_t>& payload);
bool ValidPose(const WorldPose& pose);
}
