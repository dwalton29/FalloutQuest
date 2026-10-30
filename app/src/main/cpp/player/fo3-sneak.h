#pragma once
#include <algorithm>
#include <cmath>
namespace fo3sneak {
// Quest VR policy: floor-relative HMD calibration, hysteresis, OR-combined
// physical/button stance, and no second camera drop when already crouched.
struct Stance {
 float standing=0,offset=0,drop=0;bool manual=false,physical=false,down=false,sneaking=false;
 void Step(float height,bool tracked,bool allowed,bool pressed,float dt){
  const bool edge=pressed&&!down;down=pressed;
  if(!tracked||!std::isfinite(height)||height<.35f||height>2.6f)return;
  if(!standing)standing=height;
  if(allowed&&!manual)standing=std::max(standing,height);
  if(!allowed)return;
  if(edge)manual=!manual;
  if(!physical&&height<standing-.22f)physical=true;
  else if(physical&&height>standing-.14f)physical=false;
  const float target=manual?std::min(0.f,standing*.65f-height):0;
  if(manual&&offset<target)offset=target; // Physical lowering consumes the virtual drop immediately.
  offset+=std::clamp(target-offset,-2.f*std::clamp(dt,0.f,.05f),2.f*std::clamp(dt,0.f,.05f));
  drop=std::max(0.f,standing-height-offset);sneaking=manual||physical;
 }
};
}
