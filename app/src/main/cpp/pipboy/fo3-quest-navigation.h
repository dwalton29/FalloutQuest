#pragma once
#include <algorithm>
#include "player/fo3-player-state.h"
#include "dialogue/fo3-dialogue-conditions.h"
#include "fo3-map-runtime.h"
namespace fo3pipdata {
inline std::vector<uint32_t> NavigationTargets(const fo3player::Player& player){
  std::vector<uint32_t> out;const auto& d=player.Definitions().pipboy;const auto& state=player.Snapshot().pipboy;
  const auto saved=state.quests.find(state.selectedQuest);if(saved==state.quests.end()||saved->second.status!=Completion::Active)return out;
  const auto quest=d.quests.find(saved->first);if(quest==d.quests.end())return out;
  Location pc;if(!player.ScriptReferenceLocation(fo3player::PlayerRef,pc))return out;
  fo3dialogue::Context ctx;ctx.player=&player;ctx.speaker={fo3player::PlayerRef,fo3player::PlayerBase,pc.cell,pc.world,0,pc.x,pc.y,player.ScriptPlayerHeight(),false,false};
  for(const auto& row:saved->second.objectives){if(!row.second.displayed||row.second.status!=Completion::Active)continue;const auto objective=quest->second.objectives.find(row.first);if(objective==quest->second.objectives.end())continue;
    const auto& def=objective->second;
    if(def.targetItems.empty()){if(!def.conditionalTargets)for(auto id:def.targets)if(d.targets.count(id))out.push_back(id);continue;}
    for(const auto& target:def.targetItems){const auto placed=d.targets.find(target.reference);if(placed==d.targets.end())continue;
      const auto& p=placed->second;ctx.target={target.reference,p.base,p.cell,p.world,0,p.x,p.y,p.z,false,false};std::string error;
      if(fo3dialogue::Conditions(target.conditions,ctx,error)&&error.empty())out.push_back(target.reference);
    }
  }
  std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());return out;
}
}
