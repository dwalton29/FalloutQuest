#pragma once
#include "fo3-npc-state.h"
#include "player/fo3-player-state.h"
namespace fo3npc {
struct AiData {
  uint8_t aggression=0,confidence=0,energy=0,responsibility=0,assistance=0;
  int32_t radius=0;bool radiusEnabled=false,valid=false;
};
inline AiData AI(const fo3pipdata::Definitions& d,uint32_t base){
  AiData out;const auto* a=fo3pipdata::ActorCategory(d,base,8);
  if(!a||a->aiData.size()!=20)return out;
  const auto& b=a->aiData;
  out.aggression=b[0];out.confidence=b[1];out.energy=b[2];out.responsibility=b[3];out.assistance=b[14];out.radiusEnabled=b[15]&1;
  uint32_t r=0;for(int i=0;i<4;++i)r|=uint32_t(b[16+i])<<(i*8);out.radius=int32_t(r);
  out.valid=out.aggression<=3&&out.confidence<=4&&out.assistance<=2&&out.radius>=0;return out;
}
// Explicit FACT XNAM combat reactions; disposition alone is not hostility.
enum class Reaction { Neutral, Enemy, Ally, Friend };
inline Reaction Relationship(const fo3player::Catalog& c,uint32_t a,uint32_t b){
  const auto* x=fo3pipdata::ActorCategory(c.pipboy,a,4);
  const auto* y=fo3pipdata::ActorCategory(c.pipboy,b,4);
  if(!x||!y)return Reaction::Neutral;
  Reaction result=Reaction::Neutral;
  for(const auto& from:x->factions)if(from.second>=0)for(const auto& to:y->factions)if(to.second>=0){
    if(from.first==to.first)result=Reaction::Ally;
    const auto f=c.weapons.relations.find(from.first);if(f==c.weapons.relations.end())continue;
    const auto r=f->second.find(to.first);if(r==f->second.end())continue;
    if(r->second==1)return Reaction::Enemy;
    if(r->second==2)result=Reaction::Ally;
    if(r->second==3&&result==Reaction::Neutral)result=Reaction::Friend;
  }
  return result;
}
inline bool Acquires(const AiData& a,Reaction reaction){
  return a.valid&&(a.aggression==3||(a.aggression>=1&&reaction==Reaction::Enemy)||
    (a.aggression==2&&reaction==Reaction::Neutral));
}
inline bool Assists(const AiData& a,Reaction reaction){
  return a.valid&&((a.assistance>=1&&reaction==Reaction::Ally)||(a.assistance==2&&reaction==Reaction::Friend));
}
// xEdit FO3 PKDT general bit 22: defensive actors do not initiate combat.
inline bool Defensive(const fo3pipdata::PackageDefinition* package){return package&&(package->flags&(1u<<22));}
// A received hit should not erase a committed attack or restart a reload.
inline bool MayPlayHitReaction(const RuntimeState& s){
  return s.activity!=Activity::Combat||(!s.pendingAttack&&s.reloadUntil<=0);
}
inline float CombatSetting(const fo3player::Catalog& c,const char* key,float fallback=0){
  const auto it=c.npcCombatSettings.find(key);return it==c.npcCombatSettings.end()?fallback:it->second;
}
// Quest morale bridge: source confidence determines the source threshold;
// comparing remaining health to it is an explicit approximation, not the
// recovered Bethesda relative combat-strength algorithm.
inline bool ShouldFlee(const AiData& ai,const fo3player::Catalog& c,
                       float health,float maximum,bool armed){
  if(!ai.valid||health<=0)return false;
  if(ai.confidence==0)return true;
  if(ai.confidence==4)return false;
  if(!armed&&ai.confidence<=2)return true;
  const char* key=ai.confidence==1?"fConfidenceCautious":
                  ai.confidence==2?"fConfidenceAverage":"fConfidenceBrave";
  const float threshold=CombatSetting(c,key);
  return threshold>0&&maximum>0&&health/maximum<=threshold;
}
inline float CombatChoice(uint32_t reference,uint32_t serial,float minimum,float maximum){
  const uint32_t hash=reference*2654435761u^serial*2246822519u;
  return minimum+(std::max(minimum,maximum)-minimum)*float(hash%1000u)/999.f;
}
// Timing derives from WEAP/CSTY; no catch-up volley after a long frame.
inline float ShotInterval(const fo3weapon::Definition& d,const fo3weapon::Definitions::CombatStyle* style){
  const float rate=d.Automatic()?d.rate:d.shotsPerSecond;
  float delay=d.delayMin;
  if(!d.Automatic()&&style&&style->valid)delay*=style->delayMin;
  return std::max(delay,rate>0?1.f/rate:0.f);
}
inline bool FireReady(RuntimeState& s,const fo3weapon::Definition& d,double now,const fo3weapon::Definitions::CombatStyle* style=nullptr){
  const auto interval=ShotInterval(d,style);
  if(s.activity!=Activity::Combat||!std::isfinite(now)||interval<=0||s.reloadUntil>0||now<s.nextAttack)return false;
  if(style&&style->valid&&style->fireMax>0){
    if(now<s.burstWaitUntil)return false;
    if(s.burstUntil>0&&now>=s.burstUntil){
      s.burstUntil=0;s.burstWaitUntil=now+CombatChoice(s.reference,s.fireSequence++,style->pauseMin,style->pauseMax);
      return false;
    }
    if(s.burstUntil<=0)s.burstUntil=now+std::max(.01f,CombatChoice(s.reference,s.fireSequence++,style->fireMin,style->fireMax));
  }
  float delay=d.delayMin;
  if(!d.Automatic()){
    delay=CombatChoice(s.reference,s.fireSequence++,d.delayMin,d.delayMax);
    if(style&&style->valid)delay*=CombatChoice(s.reference,s.fireSequence++,style->delayMin,style->delayMax);
  }
  s.nextAttack=now+std::max(interval,delay);return true;
}
}
