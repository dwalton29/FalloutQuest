#include "player/fo3-player-state.h"
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
bool PublishFo3DialogueReferences(fo3player::Player& p,fo3player::ScriptTransactionPlan& plan,std::string&){return p.CommitScriptTransaction(plan);}
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
fo3dialogue::Session gDialogue;
struct Chat{bool Active()const{return false;}}gQ230NpcChat;
uint32_t gCurrentCellFormId=1,gExteriorWorldspaceQ1890=0;
float gSceneCenterXQ1730=0,gSceneCenterYQ1730=0,gSceneFloorZQ1730=0;
constexpr float SCENE_FORWARD=0,FLOOR_Y=0;
bool IsFo3LoadingVisible(){return false;}
bool PreviewFo3DialogueReferences(const fo3player::Player&,const fo3player::ScriptTransactionPlan&,std::string&){return true;}
namespace fo3audio {
uint32_t token=0;uint64_t completion=0;std::string path;std::vector<uint32_t> sounds;
void SoundEvent(uint32_t id){sounds.push_back(id);}
void DialogueStop(){token=0;}
void DialogueGain(float){}
void Dialogue(const std::string& p,uint32_t t){path=p;token=t;completion=0;}
uint64_t DialogueCompletion(){return completion;}
}
#include "scripting/fo3-world-script-runtime.inc"
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
 script.source="scn C\nshort opened\nshort loaded\nBegin OnTriggerEnter player\nPlayGroup Advance 5\nEnd\nBegin OnLoad\nset loaded to loaded + 1\nEnd\nBegin GameMode\nif IsAnimPlaying == 0\nPlayGroup Idle 5\nendif\nEnd";
 c.eventPrograms[200]=fo3script::ParseEvents(script);
 c.references[81].base=810;c.pipboy.targets[81].base=810;c.pipboy.targets[81].cell=1;
 c.pipboy.dialogueActors[810].talkingActivator=true;
 c.references[80].base=800;c.containers[800].script=801;c.containers[800].entries={{900,0,2}};
 c.items[900].formId=900;c.items[900].name="Original goods";c.eventBaseScripts[800]=801;
 c.pipboy.scripts[801].source="Begin OnActivate\nActivate player\nEnd";c.eventPrograms[801]=fo3script::ParseEvents(c.pipboy.scripts[801]);
 c.references[30].base=300;c.doorBases.insert(300);c.scriptedBases.insert(300);c.eventBaseScripts[300]=301;
 c.pipboy.scripts[301].source="Begin OnActivate\nActivate\nEnd";c.eventPrograms[301]=fo3script::ParseEvents(c.pipboy.scripts[301]);
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
 auto asset=std::make_shared<fo3dooranim::Asset>();fo3anim::Bone root;root.name="root";asset->hierarchy.bones.push_back(root);fo3anim::Bone leaf;leaf.name="leaf";leaf.parent=0;asset->hierarchy.bones.push_back(leaf);
 Check(fo3anim::FinalizeSkeleton(asset->hierarchy),"hierarchy");asset->blockBones={0,1};
 fo3anim::Clip clip;clip.name="Advance";clip.stop=2;clip.cycle=2;clip.textKeys={{.1f,"sound: OriginalSound"},{1.5f,"OriginalSound"}};
 fo3anim::Track track;track.bone="leaf";track.hasTranslation=true;track.translation.dimensions=3;track.translation.interpolation=1;track.translation.keys={{0,{0,0,0,0},{},{}},{2,{4,0,0,0},{},{}}};clip.tracks.push_back(track);
 fo3anim::PropertyTrack alpha;alpha.bone="leaf";alpha.kind=fo3anim::PropertyKind::Alpha;alpha.values.dimensions=1;alpha.values.interpolation=1;alpha.values.keys={{0,{0,0,0,0},{},{}},{2,{1,0,0,0},{},{}}};clip.properties.push_back(alpha);asset->clips.push_back(clip);
 clip.textKeys.clear();clip.name="Idle";clip.cycle=0;clip.stop=1;clip.tracks[0].translation.keys={{0,{4,0,0,0},{},{}}};asset->clips.push_back(clip);
 gPlayerSession=std::make_unique<fo3player::Session>(std::move(c));auto& p=gPlayerSession->player;
 GpuObject object;object.refFormId=10;object.q2401DoorAnimation=asset;gObjects.push_back(object);
 p.SetScriptPlayerLocation(0,0,0,1,0);fo3worldscript::Register();p.SetScriptRuntimeClock(0,.25);fo3worldscript::Events();fo3worldscript::Actions();fo3worldscript::Animate();
 Check(fo3worldscript::animations.at(10).clip==0,"trigger plays authored group");Check(p.ScriptTriggerChanges().empty(),"committed trigger acknowledged");
 fo3worldscript::seconds=1;p.SetScriptRuntimeClock(1,.25);fo3worldscript::Events();fo3worldscript::Animate();
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
 // Exercise the production speech adapter with token-owned audio completion.
 ActorVisual voiceActor;voiceActor.source={40,400};gQ230NpcActors.push_back(voiceActor);++gQ230NpcResidenceRevision;
 fo3worldscript::SpeechTick(); // Initialize the independent session generation.
 fo3script::Action say;say.kind=fo3script::ActionKind::SayTo;say.reference=40;say.form=500;say.flags=fo3player::PlayerRef;
 Check(p.QueueScriptAction(say),"queue original SayTo");fo3worldscript::Actions();fo3worldscript::SpeechTick();
 Check(p.ScriptActorTalking(40)&&fo3audio::path=="sound/original.ogg","authored audio owns speech");
 const auto token=fo3audio::token;Check(gQ230NpcActors.back().runtime.scriptSpeechToken==token,"script lip owner follows the actual voice token");fo3audio::completion=(uint64_t(token+1)<<1)|1;fo3worldscript::SpeechTick();
 Check(p.ScriptActorTalking(40)&&!p.Snapshot().pipboy.dialogueVariables.count((uint64_t(40)<<32)|1),"unrelated completion cannot publish INFO results");
 fo3audio::completion=(uint64_t(token)<<1)|1;fo3worldscript::SpeechTick();
 Check(!p.ScriptActorTalking(40)&&gQ230NpcActors.back().runtime.scriptSpeechToken==0,"completion releases speaking actor and lip token even after Session::End clears actor");
 Check(p.Snapshot().pipboy.dialogueVariables.at((uint64_t(40)<<32)|1)==1&&p.Snapshot().pipboy.dialogueVariables.at((uint64_t(40)<<32)|2)==1,"INFO end precedes source filtered SayToDone");
 Check(p.QueueScriptSpeech(81,820,fo3player::PlayerRef),"source Say selects non-actor TACT INFO");
 fo3worldscript::Actions();fo3worldscript::SpeechTick();
 Check(p.ScriptActorTalking(81)&&fo3audio::path=="sound/original.ogg","production TACT adapter delivers original speech without NPC mesh");
 fo3audio::completion=(uint64_t(fo3audio::token)<<1)|1;fo3worldscript::SpeechTick();
 Check(!p.ScriptActorTalking(81),"TACT speech releases on actual voice completion");
 if(argc>1)OriginalPrayer(argv[1]);
 std::cout<<"World event/controller/presentation and original speech adapters passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
