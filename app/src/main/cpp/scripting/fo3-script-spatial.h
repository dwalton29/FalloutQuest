#pragma once
#include "pipboy/fo3-pipboy-data.h"
#include "fo3-event-scripts.h"
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
  // Outside a sphere enclosing the rotated box no narrow-phase test is needed.
  const float extent=shape.bounds[0]+shape.bounds[1]+shape.bounds[2]+body+.03f;
  if(!shape.modelSpace&&(std::abs(x)>extent||std::abs(y)>extent||std::abs(z)>extent))return false;
  const auto rotate=[](float& a,float& b,float angle){const float c=std::cos(angle),s=std::sin(angle);
    const float old=a;a=c*a-s*b;b=s*old+c*b;};
  rotate(x,y,-p.rz);rotate(z,x,-p.ry);rotate(y,z,-p.rx);
  if(shape.modelSpace){
    const std::array<float,3> d{x-shape.center[0],y-shape.center[1],z-shape.center[2]};
    auto project=[&](size_t axis){const auto* n=&shape.axes[axis*3];return n[0]*d[0]+n[1]*d[1]+n[2]*d[2];};
    x=project(0);y=project(1);z=project(2);
  }
  // Trigger bounds and the placed floor can differ by a few float ULPs.
  constexpr float tolerance=.01f;
  const float dx=std::max(0.f,std::abs(x)-shape.bounds[0]-tolerance);
  const float dy=std::max(0.f,std::abs(y)-shape.bounds[1]-tolerance);
  const float dz=std::max(0.f,std::abs(z)-shape.bounds[2]-tolerance);
  return dx*dx+dy*dy+dz*dz<=body*body;
}
// Test the authored actor OBND against the trigger in game coordinates.
inline bool OverlapsActor(const fo3pipdata::Placement& p,const fo3pipdata::TriggerPrimitive& shape,
                          const std::array<float,3>& origin,float yaw,float scale,const std::array<int16_t,6>& bounds){
  using V=std::array<float,3>;
  if(!shape.valid||!std::isfinite(p.scale)||p.scale<=0||!std::isfinite(scale)||!std::isfinite(yaw))return false;
  for(float component:origin)if(!std::isfinite(component))return false;
  if(scale<=0||bounds[3]<=bounds[0]||bounds[4]<=bounds[1]||bounds[5]<=bounds[2])
    return Contains(p,shape,origin);
  auto dot=[](V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
  auto cross=[](V a,V b)->V{return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
  auto rotate=[](float& a,float& b,float angle){
    const float c=std::cos(angle),s=std::sin(angle),old=a;a=c*a-s*b;b=s*old+c*b;
  };
  auto toWorld=[&](V v){rotate(v[1],v[2],p.rx);rotate(v[2],v[0],p.ry);rotate(v[0],v[1],p.rz);return v;};
  const float c=std::cos(yaw),s=std::sin(yaw);
  std::array<V,3> actorAxes{{{c,s,0},{-s,c,0},{0,0,1}}},triggerAxes{};
  V delta{origin[0]-p.x,origin[1]-p.y,origin[2]-p.z},half{},extent{};
  for(size_t i=0;i<3;++i){
    const float center=(bounds[i]+bounds[i+3])*.5f*scale;
    half[i]=(bounds[i+3]-bounds[i])*.5f*scale;
    extent[i]=shape.bounds[i]*p.scale;
    for(size_t j=0;j<3;++j)delta[j]+=actorAxes[i][j]*center;
  }
  if(shape.modelSpace){
    const V center=toWorld(shape.center);
    for(size_t j=0;j<3;++j)delta[j]-=center[j]*p.scale;
  }
  if(shape.kind==2){
    float squared=0;
    for(size_t i=0;i<3;++i){const float outside=std::max(0.f,std::abs(dot(delta,actorAxes[i]))-half[i]);squared+=outside*outside;}
    return squared<=extent[0]*extent[0];
  }
  if(shape.kind!=1)return false;
  const float radius=half[0]+half[1]+half[2]+extent[0]+extent[1]+extent[2]+.01f;
  if(dot(delta,delta)>radius*radius)return false;
  for(size_t i=0;i<3;++i){
    V axis{};axis[i]=1;
    if(shape.modelSpace)std::copy_n(shape.axes.begin()+3*i,3,axis.begin());
    triggerAxes[i]=toWorld(axis);
  }
  // The separating axes for two boxes are their six face normals and the
  // nine cross products of edge directions (parallel edges contribute zero).
  auto separated=[&](V axis){
    float reach=0;
    for(size_t i=0;i<3;++i)reach+=half[i]*std::abs(dot(axis,actorAxes[i]))+extent[i]*std::abs(dot(axis,triggerAxes[i]));
    return std::abs(dot(delta,axis))>reach+.01f*std::sqrt(dot(axis,axis));
  };
  for(auto axis:actorAxes)if(separated(axis))return false;
  for(auto axis:triggerAxes)if(separated(axis))return false;
  for(auto a:actorAxes)for(auto b:triggerAxes)if(separated(cross(a,b)))return false;
  return true;
}
struct AnimationSequence {std::string name;double duration=0;bool loop=false;};
inline std::string OpenStateGroup(const std::vector<AnimationSequence>& clips,bool open){
  for(const char* name:{open?"open":"close",open?"forward":"backward"}){
    for(const auto& clip:clips)if(!clip.loop&&Lower(clip.name)==name)return name;
  }
  return {};
}
struct AnimationRun {std::string group;double started=0,duration=0;bool loop=false;};
inline bool Playing(const AnimationRun& run,double now){
  return !run.group.empty()&&std::isfinite(now)&&now>=run.started&&(run.loop||now-run.started<run.duration);
}
}
