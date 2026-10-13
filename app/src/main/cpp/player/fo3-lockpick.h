#pragma once
#include "fo3-player-state.h"
#include <algorithm>
#include <cmath>
namespace fo3lockpick {
// Transient mechanical state. It can commit only through Player's actual reference.
// Original engine uses a virtual XML detect-meter and six concentric stop bands.
// VR adaptation: 1280 logical pixels across the 180-degree pin travel.
struct Game {
  uint32_t reference=0;float pin=0,cylinder=0,durability=1,sweet=0;
  float width=0,concentric=200,breakSeconds=1;bool binding=false;
  bool Begin(const fo3player::Player& p,uint32_t ref,float uniform) {
    if(p.CanPickLock(ref)!=fo3player::PickAccess::Ready||!std::isfinite(uniform)||uniform<0||uniform>=1)return false;
    *this={};reference=ref;static const char* tiers[]={"VeryEasy","Easy","Average","Hard","VeryHard"};
    const int tier=p.LockRequirement(ref)/25;static const int defaults[]={400,200,100,50,25};
    const int lower=tier*25,upper=tier<4?lower+25:255;
    const float fraction=std::clamp((float(p.Snapshot().skills[4])-lower)/(2.f*(upper-lower)),0.f,1.f);
    auto interpolate=[&](const char* prefix,float fallback){const float min=p.LockSetting(std::string(prefix)+"Min"+tiers[tier],fallback);
      const float max=p.LockSetting(std::string(prefix)+"Max"+tiers[tier],fallback);return std::max(1.f,float(int(min+(max-min)*fraction)));};
    width=interpolate("iSweetSpotLength",float(defaults[tier]));concentric=interpolate("iConcentricLength",200);
    breakSeconds=std::max(.001f,p.LockSetting("fPickBreakSecs",1));
    const float margin=width*.5f+concentric;sweet=std::clamp(uniform*1280,margin,1280-margin)*180/1280-90;
    return true;
  }
  float StopAngle() const {
    const float distance=std::fabs(pin-sweet)*1280/180;
    const float half=width*.5f;
    if(distance<half)return 90;
    const int band=int((distance-half)/(concentric/5));
    return band<5?std::max(15.f,75.f-float(band)*15.f):0.f;
  }
  enum class Event { None, Bind, Break, Unlock };
  Event Step(float dt,float motion,float torque) {
    if(!reference||!std::isfinite(dt)||!std::isfinite(motion)||!std::isfinite(torque)||dt<=0)return Event::None;
    dt=std::min(dt,.05f);torque=std::clamp(torque,0.f,1.f);
    if(torque<=.01f){cylinder=std::max(0.f,cylinder-90*dt);binding=false;
      if(cylinder==0)pin=std::clamp(pin+std::clamp(motion,-1.f,1.f)*80*dt,-90.f,90.f);
      return Event::None;}
    const float stop=StopAngle();cylinder=std::min(stop,cylinder+90*dt*torque);
    if(cylinder>=90)return Event::Unlock;
    if(cylinder>=stop){const bool was=binding;binding=true;durability=std::max(0.f,durability-dt/breakSeconds);
      if(durability<=0)return Event::Break;
      return was?Event::None:Event::Bind;}
    binding=false;return Event::None;
  }
  void Replace(){durability=1;cylinder=0;binding=false;}
};
} // namespace fo3lockpick
