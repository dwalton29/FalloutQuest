#include "fo3-unloaded-ai.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace fo3unloaded {
namespace {
int Minute(float hour) {
  if(!std::isfinite(hour))return -1;
  hour=std::fmod(hour,24.f);if(hour<0)hour+=24.f;
  return std::min(1439,int(hour*60.f));
}
bool Locatable(const fo3pipdata::Placement& t) {
  return (t.cell||t.world)&&!(t.flags&0x820u)&&!t.parent&&
    std::isfinite(t.x)&&std::isfinite(t.y)&&std::isfinite(t.z);
}
struct Candidate {uint32_t id=0,targetCell=0,targetWorld=0;};
Candidate Select(uint32_t id,uint32_t base,const fo3player::ActorState& state,float hour,
                 const fo3pipdata::Definitions& defs,const Callbacks& callbacks) {
  if(!callbacks.packages||!callbacks.eligible)return {};
  const auto authored=callbacks.packages(base);
  for(uint32_t packageId:authored){
    const auto it=defs.packages.find(packageId);
    if(it==defs.packages.end())continue;
    const auto& p=it->second;
    if(!SupportedLocalProcedure(p)||!ScheduleActive(p.schedule,hour))continue;
    if(!callbacks.eligible(id,base,state,p,hour))continue;
    const auto target=defs.targets.find(p.location.value);
    if(target==defs.targets.end()||!Locatable(target->second))continue;
    return {packageId,target->second.cell,target->second.world};
  }
  return {};
}
const fo3xtel::DoorLink* FirstDoor(const fo3xtel::Index& graph,
    uint32_t start,uint32_t destination,uint32_t actor,const Callbacks& cb,
    const fo3pipdata::Definitions& definitions) {
  if(!start||!destination||!cb.canUseDoor)return nullptr;
  std::queue<uint32_t> open;
  std::unordered_map<uint32_t,uint32_t> via;
  via.emplace(start,0);open.push(start);
  while(!open.empty()&&!via.count(destination)){
    const uint32_t cell=open.front();open.pop();
    const auto* exits=graph.Outgoing(cell);if(!exits)continue;
    for(const auto door:*exits){
      const auto* link=graph.Find(door);
      if(!link||!link->destinationCell||
          (link->sourceRecordFlags&0x820u)||(link->destinationRecordFlags&0x820u))continue;
      const auto ref=definitions.targets.find(door);
      if(ref==definitions.targets.end()||!Locatable(ref->second)||
         !cb.canUseDoor(actor,door))continue;
      if(via.emplace(link->destinationCell,door).second)open.push(link->destinationCell);
    }
  }
  if(!via.count(destination))return nullptr;
  uint32_t cell=destination,first=0;
  while(cell!=start) {
    const uint32_t door=via.at(cell);
    const auto* edge=graph.Find(door);
    if(!edge)return nullptr;
    first=door;cell=edge->sourceCell;
  }
  return graph.Find(first);
}
}
bool ScheduleActive(const fo3pipdata::PackageSchedule& s,float hour) {
  if(!s.valid)return true;
  // Without a date/month/day-of-week clock, don't claim dated packages fire.
  if(s.month!=-1||s.weekday!=-1||s.date!=0)return false;
  if(s.hour<0)return true;
  if(s.duration<=0)return false;
  hour=std::fmod(hour,24.f);if(hour<0)hour+=24.f;
  const float end=float(s.hour)+float(s.duration);
  if(end<=24.f)return hour>=s.hour&&hour<end;
  return hour>=s.hour||hour<end-24.f;
}
bool SupportedLocalProcedure(const fo3pipdata::PackageDefinition& p) {
  // Pure location intents only; no result scripts, NPC interaction, uncertain
  // idle/procedure actions or unsupported PLDT mode.
  switch(p.type){case 3:case 4:case 5:case 6:case 12:case 14:break;default:return false;}
  return p.combatStyleValid&&!p.scripted&&!p.procedureActions&&
         p.location.valid&&p.location.type==0&&!p.location2.valid;
}
void Scheduler::Reset(){minute_=-1;previousMinute_=-1;cursor_=0;pending_.clear();lastTransferredMinute_.clear();}
Report Scheduler::Tick(float hour,uint32_t residentCell,uint32_t residentWorld,
  const std::unordered_map<uint32_t,fo3player::ActorState>& tracked,
  const fo3pipdata::Definitions& defs,const fo3xtel::Index& graph,
  const Callbacks& cb) {
  Report out;
  const int now=Minute(hour);
  if(now<0||!cb.persist||!cb.alive)return out;
  if(minute_<0){minute_=now;previousMinute_=now;return out;}
  if(pending_.empty()&&now==minute_)return out;
  if(pending_.empty()) {
    previousMinute_=minute_;minute_=now;cursor_=0;
    pending_.reserve(tracked.size());
    for(const auto& a:tracked)pending_.push_back(a.first);
    std::sort(pending_.begin(),pending_.end());
    if(lastTransferredMinute_.size()>tracked.size()*2u+64u)lastTransferredMinute_.clear();
  }
  // If the game clock changes while a previous batch is running, complete
  // that batch first. No fabricated off-screen simulation in real seconds.
  constexpr size_t BATCH=64;
  for(size_t processed=0;cursor_<pending_.size()&&processed<BATCH;++cursor_,++processed){
    const uint32_t id=pending_[cursor_];
    const auto it=tracked.find(id);if(it==tracked.end())continue;
    const auto& state=it->second;
    ++out.visited;
    if(state.dead||!cb.alive(id))continue;
    if(state.hostile){++out.hostileSkipped;continue;}
    if(state.cell==residentCell&&state.world==residentWorld){++out.residentSkipped;continue;}
    if(!state.cell){++out.unsupported;continue;}
    // A persisted XTEL hop must not repeat on the same game minute (including
    // a new scheduler batch following scene transition).
    if(lastTransferredMinute_[id]==uint32_t(now+1))continue;
    const auto base=defs.targets.find(id);if(base==defs.targets.end())continue;
    const Candidate chosen=Select(id,base->second.base,state,hour,defs,cb);
    if(!chosen.id||!chosen.targetCell){++out.unsupported;continue;}
    fo3player::ActorState next=state;
    if(state.package!=chosen.id){next.package=chosen.id;next.sequence=0;next.packageWaitSeconds=0;}
    if(next.cell!=chosen.targetCell){
      const auto* edge=FirstDoor(graph,next.cell,chosen.targetCell,id,cb,defs);
      if(!edge){++out.blocked;continue;}
      // One authored CELL hop per in-game minute. No off-screen NAVM/door
      // animation or guarantee of detailed pathfinding in an unloaded CELL.
      next.cell=edge->destinationCell;next.world=edge->destinationWorld;
      next.position=edge->arrival;next.yaw=edge->rotation[2];
      next.sequence=0;next.packageWaitSeconds=0;
    }
    if(next.cell==state.cell&&next.world==state.world&&next.package==state.package)continue;
    if(cb.persist(id,next)){
      if(next.package!=state.package)++out.packageChanges;
      if(next.cell!=state.cell||next.world!=state.world){++out.doorHops;lastTransferredMinute_[id]=uint32_t(now+1);}
    }else ++out.blocked;
  }
  if(cursor_>=pending_.size()){pending_.clear();cursor_=0;}
  return out;
}
} // namespace fo3unloaded
