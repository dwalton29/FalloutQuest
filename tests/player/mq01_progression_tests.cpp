#include "mq01_test_conversation.h"
#include "ui/pause/fo3-debug-menu.h"
#include <cassert>
#include <iostream>
#include <unistd.h>

// Original-data checkpoint. The explicit developer entry action executes the
// original first stage. Locations substitute for world traversal, not quest
// gates; subsequent progression is owned by original INFO results.
static void Original(const char* path){
  fo3player::Catalog catalog;std::string error;assert(fo3player::LoadCatalog(path,catalog,error));
  const auto save="/tmp/fq-mq01-progression-"+std::to_string(getpid());
  fo3player::Player first(catalog);first.RegisterOriginalActors();assert(first.Add(0xf,100));
  // A direct-world-spawn save has no Escape handoff. Exercise the actual
  // generic debug menu rather than synthesising a post-Escape quest snapshot.
  fo3debug::Menu entry;entry.Open(first);entry.BuildQuests();
  const auto mq=std::find(entry.quests.begin(),entry.quests.end(),0x14e87u);assert(mq!=entry.quests.end());
  entry.form=*mq;entry.Go(fo3debug::Menu::Page::Quest);const auto entryRevision=first.Revision();
  assert(first.Snapshot().pipboy.quests.count(*mq)==0&&first.Revision()==entryRevision);
  assert(entry.Confirm());const auto& started=first.Snapshot().pipboy.quests.at(*mq);
  assert(started.stage==10&&started.running&&started.stages.count(10)&&started.objectives.at(10).displayed);
  const auto startedRevision=first.Revision();assert(!entry.Confirm()&&first.Revision()==startedRevision);entry.Close();
  const auto xp=first.Snapshot().experience;
  {
    Conversation moriarty(first,catalog.pipboy.formNames.at("colinmoriartyref"));moriarty.Start();
    for(auto topic:{0x3b7cu,0x14f3eu,0x151b6u})moriarty.Choose(topic);
    assert(first.ItemCount(0xf)==0&&first.Snapshot().pipboy.quests.at(0x14e87).stage==40);
    assert(first.Snapshot().pipboy.quests.at(0x14e87).objectives.at(40).displayed);
  }
  assert(first.Save(save,error));fo3player::Player travelled(catalog);assert(travelled.Restore(save,error));
  assert(travelled.Snapshot().pipboy.quests.at(0x14e87).stage==40&&travelled.ItemCount(0xf)==0);
  // Return to Megaton before continuing. Greeting/said-once state survives;
  // simply opening another conversation cannot charge Moriarty's fee again.
  {Conversation returned(travelled,catalog.pipboy.formNames.at("colinmoriartyref"));returned.Start();}
  assert(travelled.ItemCount(0xf)==0&&travelled.Snapshot().experience==xp);
  const auto sarah=catalog.pipboy.formNames.at("mq01sarahref");
  const auto destination=catalog.pipboy.targets.at(catalog.pipboy.formNames.at("citlyonspridequartersmarker"));
  {
    Conversation li(travelled,catalog.pipboy.formNames.at("doctorliref"));li.Start();
    for(auto topic:{0x426fau,0x5f9bau,0x43df7u,0x43df4u,0x43df3u})li.Choose(topic);
  }
  const auto& complete=travelled.Snapshot().pipboy.quests.at(0x14e87);
  assert(complete.stage==210&&complete.stages.count(40)&&complete.stages.count(210)&&!complete.running);
  assert(complete.status==fo3pipdata::Completion::Complete);
  for(const auto& objective:complete.objectives)assert(objective.second.status==fo3pipdata::Completion::Complete);
  assert(travelled.Snapshot().pipboy.quests.at(0x14e89).stage==30);
  assert(travelled.Snapshot().pipboy.quests.at(0x1d493).stage==150);
  // The authored early-skip outcome has no 300 XP / achievement-4 award.
  assert(travelled.Snapshot().experience==xp&&!travelled.Snapshot().achievements.count(4));
  const auto relocated=travelled.Snapshot().actors.at(sarah);
  assert(relocated.cell==destination.cell&&relocated.world==destination.world&&relocated.yaw==-destination.rz);
  assert((relocated.position==std::array<float,3>{destination.x,destination.y,destination.z}));
  bool disabled=false;
  assert(travelled.ReferenceDisabled(catalog.pipboy.formNames.at("mq01behemothref"),disabled,error)&&disabled);
  assert(travelled.ReferenceDisabled(catalog.pipboy.formNames.at("xmarkpostmq01mutantenabler"),disabled,error)&&!disabled);
  assert(travelled.ReferenceDisabled(catalog.pipboy.formNames.at("colpostmq01gnrcollision"),disabled,error)&&!disabled);
  assert(travelled.Save(save,error));fo3player::Player restored(catalog);assert(restored.Restore(save,error));unlink(save.c_str());
  const auto& saved=restored.Snapshot().pipboy.quests.at(0x14e87);
  assert(saved.stage==210&&!saved.running&&saved.status==complete.status&&saved.journal.size()==complete.journal.size());
  assert(restored.Snapshot().actors.at(sarah).position==relocated.position);
  assert(restored.Snapshot().referenceEnabled==travelled.Snapshot().referenceEnabled);
  assert(restored.Snapshot().experience==xp&&restored.ItemCount(0xf)==0);
  // After actual completion, replay of visited stages must remain a no-op.
  const auto revision=restored.Revision();
  assert(restored.ExecuteQuestStage(0x14e87,210,error)&&restored.ExecuteQuestStage(0x14e89,30,error));
  assert(restored.Revision()==revision&&!restored.ScriptActorMoveEpoch(sarah));
  {Conversation liAgain(restored,catalog.pipboy.formNames.at("doctorliref"));liAgain.Start();}
  assert(restored.Snapshot().experience==xp&&restored.ItemCount(0xf)==0);
  std::cout<<"Original MQ01 Moriarty payment -> early Rivet City dialogue -> authored stage 210; world state, return/reload and one-time effects passed\n";
  // Isolate the original ending conversation using the same stage-40 save.
  // Actor placement is a dialogue fixture: this does not bypass or certify
  // the physical GNR approach, locked entrance or Behemoth battle.
  assert(first.Save(save,error));fo3player::Player normal(catalog);assert(normal.Restore(save,error));unlink(save.c_str());
  const auto dog=catalog.pipboy.formNames.at("threedogref");
  {
    Conversation gnr(normal,dog);gnr.Start();
    for(auto topic:{0x24170u,0x3e5a0u,0x2416cu,0x24164u,0x24162u,0x2415fu,0x26184u,0x3e57fu})gnr.Choose(topic);
  }
  const auto& ending=normal.Snapshot().pipboy.quests.at(0x14e87);
  assert(ending.stage==200&&ending.status==fo3pipdata::Completion::Complete&&!ending.running);
  assert(normal.Snapshot().pipboy.quests.at(0x14e88).stage==10);
  const auto note=catalog.pipboy.formNames.at("mq02virgonote");
  assert(normal.Snapshot().experience==xp+300&&normal.Snapshot().achievements.count(4)&&normal.ItemCount(note)==1);
  assert(normal.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(catalog.pipboy,"RadioGalaxyNewsQuest.ExploitReady"))==1);
  assert(normal.Snapshot().actors.at(sarah).position==relocated.position);
  assert(normal.Save(save,error));fo3player::Player rewarded(catalog);assert(rewarded.Restore(save,error));unlink(save.c_str());
  assert(rewarded.Snapshot().experience==xp+300&&rewarded.ItemCount(note)==1&&rewarded.Snapshot().achievements.count(4));
  const auto completedRevision=rewarded.Revision();assert(rewarded.ExecuteQuestStage(0x14e87,200,error));
  assert(rewarded.Revision()==completedRevision&&!rewarded.ScriptActorMoveEpoch(sarah));
  fo3player::ExperienceNotice notice;assert(!rewarded.PollExperienceNotice(notice));
  {Conversation gnrAgain(rewarded,dog);gnrAgain.Start();}
  assert(rewarded.Snapshot().experience==xp+300&&rewarded.ItemCount(note)==1);
  std::cout<<"Original Three Dog ending conversation: stage 200, MQ02, 300 XP, NOTE/radio/achievement, relocation and duplicate-reward protection passed; GNR world approach not simulated\n";

  const auto id=[&](const char* name){return catalog.pipboy.formNames.at(name);};
  const auto dishRef=id("mq02dishref"),dish=id("mq02virgoiidish"),relay=id("mq02relaywithoutdish");
  const auto quest=id("mq02"),station=id("radiogalaxynewsref02");
  // Same operands as the relay SCDA, with station and relay in slots 1 and 2.
  fo3pipdata::ResultScript bytecode;
  bytecode.references={station,relay};
  bytecode.compiled={
    0xa7,0x11,11,0,2,0,4,0,'t','u','n','e','r',1,0,
    0x1c,0,1,0,0xd6,0x11,2,0,0,0,
    0x1c,0,2,0,0x13,0x10,9,0,2,0,4,0,'n',1,0,0,0};
  std::vector<fo3dialogue::Command> commands;
  assert(fo3dialogue::CompileResult(catalog.pipboy,bytecode,commands,error));
  assert(commands.size()==3&&commands[0].type==fo3dialogue::CommandType::TuneRadio&&commands[0].form==station);
  assert(commands[1].type==fo3dialogue::CommandType::RadioConversation&&commands[1].form==station&&!commands[1].index);
  assert(commands[2].type==fo3dialogue::CommandType::PlayGroup&&commands[2].form==relay&&commands[2].expression=="backward");
  auto sequences=std::make_shared<fo3script::AnimationRegistry>();
  (*sequences)[relay]={{"Backward",1,false}};
  (*sequences)[id("monumentosc")]={{"Forward",1,false}};
  rewarded.SetScriptAnimationRegistry(sequences);
  auto runEvent=[&](uint32_t ref,const char* event,uint32_t actor){
    auto plan=rewarded.PrepareReferenceEventTransaction(ref,event,actor,error,fo3player::ScriptProgramCapture::EnteredPrograms);
    if(!plan)std::cerr<<event<<" "<<std::hex<<ref<<": "<<error<<std::endl;
    assert(plan&&rewarded.CommitScriptTransaction(*plan));
  };
  auto chooseButton=[&](uint32_t ref,uint32_t prompt,int button){
    runEvent(ref,"OnActivate",fo3player::PlayerRef);
    fo3player::ScriptMessage message;
    assert(rewarded.PollScriptMessage(message)&&message.form==prompt);
    assert(rewarded.SubmitScriptMessageButton(ref,button));
    runEvent(ref,"GameMode",0);
  };
  auto arrive=[&](uint32_t marker,uint16_t expected){
    const auto& location=catalog.pipboy.targets.at(marker);
    rewarded.SetScriptPlayerLocation(location.x,location.y,location.z,location.cell,location.world);
    for(unsigned tick=0;tick<60&&rewarded.Snapshot().pipboy.quests.at(quest).stage!=expected;++tick){
      rewarded.PumpWorldAwareGameMode(.25,{},[&](auto plan){return rewarded.CommitScriptTransaction(*plan);});
    }
    assert(rewarded.Snapshot().pipboy.quests.at(quest).stage==expected);
  };
  arrive(id("mq02museummarker"),20);
  chooseButton(dishRef,id("mq02dishmessage"),1);
  assert(!rewarded.ItemCount(dish)&&rewarded.Snapshot().pipboy.quests.at(quest).stage==20);
  chooseButton(dishRef,id("mq02dishmessage"),0);
  assert(rewarded.ItemCount(dish)==1&&rewarded.Snapshot().pipboy.quests.at(quest).stage==30);
  assert(rewarded.ReferenceDisabled(dishRef,disabled,error)&&disabled);
  arrive(id("mq02elevatorentrance"),40);
  const auto karma=rewarded.Snapshot().karma;
  chooseButton(relay,id("mq02relaybeforemessage"),0);
  assert(!rewarded.ItemCount(dish)&&rewarded.Snapshot().pipboy.quests.at(quest).stage==50);
  assert(rewarded.Snapshot().pipboy.tunedRadio==station&&rewarded.Snapshot().karma==karma+200);
  assert(rewarded.ReferenceDisabled(id("radiogalaxynewsref01"),disabled,error)&&disabled);
  assert(rewarded.ReferenceDisabled(station,disabled,error)&&!disabled);
  assert(rewarded.ReferenceDisabled(id("mq02dishdoneref"),disabled,error)&&!disabled);
  runEvent(relay,"GameMode",0);
  assert(rewarded.Snapshot().karma==karma+200);
  assert(rewarded.Save(save,error));
  fo3player::Player repaired(catalog);assert(repaired.Restore(save,error));unlink(save.c_str());
  assert(repaired.Snapshot().pipboy.tunedRadio==station&&repaired.Snapshot().pipboy.quests.at(quest).stage==50);
  std::cout<<"Original Museum dish and Washington Monument relay events passed through MQ02 stage 50\n";

  {Conversation report(repaired,dog);report.Start();
   for(auto topic:{0x24450u,0x2444cu,0x2444du})report.Choose(topic);}
  assert(repaired.Snapshot().pipboy.quests.at(quest).stage==200);
  assert(!repaired.Snapshot().pipboy.quests.at(quest).running);
  assert(repaired.Snapshot().pipboy.quests.at(id("mq03")).running);
  assert(repaired.ItemCount(id("mq02liphotonote"))==1&&repaired.Snapshot().achievements.count(5));
  std::cout<<"Three Dog turn-in completed Galaxy News Radio and started Scientific Pursuits\n";

}
int main(int argc,char** argv){if(argc>1)Original(argv[1]);else {std::cout<<"Original MQ01 checkpoint requires Fallout3.esm; no original-data verification performed\n";return 77;}}
