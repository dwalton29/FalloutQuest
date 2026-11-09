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
inline uint64_t ReservationKey(uint32_t reference,size_t seat){return (uint64_t(reference)<<32)|seat;}
class Lease {
  std::shared_ptr<Reservations> pool_;uint64_t key_=0;uint32_t owner_=0;
public:
  Lease()=default;Lease(const Lease&)=delete;Lease& operator=(const Lease&)=delete;
  ~Lease(){Release();}
  bool Acquire(const std::shared_ptr<Reservations>& pool,uint32_t reference,size_t marker,uint32_t actor,size_t seat=0) {
    if(!pool||!reference||!actor||marker>=30)return false;
    const uint64_t key=ReservationKey(reference,seat);
    if(pool_&&pool_==pool&&key_==key&&owner_==actor)return true;
    if(pool->owners.count(key))return false;
    Release();pool_=pool;key_=key;owner_=actor;pool_->owners[key]=actor;return true;
  }
  void Release(){if(pool_){auto i=pool_->owners.find(key_);if(i!=pool_->owners.end()&&i->second==owner_)pool_->owners.erase(i);}pool_.reset();owner_=0;}
  bool Held()const{return bool(pool_);}
};
// Ephemeral leases are intentionally not persisted. Cell destruction releases
// them even when interrupted between entry and the first authored event.

