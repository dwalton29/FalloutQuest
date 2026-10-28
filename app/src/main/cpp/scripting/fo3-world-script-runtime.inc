// Render-thread adapters for original reference events and controller actions.
// No location, quest, reference, sound or animation-group IDs are embedded here.
namespace fo3worldscript {
struct Animation {
    std::shared_ptr<const fo3dooranim::Asset> asset;
    int clip=-1;double started=0,lastCollision=-1;bool settled=false;
    fo3anim::Pose pose;
    std::vector<bool> visible;
};
static std::unordered_map<uint32_t,Animation> animations;
static std::unordered_set<uint32_t> loaded;
static fo3script::ImageModifier modifier;
static double modifierStarted=0,seconds=0;
static uint64_t runtimeGeneration=~uint64_t{0};
static uint64_t registryRevision=~uint64_t{0};
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
void Register(){
    auto& player=gPlayerSession->player;
    const uint64_t revision=gFo3VisualPublication.Revision();
    if(registryPlayer==&player&&runtimeGeneration==player.ScriptRuntimeGeneration()&&registryRevision==revision&&registryStorage==gObjects.data()&&registryCount==gObjects.size())return;
    if(registryPlayer!=&player||runtimeGeneration!=player.ScriptRuntimeGeneration()){animations.clear();loaded.clear();modifier={};seconds=0;}
    runtimeGeneration=player.ScriptRuntimeGeneration();registryPlayer=&player;registryRevision=revision;registryStorage=gObjects.data();registryCount=gObjects.size();
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
    std::sort(refs.begin(),refs.end());refs.erase(std::unique(refs.begin(),refs.end()),refs.end());
    for(uint32_t id:refs){if(Event(id,"OnLoad",0))loaded.insert(id);if(Fo3ResidentFrameBlocked())return;}
}
void Actions(){
    auto& player=gPlayerSession->player;fo3script::Action action;
    while(player.PollScriptAction(action)){
        if(action.kind==fo3script::ActionKind::DefaultActivate){
            if(!DeliverFo3ScriptDefaultActivation(action.reference))
                Q6H_LOGW("SCRIPT DEFAULT ACTIVATE REJECTED: ref=%08X",action.reference);
        }else if(action.kind==fo3script::ActionKind::ImageModifier){
            const auto i=player.Definitions().imageModifiers.find(action.form);
            if(i!=player.Definitions().imageModifiers.end()){modifier=i->second;modifierStarted=seconds;}
        }else if(action.kind==fo3script::ActionKind::PlayGroup){
            const auto i=animations.find(action.reference);if(i==animations.end())continue;
            auto& state=i->second;
            for(size_t c=0;c<state.asset->clips.size();++c)if(fo3dooranim::EqualName(state.asset->clips[c].name,action.group.c_str())){
                state.clip=int(c);state.started=action.started;state.lastCollision=-1;state.settled=false;
                fo3anim::BindClip(state.asset->hierarchy,state.asset->clips[c],state.pose);break;
            }
        }
    }
}
void Animate(){
    for(auto& entry:animations){auto& state=entry.second;const auto& asset=*state.asset;
        if(state.clip<0||state.settled)continue;
        const auto& clip=asset.clips[size_t(state.clip)];const double elapsed=std::max(0.,seconds-state.started);
        if(!fo3anim::Sample(asset.hierarchy,clip,elapsed,state.pose))continue;
        for(const auto& value:state.pose.properties)if(value.kind==fo3anim::PropertyKind::Visibility)state.visible[size_t(value.bone)]=value.value[0]!=0;
        const GpuObject* first=nullptr;
        for(auto& object:gObjects){if(!object.scriptAnimated||object.refFormId!=entry.first)continue;if(!first)first=&object;
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
            for(size_t block=0;block<asset.blockBones.size();++block){const int bone=asset.blockBones[block];
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
