#pragma once
#include "player/fo3-player-state.h"
#include <algorithm>
namespace fo3script {
// Bounded logical publication: off-scene actor removal, resource-free CREA
// changes, and off-scene radio state. Every affected enable-parent descendant
// must pass independently. Resident resources need the full scene coordinator.
inline bool NonresidentActorRemoval(const fo3player::Player& player,
    const fo3player::ScriptTransactionPlan& plan,uint32_t cell,uint32_t world,
    const std::vector<uint32_t>& liveActors,std::string& error) {
  const auto after=plan.CandidateReferences();
  if(!after||!cell){error="nonresident actor scope unavailable";return false;}
  const auto& c=player.Definitions();
  for(auto id:plan.AffectedReferences()){
    const auto ref=c.references.find(id);
    // CREA currently has no scene resource adapter. Its complete
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
}
