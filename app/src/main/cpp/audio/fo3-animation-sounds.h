#pragma once
#include "npc/fo3-actor-animation.h"
#include "pipboy/fo3-pipboy-data.h"
#include <algorithm>
#include <cmath>
#include <cctype>
namespace fo3animsound {
inline uint32_t Resolve(const fo3pipdata::Definitions& d,std::string key){
  for(auto& c:key)c=char(std::tolower(static_cast<unsigned char>(c)));
  if(key.rfind("sound:",0)==0)key.erase(0,6);
  const auto first=key.find_first_not_of(" \t\r\n");if(first==std::string::npos)return 0;
  key=key.substr(first,key.find_last_not_of(" \t\r\n")-first+1);
  const auto name=d.formNames.find(key);
  return name!=d.formNames.end()&&d.sounds.count(name->second)?name->second:0;
}
// Cross the clip's original timeline once. Retained pose publication cannot
// replay sounds. Loop/reverse clips use the same cycle semantics as Sample().
template<class Emit> void Advance(const fo3pipdata::Definitions& d,const fo3anim::Clip& clip,double previous,double elapsed,Emit emit){
  if(!std::isfinite(elapsed)||elapsed<0||clip.frequency<=0||elapsed<previous)return;
  const double duration=clip.stop-clip.start;if(duration<=0)return;
  const double a=previous<0?-1:previous*clip.frequency,b=elapsed*clip.frequency;
  const bool loop=clip.cycle==0,reverse=clip.cycle==1;const double period=duration*(reverse?2:1);
  const auto begin=(loop||reverse)?std::max(0.,std::floor(std::max(0.,a)/period)):0.;
  const auto end=(loop||reverse)?std::floor(b/period):0.;
  for(double cycle=std::max(begin,end-32);cycle<=end;++cycle)for(const auto& key:clip.textKeys){
    const auto id=Resolve(d,key.text);if(!id||key.time<clip.start||key.time>clip.stop)continue;
    const double time=cycle*period+key.time-clip.start;
    if(time>a&&time<=b)emit(id);
    if(reverse&&key.time>clip.start&&key.time<clip.stop){const double back=cycle*period+2*duration-(key.time-clip.start);if(back>a&&back<=b)emit(id);}
  }
}
}
