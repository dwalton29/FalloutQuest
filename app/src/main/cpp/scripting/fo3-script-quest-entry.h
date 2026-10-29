#pragma once
#include "player/fo3-player-state.h"
#include <algorithm>
#include <limits>

namespace fo3script {
// Explicit developer action, not an automatic quest repair. Run the first
// non-empty authored stage through the interpreter; never reset progress.
inline bool QuestEntryStage(const fo3player::Player& player,uint32_t quest,uint16_t& stage,std::string& error){
  error.clear();const auto& definitions=player.Definitions().pipboy.quests;
  const auto found=definitions.find(quest);
  if(found==definitions.end()||found->second.stages.empty()){error="Quest has no original entry stage";return false;}
  const auto saved=player.Snapshot().pipboy.quests.find(quest);
  if(saved!=player.Snapshot().pipboy.quests.end()){
    const auto& q=saved->second;
    if(q.stage||!q.stages.empty()||!q.objectives.empty()||q.status!=fo3pipdata::Completion::Active){
      error="Quest already has progress; entry was not replayed";return false;
    }
  }
  stage=std::numeric_limits<uint16_t>::max();
  bool authored=false;
  for(const auto& entry:found->second.stages)if(!entry.second.items.empty()){
    stage=std::min(stage,entry.first);authored=true;
  }
  if(!authored)error="Quest has no authored entry effects";
  return authored;
}
inline std::unique_ptr<fo3player::ScriptTransactionPlan> PrepareQuestEntry(
    const fo3player::Player& player,uint32_t quest,std::string& error,
    fo3player::ScriptProgramCapture capture=fo3player::ScriptProgramCapture::None){
  uint16_t stage=0;if(!QuestEntryStage(player,quest,stage,error))return {};
  return player.PrepareQuestStageTransaction(quest,stage,error,capture);
}
}
