#include "npc/fo3-npc-combat.h"
#include "dialogue/fo3-dialogue-conditions.h"
#include "ui/pause/fo3-debug-menu.h"
#include "scripting/fo3-script-master-match.h"
#include <cassert>
#include <iostream>
#include <unistd.h>

static void Check(bool ok,const std::string& error){if(!ok)std::cerr<<error<<std::endl;assert(ok);}
static void Original(const char* path){
  fo3player::Catalog c;std::string error;Check(fo3player::LoadCatalog(path,c,error),error);
  fo3player::Player player(c);player.RegisterOriginalActors();
  fo3debug::Menu entry;entry.Open(player);entry.BuildQuests();entry.form=c.pipboy.formNames.at("mq01");entry.Go(fo3debug::Menu::Page::Quest);assert(entry.Confirm());entry.Close();
  const auto mq=c.pipboy.formNames.at("mq01"),ld=c.pipboy.formNames.at("mq01ld");
  uint32_t trigger=0;const auto script=c.pipboy.formNames.at("mq01cityambushscript");
  for(const auto& ref:c.references){const auto owner=c.eventBaseScripts.find(ref.second.base);if(owner!=c.eventBaseScripts.end()&&owner->second==script){assert(!trigger);trigger=ref.first;}}
  assert(trigger);const auto& placed=c.pipboy.targets.at(trigger);
  player.SetScriptPlayerLocation(placed.x,placed.y,placed.z,placed.cell,placed.world);
  const auto changes=player.ScriptTriggerChanges();assert(std::find(changes.begin(),changes.end(),std::make_pair(trigger,true))!=changes.end());
  Check(player.DispatchReferenceEvent(trigger,"OnTriggerEnter",fo3player::PlayerRef,error)==fo3player::ScriptEventResult::Executed,error);
  player.AcknowledgeScriptTrigger(trigger,true);assert(player.Snapshot().pipboy.quests.at(ld).stage==45);
  const auto faction=c.pipboy.formNames.at("mq01brotherhooddcfaction");
  const auto sarah=c.pipboy.formNames.at("mq01sarahref"),mutant=c.pipboy.formNames.at("mq01citysupermutant01ref");
  assert(player.ActorFactionRank(sarah,faction)==-1);
  for(int i=0;i<8;++i)player.PumpReferenceGameMode(.25,{trigger});
  auto save="/tmp/fq-mq01-scene-"+std::to_string(getpid());Check(player.Save(save,error),error);
  fo3player::Player resumed(c);Check(resumed.Restore(save,error),error);resumed.SetScriptPlayerLocation(placed.x,placed.y,placed.z,placed.cell,placed.world);
  assert(resumed.Snapshot().pipboy.quests.at(ld).stage==45);
  for(int i=0;i<16;++i)resumed.PumpReferenceGameMode(.25,{trigger});
  assert(resumed.Snapshot().pipboy.quests.at(ld).stage==50&&resumed.Snapshot().pipboy.quests.at(mq).stage==50);
  for(const auto* name:{"mq01sarahref","mq01vargasref","mq01reddinref","mq01colvinref"}){
    const auto actor=c.pipboy.formNames.at(name);float aggression=-1;assert(resumed.ActorFactionRank(actor,faction)==0&&resumed.ActorAIValue(actor,0,aggression)&&aggression==1);
  }
  assert(fo3npc::Relationship(resumed,sarah,mutant)==fo3npc::Reaction::Enemy);
  Check(resumed.Save(save,error),error);fo3player::Player returned(c);Check(returned.Restore(save,error),error);
  assert(returned.ActorFactionRank(sarah,faction)==0&&fo3npc::Relationship(returned,sarah,mutant)==fo3npc::Reaction::Enemy);
  const auto before=returned.Snapshot().actorFactions;
  Check(returned.DispatchReferenceEvent(trigger,"OnTriggerEnter",fo3player::PlayerRef,error)==fo3player::ScriptEventResult::Executed,error);
  for(int i=0;i<24;++i)returned.PumpReferenceGameMode(.25,{trigger});
  assert(returned.Snapshot().actorFactions==before&&returned.Snapshot().pipboy.quests.at(ld).stage==50);
  // Original Behemoth weapon filter, through canonical attack/damage dispatch.
  // This isolates receipt of a hit; it does not simulate its scripted entrance,
  // explosion projectiles, bus destruction, or the full GNR battle.
  const auto behemoth=c.pipboy.formNames.at("mq01behemothref"),fatman=c.pipboy.formNames.at("weapfatman");
  const auto counter=fo3dialogue::VariableKey(c.pipboy,"MQ01BehemothRef.nukeCount");assert(counter);
  {
    auto captured=returned.PrepareReferenceEventTransaction(behemoth,"OnHitWith",fo3player::PlayerRef,error,fo3player::ScriptProgramCapture::EnteredPrograms,fatman);Check(bool(captured),error);
    fo3script::ScriptMasterMatch match;std::atomic<bool> cancelled{false};
    Check(fo3script::MatchScriptProgramsToMaster(path,captured->ExecutedScripts(),match,cancelled,error),error);
    assert(match.Matches(captured->ExecutedScripts()));
    auto wrong=captured->ExecutedScripts();wrong.front().origin.filterForm=c.pipboy.formNames.at("weap10mmpistol");
    assert(!match.Matches(wrong)&&!fo3script::MatchScriptProgramsToMaster(path,wrong,match,cancelled,error));
    assert(match.Matches(captured->ExecutedScripts()));
    auto arrived=returned.PreparePackageEventTransaction(c.pipboy.formNames.at("mq01colvinref"),c.pipboy.formNames.at("mq01colvinadvancefromcheckpoint"),true,error,fo3player::ScriptProgramCapture::EnteredPrograms);
    Check(bool(arrived),error);Check(fo3script::MatchScriptProgramsToMaster(path,arrived->ExecutedScripts(),match,cancelled,error),error);
    assert(returned.Snapshot().pipboy.quests.at(mq).stage==50); // preview did not publish a scene advance

  }

  assert(returned.ApplyAttack(fo3player::PlayerRef,c.pipboy.formNames.at("weap10mmpistol"),behemoth,1));
  assert(!returned.Snapshot().pipboy.dialogueVariables.count(counter));
  assert(returned.ApplyAttack(fo3player::PlayerRef,fatman,behemoth,1));assert(returned.Snapshot().pipboy.dialogueVariables.at(counter)==1);
  Check(returned.Save(save,error),error);fo3player::Player hitSaved(c);Check(hitSaved.Restore(save,error),error);unlink(save.c_str());
  assert(hitSaved.Snapshot().pipboy.dialogueVariables.at(counter)==1);
  std::cout<<"Original MQ01 ambush trigger -> five-second timer with mid-timer reload -> authored faction/aggression/objective changes; replay protection and Behemoth weapon filter passed. World traversal/battle not simulated.\n";
}
int main(int argc,char** argv){if(argc<2)return 77;Original(argv[1]);}
