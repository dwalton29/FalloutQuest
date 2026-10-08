#pragma once
// HUDMainMenu HitPoints / EnemyHealth: VR-local reveal state only.
// Fallout3.ini [GamePlay] fHealthBarEmittanceTime=1.5,
// fHealthBarEmittanceFadeTime=0.5. The canonical Player owns all health.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace fo3health {
constexpr double kHoldSeconds=1.5;
constexpr double kFadeSeconds=0.5;
struct Bar {
  float current=0,maximum=0;
  double lastDamage=-1;
  bool valid=false;
  void Observe(float health,float maxHealth,double now) {
    if(!std::isfinite(health)||!std::isfinite(maxHealth)||maxHealth<=0||!std::isfinite(now))return;
    const float next=std::clamp(health,0.f,maxHealth);
    if(valid&&next<current-.001f)lastDamage=now;
    current=next;maximum=maxHealth;valid=true;
  }
  float Fraction()const{return valid&&maximum>0?std::clamp(current/maximum,0.f,1.f):0.f;}
  float Alpha(double now)const {
    if(!valid||lastDamage<0||!std::isfinite(now)||now<lastDamage)return 0;
    const double elapsed=now-lastDamage;
    if(elapsed<=kHoldSeconds)return 1;
    return float(std::clamp(1.-(elapsed-kHoldSeconds)/kFadeSeconds,0.,1.));
  }
};
struct Tracker {
  Bar player;
  std::unordered_map<uint32_t,Bar> actors;
  void Clear(){player={};actors.clear();}
  void ObserveActor(uint32_t id,float health,float maximum,double now){
    if(id)actors[id].Observe(health,maximum,now);
  }
  const Bar* Actor(uint32_t id)const {
    auto it=actors.find(id);return it==actors.end()?nullptr:&it->second;
  }
};
} // namespace fo3health
