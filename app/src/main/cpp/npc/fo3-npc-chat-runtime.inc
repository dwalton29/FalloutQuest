// A single resident NPC-to-NPC HELLO line at a time. Uses the existing
// dialogue voice channel only when the player dialogue owns nothing.
// No inferred quest results, generated words or synthetic animation assets.
namespace {
struct Q230NpcChatSession {
  uint32_t speaker=0,listener=0,info=0,package=0,token=0,serial=0;
  double began=0;
  bool Active()const{return speaker&&listener&&token;}
};
Q230NpcChatSession gQ230NpcChat;
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
static bool Q230ChatEligible(const Q230ActorVisual& initiator,uint32_t reference) {
  if(!gPlayerSession||gPlayerSession->saveBlocked||gDialogue.Active()||
     gQ230NpcChat.Active()||IsFo3LoadingVisible()||Fo3PipboyFocus())return false;
  const auto* partner=DialogueActor(reference);
  if(!partner||partner->runtime.offScene||!partner->runtime.CanTalk()||
     partner->runtime.dialogue||partner->runtime.furniture.Active()||
     partner->runtime.activity==fo3npc::Activity::Combat||
     !initiator.runtime.CanTalk()||initiator.runtime.furniture.Active()||
     partner->rigs.empty()||initiator.rigs.empty())return false;
  return fo3npcchat::SelectHello(gPlayerSession->player.Definitions().pipboy,
    gPlayerSession->player,Q230ChatContext(initiator,*partner)).Valid();
}
static void Q230ChatStop(const char* reason) {
  if(!gQ230NpcChat.Active())return;
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
  auto chosen=fo3npcchat::SelectHello(gPlayerSession->player.Definitions().pipboy,
    gPlayerSession->player,Q230ChatContext(*a,*b));
  if(!chosen.Valid())return false;
  const uint32_t serial=(gQ230NpcChat.serial+1u)&0xffffu;
  const uint32_t token=0x20000000u|serial;
  if(!a->runtime.CanTalk()||!b->runtime.CanTalk())return false;
  a->runtime.BeginDialogue();b->runtime.BeginDialogue();
  if(!a->runtime.dialogue||!b->runtime.dialogue){
    a->runtime.EndDialogue();b->runtime.EndDialogue();return false;
  }
  a->runtime.speaking=true;b->runtime.speaking=false;
  a->lastFrame=b->lastFrame=UINT64_MAX;
  gQ230NpcChat={speaker,listener,chosen.info,a->aiPackage,token,serial,now};
  Q6H_LOGI("NPC CHAT BEGIN speaker=%08X listener=%08X package=%08X info=%08X token=%u text=%s",
    speaker,listener,a->aiPackage,chosen.info,token,chosen.subtitle.c_str());
  fo3audio::DialogueGain(.7f);
  fo3audio::Dialogue(chosen.audio,token);
  return true;
}
static void Q230ChatTick(double now) {
  if(!gQ230NpcChat.Active())return;
  const auto* a=DialogueActor(gQ230NpcChat.speaker);
  const auto* b=DialogueActor(gQ230NpcChat.listener);
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
  const uint64_t done=fo3audio::DialogueCompletion();
  if(done&&uint32_t(done>>1)==gQ230NpcChat.token){
    Q230ChatStop((done&1u)?"authored audio completed":"voice unavailable");
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
