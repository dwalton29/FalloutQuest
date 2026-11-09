#pragma once
#include "pipboy/fo3-pipboy-data.h"
#include <algorithm>
#include <cmath>
namespace fo3script {
// Inverse of the renderer's original ESM X/Y/Z placement rotations. This is
// a logical player-position test against authored XPRM, not a distance gate.
inline bool Contains(const fo3pipdata::Placement& p,const fo3pipdata::TriggerPrimitive& shape,
                     const std::array<float,3>& point,float bodyRadius=0){
  if(!shape.valid||!std::isfinite(p.scale)||p.scale<=0)return false;
  float x=(point[0]-p.x)/p.scale,y=(point[1]-p.y)/p.scale,z=(point[2]-p.z)/p.scale;
  if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return false;
  const float body=bodyRadius/p.scale;
  if(shape.kind==2){const float radius=shape.bounds[0]+body;return x*x+y*y+z*z<=radius*radius;}
  if(shape.kind!=1)return false;
  const auto rotate=[](float& a,float& b,float angle){const float c=std::cos(angle),s=std::sin(angle);
    const float old=a;a=c*a-s*b;b=s*old+c*b;};
  rotate(x,y,-p.rz);rotate(z,x,-p.ry);rotate(y,z,-p.rx);
  // Trigger bounds and the placed floor can differ by a few float ULPs.
  constexpr float tolerance=.01f;
  const float dx=std::max(0.f,std::abs(x)-shape.bounds[0]-tolerance);
  const float dy=std::max(0.f,std::abs(y)-shape.bounds[1]-tolerance);
  const float dz=std::max(0.f,std::abs(z)-shape.bounds[2]-tolerance);
  return dx*dx+dy*dy+dz*dz<=body*body;
}
struct AnimationSequence {std::string name;double duration=0;bool loop=false;};
struct AnimationRun {std::string group;double started=0,duration=0;bool loop=false;};
inline bool Playing(const AnimationRun& run,double now){
  return !run.group.empty()&&std::isfinite(now)&&now>=run.started&&(run.loop||now-run.started<run.duration);
}
}
