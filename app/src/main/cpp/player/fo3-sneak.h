#pragma once
#include <algorithm>
#include <cmath>
namespace fo3sneak {
// Quest VR policy: floor-relative HMD calibration, hysteresis, OR-combined
// physical/button stance, and no second camera drop when already crouched.
struct Stance {
 float standing=0,offset=0,drop=0,pending=0;bool manual=false,physical=false,down=false,sneaking=false;
 void Calibrate(float height,bool pressed=false){
  if(!std::isfinite(height)||height<.7f||height>2.5f)return;
  standing=height;offset=drop=pending=0;manual=physical=sneaking=false;down=pressed;
 }
 void Step(float height,bool tracked,bool allowed,bool pressed,float dt,bool physicalEnabled=true,float threshold=.28f,float adjustment=0){
  const bool edge=pressed&&!down;down=pressed;
  if(!tracked||!std::isfinite(height)||height<.08f||height>3.f){pending=0;return;}
  if(!standing)standing=height;
  // The standing baseline changes only through calibration, never from a
  // tracking spike, stretching, loading, or opening a menu while crouched.
  if(allowed&&edge)manual=!manual;
  dt=std::isfinite(dt)?std::clamp(dt,0.f,.05f):0;
  threshold=std::clamp(threshold,.15f,.5f);
  const bool desired=physicalEnabled&&(physical?height<standing-threshold+.08f:height<standing-threshold);
  if(desired!=physical){pending+=dt;if(pending>=.12f){physical=desired;pending=0;}}
  else pending=0;
  if(!physicalEnabled){physical=false;pending=0;}
  const float target=manual?std::min(0.f,(standing+adjustment)*.65f-height-adjustment):0;
  if(manual&&offset<target)offset=target; // Physical lowering consumes the virtual drop immediately.
  offset+=std::clamp(target-offset,-2.f*dt,2.f*dt);
  drop=std::max(0.f,standing-height-offset);sneaking=manual||physical||offset<-.12f;
 }
};
}
