#pragma once
#include "fo3-weapon-data.h"
#include "player/fo3-vr-body.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace fo3vats {
inline constexpr size_t QueueLimit=6; // vats_menu.xml _number_of_visible_items.
inline constexpr uint8_t NoPart=255;

inline float Cost(const fo3weapon::VatsSettings& settings,const fo3weapon::Definition& weapon){
  if((weapon.flags2&8)&&std::isfinite(weapon.vatsAP)&&weapon.vatsAP>=0)return weapon.vatsAP;
  if(weapon.animation>=8)return settings.launcherAP;
  return weapon.LongGun()?settings.rifleAP:settings.pistolAP;
}

inline unsigned Burst(const fo3weapon::VatsSettings& settings,const fo3weapon::Definition& weapon){
  if(!weapon.Automatic())return 1;
  const float seconds=(weapon.flags2&0x800)?settings.longBurst:settings.burst;
  return unsigned(std::clamp(std::round(seconds*weapon.shotsPerSecond),1.f,255.f));
}

// The native records supply the balancing inputs. The VR estimate uses the
// distance from the real muzzle and the available shot line to the part.
inline unsigned Chance(const fo3weapon::VatsSettings& settings,const fo3weapon::Definition& weapon,
                       const fo3weapon::BodyPart& part,float distance,float skill,bool clear){
  if(!clear||!std::isfinite(distance)||distance<0||distance>settings.maxDistance)return 0;
  const float cap=std::clamp(settings.maxChance,0.f,100.f);
  if(part.flags&0x40)return unsigned(std::round(std::min(cap,float(part.chance))));
  const float range=weapon.maxRange>0?std::min(weapon.maxRange,settings.maxDistance):settings.maxDistance;
  if(range<=0||distance>=range)return 0;
  const float accuracy=std::clamp((float(weapon.vatsChance)+part.chance)/200.f,0.f,1.f);
  const float training=std::clamp(skill*settings.skillFactor/100.f,0.f,1.f);
  const float falloff=std::clamp(distance/range,0.f,1.f);
  const float penalty=falloff*settings.distanceFactor*
                      (1.f-accuracy*.60f-training*.28f+weapon.minSpread*settings.spreadMult*.025f);
  return unsigned(std::round(std::clamp(cap*(1.f-penalty)*settings.hitMult,0.f,cap)));
}

inline float AimScore(fo3vr::V origin,fo3vr::V direction,fo3vr::V point,float minimumDot){
  const auto offset=point-origin;
  const float length=fo3vr::Length(offset);
  if(!std::isfinite(length)||length<.001f)return -1;
  const float dot=fo3vr::Dot(direction,offset)*(1.f/length);
  return dot>=minimumDot?dot:-1.f;
}

struct Edges { bool enter=false,select=false,confirm=false,cancel=false; };
struct Input {
  bool left=true,right=true,a=true,b=true;
  Edges Sample(float lt,float rt,bool accept,bool back){
    const auto trigger=[](bool& down,float value){
      if(!std::isfinite(value))return false;
      const bool press=!down&&value>=.65f;
      if(value<=.25f)down=false;
      else if(value>=.65f)down=true;
      return press;
    };
    const Edges edges{trigger(left,lt),trigger(right,rt),accept&&!a,back&&!b};
    a=accept;b=back;
    return edges;
  }
};
// Only the actions V.A.T.S. consumed wait for release on exit. Grips and
// locomotion remain live, including when the player is holding a weapon.
struct InputRelease {
  bool left=false,right=false,a=false,b=false;
  void Hold(float lt,float rt,bool accept,bool back){
    left=lt>.25f;right=rt>.25f;a=accept;b=back;
  }
  void Filter(float& lt,float& rt,bool& accept,bool& back){
    left=left&&lt>.25f;right=right&&rt>.25f;
    a=a&&accept;b=b&&back;
    if(left)lt=0;
    if(right)rt=0;
    if(a)accept=false;
    if(b)back=false;
  }
};
struct Shot {
  uint32_t target=0;
  uint8_t part=NoPart;
  unsigned rounds=0,chance=0;
  float ap=0;
};
enum class QueueResult { Added, NoTarget, NoChance, NoAmmo, LowAP, Full };
enum class Phase { Inactive, Aiming, Executing };
struct Session {
  Phase phase=Phase::Inactive;
  uint64_t weapon=0;
  std::array<Shot,QueueLimit> shots{};
  size_t count=0,index=0;
  unsigned roundsFired=0;
  bool Active()const{return phase!=Phase::Inactive;}
  void Open(uint64_t instance){*this={};weapon=instance;phase=Phase::Aiming;}
  float ReservedAP()const{
    float ap=0;
    for(size_t i=0;i<count;++i)ap+=shots[i].ap;
    return ap;
  }
  unsigned ReservedRounds()const{
    unsigned rounds=0;
    for(size_t i=0;i<count;++i)rounds+=shots[i].rounds;
    return rounds;
  }
  QueueResult Queue(Shot shot,float ap,unsigned rounds){
    if(phase!=Phase::Aiming||!shot.target||shot.part==NoPart)return QueueResult::NoTarget;
    if(!shot.chance)return QueueResult::NoChance;
    if(count==shots.size())return QueueResult::Full;
    if(!shot.rounds||rounds<=ReservedRounds())return QueueResult::NoAmmo;
    if(!std::isfinite(shot.ap)||shot.ap<0||shot.ap+ReservedAP()>ap)return QueueResult::LowAP;
    shot.rounds=std::min(shot.rounds,rounds-ReservedRounds());
    shots[count++]=shot;
    return QueueResult::Added;
  }
  bool Confirm(){
    if(phase!=Phase::Aiming||!count)return false;
    phase=Phase::Executing;
    return true;
  }
  const Shot* Current()const{
    return phase==Phase::Executing&&index<count?&shots[index]:nullptr;
  }
  void Skip(){
    if(!Current())return;
    ++index;roundsFired=0;
  }
  void Fired(){
    if(const auto* shot=Current();shot&&++roundsFired>=shot->rounds)Skip();
  }
};
}
