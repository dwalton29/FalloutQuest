#pragma once
#include "fo3-actor-animation.h"
#include "../pipboy/fo3-pipboy-data.h"
#include <cmath>
#include <memory>
#include <unordered_map>
namespace fo3furniture {
struct Marker { std::array<float,3> position{}; uint16_t orientation=0; uint8_t reference=0,reference2=0; };
bool DecodeMarkers(const std::vector<uint8_t>&,std::vector<Marker>&);
inline fo3anim::Matrix Placement(const fo3pipdata::Placement& p) {
  // Same Rz*Ry*Rx convention as ApplyEsmRotation; FRN angles are clockwise
  // milliradians (the original 1570/3141/4712 values), unlike REFR radians.
  fo3anim::Transform t;t.translation={p.x,p.y,p.z};t.scale=p.scale;
  const float cx=std::cos(p.rx/2),sx=std::sin(p.rx/2),cy=std::cos(p.ry/2),sy=std::sin(p.ry/2),cz=std::cos(p.rz/2),sz=std::sin(p.rz/2);
  t.rotation={cz*cy*cx+sz*sy*sx,cz*cy*sx-sz*sy*cx,cz*sy*cx+sz*cy*sx,sz*cy*cx-cz*sy*sx};return fo3anim::ToMatrix(t);
}
inline fo3anim::Matrix Alignment(const fo3pipdata::Placement& p,const Marker& m) {
  fo3anim::Transform t;t.translation=m.position;const float angle=-float(m.orientation)/1000.f;
  t.rotation={std::cos(angle/2),0,0,std::sin(angle/2)};
  return fo3anim::Multiply(Placement(p),fo3anim::ToMatrix(t));
}
inline bool Permitted(uint32_t flags,size_t index,bool sleep) {
  return index<30&&(flags&(1u<<index))&&(flags&(sleep?0x80000000u:0x40000000u));
}
struct Reservations { std::unordered_map<uint64_t,uint32_t> owners; };
class Lease {
  std::shared_ptr<Reservations> pool_;uint64_t key_=0;uint32_t owner_=0;
public:
  Lease()=default;Lease(const Lease&)=delete;Lease& operator=(const Lease&)=delete;
  ~Lease(){Release();}
  bool Acquire(const std::shared_ptr<Reservations>& pool,uint32_t reference,size_t marker,uint32_t actor) {
    if(!pool||!reference||!actor||marker>=30)return false;
    const uint64_t key=(uint64_t(reference)<<32)|marker;
    if(pool_&&pool_==pool&&key_==key&&owner_==actor)return true;
    if(pool->owners.count(key))return false;
    Release();pool_=pool;key_=key;owner_=actor;pool_->owners[key]=actor;return true;
  }
  void Release(){if(pool_){auto i=pool_->owners.find(key_);if(i!=pool_->owners.end()&&i->second==owner_)pool_->owners.erase(i);}pool_.reset();owner_=0;}
  bool Held()const{return bool(pool_);}
};
// Ephemeral leases are intentionally not persisted. Cell destruction releases
// them even when interrupted between entry and the first authored event.
struct Program { fo3anim::Clip enter,loop,exit; bool Valid()const{return !enter.tracks.empty()&&!loop.tracks.empty()&&!exit.tracks.empty()&&enter.cycle==2&&exit.cycle==2;} };
struct Slot {uint32_t reference=0,base=0;size_t index=0;Marker marker;fo3pipdata::Placement placement;fo3anim::Matrix alignment=fo3anim::Identity();std::shared_ptr<const Program> program;bool sleep=false;};
struct Scene {std::vector<Slot> slots;std::shared_ptr<Reservations> reservations=std::make_shared<Reservations>();};
enum class Phase {None,Approach,Enter,Loop,Exit};
struct State {
  std::shared_ptr<const Scene> scene;std::shared_ptr<Lease> lease;size_t slot=SIZE_MAX;Phase phase=Phase::None;double started=0;
  fo3anim::Transform retainedRoot;std::array<float,3> anchor{};float yaw=0;uint32_t package=0;
  bool Active()const{return phase!=Phase::None;}
  const Slot* Selected()const{return scene&&slot<scene->slots.size()?&scene->slots[slot]:nullptr;}
  void Clear(){lease.reset();slot=SIZE_MAX;phase=Phase::None;retainedRoot={};package=0;}
};
}
