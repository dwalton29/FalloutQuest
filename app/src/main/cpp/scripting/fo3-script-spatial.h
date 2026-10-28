#pragma once
#include "pipboy/fo3-pipboy-data.h"
#include <cmath>
namespace fo3script {
// Inverse of the renderer's original ESM X/Y/Z placement rotations. This is
// a logical player-position test against authored XPRM, not a distance gate.
inline bool Contains(const fo3pipdata::Placement& p,const fo3pipdata::TriggerPrimitive& shape,
                     const std::array<float,3>& point){
  if(!shape.valid||!std::isfinite(p.scale)||p.scale<=0)return false;
  float x=(point[0]-p.x)/p.scale,y=(point[1]-p.y)/p.scale,z=(point[2]-p.z)/p.scale;
  if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return false;
  if(shape.kind==2){const float radius=shape.bounds[0];return x*x+y*y+z*z<=radius*radius;}
  if(shape.kind!=1)return false;
  const auto rotate=[](float& a,float& b,float angle){const float c=std::cos(angle),s=std::sin(angle);
    const float old=a;a=c*a-s*b;b=s*old+c*b;};
  rotate(x,y,-p.rz);rotate(z,x,-p.ry);rotate(y,z,-p.rx);
  return std::abs(x)<=shape.bounds[0]&&std::abs(y)<=shape.bounds[1]&&std::abs(z)<=shape.bounds[2];
}
struct AnimationSequence {std::string name;double duration=0;bool loop=false;};
struct AnimationRun {std::string group;double started=0,duration=0;bool loop=false;};
inline bool Playing(const AnimationRun& run,double now){
  return !run.group.empty()&&std::isfinite(now)&&now>=run.started&&(run.loop||now-run.started<run.duration);
}
}
