#include "player/fo3-new-game.h"
#include "scripting/fo3-script-master-match.h"
#include <cassert>
#include <iostream>
#include <unistd.h>
static void Check(bool ok,const std::string& error){if(!ok)std::cerr<<error<<'\n';assert(ok);}
static void Synthetic(){
  fo3player::Catalog c;c.initial.special.fill(1);c.initial.baseHealth=100;
  c.pipboy.formNames={{"intro",101},{"arrival",303}};
  c.pipboy.quests[101].script=202;
  c.pipboy.scripts[202].source="Begin MenuMode\nSetStage Intro 4\nEnd";
  c.eventPrograms[202]=fo3script::ParseEvents(c.pipboy.scripts[202]);
  auto& result=c.pipboy.quests[101].stages[4].items.emplace_back().result;
  result.source="SetInCharGen 1\nSetLocationSpecificLoadScreensOnly 1\nplayer.MoveTo Arrival 1 2 3";
  c.pipboy.targets[303].cell=404;c.pipboy.cellWorlds[404]=0;
  c.pipboy.targets[303].x=10;c.pipboy.targets[303].y=20;c.pipboy.targets[303].z=30;c.pipboy.targets[303].rz=1;
  fo3player::Player p(c);std::string error;
  auto plan=p.PrepareNewGameTransaction(101,error);Check(bool(plan),error);
  assert(!p.Snapshot().playerMove.pending&&!p.Snapshot().inCharGen);
  Check(p.CommitScriptTransaction(*plan),error);assert((p.Snapshot().playerMove.position==std::array<float,3>{11,22,33}));
  assert(p.Snapshot().playerMove.rotation[2]==1&&p.Snapshot().playerMove.pending);
  Check(p.RollbackScriptTransaction(*plan),error);assert(!p.Snapshot().playerMove.marker&&!p.Snapshot().inCharGen);
  assert(!p.MoveActorTo(fo3player::PlayerRef,999,{}));assert(!p.Snapshot().playerMove.pending);
  // A later unsupported setup command must roll back the pending teleport too.
  c.pipboy.quests[101].stages[4].items[0].result.source+="\nUnknownSetupCommand";
  fo3player::Player unsupported(c);assert(!unsupported.PrepareNewGameTransaction(101,error));
  assert(unsupported.Snapshot().pipboy.quests.empty()&&!unsupported.Snapshot().playerMove.marker);
}
int main(int argc,char** argv){
  std::string error;uint32_t quest=0;
  std::istringstream config("[Other]\nSCharGenQuest=bad\n[General]\nSCharGenQuest=0001f388 ; original\n");
  Check(fo3newgame::ReadQuest(config,quest,error),error);assert(quest==0x1f388);
  for(const char* invalid:{"[General]\nSCharGenQuest=0", "[General]\nSCharGenQuest=1z", "[General]\nSCharGenQuest=1\nSCharGenQuest=2", "[Other]\nSCharGenQuest=1"}){
    std::istringstream input(invalid);assert(!fo3newgame::ReadQuest(input,quest,error));
  }
  Synthetic();
  if(argc<2)return 0;
  fo3player::Catalog catalog;Check(fo3player::LoadCatalog(argv[1],catalog,error),error);
  fo3player::Player player(catalog);player.RegisterOriginalActors();
  auto plan=player.PrepareNewGameTransaction(catalog.pipboy.formNames.at("cg00"),error);Check(bool(plan),error);
  fo3script::ScriptMasterMatch match;std::atomic<bool> cancelled{false};
  Check(fo3script::MatchScriptProgramsToMaster(argv[1],plan->ExecutedScripts(),match,cancelled,error),error);
  assert(!player.Snapshot().playerMove.pending);Check(player.CommitScriptTransaction(*plan),error);
  const auto& state=player.Snapshot();const auto& destination=catalog.pipboy.targets.at(catalog.pipboy.formNames.at("cg00playerstartmarker"));
  assert(state.inCharGen&&state.locationSpecificLoadScreens&&state.playerMove.pending);
  assert(state.playerMove.cell==destination.cell&&state.playerMove.position[0]==destination.x);
  assert(state.pipboy.quests.at(catalog.pipboy.formNames.at("cg00")).stage==5);
  assert(!player.PrepareNewGameTransaction(catalog.pipboy.formNames.at("cg00"),error));
  const std::string save="/tmp/fq-new-game-"+std::to_string(getpid());Check(player.Save(save,error),error);
  fo3player::Player resumed(catalog);Check(resumed.Restore(save,error),error);unlink(save.c_str());
  assert(resumed.Snapshot().inCharGen&&resumed.Snapshot().playerMove.pending);
  assert(resumed.Snapshot().playerMove.marker==state.playerMove.marker);
  resumed.AcknowledgePlayerMove();assert(!resumed.Snapshot().playerMove.pending);
  const auto q=catalog.pipboy.formNames.at("cg00"),dad=catalog.pipboy.formNames.at("cg00dadref");
  resumed.SetScriptPlayerLocation(destination.x,destination.y,destination.z,destination.cell,destination.world);

  for(unsigned n=0;n<240;++n){resumed.SetScriptRuntimeClock(n*.1,.1);resumed.PumpWorldAwareGameMode(.1,{dad},[&](auto transaction){return resumed.CommitScriptTransaction(*transaction);});}
  assert(resumed.Snapshot().pipboy.quests.at(q).stage==10);
  bool firstSpeech=false,screenBlood=false;fo3script::Action action;
  while(resumed.PollScriptAction(action)){
    firstSpeech|=action.kind==fo3script::ActionKind::SayTo&&action.reference==dad&&action.info!=0&&action.forceSubtitle;
    screenBlood|=action.kind==fo3script::ActionKind::ScreenBlood&&action.flags==10;
  }
  assert(firstSpeech&&screenBlood&&resumed.Snapshot().noActivationSound);
  const auto& base=catalog.imageModifiers.at(catalog.pipboy.formNames.at("cg00birthbaseisfx"));assert(base.valid&&!(base.flags&1));
  fo3script::ImageStack images;images.Apply(1,base,0);images.Apply(2,catalog.imageModifiers.at(catalog.pipboy.formNames.at("cg00blackscreenisfx")),0);
  assert(images.Sample(0).fade[3]>.9f);images.Sample(20);assert(images.entries.size()==1);images.Remove(1);assert(images.entries.empty());
  std::cout<<"Original configured quest bootstrap -> birth stages 0/5/6/8/9/10 -> first voiced INFO; authored arrival, effect lifetimes and save round trip passed\n";
}
