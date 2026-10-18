#include "player/fo3-levelup-rules.h"
#include <cassert>
#include <iostream>

int main() {
  using namespace fo3levelup;
  Special special{};special.fill(5);
  Skills skills{};skills.fill(25);
  const std::unordered_map<uint32_t,uint8_t> none;
  fo3pipdata::Perk educated;
  educated.name="Educated";educated.playable=true;
  educated.minLevel=4;educated.ranks=1;
  fo3pipdata::Condition intelligence;
  intelligence.function=495;intelligence.a=9; // INT AV
  intelligence.flags=0x60;intelligence.value=4;
  educated.prerequisites.push_back(intelligence);
  assert(!PerkEligible(1,educated,3,special,skills,false,none));
  assert(PerkEligible(1,educated,4,special,skills,false,none));
  special[4]=3;
  assert(!PerkEligible(1,educated,4,special,skills,false,none));
  special[4]=5;
  const std::unordered_map<uint32_t,uint8_t> owned{{1,1}};
  assert(!PerkEligible(1,educated,4,special,skills,false,owned));
  fo3pipdata::Perk female;
  female.playable=true;female.minLevel=2;female.ranks=1;
  fo3pipdata::Condition isFemale;
  isFemale.function=70;isFemale.a=1;isFemale.value=1;
  female.prerequisites.push_back(isFemale);
  assert(!PerkEligible(2,female,2,special,skills,false,none));
  assert(PerkEligible(2,female,2,special,skills,true,none));
  fo3pipdata::Perk here;
  here.playable=true;here.minLevel=10;here.ranks=1;
  fo3pipdata::Condition notMax;
  notMax.function=80;notMax.value=20;notMax.flags=0x80;
  here.prerequisites.push_back(notMax);
  assert(PerkEligible(3,here,19,special,skills,false,none));
  assert(!PerkEligible(3,here,20,special,skills,false,none));
  here.prerequisites[0].function=999;
  assert(!PerkEligible(3,here,19,special,skills,false,none));
  here.prerequisites.clear();here.hidden=true;
  assert(!PerkEligible(3,here,19,special,skills,false,none));
  here.hidden=false;here.playable=false;
  assert(!PerkEligible(3,here,19,special,skills,false,none));

  assert(SkillBudget(special,11,1,none,{})==15);
  special[4]=10;
  assert(SkillBudget(special,11,1,none,{})==20);
  special[4]=5;
  fo3pipdata::Perk edu;
  edu.playable=true;
  fo3pipdata::PerkEffect effect;
  effect.type=2;effect.entryPoint=10;effect.function=2;
  effect.parameterType=1;effect.parameters={0,0,0x40,0x40}; // 3.0f
  edu.effects.push_back(effect);
  assert(SkillBudget(special,11,1,{{4,1}},{{4,edu}})==18);
  edu.effects[0].scripted=true;
  assert(SkillBudget(special,11,1,{{4,1}},{{4,edu}})==-1);
  Skills next=skills;next[0]+=10;next[1]+=5;
  assert(SkillAllocationValid(skills,next,15));
  next[12]+=1;
  assert(!SkillAllocationValid(skills,next,15));
  next=skills;next[0]+=10;
  assert(!SkillAllocationValid(skills,next,15));
  next=skills;next[0]-=1;next[1]+=16;
  assert(!SkillAllocationValid(skills,next,15));
  std::cout<<"Original perk prerequisites and skill budget tests passed\n";
}
