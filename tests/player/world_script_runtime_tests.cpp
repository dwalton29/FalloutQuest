#include "player/fo3-player-state.h"
#include "world/interaction/fo3-door-nif-animation.h"
#include "world/fo3-cell-traversal.h"
struct Fo3DoorCollisionPose {fo3anim::Matrix transform=fo3anim::Identity();bool enabled=true;std::unordered_map<uint32_t,fo3anim::Matrix> nodeTransforms;};
#include <stdexcept>
#include <iostream>
static void Check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
#define Q6H_LOGW(...) ((void)0)
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
Fo3DoorCollisionPose collision;int collisionUpdates=0;
bool SetFo3ScriptCollisionPose(uint32_t,const Fo3DoorCollisionPose& pose){collision=pose;++collisionUpdates;return true;}
float gQ210Head[4]{};
Fo3DoorTeleport defaultTeleport;int defaultTransitions=0;uint32_t defaultSource=0,defaultDestination=0;bool localOpen=false;
bool ResolveFo3DoorTeleportQ1700(uint32_t,Fo3DoorTeleport* out){*out=defaultTeleport;return out->valid;}
bool QueueFo3DoorTransitionQ1700(uint32_t source,uint32_t destination,float x,float y,float z,float,float,float){Check(x==100&&y==200&&z==300,"original default arrival passed through");++defaultTransitions;defaultSource=source;defaultDestination=destination;return true;}
bool Q2400ToggleInteriorDoor(uint32_t,float,float){localOpen=!localOpen;return true;}
bool Q2400DoorOpen(uint32_t){return localOpen;}
#include "world/interaction/fo3-script-default-activation.inc"
#include "scripting/fo3-world-script-runtime.inc"
int main(){try{
 fo3player::Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);
 c.references[10].base=100;c.references[10].cell=1;c.pipboy.targets[10].cell=1;
 c.pipboy.triggerPrimitives[10]={{2,2,2},1,true};c.eventBaseScripts[100]=200;c.pipboy.objectReferenceScripts[10]={200,100};
 auto& script=c.pipboy.scripts[200];script.variables={{1,"opened"},{2,"loaded"}};
 script.source="scn C\nshort opened\nshort loaded\nBegin OnTriggerEnter player\nPlayGroup Advance 5\nEnd\nBegin OnLoad\nset loaded to loaded + 1\nEnd\nBegin GameMode\nif IsAnimPlaying == 0\nPlayGroup Idle 5\nendif\nEnd";
 c.eventPrograms[200]=fo3script::ParseEvents(script);
 c.references[30].base=300;c.doorBases.insert(300);c.scriptedBases.insert(300);c.eventBaseScripts[300]=301;
 c.pipboy.scripts[301].source="Begin OnActivate\nActivate\nEnd";c.eventPrograms[301]=fo3script::ParseEvents(c.pipboy.scripts[301]);
 auto asset=std::make_shared<fo3dooranim::Asset>();fo3anim::Bone root;root.name="root";asset->hierarchy.bones.push_back(root);fo3anim::Bone leaf;leaf.name="leaf";leaf.parent=0;asset->hierarchy.bones.push_back(leaf);
 Check(fo3anim::FinalizeSkeleton(asset->hierarchy),"hierarchy");asset->blockBones={0,1};
 fo3anim::Clip clip;clip.name="Advance";clip.stop=2;clip.cycle=2;
 fo3anim::Track track;track.bone="leaf";track.hasTranslation=true;track.translation.dimensions=3;track.translation.interpolation=1;track.translation.keys={{0,{0,0,0,0},{},{}},{2,{4,0,0,0},{},{}}};clip.tracks.push_back(track);
 fo3anim::PropertyTrack alpha;alpha.bone="leaf";alpha.kind=fo3anim::PropertyKind::Alpha;alpha.values.dimensions=1;alpha.values.interpolation=1;alpha.values.keys={{0,{0,0,0,0},{},{}},{2,{1,0,0,0},{},{}}};clip.properties.push_back(alpha);asset->clips.push_back(clip);
 clip.name="Idle";clip.cycle=0;clip.stop=1;clip.tracks[0].translation.keys={{0,{4,0,0,0},{},{}}};asset->clips.push_back(clip);
 gPlayerSession=std::make_unique<fo3player::Session>(std::move(c));auto& p=gPlayerSession->player;
 GpuObject object;object.refFormId=10;object.q2401DoorAnimation=asset;gObjects.push_back(object);
 p.SetScriptPlayerLocation(0,0,0,1,0);fo3worldscript::Register();p.SetScriptRuntimeClock(0,.25);fo3worldscript::Events();fo3worldscript::Actions();fo3worldscript::Animate();
 Check(fo3worldscript::animations.at(10).clip==0,"trigger plays authored group");Check(p.ScriptTriggerChanges().empty(),"committed trigger acknowledged");
 fo3worldscript::seconds=1;p.SetScriptRuntimeClock(1,.25);fo3worldscript::Events();fo3worldscript::Animate();
 Check(std::abs(gObjects[0].q2400DoorTransform[12]-2)<1e-5&&gObjects[0].materialAlpha==.5f,"live node transform and alpha");
 Check(collision.nodeTransforms.at(1)[12]==2&&collision.nodeTransforms.at(0)[12]==0,"independent collision node transforms");
 fo3worldscript::seconds=2;p.SetScriptRuntimeClock(2,.25);fo3worldscript::Animate();
 bool reserved=p.PumpWorldAwareGameMode(.25,{10},[](auto){return false;});Check(!reserved,"non-world animation transaction");fo3worldscript::Actions();fo3worldscript::Animate();
 Check(fo3worldscript::animations.at(10).clip==1&&gObjects[0].q2400DoorTransform[12]==4,"source waits and selects idle");
 // Re-publication reapplies the retained pose and does not invent another OnLoad.
 ++gFo3VisualPublication.revision;gObjects[0].q2400DoorTransform[12]=0;fo3worldscript::Register();fo3worldscript::Animate();Check(gObjects[0].q2400DoorTransform[12]==4,"stream publication retains pose");
 defaultTeleport.valid=true;defaultTeleport.destinationDoorRefFormId=31;defaultTeleport.x=100;defaultTeleport.y=200;defaultTeleport.z=300;
 Check(fo3worldscript::Event(30,"OnActivate",fo3player::PlayerRef),"source delegates default activation");fo3worldscript::Actions();
 Check(defaultTransitions==1&&defaultSource==30&&defaultDestination==31,"production adapter queues original door transition");
 Check(fo3worldscript::Event(30,"OnActivate",fo3player::PlayerRef),"queue another source activation");Check(p.ScriptLock(30,100),"lock before delivery");fo3worldscript::Actions();Check(defaultTransitions==1,"delivery rechecks changed lock");
 Check(p.ScriptUnlock(30),"unlock local door");defaultTeleport.valid=false;Check(fo3worldscript::Event(30,"OnActivate",fo3player::PlayerRef),"source delegates local door");fo3worldscript::Actions();Check(localOpen,"production local swing default");
 std::cout<<"World event/controller/presentation adapters passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
