#pragma once
#include "player/fo3-player-state.h"
#include <functional>
namespace fo3dialogue {
struct CommandFormCapture;
struct ActorContext {
  uint32_t reference=0,base=0,cell=0,world=0,package=0;
  float x=0,y=0,z=0;
  bool combat=false,packageKnown=false;
  uint32_t proxy=0; // Physical talking activator; reference/base retain the actor identity.
};
struct Context {
  const fo3player::Player* player=nullptr;
  ActorContext speaker, target;
  bool talking=false;
  const std::unordered_map<uint64_t,float>* activationVariables=nullptr;
  float randomPercent=-1; // Optional bounded caller sample for original CTDA 77.
};
using Diagnostic=std::function<void(const std::string&)>;
bool Compare(float,float,uint8_t);
bool DeadCount(const fo3player::Player&,uint32_t base,float& value,std::string& error);
bool Conditions(const std::vector<fo3pipdata::Condition>&,const Context&,std::string&);
uint64_t VariableKey(const fo3pipdata::Definitions&,uint32_t owner,uint32_t index,bool quest,CommandFormCapture* capture=nullptr);
uint64_t VariableKey(const fo3pipdata::Definitions&,const std::string& name,CommandFormCapture* capture=nullptr);
}
