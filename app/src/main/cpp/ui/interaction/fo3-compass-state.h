#pragma once
#include "player/fo3-player-state.h"
#include "pipboy/fo3-map-runtime.h"
#include "pipboy/fo3-quest-navigation.h"
#include <cmath>
namespace fo3compass {
constexpr float Pi=3.14159265358979323846f,Window=345.f/1024.f*2*Pi;
enum class Kind {Landmark,Discovered,Quest,Waypoint,Enemy};
struct Icon {Kind kind;float x=0;};
struct Frame {float bearing=0;std::vector<Icon> icons;};
inline float Relative(float x,float y,float bearing){return std::atan2(std::sin(std::atan2(x,y)-bearing),std::cos(std::atan2(x,y)-bearing));}
// One gate per original XTEL hop; interior objectives point at the door leading
// toward their target, not at unrelated coordinates in the other CELL.
inline bool TargetPoint(const fo3player::Player& player,const fo3pipdata::Location& from,uint32_t reference,fo3pipdata::Point& point){
  const auto& d=player.Definitions().pipboy;fo3pipdata::Location target;if(!player.ScriptReferenceLocation(reference,target))return false;
  if((from.world&&from.world==target.world)||(!from.world&&from.cell==target.cell)){point={target.x,target.y};return true;}
  if(from.world){const auto exit=fo3pipdata::ExteriorEntrance(d,target.cell,target.x,target.y);if(exit.world==from.world){point={exit.x,exit.y};return true;}return false;}
  struct Hop{uint32_t cell;bool hasFirst;fo3pipdata::Point first;};std::vector<Hop> pending{{from.cell,false,{}}};std::unordered_set<uint32_t> seen;
  for(size_t i=0;i<pending.size()&&i<128;++i){const auto hop=pending[i];if(!seen.insert(hop.cell).second)continue;const auto doors=d.doors.find(hop.cell);if(doors==d.doors.end())continue;
    for(const auto& door:doors->second){const auto dest=d.targets.find(door.teleportRef);if(dest==d.targets.end())continue;const auto& t=dest->second;
      const auto first=hop.hasFirst?hop.first:fo3pipdata::Point{door.x,door.y};
      if((target.world&&t.world==target.world)||(!target.world&&t.cell==target.cell)){point=first;return true;}
      if(!t.world)pending.push_back({t.cell,true,first});
    }
  }return false;
}
inline Frame Build(const fo3player::Player& player,const fo3pipdata::Location& location,float bearing){
  Frame out;out.bearing=bearing;const auto& d=player.Definitions().pipboy;const auto& state=player.Snapshot().pipboy;
  auto add=[&](Kind kind,fo3pipdata::Point p){const float dx=p.x-location.x,dy=p.y-location.y;if(dx*dx+dy*dy<.001f)return;const auto angle=Relative(dx,dy,bearing);out.icons.push_back({kind,angle/Window});};
  if(location.world)for(const auto& entry:d.markers){const auto& m=entry.second;
    if(m.world!=location.world||!player.MapMarkerEnabled(entry.first))continue;
    const float dx=m.x-location.x,dy=m.y-location.y;
    if(dx*dx+dy*dy>d.markerVisibleDistance*d.markerVisibleDistance)continue;
    add(fo3pipdata::MarkerKnown(m,entry.first,state)?Kind::Discovered:Kind::Landmark,{m.x,m.y});
  }
  for(auto id:fo3pipdata::NavigationTargets(player)){fo3pipdata::Point point;if(TargetPoint(player,location,id,point))add(Kind::Quest,point);}
  if(location.world&&state.waypoint.world==fo3pipdata::MapWorld(d,location.world)){
    // Waypoints are stored in the root map's game coordinates.
    const auto here=fo3pipdata::WorldPoint(d,location.world,location.x,location.y);
    const auto& p=state.waypoint.point;const auto angle=Relative(p.x-here.x,p.y-here.y,bearing);
    out.icons.push_back({Kind::Waypoint,angle/Window});
  }
  return out;
}
}
