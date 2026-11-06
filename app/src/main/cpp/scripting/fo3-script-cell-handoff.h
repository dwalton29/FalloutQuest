#pragma once
#include "player/fo3-player-state.h"
namespace fo3script {
inline bool CellHandoff(const fo3player::Player& player,const fo3player::ScriptTransactionPlan& plan,uint32_t cell,uint32_t world){
  const auto* state=plan.CandidateState();if(!state||!plan.CandidateReferences()||!state->playerMove.pending)return false;
  const auto& move=state->playerMove;const auto& old=player.Snapshot().playerMove;
  if(old.pending||!move.marker||!move.cell)return false;
  if(move.cell==cell&&move.world==world&&move.marker==old.marker&&move.position==old.position&&move.rotation==old.rotation)return false;
  const auto placed=player.Definitions().pipboy.targets.find(move.marker);
  return placed!=player.Definitions().pipboy.targets.end()&&placed->second.cell==move.cell&&placed->second.world==move.world;
}
// Queueing must synchronously enter the standard loading barrier on success,
// and leave it untouched on failure. Side effects stay queued until publication.
template<class Queue>
bool CommitCellHandoff(fo3player::Player& player,fo3player::ScriptTransactionPlan& plan,Queue queue,std::string& error){
  const auto* state=plan.CandidateState();if(!state){error="cell handoff candidate unavailable";return false;}
  const auto move=state->playerMove;
  if(!player.CommitScriptTransaction(plan)){error="cell handoff transaction changed";return false;}
  if(queue(move))return true;
  if(!player.RollbackScriptTransaction(plan))error="cell handoff rollback failed";
  else error="cell handoff destination queue unavailable";
  return false;
}
}
