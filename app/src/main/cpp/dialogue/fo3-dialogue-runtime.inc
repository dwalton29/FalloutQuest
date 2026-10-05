#include "dialogue/fo3-dialogue-panel.h"
// Render-thread bridge only. Authored graph/conditions/results live in host-testable modules.
fo3dialogue::Session gDialogue;
fo3dialogue::Panel gDialoguePanel;
fo3loot::Cursor gDialogueCursor;
bool gDialoguePreviousA=false,gDialoguePreviousB=false;
double gDialogueTime=0,gDialogueGainTime=0;
uint32_t gDialoguePublishedToken=0,gDialoguePanelInfo=0;
size_t gDialoguePanelResponse=SIZE_MAX;
fo3dialogue::Phase gDialoguePanelPhase=fo3dialogue::Phase::Inactive;
struct DialogueEligibility {uint64_t revision=UINT64_MAX;double until=0;bool valid=false;};
std::unordered_map<uint32_t,DialogueEligibility> gDialogueEligibility;
std::unordered_set<std::string> gDialogueDiagnostics;
bool Fo3DialogueFocus(){return gDialogue.Active();}
Q230ActorVisual* DialogueActor(uint32_t reference) {
  for(auto& actor:gQ230NpcActors)if(actor.source.refFormId==reference)return &actor;
  return nullptr;
}
fo3dialogue::Context DialogueContext(const Q230ActorVisual& actor) {
  fo3dialogue::Context ctx;if(!gPlayerSession)return ctx;
  ctx.player=&gPlayerSession->player;ctx.talking=gDialogue.Active();
  ctx.speaker={actor.source.refFormId,actor.source.baseFormId,gCurrentCellFormId,gExteriorWorldspaceQ1890,
    actor.runtime.package,
    gSceneCenterXQ1730+actor.runtime.position[0]*FO3_UNITS_PER_METRE,
    gSceneCenterYQ1730-(actor.runtime.position[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,
    gSceneFloorZQ1730+(actor.runtime.position[1]-FLOOR_Y)*FO3_UNITS_PER_METRE,
    actor.runtime.activity==fo3npc::Activity::Combat,actor.runtime.packageKnown};
  ctx.target={0x14,7,gCurrentCellFormId,gExteriorWorldspaceQ1890,0,
    gSceneCenterXQ1730+gQ210Head[0]*FO3_UNITS_PER_METRE,
    gSceneCenterYQ1730-(gQ210Head[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,
    gSceneFloorZQ1730+(gQ210Head[1]-FLOOR_Y)*FO3_UNITS_PER_METRE,false,false};
  return ctx;
}
void DialogueDiagnostic(const std::string& message) {
  // Unsupported authored branches are logged once, never every aim frame.
  if(message.find("UNSUPPORTED")!=std::string::npos&&!gDialogueDiagnostics.insert(message).second)return;
  Q6H_LOGI("%s",message.c_str());
}
bool DialogueCanStart(Q230ActorVisual& actor) {
  if(!gPlayerSession||gPlayerSession->saveBlocked||gPlayerSession->player.ActorHealth(actor.source.refFormId)<=0)return false;
  auto& cached=gDialogueEligibility[actor.source.refFormId];
  const auto revision=gPlayerSession->player.Revision();
  if(cached.revision!=revision||cached.until<gDialogueTime) {
    gDialogue.diagnostic=DialogueDiagnostic;cached.valid=gDialogue.CanStart(DialogueContext(actor));cached.revision=revision;cached.until=gDialogueTime+1;
  }
  return cached.valid;
}
void DialoguePublishAudio() {
  if(!gPlayerSession||gDialogue.phase!=fo3dialogue::Phase::Speaking||gDialoguePublishedToken==gDialogue.audioToken)return;
  gDialoguePublishedToken=gDialogue.audioToken;
  auto current=gDialogue.Current(gPlayerSession->player.Definitions().pipboy);
  Q6H_LOGI("DIALOGUE RESPONSE actor=%08X base=%08X dial=%08X info=%08X number=%u voice=%08X token=%u",
    gDialogue.actor,gDialogue.base,gDialogue.topic,gDialogue.info,current&&gDialogue.response<current->responses.size()?current->responses[gDialogue.response].number:0,gDialogue.voice,gDialogue.audioToken);
  fo3audio::Dialogue(gDialogue.Audio(gPlayerSession->player.Definitions().pipboy),gDialogue.audioToken);
}
void CleanupFo3Dialogue(uint32_t previous,const char* reason) {
  if(auto* actor=DialogueActor(previous)){actor->runtime.EndDialogue();Q6H_LOGI("NPC DIALOGUE OVERRIDE actor=%08X restore=1 package=%08X",previous,actor->runtime.package);}
  if(gDialogue.Active())gDialogue.End(reason);gDialoguePanel.visible=false;gDialoguePanel.choices.clear();gDialoguePanel.subtitle.clear();++gDialoguePanel.revision;
  gDialogueCursor.Target(0,0);gDialogueEligibility.clear();gDialoguePanelPhase=fo3dialogue::Phase::Inactive;
  fo3audio::DialogueStop();
  if(previous&&gPlayerSession){fo3audio::RadioState(gPlayerSession->player.Snapshot().pipboy);FlushFo3PlayerState();}
}
void EndFo3Dialogue(const char* reason){CleanupFo3Dialogue(gDialogue.actor,reason);}
bool StartFo3Dialogue(uint32_t reference) {
  if(Fo3PipboyFocus()||Fo3DialogueFocus()||IsFo3LoadingVisible()||!gPlayerSession||gPlayerSession->saveBlocked)return false;
  auto* actor=DialogueActor(reference);if(!actor)return false;
  gDialogue.diagnostic=DialogueDiagnostic;
  if(!gDialogue.Start(DialogueContext(*actor),gPlayerSession->player))return false;
  actor->runtime.BeginDialogue();gDialoguePanel.visible=false;
  Q6H_LOGI("DIALOGUE START actor=%08X base=%08X name=%s dial=%08X info=%08X",reference,actor->source.baseFormId,actor->source.fullName.c_str(),gDialogue.topic,gDialogue.info);
  Q6H_LOGI("DIALOGUE GREETING actor=%08X info=%08X",reference,gDialogue.info);
  Q6H_LOGI("NPC DIALOGUE OVERRIDE actor=%08X begin=1 package=%08X",reference,actor->runtime.package);
  fo3audio::NamedSound("UIMenuOK");DialoguePublishAudio();FlushFo3PlayerState();return true;
}
void UpdateFo3Dialogue(double now,float stick,bool a,bool b,bool focused) {
  const float dt=std::clamp(float(now-gDialogueTime),0.f,.1f);gDialogueTime=now;
  const bool pressA=a&&!gDialoguePreviousA,pressB=b&&!gDialoguePreviousB;
  gDialoguePreviousA=a;gDialoguePreviousB=b;
  if(gDialogue.Active()) {
    auto* actor=DialogueActor(gDialogue.actor);
    std::array<float,3> head{};
    // Actor residency is a scene-lifetime fact. A temporarily unavailable head
    // bone/pose is not an unload signal: newly uploaded or culled actors can
    // legitimately miss one live-bone sample while their stable root remains.
    const bool resident=actor&&!actor->rigs.empty();
    const bool liveHead=resident&&Q230LiveBone(*actor,actor->headBone,head);
    if(resident&&!liveHead){
      std::array<float,3> lo{},hi{};
      if(Q230LiveBounds(*actor,lo,hi))
        head={(lo[0]+hi[0])*.5f,lo[1]+(hi[1]-lo[1])*.82f,(lo[2]+hi[2])*.5f};
      else
        head={actor->runtime.position[0],actor->runtime.position[1]+1.55f,actor->runtime.position[2]};
    }
    // Dialogue interruption must use the actor root, not an animated head
    // sample. Head bones are presentation data and can jump during clip/blend
    // changes; the runtime root is the stable scene-space actor position.
    const float distance=resident?std::sqrt(
      (actor->runtime.position[0]-gQ210Head[0])*(actor->runtime.position[0]-gQ210Head[0])+
      (actor->runtime.position[2]-gQ210Head[2])*(actor->runtime.position[2]-gQ210Head[2])):INFINITY;
    const bool alive=actor&&gPlayerSession&&gPlayerSession->player.ActorHealth(gDialogue.actor)>0;
    const bool combat=actor&&actor->runtime.activity==fo3npc::Activity::Combat;
    if(Fo3PipboyFocus())EndFo3Dialogue("Pip-Boy takes focus");
    else if(IsFo3LoadingVisible())EndFo3Dialogue("loading");
    else if(!resident)EndFo3Dialogue("actor unloaded");
    else if(!alive)EndFo3Dialogue("actor dead");
    else if(combat)EndFo3Dialogue("actor combat");
    else if(!std::isfinite(distance)||distance>fo3npc::DialoguePolicy::LeaveRange)EndFo3Dialogue("walk-away");
    // OpenXR focus can transiently leave FOCUSED while the application remains
    // visible (system UI/headset state). Do not destroy an authored conversation
    // for that presentation-level event; input is already gated by focused below.
    else if(focused&&pressB)EndFo3Dialogue("B exit");
    else {
      auto ctx=DialogueContext(*actor);
      const auto complete=fo3audio::DialogueCompletion();
      if(complete){gDialogue.AudioDone(uint32_t(complete>>1),(complete&1)!=0,ctx,gPlayerSession->player);if(!gDialogue.Active()){CleanupFo3Dialogue(actor->source.refFormId,"voice/goodbye complete");}}
      if(gDialogue.Active()) {
        if(gDialogue.phase==fo3dialogue::Phase::Choices&&(gDialoguePanelPhase!=gDialogue.phase||gDialoguePanelInfo!=gDialogue.info))gDialogueCursor.Target(0,0);
        gDialogueCursor.Target(gDialogue.actor,gDialogue.choices.size());
        if(gDialogue.phase==fo3dialogue::Phase::Choices) {
          const auto before=gDialogueCursor.selected;gDialogueCursor.Scroll(stick,now,gDialogue.choices.size());
          if(before!=gDialogueCursor.selected)fo3audio::Scroll();gDialogue.selected=gDialogueCursor.selected;
          if(focused&&pressA&&gDialogue.Choose(gDialogue.selected,ctx,gPlayerSession->player)){fo3audio::NamedSound("UIMenuOK");FlushFo3PlayerState();}
        }
        if(!gDialogue.Active())CleanupFo3Dialogue(actor->source.refFormId,"choice result rejected");
        else {
        DialoguePublishAudio();
        const bool first=!gDialoguePanel.visible;
        const float dx=gQ210Head[0]-head[0],dz=gQ210Head[2]-head[2];const float len=std::max(.01f,std::sqrt(dx*dx+dz*dz));
        const std::array<float,3> anchor{head[0]+dx/len*fo3npc::DialoguePolicy::PanelForward,head[1]-fo3npc::DialoguePolicy::PanelBelowHead,head[2]+dz/len*fo3npc::DialoguePolicy::PanelForward};
        fo3dialogue::SmoothAnchor(gDialoguePanel,anchor,dt,first);
        const float desired=std::atan2(dx,dz);
        gDialoguePanel.yaw=first?desired:gDialoguePanel.yaw+fo3npc::Angle(desired-gDialoguePanel.yaw)*(1-std::exp(-dt/.25f));
        gDialoguePanel.visible=true;
        if(gDialoguePanelInfo!=gDialogue.info||gDialoguePanelResponse!=gDialogue.response||gDialoguePanelPhase!=gDialogue.phase||first) {
          gDialoguePanelInfo=gDialogue.info;gDialoguePanelResponse=gDialogue.response;gDialoguePanelPhase=gDialogue.phase;
          gDialoguePanel.title=actor->source.fullName;gDialoguePanel.subtitle=gDialogue.Subtitle(gPlayerSession->player.Definitions().pipboy);
          gDialoguePanel.choices.clear();for(const auto& choice:gDialogue.choices)gDialoguePanel.choices.push_back(choice.text);++gDialoguePanel.revision;
          fo3audio::RadioState(gPlayerSession->player.Snapshot().pipboy);FlushFo3PlayerState();
        }
        gDialoguePanel.selected=gDialogue.selected;
        if(now>=gDialogueGainTime){fo3audio::DialogueGain(1.f/std::max(1.f,distance*.5f));gDialogueGainTime=now+.1;}
        }
      }
    }
  }
  for(auto& actor:gQ230NpcActors) {
    if(actor.runtime.activity!=fo3npc::Activity::Combat&&(actor.runtime.dialogue||std::fabs(actor.runtime.headYaw)>.01f||std::fabs(fo3npc::Angle(actor.runtime.yaw-actor.runtime.returnYaw))>.01f)) {
      // Humanoid authored forward is +Y game space; scene Z is -game Y.
      const auto& rig=actor.rigs;const float x=rig.empty()?0:rig[0].scenePlacement[12],z=rig.empty()?0:rig[0].scenePlacement[14];
      const float desired=std::atan2(-(gQ210Head[0]-x),-(gQ210Head[2]-z));
      actor.runtime.Face(desired,dt);actor.lastFrame=UINT64_MAX;Q230UpdateActor(actor);
    }
  }
}
