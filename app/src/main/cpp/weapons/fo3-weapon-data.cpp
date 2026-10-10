#include "fo3-weapon-data.h"
#include "data/fo3-esm-reader.h"
#include <cmath>
#include <unordered_set>
#include <functional>
namespace fo3weapon {
bool Relevant(const std::string&t){return t=="PROJ"||t=="STAT"||t=="NPC_"||t=="LVLN"||t=="FACT"||t=="CSTY";}
bool DecodeWeapon(const std::vector<uint8_t>&p,Definition&out){
  Definition o;bool data=false,dnam=false;
  fo3esm::WalkSubrecords(p,[&](const char*tag,const uint8_t*b,uint32_t n){
    const std::string t(tag,4);auto u=[&](size_t a){return fo3esm::ReadU32(b+a);};auto f=[&](size_t a){return fo3esm::ReadF32(b+a);};
    // Fallout 3 DATA is 15 bytes, not the New Vegas layout.
    if(t=="DATA"&&n==15){o.damage=fo3esm::ReadU16(b+12);o.clip=b[14];data=true;}
    if(t=="DNAM"&&n>=136){
      o.animation=u(0);o.animationMult=f(4);o.flags=b[12];o.grip=b[13];o.ammoUse=b[14];o.reload=b[15];
      o.minSpread=f(16);o.spread=f(20);o.projectile=u(36);o.pellets=b[42];o.minRange=f(44);o.maxRange=f(48);
      o.attackAnimation=b[41];o.flags2=u(56);o.attackMult=f(60);o.rate=f(64);o.rumbleLeft=f(72);o.rumbleRight=f(76);o.rumbleDuration=f(80);
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
  Projectile q;Definitions::Actor actor;Definitions::CombatStyle style;
  bool actorData=false;bool valid=false;std::string model,editor;
  Definitions::LevelledActor levelled;bool chance=false,listFlags=false,listValid=true;
  fo3esm::WalkSubrecords(p,[&](const char*tag,const uint8_t*b,uint32_t n){const std::string k(tag,4);
    if(t=="LVLN") {
      if(k=="LVLD"){if(n!=1||chance)listValid=false;else{levelled.chanceNone=b[0];chance=true;}}
      if(k=="LVLF"){if(n!=1||listFlags)listValid=false;else{levelled.flags=b[0];listFlags=true;}}
      if(k=="LVLG"){if(n!=4)listValid=false;else levelled.chanceGlobal=fo3esm::ReadU32(b);}
      if(k=="LVLO") {
        if(n!=12||levelled.entries.size()>=4096)listValid=false;
        else levelled.entries.push_back({fo3esm::ReadU16(b),fo3esm::ReadU16(b+8),fo3esm::ReadU32(b+4)});
      }
    }
    if(k=="EDID")editor=fo3esm::ZString(b,n);
    if(t=="NPC_"&&k=="DATA"&&n>=11){actor.health=static_cast<int32_t>(fo3esm::ReadU32(b));actor.endurance=b[6];actorData=true;}
    if(t=="NPC_"&&k=="ACBS"&&n==24){actor.flags=fo3esm::ReadU32(b);actor.level=fo3esm::ReadU16(b+8);actor.minLevel=fo3esm::ReadU16(b+10);actor.maxLevel=fo3esm::ReadU16(b+12);actor.templates=fo3esm::ReadU16(b+22);}
    if(t=="NPC_"&&k=="TPLT"&&n==4)actor.templateId=fo3esm::ReadU32(b);
    if(t=="NPC_"&&k=="DNAM"&&n==28)for(size_t i=0;i<14;++i)actor.skills[i]=b[i];
    if(t=="ARMO"&&k=="DNAM"&&n==4)o.armourDR[id]=std::max(0.f,float(int16_t(fo3esm::ReadU16(b)))/100.f);
    if(t=="FACT"&&k=="XNAM"&&n==12)o.relations[id][fo3esm::ReadU32(b)]=int32_t(fo3esm::ReadU32(b+8));
    if(t=="CSTY"&&k=="CSTD"&&n>=82)style.flags=fo3esm::ReadU16(b+80);
    if(t=="CSTY"&&k=="CSSD"&&n==64){
      style.coverRadius=fo3esm::ReadF32(b);style.coverChance=fo3esm::ReadF32(b+4);
      style.pauseMin=fo3esm::ReadF32(b+8);style.pauseMax=fo3esm::ReadF32(b+12);
      style.waitMin=fo3esm::ReadF32(b+16);style.waitMax=fo3esm::ReadF32(b+20);
      style.fireMin=fo3esm::ReadF32(b+24);style.fireMax=fo3esm::ReadF32(b+28);
      style.rangeMin=fo3esm::ReadF32(b+32);style.restrictions=fo3esm::ReadU32(b+40);
      style.rangeMax=fo3esm::ReadF32(b+44);style.radius=fo3esm::ReadF32(b+52);
      style.delayMin=fo3esm::ReadF32(b+56);style.delayMax=fo3esm::ReadF32(b+60);style.valid=true;
      for(float v:{style.waitMin,style.waitMax,style.fireMin,style.fireMax,style.rangeMin,style.rangeMax,style.radius,style.delayMin,style.delayMax,style.pauseMin,style.pauseMax,style.coverRadius,style.coverChance})if(!std::isfinite(v)||v<0)style.valid=false;
      if(style.coverChance>100)style.valid=false;
      if(style.restrictions>2)style.valid=false;
    }
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
      if(editor=="fMaxArmorRating")o.drMax=v;
      if(editor=="fSneakMaxDistance")o.detectionDistance=v;
    }}
  });
  if(t=="STAT")o.models[id]=model;
  if(t=="CSTY"){o.styles[id]=style;if(editor=="DefaultCombatstyle"&&style.valid)o.defaultCombatStyle=id;}
  if(t=="NPC_"&&actorData&&actor.health>=0&&actor.endurance<=10)o.actors[id]=actor;
  if(t=="LVLN"){levelled.valid=listValid&&chance&&listFlags&&!levelled.entries.empty();o.levelledActors[id]=std::move(levelled);}
  for(float v:{q.gravity,q.speed,q.range,q.flashDuration,q.impactForce})if(!std::isfinite(v)||v<0)valid=false;
  if(t=="PROJ"&&valid){q.model=model;o.projectiles[id]=std::move(q);}
}
static bool SameStatistics(const Definitions::Actor& a,const Definitions::Actor& b) {
  // Sex/essential/AI flags are Traits, not Statistics. Only the health/level
  // calculation flags are consumed with DATA/DNAM by the existing runtime.
  return (a.flags&0x90)==(b.flags&0x90)&&a.level==b.level&&a.minLevel==b.minLevel&&
    a.maxLevel==b.maxLevel&&a.health==b.health&&a.endurance==b.endurance&&a.skills==b.skills;
}
void FinalizeStatistics(Definitions& d) {
  d.invariantStatistics.clear();
  std::unordered_set<uint32_t> visiting;
  unsigned budget=0;
  std::function<uint32_t(uint32_t,unsigned)> resolve=[&](uint32_t id,unsigned depth)->uint32_t {
    if(depth>=16||budget++>=65536||!visiting.insert(id).second)return 0;
    uint32_t source=0;
    const auto npc=d.actors.find(id);
    if(npc!=d.actors.end())source=(npc->second.templates&2)?resolve(npc->second.templateId,depth+1):id;
    else {
      const auto list=d.levelledActors.find(id);
      if(list!=d.levelledActors.end()&&list->second.valid&&list->second.chanceNone==0&&
         list->second.chanceGlobal==0&&list->second.flags<=1) {
        bool identical=true;
        for(const auto& entry:list->second.entries) {
          // Restrict to always-eligible, single-actor entries. Lists whose
          // eligibility changes with level still require a spawn resolver.
          if(entry.level!=1||entry.count!=1){identical=false;break;}
          const auto leaf=resolve(entry.actor,depth+1);
          if(!leaf||(source&&!SameStatistics(d.actors.at(source),d.actors.at(leaf)))){identical=false;break;}
          source=leaf;
        }
        if(!identical)source=0;
      }
    }
    visiting.erase(id);return source;
  };
  for(const auto& list:d.levelledActors){budget=0;if(const auto source=resolve(list.first,0))d.invariantStatistics[list.first]=source;}
}
const Definitions::Actor* ActorStatistics(const Definitions& d,uint32_t id) {
  for(unsigned depth=0;depth<16;++depth) {
    const auto a=d.actors.find(id);
    if(a!=d.actors.end()){if(!(a->second.templates&2))return &a->second;id=a->second.templateId;}
    else {const auto source=d.invariantStatistics.find(id);if(source==d.invariantStatistics.end())return nullptr;id=source->second;}
  }
  return nullptr;
}
bool ValidPose(const WorldPose&p){
  if(!p.cell&&!p.world)return false;
  for(const auto&a:{p.position,p.velocity,p.angularVelocity})for(float v:a)if(!std::isfinite(v))return false;
  float n=0;for(float v:p.rotation){if(!std::isfinite(v))return false;n+=v*v;}return n>.99f&&n<1.01f;
}
}
