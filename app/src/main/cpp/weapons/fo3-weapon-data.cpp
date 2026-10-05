#include "fo3-weapon-data.h"
#include "data/fo3-esm-reader.h"
#include <cmath>
namespace fo3weapon {
bool Relevant(const std::string&t){return t=="PROJ"||t=="STAT"||t=="NPC_";}
bool DecodeWeapon(const std::vector<uint8_t>&p,Definition&out){
  Definition o;bool data=false,dnam=false;
  fo3esm::WalkSubrecords(p,[&](const char*tag,const uint8_t*b,uint32_t n){
    const std::string t(tag,4);auto u=[&](size_t a){return fo3esm::ReadU32(b+a);};auto f=[&](size_t a){return fo3esm::ReadF32(b+a);};
    // Fallout 3 DATA is 15 bytes, not the New Vegas layout.
    if(t=="DATA"&&n==15){o.damage=fo3esm::ReadU16(b+12);o.clip=b[14];data=true;}
    if(t=="DNAM"&&n>=136){
      o.animation=u(0);o.animationMult=f(4);o.flags=b[12];o.grip=b[13];o.ammoUse=b[14];o.reload=b[15];
      o.minSpread=f(16);o.spread=f(20);o.projectile=u(36);o.pellets=b[42];o.minRange=f(44);o.maxRange=f(48);
      o.flags2=u(56);o.attackMult=f(60);o.rate=f(64);o.rumbleLeft=f(72);o.rumbleRight=f(76);o.rumbleDuration=f(80);
      o.degradationOverride=f(84);o.shotsPerSecond=f(88);o.reloadTime=f(92);o.skill=u(104);o.delayMin=f(128);o.delayMax=f(132);dnam=true;
    }
    if(n==4){if(t=="NAM0")o.ammo=u(0);if(t=="WNAM")o.firstPerson=u(0);if(t=="SNAM")o.fire3D=u(0);if(t=="XNAM")o.fire2D=u(0);if(t=="TNAM")o.dry=u(0);if(t=="NAM9")o.equip=u(0);if(t=="NAM8")o.unequip=u(0);}
    if(t=="NNAM")o.embeddedNode=fo3esm::ZString(b,n);
    if(t=="CRDT"&&n>=16){o.criticalDamage=fo3esm::ReadU16(b);o.criticalMult=f(4);o.criticalEffect=u(12);}
  });
  o.valid=data&&dnam&&o.animation<=12;
  for(float v:{o.animationMult,o.attackMult,o.minSpread,o.spread,o.minRange,o.maxRange,o.rate,o.shotsPerSecond,o.reloadTime,o.delayMin,o.delayMax,o.rumbleLeft,o.rumbleRight,o.rumbleDuration,o.degradationOverride,o.criticalMult})if(!std::isfinite(v)||v<0)o.valid=false;
  out=std::move(o);return out.valid;
}
void Decode(Definitions&o,const std::string&t,uint32_t id,const std::vector<uint8_t>&p){
  Projectile q;Definitions::Actor actor;bool actorData=false;bool valid=false;std::string model,editor;
  fo3esm::WalkSubrecords(p,[&](const char*tag,const uint8_t*b,uint32_t n){const std::string k(tag,4);
    if(k=="EDID")editor=fo3esm::ZString(b,n);
    if(t=="NPC_"&&k=="DATA"&&n>=11){actor.health=static_cast<int32_t>(fo3esm::ReadU32(b));actor.endurance=b[6];actorData=true;}
    if(t=="NPC_"&&k=="ACBS"&&n==24){actor.flags=fo3esm::ReadU32(b);actor.level=fo3esm::ReadU16(b+8);actor.minLevel=fo3esm::ReadU16(b+10);actor.maxLevel=fo3esm::ReadU16(b+12);actor.templates=fo3esm::ReadU16(b+22);}
    if(t=="NPC_"&&k=="TPLT"&&n==4)actor.templateId=fo3esm::ReadU32(b);
    if(k=="MODL")model=fo3esm::ZString(b,n);
    if(k=="NAM1")q.flashModel=fo3esm::ZString(b,n);
    if(t=="PROJ"&&k=="DATA"&&n>=68){q.flags=fo3esm::ReadU16(b);q.type=fo3esm::ReadU16(b+2);q.gravity=fo3esm::ReadF32(b+4);q.speed=fo3esm::ReadF32(b+8);q.range=fo3esm::ReadF32(b+12);q.explosion=fo3esm::ReadU32(b+36);q.flashDuration=fo3esm::ReadF32(b+44);q.impactForce=fo3esm::ReadF32(b+52);valid=true;}
    if(t=="GMST"&&k=="DATA"&&n==4){float v=fo3esm::ReadF32(b);if(std::isfinite(v)&&v>=0){if(editor=="fDamageToWeaponGunMult")o.damageGun=v;if(editor=="fDamageToWeaponEnergyMult")o.damageEnergy=v;if(editor=="fDamageToWeaponLauncherMult")o.damageLauncher=v;
      if(editor=="fDamageSkillBase")o.skillBase=v;
      if(editor=="fDamageSkillMult")o.skillMult=v;
      if(editor=="fDamageGunWeapCondBase")o.conditionBase=v;
      if(editor=="fDamageGunWeapCondMult")o.conditionMult=v;
      if(editor=="fAVDNPCHealthLevelMult")o.npcHealthLevel=v;
      if(editor=="fAVDNPCHealthEnduranceMult")o.npcHealthEndurance=v;
    }}
  });
  if(t=="STAT")o.models[id]=model;
  if(t=="NPC_"&&actorData&&actor.health>=0&&actor.endurance<=10)o.actors[id]=actor;
  for(float v:{q.gravity,q.speed,q.range,q.flashDuration,q.impactForce})if(!std::isfinite(v)||v<0)valid=false;
  if(t=="PROJ"&&valid){q.model=model;o.projectiles[id]=std::move(q);}
}
bool ValidPose(const WorldPose&p){
  if(!p.cell&&!p.world)return false;
  for(const auto&a:{p.position,p.velocity,p.angularVelocity})for(float v:a)if(!std::isfinite(v))return false;
  float n=0;for(float v:p.rotation){if(!std::isfinite(v))return false;n+=v*v;}return n>.99f&&n<1.01f;
}
}
