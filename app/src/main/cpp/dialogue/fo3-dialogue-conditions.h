#pragma once
#include "player/fo3-player-state.h"
#include <functional>
namespace fo3dialogue {
struct ActorContext {
  uint32_t reference=0,base=0,cell=0,world=0,package=0;
  float x=0,y=0,z=0;
  bool combat=false,packageKnown=false;
};
struct Context {
  const fo3player::Player* player=nullptr;
  ActorContext speaker, target;
  bool talking=false;
};
using Diagnostic=std::function<void(const std::string&)>;
bool Compare(float,float,uint8_t);
bool Conditions(const std::vector<fo3pipdata::Condition>&,const Context&,std::string&);
uint64_t VariableKey(const fo3pipdata::Definitions&,uint32_t owner,uint32_t index,bool quest);
uint64_t VariableKey(const fo3pipdata::Definitions&,const std::string& name);
}
