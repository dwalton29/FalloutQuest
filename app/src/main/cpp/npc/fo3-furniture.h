#pragma once
#include "fo3-actor-animation.h"
#include "../pipboy/fo3-pipboy-data.h"
#include <cmath>
#include <algorithm>
#include <memory>
#include <unordered_map>
namespace fo3furniture {
struct Marker { std::array<float,3> position{}; uint16_t orientation=0; uint8_t reference=0,reference2=0; };
bool DecodeMarkers(const std::vector<uint8_t>&,std::vector<Marker>&);
inline fo3anim::Matrix Placement(const fo3pipdata::Placement& p) {
  // Raw Bethesda REFR rotation is the inverse of Rz*Ry*Rx, matching
  // Q230ConvertBethesdaRotation. FRN angles are clockwise
  // milliradians (the original 1570/3141/4712 values), unlike REFR radians.
  fo3anim::Transform t;t.translation={p.x,p.y,p.z};t.scale=p.scale;
  const float cx=std::cos(p.rx/2),sx=std::sin(p.rx/2),cy=std::cos(p.ry/2),sy=std::sin(p.ry/2),cz=std::cos(p.rz/2),sz=std::sin(p.rz/2);
  t.rotation={cz*cy*cx+sz*sy*sx,cz*cy*sx-sz*sy*cx,cz*sy*cx+sz*cy*sx,sz*cy*cx-cz*sy*sx};for(int i=1;i<4;++i)t.rotation[i]=-t.rotation[i];return fo3anim::ToMatrix(t);
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
    const uint64_t key=uint64_t(reference)<<32; // Side markers are alternative entries to one object.
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
inline fo3anim::Transform Root(const fo3anim::Clip& clip,double time,const fo3anim::Transform* retained=nullptr) {
  fo3anim::Skeleton s;fo3anim::Bone b;b.name=clip.accumulationRoot;s.bones.push_back(b);fo3anim::FinalizeSkeleton(s);fo3anim::Pose p;fo3anim::BindClip(s,clip,p);
  if(!fo3anim::Sample(s,clip,time,p,nullptr,fo3anim::RootPolicy::Furniture,retained)||p.local.empty())return {};
  return p.local[0];
}
struct Slot {uint32_t reference=0,base=0;size_t index=0;Marker marker;fo3pipdata::Placement placement;fo3anim::Matrix alignment=fo3anim::Identity();std::shared_ptr<const Program> program;bool sleep=false;};
struct Scene {std::vector<Slot> slots;std::shared_ptr<Reservations> reservations=std::make_shared<Reservations>();};
template<class Loader>
inline std::shared_ptr<Scene> BuildScene(const fo3pipdata::Definitions& d,uint32_t cell,uint32_t world,Loader&& load,bool includeEat=false) {
  auto scene=std::make_shared<Scene>();
  std::unordered_map<unsigned,std::shared_ptr<Program>> programs;
  auto clip=[&](const char* editor,fo3anim::Clip& out){const auto name=d.formNames.find(editor);if(name==d.formNames.end())return false;const auto model=d.idleModels.find(name->second);if(model==d.idleModels.end())return false;std::vector<uint8_t> bytes;return load(model->second,bytes)&&fo3anim::DecodeClip(bytes,out);};
  for(const auto& r:d.targets){const auto& p=r.second;if(p.world!=world||(!world&&p.cell!=cell)||(p.flags&0x820)||p.parent||std::fabs(p.rx)>1e-5f||std::fabs(p.ry)>1e-5f||std::fabs(p.scale-1)>1e-5f)continue;
    const auto f=d.furniture.find(p.base);if(f==d.furniture.end()||!f->second.valid||!std::isfinite(p.scale)||p.scale<=0)continue;
    std::vector<uint8_t> bytes;std::vector<Marker> markers;if(!load(f->second.model,bytes)||!DecodeMarkers(bytes,markers))continue;
    for(size_t i=0;i<markers.size();++i){const auto& m=markers[i];const bool sleep=Permitted(f->second.markers,i,true),eat=includeEat&&Permitted(f->second.markers,i,false);if(!sleep&&!eat)continue;
      // Verified vanilla adult bed IDLE CTDA function 160 uses 1/2.
      // Child, floor-bed and scripted furniture are deliberately not substituted.
      const char* enter=nullptr;const char* loop=nullptr;const char* exit=nullptr;
      if(sleep&&m.reference==1){enter="bedleft";loop="beddynamicidle";exit="bedleftgetup";}
      if(sleep&&m.reference==2){enter="bedright";loop="beddynamicidle";exit="bedrightgetup";}
      if(eat&&m.reference==11){enter="chairleftsit";loop="chairdynamicidle";exit="chairleftstand";}
      if(eat&&m.reference==12){enter="chairrightsit";loop="chairdynamicidle";exit="chairrightstand";}
      if(eat&&m.reference==14){enter="chairfrontsit";loop="chairdynamicidle";exit="chairfrontstand";}
      if(!enter)continue;
      auto& program=programs[m.reference];if(!program){auto ready=std::make_shared<Program>();if(!clip(enter,ready->enter)||!clip(loop,ready->loop)||!clip(exit,ready->exit)||!ready->Valid())continue;program=std::move(ready);}
      Slot slot;slot.reference=r.first;slot.base=p.base;slot.index=i;slot.marker=m;slot.placement=p;slot.alignment=Alignment(p,m);slot.program=program;slot.sleep=sleep;scene->slots.push_back(std::move(slot));
    }
  }
  std::sort(scene->slots.begin(),scene->slots.end(),[](const Slot& a,const Slot& b){return a.reference<b.reference||(a.reference==b.reference&&a.index<b.index);});return scene;
}
enum class Phase {None,Approach,Enter,Loop,Exit};
struct State {
  std::shared_ptr<const Scene> scene;std::shared_ptr<Lease> lease;size_t slot=SIZE_MAX;Phase phase=Phase::None;double started=0;bool interrupted=false;
  fo3anim::Transform retainedRoot;std::array<float,3> anchor{};float yaw=0;uint32_t package=0;
  bool Active()const{return phase!=Phase::None;}
  const Slot* Selected()const{return scene&&slot<scene->slots.size()?&scene->slots[slot]:nullptr;}
  void RequestExit(){if(Active())interrupted=true;}
  void Clear(){interrupted=false;lease.reset();slot=SIZE_MAX;phase=Phase::None;retainedRoot={};package=0;}
};
}
