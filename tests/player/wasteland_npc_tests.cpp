#include "player/fo3-player-state.h"
#include "dialogue/fo3-dialogue-session.h"
#include "npc/fo3-npc-residency.h"
#include "scripting/fo3-nonresident-script.h"
#include <cassert>
#include <iostream>
#include <unistd.h>
using namespace fo3player;
int main(int argc,char** argv){
  assert(fo3npc::Resident(1,0x3c,false,0,0,2,0x3c,{4096,0,0}));
  assert(!fo3npc::Resident(1,0x3c,false,0,0,2,0x3c,{8192,0,0}));
  assert(!fo3npc::Resident(1,0x3c,false,0,0,2,0,{0,0,0}));
  assert(!fo3npc::Resident(1,0,false,0,0,2,0,{0,0,0}));
  if(argc<2)return 0;
  Catalog c;std::string error;assert(LoadCatalog(argv[1],c,error));
  Player p(c);const auto census=p.RegisterOriginalActors();assert(census.registered>1000);
  for(uint32_t ref:{0x1fb3a,0xc6e91,0x41611,0x41610,0x41614}){
    assert(p.Snapshot().actors.count(ref));assert(p.RealizeActor(ref));assert(p.ActorHealth(ref)>0);
  }
  assert(p.PrepareActorInventory(0x41611));
  assert(p.ActorCategory(0x41611,8)&&p.ActorCategory(0x41611,4));
  bool disabled=false;assert(p.ReferenceDisabled(0x41614,disabled,error)&&disabled);
  assert(p.DispatchReferenceEvent(0xc6e91,"OnLoad",0,error)==ScriptEventResult::Executed);
  // Exercise the source GetAV / SetAV paths and the original water-beggar OnLoad death.
  fo3pipdata::ResultScript set;set.source="WaterBeggar01Ref.SetAV Variable10 2";
  assert(p.ExecuteDialogueResult(set,error));float value=0;assert(p.ActorVariable(0xc6e91,9,value)&&value==2);
  const auto save="/tmp/fq-wasteland-"+std::to_string(getpid());assert(p.Save(save,error));
  Player restored(c);if(!restored.Restore(save,error)){std::cerr<<error<<'\n';assert(false);}unlink(save.c_str());
  assert(restored.ActorVariable(0xc6e91,9,value)&&value==2);
  assert(restored.Snapshot().actorTemplateChoices==p.Snapshot().actorTemplateChoices);
  assert(restored.ActorHealth(0x41611)==p.ActorHealth(0x41611));
  assert(restored.DispatchReferenceEvent(0xc6e91,"OnLoad",0,error)==ScriptEventResult::Executed);
  assert(restored.ActorHealth(0xc6e91)==0);
  // Source Weld GameMode emits the original topic action; no location-specific runtime code.
  fo3pipdata::ResultScript near;near.source="set MegatonGateProtectronREF.playernear to 1";
  assert(p.ExecuteDialogueResult(near,error));
  if(p.DispatchReferenceEvent(0x1fb3a,"GameMode",0,error)!=ScriptEventResult::Executed){std::cerr<<error<<'\n';assert(false);}
  fo3script::Action action;assert(p.PollScriptAction(action));
  assert(action.kind==fo3script::ActionKind::SayTo&&action.reference==0x1fb3a&&action.flags==PlayerRef&&action.form==0x1fb2b);
  fo3dialogue::Context context;context.player=&p;context.talking=true;
  const auto& pose=c.actorPlacements.at(0x1fb3a);
  context.speaker={0x1fb3a,0x1fb39,pose.cell,pose.world,0,pose.x,pose.y,pose.z};
  context.target={PlayerRef,7,pose.cell,pose.world,0,pose.x+100,pose.y,pose.z};
  fo3dialogue::Session speech;
  assert(speech.StartTopic(0x1fb2b,context,p));
  while(speech.phase==fo3dialogue::Phase::Speaking)speech.AudioDone(speech.audioToken,true,context,p);
  assert(p.DispatchReferenceEvent(0x1fb3a,"SayToDone",0x1fb2b,error)==ScriptEventResult::Executed);
  for(int step=0;step<3;++step){
    fo3dialogue::Session next;
    if(next.StartTopic(0x1fb2b,context,p)){
      while(next.phase==fo3dialogue::Phase::Speaking)next.AudioDone(next.audioToken,true,context,p);
      assert(p.DispatchReferenceEvent(0x1fb3a,"SayToDone",0x1fb2b,error)==ScriptEventResult::Executed);
    }
  }
  const auto firstSpeech=fo3dialogue::VariableKey(c.pipboy,"MegatonGateProtectronREF.firstSpeechDone");
  assert(firstSpeech&&p.Snapshot().pipboy.dialogueVariables.count(firstSpeech)&&p.Snapshot().pipboy.dialogueVariables.at(firstSpeech)==1);
  // Original raider parent closure publishes a spawn intent; the arrival worker owns resources.
  fo3pipdata::ResultScript enable;enable.source="";
  const auto name=c.pipboy.formNames.find("superdupermartintercomref");assert(name!=c.pipboy.formNames.end());
  uint32_t trigger=0;
  for(const auto& r:c.references){const auto sc=c.eventBaseScripts.find(r.second.base);
    if(sc!=c.eventBaseScripts.end()&&sc->second==0x9dc6c){trigger=r.first;break;}}
  assert(trigger);
  auto plan=p.PrepareReferenceEventTransaction(trigger,"OnTriggerLeave",PlayerRef,error);
  if(!plan){std::cerr<<error<<'\n';assert(false);}
  assert(plan->HasReferenceChanges());
  assert(fo3script::NonresidentActorRemoval(p,*plan,0x17f37,0,{},error));
  assert(p.CommitScriptTransaction(*plan));assert(p.ReferenceDisabled(0x41614,disabled,error)&&!disabled);
  std::cout<<"Original Weld, Micky and Super-Duper Mart source-script tests passed\n";
}
