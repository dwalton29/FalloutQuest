#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
namespace fo3tags {
inline bool Skill(unsigned skill){return skill<14&&skill!=12;}
inline unsigned Count(uint16_t mask){unsigned n=0;while(mask){n+=mask&1u;mask>>=1;}return n;}
inline uint16_t Mask(const std::array<uint8_t,4>& slots){uint16_t mask=0;for(auto skill:slots)if(Skill(skill))mask|=uint16_t(1u<<skill);return mask;}
template<class State> bool Valid(const State& state){
  for(auto skill:state.taggedSkills)if(skill!=255&&!Skill(skill))return false;
  const auto mask=Mask(state.taggedSkills);
  for(unsigned i=0;i<14;++i)if(state.tagBonuses[i]>100||(!(mask&(1u<<i))&&state.tagBonuses[i]))return false;
  if(state.tagDraft&~uint16_t(0x2fff))return false; // Throwing is not a Fallout 3 selectable skill.
  return state.tagMenuOwner?(state.tagMenuTotal>=1&&state.tagMenuTotal<=4&&Count(state.tagDraft)<=state.tagMenuTotal):!state.tagMenuTotal&&!state.tagDraft;
}
template<class State> void Apply(State& state,const std::array<uint8_t,4>& slots,unsigned bonus){
  const auto mask=Mask(slots);
  for(unsigned i=0;i<14;++i){
    const unsigned base=state.skills[i]>state.tagBonuses[i]?state.skills[i]-state.tagBonuses[i]:0;
    state.tagBonuses[i]=uint8_t(mask&(1u<<i)?std::min(bonus,100u-base):0);
    state.skills[i]=uint8_t(base+state.tagBonuses[i]);
  }
  state.taggedSkills=slots;
}
}
