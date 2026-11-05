#include "player/fo3-new-game.h"
#include "scripting/fo3-script-master-match.h"
#include "ui/chargen/fo3-name-entry.h"
#include "dialogue/fo3-dialogue-conditions.h"
#include "dialogue/fo3-dialogue-session.h"
#include "dialogue/fo3-dialogue-selection.h"
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
static void Identity(){
  fo3player::Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);
  c.pipboy.dialogueActors[fo3player::PlayerBase].name="Original";
  c.pipboy.dialogueActors[fo3player::PlayerBase].race=99;
  c.pipboy.raceBodyModels[99][0]={"male/body","male/left","male/right"};
  c.pipboy.raceBodyModels[99][1]={"female/body","female/left","female/right"};
  c.pipboy.quests[101].script=202;c.pipboy.quests[101].scriptDelay=.01f;
  c.pipboy.formNames["intro"]=101;
  auto& script=c.pipboy.scripts[202];script.variables={{1,"timer"},{2,"menus"}};
  script.source="float timer\nshort menus\nBegin MenuMode\nSet menus to menus + 1\nEnd\nBegin GameMode\nSet timer to timer + GetSecondsPassed\nEnd";
  c.eventPrograms[202]=fo3script::ParseEvents(script);
  fo3player::Player p(c);assert(p.PlayerName()=="Original"&&!p.PlayerFemale());
  fo3pipdata::ResultScript change;change.source="player.SexChange female 1";std::string error;
  Check(p.ExecuteDialogueResult(change,error),error);assert(p.PlayerFemale()&&p.PlayerBodyModel(2)=="female/right");
  assert(!p.Definitions().pipboy.playerFemale);
  fo3dialogue::Context context;context.player=&p;context.speaker.reference=fo3player::PlayerRef;context.speaker.base=fo3player::PlayerBase;
  fo3pipdata::Condition sex;sex.function=70;sex.a=1;sex.value=1;
  Check(fo3dialogue::Conditions({sex},context,error),error);sex.a=0;assert(!fo3dialogue::Conditions({sex},context,error));
  sex.function=131;sex.a=1;Check(fo3dialogue::Conditions({sex},context,error),error);
  assert(p.StartQuest(101));
  auto reserve=[&](auto plan){return p.CommitScriptTransaction(*plan);};
  for(unsigned n=0;n<20;++n){p.SetScriptRuntimeClock(0,0);p.PumpWorldAwareGameMode(.25,{},reserve,true);}
  const uint64_t timer=(uint64_t(101)<<32)|1,menus=(uint64_t(101)<<32)|2;
  assert(!p.Snapshot().pipboy.dialogueVariables.count(timer)&&p.Snapshot().pipboy.dialogueVariables.at(menus)==20);
  p.PumpWorldAwareGameMode(.1,{},reserve);assert(std::abs(p.Snapshot().pipboy.dialogueVariables.at(timer)-.1f)<.001f);
  change.source="GetPlayerName";Check(p.ExecuteDialogueResult(change,error,101),error);
  assert(p.Snapshot().nameMenuOwner==101);assert(!p.EditPlayerName("bad\nname")&&!p.EditPlayerName(std::string(33,'x')));
  assert(p.EditPlayerName("Raymond"));
  const auto save="/tmp/fq-identity-"+std::to_string(getpid());Check(p.Save(save,error),error);
  fo3player::Player restored(c);Check(restored.Restore(save,error),error);unlink(save.c_str());
  assert(restored.PlayerFemale()&&restored.Snapshot().nameMenuOwner==101&&restored.Snapshot().nameDraft=="Raymond");
  assert(restored.ConfirmPlayerName()&&restored.PlayerName()=="Raymond"&&!restored.Snapshot().nameMenuOwner);
  assert(restored.OpenPlayerNameMenu(101)&&restored.EditPlayerName(""));
  fo3name::Model keyboard;
  keyboard.Step(restored,0,0,0,true,false,true);assert(restored.Snapshot().nameDraft.empty());
  keyboard.Step(restored,.1,0,0,false,false,true);keyboard.Step(restored,.2,0,0,false,false,true);
  keyboard.Step(restored,.3,0,0,true,false,true);assert(restored.Snapshot().nameDraft=="A");
  keyboard.Step(restored,.4,0,0,false,false,false);keyboard.Step(restored,.5,0,0,true,false,true);assert(restored.Snapshot().nameDraft=="A");
  keyboard.Step(restored,.6,0,0,false,false,true);keyboard.Step(restored,.7,0,0,false,false,true);
  keyboard.cursor.selected=2;assert(keyboard.Step(restored,.8,0,0,true,false,true)==fo3name::Model::Result::Confirmed);
  assert(restored.PlayerName()=="A");
}
static void OriginalIdentity(const fo3player::Catalog& catalog,bool female){
  fo3player::Player p(catalog);p.RegisterOriginalActors();std::string error;
  const auto& d=catalog.pipboy;const auto quest=d.formNames.at("cg00");
  auto boot=p.PrepareNewGameTransaction(quest,error);Check(bool(boot)&&p.CommitScriptTransaction(*boot),error);
  const auto start=p.Snapshot().playerMove;p.AcknowledgePlayerMove();
  p.SetScriptPlayerLocation(start.position[0],start.position[1],start.position[2],start.cell,start.world);
  const std::vector<uint32_t> residents={d.formNames.at("cg00dadref"),d.formNames.at("cg00momref"),d.formNames.at("cg00doctorliref")};
  bool chose=false,genderLine=false;
  for(unsigned frame=0;frame<900&&!p.Snapshot().nameMenuOwner;++frame){
    p.SetScriptRuntimeClock(frame*.1,.1);p.PumpWorldAwareGameMode(.1,residents,[&](auto plan){return p.CommitScriptTransaction(*plan);});
    fo3player::ScriptMessage message;
    while(p.PollScriptMessage(message))if(message.owner==quest){
      assert(!chose&&message.buttons.size()==2);
      const auto before=p.Snapshot().pipboy.dialogueVariables;
      for(int tick=0;tick<20;++tick)p.PumpWorldAwareGameMode(.25,residents,[&](auto plan){return p.CommitScriptTransaction(*plan);},true);
      assert(p.Snapshot().pipboy.dialogueVariables==before);
      assert(p.SubmitScriptMessageButton(message.owner,female?1:0));chose=true;
    }
    fo3script::Action action;
    while(p.PollScriptAction(action))if(action.kind==fo3script::ActionKind::SayTo){
      if(std::find(residents.begin(),residents.end(),action.reference)==residents.end())continue;
      fo3dialogue::Context ctx;ctx.player=&p;ctx.talking=true;
      Check(fo3dialogue::ReferenceContext(p,action.reference,ctx.speaker),"birth speaker context");
      Check(fo3dialogue::ReferenceContext(p,action.flags,ctx.target),"birth listener context");
      fo3dialogue::Session voice;Check(voice.StartSelectedTopic(action.form,action.info,ctx,p),"original birth INFO selected");
      const auto* info=voice.Current(d);assert(info);
      for(const auto& condition:info->conditions)if(condition.function==70||condition.function==131){
        assert(chose&&condition.a==uint32_t(female));genderLine=true;
      }
      p.SetScriptActorTalking(action.reference,true);
      unsigned responses=0;while(voice.phase==fo3dialogue::Phase::Speaking){assert(++responses<32);voice.AudioDone(voice.audioToken,true,ctx,p);}
      p.SetScriptActorTalking(action.reference,false);
      const auto done=p.DispatchReferenceEvent(action.reference,"SayToDone",action.form,error);
      Check(done==fo3player::ScriptEventResult::Executed||done==fo3player::ScriptEventResult::NoHandler,error);
    }
  }
  assert(chose&&genderLine&&p.PlayerFemale()==female&&p.Snapshot().nameMenuOwner==quest);
  const auto before=p.Snapshot().pipboy.quests.at(quest).stage;
  for(int tick=0;tick<20;++tick)p.PumpWorldAwareGameMode(.25,residents,[&](auto plan){return p.CommitScriptTransaction(*plan);},true);
  assert(p.Snapshot().pipboy.quests.at(quest).stage==before);
  assert(p.EditPlayerName(female?"Catherine":"James")&&p.ConfirmPlayerName());
  for(unsigned frame=0;frame<20;++frame)p.PumpWorldAwareGameMode(.1,residents,[&](auto plan){return p.CommitScriptTransaction(*plan);});
  assert(p.Snapshot().pipboy.quests.at(quest).stage==42);
  const auto save="/tmp/fq-original-identity-"+std::to_string(getpid());Check(p.Save(save,error),error);
  fo3player::Player restored(catalog);Check(restored.Restore(save,error),error);unlink(save.c_str());
  assert(restored.PlayerFemale()==female&&restored.PlayerName()==(female?"Catherine":"James"));
}
int main(int argc,char** argv){
  std::string error;uint32_t quest=0;
  std::istringstream config("[Other]\nSCharGenQuest=bad\n[General]\nSCharGenQuest=0001f388 ; original\n");
  Check(fo3newgame::ReadQuest(config,quest,error),error);assert(quest==0x1f388);
  for(const char* invalid:{"[General]\nSCharGenQuest=0", "[General]\nSCharGenQuest=1z", "[General]\nSCharGenQuest=1\nSCharGenQuest=2", "[Other]\nSCharGenQuest=1"}){
    std::istringstream input(invalid);assert(!fo3newgame::ReadQuest(input,quest,error));
  }
  Synthetic();Identity();
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
  OriginalIdentity(catalog,false);OriginalIdentity(catalog,true);
  std::cout<<"Original birth bootstrap, both sex-dependent dialogue branches, modal name entry, timer isolation and identity/save round trips passed\n";
}
