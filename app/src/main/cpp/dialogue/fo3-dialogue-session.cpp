#include "fo3-dialogue-session.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
namespace fo3dialogue {
static std::string Id(uint32_t id){char b[16];std::snprintf(b,sizeof(b),"%08X",id);return b;}
const fo3pipdata::Info* Session::Current(const fo3pipdata::Definitions& d)const {
  auto t=d.topics.find(topic);if(t==d.topics.end())return nullptr;
  for(auto& i:t->second)if(i.id==info)return &i;
  return nullptr;
}
bool Session::Eligible(const fo3pipdata::Info& i,const Context& ctx)const {
  const auto& p=*ctx.player;const auto& d=p.Definitions().pipboy;
  if(!i.orderValid){if(diagnostic)diagnostic("DIALOGUE UNSUPPORTED SCRIPT info="+Id(i.id)+" cyclic Previous INFO order");return false;}
  if((i.type!=0&&i.type!=3)||(i.recordFlags&0x20)||i.nextSpeaker||i.flags2||(i.flags&0x90)||i.responses.empty()||(i.speaker&&i.speaker!=ctx.speaker.base))return false;
  if((i.flags&4)&&p.Snapshot().pipboy.saidInfos.count((uint64_t(ctx.speaker.reference)<<32)|i.id))return false;
  std::string error;
  if(!Conditions(i.conditions,ctx,error)){if(!error.empty()&&diagnostic)diagnostic("DIALOGUE UNSUPPORTED CONDITION actor="+Id(ctx.speaker.reference)+" base="+Id(ctx.speaker.base)+" dial="+Id(i.topic)+" info="+Id(i.id)+" "+error);return false;}
  auto quest=d.quests.find(i.quest);
  if(quest==d.quests.end())return false;
  if(!Conditions(quest->second.conditions,ctx,error)){if(!error.empty()&&diagnostic)diagnostic("DIALOGUE UNSUPPORTED CONDITION info="+Id(i.id)+" quest="+Id(i.quest)+" "+error);return false;}
  auto qs=p.Snapshot().pipboy.quests.find(i.quest);
  if(qs==p.Snapshot().pipboy.quests.end()&&!(quest->second.flags&1))return false;
  if(qs!=p.Snapshot().pipboy.quests.end()&&!qs->second.running)return false;
  // Transactional preflight includes nested QUST dependencies and both phases.
  if(!p.PreviewDialogueResults(i.begin,i.end,error)) {
    if(diagnostic)diagnostic("DIALOGUE UNSUPPORTED SCRIPT info="+Id(i.id)+" "+error);
    return false;
  }
  for(auto& r:i.responses)if(!r.sound&&!fo3pipdata::ActorVoice(d,ctx.speaker.base))return false;
  return true;
}
const fo3pipdata::Info* Session::Resolve(uint32_t id,const Context& ctx,bool draw)const {
  const auto& d=ctx.player->Definitions().pipboy;auto t=d.topics.find(id);if(t==d.topics.end())return nullptr;
  // Quest priority precedes authored INFO order. Random stacks include only
  // eligible INFOs and stop at the next eligible non-random / Random End.
  int priority=-1;
  for(auto& i:t->second)if(Eligible(i,ctx)){auto q=d.quests.find(i.quest);priority=std::max(priority,int(q->second.priority));}
  std::vector<const fo3pipdata::Info*> pool;
  for(auto& i:t->second) {
    auto q=d.quests.find(i.quest);
    if(q==d.quests.end()||q->second.priority!=priority||!Eligible(i,ctx))continue;
    if(!(i.flags&2)){if(pool.empty())return &i;break;}
    pool.push_back(&i);if(i.flags&32)break;
  }
  if(pool.empty())return nullptr;
  return pool[draw?std::uniform_int_distribution<size_t>(0,pool.size()-1)(random):0];
}
bool Session::CanStart(const Context& ctx)const {
  if(!ctx.player||ctx.player->ActorHealth(ctx.speaker.reference)<=0||ctx.speaker.combat)return false;
  for(auto t:ctx.player->Definitions().pipboy.greetings)if(Resolve(t,ctx))return true;
  return false;
}
bool Session::CanActivate(const Context& ctx)const {
  if(!ctx.player)return false;
  std::unordered_map<uint64_t,float> pending;std::string error;
  if(!ActivationVariables(ctx,*ctx.player,pending,error))return false;
  Context activated=ctx;activated.activationVariables=&pending;
  return CanStart(activated);
}
bool Session::Start(const Context& ctx,fo3player::Player& p) {
  if(Active()||!ctx.player||ctx.speaker.combat||p.ActorHealth(ctx.speaker.reference)<=0)return false;
  std::string activationError;
  if(!Activate(ctx,p,activationError)){if(diagnostic)diagnostic("DIALOGUE UNSUPPORTED SCRIPT actor="+Id(ctx.speaker.reference)+" "+activationError);return false;}
  // OnActivate can change local variables used by the next GREETING. Resolve
  // against the resulting state, not the pre-activation eligibility snapshot.
  if(!CanStart(ctx))return false;
  interruption.clear();const float dx=ctx.speaker.x-ctx.target.x,dy=ctx.speaker.y-ctx.target.y,dz=ctx.speaker.z-ctx.target.z;
  startDistanceGameUnits=std::sqrt(dx*dx+dy*dy+dz*dz);
  startDistance=std::sqrt((ctx.speaker.x-ctx.target.x)*(ctx.speaker.x-ctx.target.x)+(ctx.speaker.y-ctx.target.y)*(ctx.speaker.y-ctx.target.y)+(ctx.speaker.z-ctx.target.z)*(ctx.speaker.z-ctx.target.z))/100.f;endReason.clear();
  actor=ctx.speaker.reference;base=ctx.speaker.base;voice=fo3pipdata::ActorVoice(p.Definitions().pipboy,base);
  for(auto t:p.Definitions().pipboy.greetings)if(auto i=Resolve(t,ctx,true))return Enter(*i,ctx,p);
  return false;
}
bool Session::Enter(const fo3pipdata::Info& i,const Context&,fo3player::Player& p) {
  std::string error;if(!p.ExecuteDialogueResult(i.begin,error)){if(diagnostic)diagnostic("DIALOGUE UNSUPPORTED SCRIPT info="+Id(i.id)+" "+error);End("result rejected");return false;}
  if(diagnostic&&!i.begin.source.empty())diagnostic("DIALOGUE RESULT info="+Id(i.id)+" phase=begin");
  topic=i.topic;info=i.id;response=selected=0;choices.clear();phase=Phase::Speaking;++audioToken;
  if(diagnostic)diagnostic("DIALOGUE INFO actor="+Id(actor)+" base="+Id(base)+" topic="+Id(topic)+" info="+Id(info)+" voice="+Id(voice));
  return true;
}
bool Session::Choose(size_t index,const Context& ctx,fo3player::Player& p) {
  if(phase!=Phase::Choices||index>=choices.size())return false;
  auto t=choices[index].topic;auto i=Resolve(t,ctx,true);if(!i)return false;
  if(diagnostic)diagnostic("DIALOGUE CHOICE actor="+Id(actor)+" topic="+Id(t)+" index="+std::to_string(index));
  return Enter(*i,ctx,p);
}
void Session::BuildChoices(const Context& ctx) {
  choices.clear();const auto& d=ctx.player->Definitions().pipboy;auto i=Current(d);if(!i)return;
  auto topics=i->links;
  if(topics.empty()) {topics=d.topLevelTopics;for(auto t:ctx.player->Snapshot().pipboy.knownTopics)topics.push_back(t);}
  std::unordered_set<uint32_t> seen;
  for(auto t:topics) {
    auto td=d.dialogueTopics.find(t);if(td==d.dialogueTopics.end()||td->second.type!=0||!seen.insert(t).second)continue;
    auto chosen=Resolve(t,ctx);if(!chosen)continue;
    const auto& text=chosen->prompt.empty()?td->second.text:chosen->prompt;
    if(!text.empty())choices.push_back({t,chosen->id,text});
  }
  selected=0;
}
bool Session::AudioDone(uint32_t token,bool success,const Context& ctx,fo3player::Player& p) {
  if(phase!=Phase::Speaking||token!=audioToken)return false;
  if(!success&&diagnostic)diagnostic("DIALOGUE VOICE UNAVAILABLE actor="+Id(actor)+" info="+Id(info)+" response="+std::to_string(response)+" continuing without audio");
  auto i=Current(p.Definitions().pipboy);if(!i){End("INFO missing");return false;}
  if(++response<i->responses.size()){++audioToken;return true;}
  if(diagnostic)diagnostic("DIALOGUE TRANSITION info="+Id(i->id)+" stage=end-result");
  std::string error;if(!p.ExecuteDialogueResult(i->end,error)){if(diagnostic)diagnostic("DIALOGUE UNSUPPORTED SCRIPT info="+Id(i->id)+" "+error);End("end result rejected");return false;}
  if(diagnostic&&!i->end.source.empty())diagnostic("DIALOGUE RESULT info="+Id(i->id)+" phase=end");
  p.RecordDialogue(actor,(i->flags&4)?i->id:0,i->addedTopics);
  if(i->flags&1){End("authored goodbye");return false;}
  phase=Phase::Choices;BuildChoices(ctx);return false;
}
std::string Session::Audio(const fo3pipdata::Definitions& d)const {
  auto i=Current(d);if(phase!=Phase::Speaking||!i||response>=i->responses.size())return {};
  auto& r=i->responses[response];auto sound=d.sounds.find(r.sound);
  return sound!=d.sounds.end()?sound->second:fo3pipdata::VoicePath(d,*i,r,voice);
}
std::string Session::Subtitle(const fo3pipdata::Definitions& d)const {
  auto i=Current(d);return phase==Phase::Speaking&&i&&response<i->responses.size()?i->responses[response].text:"";
}
void Session::End(const std::string& reason) {
  if(Active()&&diagnostic)diagnostic("DIALOGUE END actor="+Id(actor)+" info="+Id(info)+" reason="+reason);
  interruption=reason;phase=Phase::Inactive;actor=base=topic=info=voice=0;response=selected=0;choices.clear();++audioToken;
}
}
