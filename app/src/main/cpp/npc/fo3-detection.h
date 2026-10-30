#pragma once
#include "player/fo3-sneak-defaults.h"
#include <algorithm>
#include <cstdint>
#include <array>
#include <cmath>
namespace fo3sneak {
inline float Setting(const std::unordered_map<std::string,float>& s,const char* key){const auto i=s.find(key);return i==s.end()?0:i->second;}
struct Evidence {
 float distance=0,light=0,perception=0,skill=0,armourWeight=0,action=0;
 bool exterior=false,los=false,moving=false,running=false,sneaking=false,alert=false,sleeping=false,otherCombat=false;
};
// FO3 detector/target formula, not FNV's level and armour-class bonuses.
inline float Value(const std::unordered_map<std::string,float>& s,const Evidence& e){
 const auto g=[&](const char* k){return Setting(s,k);};
 const float limit=g("fSneakMaxDistance")*(e.exterior?g("fSneakExteriorDistanceMult"):1);
 if(limit<=0||!std::isfinite(e.distance)||e.distance>=limit)return -INFINITY;
 const float attenuation=std::pow(std::clamp(1-e.distance/limit,0.f,1.f),g("fSneakDistanceAttenuationExponent"));
 const float movement=e.moving?(g("fSneakBootWeightBase")+g("fSneakBootWeightMult")*e.armourWeight)*(e.running?g("fSneakRunningMult"):1):0;
 const float sound=g("fSneakSoundsMult")*(movement+e.action*g("fSneakActionMult"))*(e.los?1:g("fSneakSoundLosMult"));
 const float visual=e.los?(g("fDetectionSneakLightMod")+std::clamp(e.light,0.f,100.f))*g("fSneakLightMult")*(1+(e.moving?g("fSneakLightMoveMult"):0)+(e.running?g("fSneakLightRunMult"):0)):0;
 const float perception=g("fSneakPerceptionSkillMin")+(g("fSneakPerceptionSkillMax")-g("fSneakPerceptionSkillMin"))*std::clamp(e.perception/10,0.f,1.f);
 const float detector=perception*(1+(e.alert?g("fSneakAlertMod"):0)+(e.sleeping?g("fSneakSleepBonus"):0)+(e.otherCombat?g("fSneakCombatMod"):0));
 return g("fSneakBaseValue")+attenuation*(sound+visual+detector*g("fSneakSkillMult"))-(e.sneaking?e.skill*g("fSneakSkillMult"):0);
}
enum class Phase : uint8_t {Normal,Alert,Combat,Lost};
enum class Indicator : uint8_t {Hidden,Detected,Caution,Danger};
struct Observer {
 Phase phase=Phase::Normal;float value=-INFINITY;bool detected=false,hostile=false;
 double next=0,expires=0;std::array<float,3> lastKnown{};
 // Damage is an explicit detection event even when a silent attack is unseen.
 void Hit(const std::unordered_map<std::string,float>& settings,double now,const std::array<float,3>& point){
  hostile=true;if(phase==Phase::Normal)phase=Phase::Alert;lastKnown=point;
  expires=now+Setting(settings,"fDetectionEventExpireTime");next=now;
 }
 void Step(const std::unordered_map<std::string,float>& settings,float score,bool enemy,double now,const std::array<float,3>& point){
  const auto g=[&](const char* k){return Setting(settings,k);};value=score;hostile=enemy;detected=score>g("fSneakSeenMin");next=now+g("fDetectionTimerSetting");
  if(!enemy){phase=Phase::Normal;return;}
  const bool noticed=score>g("fSneakNoticedMin");
  if(detected){phase=Phase::Combat;lastKnown=point;expires=now+g("fCombatSearchTargetHiddenRemoveTime");}
  else if(phase==Phase::Combat){phase=Phase::Lost;expires=now+g("fCombatSearchTargetHiddenRemoveTime");}
  else if(noticed){if(phase!=Phase::Lost)phase=Phase::Alert;lastKnown=point;expires=now+g("fDetectionEventExpireTime");}
  else if(now>=expires)phase=Phase::Normal;
  next=now+g("fDetectionTimerSetting");
 }
 Indicator Hud()const{return hostile?(detected?Indicator::Danger:phase==Phase::Normal?Indicator::Hidden:Indicator::Caution):(detected?Indicator::Detected:Indicator::Hidden);}
};
inline Indicator Strongest(Indicator a,Indicator b){return static_cast<Indicator>(std::max(int(a),int(b)));}
}
