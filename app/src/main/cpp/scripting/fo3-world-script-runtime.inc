#include "ui/interaction/fo3-item-notifications.h"
#include "dialogue/fo3-dialogue-selection.h"
#include "dialogue/fo3-dialogue-panel.h"
#include "audio/fo3-animation-sounds.h"
// Render-thread adapters for original reference events and controller actions.
// No location, quest, reference, sound or animation-group IDs are embedded here.
namespace fo3worldscript {
bool PackageEvent(uint32_t reference,uint32_t package,bool completed);
struct Animation {
    std::shared_ptr<const fo3dooranim::Asset> asset;
    int clip=-1;double started=0,lastCollision=-1,lastSound=-1,presentationTime=0;bool settled=false;
    fo3anim::Pose pose;
    std::vector<bool> visible;
    std::vector<size_t> shapes;
};
static std::unordered_map<uint32_t,Animation> animations;
static std::unordered_set<uint32_t> loaded;
struct LookTarget { uint32_t reference=0; bool rotateBody=false; };
static std::unordered_map<uint32_t,LookTarget> lookTargets;
static fo3script::ImageStack modifiers;
static double seconds=0;
struct SpeechLane {
    fo3dialogue::Session dialogue;
    uint32_t reference=0,identity=0,target=0,listener=0,topic=0,token=0;
    uint32_t package=0,packageOwner=0;
    std::pair<uint32_t,uint32_t> pendingPackageDone{};
    double began=0;
    bool subtitle=false,noTargetLook=false,noHeadTracking=false;
    bool paused=false;
    uint32_t pausedQuest=0;
    uint16_t pausedStage=0;
};
static std::array<SpeechLane,2> speechLanes;
uint32_t SpeechLookTarget(uint32_t actor){
    for(const auto& lane:speechLanes)
        if(!lane.paused&&lane.dialogue.phase==fo3dialogue::Phase::Speaking&&!lane.noHeadTracking&&lane.reference==actor&&lane.reference==lane.identity)
            return lane.target==actor?0:lane.target;
    for(const auto& lane:speechLanes)
        if(!lane.paused&&lane.dialogue.phase==fo3dialogue::Phase::Speaking&&!lane.noHeadTracking&&!lane.noTargetLook&&lane.target==actor&&lane.target==lane.listener&&lane.reference!=actor)
            return lane.reference;
    return 0;
}
static fo3dialogue::Panel scriptSubtitle;
static std::vector<fo3script::Action> speechPending,hitPending;
static uint32_t speechSerial=0;
static uint64_t speechGeneration=~uint64_t{0};
bool OwnsSpeechResultAdmission(bool admitting,uint64_t ticket){
    return std::any_of(speechLanes.begin(),speechLanes.end(),[&](const auto& lane){
        return lane.dialogue.phase==fo3dialogue::Phase::Results&&
            (admitting||(ticket&&lane.dialogue.resultTicket==ticket));
    });
}
static fo3dialogue::Context SpeechContext(uint32_t reference,uint32_t target){
    auto& player=gPlayerSession->player;
    fo3dialogue::Context ctx;ctx.player=&player;ctx.talking=true;
    fo3dialogue::ReferenceContext(player,reference,ctx.speaker);
    if(auto* actor=ctx.speaker.proxy?nullptr:DialogueActor(reference)){
        const auto pos=Q240GamePosition(*actor);
        ctx.speaker={reference,actor->source.baseFormId,gCurrentCellFormId,gExteriorWorldspaceQ1890,actor->aiPackage,pos[0],pos[1],pos[2],actor->runtime.activity==fo3npc::Activity::Combat,actor->runtime.packageKnown};
    }
    fo3dialogue::ReferenceContext(gPlayerSession->player,target,ctx.target);
    if(target==fo3player::PlayerRef){ctx.target={target,7,gCurrentCellFormId,gExteriorWorldspaceQ1890,0,
        gSceneCenterXQ1730+gQ210Head[0]*FO3_UNITS_PER_METRE,
        gSceneCenterYQ1730-(gQ210Head[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,
        gSceneFloorZQ1730+(gQ210Head[1]-FLOOR_Y)*FO3_UNITS_PER_METRE,false,false};
    }else if(const auto* actor=ctx.target.proxy?nullptr:DialogueActor(target)){const auto pos=Q240GamePosition(*actor);
      ctx.target={target,actor->source.baseFormId,gCurrentCellFormId,gExteriorWorldspaceQ1890,actor->aiPackage,pos[0],pos[1],pos[2],actor->runtime.activity==fo3npc::Activity::Combat,actor->runtime.packageKnown};
    }
    return ctx;
}
static void SpeechStop(SpeechLane& lane,unsigned index){
    if(gPlayerSession){
        gPlayerSession->player.SetScriptActorTalking(lane.reference,false);
        gPlayerSession->player.SetScriptActorTalking(lane.identity,false);
    }
    if(auto* actor=DialogueActor(lane.reference)){
        actor->runtime.speaking=false;actor->runtime.scriptSpeechToken=0;actor->lastFrame=UINT64_MAX;
    }
    if(lane.dialogue.Active()&&(index||!gDialogue.Active()))fo3audio::StopDialogueLane(index);
    lane.dialogue.End("scripted speech stopped");
    lane.reference=lane.identity=lane.target=lane.listener=lane.topic=lane.token=0;
    lane.package=lane.packageOwner=0;lane.subtitle=lane.noTargetLook=lane.noHeadTracking=false;
    lane.paused=false;lane.pausedQuest=lane.pausedStage=0;
}
bool AllowsPlayerDialogue(){
    return std::none_of(speechLanes.begin(),speechLanes.end(),[](const auto& lane){
        return lane.dialogue.Active()&&(lane.target==fo3player::PlayerRef||lane.dialogue.phase==fo3dialogue::Phase::Results);
    });
}
static uint16_t SpeechQuestStage(const fo3player::Player& player,uint32_t quest){
    const auto found=player.Snapshot().pipboy.quests.find(quest);
    return found==player.Snapshot().pipboy.quests.end()?0:found->second.stage;
}
void PauseForPlayerDialogue(){
    if(!gPlayerSession)return;
    auto& player=gPlayerSession->player;
    for(unsigned index=0;index<speechLanes.size();++index){
        auto& lane=speechLanes[index];
        if(lane.paused||lane.dialogue.phase!=fo3dialogue::Phase::Speaking)continue;
        const auto* info=lane.dialogue.Current(player.Definitions().pipboy);
        lane.pausedQuest=info?info->quest:0;
        lane.pausedStage=SpeechQuestStage(player,lane.pausedQuest);
        lane.paused=true;
        player.SetScriptActorTalking(lane.reference,false);
        player.SetScriptActorTalking(lane.identity,false);
        if(auto* actor=DialogueActor(lane.reference)){
            actor->runtime.speaking=false;actor->runtime.scriptSpeechToken=0;actor->lastFrame=UINT64_MAX;
        }
        fo3audio::StopDialogueLane(index);
    }
    scriptSubtitle.visible=false;
}

static uint64_t runtimeGeneration=~uint64_t{0};
static uint64_t registryRevision=~uint64_t{0},actorRegistryRevision=~uint64_t{0};
static const GpuObject* registryStorage=nullptr;
static size_t registryCount=0;
static const fo3player::Player* registryPlayer=nullptr;

bool Event(uint32_t reference,const char* event,uint32_t activator,bool persist=true){
    auto& player=gPlayerSession->player;
    if(!player.HasReferenceEvent(reference,event))return true;
    std::string error;
    auto plan=player.PrepareReferenceEventTransaction(reference,event,activator,error,fo3player::ScriptProgramCapture::EnteredPrograms);
    const bool invalidProgram=!plan;
    bool committed=false;
    if(plan){
        if(!plan->HasReferenceChanges())committed=player.CommitScriptTransaction(*plan);
        else committed=PublishFo3DialogueReferences(player,*plan,error);
        if(!committed&&plan->HasReferenceChanges()){
            const auto ticket=BeginFo3ResidentPreparedWorkflow({},std::move(plan));
            if(ticket){gFo3ResidentGameplayTicket=ticket;FlushFo3PlayerState();return false;}
        }
    }
    if(!committed){
        static std::unordered_set<std::string> reported;
        const auto key=std::to_string(reference)+event+error;
        if(reported.size()<256&&reported.insert(key).second)
            Q6H_LOGW("SCRIPT WORLD EVENT REJECTED: ref=%08X event=%s reason=%s",reference,event,error.c_str());
    }else if(persist)FlushFo3PlayerState();
    return committed||invalidProgram;
}
static void SpeechPlay(SpeechLane& lane,unsigned index,fo3player::Player& player){
    lane.began=seconds;
    lane.token=0x30000000u|(++speechSerial&0xffffffu);
    if(auto* actor=lane.dialogue.proxy?nullptr:DialogueActor(lane.reference)){
        actor->runtime.speaking=true;actor->runtime.scriptSpeechToken=lane.token;
        actor->lastFrame=UINT64_MAX;
    }
    fo3audio::ScriptDialogue(lane.dialogue.Audio(player.Definitions().pipboy),
        lane.token,index,lane.dialogue.proxy!=0);
}
static void AdvanceSpeech(SpeechLane& lane,unsigned index,fo3player::Player& player){
    if(lane.pendingPackageDone.first){
        if(!PackageEvent(lane.pendingPackageDone.first,lane.pendingPackageDone.second,true))return;
        lane.pendingPackageDone={};
    }
    auto& speech=lane.dialogue;
    if(!speech.Active())return;
    if(auto* actor=speech.proxy?nullptr:DialogueActor(lane.reference))
        Q230DialogueResponseAnimation(*actor,speech);
    bool disabled=true;
    std::string error;
    if(IsFo3LoadingVisible()||
       !player.ReferenceDisabled(lane.reference,disabled,error)||disabled||
       (lane.identity!=lane.reference&&(player.TalkingActivatorActor(lane.reference)!=lane.identity||player.ActorHealth(lane.identity)<=0))||
       (lane.listener!=lane.target&&(player.TalkingActivatorActor(lane.target)!=lane.listener||player.ActorHealth(lane.listener)<=0))||
       (!lane.paused&&seconds-lane.began>60)){
        SpeechStop(lane,index);
        return;
    }
    if(lane.paused){
        // Player dialogue can resolve the same scene. Do not replay an
        // interrupted line from a previous stage or execute its old results.
        if(lane.pausedQuest&&SpeechQuestStage(player,lane.pausedQuest)!=lane.pausedStage){
            SpeechStop(lane,index);
            return;
        }
        lane.paused=false;
        player.SetScriptActorTalking(lane.reference,true);
        player.SetScriptActorTalking(lane.identity,true);
        SpeechPlay(lane,index,player);
        return;
    }

    const uint32_t actor=speech.actor,topic=lane.topic;
    bool advanced=false;
    if(speech.phase==fo3dialogue::Phase::Results){
        advanced=speech.ResumeResults(SpeechContext(lane.reference,lane.target),player);
    }else{
        const auto done=fo3audio::TakeDialogueLane(index);
        if(!done||uint32_t(done>>1)!=lane.token)return;
        const bool heard=(done&1u)!=0;
        if(!heard)Q6H_LOGW("SCRIPT SPEECH VOICE UNAVAILABLE ref=%08X topic=%08X",actor,topic);
        advanced=speech.AudioDone(speech.audioToken,heard,SpeechContext(lane.reference,lane.target),player);
    }
    if(speech.phase==fo3dialogue::Phase::Results)return;
    if(advanced){
        const uint32_t endpoint=speech.proxy?speech.proxy:speech.actor;
        if(endpoint!=lane.reference||speech.actor!=lane.identity){
            player.SetScriptActorTalking(lane.reference,false);
            player.SetScriptActorTalking(lane.identity,false);
            if(auto* previous=DialogueActor(lane.reference)){
                previous->runtime.speaking=false;previous->runtime.scriptSpeechToken=0;
            }
            lane.target=lane.reference;lane.listener=lane.identity;
            lane.reference=endpoint;lane.identity=speech.actor;
            player.SetScriptActorTalking(lane.reference,true);
            player.SetScriptActorTalking(lane.identity,true);
        }
        SpeechPlay(lane,index,player);
        return;
    }
    const bool completed=speech.infoCompleted;
    const auto package=lane.package,owner=lane.packageOwner;
    SpeechStop(lane,index);
    if(!completed)return;
    if(package){
        if(!PackageEvent(owner,package,true))lane.pendingPackageDone={owner,package};
    }else Event(actor,"SayToDone",topic);
    FlushFo3PlayerState();
}
static void StartSpeech(SpeechLane& lane,unsigned index,const fo3script::Action& action,
                        fo3player::Player& player){
    bool disabled=true;
    std::string error;
    if(!player.ReferenceDisabled(action.reference,disabled,error)||disabled)return;
    const auto admission=SpeechContext(action.reference,action.flags);
    if((action.sourceActor&&admission.speaker.reference!=action.sourceActor)||
       (action.targetActor&&admission.target.reference!=action.targetActor))return;
    const bool conversation=action.kind==fo3script::ActionKind::Conversation;
    if(const auto* actor=admission.speaker.proxy?nullptr:DialogueActor(action.reference)){
        if(actor->runtime.offScene||!actor->runtime.Alive()||
           (conversation&&!actor->runtime.CanTalk()))return;
    }
    auto& speech=lane.dialogue;
    ConfigureFo3DialogueSession(speech);
    const bool started=action.info?
        speech.StartSelectedTopic(action.form,action.info,admission,player,conversation):
        speech.StartTopic(action.form,admission,player);
    if(!started){
        Q6H_LOGW("SCRIPT SPEECH TOPIC UNAVAILABLE ref=%08X topic=%08X",action.reference,action.form);
        return;
    }
    lane.reference=action.reference;lane.identity=speech.actor;lane.target=action.flags;
    lane.listener=admission.target.reference;lane.topic=action.form;
    lane.package=action.package;lane.packageOwner=action.reference;lane.subtitle=action.forceSubtitle;
    lane.noTargetLook=action.noTargetLook;
    const auto& packages=player.Definitions().pipboy.packages;
    const auto package=packages.find(action.package);
    lane.noHeadTracking=package!=packages.end()&&(package->second.dialogueFlags&1u);
    player.SetScriptActorTalking(lane.reference,true);
    player.SetScriptActorTalking(lane.identity,true);
    fo3audio::DialogueGain(.7f);
    SpeechPlay(lane,index,player);
}
static void UpdateSpeechSubtitle(const fo3pipdata::Definitions& definitions){
    std::string text;
    for(const auto& lane:speechLanes)if(lane.subtitle&&lane.dialogue.Active()){
        const auto line=lane.dialogue.Subtitle(definitions);
        if(!line.empty()){
            if(!text.empty())text+="\n";
            text+=line;
        }
    }
    if(!text.empty()&&!scriptSubtitle.visible){
        const float yaw=gQ210Head[3];
        scriptSubtitle.yaw=yaw;
        scriptSubtitle.anchor={gQ210Head[0]-std::sin(yaw)*1.5f,gQ210Head[1]-.25f,
                               gQ210Head[2]-std::cos(yaw)*1.5f};
    }
    scriptSubtitle.visible=!text.empty();
    if(text!=scriptSubtitle.subtitle){scriptSubtitle.subtitle=std::move(text);++scriptSubtitle.revision;}
}
void ResetSpeech(){
    for(unsigned i=0;i<speechLanes.size();++i){
        SpeechStop(speechLanes[i],i);
        speechLanes[i].pendingPackageDone={};
    }
    speechPending.clear();
    scriptSubtitle={};
}
void SpeechTick(){
    if(!gPlayerSession)return;
    auto& player=gPlayerSession->player;
    if(speechGeneration!=player.ScriptRuntimeGeneration()){
        ResetSpeech();
        speechGeneration=player.ScriptRuntimeGeneration();
    }
    if(gDialogue.Active())return;
    for(unsigned i=0;i<speechLanes.size();++i) {
        AdvanceSpeech(speechLanes[i],i,player);
        if(speechLanes[i].dialogue.resultTicket)break;
    }
    if(!speechPending.empty()&&!gDialogue.Active()&&!gQ230NpcChat.Active()&&
       !IsFo3LoadingVisible()&&CanDeferFo3DialogueResult()){
        const auto& next=speechPending.front();
        const bool occupied=std::any_of(speechLanes.begin(),speechLanes.end(),
            [](const auto& lane){return lane.dialogue.Active();});
        const bool linked=std::any_of(speechLanes.begin(),speechLanes.end(),
            [](const auto& lane){return lane.dialogue.Active()&&lane.dialogue.linkedConversation;});
        if(!linked&&(!occupied||next.kind!=fo3script::ActionKind::Conversation)&&
           !player.ScriptActorTalking(next.reference)&&
           (!next.sourceActor||!player.ScriptActorTalking(next.sourceActor))){
            for(unsigned i=0;i<speechLanes.size();++i){
                auto& lane=speechLanes[i];
                if(lane.dialogue.Active()||lane.pendingPackageDone.first)continue;
                const auto action=next;
                speechPending.erase(speechPending.begin());
                StartSpeech(lane,i,action,player);
                break;
            }
        }
    }
    UpdateSpeechSubtitle(player.Definitions().pipboy);
}
void Register(){
    auto& player=gPlayerSession->player;
    const uint64_t revision=gFo3VisualPublication.Revision();
    if(actorRegistryRevision!=gQ230NpcResidenceRevision||registryPlayer!=&player||runtimeGeneration!=player.ScriptRuntimeGeneration())player.ClearScriptActorFurniture();
    for(const auto& actor:gQ230NpcActors){
        if(actor.runtime.offScene){player.SetScriptActorFurniture(actor.source.refFormId,0,0,255);continue;}
        const auto& furniture=actor.runtime.furniture;const auto* slot=furniture.Selected();
        uint8_t sitting=0;uint32_t ref=0;uint8_t marker=0;
        if(slot&&!slot->idleMarker&&furniture.phase!=fo3furniture::Phase::None&&furniture.phase!=fo3furniture::Phase::Approach){
            ref=slot->reference;marker=slot->marker.reference;
            if(!slot->sleep)sitting=furniture.phase==fo3furniture::Phase::Enter?2:furniture.phase==fo3furniture::Phase::Loop?3:4;
        }
        player.SetScriptActorFurniture(actor.source.refFormId,ref,marker,sitting);
    }
    if(actorRegistryRevision==gQ230NpcResidenceRevision&&registryPlayer==&player&&runtimeGeneration==player.ScriptRuntimeGeneration()&&registryRevision==revision&&registryStorage==gObjects.data()&&registryCount==gObjects.size())return;
    if(registryPlayer!=&player||runtimeGeneration!=player.ScriptRuntimeGeneration()){animations.clear();loaded.clear();lookTargets.clear();hitPending.clear();modifiers.entries.clear();fo3bloodui::Clear();seconds=0;for(auto& actor:gQ230NpcActors){actor.runtime.scriptLookTarget=actor.runtime.scriptSpeechToken=0;actor.runtime.scriptLookRotateBody=false;actor.responseIdle=actor.responseToken=actor.responseInfo=0;}}
    actorRegistryRevision=gQ230NpcResidenceRevision;runtimeGeneration=player.ScriptRuntimeGeneration();registryPlayer=&player;registryRevision=revision;registryStorage=gObjects.data();registryCount=gObjects.size();
    auto registry=std::make_shared<fo3script::AnimationRegistry>();
    std::unordered_set<uint32_t> present;
    for(const auto& object:gObjects){
        if(object.refFormId)present.insert(object.refFormId);
        if(!object.scriptAnimated||!object.q2401DoorAnimation)continue;
        const auto& asset=object.q2401DoorAnimation;
        if(registry->count(object.refFormId))continue;
        auto& definitions=(*registry)[object.refFormId];
        for(const auto& clip:asset->clips)definitions.push_back({clip.name,clip.frequency>0?std::max(0.,double(clip.stop-clip.start)/clip.frequency):0,clip.cycle==0||clip.cycle==1});
        auto& state=animations[object.refFormId];
        if(state.asset!=asset){state={};state.asset=asset;state.visible.resize(asset->hierarchy.bones.size());
            for(size_t i=0;i<state.visible.size();++i)state.visible[i]=asset->hierarchy.bones[i].visible;}
        state.settled=false; // Reapply retained pose to newly published shapes.
    }
    for(auto& entry:animations)entry.second.shapes.clear();
    for(size_t i=0;i<gObjects.size();++i)if(gObjects[i].scriptAnimated){
        const auto state=animations.find(gObjects[i].refFormId);
        if(state!=animations.end())state->second.shapes.push_back(i);
    }
    for(const auto& actor:gQ230NpcActors)if(!actor.runtime.offScene)present.insert(actor.source.refFormId);
    for(auto i=animations.begin();i!=animations.end();)if(!present.count(i->first))i=animations.erase(i);else ++i;
    for(auto i=loaded.begin();i!=loaded.end();)if(!present.count(*i))i=loaded.erase(i);else ++i;
    for(auto& actor:gQ230NpcActors){
        const auto target=lookTargets.find(actor.source.refFormId);
        if(target==lookTargets.end())continue;
        actor.runtime.scriptLookTarget=target->second.reference;
        actor.runtime.scriptLookRotateBody=target->second.rotateBody;
    }
    player.SetScriptAnimationRegistry(std::move(registry));
}
void Hits(){
    if(!gPlayerSession||Fo3ResidentFrameBlocked())return;
    auto& player=gPlayerSession->player;
    while(!hitPending.empty()){
        const auto hit=hitPending.front();bool disabled=true;std::string error;
        if(!player.ReferenceDisabled(hit.reference,disabled,error)||disabled){hitPending.erase(hitPending.begin());continue;}
        auto plan=player.PrepareWeaponHitTransaction(hit.reference,hit.flags,hit.form,error,fo3player::ScriptProgramCapture::EnteredPrograms);
        if(!plan){Q6H_LOGW("SCRIPT WEAPON HIT REJECTED ref=%08X weapon=%08X reason=%s",hit.reference,hit.form,error.c_str());hitPending.erase(hitPending.begin());continue;}
        bool committed=!plan->HasReferenceChanges()?player.CommitScriptTransaction(*plan):PublishFo3DialogueReferences(player,*plan,error);
        if(!committed&&plan->HasReferenceChanges()){
            if(!CanDeferFo3DialogueResult())return;
            const auto ticket=BeginFo3ResidentPreparedWorkflow({},std::move(plan));
            if(!ticket)return;
            gFo3ResidentGameplayTicket=ticket;hitPending.erase(hitPending.begin());return;
        }
        if(!committed)return;
        hitPending.erase(hitPending.begin());
    }
}
void Events(){
    if(!gPlayerSession||Fo3ResidentFrameBlocked())return;
    auto& player=gPlayerSession->player;
    // Trigger volumes are ESM primitives, and need no visible mesh residency.
    for(const auto& change:player.ScriptTriggerChanges()){
        if(Event(change.first,change.second?"OnTriggerEnter":"OnTriggerLeave",fo3player::PlayerRef))
            player.AcknowledgeScriptTrigger(change.first,change.second);
        if(Fo3ResidentFrameBlocked())return;
    }
    struct ActorTriggerChange {uint32_t actor,volume;bool entering;};
    std::vector<ActorTriggerChange> actorChanges;
    for(const auto& actor:gQ230NpcActors){
        if(actor.runtime.offScene||!actor.runtime.Alive())continue;
        const auto id=actor.source.refFormId;
        for(const auto& change:player.ScriptActorTriggerChanges(
                id,gCurrentCellFormId,gExteriorWorldspaceQ1890,Q240GamePosition(actor)))
            actorChanges.push_back({id,change.first,change.second});
    }
    for(const auto& change:actorChanges){
        if(Event(change.volume,change.entering?"OnTriggerEnter":"OnTriggerLeave",change.actor))
            player.AcknowledgeScriptTrigger(change.volume,change.entering,change.actor);
        if(Fo3ResidentFrameBlocked())return;
    }
    std::vector<uint32_t> refs;
    for(const auto& object:gObjects)if(object.refFormId&&!loaded.count(object.refFormId))refs.push_back(object.refFormId);
    for(const auto& actor:gQ230NpcActors)if(!actor.runtime.offScene&&!loaded.count(actor.source.refFormId))refs.push_back(actor.source.refFormId);
    std::sort(refs.begin(),refs.end());refs.erase(std::unique(refs.begin(),refs.end()),refs.end());
    for(uint32_t id:refs){if(Event(id,"OnLoad",0))loaded.insert(id);if(Fo3ResidentFrameBlocked())return;}
}
void TriggerTick(){
    if(!gPlayerSession||Fo3ResidentFrameBlocked())return;
    // Only acknowledged, still-overlapping volumes run continuously. Source
    // changes use the normal throttled script-save path, not a save per frame.
    for(uint32_t reference:gPlayerSession->player.ScriptOccupiedTriggers()){
        Event(reference,"OnTrigger",fo3player::PlayerRef,false);
        if(Fo3ResidentFrameBlocked())return;
    }
}
void Actions(){
    Hits();if(Fo3ResidentFrameBlocked())return;
    auto& player=gPlayerSession->player;fo3script::Action action;
    SetFo3PlayerScale(std::clamp(player.PlayerTrackingScale(),.1f,10.f));
    const auto& move=player.Snapshot().playerMove;
    if(move.pending&&!IsFo3LoadingVisible()&&!HasPendingFo3CellTransitionRequestQ74())
        QueueFo3MapMarkerTransitionQ74(move.marker,move.position[0],move.position[1],move.position[2],move.rotation[0],move.rotation[1],move.rotation[2],0);
    fo3loading::gLocationSpecificOnly.store(player.Snapshot().locationSpecificLoadScreens);
    while(player.PollScriptAction(action)){
        if(action.kind==fo3script::ActionKind::StopCombat){
            if(action.flags&1){for(auto& actor:gQ230NpcActors)if(actor.runtime.combatTarget==action.reference)Q230StopScriptCombat(actor,seconds);}
            else if(auto* actor=DialogueActor(action.reference))Q230StopScriptCombat(*actor,seconds);
        }else if(action.kind==fo3script::ActionKind::PipboyReset){ResetFo3Pipboy();}
        else if(action.kind==fo3script::ActionKind::RadioRefresh){RefreshFo3PipboyRadio();}
        else if(action.kind==fo3script::ActionKind::RadioConversation){
            bool disabled=false;std::string error;
            if(player.ReferenceDisabled(action.reference,disabled,error)&&!disabled)
                fo3audio::RadioConversation(action.reference,action.form,player.Snapshot().pipboy);
        }
        else if(action.kind==fo3script::ActionKind::PackageIdle){
            if(auto* actor=DialogueActor(action.reference))Q230PlayPackageIdle(*actor,action.form,action.flags);
        }else if(action.kind==fo3script::ActionKind::KillQuestUpdates){
            fo3notify::Notifications().ClearQuests();
        }else if(action.kind==fo3script::ActionKind::AutoSave){
            FlushFo3PlayerState();
        }else if(action.kind==fo3script::ActionKind::Look){
            const LookTarget target{action.form,action.form!=0&&action.flags!=0};
            if(action.form)lookTargets[action.reference]=target;else lookTargets.erase(action.reference);
            if(auto* actor=DialogueActor(action.reference)){
                actor->runtime.scriptLookTarget=target.reference;
                actor->runtime.scriptLookRotateBody=target.rotateBody;
            }
        }else if((action.kind==fo3script::ActionKind::SayTo||action.kind==fo3script::ActionKind::Conversation)){if(speechPending.size()<128)speechPending.push_back(action);
        }else if(action.kind==fo3script::ActionKind::WeaponHit){
            if(hitPending.size()<256)hitPending.push_back(action);
            else Q6H_LOGW("SCRIPT WEAPON HIT QUEUE FULL ref=%08X",action.reference);
            Hits();if(Fo3ResidentFrameBlocked())return;
        }else if(action.kind==fo3script::ActionKind::DefaultActivate){
            if(!DeliverFo3ScriptDefaultActivation(action.reference,action.flags))
                Q6H_LOGW("SCRIPT DEFAULT ACTIVATE REJECTED: ref=%08X",action.reference);
        }else if(action.kind==fo3script::ActionKind::SoundAt){
            const auto source=SpeechContext(action.reference,fo3player::PlayerRef);
            const auto& emitter=source.speaker;const auto& listener=source.target;
            bool disabled=false;std::string error;
            if(action.reference!=fo3player::PlayerRef&&
               (!player.ReferenceDisabled(action.reference,disabled,error)||disabled))continue;
            if(!emitter.cell||emitter.world!=listener.world||(!emitter.world&&emitter.cell!=listener.cell))continue;
            const float dx=emitter.x-listener.x,dy=emitter.y-listener.y,dz=emitter.z-listener.z;
            const float horizontal=std::hypot(dx,dy),yaw=gQ210Head[3];
            const float pan=horizontal>0?(dx*std::cos(yaw)+dy*std::sin(yaw))/horizontal:0;
            fo3audio::SoundAt(action.form,std::hypot(horizontal,dz),pan);
        }else if(action.kind==fo3script::ActionKind::ImageModifier){
            const auto i=player.Definitions().imageModifiers.find(action.form);
            if(i!=player.Definitions().imageModifiers.end()){modifiers.Apply(action.form,i->second,seconds);}
        }else if(action.kind==fo3script::ActionKind::RemoveImageModifier){modifiers.Remove(action.form);
        }else if(action.kind==fo3script::ActionKind::ScreenBlood){fo3bloodui::Trigger(action.flags,seconds,player.Definitions());
        }else if(action.kind==fo3script::ActionKind::PlayGroup){
            const auto i=animations.find(action.reference);if(i==animations.end())continue;
            auto& state=i->second;
            for(size_t c=0;c<state.asset->clips.size();++c)if(fo3dooranim::EqualName(state.asset->clips[c].name,action.group.c_str())){
                state.clip=int(c);state.started=action.started;state.lastCollision=-1;state.lastSound=-1;state.presentationTime=0;state.settled=false;
                fo3anim::BindClip(state.asset->hierarchy,state.asset->clips[c],state.pose);break;
            }
        }
    }
}
// A rendered menu freezes the game clock. Only its own physical display may
// finish opening on presentation time. A restored menu reposes its display
// from the authored endpoint when no transient playback survived the save.
void AdvanceMenuDisplay(uint32_t reference,double dt){
    const auto found=animations.find(reference);
    if(found==animations.end())return;
    auto& state=found->second;
    if(state.clip<0){
        for(size_t i=0;i<state.asset->clips.size();++i){
            const auto& clip=state.asset->clips[i];
            if(!fo3dooranim::EqualName(clip.name,"Forward")||clip.frequency<=0)continue;
            state.clip=int(i);state.started=seconds;
            state.presentationTime=(clip.stop-clip.start)/clip.frequency;
            state.lastSound=state.presentationTime;
            fo3anim::BindClip(state.asset->hierarchy,clip,state.pose);
            return;
        }
    }else if(!state.settled)state.presentationTime+=dt;
}
void Animate(){
    // OnLoad/GameMode actions can publish a new visual inventory after the
    // frame's initial Register. Revalidate cached shape slots before use.
    Register();
    for(auto& entry:animations){auto& state=entry.second;const auto& asset=*state.asset;
        if(state.clip<0||state.settled)continue;
        const auto& clip=asset.clips[size_t(state.clip)];const double elapsed=std::max(0.,seconds-state.started+state.presentationTime);
        if(!fo3anim::Sample(asset.hierarchy,clip,elapsed,state.pose))continue;
        auto& player=gPlayerSession->player;
        const auto* first=state.shapes.empty()?nullptr:&gObjects[state.shapes.front()];
        bool disabled=true;std::string soundError;
        // A controller sequence can survive a resident visual refresh. Only
        // the live placed object can emit its keys, from its authored origin.
        if(first&&!IsFo3LoadingVisible()&&
           player.ReferenceDisabled(entry.first,disabled,soundError)&&!disabled){
            const auto* source=first->q2016PlacementMatrix+12;
            const float dx=source[0]-gQ210Head[0],dy=source[1]-gQ210Head[1],dz=source[2]-gQ210Head[2];
            const float horizontal=std::hypot(dx,dz),yaw=gQ210Head[3];
            const float pan=horizontal>0?(dx*std::cos(yaw)-dz*std::sin(yaw))/horizontal:0;
            fo3animsound::Advance(player.Definitions().pipboy,clip,state.lastSound,elapsed,[&](uint32_t id){
                fo3audio::SoundAt(id,std::hypot(horizontal,dy)*FO3_UNITS_PER_METRE,pan);
            });
        }
        state.lastSound=elapsed;
        for(const auto& value:state.pose.properties)if(value.kind==fo3anim::PropertyKind::Visibility)state.visible[size_t(value.bone)]=value.value[0]!=0;
        for(size_t shape:state.shapes){auto& object=gObjects[shape];
            const int bone=object.q2401AnimationBone;
            const auto* delta=fo3dooranim::ShapeDelta(asset,state.pose,bone);
            fo3anim::Matrix scene{};
            if(delta&&fo3dooranim::ModelDeltaToScene(object.q2016PlacementMatrix,FO3_UNITS_PER_METRE,*delta,scene))std::copy(scene.begin(),scene.end(),object.q2400DoorTransform);
            bool visible=true;for(int i=bone;i>=0;i=asset.hierarchy.bones[size_t(i)].parent)visible=visible&&state.visible[size_t(i)];object.scriptVisible=visible;
            for(const auto& property:state.pose.properties)if(property.bone==bone){
                using K=fo3anim::PropertyKind;
                if(property.kind==K::Alpha)object.materialAlpha=std::clamp(property.value[0],0.f,1.f);
                if(property.kind==K::Emissive)std::copy_n(property.value.begin(),3,object.emissiveColor);
                if(property.kind==K::Specular)std::copy_n(property.value.begin(),3,object.specularColor);
            }
        }
        const double duration=clip.frequency>0?double(clip.stop-clip.start)/clip.frequency:0;
        const bool ended=clip.cycle==2&&elapsed>=duration;
        if(ended&&state.presentationTime>0)
            gPlayerSession->player.FinishScriptAnimation(entry.first,clip.name);
        // Collision targets come from BHK's targetBlock, never nearest geometry.
        // Publish at 10 Hz and once at the precise settled endpoint.
        if(first&&(state.lastCollision<0||elapsed-state.lastCollision>=.1||ended)){
            Fo3DoorCollisionPose collision;
            for(uint32_t block:GetFo3ScriptCollisionTargets(entry.first)){
                if(block>=asset.blockBones.size())continue;
                const int bone=asset.blockBones[block];
                if(bone<0||size_t(bone)>=state.pose.delta.size())continue;
                fo3anim::Matrix scene{};if(fo3dooranim::ModelDeltaToScene(first->q2016PlacementMatrix,FO3_UNITS_PER_METRE,state.pose.delta[size_t(bone)],scene))collision.nodeTransforms.emplace(uint32_t(block),scene);
            }
            SetFo3ScriptCollisionPose(entry.first,collision);state.lastCollision=elapsed;
        }
        state.settled=ended;
    }
}
fo3script::ImagePresentation Presentation(){return modifiers.Sample(seconds);}
bool PackageEvent(uint32_t reference,uint32_t package,bool completed){
    if(!gPlayerSession||gPlayerSession->saveBlocked||Fo3ResidentFrameBlocked())return false;
    auto& player=gPlayerSession->player;std::string error;
    auto plan=player.PreparePackageEventTransaction(reference,package,completed,error,fo3player::ScriptProgramCapture::EnteredPrograms);
    if(plan){
        if(!plan->HasReferenceChanges()?player.CommitScriptTransaction(*plan):PublishFo3DialogueReferences(player,*plan,error)){
            FlushFo3PlayerState();return true;
        }
        if(plan->HasReferenceChanges()){
            const auto ticket=BeginFo3ResidentPreparedWorkflow({},std::move(plan));
            if(ticket){gFo3ResidentGameplayTicket=ticket;return false;}
        }
    }
    static std::unordered_set<std::string> reported;
    const auto key=std::to_string(reference)+":"+std::to_string(package)+":"+error;
    if(reported.size()<256&&reported.insert(key).second)Q6H_LOGW("SCRIPT PACKAGE EVENT REJECTED ref=%08X package=%08X completed=%d reason=%s",reference,package,completed,error.c_str());
    return false;
}
}
