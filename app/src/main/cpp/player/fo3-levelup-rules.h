#pragma once

#include "pipboy/fo3-pipboy-data.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace fo3levelup {

using Skills = std::array<uint8_t,14>;
using Special = std::array<uint8_t,7>;

// Original CTDA comparison operators (high three bits).
inline bool Compare(float actual,float expected,uint8_t flags) {
  if(!std::isfinite(actual)||!std::isfinite(expected))return false;
  switch(flags>>5) {
    case 0:return actual==expected;
    case 1:return actual!=expected;
    case 2:return actual>expected;
    case 3:return actual>=expected;
    case 4:return actual<expected;
    case 5:return actual<=expected;
    default:return false;
  }
}

// Fallout3.esm level-up prerequisite CTDAs use GetPermanentActorValue (495),
// GetIsSex (70) and GetLevel (80). Unknown conditions fail closed; no
// unsupported perk is made available by guessing its prerequisites.
inline bool Requirement(const fo3pipdata::Condition& c,uint16_t targetLevel,
                        const Special& special,const Skills& skills,
                        bool playerFemale) {
  if((c.flags&0x1f)!=0||c.run!=0||c.reference||c.b)return false;
  float actual=0;
  switch(c.function) {
    case 495:
      if(c.a>=5&&c.a<=11)actual=special[c.a-5];
      else if(c.a>=32&&c.a<=45)actual=skills[c.a-32];
      else return false;
      break;
    case 70: actual=(uint32_t(playerFemale)==c.a)?1.f:0.f;break;
    case 80: actual=targetLevel;break;
    default:return false;
  }
  return Compare(actual,c.value,c.flags);
}

inline bool PerkEligible(uint32_t id,const fo3pipdata::Perk& perk,
                        uint16_t targetLevel,const Special& special,
                        const Skills& skills,bool playerFemale,
                        const std::unordered_map<uint32_t,uint8_t>& owned) {
  if(!id||targetLevel<2||!perk.playable||perk.hidden||!perk.ranks||
     targetLevel<perk.minLevel)return false;
  const auto found=owned.find(id);
  if(found!=owned.end()&&found->second>=perk.ranks)return false;
  for(const auto& condition:perk.prerequisites)
    if(!Requirement(condition,targetLevel,special,skills,playerFemale))return false;
  return true;
}

// Fallout3.esm: iLevelUpSkillPointsBase=11 and
// iLevelUpSkillPointsInterval=1, making 10 + Intelligence. Existing
// Educated perk effects modify this entry point each subsequent level.
// Return -1 if effect semantics are unsupported rather than allocating wrong
// skill points.
inline int SkillBudget(const Special& special,int base,int interval,
                       const std::unordered_map<uint32_t,uint8_t>& owned,
                       const std::unordered_map<uint32_t,fo3pipdata::Perk>& defs) {
  if(special[4]<1||special[4]>10||base<0||interval<0)return -1;
  double budget=double(base)+double(special[4]-1)*interval;
  for(const auto& kv:owned) {
    const auto p=defs.find(kv.first);
    if(p==defs.end())return -1;
    for(const auto& effect:p->second.effects) {
      if(effect.type!=2||effect.entryPoint!=10||
         effect.rank+1!=kv.second)continue;
      for(const auto& tab:effect.conditions)if(!tab.second.empty())return -1;
      if(effect.scripted||effect.parameterType!=1||
         effect.parameters.size()!=sizeof(float))return -1;
      float amount=0;
      std::memcpy(&amount,effect.parameters.data(),sizeof(float));
      if(!std::isfinite(amount))return -1;
      switch(effect.function){
        case 1:budget=amount;break;
        case 2:budget+=amount;break;
        case 3:budget*=amount;break;
        default:return -1;
      }
    }
  }
  if(!std::isfinite(budget)||budget<0||budget>1000)return -1;
  return int(budget);
}

inline bool SkillAllocationValid(const Skills& before,const Skills& chosen,
                                 int available) {
  if(available<0)return false;
  int allocated=0;
  for(size_t i=0;i<chosen.size();++i) {
    // Original DNAM Throwing slot is unused in Fallout 3.
    if(i==12){if(chosen[i]!=before[i])return false;continue;}
    if(chosen[i]<before[i]||chosen[i]>100)return false;
    allocated+=int(chosen[i])-int(before[i]);
  }
  return allocated==available;
}
} // namespace fo3levelup
