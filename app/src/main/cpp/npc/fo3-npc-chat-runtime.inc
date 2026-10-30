// One resident NPC-to-NPC HELLO response sequence, with original INFO results.
// Uses the existing
// dialogue voice channel only when the player dialogue owns nothing.
// No inferred quest results, generated words or synthetic animation assets.
namespace {
struct Q230NpcChatSession {
  uint32_t speaker=0,listener=0,info=0,package=0,token=0,serial=0;
  double began=0;
  bool Active()const{return speaker&&listener&&token;}
};
Q230NpcChatSession gQ230NpcChat;
fo3dialogue::Session gQ230AuthoredNpcChat;
}
static fo3dialogue::Context Q230ChatContext(const Q230ActorVisual& speaker,
                                           const Q230ActorVisual& listener){
  fo3dialogue::Context ctx;ctx.player=&gPlayerSession->player;
  const auto a=Q240GamePosition(speaker),b=Q240GamePosition(listener);
  ctx.speaker={speaker.source.refFormId,speaker.source.baseFormId,
    gCurrentCellFormId,gExteriorWorldspaceQ1890,speaker.aiPackage,
    a[0],a[1],a[2],false,speaker.runtime.packageKnown};
  ctx.target={listener.source.refFormId,listener.source.baseFormId,
    gCurrentCellFormId,gExteriorWorldspaceQ1890,listener.aiPackage,
    b[0],b[1],b[2],false,listener.runtime.packageKnown};
  return ctx;
}
static uint32_t Q230HelloTopic(){
  for(const auto& topic:gPlayerSession->player.Definitions().pipboy.dialogueTopics)if(topic.second.editor=="HELLO"&&topic.second.type==1)return topic.first;
  return 0;
}
static void Q230PrepareNpcChat(fo3dialogue::Session& session){
  session.previewReferences=PreviewFo3DialogueReferences;session.publishReferences=PublishFo3DialogueReferences;
}
static bool Q230ChatEligible(const Q230ActorVisual& initiator,uint32_t reference,uint32_t package) {
  if(!gPlayerSession||gPlayerSession->saveBlocked||gDialogue.Active()||
     gQ230NpcChat.Active()||gPlayerSession->player.AnyScriptActorTalking()||IsFo3LoadingVisible()||Fo3PipboyFocus())return false;
  const auto* partner=DialogueActor(reference);
  if(!partner||partner->runtime.offScene||!partner->runtime.CanTalk()||
     partner->runtime.dialogue||partner->runtime.furniture.Active()||
     partner->runtime.activity==fo3npc::Activity::Combat||
     !initiator.runtime.CanTalk()||initiator.runtime.furniture.Active()||
     partner->rigs.empty()||initiator.rigs.empty())return false;
  auto context=Q230ChatContext(initiator,*partner);context.talking=true;
  if(package){context.speaker.package=package;context.speaker.packageKnown=true;}
  fo3dialogue::Session candidate;Q230PrepareNpcChat(candidate);
  return candidate.Resolve(Q230HelloTopic(),context)!=nullptr;
}
static void Q230ChatStop(const char* reason) {
  if(!gQ230NpcChat.Active())return;
  gQ230AuthoredNpcChat.End(reason);
  const auto speaker=gQ230NpcChat.speaker,listener=gQ230NpcChat.listener;
  if(auto* actor=DialogueActor(speaker)){
    actor->runtime.EndDialogue();
    actor->runtime.packageRetryAfter[gQ230NpcChat.package]=gDialogueTime+6.;
    actor->lastFrame=UINT64_MAX;
  }
  if(auto* actor=DialogueActor(listener)){
    actor->runtime.EndDialogue();actor->lastFrame=UINT64_MAX;
  }
  fo3audio::DialogueStop();
  Q6H_LOGI("NPC CHAT END speaker=%08X listener=%08X info=%08X reason=%s",
     speaker,listener,gQ230NpcChat.info,reason);
  const uint32_t serial=gQ230NpcChat.serial;
  gQ230NpcChat={};gQ230NpcChat.serial=serial;
}
static bool Q230ChatBegin(uint32_t speaker,uint32_t listener,double now) {
  if(gQ230NpcChat.Active()||!gPlayerSession||gDialogue.Active())return false;
  auto* a=DialogueActor(speaker);auto* b=DialogueActor(listener);
  if(!a||!b||!Q230ChatEligible(*a,listener))return false;
  auto context=Q230ChatContext(*a,*b);context.talking=true;
  Q230PrepareNpcChat(gQ230AuthoredNpcChat);
  if(!gQ230AuthoredNpcChat.StartTopic(Q230HelloTopic(),context,gPlayerSession->player))return false;
  const uint32_t serial=(gQ230NpcChat.serial+1u)&0xffffu;
  const uint32_t token=0x20000000u|serial;
  if(!a->runtime.CanTalk()||!b->runtime.CanTalk())return false;
  a->runtime.BeginDialogue();b->runtime.BeginDialogue();
  if(!a->runtime.dialogue||!b->runtime.dialogue){
    a->runtime.EndDialogue();b->runtime.EndDialogue();return false;
  }
  a->runtime.speaking=true;b->runtime.speaking=false;
  a->lastFrame=b->lastFrame=UINT64_MAX;
  gQ230NpcChat={speaker,listener,gQ230AuthoredNpcChat.info,a->aiPackage,token,serial,now};
  Q6H_LOGI("NPC CHAT BEGIN speaker=%08X listener=%08X package=%08X info=%08X token=%u text=%s",
    speaker,listener,a->aiPackage,gQ230AuthoredNpcChat.info,token,gQ230AuthoredNpcChat.Subtitle(gPlayerSession->player.Definitions().pipboy).c_str());
  fo3audio::DialogueGain(.7f);
  fo3audio::Dialogue(gQ230AuthoredNpcChat.Audio(gPlayerSession->player.Definitions().pipboy),token);
  return true;
}
static void Q230ChatTick(double now) {
  static double nextScan=0,nextRemark=0;
  static std::unordered_map<uint32_t,double> greeted;
  if(!gQ230NpcChat.Active()){
    if(now<nextScan)return;nextScan=now+.5;
    if(now<nextRemark||!gPlayerSession||gPlayerSession->saveBlocked||gDialogue.Active()||
       gPlayerSession->player.AnyScriptActorTalking()||IsFo3LoadingVisible()||Fo3PipboyFocus())return;
    for(auto& actor:gQ230NpcActors){
      const auto ref=actor.source.refFormId;
      if(actor.runtime.offScene||!actor.runtime.CanTalk()||actor.runtime.dialogue||actor.rigs.empty()||
         actor.runtime.activity==fo3npc::Activity::Combat)continue;
      const float distance=std::hypot(actor.runtime.position[0]-gQ210Head[0],actor.runtime.position[2]-gQ210Head[2]);
      if(distance>2.5f||now<greeted[ref])continue;
      if(HasFo3InteractionOccluder(gQ210Head[0],gQ210Head[1],gQ210Head[2],
          actor.runtime.position[0]-gQ210Head[0],actor.runtime.position[1]+1-gQ210Head[1],
          actor.runtime.position[2]-gQ210Head[2],1,ref))continue;
      auto line=fo3npcchat::SelectHello(gPlayerSession->player.Definitions().pipboy,
        gPlayerSession->player,DialogueContext(actor));
      greeted[ref]=now+30;
      if(!line.Valid())continue;
      const uint32_t serial=(gQ230NpcChat.serial+1u)&0xffffu;
      gQ230NpcChat={ref,0x14,line.info,actor.aiPackage,0x20000000u|serial,serial,now};
      nextRemark=now+8;fo3audio::DialogueGain(1.f/std::max(1.f,distance*.5f));
      fo3audio::Dialogue(line.audio,gQ230NpcChat.token);return;
    }
    return;
  }
  if(gQ230NpcChat.listener==0x14){
    const auto* speaker=DialogueActor(gQ230NpcChat.speaker);
    const auto done=fo3audio::DialogueCompletion();
    if(!speaker||!gPlayerSession||gDialogue.Active()||IsFo3LoadingVisible()||Fo3PipboyFocus()||
       speaker->runtime.activity==fo3npc::Activity::Combat||!speaker->runtime.Alive()||
       now-gQ230NpcChat.began>30||(done&&uint32_t(done>>1)==gQ230NpcChat.token))Q230ChatStop("ambient completion/interruption");
    return;
  }
  auto* a=DialogueActor(gQ230NpcChat.speaker);
  auto* b=DialogueActor(gQ230NpcChat.listener);
  if(!gPlayerSession||gPlayerSession->saveBlocked||IsFo3LoadingVisible()||
     Fo3PipboyFocus()||gDialogue.Active()||!a||!b||!a->runtime.dialogue||
     !b->runtime.dialogue||!a->runtime.Alive()||!b->runtime.Alive()||
     !a->runtime.CanTalk()||!b->runtime.CanTalk()||
     a->runtime.activity==fo3npc::Activity::Combat||
     b->runtime.activity==fo3npc::Activity::Combat||a->runtime.offScene||
     b->runtime.offScene){
    Q230ChatStop("actor/state interruption");return;
  }
  const float dx=a->runtime.position[0]-b->runtime.position[0],
              dz=a->runtime.position[2]-b->runtime.position[2];
  if(std::hypot(dx,dz)>fo3npc::DialoguePolicy::LeaveRange||
     now-gQ230NpcChat.began>30.){
    Q230ChatStop("separation/audio timeout");return;
  }
  Q230DialogueResponseAnimation(*a,gQ230AuthoredNpcChat);
  const uint64_t done=fo3audio::DialogueCompletion();
  if(done&&uint32_t(done>>1)==gQ230NpcChat.token){
    if(!(done&1u)){Q230ChatStop("voice unavailable");return;}
    auto context=Q230ChatContext(*a,*b);context.talking=true;
    if(gQ230AuthoredNpcChat.AudioDone(gQ230AuthoredNpcChat.audioToken,true,context,gPlayerSession->player)){
      gQ230NpcChat.token=0x20000000u|((++gQ230NpcChat.serial)&0xffffu);
      fo3audio::Dialogue(gQ230AuthoredNpcChat.Audio(gPlayerSession->player.Definitions().pipboy),gQ230NpcChat.token);return;
    }
    FlushFo3PlayerState();Q230ChatStop("authored audio and INFO results completed");
  }
}
static bool Q230ChatSpeaking(uint32_t reference) {
  return gQ230NpcChat.Active()&&gQ230NpcChat.speaker==reference;
}
static fo3face::Weights Q230ChatWeights(uint32_t reference) {
  fo3face::Weights result{};
  if(!Q230ChatSpeaking(reference))return result;
  auto face=fo3audio::DialogueFace(gQ230NpcChat.token);
  if(face.lip)result=fo3face::Sample(*face.lip,face.seconds);
  return result;
}
static bool Q230ChatLook(uint32_t reference,std::array<float,3>& position) {
  if(!gQ230NpcChat.Active())return false;
  const uint32_t partner=reference==gQ230NpcChat.speaker?gQ230NpcChat.listener:
                         reference==gQ230NpcChat.listener?gQ230NpcChat.speaker:0;
  auto* target=partner?DialogueActor(partner):nullptr;
  if(!target)return false;
  position=target->runtime.position;return true;
}
