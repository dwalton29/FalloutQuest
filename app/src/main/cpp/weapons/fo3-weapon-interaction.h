#pragma once
#include "fo3-weapon-data.h"
#include "player/fo3-vr-body.h"
namespace fo3weapon {
using V=fo3vr::V;
using R=fo3vr::R;
struct Edge {
  bool latched=true,armed=false;
  bool Press(float value) {
    if(!std::isfinite(value)){Block();return false;}
    if(value<=.25f){latched=armed=false;return false;}
    if(value>=.65f&&!latched){latched=armed=true;return true;}
    return false;
  }
  void Block(){latched=true;armed=false;}
};
struct Inputs {
  Edge right,left,trigger;
  bool back=true,r=false,l=false,fire=false,eject=false,blocked=true;
  void Update(bool owned,float rg,float lg,float rt,bool b) {
    r=l=fire=eject=false;
    if(owned){right.Block();left.Block();trigger.Block();back=blocked=true;return;}
    r=right.Press(rg);l=left.Press(lg);fire=trigger.Press(rt);
    eject=b&&!back;back=b;blocked=false;
  }
};
struct Zone {
  V center{};R body=fo3vr::Identity();V radius{.16f,.20f,.16f};
  bool Contains(V p,bool inside=false)const {
    if(!fo3vr::Finite(p))return false;
    auto q=fo3vr::Rotate(fo3vr::Transpose(body),p-center);
    float h=inside?.04f:0.f;
    return q.x*q.x/((radius.x+h)*(radius.x+h))+q.y*q.y/((radius.y+h)*(radius.y+h))+q.z*q.z/((radius.z+h)*(radius.z+h))<=1;
  }
};
inline V ClosestSupport(V palm,V begin,V end) {
  auto d=end-begin;float n=fo3vr::Dot(d,d);
  return begin+d*std::clamp(n>1e-8f?fo3vr::Dot(palm-begin,d)/n:0.f,0.f,1.f);
}
// Right roll determines the transported plane. Left roll is not an input.
// Reversed/degenerate hands retain the one-hand pose rather than flipping.
inline R TwoHand(const R&one,V primary,V support,V axis,float weight=1) {
  auto direction=support-primary;
  if(fo3vr::Length(direction)<.08f)return one;
  auto oldAxis=fo3vr::Rotate(one,fo3vr::Unit(axis));
  auto newAxis=fo3vr::Unit(direction),cross=fo3vr::Cross(oldAxis,newAxis);
  float dot=std::clamp(fo3vr::Dot(oldAxis,newAxis),-1.f,1.f);
  if(fo3vr::Length(cross)<1e-5f||dot<-.85f)return one;
  return fo3vr::Multiply(fo3vr::Axis(cross,std::acos(dot)*std::clamp(weight,0.f,1.f)),one);
}
struct Trigger {
  double next=0;
  bool Ready(const Definition&w,const Inputs&input,float squeeze,double now) {
    if(input.blocked){next=now;return false;}
    bool request=w.Automatic()?input.trigger.armed&&squeeze>=.65f:input.fire;
    if(!request||!std::isfinite(now)||now<next)return false;
    float rate=w.Automatic()?w.rate:w.shotsPerSecond;
    float interval=std::max(w.delayMin,rate>0?1.f/rate:0.f);
    if(interval<=0)return false;
    next=now+interval;return true;
  }
};
}
