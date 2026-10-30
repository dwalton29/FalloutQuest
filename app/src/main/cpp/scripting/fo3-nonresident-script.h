#pragma once
#include "player/fo3-player-state.h"
#include <algorithm>
namespace fo3script {
// Logical publication for references outside the current CELL/world and live
// resource inventory, plus existing resource-free actor/CREA changes. Every
// enable-parent descendant is checked. Resident resources use the coordinator.
inline bool NonresidentActorRemoval(const fo3player::Player& player,
    const fo3player::ScriptTransactionPlan& plan,uint32_t cell,uint32_t world,
    const std::vector<uint32_t>& liveActors,std::string& error) {
  const auto after=plan.CandidateReferences();
  if(!after||!cell){error="nonresident actor scope unavailable";return false;}
  const auto& c=player.Definitions();
  for(auto id:plan.AffectedReferences()){
    const auto ref=c.references.find(id);
    if(ref!=c.references.end()&&ref->second.valid&&ref->second.base&&!(ref->second.flags&0x20u)&&
       std::find(liveActors.begin(),liveActors.end(),id)==liveActors.end()){
      uint32_t location=ref->second.cell,space=0;
      const auto originalWorld=c.pipboy.cellWorlds.find(location);
      bool located=originalWorld!=c.pipboy.cellWorlds.end();
      if(located)space=originalWorld->second;
      const auto actor=player.Snapshot().actors.find(id);
      if(actor!=player.Snapshot().actors.end()){location=actor->second.cell;space=actor->second.world;located=true;}
      if(located&&location&&location!=cell&&(!world||space!=world))continue;
    }

    if(ref!=c.references.end()&&c.actorPlacements.count(id)&&c.weapons.actors.count(ref->second.base)&&
       player.Snapshot().actors.count(id)&&std::find(liveActors.begin(),liveActors.end(),id)==liveActors.end())continue;
    // Unprepared creature resources remain deferred. Their complete
    // enable-parent closure may be published logically only when none of the
    // affected identities owns live resources. The canonical saved overlay is
    // still real; this does not manufacture a visible creature.
    if(ref!=c.references.end()&&c.deferredBases.count(ref->second.base)&&
       std::find(liveActors.begin(),liveActors.end(),id)==liveActors.end())continue;
    // Radios can own visible terminal geometry. Admit their logical state
    // only outside the current CELL/world, never just because a mesh is culled.
    if(ref!=c.references.end()&&c.pipboy.stations.count(ref->second.base)&&
       std::find(liveActors.begin(),liveActors.end(),id)==liveActors.end()){
      const auto target=c.pipboy.targets.find(id);
      if(target!=c.pipboy.targets.end()&&target->second.cell&&target->second.cell!=cell&&
         (!world||target->second.world!=world))continue;
    }
    if(ref==c.references.end()||!c.actorInventories.count(ref->second.base)||
       c.enableParents.count(id)||after->Enabled(id,true)){
      error="reference change requires resident resource coordinator";return false;
    }
    uint32_t actorCell=ref->second.cell,actorWorld=0;
    const auto saved=player.Snapshot().actors.find(id);
    if(saved!=player.Snapshot().actors.end()){actorCell=saved->second.cell;actorWorld=saved->second.world;}
    else {const auto w=c.pipboy.cellWorlds.find(actorCell);if(w!=c.pipboy.cellWorlds.end())actorWorld=w->second;}
    if(!actorCell||actorCell==cell||(world&&actorWorld==world)||
       std::find(liveActors.begin(),liveActors.end(),id)!=liveActors.end()){
      error="actor removal intersects resident scene";return false;
    }
  }
  return true;
}
// Existing inventory displays can be hidden without rebuilding GPU resources.
// Every affected reference must be an actual item and become disabled; a held
// item, actor, furniture, load door or enabling change still needs its owner.
template<class Available>
inline bool InventoryReferenceRemoval(const fo3player::Player& player,
    const fo3player::ScriptTransactionPlan& plan,Available available,std::string& error){
  const auto after=plan.CandidateReferences();const auto& catalog=player.Definitions();
  if(!after||plan.AffectedReferences().empty())return false;
  for(auto ref:plan.AffectedReferences()){
    const auto item=catalog.references.find(ref);
    if(item==catalog.references.end()||!item->second.valid||!catalog.items.count(item->second.base)||
       after->Enabled(ref,true)||!available(ref)){error="inventory removal requires an unheld disabled item";return false;}
  }
  error.clear();return true;
}

}
