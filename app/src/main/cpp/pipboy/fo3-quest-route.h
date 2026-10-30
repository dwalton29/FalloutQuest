#pragma once
#include "player/fo3-player-state.h"
#include <queue>
#include <limits>
#include <cmath>
namespace fo3questroute {
using Region=uint64_t;
inline Region Area(const fo3pipdata::Location& p){return p.world?(uint64_t(1)<<63)|p.world:p.cell;}
inline Region Area(const fo3pipdata::Placement& p){return p.world?(uint64_t(1)<<63)|p.world:p.cell;}
inline float Distance(float x,float y,float a,float b){return std::hypot(x-a,y-b);}
struct Result {uint32_t door=0;fo3pipdata::Point point{};bool valid=false;};
class Navigator {
  struct Node {uint32_t ref=0;fo3pipdata::Placement placed;std::vector<size_t> incoming;};
  struct Goal {Region area=0;int x=0,y=0;bool ignoreLocks=false;std::vector<float> cost;};
  const fo3pipdata::Definitions* definitions_=nullptr;
  std::vector<Node> nodes_;
  std::unordered_map<uint32_t,size_t> ids_;
  std::unordered_map<Region,std::vector<size_t>> regions_;
  std::unordered_map<uint32_t,Goal> goals_;
  uint64_t policy_=~uint64_t{0};
  void Bind(const fo3pipdata::Definitions& d){
    if(definitions_==&d)return;
    definitions_=&d;nodes_.clear();ids_.clear();regions_.clear();goals_.clear();policy_=~uint64_t{0};
    auto add=[&](uint32_t id){if(ids_.count(id))return;const auto p=d.targets.find(id);if(p==d.targets.end()||!Area(p->second))return;ids_[id]=nodes_.size();nodes_.push_back({id,p->second,{}});};
    std::vector<uint32_t> portals;for(const auto& row:d.targets)if(row.second.teleportRef)portals.push_back(row.first);std::sort(portals.begin(),portals.end());
    for(auto ref:portals){add(ref);add(d.targets.at(ref).teleportRef);}
    for(size_t i=0;i<nodes_.size();++i){regions_[Area(nodes_[i].placed)].push_back(i);const auto destination=ids_.find(nodes_[i].placed.teleportRef);if(destination!=ids_.end())nodes_[destination->second].incoming.push_back(i);}
  }
  static uint64_t Policy(const fo3player::Player& p){
    const auto& s=p.Snapshot();uint64_t h=s.referenceStateRevision*1099511628211ull;
    // Live locks, keys, pins and skill changes invalidate routes; actor pose
    // revisions do not. No catalogue/ESM scan in the compass frame loop.
    for(const auto& row:s.locks)h^=(uint64_t(row.first)<<32)^(uint64_t(row.second.change)*2654435761u+row.second.level);
    for(const auto& item:s.inventory){h=h*1099511628211ull+item.id;h=h*1099511628211ull+item.formId;h=h*1099511628211ull+uint32_t(item.count);}
    for(auto skill:s.skills)h=h*1099511628211ull+skill;
    for(auto value:s.special)h=h*1099511628211ull+value;
    for(auto value:s.skillOffsets)h=h*1099511628211ull+value;
    for(const auto& perk:s.pipboy.perks)h^=uint64_t(perk.first)*2654435761u+perk.second;
    return h;
  }
  bool Enabled(const fo3player::Player& p,size_t i)const{bool disabled=false;std::string error;return !(nodes_[i].placed.flags&0x20u)&&p.ReferenceDisabled(nodes_[i].ref,disabled,error)&&!disabled;}
  bool CanTraverse(const fo3player::Player& p,size_t i,bool ignore)const{
    if(!Enabled(p,i))return false;
    const auto lock=p.GetLock(nodes_[i].ref);
    return ignore||!lock.locked||(lock.key&&p.ItemCount(lock.key)>0)||p.CanPickLock(nodes_[i].ref)==fo3player::PickAccess::Ready;
  }
public:
  Result Toward(const fo3player::Player& player,const fo3pipdata::Location& from,uint32_t reference,bool ignoreLocks=false){
    const auto& d=player.Definitions().pipboy;fo3pipdata::Location target;if(!player.ScriptReferenceLocation(reference,target)||!Area(from)||!Area(target))return {};
    if(Area(from)==Area(target))return {0,{target.x,target.y},true};
    Bind(d);const auto policy=Policy(player);if(policy!=policy_){goals_.clear();policy_=policy;}
    if(goals_.size()>16)goals_.clear();
    auto& goal=goals_[reference];
    const int gx=int(std::floor(target.x/256)),gy=int(std::floor(target.y/256));
    if(goal.cost.size()!=nodes_.size()||goal.area!=Area(target)||goal.x!=gx||goal.y!=gy||goal.ignoreLocks!=ignoreLocks){
      goal={Area(target),gx,gy,ignoreLocks,std::vector<float>(nodes_.size(),INFINITY)};
      using Entry=std::pair<float,size_t>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> open;
      auto relax=[&](size_t index,float cost){if(cost<goal.cost[index]){goal.cost[index]=cost;open.push({cost,index});}};
      const auto end=regions_.find(goal.area);if(end==regions_.end())return {};
      for(auto index:end->second)if(Enabled(player,index)){const auto& p=nodes_[index].placed;relax(index,Distance(p.x,p.y,target.x,target.y));}
      while(!open.empty()){
        const auto current=open.top();open.pop();const auto i=current.second;if(current.first!=goal.cost[i])continue;
        const auto& p=nodes_[i].placed;
        // Reverse directed XTEL edges. Arrival geometry is not assumed to
        // teleport back, and disabled destinations cannot be used as exits.
        if(Enabled(player,i))for(auto source:nodes_[i].incoming)if(CanTraverse(player,source,ignoreLocks))relax(source,current.first+1.f);
        const auto group=regions_.find(Area(p));
        if(group!=regions_.end())for(auto neighbor:group->second)if(Enabled(player,neighbor)){const auto& q=nodes_[neighbor].placed;relax(neighbor,current.first+Distance(p.x,p.y,q.x,q.y));}
      }
    }
    Result out;float best=INFINITY;const auto start=regions_.find(Area(from));if(start==regions_.end())return out;
    for(auto index:start->second){const auto& p=nodes_[index].placed;if(!p.teleportRef||!CanTraverse(player,index,ignoreLocks))continue;const auto destination=ids_.find(p.teleportRef);if(destination==ids_.end()||!Enabled(player,destination->second))continue;const float cost=Distance(from.x,from.y,p.x,p.y)+1.f+goal.cost[destination->second];if(cost<best){best=cost;out={nodes_[index].ref,{p.x,p.y},true};}}
    return out;
  }
};
}
