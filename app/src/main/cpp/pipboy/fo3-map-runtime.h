#pragma once
#include "fo3-radio-runtime.h"
#include <memory>
namespace fo3pipdata {
struct LocalMap {
  uint32_t cell = 0;
  uint64_t generation = 0;
  float minX = 0, minY = 0, maxX = 1, maxY = 1;
  std::vector<uint8_t> rgba;
  Point Project(float x, float y) const {
    return {(x - minX) / (maxX - minX), (maxY - y) / (maxY - minY)};
  }
};
struct MapContext {
  Location location, exterior;
  Point localOrigin;
  float sceneForward = 0;
  uint32_t mapWorld = 0;
  Point player, centre{.5f, .5f};
  float zoom = 1;
  bool playerKnown=true;
  std::shared_ptr<const LocalMap> local;
};
inline uint32_t MapWorld(const Definitions &d, uint32_t world) {
  for (int depth = 0; depth < 8 && world; ++depth) {
    auto w = d.worlds.find(world);
    if (w == d.worlds.end())
      return 0;
    if (w->second.parent && (w->second.parentFlags & 4))
      world = w->second.parent;
    else
      return w->second.valid ? world : 0;
  }
  return 0;
}
// ONAM offsets are game units. Scale about NAM0/NAM9 centre (EXE 007C8D00).
inline Point WorldPoint(const Definitions &d, uint32_t world, float x,
                        float y) {
  for (int depth = 0; depth < 8; ++depth) {
    auto w = d.worlds.find(world);
    if (w == d.worlds.end() || !w->second.parent ||
        !(w->second.parentFlags & 4))
      break;
    const auto point=w->second.ToMapPoint(x,y);x=point.x;y=point.y;
    world = w->second.parent;
  }
  return {x, y};
}
inline Point MapProject(const Definitions &d, uint32_t world, float x,
                        float y) {
  const auto point = WorldPoint(d, world, x, y);
  const auto root = MapWorld(d, world);
  return root ? d.worlds.at(root).Project(point.x, point.y) : Point{};
}
inline const char* TravelDenial(const std::string& key){
  if(key=="sNoFastTravelCombat")return "You cannot fast travel during combat!";
  if(key=="sNoFastTravelHostileActorsNear")return "You cannot fast travel when enemies are nearby.";
  if(key=="sNoFastTravelScriptBlock")return "Fast travel is currently unavaliable from this location.";
  if(key=="sNoFastTravelOverencumbered")return "You cannot fast travel while overencumbered.";
  if(key=="sNoFastTravelUndiscovered")return "You have not discovered this location yet.";
  if(key=="sNoFastTravelInAir")return "You can't fast travel while jumping or falling.";
  if(key=="sFastTravelNoTravelHealthDamage")return "You can't fast travel while taking health damage";
  return "You cannot fast travel from this location.";
}
inline bool MarkerKnown(const Marker& m,uint32_t id,const SessionState& s){
  return (m.mapFlags&2)||s.discovered.count(id)||s.travelEnabled.count(id);
}
inline bool MarkerVisible(const Marker& m,uint32_t id,const SessionState& s){
  return (m.mapFlags&1)||s.revealed.count(id)||MarkerKnown(m,id,s);
}
// Follow original XTEL links out of an interior to its exterior map position.
inline Location ExteriorEntrance(const Definitions& d,uint32_t cell,float x=0,float y=0){
  std::vector<uint32_t> pending{cell};std::unordered_set<uint32_t> seen;
  for(size_t i=0;i<pending.size()&&i<128;++i){
    if(!seen.insert(pending[i]).second)continue;
    const auto doors=d.doors.find(pending[i]);if(doors==d.doors.end())continue;
    Location best;double closest=1e100;
    for(const auto& door:doors->second){
      const auto target=d.targets.find(door.teleportRef);if(target==d.targets.end())continue;
      const auto& p=target->second;
      if(p.world&&MapWorld(d,p.world)){
        const double dx=double(door.x)-x,dy=double(door.y)-y,distance=dx*dx+dy*dy;
        if(distance<closest){closest=distance;best={p.cell,p.world,p.x,p.y,p.rz};}
      }
      if(!p.world&&p.cell&&!seen.count(p.cell))pending.push_back(p.cell);
    }
    if(best.world)return best;
  }
  return {};
}
inline std::vector<uint32_t> QuestTargets(const Definitions &d,
                                          const SessionState &s) {
  std::vector<uint32_t> out;
  auto q = s.quests.find(s.selectedQuest);
  if (q == s.quests.end() || q->second.status != Completion::Active)
    return out;
  auto def = d.quests.find(q->first);
  if (def == d.quests.end())
    return out;
  for (auto &o : q->second.objectives) {
    auto f = def->second.objectives.find(o.first);
    if (o.second.displayed && o.second.status == Completion::Active &&
        f != def->second.objectives.end() && !f->second.conditionalTargets)
      for (auto id : f->second.targets)
        if (d.targets.count(id))
          out.push_back(id);
  }
  return out;
}
} // namespace fo3pipdata
