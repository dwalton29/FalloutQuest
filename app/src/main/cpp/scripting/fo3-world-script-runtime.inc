#include "dialogue/fo3-dialogue-selection.h"
#include "audio/fo3-animation-sounds.h"
// Render-thread adapters for original reference events and controller actions.
// No location, quest, reference, sound or animation-group IDs are embedded here.
namespace fo3worldscript {
struct Animation {
    std::shared_ptr<const fo3dooranim::Asset> asset;
    int clip=-1;double started=0,lastCollision=-1,lastSound=-1;bool settled=false;
    fo3anim::Pose pose;
    std::vector<bool> visible;
    std::vector<size_t> shapes;
};
static std::unordered_map<uint32_t,Animation> animations;
static std::unordered_set<uint32_t> loaded;
static fo3script::ImageModifier modifier;
static double modifierStarted=0,seconds=0;
static fo3dialogue::Session speech;
static std::vector<fo3script::Action> speechPending;
static uint32_t speechReference=0;
static uint32_t speechTopic=0,speechTarget=0,speechToken=0,speechSerial=0;
static double speechBegan=0;
static uint64_t speechGeneration=~uint64_t{0};
static fo3dialogue::Context SpeechContext(uint32_t reference,uint32_t target){
    auto& player=gPlayerSession->player;
    fo3dialogue::Context ctx;ctx.player=&player;ctx.talking=true;
    const auto& defs=player.Definitions();
    auto placed=defs.pipboy.targets.find(reference);
    if(placed!=defs.pipboy.targets.end()){const auto& p=placed->second;ctx.speaker={reference,p.base,p.cell,p.world,0,p.x,p.y,p.z,false,false};}
    if(auto* actor=DialogueActor(reference)){
        const auto pos=Q240GamePosition(*actor);
        ctx.speaker={reference,actor->source.baseFormId,gCurrentCellFormId,gExteriorWorldspaceQ1890,actor->aiPackage,pos[0],pos[1],pos[2],actor->runtime.activity==fo3npc::Activity::Combat,actor->runtime.packageKnown};
    }
    fo3dialogue::ReferenceContext(gPlayerSession->player,target,ctx.target);
    if(target==fo3player::PlayerRef){ctx.target={target,7,gCurrentCellFormId,gExteriorWorldspaceQ1890,0,
        gSceneCenterXQ1730+gQ210Head[0]*FO3_UNITS_PER_METRE,
        gSceneCenterYQ1730-(gQ210Head[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,
        gSceneFloorZQ1730+(gQ210Head[1]-FLOOR_Y)*FO3_UNITS_PER_METRE,false,false};
    }else if(const auto* actor=DialogueActor(target)){const auto pos=Q240GamePosition(*actor);
      ctx.target={target,actor->source.baseFormId,gCurrentCellFormId,gExteriorWorldspaceQ1890,actor->aiPackage,pos[0],pos[1],pos[2],actor->runtime.activity==fo3npc::Activity::Combat,actor->runtime.packageKnown};
    }
    return ctx;
}
static void SpeechStop(){
    if(gPlayerSession&&speechReference)gPlayerSession->player.SetScriptActorTalking(speechReference,false);
    if(auto* actor=DialogueActor(speechReference)){actor->runtime.speaking=false;actor->runtime.scriptSpeechToken=0;actor->lastFrame=UINT64_MAX;}
    if(speech.Active()&&!gDialogue.Active())fo3audio::DialogueStop();
    speech.End("scripted speech stopped");speechToken=0;speechReference=0;
}

static uint64_t runtimeGeneration=~uint64_t{0};
static uint64_t registryRevision=~uint64_t{0},actorRegistryRevision=~uint64_t{0};
static const GpuObject* registryStorage=nullptr;
static size_t registryCount=0;
static const fo3player::Player* registryPlayer=nullptr;

bool Event(uint32_t reference,const char* event,uint32_t activator){
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
    }else FlushFo3PlayerState();
    return committed||invalidProgram;
}
void SpeechTick(){
    if(!gPlayerSession)return;
    auto& player=gPlayerSession->player;
    if(speechGeneration!=player.ScriptRuntimeGeneration()){
        SpeechStop();speechPending.clear();speechGeneration=player.ScriptRuntimeGeneration();
    }
    if(speech.Active()){
        if(auto* actor=DialogueActor(speech.actor))Q230DialogueResponseAnimation(*actor,speech);
        bool disabled=true;std::string error;
        if(gDialogue.Active()||IsFo3LoadingVisible()||!player.ReferenceDisabled(speech.actor,disabled,error)||disabled||seconds-speechBegan>60){SpeechStop();return;}
        const auto done=fo3audio::DialogueCompletion();
        if(!done||uint32_t(done>>1)!=speechToken)return;
        const uint32_t actor=speech.actor,topic=speechTopic;
        // Only actual successful audio completion can run authored INFO end
        // results. Missing audio must never pretend Weld finished his greeting.
        if(!(done&1u)){Q6H_LOGW("SCRIPT SPEECH VOICE UNAVAILABLE ref=%08X topic=%08X",actor,topic);SpeechStop();return;}
        if(speech.AudioDone(speech.audioToken,true,SpeechContext(actor,speechTarget),player)){
            speechToken=0x30000000u|(++speechSerial&0xffffffu);
            if(auto* live=DialogueActor(actor))live->runtime.scriptSpeechToken=speechToken;
            fo3audio::Dialogue(speech.Audio(player.Definitions().pipboy),speechToken);return;
        }
        SpeechStop();Event(actor,"SayToDone",topic);FlushFo3PlayerState();
    }
    if(speechPending.empty()||gDialogue.Active()||gQ230NpcChat.Active()||IsFo3LoadingVisible())return;
    const auto action=speechPending.front();speechPending.erase(speechPending.begin());
    auto* actor=DialogueActor(action.reference);
    if(actor&&(actor->runtime.offScene||!actor->runtime.CanTalk()))return;
    speech.publishReferences=[](fo3player::Player& p,fo3player::ScriptTransactionPlan& plan,std::string& e){return PublishFo3DialogueReferences(p,plan,e);};
    speech.previewReferences=[](const fo3player::Player& p,const fo3player::ScriptTransactionPlan& plan,std::string& e){return PreviewFo3DialogueReferences(p,plan,e);};
    if(!(action.info?speech.StartSelectedTopic(action.form,action.info,SpeechContext(action.reference,action.flags),player):speech.StartTopic(action.form,SpeechContext(action.reference,action.flags),player))){
        Q6H_LOGW("SCRIPT SPEECH TOPIC UNAVAILABLE ref=%08X topic=%08X",action.reference,action.form);return;
    }
    speechReference=action.reference;speechTopic=action.form;speechTarget=action.flags;speechBegan=seconds;
    player.SetScriptActorTalking(action.reference,true);if(actor){actor->runtime.speaking=true;actor->lastFrame=UINT64_MAX;}
    speechToken=0x30000000u|(++speechSerial&0xffffffu);
    if(actor)actor->runtime.scriptSpeechToken=speechToken;
    fo3audio::DialogueGain(.7f);fo3audio::Dialogue(speech.Audio(player.Definitions().pipboy),speechToken);
}
void Register(){
    auto& player=gPlayerSession->player;
    const uint64_t revision=gFo3VisualPublication.Revision();
    if(actorRegistryRevision==gQ230NpcResidenceRevision&&registryPlayer==&player&&runtimeGeneration==player.ScriptRuntimeGeneration()&&registryRevision==revision&&registryStorage==gObjects.data()&&registryCount==gObjects.size())return;
    if(registryPlayer!=&player||runtimeGeneration!=player.ScriptRuntimeGeneration()){animations.clear();loaded.clear();modifier={};seconds=0;for(auto& actor:gQ230NpcActors){actor.runtime.scriptLookTarget=actor.runtime.scriptSpeechToken=0;actor.responseIdle=actor.responseToken=actor.responseInfo=0;}}
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
    player.SetScriptAnimationRegistry(std::move(registry));
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
    std::vector<uint32_t> refs;
    for(const auto& object:gObjects)if(object.refFormId&&!loaded.count(object.refFormId))refs.push_back(object.refFormId);
    for(const auto& actor:gQ230NpcActors)if(!actor.runtime.offScene&&!loaded.count(actor.source.refFormId))refs.push_back(actor.source.refFormId);
    std::sort(refs.begin(),refs.end());refs.erase(std::unique(refs.begin(),refs.end()),refs.end());
    for(uint32_t id:refs){if(Event(id,"OnLoad",0))loaded.insert(id);if(Fo3ResidentFrameBlocked())return;}
}
void Actions(){
    auto& player=gPlayerSession->player;fo3script::Action action;
    while(player.PollScriptAction(action)){
        if(action.kind==fo3script::ActionKind::Look){
            if(auto* actor=DialogueActor(action.reference))actor->runtime.scriptLookTarget=action.form;
        }else if(action.kind==fo3script::ActionKind::SayTo){if(speechPending.size()<128)speechPending.push_back(action);
        }else if(action.kind==fo3script::ActionKind::DefaultActivate){
            if(!DeliverFo3ScriptDefaultActivation(action.reference))
                Q6H_LOGW("SCRIPT DEFAULT ACTIVATE REJECTED: ref=%08X",action.reference);
        }else if(action.kind==fo3script::ActionKind::ImageModifier){
            const auto i=player.Definitions().imageModifiers.find(action.form);
            if(i!=player.Definitions().imageModifiers.end()){modifier=i->second;modifierStarted=seconds;}
        }else if(action.kind==fo3script::ActionKind::PlayGroup){
            const auto i=animations.find(action.reference);if(i==animations.end())continue;
            auto& state=i->second;
            for(size_t c=0;c<state.asset->clips.size();++c)if(fo3dooranim::EqualName(state.asset->clips[c].name,action.group.c_str())){
                state.clip=int(c);state.started=action.started;state.lastCollision=-1;state.lastSound=-1;state.settled=false;
                fo3anim::BindClip(state.asset->hierarchy,state.asset->clips[c],state.pose);break;
            }
        }
    }
}
void Animate(){
    // OnLoad/GameMode actions can publish a new visual inventory after the
    // frame's initial Register. Revalidate cached shape slots before use.
    Register();
    for(auto& entry:animations){auto& state=entry.second;const auto& asset=*state.asset;
        if(state.clip<0||state.settled)continue;
        const auto& clip=asset.clips[size_t(state.clip)];const double elapsed=std::max(0.,seconds-state.started);
        if(!fo3anim::Sample(asset.hierarchy,clip,elapsed,state.pose))continue;
        fo3animsound::Advance(gPlayerSession->player.Definitions().pipboy,clip,state.lastSound,elapsed,[](uint32_t id){fo3audio::SoundEvent(id);});state.lastSound=elapsed;
        for(const auto& value:state.pose.properties)if(value.kind==fo3anim::PropertyKind::Visibility)state.visible[size_t(value.bone)]=value.value[0]!=0;
        const GpuObject* first=nullptr;
        for(size_t shape:state.shapes){auto& object=gObjects[shape];if(!first)first=&object;
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
fo3script::ImagePresentation Presentation(){return fo3script::SampleImageModifier(modifier,seconds-modifierStarted);}
}
