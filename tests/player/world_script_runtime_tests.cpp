#include "player/fo3-player-state.h"
#include "ui/loading/fo3-loading-catalog.h"
#include "world/interaction/fo3-door-nif-animation.h"
#include "world/fo3-cell-traversal.h"
#include "dialogue/fo3-dialogue-session.h"
#include "npc/fo3-npc-state.h"
struct Fo3DoorCollisionPose {fo3anim::Matrix transform=fo3anim::Identity();bool enabled=true;std::unordered_map<uint32_t,fo3anim::Matrix> nodeTransforms;};
#include <stdexcept>
#include <iostream>
static void Check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
#define Q6H_LOGW(...) ((void)0)
#define Q6H_LOGI(...) ((void)0)
std::unique_ptr<fo3player::Session> gPlayerSession;
struct GpuObject {
 uint32_t refFormId=0;bool scriptAnimated=true,scriptVisible=true;
 std::shared_ptr<const fo3dooranim::Asset> q2401DoorAnimation;int q2401AnimationBone=1;
 float q2016PlacementMatrix[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 float q2400DoorTransform[16]{},materialAlpha=1,emissiveColor[3]{},specularColor[3]{};
};
std::vector<GpuObject> gObjects;
struct Publication {uint64_t revision=1;uint64_t Revision()const{return revision;}} gFo3VisualPublication;
constexpr float FO3_UNITS_PER_METRE=1;
uint64_t gFo3ResidentGameplayTicket=0;int saves=0;
bool Fo3ResidentFrameBlocked(){return false;}
void FlushFo3PlayerState(){++saves;}
bool worldDefer=false,worldBusy=false;
bool CanDeferFo3DialogueResult(){return !worldBusy;}
int worldResult=0;uint64_t worldResultTicket=0;
std::unique_ptr<fo3player::ScriptTransactionPlan> worldResultPlan;
bool PublishFo3DialogueReferences(fo3player::Player& p,fo3player::ScriptTransactionPlan& plan,std::string&){return !worldDefer&&p.CommitScriptTransaction(plan);}
uint64_t BeginFo3ResidentPreparedWorkflow(int,std::unique_ptr<fo3player::ScriptTransactionPlan>){return 0;}
std::vector<uint32_t> GetFo3ScriptCollisionTargets(uint32_t){return {0,1};}
Fo3DoorCollisionPose collision;int collisionUpdates=0;
bool SetFo3ScriptCollisionPose(uint32_t,const Fo3DoorCollisionPose& pose){collision=pose;++collisionUpdates;return true;}
float gQ210Head[4]{};
Fo3DoorTeleport defaultTeleport;int defaultTransitions=0;uint32_t defaultSource=0,defaultDestination=0;bool localOpen=false;
bool ResolveFo3DoorTeleportQ1700(uint32_t,Fo3DoorTeleport* out){*out=defaultTeleport;return out->valid;}
bool QueueFo3DoorTransitionQ1700(uint32_t source,uint32_t destination,float x,float y,float z,float,float,float){Check(x==100&&y==200&&z==300,"original default arrival passed through");++defaultTransitions;defaultSource=source;defaultDestination=destination;return true;}
bool Q2400ToggleInteriorDoor(uint32_t,float,float){localOpen=!localOpen;return true;}
bool Q2400DoorOpen(uint32_t){return localOpen;}
#include "world/interaction/fo3-script-default-activation.inc"
struct ActorVisual {
  struct Source{uint32_t refFormId=0,baseFormId=0;}source;
  fo3npc::RuntimeState runtime;uint32_t aiPackage=0,responseIdle=0,responseToken=0,responseInfo=0;uint64_t lastFrame=0;
};
std::vector<ActorVisual> gQ230NpcActors;
uint64_t gQ230NpcResidenceRevision=0;
ActorVisual* DialogueActor(uint32_t ref){for(auto& a:gQ230NpcActors)if(a.source.refFormId==ref)return &a;return nullptr;}
std::array<float,3> Q240GamePosition(const ActorVisual& a){return a.runtime.position;}
void Q230DialogueResponseAnimation(ActorVisual&,const fo3dialogue::Session&){}
void Q230PlayPackageIdle(ActorVisual& a,uint32_t idle,uint32_t){a.responseIdle=idle;}
void Q230StopScriptCombat(ActorVisual& a,double){a.runtime.EndCombat();a.runtime.pendingAttack=false;}
fo3dialogue::Session gDialogue;
struct Chat{bool Active()const{return false;}}gQ230NpcChat;
uint32_t gCurrentCellFormId=1,gExteriorWorldspaceQ1890=0;
float gSceneCenterXQ1730=0,gSceneCenterYQ1730=0,gSceneFloorZQ1730=0;
constexpr float SCENE_FORWARD=0,FLOOR_Y=0;
bool IsFo3LoadingVisible(){return false;}
bool PreviewFo3DialogueReferences(const fo3player::Player&,const fo3player::ScriptTransactionPlan&,std::string&){return true;}
void ConfigureFo3DialogueSession(fo3dialogue::Session& s){
 s.previewReferences=PreviewFo3DialogueReferences;s.publishReferences=PublishFo3DialogueReferences;
 s.deferEndResult=[](auto plan,std::string&)->uint64_t{if(!worldDefer||worldResultPlan)return 0;worldResultPlan=std::move(plan);worldResult=0;return ++worldResultTicket;};
 s.readyForEndResult=CanDeferFo3DialogueResult;
 s.endResultStatus=[](uint64_t ticket){return ticket==worldResultTicket?worldResult:-1;};
}
namespace fo3audio {
uint32_t token=0;uint64_t completion=0;std::string path;std::vector<uint32_t> sounds;
void SoundEvent(uint32_t id){sounds.push_back(id);}
void DialogueStop(){token=0;}
void DialogueGain(float){}
void Dialogue(const std::string& p,uint32_t t){path=p;token=t;completion=0;}
uint64_t DialogueCompletion(){return completion;}
}
namespace fo3bloodui {unsigned triggered=0;void Clear(){triggered=0;}void Trigger(uint32_t count,double,const fo3player::Catalog&){triggered+=count;}}
unsigned pipboyResets=0,radioRefreshes=0;void ResetFo3Pipboy(){++pipboyResets;}void RefreshFo3PipboyRadio(){++radioRefreshes;}
float playerCollisionScale=1;void SetFo3PlayerScale(float scale){playerCollisionScale=scale;}
bool playerTransitionPending=false;unsigned playerTransitions=0;
bool HasPendingFo3CellTransitionRequestQ74(){return playerTransitionPending;}
bool QueueFo3MapMarkerTransitionQ74(uint32_t,float,float,float,float,float,float,double){playerTransitionPending=true;++playerTransitions;return true;}
#include "scripting/fo3-world-script-runtime.inc"
static void OriginalIntercom(const char* path){
 fo3player::Catalog c;std::string error;Check(fo3player::LoadCatalog(path,c,error),"original intercom catalogue");
 const auto q=c.pipboy.formNames.at("cg02"),dad=c.pipboy.formNames.at("cg02dadref"),jonas=c.pipboy.formNames.at("cg02jonasref"),intercom=c.pipboy.formNames.at("cg02dinerintercomref"),package=c.pipboy.formNames.at("cg02dadtalktojonasonintercom");
 // Isolate the reached birthday intercom boundary; the full New Game test
 // reaches this same assignment through the unmodified quest timer.
 auto& quest=c.initial.pipboy.quests[q];quest.stage=35;quest.running=true;quest.stages.insert(35);c.initial.referenceEnabled[dad]=true;
 gPlayerSession=std::make_unique<fo3player::Session>(c);auto& p=gPlayerSession->player;p.RegisterOriginalActors();
 Check(p.SetTalkingActivatorActor(intercom,jonas),"original stage-35 assignment fixture");
 const auto& at=c.pipboy.targets.at(intercom);gCurrentCellFormId=at.cell;gExteriorWorldspaceQ1890=at.world;
 p.SetScriptPlayerLocation(at.x,at.y,at.z,at.cell,at.world);gQ210Head[0]=at.x;gQ210Head[1]=at.z;gQ210Head[2]=-at.y;
 gQ230NpcActors.clear();ActorVisual actor;actor.source={dad,c.pipboy.targets.at(dad).base};actor.aiPackage=package;actor.runtime.position={at.x,at.y,at.z};gQ230NpcActors.push_back(actor);
 actor.source={jonas,c.pipboy.targets.at(jonas).base};actor.runtime.offScene=true;gQ230NpcActors.push_back(actor);++gQ230NpcResidenceRevision;
 fo3worldscript::SpeechTick();Check(fo3worldscript::PackageEvent(dad,package,false),"original intercom package start");
 Check(p.QueueScriptSpeech(dad,c.pipboy.packages.at(package).dialogueTopic,intercom,true,false,package),"original intercom selected topic");
 fo3worldscript::Actions();fo3worldscript::SpeechTick();std::unordered_set<uint32_t> infos;unsigned responses=0;
 while(fo3worldscript::speech.Active()){
  const auto* info=fo3worldscript::speech.Current(c.pipboy);Check(info!=nullptr,"original intercom INFO");infos.insert(info->id);Check(!fo3audio::path.empty(),"original intercom voice path");Check(++responses<=16,"bounded original intercom chain");
  fo3audio::completion=(uint64_t(fo3audio::token)<<1)|1;fo3worldscript::SpeechTick();
 }
 Check(infos.size()>=3&&p.Snapshot().pipboy.quests.at(q).stage==38&&!p.TalkingActivatorActor(intercom),"original linked greeting, reply and goodbye clear assignment");
 Check(p.Snapshot().actorPackageEvents.at(dad)==std::make_pair(package,true),"original intercom completes Dad package");
 Check(!p.ScriptActorTalking(dad)&&!p.ScriptActorTalking(jonas)&&!p.ScriptActorTalking(intercom)&&gQ230NpcActors.back().runtime.scriptSpeechToken==0,"remote actor remains off-scene and speech ownership releases");
 std::cout<<"Original intercom package, linked voices, stage 38, assignment clear and initiating package completion passed\n";
}
static void OriginalPrayer(const char* path){
 fo3player::Catalog catalog;std::string error;Check(fo3player::LoadCatalog(path,catalog,error),"original prayer catalogue");
 gPlayerSession=std::make_unique<fo3player::Session>(catalog);auto& p=gPlayerSession->player;p.RegisterOriginalActors();
 const auto ref=catalog.pipboy.formNames.at("confessorcromwellref"),marker=catalog.pipboy.formNames.at("megcromwellpraybombmarkerref");
 const auto& at=catalog.pipboy.targets.at(marker);gCurrentCellFormId=at.cell;gExteriorWorldspaceQ1890=at.world;
 p.SetScriptPlayerLocation(at.x+100,at.y,at.z,at.cell,at.world);gQ210Head[0]=at.x+100;gQ210Head[1]=at.z;gQ210Head[2]=-at.y;
 auto state=p.Snapshot().actors.at(ref);state.position={at.x,at.y,at.z};state.cell=at.cell;state.world=at.world;Check(p.UpdateActor(ref,state),"resident at original prayer marker");
 gQ230NpcActors.clear();ActorVisual actor;actor.source={ref,catalog.pipboy.targets.at(ref).base};actor.runtime.position={at.x,at.y,at.z};gQ230NpcActors.push_back(actor);++gQ230NpcResidenceRevision;
 fo3worldscript::SpeechTick();
 Check(fo3worldscript::Event(ref,"OnLoad",0),"original Cromwell OnLoad");
 const auto errors=[&](const std::string& text){std::cerr<<text<<"\n";};p.SetScriptEventDiagnostic(errors);
 p.PumpReferenceGameMode(.3,{ref});fo3worldscript::Actions();fo3worldscript::SpeechTick();
 Check(p.ScriptActorTalking(ref),"original StartConversation begins prayer");
 std::unordered_set<uint32_t> spoken;unsigned gestures=0;
 while(p.ScriptActorTalking(ref)){
  const auto* info=fo3worldscript::speech.Current(catalog.pipboy);Check(info&&spoken.insert(info->id).second,"authored prayer line does not repeat");
  for(const auto& r:info->responses)if(r.speakerAnimation){Check(catalog.pipboy.idleModels.count(r.speakerAnimation),"original preaching IDLE bound");++gestures;}
  Check(spoken.size()<=32,"bounded original prayer chain");
  fo3audio::completion=(uint64_t(fo3audio::token)<<1)|1;fo3worldscript::SpeechTick();
 }
 Check(spoken.size()==17&&gestures==15,"all original prayer lines and authored gestures");
 const auto key=fo3dialogue::VariableKey(catalog.pipboy,"ConfessorCromwellREF.Tick");Check(p.Snapshot().pipboy.dialogueVariables.at(key)==0,"authored final Amen resets prayer tick");
 const auto before=fo3audio::token;p.PumpReferenceGameMode(.3,{ref});fo3worldscript::Actions();fo3worldscript::SpeechTick();Check(!p.ScriptActorTalking(ref)&&fo3audio::token==before,"source timer prevents immediate repeat");
 std::cout<<"Original Cromwell OnLoad/GameMode, self conversation, 17 linked lines, 15 gestures and final reset passed\n";
}
int main(int argc,char** argv){try{
 fo3player::Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);
 c.references[10].base=100;c.references[10].cell=1;c.pipboy.targets[10].cell=1;
 c.pipboy.triggerPrimitives[10]={{2,2,2},1,true};c.eventBaseScripts[100]=200;c.pipboy.objectReferenceScripts[10]={200,100};
 auto& script=c.pipboy.scripts[200];script.variables={{1,"opened"},{2,"loaded"}};
 script.source="scn C\nshort opened\nshort loaded\nBegin OnTrigger\nRewardKarma 1\nEnd\nBegin OnTriggerEnter player\nPlayGroup Advance 5\nEnd\nBegin OnLoad\nset loaded to loaded + 1\nEnd\nBegin GameMode\nif IsAnimPlaying == 0\nPlayGroup Idle 5\nendif\nEnd";
 c.eventPrograms[200]=fo3script::ParseEvents(script);
 c.references[82].base=810;c.references[82].flags=0x800;c.pipboy.targets[82].base=810;c.pipboy.targets[82].cell=1;
 c.references[81].base=810;c.pipboy.targets[81].base=810;c.pipboy.targets[81].cell=1;
 c.pipboy.dialogueActors[810].talkingActivator=true;
 c.references[80].base=800;c.containers[800].script=801;c.containers[800].entries={{900,0,2}};
 c.items[900].formId=900;c.items[900].name="Original goods";c.eventBaseScripts[800]=801;
 c.pipboy.scripts[801].source="Begin OnActivate\nActivate player\nEnd";c.eventPrograms[801]=fo3script::ParseEvents(c.pipboy.scripts[801]);
 c.references[30].base=300;c.doorBases.insert(300);c.scriptedBases.insert(300);c.eventBaseScripts[300]=301;
 c.pipboy.scripts[301].source="Begin OnActivate\nActivate\nEnd";c.eventPrograms[301]=fo3script::ParseEvents(c.pipboy.scripts[301]);
 c.actorPlacements[40].base=400;
 c.pipboy.packages[850].type=15;c.pipboy.packages[850].dialogueType=1;c.pipboy.packages[850].dialogueTopic=500;c.pipboy.packages[850].target.valid=true;c.pipboy.packages[850].target.value=fo3player::PlayerRef;
 c.references[40].base=400;c.references[40].valid=true;c.pipboy.targets[40].base=400;c.pipboy.targets[40].cell=1;
 c.weapons.actors[400].health=20;c.pipboy.dialogueActors[400].script=401;
 c.pipboy.referenceScripts[40]={"speaker",400};c.eventBaseScripts[400]=401;
 c.pipboy.formNames["speaker"]=40;c.pipboy.formNames["voicetopic"]=500;
 auto& spoken=c.pipboy.scripts[401];spoken.variables={{1,"ended"},{2,"done"}};
 spoken.source="scn Speech\nshort ended\nshort done\nBegin SayToDone VoiceTopic\nset done to 1\nEnd";
 c.eventPrograms[401]=fo3script::ParseEvents(spoken);
 c.pipboy.quests[600].flags=1;c.pipboy.sounds[700]="sound/original.ogg";c.pipboy.formNames["originalsound"]=700;
 fo3pipdata::Info line;line.id=501;line.topic=500;line.quest=600;line.type=1;
 line.responses.push_back({700,0,"Original speech"});line.end.source="set speaker.ended to 1";
 c.pipboy.topics[500].push_back(line);
 auto broadcast=line;broadcast.id=821;broadcast.topic=820;broadcast.flags=1;broadcast.end={};c.pipboy.topics[820].push_back(broadcast);
 c.references[82].valid=true;c.references[82].cell=1;c.pipboy.formNames["hiddenspeaker"]=82;
 auto deferred=line;deferred.id=831;deferred.topic=830;deferred.end.source="HiddenSpeaker.Enable\nRewardKarma 3";c.pipboy.topics[830].push_back(deferred);
 c.pipboy.packages[852]=c.pipboy.packages[850];c.pipboy.packages[852].dialogueTopic=830;
 c.references[10].valid=true;c.pipboy.formNames["testweapon"]=901;c.items[901].formId=901;c.items[901].weapon.valid=true;c.items[902]=c.items[901];c.items[902].formId=902;
 script.variables[3]="hitactor";script.source.insert(script.source.find('\n')+1,"short hitactor\n");script.source+="\nBegin OnHit player\nset opened to opened + 1\nset hitActor to IsActionRef player\nEnd\nBegin OnHitWith TestWeapon\nif opened == 1\nRewardKarma 2\nendif\nEnd";c.eventPrograms[200]=fo3script::ParseEvents(script);
 c.references[83].base=813;c.references[83].valid=true;c.pipboy.targets[83].cell=1;c.eventBaseScripts[813]=814;
 c.pipboy.scripts[814].source="Begin OnHit\nRewardKarma 5\nEnd\nBegin OnHitWith\nUnsupportedHitOperation\nEnd";c.eventPrograms[814]=fo3script::ParseEvents(c.pipboy.scripts[814]);
 auto asset=std::make_shared<fo3dooranim::Asset>();fo3anim::Bone root;root.name="root";asset->hierarchy.bones.push_back(root);fo3anim::Bone leaf;leaf.name="leaf";leaf.parent=0;asset->hierarchy.bones.push_back(leaf);
 Check(fo3anim::FinalizeSkeleton(asset->hierarchy),"hierarchy");asset->blockBones={0,1};
 fo3anim::Clip clip;clip.name="Advance";clip.stop=2;clip.cycle=2;clip.textKeys={{.1f,"sound: OriginalSound"},{1.5f,"OriginalSound"}};
 fo3anim::Track track;track.bone="leaf";track.hasTranslation=true;track.translation.dimensions=3;track.translation.interpolation=1;track.translation.keys={{0,{0,0,0,0},{},{}},{2,{4,0,0,0},{},{}}};clip.tracks.push_back(track);
 fo3anim::PropertyTrack alpha;alpha.bone="leaf";alpha.kind=fo3anim::PropertyKind::Alpha;alpha.values.dimensions=1;alpha.values.interpolation=1;alpha.values.keys={{0,{0,0,0,0},{},{}},{2,{1,0,0,0},{},{}}};clip.properties.push_back(alpha);asset->clips.push_back(clip);
 clip.textKeys.clear();clip.name="Idle";clip.cycle=0;clip.stop=1;clip.tracks[0].translation.keys={{0,{4,0,0,0},{},{}}};asset->clips.push_back(clip);
 c.references[41].base=411;c.references[41].valid=true;c.actorPlacements[41].base=411;c.pipboy.targets[41].base=411;c.pipboy.targets[41].cell=1;c.weapons.actors[411].health=20;
 c.pipboy.dialogueActors[411].voice=0;
 fo3pipdata::Info first;first.id=911;first.topic=910;first.quest=600;first.type=1;first.links={920};first.nextSpeaker=0;first.responses.push_back({700,0,"Call"});
 fo3pipdata::Condition caller;caller.function=72;caller.a=411;caller.value=1;first.conditions.push_back(caller);c.pipboy.topics[910].push_back(first);
 auto reply=first;reply.id=921;reply.topic=920;reply.flags=1;reply.links.clear();reply.conditions.front().a=400;reply.end.source="RewardKarma 2";c.pipboy.topics[920].push_back(reply);
 auto& conversation=c.pipboy.packages[851];conversation.type=15;conversation.dialogueValid=true;conversation.dialogueType=0;conversation.dialogueTopic=910;conversation.target.valid=true;conversation.target.value=81;
 c.pipboy.targets[888].base=889;c.pipboy.targets[888].cell=1;c.pipboy.furniture[889].markers=1u<<11;
 gPlayerSession=std::make_unique<fo3player::Session>(std::move(c));auto& p=gPlayerSession->player;
 GpuObject object;object.refFormId=10;object.q2401DoorAnimation=asset;gObjects.push_back(object);
 p.SetScriptPlayerLocation(0,0,0,1,0);fo3worldscript::Register();p.SetScriptRuntimeClock(0,.25);fo3worldscript::Events();fo3worldscript::TriggerTick();fo3worldscript::Actions();fo3worldscript::Animate();
 Check(fo3worldscript::animations.at(10).clip==0,"trigger plays authored group");Check(p.ScriptTriggerChanges().empty(),"committed trigger acknowledged");
 fo3worldscript::seconds=1;p.SetScriptRuntimeClock(1,.25);fo3worldscript::Events();fo3worldscript::TriggerTick();fo3worldscript::Animate();
 Check(std::abs(gObjects[0].q2400DoorTransform[12]-2)<1e-5&&gObjects[0].materialAlpha==.5f,"live node transform and alpha");
 Check(fo3audio::sounds==std::vector<uint32_t>{700},"authored lowercase sound key emitted once");
 Check(collision.nodeTransforms.at(1)[12]==2&&collision.nodeTransforms.at(0)[12]==0,"independent collision node transforms");
 fo3worldscript::seconds=2;p.SetScriptRuntimeClock(2,.25);fo3worldscript::Animate();
 bool reserved=p.PumpWorldAwareGameMode(.25,{10},[](auto){return false;});Check(!reserved,"non-world animation transaction");fo3worldscript::Actions();fo3worldscript::Animate();
 Check(fo3worldscript::animations.at(10).clip==1&&gObjects[0].q2400DoorTransform[12]==4,"source waits and selects idle");
 // Re-publication reapplies the retained pose and does not invent another OnLoad.
 ++gFo3VisualPublication.revision;gObjects[0].q2400DoorTransform[12]=0;fo3worldscript::Register();fo3worldscript::Animate();Check(gObjects[0].q2400DoorTransform[12]==4,"stream publication retains pose");
 Check(fo3audio::sounds==std::vector<uint32_t>({700,700}),"bare authored SOUN key emitted, publication does not replay");
 // Events can republish/reorder meshes after Register and before Animate.
 GpuObject unrelated;unrelated.refFormId=55;unrelated.scriptAnimated=false;
 gObjects.insert(gObjects.begin(),unrelated);++gFo3VisualPublication.revision;
 fo3worldscript::Animate();
 Check(gObjects[0].q2400DoorTransform[12]==0&&gObjects[1].q2400DoorTransform[12]==4,"animation refreshes shape slots after mid-frame publication");
 gObjects.erase(gObjects.begin());++gFo3VisualPublication.revision;fo3worldscript::Animate();
 Check(p.Snapshot().karma==2,"OnTrigger follows enter and runs while occupied");
 const int beforeContinuousSave=saves;fo3worldscript::TriggerTick();Check(p.Snapshot().karma==3,"continuous trigger repeats without another enter");
 Check(saves==beforeContinuousSave,"continuous callback leaves persistence to throttled script saves");
 p.SetScriptPlayerLocation(10,0,0,1,0);fo3worldscript::Events();fo3worldscript::TriggerTick();Check(p.Snapshot().karma==3&&p.ScriptOccupiedTriggers().empty(),"leave ends continuous events");
 p.SetScriptPlayerLocation(0,0,0,999,0);fo3worldscript::Events();fo3worldscript::TriggerTick();Check(p.Snapshot().karma==3,"another cell cannot trigger");
 p.SetScriptPlayerLocation(0,0,0,1,0);
 defaultTeleport.valid=true;defaultTeleport.destinationDoorRefFormId=31;defaultTeleport.x=100;defaultTeleport.y=200;defaultTeleport.z=300;
 Check(fo3worldscript::Event(30,"OnActivate",fo3player::PlayerRef),"source delegates default activation");fo3worldscript::Actions();
 Check(defaultTransitions==1&&defaultSource==30&&defaultDestination==31,"production adapter queues original door transition");
 Check(fo3worldscript::Event(30,"OnActivate",fo3player::PlayerRef),"queue another source activation");Check(p.ScriptLock(30,100),"lock before delivery");fo3worldscript::Actions();Check(defaultTransitions==1,"delivery rechecks changed lock");
 Check(p.ScriptUnlock(30),"unlock local door");defaultTeleport.valid=false;Check(fo3worldscript::Event(30,"OnActivate",fo3player::PlayerRef),"source delegates local door");fo3worldscript::Actions();Check(localOpen,"production local swing default");
 Check(fo3worldscript::Event(80,"OnActivate",fo3player::PlayerRef)&&!p.CanLootContainer(80),"container event waits for normal action delivery");
 fo3worldscript::Actions();Check(p.CanLootContainer(80)&&p.ContainerContents(80)->front().count==2,"production default adapter opens real container inventory");
 p.EndContainerInteraction();Check(!p.CanLootContainer(80),"production loot delegation is transient");
 Check(fo3worldscript::Event(80,"OnActivate",fo3player::PlayerRef),"queue container default action");Check(p.ScriptLock(80,100),"lock container before delivery");
 fo3worldscript::Actions();Check(!p.CanLootContainer(80),"default container delivery rechecks lock");
 const auto impactKarma=p.Snapshot().karma;
 {std::string error;auto plan=p.PrepareWeaponHitTransaction(10,fo3player::PlayerRef,901,error);if(!plan)throw std::runtime_error("hit preflight: "+error); }
 Check(!p.QueueWeaponHit(82,fo3player::PlayerRef,901)&&!p.QueueWeaponHit(40,fo3player::PlayerRef,901)&&!p.QueueWeaponHit(10,fo3player::PlayerRef,999),"disabled objects, actor damage and invalid weapons are not object-hit events");
 Check(p.QueueWeaponHit(10,fo3player::PlayerRef,901),"queue physical weapon impact");worldBusy=true;fo3worldscript::Actions();fo3worldscript::Hits();worldBusy=false;
 Check(p.Snapshot().karma==impactKarma+2&&p.Snapshot().pipboy.dialogueVariables.at((uint64_t(10)<<32)|3)==1,"OnHit runs before weapon-filtered OnHitWith; the action reference stays the attacker even while weapon handling is busy");
 fo3worldscript::Hits();Check(p.Snapshot().karma==impactKarma+2,"impact is consumed once");
 Check(p.QueueWeaponHit(10,fo3player::PlayerRef,902),"queue different weapon");fo3worldscript::Actions();fo3worldscript::Hits();Check(p.Snapshot().karma==impactKarma+2,"weapon filter does not use attacker identity");
 Check(p.QueueWeaponHit(83,fo3player::PlayerRef,901),"queue unsupported hit script");fo3worldscript::Actions();fo3worldscript::Hits();Check(p.Snapshot().karma==impactKarma+2,"unsupported OnHitWith rolls back preceding OnHit in the same impact");
 // Exercise the production speech adapter with token-owned audio completion.
 fo3script::Action look;look.kind=fo3script::ActionKind::Look;look.reference=40;look.form=fo3player::PlayerRef;
 Check(p.QueueScriptAction(look),"queue look before actor arrival");fo3worldscript::Actions();
 ActorVisual voiceActor;voiceActor.source={40,400};gQ230NpcActors.push_back(voiceActor);++gQ230NpcResidenceRevision;fo3worldscript::Register();
 auto seats=std::make_shared<fo3furniture::Scene>();fo3furniture::Slot seat;seat.reference=888;seat.marker.reference=11;seats->slots.push_back(seat);
 auto& seated=gQ230NpcActors.back().runtime.furniture;seated.scene=seats;seated.slot=0;
 uint32_t observedRef;uint8_t observedMarker,observedState;
 for(auto phase:{fo3furniture::Phase::Approach,fo3furniture::Phase::Enter,fo3furniture::Phase::Loop,fo3furniture::Phase::Exit}){
   seated.phase=phase;fo3worldscript::Register();const auto expected=phase==fo3furniture::Phase::Approach?0:phase==fo3furniture::Phase::Enter?2:phase==fo3furniture::Phase::Loop?3:4;
   Check(p.ScriptFurniture(40,observedRef,observedMarker,observedState)&&observedState==expected,"furniture changes observed through registry cache hit");
   if(expected)Check(observedRef==888&&observedMarker==11,"authored marker reference reaches script queries");
 }
 gQ230NpcActors.back().runtime.offScene=true;fo3worldscript::Register();Check(!p.ScriptFurniture(40,observedRef,observedMarker,observedState),"off-scene seat observations expire");
 gQ230NpcActors.back().runtime.offScene=false;seated.Clear();fo3worldscript::Register();Check(p.ScriptFurniture(40,observedRef,observedMarker,observedState)&&!observedState,"resident standing observation replaces seat");
 Check(gQ230NpcActors.back().runtime.scriptLookTarget==fo3player::PlayerRef,"delayed actor arrival retains original look target");
 look.form=0;Check(p.QueueScriptAction(look),"stop retained look");fo3worldscript::Actions();
 Check(!gQ230NpcActors.back().runtime.scriptLookTarget&&!fo3worldscript::lookTargets.count(40),"stop look removes retained target");
 fo3script::Action idle;idle.kind=fo3script::ActionKind::PackageIdle;idle.reference=40;idle.form=701;idle.flags=4;
 Check(p.QueueScriptAction(idle),"queue authored package idle");fo3worldscript::Actions();Check(gQ230NpcActors.back().responseIdle==701,"package idle reaches resident animation adapter");
 fo3worldscript::SpeechTick(); // Initialize the independent session generation.
 fo3script::Action say;say.kind=fo3script::ActionKind::SayTo;say.reference=40;say.form=500;say.flags=fo3player::PlayerRef;
 Check(p.QueueScriptAction(say),"queue original SayTo");fo3worldscript::Actions();fo3worldscript::SpeechTick();
 Check(p.ScriptActorTalking(40)&&fo3audio::path=="sound/original.ogg","authored audio owns speech");
 const auto token=fo3audio::token;Check(gQ230NpcActors.back().runtime.scriptSpeechToken==token,"script lip owner follows the actual voice token");fo3audio::completion=(uint64_t(token+1)<<1)|1;fo3worldscript::SpeechTick();
 Check(p.ScriptActorTalking(40)&&!p.Snapshot().pipboy.dialogueVariables.count((uint64_t(40)<<32)|1),"unrelated completion cannot publish INFO results");
 fo3audio::completion=(uint64_t(token)<<1)|1;fo3worldscript::SpeechTick();
 Check(!p.ScriptActorTalking(40)&&gQ230NpcActors.back().runtime.scriptSpeechToken==0,"completion releases speaking actor and lip token even after Session::End clears actor");
 Check(p.Snapshot().pipboy.dialogueVariables.at((uint64_t(40)<<32)|1)==1&&p.Snapshot().pipboy.dialogueVariables.at((uint64_t(40)<<32)|2)==1,"INFO end precedes source filtered SayToDone");
 Check(p.SetDialogueVariable((uint64_t(40)<<32)|2,0),"clear SayToDone marker");
 Check(fo3worldscript::PackageEvent(40,850,false),"adopt original dialogue package");
 Check(p.QueueScriptSpeech(40,500,fo3player::PlayerRef,false,false,850),"queue package-specific speech");fo3worldscript::Actions();fo3worldscript::SpeechTick();
 fo3audio::completion=(uint64_t(fo3audio::token)<<1)|1;fo3worldscript::SpeechTick();
 Check(p.Snapshot().actorPackageEvents.at(40)==std::make_pair(uint32_t(850),true)&&p.Snapshot().pipboy.dialogueVariables.at((uint64_t(40)<<32)|2)==0,"package speech completes its package without sending SayToDone");
 for(bool success:{false,true}){
  worldDefer=true;const auto before=p.Snapshot().karma;
  Check(fo3worldscript::PackageEvent(40,852,false),"start world-result package");
  Check(p.QueueScriptSpeech(40,830,fo3player::PlayerRef,false,false,852),"queue world-result speech");
  worldBusy=true;fo3worldscript::Actions();fo3worldscript::SpeechTick();
  Check(!p.ScriptActorTalking(40)&&!fo3worldscript::speechPending.empty(),"busy admission preserves queued speech");
  worldBusy=false;fo3worldscript::SpeechTick();
  worldBusy=true;fo3audio::completion=(uint64_t(fo3audio::token)<<1)|1;fo3worldscript::SpeechTick();
  Check(fo3worldscript::speech.phase==fo3dialogue::Phase::Results&&!worldResultPlan,"busy world work retains the closing INFO");
  fo3worldscript::SpeechTick();Check(p.Snapshot().karma==before&&!worldResultPlan,"busy retry does not publish results");
  worldBusy=false;fo3worldscript::SpeechTick();
  Check(fo3worldscript::speech.phase==fo3dialogue::Phase::Results&&bool(worldResultPlan),"EOF waits for resident result transaction");
  fo3worldscript::SpeechTick();Check(p.ScriptActorTalking(40)&&p.Snapshot().karma==before&&p.Snapshot().actorPackageEvents.at(40)==std::make_pair(uint32_t(852),false),"pending result cannot complete package or replay EOF");
  if(success)Check(p.CommitScriptTransaction(*worldResultPlan),"resident result commit");
  worldResultPlan.reset();worldResult=success?1:-1;fo3worldscript::SpeechTick();
  Check(!p.ScriptActorTalking(40)&&!fo3worldscript::speech.Active(),"settled result releases speech");
  Check(p.Snapshot().karma==before+(success?3:0)&&p.Snapshot().actorPackageEvents.at(40)==std::make_pair(uint32_t(852),success),"only finalized results can complete their package");
  worldDefer=false;
 }
 // Restore this originally disabled reference before testing ghost speech.
 {fo3pipdata::ResultScript disable;disable.source="HiddenSpeaker.Disable";std::string error;auto reset=p.PrepareDialogueTransaction(disable,{},error);Check(bool(reset)&&p.CommitScriptTransaction(*reset),"restore disabled test reference");}
 const auto priorVoiceToken=fo3audio::token;Check(p.QueueScriptSpeech(82,820,fo3player::PlayerRef),"queue disabled reference speech");fo3worldscript::Actions();fo3worldscript::SpeechTick();
 Check(!p.ScriptActorTalking(82)&&fo3audio::token==priorVoiceToken,"disabled references cannot start ghost speech");
 Check(p.QueueScriptSpeech(81,820,fo3player::PlayerRef),"source Say selects non-actor TACT INFO");
 fo3worldscript::Actions();fo3worldscript::SpeechTick();
 Check(p.ScriptActorTalking(81)&&fo3audio::path=="sound/original.ogg","production TACT adapter delivers original speech without NPC mesh");
 fo3audio::completion=(uint64_t(fo3audio::token)<<1)|1;fo3worldscript::SpeechTick();
 Check(!p.ScriptActorTalking(81),"TACT speech releases on actual voice completion");
 Check(p.SetTalkingActivatorActor(81,40),"assign remote actor to intercom");
 Check(fo3worldscript::PackageEvent(41,851,false),"adopt specified conversation package");
 const auto expiredToken=fo3audio::token;
 Check(p.QueueScriptSpeech(41,910,81,true,false,851),"bind queued intercom actors");Check(p.SetTalkingActivatorActor(81,41),"replace queued target identity");
 fo3worldscript::Actions();fo3worldscript::SpeechTick();Check(fo3audio::token==expiredToken&&!p.ScriptActorTalking(41),"stale assigned actor cannot inherit queued dialogue");
 Check(p.SetTalkingActivatorActor(81,40),"restore intended intercom actor");
 const auto oldKarma=p.Snapshot().karma;
 Check(p.QueueScriptSpeech(41,910,81,true,false,851),"bind specified conversation topic");fo3worldscript::Actions();fo3worldscript::SpeechTick();
 Check(p.ScriptActorTalking(41),"initiating actor speaks first");
 fo3audio::completion=(uint64_t(fo3audio::token)<<1)|1;fo3worldscript::SpeechTick();
 Check(!p.ScriptActorTalking(41)&&p.ScriptActorTalking(40)&&p.ScriptActorTalking(81),"linked response owns actor identity and physical intercom");
 Check(fo3worldscript::speech.actor==40&&fo3worldscript::speech.proxy==81&&gQ230NpcActors.back().runtime.scriptSpeechToken==0,"proxy voice does not animate remote NPC mesh");
 fo3audio::completion=(uint64_t(fo3audio::token)<<1)|1;fo3worldscript::SpeechTick();
 Check(!p.ScriptActorTalking(40)&&!p.ScriptActorTalking(81)&&p.Snapshot().karma==oldKarma+2,"proxy end results publish once and both speech flags release");
 Check(p.Snapshot().actorPackageEvents.at(41)==std::make_pair(uint32_t(851),true),"completion belongs to initiating package actor");
 Check(p.SetTalkingActivatorActor(81,0),"restore ordinary activator voice");
 auto& fighter=gQ230NpcActors.back();fighter.runtime.BeginCombat(fo3player::PlayerRef);fighter.runtime.pendingAttack=true;
 Check(p.SetActorHostile(40,fo3player::PlayerRef),"canonical combat fixture");
 fo3pipdata::ResultScript stop,unsupported;stop.source="speaker.StopCombat";unsupported.source="UnsupportedStopStep";std::string stopError;
 Check(!p.PrepareDialogueTransaction(stop,unsupported,stopError),"combat stop preparation rolls back");fo3worldscript::Actions();
 Check(fighter.runtime.activity==fo3npc::Activity::Combat&&p.Snapshot().actors.at(40).hostile==fo3player::PlayerRef,"failed scripts leave combat live");
 Check(p.ExecuteDialogueResult(stop,stopError),"authored StopCombat accepted");Check(fighter.runtime.activity==fo3npc::Activity::Combat,"live combat changes only after commit delivery");
 fo3worldscript::Actions();Check(fighter.runtime.activity!=fo3npc::Activity::Combat&&!fighter.runtime.pendingAttack&&p.Snapshot().actors.at(40).hostile==0,"committed stop ends canonical and live combat");
 ActorVisual otherFighter;otherFighter.source={41,p.Definitions().pipboy.targets.at(41).base};gQ230NpcActors.push_back(otherFighter);++gQ230NpcResidenceRevision;
 for(auto& actor:gQ230NpcActors)actor.runtime.BeginCombat(actor.source.refFormId==40?fo3player::PlayerRef:40);
 Check(p.SetActorHostile(40,fo3player::PlayerRef)&&p.SetActorHostile(41,40),"separate combat targets");stop.source="Player.SCAOnActor";
 Check(!p.PrepareDialogueTransaction(stop,unsupported,stopError)&&p.Snapshot().actors.at(40).hostile==fo3player::PlayerRef,"failed group stop remains private");
 Check(p.ExecuteDialogueResult(stop,stopError)&&p.Snapshot().actors.at(40).hostile==0&&p.Snapshot().actors.at(41).hostile==40,"group stop clears matching canonical target only");
 fo3worldscript::Actions();Check(DialogueActor(40)->runtime.activity!=fo3npc::Activity::Combat&&DialogueActor(41)->runtime.combatTarget==40,"group stop preserves unrelated live combat");
 fo3pipdata::ResultScript reset,broken;reset.source="ResetPipboyManager\nForceRadioStationUpdate";broken.source="UnsupportedResetStep";
 std::string resetError;Check(!p.PrepareDialogueTransaction(reset,broken,resetError),"failed reset transaction is private");fo3worldscript::Actions();Check(!pipboyResets&&!radioRefreshes,"failed scripts cannot reset presentation");
 Check(p.ExecuteDialogueResult(reset,resetError),"queue original manager refresh commands");fo3worldscript::Actions();Check(pipboyResets==1&&radioRefreshes==1,"post-commit refresh reaches real adapters");
 Check(p.SetPlayerScale(.4f),"scripted toddler scale");
 Check(p.MoveActorTo(fo3player::PlayerRef,10,{}),"player MoveTo marker accepted");
 Check(p.SetLocationSpecificLoadScreens(true),"location-only loading flag");
 fo3script::Action blood;blood.kind=fo3script::ActionKind::ScreenBlood;blood.flags=3;Check(p.QueueScriptAction(blood),"queue screen blood");
 fo3worldscript::Actions();Check(playerCollisionScale==.4f&&playerTransitions==1&&fo3bloodui::triggered==3&&fo3loading::gLocationSpecificOnly.load(),"production player move and presentation actions");
 fo3worldscript::Actions();Check(playerTransitions==1,"pending player scene transition is not duplicated");
 p.AcknowledgePlayerMove();playerTransitionPending=false;
 fo3pipdata::ResultScript request;request.source="autosave";
 const auto savedBefore=saves;std::string saveError;fo3pipdata::ResultScript empty;
 auto savePlan=p.PrepareDialogueTransaction(request,empty,saveError);
 Check(bool(savePlan)&&saves==savedBefore,"autosave preparation cannot write live files");
 Check(p.CommitScriptTransaction(*savePlan),"autosave commit");
 fo3worldscript::Actions();Check(saves==savedBefore+1,"committed autosave reaches production persistence adapter");
 fo3worldscript::Actions();Check(saves==savedBefore+1,"autosave request consumed once");
 if(argc>1){OriginalIntercom(argv[1]);OriginalPrayer(argv[1]);}
 std::cout<<"World event/controller/presentation and original speech adapters passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
