#include "fo3-npc-conversation.h"
#include <algorithm>
#include <limits>
namespace fo3npcchat {
Line SelectHello(const fo3pipdata::Definitions& d,const fo3player::Player& player,
                 const fo3dialogue::Context& context) {
  if(!context.speaker.reference||!context.target.reference||
     context.target.reference==0x14||context.speaker.reference==context.target.reference||
     player.ActorHealth(context.speaker.reference)<=0||player.ActorHealth(context.target.reference)<=0||
     context.speaker.combat||context.target.combat)return {};
  uint32_t hello=0;
  for(const auto& topic:d.dialogueTopics)
    if(topic.second.editor=="HELLO"&&topic.second.type==1){hello=topic.first;break;}
  if(!hello)return {};
  const auto entries=d.topics.find(hello);if(entries==d.topics.end())return {};
  const fo3pipdata::Info* best=nullptr;
  int priority=std::numeric_limits<int>::min();
  const uint32_t voice=fo3pipdata::ActorVoice(d,context.speaker.base);
  fo3dialogue::Context ctx=context;ctx.player=&player;ctx.talking=true;
  for(const auto& info:entries->second) {
    if(!info.orderValid||(info.type!=0&&info.type!=3)||info.nextSpeaker||info.flags2||
       (info.recordFlags&0x20u)||(info.flags&0x90u)||info.responses.size()!=1||
       (info.speaker&&info.speaker!=context.speaker.base)||
       !info.begin.source.empty()||!info.begin.compiled.empty()||
       !info.end.source.empty()||!info.end.compiled.empty())continue;
    const auto& r=info.responses.front();
    if(r.text.empty()&&!r.sound)continue;
    if((info.flags&4u)&&player.Snapshot().pipboy.saidInfos.count(
       (uint64_t(context.speaker.reference)<<32)|info.id))continue;
    const auto quest=d.quests.find(info.quest);
    if(quest==d.quests.end())continue;
    const auto active=player.Snapshot().pipboy.quests.find(info.quest);
    if(active==player.Snapshot().pipboy.quests.end()&&!(quest->second.flags&1))continue;
    if(active!=player.Snapshot().pipboy.quests.end()&&
       active->second.status!=fo3pipdata::Completion::Active)continue;
    std::string error;
    if(!fo3dialogue::Conditions(info.conditions,ctx,error)||!error.empty())continue;
    error.clear();
    if(!fo3dialogue::Conditions(quest->second.conditions,ctx,error)||!error.empty())continue;
    const auto sound=d.sounds.find(r.sound);
    const std::string audio=sound!=d.sounds.end()?sound->second:
      voice?fo3pipdata::VoicePath(d,info,r,voice):std::string{};
    if(audio.empty())continue;
    if(quest->second.priority>priority){
      priority=quest->second.priority;best=&info;
    }
  }
  if(!best)return {};
  const auto& response=best->responses.front();
  const auto sound=d.sounds.find(response.sound);
  return {hello,best->id,context.speaker.reference,context.target.reference,
    sound!=d.sounds.end()?sound->second:fo3pipdata::VoicePath(d,*best,response,voice),
    response.text};
}
}
