#pragma once
#include "fo3-dialogue-results.h"
#include <random>
namespace fo3dialogue {
enum class Phase { Inactive, Speaking, Choices };
struct Choice {uint32_t topic=0,info=0;std::string text;};
struct Session {
  Phase phase=Phase::Inactive;
  uint32_t actor=0,base=0,topic=0,info=0,voice=0;
  size_t response=0,selected=0;
  uint32_t audioToken=0;
  float startDistance=0;
  std::string endReason;
  float startDistanceGameUnits=0;
  std::string interruption;
  std::vector<Choice> choices;
  Diagnostic diagnostic;
  mutable std::mt19937 random{std::random_device{}()};
  bool Active()const{return phase!=Phase::Inactive;}
  const fo3pipdata::Info* Current(const fo3pipdata::Definitions&)const;
  const fo3pipdata::Info* Resolve(uint32_t,const Context&,bool draw=false)const;
  bool CanStart(const Context&)const;
  bool Start(const Context&,fo3player::Player&);
  bool Choose(size_t,const Context&,fo3player::Player&);
  bool AudioDone(uint32_t,bool,const Context&,fo3player::Player&);
  std::string Audio(const fo3pipdata::Definitions&)const;
  std::string Subtitle(const fo3pipdata::Definitions&)const;
  void End(const std::string&);
private:
  bool Enter(const fo3pipdata::Info&,const Context&,fo3player::Player&);
  bool Eligible(const fo3pipdata::Info&,const Context&)const;
  void BuildChoices(const Context&);
};
}
