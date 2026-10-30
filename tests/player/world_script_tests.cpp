#include "player/fo3-player-state.h"
#include "dialogue/fo3-dialogue-results.h"
#include "dialogue/fo3-dialogue-session.h"
#include "dialogue/fo3-dialogue-selection.h"
#include "scripting/fo3-nonresident-script.h"
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <cstdio>
#include <filesystem>
#include "world/fo3-worldspace-runtime.h"
#include "ui/interaction/fo3-compass-state.h"
using namespace fo3player;
// Compile the actual native activation adapter, with resource publication
// supplied by this host scene. Original door events use the same route as A.
std::unique_ptr<fo3player::Session> gPlayerSession;
uint64_t gFo3ResidentGameplayTicket=0;
uint32_t activationCell=0,activationWorld=0;
unsigned activationReservations=0;
std::vector<uint32_t> activationLive;
bool Fo3ResidentFrameBlocked(){return false;}
void FlushFo3PlayerState(){}
bool PublishFo3DialogueReferences(Player& player,ScriptTransactionPlan& plan,std::string& error){
  return fo3script::NonresidentActorRemoval(player,plan,activationCell,activationWorld,activationLive,error)&&player.CommitScriptTransaction(plan);
}
struct ActivationRequest {};
uint64_t BeginFo3ResidentPreparedWorkflow(ActivationRequest,std::unique_ptr<ScriptTransactionPlan>){++activationReservations;return 0;}
#include "world/interaction/fo3-script-activation-runtime.inc"
static void Check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
static float Local(const Player& p,const std::string& name){
  const auto key=fo3dialogue::VariableKey(p.Definitions().pipboy,name);
  const auto i=p.Snapshot().pipboy.dialogueVariables.find(key);return i==p.Snapshot().pipboy.dialogueVariables.end()?0:i->second;
}
static void Synthetic(){
  Check(std::abs(fo3compass::Relative(0,100,0))<1e-6f,"north centered");
  Check(fo3compass::Relative(100,0,0)>0,"east moves right");
  Check(std::abs(fo3compass::Relative(-.001f,100,2*fo3compass::Pi))<.001f,"north seam wraps");

  {
    Catalog navigation;navigation.initial.baseHealth=100;navigation.initial.special.fill(5);
    auto& d=navigation.pipboy;d.targets[800].cell=30;d.targets[800].world=1;d.targets[800].x=100;d.targets[800].y=100;
    auto& objective=d.quests[900].objectives[10];objective.conditionalTargets=true;fo3pipdata::ObjectiveTarget target;target.reference=800;
    fo3pipdata::Condition condition;condition.function=72;condition.a=PlayerBase;condition.value=1;target.conditions.push_back(condition);objective.targetItems.push_back(target);
    auto& progress=navigation.initial.pipboy.quests[900];progress.objectives[10].displayed=true;navigation.initial.pipboy.selectedQuest=900;
    fo3pipdata::Placement door;door.cell=10;door.x=20;door.y=40;door.teleportRef=802;d.doors[10].push_back(door);d.targets[802].cell=30;d.targets[802].world=1;
    Player player(navigation);player.SetScriptPlayerLocation(0,0,0,10,0);
    const auto targets=fo3pipdata::NavigationTargets(player);Check(targets.size()==1&&targets.front()==800,"conditional objective uses original player context");
    fo3pipdata::Point point;Check(fo3compass::TargetPoint(player,{10,0,0,0,0},800,point)&&point.x==20&&point.y==40,"interior quest compass points to exit door");
    const auto compass=fo3compass::Build(player,{10,0,0,0,0},0);Check(compass.icons.size()==1&&compass.icons[0].kind==fo3compass::Kind::Quest,"quest icon reaches compass frame");
  }
  {
    Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);c.pipboy.referenceScripts[10]={"Speaker",20};
    fo3pipdata::Info daily;daily.id=40;daily.topic=30;daily.flags2=1;c.pipboy.topics[30].push_back(daily);
    Player p(c);Check(p.CanSayDailyInfo(10,40)&&p.RecordDialogue(10,0,{},false,40)&&!p.CanSayDailyInfo(10,40),"authored daily INFO cooldown");
    const auto path="/tmp/fq-daily-"+std::to_string(getpid());std::string error;Check(p.Save(path,error),error);Player restored(c);Check(restored.Restore(path,error),error);std::remove(path.c_str());
    Check(!restored.CanSayDailyInfo(10,40)&&restored.AdvanceGameHours(11)&&!restored.CanSayDailyInfo(10,40),"daily INFO history persists before original 12-hour timer");
    Check(restored.AdvanceGameHours(1)&&restored.CanSayDailyInfo(10,40),"original timer expiry permits another line");
  }
  {Catalog speech;speech.initial.baseHealth=100;speech.initial.special.fill(5);
    speech.references[10].base=20;speech.references[10].cell=1;speech.pipboy.targets[10].base=20;speech.pipboy.targets[10].cell=1;
    speech.eventBaseScripts[20]=30;speech.pipboy.referenceScripts[10]={"Speaker",20};speech.pipboy.dialogueActors[20].script=30;
    speech.pipboy.formNames["speaker"]=10;speech.pipboy.formNames["voicetopic"]=40;
    auto& source=speech.pipboy.scripts[30];source.variables[1]="waiting";source.source="scn voice\nshort waiting\nBegin GameMode\nset waiting to 1\nSay VoiceTopic\nEnd";speech.eventPrograms[30]=fo3script::ParseEvents(source);
    speech.pipboy.quests[60].flags=1;speech.pipboy.sounds[70]="sound/original.wav";
    fo3pipdata::Info line;line.id=50;line.topic=40;line.quest=60;line.type=1;line.responses.push_back({70,0,"Original line"});speech.pipboy.topics[40].push_back(line);
    Player valid(speech);valid.SetScriptPlayerLocation(0,0,0,1,0);std::string error;auto plan=valid.PrepareReferenceEventTransaction(10,"GameMode",PlayerRef,error,ScriptProgramCapture::EnteredPrograms);
    Check(bool(plan)&&valid.CommitScriptTransaction(*plan),"captured Say selects without nested detached publication: "+error);
    fo3script::Action queued;Check(valid.PollScriptAction(queued)&&queued.info==50,"captured Say pins original INFO");
    speech.pipboy.topics[40][0].end.source="UnsupportedCommand 1";Player invalid(speech);invalid.SetScriptPlayerLocation(0,0,0,1,0);
    auto rejected=invalid.PrepareReferenceEventTransaction(10,"GameMode",PlayerRef,error,ScriptProgramCapture::EnteredPrograms);
    Check(!rejected&&Local(invalid,"speaker.waiting")==0&&!invalid.PollScriptAction(queued),"unsupported speech preflight cannot commit waiting flags without a line");
  }
  Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);
  c.references[10].base=100;c.references[10].cell=1;
  c.pipboy.targets[10].cell=1;c.pipboy.targets[10].scale=2;c.pipboy.targets[10].rz=1.57079632679f;
  c.pipboy.triggerPrimitives[10]={{2,4,6},1,true};
  c.eventBaseScripts[100]=200;c.pipboy.objectReferenceScripts[10]={200,100};
  auto& script=c.pipboy.scripts[200];script.variables[1]="count";
  script.source="scn controller\nshort count\nBegin OnTriggerEnter Player\nset count to count + 1\nPlayGroup Advance 5\nEnd\nBegin OnTriggerLeave Player\nset count to count + 1\nEnd";
  c.eventPrograms[200]=fo3script::ParseEvents(script);
  Player p(std::move(c));auto registry=std::make_shared<fo3script::AnimationRegistry>();
  (*registry)[10]={{"Advance",2,false},{"Idle",1,true}};p.SetScriptAnimationRegistry(registry);p.SetScriptRuntimeClock(1,.25);
  p.SetScriptPlayerLocation(7,0,0,1,0);auto changes=p.ScriptTriggerChanges();Check(changes.size()==1&&changes[0].second,"rotated scaled XPRM entry");
  std::string error;auto plan=p.PrepareReferenceEventTransaction(10,"OnTriggerEnter",PlayerRef,error);
  Check(bool(plan),error);fo3script::Action action;Check(!p.PollScriptAction(action),"preparation has no effect");
  Check(p.CommitScriptTransaction(*plan),"commit candidate");
  bool rolledPlaying=false;Check(p.ScriptAnimationPlaying(10,rolledPlaying)&&rolledPlaying,"candidate playback published");
  Check(p.RollbackScriptTransaction(*plan)&&!p.PollScriptAction(action),"rollback removes queued controller action");
  Check(p.ScriptAnimationPlaying(10,rolledPlaying)&&!rolledPlaying,"rollback restores playback");
  plan=p.PrepareReferenceEventTransaction(10,"OnTriggerEnter",PlayerRef,error);Check(bool(plan),error);
  Check(p.CommitScriptTransaction(*plan),"commit event");Check(p.PollScriptAction(action)&&action.group=="advance"&&action.started==1,"source group action");
  bool playing=false;Check(p.ScriptAnimationPlaying(10,playing)&&playing,"playing query");
  p.AcknowledgeScriptTrigger(10,true);Check(p.ScriptTriggerChanges().empty(),"no repeated entry");
  p.SetScriptRuntimeClock(3,.25);Check(p.ScriptAnimationPlaying(10,playing)&&!playing,"exact duration completion");
  p.SetScriptPlayerLocation(0,5,0,1,0);changes=p.ScriptTriggerChanges();Check(changes.size()==1&&!changes[0].second,"rotated XPRM leave");
  p.AcknowledgeScriptTrigger(10,false);p.SetScriptPlayerLocation(0,0,0,2,0);Check(p.ScriptTriggerChanges().empty(),"other cell excluded");
  Check(!p.QueueScriptPlayGroup(10,"Missing",5),"unknown group rejects");
  const std::string save="/tmp/fq-world-script-"+std::to_string(getpid())+".sav";
  Check(p.Save(save,error),error);const auto generation=p.ScriptRuntimeGeneration();
  Check(p.QueueScriptPlayGroup(10,"Idle",5),"transient idle");p.AcknowledgeScriptTrigger(10,true);
  Check(p.Restore(save,error),error);std::remove(save.c_str());
  Check(p.ScriptRuntimeGeneration()!=generation&&!p.PollScriptAction(action),"restore invalidates transient presentation");
  Check(p.ScriptAnimationPlaying(10,playing)&&!playing,"restore clears animation clock");
  p.SetScriptPlayerLocation(0,0,0,1,0);Check(p.ScriptTriggerChanges().size()==1,"restore starts fresh spatial lifecycle");
  // A source command delegates the default action; the normal A policy still
  // blocks scripted doors until their event explicitly authorizes the command.
  Catalog doors;doors.initial.baseHealth=100;doors.initial.special.fill(5);
  doors.references[30].base=300;doors.doorBases.insert(300);doors.scriptedBases.insert(300);doors.eventBaseScripts[300]=301;
  auto& ds=doors.pipboy.scripts[301];ds.source="Begin OnActivate\nif 0\nplayer.scaonactor\nforceSave\nendif\nActivate\nEnd";
  doors.eventPrograms[301]=fo3script::ParseEvents(ds);
  Player doorPlayer(doors);Check(!doorPlayer.CanOpenDoor(30)&&doorPlayer.CanScriptActivateDoor(30),"explicit source delegation preserves ordinary script gate");
  auto delegated=doorPlayer.PrepareReferenceEventTransaction(30,"OnActivate",PlayerRef,error);Check(bool(delegated),error);
  Check(!doorPlayer.PollScriptAction(action)&&doorPlayer.CommitScriptTransaction(*delegated),"default action is transactional");
  Check(doorPlayer.PollScriptAction(action)&&action.kind==fo3script::ActionKind::DefaultActivate&&action.reference==30,"source default action queued");
  Check(doorPlayer.ScriptLock(30,100)&&!doorPlayer.CanScriptActivateDoor(30),"source delegation respects locks");
  doors.pipboy.scripts[301].source="Begin OnActivate\nif 1\nforceSave\nendif\nActivate\nEnd";doors.eventPrograms[301]=fo3script::ParseEvents(doors.pipboy.scripts[301]);
  Player unsupported(doors);Check(!unsupported.PrepareReferenceEventTransaction(30,"OnActivate",PlayerRef,error)&&!unsupported.PollScriptAction(action),"executed unsupported operation rejects entire event");
  // Image curves are sampled over the authored duration, not reduced to last keys.
  fo3script::ImageModifier m;m.valid=true;m.duration=10;m.fade.keys={{0,{1,1,1,.8f}},{1,{1,1,1,0}}};m.blur.keys={{0,{2,0,0,0}},{1,{0,0,0,0}}};
  auto image=fo3script::SampleImageModifier(m,5);Check(std::abs(image.fade[3]-.4f)<.001&&image.blur==1,"temporal modifier midpoint");
  Check(fo3script::SampleImageModifier(m,10).fade[3]==0,"modifier expires");
}
static void Original(const char* path){
  const auto root="/tmp/fq-world-original-"+std::to_string(getpid());std::filesystem::create_directory(root);
  std::filesystem::create_symlink(std::filesystem::absolute(path),root+"/Fallout3.esm");Check(setenv("FALLOUTQUEST_DATA_ROOT",root.c_str(),1)==0,"original data root");
  struct Cleanup {std::string root;~Cleanup(){std::filesystem::remove(root+"/Fallout3.esm");std::filesystem::remove(root);}} cleanup{root};
  Catalog c;std::string error;Check(LoadCatalog(path,c,error),error);c.initial.gameHour=12;gPlayerSession=std::make_unique<Session>(std::move(c));auto& p=gPlayerSession->player;
  const auto& d=p.Definitions().pipboy;auto id=[&](const char* name){auto f=d.formNames.find(name);Check(f!=d.formNames.end(),std::string("missing ")+name);return f->second;};
  const auto& dc=d.worlds.at(0x18de6);Check(dc.centreX==55296&&dc.centreY==-20480&&dc.scale==.5f&&dc.offsetX==2048&&dc.offsetY==6076,"original Chevy Chase map metadata");
  const auto transformed=fo3pipdata::WorldPoint(d,0x18de6,32768,-45056);
  Check(transformed.x==46080&&transformed.y==-26692,"original child-world scale pivot and game-unit offset");
  const auto projected=fo3pipdata::MapProject(d,0x18de6,32768,-45056);
  Check(std::abs(projected.x-(.1015625f+.796875f*(41.25f/51)))<1e-6f,"original map-art border and whole-cell extent");
  const auto inverse=d.worlds.at(0x3c).Unproject(projected);Check(std::abs(inverse.x-transformed.x)<.05f&&std::abs(inverse.y-transformed.y)<.05f,"world-map waypoint inversion");
  std::cout<<"Original executable map border, whole cells and ESM child-world scale/offset verified\n";
  const uint32_t house=id("megatongatehouseref"),weld=id("megatongateprotectronref");
  auto registry=std::make_shared<fo3script::AnimationRegistry>();(*registry)[house]={{"Forward",15,false},{"Backward",15,false},{"Left",3,true},{"SpecialIdle",3,true}};
  p.SetScriptAnimationRegistry(registry);p.SetScriptRuntimeClock(100,.25);
  uint32_t trigger=0;for(const auto& t:d.triggerPrimitives){const auto ref=p.Definitions().references.find(t.first);if(ref!=p.Definitions().references.end()){
    const auto script=p.Definitions().eventBaseScripts.find(ref->second.base);
    if(script!=p.Definitions().eventBaseScripts.end()&&script->second==id("megatongatetriggerscript"))trigger=t.first;
  }}Check(trigger!=0,"source gate trigger");
  const auto& gateVolume=d.targets.at(trigger);p.SetScriptPlayerLocation(gateVolume.x,gateVolume.y,gateVolume.z,gateVolume.cell,gateVolume.world);
  auto changes=p.ScriptTriggerChanges();Check(std::find(changes.begin(),changes.end(),std::make_pair(trigger,true))!=changes.end(),"original gate volume retained and entered");
  std::vector<Fo3WorldPlacement> placements;
  Check(LoadFo3WorldspacePlacements({gateVolume.world,0x2db4,gateVolume.x,gateVolume.y,1},placements),"original gate neighborhood");
  std::vector<uint32_t> resident;bool entrance=false;
  for(const auto& placement:placements){resident.push_back(placement.refFormId);entrance|=placement.refFormId==0x3b24;}
  Check(entrance&&std::find(resident.begin(),resident.end(),house)!=resident.end(),"authored entrance and scripted gatehouse survive real scene selection");
  auto enter=p.PrepareReferenceEventTransaction(trigger,"OnTriggerEnter",PlayerRef,error);Check(bool(enter),"gate enter: "+error);Check(p.CommitScriptTransaction(*enter),"gate trigger commit");
  Check(Local(p,"megatongatehouseref.opennow")==1&&Local(p,"megatongateprotectronref.playernear")==1,"original cross-reference locals");
  auto run=[&](const char* event){if(std::string(event)=="GameMode"){
    p.PumpWorldAwareGameMode(.25,resident,[](auto){return false;});return;
  }auto plan=p.PrepareReferenceEventTransaction(house,event,0,error);Check(bool(plan),std::string(event)+": "+error);Check(p.CommitScriptTransaction(*plan),"gate commit");};
  run("OnLoad");run("GameMode");fo3script::Action action;Check(p.PollScriptAction(action)&&action.group=="forward","original Forward");Check(!p.PollScriptAction(action),"idle cannot interrupt opening");
  p.SetScriptRuntimeClock(114.99,.25);run("GameMode");Check(!p.PollScriptAction(action),"source waits for duration");
  p.SetScriptRuntimeClock(115,.25);run("GameMode");Check(p.PollScriptAction(action)&&action.group=="left"&&Local(p,"megatongatehouseref.opened")==2,"source Left at completion");
  run("OnLoad");run("GameMode");Check(p.PollScriptAction(action)&&action.group=="left","source reload idle");
  (void)weld;
  // The saloon door's original script is independent of MQ01. Its time
  // branches change customers in the destination CELL and then delegate the
  // ordinary door action. Repeated presses/time changes must not drop it.
  const auto saloon=id("megmoriartysmaindoorexref");const auto& front=d.targets.at(saloon);
  p.SetScriptPlayerLocation(front.x,front.y,front.z,front.cell,front.world);
  Check(!p.GetLock(saloon).locked&&p.CanScriptActivateDoor(saloon),"original saloon entrance is unlocked");
  activationCell=front.cell;activationWorld=front.world;activationLive={saloon};
  const auto customer1=id("megatonmoriartyscustomer01ref"),customer2=id("megatonmoriartyscustomer02ref"),customer3=id("megatonmoriartyscustomer03ref");
  for(const auto& time:std::vector<std::pair<int,int>>{{0,0},{3,0},{3,1},{12,0},{18,0},{23,0},{23,1}}){
    Check(p.DebugSetTime(time.first,time.second),"set saloon clock");const auto before=p.Revision();
    auto door=p.PrepareReferenceEventTransaction(saloon,"OnActivate",PlayerRef,error,ScriptProgramCapture::EnteredPrograms);
    Check(bool(door),"original saloon script: "+error);Check(p.Revision()==before&&!p.PollScriptAction(action),"saloon preview has no effects");
    if(door->HasReferenceChanges())Check(fo3script::NonresidentActorRemoval(p,*door,front.cell,front.world,{saloon},error),"saloon customer publication: "+error);
    Check(DispatchFo3ScriptedActivation(saloon,PlayerRef,error)==ScriptEventResult::Executed,"native saloon activation adapter: "+error);
    Check(activationReservations==0,"off-scene customers do not request resident mesh publication");bool disabled=false;
    const float hour=p.Snapshot().gameHour;const bool evening=hour>=18&&hour<=23,late=hour<=3;
    Check(p.ReferenceDisabled(customer1,disabled,error)&&!disabled,"first original saloon customer enabled");
    Check(p.ReferenceDisabled(customer2,disabled,error)&&disabled==!(evening||late),"second saloon customer schedule");
    Check(p.ReferenceDisabled(customer3,disabled,error)&&disabled==!evening,"third saloon customer schedule");
    Check(p.PollScriptAction(action)&&action.kind==fo3script::ActionKind::DefaultActivate&&action.reference==saloon,"original saloon default activation delivered");
    Check(!p.PollScriptAction(action),"one saloon activation per press");
  }
  Check(p.DebugSetTime(12,0),"restore daylight for cave presentation check");
  std::cout<<"Original saloon time branches and native activation adapter passed\n";
  p.SetScriptPlayerLocation(gateVolume.x,gateVolume.y,gateVolume.z,gateVolume.cell,gateVolume.world);
  // Actual cave exit OnActivate must reach its authored fallback when CG04
  // is not running. No forced quest stage or source replacement is involved.
  auto cave=p.PrepareReferenceEventTransaction(id("vault101cavedoorinterior"),"OnActivate",PlayerRef,error);
  Check(bool(cave),"original cave exit: "+error);Check(p.CommitScriptTransaction(*cave),"cave default delegation commit");
  Check(p.PollScriptAction(action)&&action.kind==fo3script::ActionKind::DefaultActivate,"original cave exit default action");
  // Sun-blind trigger is located by its base SCRI, not by a runtime hardcode.
  trigger=0;for(const auto& t:d.triggerPrimitives){const auto ref=p.Definitions().references.find(t.first);if(ref==p.Definitions().references.end())continue;const auto script=p.Definitions().eventBaseScripts.find(ref->second.base);if(script!=p.Definitions().eventBaseScripts.end()&&script->second==id("cg04sunblindscript"))trigger=t.first;}
  Check(trigger!=0,"source exit trigger");
  const auto& exitVolume=d.targets.at(trigger);p.SetScriptPlayerLocation(exitVolume.x,exitVolume.y,exitVolume.z,exitVolume.cell,exitVolume.world);
  changes=p.ScriptTriggerChanges();Check(std::find(changes.begin(),changes.end(),std::make_pair(trigger,true))!=changes.end(),"original exit volume retained and entered");
  auto exit=p.PrepareReferenceEventTransaction(trigger,"OnTriggerEnter",PlayerRef,error);Check(bool(exit),"exit: "+error);
  Check(exit->HasReferenceChanges(),"exit enables original references");
  const bool admitted=fo3script::NonresidentActorRemoval(p,*exit,exitVolume.cell,exitVolume.world,{},error);Check(admitted,"deferred enable policy: "+error);
  Check(p.CommitScriptTransaction(*exit),"exit commit");
  bool disabled=true;Check(p.ReferenceDisabled(id("radioenclaveref"),disabled,error)&&!disabled,"radio state enabled");
  Check(p.ReferenceDisabled(id("springvaleeyebotref"),disabled,error)&&!disabled,"eyebot state enabled");
  uint32_t sound=0;Check(p.PollScriptSound(sound)&&sound==id("musexitthevault"),"source exit music");
  Check(p.PollScriptAction(action)&&action.kind==fo3script::ActionKind::ImageModifier&&action.form==id("demoeyeadjustisfx"),"source eye adjustment");
  const auto& modifier=p.Definitions().imageModifiers.at(action.form);Check(modifier.valid&&modifier.duration==10,"original modifier duration");
  Check(fo3script::SampleImageModifier(modifier,0).fade[3]>.8f&&fo3script::SampleImageModifier(modifier,0).blur==2.5f,"original fade and blur");
  exit=p.PrepareReferenceEventTransaction(trigger,"OnTriggerEnter",PlayerRef,error);Check(bool(exit)&&p.CommitScriptTransaction(*exit),"second exit");Check(!p.PollScriptSound(sound)&&!p.PollScriptAction(action),"source doOnce");
  {Player live(p.Definitions());live.RegisterOriginalActors();const auto& position=d.targets.at(id("gobref"));
    live.SetScriptPlayerLocation(position.x,position.y+40,position.z,position.cell,position.world);
    live.SetScriptEventDiagnostic([](const std::string& message){std::cerr<<message<<"\n";});
    for(int frame=0;frame<30;++frame){live.SetScriptRuntimeClock(frame*.05,.05);live.PumpWorldAwareGameMode(.05,{id("gobref"),id("novaref")},[&](auto plan){return live.CommitScriptTransaction(*plan);});}
    Check(Local(live,"gobref.tick")==2,"normal frame pump starts original Gob speech");
    bool queued=false;fo3script::Action request;while(live.PollScriptAction(request))queued|=request.kind==fo3script::ActionKind::SayTo&&request.reference==id("gobref")&&request.info!=0;
    Check(queued,"normal captured frame pump queues Gob's original INFO");
  }
  {Player live(p.Definitions());live.RegisterOriginalActors();live.SetScriptPlayerLocation(gateVolume.x,gateVolume.y,gateVolume.z,gateVolume.cell,gateVolume.world);
    std::unique_ptr<ScriptTransactionPlan> entered;
    // Deliver the original gate volume rather than manufacturing its script variable.
    for(const auto& volume:d.triggerPrimitives){const auto placed=p.Definitions().references.find(volume.first);if(placed==p.Definitions().references.end())continue;const auto script=p.Definitions().eventBaseScripts.find(placed->second.base);if(script!=p.Definitions().eventBaseScripts.end()&&script->second==id("megatongatetriggerscript")){entered=live.PrepareReferenceEventTransaction(volume.first,"OnTriggerEnter",PlayerRef,error,ScriptProgramCapture::EnteredPrograms);break;}}
    Check(bool(entered)&&live.CommitScriptTransaction(*entered),"original gate proximity event captured");
    for(int frame=0;frame<10;++frame){live.SetScriptRuntimeClock(frame*.05,.05);live.PumpWorldAwareGameMode(.05,{weld},[&](auto plan){return live.CommitScriptTransaction(*plan);});}
    bool queued=false;fo3script::Action request;while(live.PollScriptAction(request))queued|=request.kind==fo3script::ActionKind::SayTo&&request.reference==weld&&request.info!=0;
    Check(queued,"normal captured frame pump queues Weld's original greeting");
  }
  p.RegisterOriginalActors();
  const auto gob=id("gobref"),nova=id("novaref"),radio=id("moriartysradioref");
  const auto& gobPlaced=d.targets.at(gob);p.SetScriptPlayerLocation(gobPlaced.x,gobPlaced.y+40,gobPlaced.z,gobPlaced.cell,gobPlaced.world);
  Check(!p.DoorInteractionCrime(saloon),"original unlocked saloon entrance is not theft");
  Check(d.targets.at(radio).openByDefault,"original radio is on/open by default");
  for(auto ref:{gob,nova}){auto event=p.PrepareReferenceEventTransaction(ref,"GameMode",PlayerRef,error);Check(bool(event),"saloon original GameMode: "+error);Check(p.CommitScriptTransaction(*event),"saloon source commit");}
  Check(Local(p,"gobref.radioconversation")==1&&Local(p,"gobref.tick")==1,"Gob's original proximity trigger and first radio phase");
  Check(p.PollScriptAction(action)&&action.kind==fo3script::ActionKind::Look&&action.reference==gob&&action.form==radio,"Gob looks at original radio");
  Check(p.PollScriptAction(action)&&action.kind==fo3script::ActionKind::Look&&action.reference==nova&&action.form==gob,"Nova looks at Gob");
  unsigned radioGestures=0;
  auto finishSpeech=[&](const fo3script::Action& request){
    const auto& speaker=d.targets.at(request.reference);fo3dialogue::Context context;context.player=&p;context.talking=true;
    const auto sourcePackage=request.reference==gob?id("meggobradiocomplain"):id("megnovaradiostand");
    context.speaker={request.reference,speaker.base,speaker.cell,speaker.world,sourcePackage,speaker.x,speaker.y,speaker.z,false,true};
    if(request.flags==PlayerRef)context.target={PlayerRef,PlayerBase,speaker.cell,speaker.world,0,speaker.x,speaker.y+40,speaker.z,false,false};
    else {const auto& target=d.targets.at(request.flags);context.target={request.flags,target.base,target.cell,target.world,0,target.x,target.y,target.z,false,false};}
    fo3dialogue::Session speech;Check(speech.StartSelectedTopic(request.form,request.info,context,p),"original scripted speech INFO resolves");
    unsigned count=0;while(speech.phase==fo3dialogue::Phase::Speaking){Check(++count<20,"bounded source response sequence");const auto* info=speech.Current(d);if(info&&speech.response<info->responses.size()&&info->responses[speech.response].speakerAnimation==id("looseradiobangon"))++radioGestures;speech.AudioDone(speech.audioToken,true,context,p);}
    Check(!p.Snapshot().pipboy.talkedActors.count(request.reference),"scripted speech is not a player conversation");
  };
  auto gobTick=[&](){auto event=p.PrepareReferenceEventTransaction(gob,"GameMode",PlayerRef,error);Check(bool(event),"Gob original phase: "+error);Check(p.CommitScriptTransaction(*event),"Gob phase commit");};
  gobTick();Check(Local(p,"gobref.tick")==2,"Gob advances to authored speech");
  Check(p.PollScriptAction(action)&&action.kind==fo3script::ActionKind::SayTo&&action.reference==gob&&action.form==id("meggobradiotalk"),"Gob original topic queued");finishSpeech(action);
  auto done=p.PrepareReferenceEventTransaction(gob,"SayToDone",id("meggobradiotalk"),error);Check(bool(done)&&p.CommitScriptTransaction(*done),"source Gob SayToDone");
  gobTick();Check(p.PollScriptAction(action)&&action.kind==fo3script::ActionKind::SayTo&&action.reference==nova&&action.flags==gob,"Nova's authored SayTo targets Gob");finishSpeech(action);
  done=p.PrepareReferenceEventTransaction(nova,"SayToDone",id("megnovaradiotalk"),error);Check(bool(done)&&p.CommitScriptTransaction(*done),"source Nova SayToDone");
  auto finishGobReply=[&](){
    bool voiced=false;
    while(p.PollScriptAction(action)){if(action.kind==fo3script::ActionKind::SayTo&&action.reference==gob){finishSpeech(action);voiced=true;
      auto complete=p.PrepareReferenceEventTransaction(gob,"SayToDone",action.form,error);Check(bool(complete)&&p.CommitScriptTransaction(*complete),"actual Gob reply completion");}}
    Check(voiced,"original radio phase produced a real queued Gob INFO");
  };
  gobTick();gobTick();Check(Local(p,"gobref.tick")==5,"radio exchange resumes after actual completion event");finishGobReply();
  for(int phase=5;phase<=8;++phase){gobTick();finishGobReply();}
  gobTick();while(p.PollScriptAction(action)){}
  const auto moriarty=id("colinmoriartyref"),package=id("megmoriartyfindgob");
  Check(Local(p,"gobref.tick")==10&&Local(p,"gobref.radioconversation")==-1,"original radio exchange reaches its final phase");
  Check(p.Snapshot().scriptPackages.at(moriarty).front()==package,"original Gob script installs Moriarty dialogue package");
  const auto save="/tmp/fq293-saloon-"+std::to_string(getpid());Check(p.Save(save,error),error);Player reloaded(p.Definitions());Check(reloaded.Restore(save,error),error);std::remove(save.c_str());
  Check(reloaded.Snapshot().scriptPackages.at(moriarty).front()==package&&Local(reloaded,"gobref.tick")==10,"saloon script progress and package persist");
  const auto& colin=d.targets.at(moriarty);fo3dialogue::Context conversation;conversation.player=&reloaded;conversation.talking=true;
  conversation.speaker={moriarty,colin.base,gobPlaced.cell,gobPlaced.world,package,colin.x,colin.y,colin.z,false,true};
  conversation.target={gob,gobPlaced.base,gobPlaced.cell,gobPlaced.world,0,gobPlaced.x,gobPlaced.y,gobPlaced.z,false,true};
  fo3dialogue::Session hello;Check(hello.StartTopic(id("hello"),conversation,reloaded),"Moriarty's original NPC HELLO resolves");
  unsigned replies=0;while(hello.phase==fo3dialogue::Phase::Speaking){Check(++replies<20,"bounded original NPC responses");hello.AudioDone(hello.audioToken,true,conversation,reloaded);}
  Check(!reloaded.Snapshot().scriptPackages.count(moriarty),"original HELLO result removes the temporary Moriarty package");
  Check(!reloaded.Snapshot().pipboy.talkedActors.count(moriarty),"NPC conversation does not count as speaking to the player");
  Check(!reloaded.Snapshot().dailySaidInfos.empty(),"daily radio INFO history survives reload");
  Check(radioGestures>0,"original radio response supplies the bang-on IDLE");
  reloaded.SetScriptPlayerLocation(gobPlaced.x,gobPlaced.y+40,gobPlaced.z,gobPlaced.cell,gobPlaced.world);
  const auto repeat=reloaded.PrepareReferenceEventTransaction(gob,"GameMode",PlayerRef,error);Check(bool(repeat)&&reloaded.CommitScriptTransaction(*repeat),"return to saloon after completed radio scene");
  Check(!reloaded.PollScriptAction(action)&&!reloaded.Snapshot().scriptPackages.count(moriarty),"one-time scene does not replay after return/reload");
  std::cout<<"Original Gob/Nova radio scene, authored response IDLEs, Moriarty HELLO/results, return/reload and package persistence passed\n";

}
int main(int argc,char** argv){try{Synthetic();if(argc>1)Original(argv[1]);std::cout<<"World source scripts passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
