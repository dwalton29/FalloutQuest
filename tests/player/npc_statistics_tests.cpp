#include "weapons/fo3-weapon-data.h"
#include <cassert>
#include <iostream>
using namespace fo3weapon;
static Definitions Fixture() {
  Definitions d;auto& a=d.actors[1];a.health=10;a.endurance=4;a.flags=0x10;
  d.actors[2]=a;d.actors[2].flags|=1; // Sex is a Traits category.
  auto& l=d.levelledActors[3];l.valid=true;l.entries={{1,1,1},{1,1,2}};
  d.actors[4].templates=2;d.actors[4].templateId=3;return d;
}
int main() {
  auto d=Fixture();FinalizeStatistics(d);auto* resolved=ActorStatistics(d,4);
  assert(resolved&&resolved->health==10&&resolved->endurance==4);
  for(unsigned field=0;field<11;++field) {
    d=Fixture();auto& a=d.actors[2];auto& l=d.levelledActors[3];
    switch(field){case 0:++a.health;break;case 1:++a.endurance;break;case 2:++a.skills[0];break;
      case 3:a.flags^=0x10;break;case 4:++a.level;break;case 5:++a.minLevel;break;
      case 6:l.chanceNone=1;break;case 7:l.chanceGlobal=99;break;case 8:l.flags=2;break;
      case 9:l.entries[0].level=2;break;case 10:l.entries[0].count=2;break;}
    FinalizeStatistics(d);assert(!ActorStatistics(d,4));
  }
  d=Fixture();d.actors[1].templates=2;d.actors[1].templateId=3;
  FinalizeStatistics(d);assert(!ActorStatistics(d,4)); // Cycle through LVLN.
  d=Fixture();d.levelledActors[3].entries.push_back({1,1,99});
  FinalizeStatistics(d);assert(!ActorStatistics(d,4)); // Missing entry cannot be skipped.
  d=Fixture();d.levelledActors[3].valid=false;FinalizeStatistics(d);assert(!ActorStatistics(d,4));
  // Actual FO3 subrecord widths; LVLO padding is deliberately nonzero.
  std::vector<uint8_t> bytes;
  auto sub=[&](const char* tag,const std::vector<uint8_t>& b){bytes.insert(bytes.end(),tag,tag+4);bytes.push_back(uint8_t(b.size()));bytes.push_back(0);bytes.insert(bytes.end(),b.begin(),b.end());};
  sub("LVLD",{0});sub("LVLF",{0});sub("LVLO",{1,0,32,13,1,0,0,0,1,0,32,13});
  d=Fixture();Decode(d,"LVLN",3,bytes);assert(d.levelledActors.at(3).valid&&d.levelledActors.at(3).entries[0].actor==1);
  FinalizeStatistics(d);assert(ActorStatistics(d,4));
  sub("LVLO",{1,0});Decode(d,"LVLN",3,bytes);FinalizeStatistics(d);assert(!ActorStatistics(d,4));
  std::cout<<"Levelled statistics invariance, original layout, divergence and cycle rejection passed\n";
}