inline fo3anim::Transform Root(const fo3anim::Clip& clip,double time,const fo3anim::Transform* retained=nullptr) {
  fo3anim::Skeleton s;fo3anim::Bone b;b.name=clip.accumulationRoot;s.bones.push_back(b);fo3anim::FinalizeSkeleton(s);fo3anim::Pose p;fo3anim::BindClip(s,clip,p);
  if(!fo3anim::Sample(s,clip,time,p,nullptr,fo3anim::RootPolicy::Furniture,retained)||p.local.empty())return {};
  return p.local[0];
}
// Entry/exit KFs retain their approach frame. Shared dynamic idle KFs do
// not: their NonAccum orientation must inherit the entry endpoint heading.
// Derive the planar change once per decoded program; never rotate the FRN
// marker or its accumulated translation (that would move bodies off the seat).
struct Program {
  fo3anim::Clip enter,loop,exit;
  float loopHeading=0;
  std::array<float,3> entryOrigin{};
  fo3anim::Matrix Frame(const fo3anim::Matrix& marker)const{
    auto frame=marker;
    const auto origin=fo3anim::Point(marker,{-entryOrigin[0],-entryOrigin[1],-entryOrigin[2]});
    std::copy(origin.begin(),origin.end(),frame.begin()+12);
    return frame;
  }
  bool Valid()const{return !enter.tracks.empty()&&!loop.tracks.empty()&&!exit.tracks.empty()&&enter.cycle==2&&exit.cycle==2;}
  void Prepare(){
    loopHeading=0;
    if(enter.accumulationRoot.empty()||enter.accumulationRoot!=loop.accumulationRoot||enter.frequency<=0)return;
    fo3anim::Skeleton skeleton;fo3anim::Bone bone;bone.name=enter.accumulationRoot+" NonAccum";skeleton.bones.push_back(bone);
    if(!fo3anim::FinalizeSkeleton(skeleton))return;
    fo3anim::Pose a,b;fo3anim::BindClip(skeleton,enter,a);fo3anim::BindClip(skeleton,loop,b);
    if(!fo3anim::Sample(skeleton,enter,(enter.stop-enter.start)/enter.frequency,a,nullptr,fo3anim::RootPolicy::Furniture,nullptr,fo3anim::Playback::Clamp)||
       !fo3anim::Sample(skeleton,loop,0,b,nullptr,fo3anim::RootPolicy::Furniture)||!(a.channels[0]&2)||!(b.channels[0]&2))return;
    fo3anim::Transform from,to;from.rotation=a.local[0].rotation;to.rotation=b.local[0].rotation;
    fo3anim::Matrix inverse;if(!fo3anim::Inverse(fo3anim::ToMatrix(to),inverse))return;
    const auto change=fo3anim::Multiply(fo3anim::ToMatrix(from),inverse);
    loopHeading=std::atan2(change[1],change[0]);
  }
  fo3anim::Transform LoopRoot(const fo3anim::Transform& retained)const{
    auto root=retained;const auto q=retained.rotation;const float c=std::cos(loopHeading/2),s=std::sin(loopHeading/2);
    root.rotation={q[0]*c-q[3]*s,q[1]*c+q[2]*s,q[2]*c-q[1]*s,q[3]*c+q[0]*s};return root;
  }
};
struct MarkerIdle { uint32_t form=0; fo3anim::Clip clip; };
struct Slot { bool idleMarker=false; float idleTimer=0; std::vector<MarkerIdle> idles; uint32_t reference=0,base=0;size_t index=0,seat=0;Marker marker;fo3pipdata::Placement placement;fo3anim::Matrix alignment=fo3anim::Identity();std::shared_ptr<const Program> program;bool sleep=false;std::shared_ptr<const Program> childProgram;};
struct Scene {std::vector<Slot> slots;std::shared_ptr<Reservations> reservations=std::make_shared<Reservations>();};
template<class Loader>
inline std::shared_ptr<Scene> BuildScene(const fo3pipdata::Definitions& d,uint32_t cell,uint32_t world,Loader&& load,bool includeEat=false,const std::unordered_set<uint32_t>* cells=nullptr,bool includeChildren=false) {
  auto scene=std::make_shared<Scene>();
  std::unordered_map<unsigned,std::shared_ptr<Program>> programs,childPrograms;
  auto clip=[&](const char* editor,fo3anim::Clip& out){const auto name=d.formNames.find(editor);if(name==d.formNames.end())return false;const auto model=d.idleModels.find(name->second);if(model==d.idleModels.end())return false;std::vector<uint8_t> bytes;return load(model->second,bytes)&&fo3anim::DecodeClip(bytes,out);};
  for(const auto& r:d.targets){const auto& p=r.second;if((cells&&!cells->count(p.cell))||p.world!=world||(!world&&p.cell!=cell)||(p.flags&0x820)||p.parent||std::fabs(p.rx)>1e-5f||std::fabs(p.ry)>1e-5f||std::fabs(p.scale-1)>1e-5f)continue;
    const auto f=d.furniture.find(p.base);if(f==d.furniture.end()||!f->second.valid||!std::isfinite(p.scale)||p.scale<=0)continue;
    std::vector<uint8_t> bytes;std::vector<Marker> markers;if(!load(f->second.model,bytes)||!DecodeMarkers(bytes,markers))continue;
    std::vector<std::pair<size_t,std::array<float,3>>> seats;
    for(size_t i=0;i<markers.size();++i){const auto& m=markers[i];const bool sleep=Permitted(f->second.markers,i,true),eat=includeEat&&Permitted(f->second.markers,i,false);if(!sleep&&!eat)continue;
      // Verified vanilla adult bed IDLE CTDA function 160 uses 1/2.
      // Floor-bed and scripted furniture require their own procedures.
      const char* enter=nullptr;const char* loop=nullptr;const char* exit=nullptr;
      if(sleep&&m.reference==1){enter="bedleft";loop="beddynamicidle";exit="bedleftgetup";}
      if(sleep&&m.reference==2){enter="bedright";loop="beddynamicidle";exit="bedrightgetup";}
      if(eat&&m.reference==11){enter="chairleftsit";loop="chairdynamicidle";exit="chairleftstand";}
      if(eat&&m.reference==12){enter="chairrightsit";loop="chairdynamicidle";exit="chairrightstand";}
      if(eat&&m.reference==14){enter="chairfrontsit";loop="chairdynamicidle";exit="chairfrontstand";}
      if(eat&&m.reference==15){enter="sitdownstool";loop="stooldynamicidle";exit="standupstoolnormal";}
      if(eat&&m.reference==17){enter="floorsitenter";loop="floorsitdynamicidle";exit="floorexit";}
      const auto loadProgram=[&](auto& cache,const char* begin,const char* idle,const char* end)->std::shared_ptr<const Program>{
        if(!begin)return {};
        auto& program=cache[m.reference];
        if(!program){
          auto ready=std::make_shared<Program>();
          if(!clip(begin,ready->enter)||!clip(idle,ready->loop)||!clip(end,ready->exit)||!ready->Valid())return {};
          ready->Prepare();
          // Stool KFs already place their first accumulation root at the
          // FRN approach in model space. Anchor that root to the marker once.
          if(m.reference==15)ready->entryOrigin=Root(ready->enter,0).translation;
          program=std::move(ready);
        }
        return program;
      };
      auto program=loadProgram(programs,enter,loop,exit);
      std::shared_ptr<const Program> child;
      if(eat&&includeChildren){
        // Select the KF by the raw NIF approach side. The IDLE condition's
        // furniture marker ID is not the raw side code for child chairs.
        const char* begin=nullptr;const char* end=nullptr;
        if(m.reference==11){begin="chairchildleftenter";end="chairchildleftexit";}
        if(m.reference==12){begin="chairchildrightenter";end="chairchildrightexit";}
        if(m.reference==14){begin="chairchildforwardenter";end="chairchildforwardexit";}
        child=loadProgram(childPrograms,begin,"dynamicidlechairchildsit",end);
      }
      if(!program&&!child)continue;
      Slot slot;slot.reference=r.first;slot.base=p.base;slot.index=i;slot.marker=m;slot.placement=p;slot.alignment=Alignment(p,m);
      slot.program=std::move(program);slot.childProgram=std::move(child);slot.sleep=sleep;
      if(eat&&markers.size()>1){
        // Several FRN markers can lead to one chair. Reserve the occupied
        // entry endpoint, so a booth with separated seats supports both actors.
        const auto& program=*(slot.program?slot.program:slot.childProgram);
        const auto& entry=program.enter;
        const auto end=fo3anim::Point(program.Frame(slot.alignment),Root(entry,(entry.stop-entry.start)/entry.frequency).translation);
        const auto shared=std::find_if(seats.begin(),seats.end(),[&](const auto& existing){
          return std::hypot(end[0]-existing.second[0],end[1]-existing.second[1])<35.f&&
                 std::abs(end[2]-existing.second[2])<20.f;
        });
        if(shared!=seats.end())slot.seat=shared->first;
        else {slot.seat=i;seats.emplace_back(i,end);}
      }
      scene->slots.push_back(std::move(slot));
    }
  }
  // Cache decoded marker clips once per resident scene. Conditions are checked
  // by the canonical dialogue evaluator at selection time, never bypassed.
  std::unordered_map<uint32_t,std::vector<MarkerIdle>> markerClips;
  for(const auto& r:d.targets){const auto& p=r.second;
    if((cells&&!cells->count(p.cell))||p.world!=world||(!world&&p.cell!=cell)||(p.flags&0x820)||p.parent||std::fabs(p.rx)>1e-5f||std::fabs(p.ry)>1e-5f||std::fabs(p.scale-1)>1e-5f)continue;
    const auto m=d.idleMarkers.find(p.base);if(m==d.idleMarkers.end()||!m->second.valid||m->second.flags)continue; // Sequenced/do-once markers need separate semantics.
    auto cached=markerClips.find(p.base);
    if(cached==markerClips.end()){
      std::vector<MarkerIdle> ready;
      for(auto id:m->second.animations){if(d.idleAnimationObjects.count(id))continue;auto model=d.idleModels.find(id);if(model==d.idleModels.end())continue;
        std::vector<uint8_t> bytes;MarkerIdle idle;idle.form=id;
        if(!load(model->second,bytes)||!fo3anim::DecodeClip(bytes,idle.clip)||idle.clip.tracks.empty()||idle.clip.frequency<=0||idle.clip.stop<=idle.clip.start||(idle.clip.stop-idle.clip.start)/idle.clip.frequency>60)continue;
        bool props=false;for(const auto& event:idle.clip.textKeys){auto key=event.text;for(auto& c:key)c=char(std::tolower((unsigned char)c));if(key.find("animobject")!=std::string::npos||key.find("attach")!=std::string::npos||key.find("detach")!=std::string::npos)props=true;}
        if(!props)ready.push_back(std::move(idle));
      }
      cached=markerClips.emplace(p.base,std::move(ready)).first;
    }
    if(cached->second.empty())continue;
    Slot slot;slot.reference=r.first;slot.base=p.base;slot.placement=p;slot.alignment=Placement(p);slot.idleMarker=true;slot.idleTimer=m->second.timer;slot.idles=cached->second;scene->slots.push_back(std::move(slot));
  }
  std::sort(scene->slots.begin(),scene->slots.end(),[](const Slot& a,const Slot& b){return a.reference<b.reference||(a.reference==b.reference&&a.index<b.index);});return scene;
}
enum class Phase {None,Approach,Enter,Loop,Exit};
struct State {
  // Asset scenes can belong to different preparation batches. Ownership must
  // instead belong to the complete resident population.
  std::shared_ptr<Reservations> residentReservations;
  std::shared_ptr<Reservations> Pool()const{return residentReservations?residentReservations:(scene?scene->reservations:nullptr);}
  std::shared_ptr<const Scene> scene;std::shared_ptr<Lease> lease;size_t slot=SIZE_MAX;Phase phase=Phase::None;double started=0;bool interrupted=false,child=false;
  fo3anim::Transform retainedRoot;std::array<float,3> anchor{};float yaw=0;uint32_t package=0;size_t idleIndex=SIZE_MAX;double until=0;
  bool Active()const{return phase!=Phase::None;}
  const Slot* Selected()const{return scene&&slot<scene->slots.size()?&scene->slots[slot]:nullptr;}
  const Program* SelectedProgram()const{
    const auto* selected=Selected();return selected?(child?selected->childProgram:selected->program).get():nullptr;
  }
  void RequestExit(){if(Active())interrupted=true;}
  void Clear(){interrupted=false;child=false;lease.reset();slot=SIZE_MAX;phase=Phase::None;retainedRoot={};package=0;idleIndex=SIZE_MAX;until=0;}
};
inline void JoinResidentActivities(State& arrival,const State& resident){arrival.residentReservations=resident.Pool();}
}
