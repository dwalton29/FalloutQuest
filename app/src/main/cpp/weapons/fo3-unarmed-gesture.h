#pragma once
#include "player/fo3-vr-body.h"
#include <optional>

namespace fo3unarmed {
// Quest gesture thresholds in tracking-space metres and seconds.
// A successful strike requires a pullback or an opened fist before it can hit again.
class Hand {
  fo3vr::V lastPosition{},contact{},strikeDirection{};
  double lastTime=-1,impactTime=-1;
  bool closed=false,spent=false;

public:
  void Reset(){*this={};}
  bool Closed()const{return closed;}

  std::optional<fo3vr::V> Advance(double now,float grip,fo3vr::V position){
    if(!std::isfinite(now)||!std::isfinite(grip)||!fo3vr::Finite(position)){
      Reset();
      return {};
    }
    const auto motion=position-lastPosition;
    const double elapsed=now-lastTime;
    const bool continuing=lastTime>=0&&closed;
    lastTime=now;lastPosition=position;
    closed=grip>=(closed?.55f:.75f);
    if(!closed){spent=false;return {};}
    if(!continuing||elapsed<=0||elapsed>.10)return {};
    const float distance=fo3vr::Length(motion);
    const float speed=distance/float(elapsed);
    // Tracking discontinuities must never become strikes or rearm a spent fist.
    if(distance>.40f||speed>12.f)return {};
    if(spent){
      if(fo3vr::Dot(position-contact,strikeDirection)<=-.08f)spent=false;
      return {};
    }
    if((impactTime>=0&&now-impactTime<.18)||speed<1.55f)return {};
    return motion;
  }

  void Impact(fo3vr::V motion){
    contact=lastPosition;
    strikeDirection=fo3vr::Unit(motion);
    impactTime=lastTime;
    spent=true;
  }
};
}
