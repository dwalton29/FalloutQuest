#pragma once
#include "fo3-dialogue-info-policy.h"
#include "fo3-dialogue-conditions.h"
#include <algorithm>
#include <random>
#include <cstdio>
namespace fo3dialogue {
inline std::string SelectionId(uint32_t id){char b[16];std::snprintf(b,sizeof(b),"%08X",id);return b;}
inline bool EligibleInfo(const fo3pipdata::Info& i,const Context& ctx,
  const Diagnostic& diagnostic,const std::function<bool(const fo3player::Player&,const fo3player::ScriptTransactionPlan&,std::string&)>& previewReferences) {
  const auto& p=*ctx.player;const auto& d=p.Definitions().pipboy;
  if(!i.orderValid){if(diagnostic)diagnostic("DIALOGUE UNSUPPORTED SCRIPT info="+SelectionId(i.id)+" cyclic Previous INFO order");return false;}
  if(!SupportedInfoStructure(i)||(i.speaker&&i.speaker!=ctx.speaker.base))return false;
  if((i.flags&4)&&p.Snapshot().pipboy.saidInfos.count((uint64_t(ctx.speaker.reference)<<32)|i.id))return false;
  if((i.flags2&1u)&&!p.CanSayDailyInfo(ctx.speaker.reference,i.id))return false;
  std::string error;
  if(!Conditions(i.conditions,ctx,error)){if(!error.empty()&&diagnostic)diagnostic("DIALOGUE UNSUPPORTED CONDITION actor="+SelectionId(ctx.speaker.reference)+" base="+SelectionId(ctx.speaker.base)+" dial="+SelectionId(i.topic)+" info="+SelectionId(i.id)+" "+error);return false;}
  auto quest=d.quests.find(i.quest);
  if(quest==d.quests.end())return false;
  if(!Conditions(quest->second.conditions,ctx,error)){if(!error.empty()&&diagnostic)diagnostic("DIALOGUE UNSUPPORTED CONDITION info="+SelectionId(i.id)+" quest="+SelectionId(i.quest)+" "+error);return false;}
  auto qs=p.Snapshot().pipboy.quests.find(i.quest);
  if(qs==p.Snapshot().pipboy.quests.end()&&!(quest->second.flags&1))return false;
  if(qs!=p.Snapshot().pipboy.quests.end()&&!qs->second.running)return false;
  // Transactional preflight includes nested QUST dependencies and both phases.
  auto plan=p.PrepareDialogueInfoTransaction(i.topic,i.id,fo3player::DialogueScriptPhase::Both,error,ctx.speaker.reference);
  if(plan&&plan->HasReferenceChanges()&&(!previewReferences||!previewReferences(p,*plan,error))){
    if(error.empty())error="world mutation requires resident coordinator";
    plan.reset();
  }
  if(!plan) {
    if(diagnostic)diagnostic("DIALOGUE UNSUPPORTED SCRIPT info="+SelectionId(i.id)+" "+error);
    return false;
  }
  for(auto& r:i.responses)if(!r.sound&&!(p.ActorVoice(ctx.speaker.reference)?p.ActorVoice(ctx.speaker.reference):fo3pipdata::ActorVoice(d,ctx.speaker.base)))return false;
  return true;
}
template<class Eligible>
inline const fo3pipdata::Info* ResolveTopicInfo(uint32_t id,const Context& ctx,Eligible eligible,std::mt19937* random) {
  const auto& d=ctx.player->Definitions().pipboy;auto t=d.topics.find(id);if(t==d.topics.end())return nullptr;
  // Quest priority precedes authored INFO order. Random stacks include only
  // eligible INFOs and stop at the next eligible non-random / Random End.
  int priority=-1;
  for(auto& i:t->second)if(eligible(i)){auto q=d.quests.find(i.quest);priority=std::max(priority,int(q->second.priority));}
  std::vector<const fo3pipdata::Info*> pool;
  for(auto& i:t->second) {
    auto q=d.quests.find(i.quest);
    if(q==d.quests.end()||q->second.priority!=priority||!eligible(i))continue;
    if(!(i.flags&2)){if(pool.empty())return &i;break;}
    pool.push_back(&i);if(i.flags&32)break;
  }
  if(pool.empty())return nullptr;
  return pool[random?std::uniform_int_distribution<size_t>(0,pool.size()-1)(*random):0];
}

inline bool ReferenceContext(const fo3player::Player& player,uint32_t id,ActorContext& out){
  fo3pipdata::Location location;if(!player.ScriptReferenceLocation(id,location))return false;
  const auto& d=player.Definitions().pipboy;const auto placed=d.targets.find(id);
  out={id,id==fo3player::PlayerRef?fo3player::PlayerBase:placed==d.targets.end()?0:placed->second.base,location.cell,location.world,0,location.x,location.y,id==fo3player::PlayerRef?player.ScriptPlayerHeight():placed==d.targets.end()?0:placed->second.z,false,false};
  const auto actor=player.Snapshot().actors.find(id);if(actor!=player.Snapshot().actors.end()){out.z=actor->second.position[2];out.package=actor->second.package;out.packageKnown=true;out.combat=actor->second.hostile!=0;}
  return out.base!=0;
}
}
