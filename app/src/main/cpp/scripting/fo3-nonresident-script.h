#pragma once
#include "player/fo3-player-state.h"
#include <algorithm>
namespace fo3script {
// A deliberately bounded publication policy: only removal of off-scene actors.
// Static references, enable-parent children, arrivals and resident actors need
// the full resource exchange coordinator; they are never success-only effects.
inline bool NonresidentActorRemoval(const fo3player::Player& player,
    const fo3player::ScriptTransactionPlan& plan,uint32_t cell,uint32_t world,
    const std::vector<uint32_t>& liveActors,std::string& error) {
  const auto after=plan.CandidateReferences();
  if(!after||!cell){error="nonresident actor scope unavailable";return false;}
  const auto& c=player.Definitions();
  for(auto id:plan.AffectedReferences()){
    const auto ref=c.references.find(id);
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
