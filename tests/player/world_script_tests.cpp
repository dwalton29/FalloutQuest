#include "player/fo3-player-state.h"
#include "dialogue/fo3-dialogue-results.h"
#include "scripting/fo3-nonresident-script.h"
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <cstdio>
using namespace fo3player;
static void Check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
static float Local(const Player& p,const std::string& name){
  const auto key=fo3dialogue::VariableKey(p.Definitions().pipboy,name);
  const auto i=p.Snapshot().pipboy.dialogueVariables.find(key);return i==p.Snapshot().pipboy.dialogueVariables.end()?0:i->second;
}
static void Synthetic(){
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
  // Image curves are sampled over the authored duration, not reduced to last keys.
  fo3script::ImageModifier m;m.valid=true;m.duration=10;m.fade.keys={{0,{1,1,1,.8f}},{1,{1,1,1,0}}};m.blur.keys={{0,{2,0,0,0}},{1,{0,0,0,0}}};
  auto image=fo3script::SampleImageModifier(m,5);Check(std::abs(image.fade[3]-.4f)<.001&&image.blur==1,"temporal modifier midpoint");
  Check(fo3script::SampleImageModifier(m,10).fade[3]==0,"modifier expires");
}
static void Original(const char* path){
  Catalog c;std::string error;Check(LoadCatalog(path,c,error),error);c.initial.gameHour=12;Player p(std::move(c));
  const auto& d=p.Definitions().pipboy;auto id=[&](const char* name){auto f=d.formNames.find(name);Check(f!=d.formNames.end(),std::string("missing ")+name);return f->second;};
  const uint32_t house=id("megatongatehouseref"),weld=id("megatongateprotectronref");
  auto registry=std::make_shared<fo3script::AnimationRegistry>();(*registry)[house]={{"Forward",15,false},{"Backward",15,false},{"Left",3,true},{"SpecialIdle",3,true}};
  p.SetScriptAnimationRegistry(registry);p.SetScriptRuntimeClock(100,.25);
  uint32_t trigger=0;for(const auto& t:d.triggerPrimitives){const auto ref=p.Definitions().references.find(t.first);if(ref!=p.Definitions().references.end()){
    const auto script=p.Definitions().eventBaseScripts.find(ref->second.base);
    if(script!=p.Definitions().eventBaseScripts.end()&&script->second==id("megatongatetriggerscript"))trigger=t.first;
  }}Check(trigger!=0,"source gate trigger");
  const auto& gateVolume=d.targets.at(trigger);p.SetScriptPlayerLocation(gateVolume.x,gateVolume.y,gateVolume.z,gateVolume.cell,gateVolume.world);
  auto changes=p.ScriptTriggerChanges();Check(std::find(changes.begin(),changes.end(),std::make_pair(trigger,true))!=changes.end(),"original gate volume retained and entered");
  auto enter=p.PrepareReferenceEventTransaction(trigger,"OnTriggerEnter",PlayerRef,error);Check(bool(enter),"gate enter: "+error);Check(p.CommitScriptTransaction(*enter),"gate trigger commit");
  Check(Local(p,"megatongatehouseref.opennow")==1&&Local(p,"megatongateprotectronref.playernear")==1,"original cross-reference locals");
  auto run=[&](const char* event){auto plan=p.PrepareReferenceEventTransaction(house,event,0,error);Check(bool(plan),std::string(event)+": "+error);Check(p.CommitScriptTransaction(*plan),"gate commit");};
  run("OnLoad");run("GameMode");fo3script::Action action;Check(p.PollScriptAction(action)&&action.group=="forward","original Forward");Check(!p.PollScriptAction(action),"idle cannot interrupt opening");
  p.SetScriptRuntimeClock(114.99,.25);run("GameMode");Check(!p.PollScriptAction(action),"source waits for duration");
  p.SetScriptRuntimeClock(115,.25);run("GameMode");Check(p.PollScriptAction(action)&&action.group=="left"&&Local(p,"megatongatehouseref.opened")==2,"source Left at completion");
  run("OnLoad");run("GameMode");Check(p.PollScriptAction(action)&&action.group=="left","source reload idle");
  (void)weld;
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
}
int main(int argc,char** argv){try{Synthetic();if(argc>1)Original(argv[1]);std::cout<<"World source scripts passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
