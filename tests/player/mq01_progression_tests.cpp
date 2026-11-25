#include "mq01_test_conversation.h"
#include "scripting/fo3-script-master-match.h"
#include "ui/pause/fo3-debug-menu.h"
#include "ui/terminal/fo3-terminal-state.h"
bool LoadQuestObjectClips(const std::string&,std::vector<fo3script::AnimationSequence>&);
bool SitInQuestFurniture(const fo3pipdata::Definitions&,uint32_t,const std::string&,uint8_t&,std::array<float,3>&,std::array<float,3>&);
#include <cassert>
#include <iostream>
#include <unistd.h>

// Original-data checkpoint. The explicit developer entry action executes the
// original first stage. Locations substitute for world traversal, not quest
// gates; subsequent progression is owned by original INFO results.
static void Original(const char* path,const std::string& meshRoot){
  fo3player::Catalog catalog;std::string error;assert(fo3player::LoadCatalog(path,catalog,error));
  char savePath[]="/tmp/fq-mq01-progression-XXXXXX";
  const int fd=mkstemp(savePath);assert(fd>=0);close(fd);
  const std::string save(savePath);
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
  assert(first.Save(save,error));fo3player::Player travelled(catalog);if(!travelled.Restore(save,error)){std::cerr<<"Failed to restore Moriarty save: "<<error<<std::endl;std::abort();}
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
  if(meshRoot.empty()){
    (*sequences)[relay]={{"Backward",1,false}};
    (*sequences)[id("monumentosc")]={{"Forward",1,false}};
  }else{
    const std::string monument="architecture/washingtonmonument/";
    const std::vector<std::pair<const char*,std::string>> models={
      {"mq02relaywithoutdish","clutter/questitems/mq02radiotower.nif"},
      {"monumentosc",monument+"oscilloscope.nif"},{"washmonumentgate01ref",monument+"washmonumentgate01.nif"},
      {"washingmonumentelevatorinsidepanel",monument+"elevatorcontrol01.nif"},
      {"washdoorbase01",monument+"washelevatordoor01.nif"},{"elevatorshaft",monument+"washingtonmonelevatorshaft01.nif"},
      {"washeshaftlight01ref",monument+"fxwashelvtrshaftlight01.nif"},
      {"washeshaftlight02ref",monument+"fxwashelvtrshaftlight01.nif"},
      {"washeshaftlight03ref",monument+"fxwashelvtrshaftlight01.nif"}
    };
    for(const auto& model:models){
      const auto path=meshRoot+"/"+model.second;
      if(!LoadQuestObjectClips(path,(*sequences)[id(model.first)])){
        std::cerr<<"Could not register the original controller sequences in "<<path<<std::endl;std::abort();
      }
    }
  }
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
  if(!meshRoot.empty()){
    const auto terminalBase=id("mq02washingtonmonumentterminal"),gate=id("washmonumentgate01ref");
    const auto& gatePosition=catalog.pipboy.targets.at(gate);
    rewarded.SetScriptPlayerLocation(gatePosition.x,gatePosition.y,gatePosition.z,gatePosition.cell,gatePosition.world);
    uint32_t terminalRef=0;float best=INFINITY;
    for(const auto& placed:catalog.pipboy.targets){
      if(placed.second.base!=terminalBase||placed.second.world!=gatePosition.world)continue;
      const float distance=std::hypot(placed.second.x-gatePosition.x,placed.second.y-gatePosition.y);
      if(distance<best){best=distance;terminalRef=placed.first;}
    }
    assert(terminalRef&&rewarded.TerminalPassword(terminalRef));
    fo3terminal::Session terminal;terminal.Open(rewarded,terminalRef,0);
    assert(terminal.page==fo3terminal::Session::Page::Menu&&!terminal.entries.empty());
    auto open=rewarded.PrepareTerminalTransaction(terminalRef,terminal.menu,terminal.entries.front(),error);
    if(!open)std::cerr<<"Monument access terminal: "<<error<<std::endl;
    assert(open&&rewarded.CommitScriptTransaction(*open));
    bool opening=false;
    assert(rewarded.ScriptAnimationPlaying(gate,opening,"Forward")&&opening);
  }
  arrive(id("mq02elevatorentrance"),40);
  if(!meshRoot.empty()){
    const auto panel=id("washingmonumentelevatorinsidepanel"),destination=id("mq02elevatorexit");
    const auto& button=catalog.pipboy.targets.at(panel);
    rewarded.SetScriptPlayerLocation(button.x,button.y,button.z,button.cell,button.world);
    rewarded.SetScriptRuntimeClock(10.,.1);
    auto press=rewarded.PrepareActivationTransaction(panel,fo3player::PlayerRef,error);
    if(!press)std::cerr<<"Elevator control: "<<error<<std::endl;
    assert(press&&rewarded.CommitScriptTransaction(*press));
    bool shutting=false;assert(rewarded.ScriptAnimationPlaying(id("washdoorbase01"),shutting,"Backward")&&shutting);
    std::unordered_set<uint32_t> moving;
    for(unsigned tick=1;tick<=600&&!rewarded.Snapshot().playerMove.pending;++tick){
      rewarded.SetScriptRuntimeClock(10.+.1*tick,.1);
      runEvent(panel,"GameMode",0);
      fo3script::Action action;
      while(rewarded.PollScriptAction(action))if(action.kind==fo3script::ActionKind::PlayGroup)moving.insert(action.reference);
    }
    const auto move=rewarded.Snapshot().playerMove;
    assert(move.pending&&move.marker==destination&&moving.count(id("elevatorshaft"))&&moving.count(panel));
    for(const auto* light:{"washeshaftlight01ref","washeshaftlight02ref","washeshaftlight03ref"})assert(moving.count(id(light)));
    rewarded.SetScriptPlayerLocation(move.position[0],move.position[1],move.position[2],move.cell,move.world);
    rewarded.AcknowledgePlayerMove();
    std::cout<<"Monument password opened the gate and original elevator animation completed the MoveTo handoff\n";
  }
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

  {
    // A player who finds Doctor Li before reporting the repair can take the
    // alternative reward instead of directions to Rivet City.
    fo3player::Player detour(repaired);
    Conversation li(detour,id("doctorliref"));li.Start();
    for(auto topic:{0x426fau,0x5f9bau,0x43df7u,0x43df4u,0x43df3u})li.Choose(topic);
    assert(detour.Snapshot().pipboy.quests.at(id("mq03")).stage>=30);
    Conversation deal(detour,dog);deal.Start();
    auto chooseAvailable=[&](std::initializer_list<uint32_t> topics){
      for(auto topic:topics){
        if(std::any_of(deal.session.choices.begin(),deal.session.choices.end(),[&](const auto& choice){return choice.topic==topic;})){
          deal.Choose(topic);return;
        }
      }
      for(const auto& choice:deal.session.choices)std::cerr<<std::hex<<choice.topic<<" ";
      std::cerr<<std::dec<<"missing GNR reward choice"<<std::endl;std::abort();
    };
    chooseAvailable({0x24450,0x2444f,0x3e578});
    chooseAvailable({0xbc710,0xbc711,0x58fa6,0x58fa7});
    assert(detour.Snapshot().pipboy.quests.at(quest).stage==200&&detour.ItemCount(id("mq02lootstashkey"))==1&&
           detour.ItemCount(id("mq02weaponscache"))==1);
    std::cout<<"Three Dog's alternate weapons cache reward unlocked after finding Doctor Li independently\n";
  }

  {Conversation report(repaired,dog);report.Start();
   for(auto topic:{0x24450u,0x2444cu,0x2444du})report.Choose(topic);}
  assert(repaired.Snapshot().pipboy.quests.at(quest).stage==200);
  assert(!repaired.Snapshot().pipboy.quests.at(quest).running);
  assert(repaired.Snapshot().pipboy.quests.at(id("mq03")).running);
  assert(repaired.ItemCount(id("mq02liphotonote"))==1&&repaired.Snapshot().achievements.count(5));
  std::cout<<"Three Dog turn-in completed Galaxy News Radio and started Scientific Pursuits\n";

  const auto pursuits=id("mq03"),panel=id("mqapanelref");
  auto visit=[&](uint32_t marker,uint16_t stage){
    const auto& location=catalog.pipboy.targets.at(marker);
    repaired.SetScriptPlayerLocation(location.x,location.y,location.z,location.cell,location.world);
    for(unsigned tick=0;tick<60&&repaired.Snapshot().pipboy.quests.at(pursuits).stage!=stage;++tick)
      repaired.PumpWorldAwareGameMode(.25,{},[&](auto plan){return repaired.CommitScriptTransaction(*plan);});
    assert(repaired.Snapshot().pipboy.quests.at(pursuits).stage==stage);
  };
  visit(id("sciencelabmarker"),20);
  {Conversation li(repaired,id("doctorliref"));li.Start();
   for(auto topic:{0x426fau,0x5f9bau,0x43df7u,0x43df4u,0x43df3u})li.Choose(topic);}
  assert(repaired.Snapshot().pipboy.quests.at(pursuits).stage==30);
  visit(id("mq05controlroommarker"),40);
  auto panelTick=[&]{
    auto plan=repaired.PrepareReferenceEventTransaction(panel,"GameMode",0,error,fo3player::ScriptProgramCapture::EnteredPrograms);
    if(!plan)std::cerr<<error<<std::endl;
    assert(plan&&repaired.CommitScriptTransaction(*plan));
  };
  panelTick();
  assert(repaired.Snapshot().pipboy.quests.at(pursuits).stage==40);
  assert(repaired.Pickup(id("mq03holo21ref")));
  panelTick();
  assert(repaired.Snapshot().pipboy.quests.at(pursuits).stage==50);
  assert(repaired.Snapshot().pipboy.quests.at(pursuits).objectives.at(50).displayed);
  std::cout<<"Scientific Pursuits reached the journal listening objective via Rivet City, Doctor Li and the placed holotape\n";

  const auto tape=id("mq03holo21");
  const auto segments=fo3pipdata::NoteSegments(catalog.pipboy,tape);
  assert(segments.size()==1&&segments.front().paths.size()==4);
  const auto& journal=catalog.pipboy.notes.at(tape);
  auto finished=repaired.PrepareDialogueInfoTransaction(journal.topic,segments.front().info,
      fo3player::DialogueScriptPhase::End,error,0,fo3player::ScriptProgramCapture::EnteredPrograms);
  if(!finished)std::cerr<<error<<std::endl;
  assert(finished&&repaired.CommitScriptTransaction(*finished));
  assert(repaired.Snapshot().pipboy.quests.at(pursuits).stage==60);
  assert(repaired.Snapshot().pipboy.quests.at(pursuits).objectives.at(60).displayed);
  const auto read=fo3dialogue::VariableKey(catalog.pipboy,"MQ03.HoloRead");
  assert(repaired.Snapshot().pipboy.dialogueVariables.at(read)==1);
  assert(repaired.Save(save,error));
  fo3player::Player listened(catalog);assert(listened.Restore(save,error));unlink(save.c_str());
  assert(listened.Snapshot().pipboy.quests.at(pursuits).stage==60);
  std::cout<<"Dad's journal end result displayed Vault 112 objective and survived reload\n";

  if(!meshRoot.empty()){
    const auto suit=id("vaultsuit112"),shell=id("mq04playerpodshellref"),chair=id("mq04playerpod");
    const auto& pod=catalog.pipboy.targets.at(shell);
    listened.SetScriptPlayerLocation(pod.x,pod.y,pod.z,pod.cell,pod.world);
    {
      Conversation robot(listened,id("mq03robobrain01"));robot.Start();
      assert(listened.ItemCount(suit)==1&&listened.Snapshot().pipboy.quests.at(pursuits).stage==65);
    }
    // Vault 112 can also be discovered before Three Dog or Doctor Li.
    fo3player::Player explorer(catalog);explorer.RegisterOriginalActors();
    {
      Conversation robot(explorer,id("mq03robobrain01"));robot.Start();
      assert(explorer.ItemCount(suit)==1&&explorer.Snapshot().pipboy.quests.at(pursuits).stage==65);
      const auto& gear=explorer.Snapshot().inventory;
      auto provided=std::find_if(gear.begin(),gear.end(),[&](const auto& stack){return stack.formId==suit;});
      assert(provided!=gear.end()&&explorer.Equip(provided->id)&&explorer.Snapshot().pipboy.quests.at(pursuits).stage==70);
    }
    auto animations=std::make_shared<fo3script::AnimationRegistry>();
    assert(LoadQuestObjectClips(meshRoot+"/furniture/tlpodplayer01.nif",(*animations)[shell]));
    listened.SetScriptAnimationRegistry(animations);
    auto activate=[&](uint32_t ref){
      auto plan=listened.PrepareActivationTransaction(ref,fo3player::PlayerRef,error);
      if(!plan)std::cerr<<"Pod activation "<<std::hex<<ref<<std::dec<<": "<<error<<std::endl;
      assert(plan&&listened.CommitScriptTransaction(*plan));
    };
    activate(shell);
    fo3player::ScriptMessage noSuit;
    assert(listened.PollScriptMessage(noSuit)&&noSuit.form==id("mq04podnosuitmessage"));
    const auto& inventory=listened.Snapshot().inventory;
    const auto clothes=std::find_if(inventory.begin(),inventory.end(),[&](const auto& stack){return stack.formId==suit;});
    assert(clothes!=inventory.end()&&listened.Equip(clothes->id));
    const auto suitOn=fo3dialogue::VariableKey(catalog.pipboy,"MQ03.SuitOn");
    assert(listened.Snapshot().pipboy.quests.at(pursuits).stage==70);
    assert(listened.Snapshot().pipboy.dialogueVariables.at(suitOn)==1);
    activate(shell);
    bool opening=false;assert(listened.ScriptAnimationPlaying(shell,opening,"Forward")&&opening);
    activate(chair);
    bool delegated=false;fo3script::Action action;
    while(listened.PollScriptAction(action))
      if(action.kind==fo3script::ActionKind::DefaultActivate&&action.reference==chair)delegated=true;
    assert(delegated);
    uint8_t seatMarker=0;std::array<float,3> seatedEye{},actorOrigin{};
    assert(SitInQuestFurniture(catalog.pipboy,chair,meshRoot,seatMarker,seatedEye,actorOrigin));
    const auto& placement=catalog.pipboy.targets.at(chair);
    listened.SetScriptPlayerLocation(seatedEye[0],seatedEye[1],(seatedEye[2]+placement.z)*.5f,placement.cell,placement.world,0,&actorOrigin);
    listened.SetFurnitureUse(chair,seatMarker,3);
    const auto chairState=fo3dialogue::VariableKey(catalog.pipboy,"MQ04.PlayerInChair");
    for(int tick=1;tick<=66;++tick){
      listened.SetScriptRuntimeClock(tick*.25,.25);
      auto plan=listened.PrepareReferenceEventTransaction(chair,"GameMode",0,error);
      if(!plan)std::cerr<<"Vault 112 chair tick: "<<error<<std::endl;
      assert(plan&&listened.CommitScriptTransaction(*plan));
      if(tick==1){
        bool closing=false;
        if(!listened.ScriptAnimationPlaying(shell,closing,"Backward")||!closing)
          std::cerr<<"Seated origin "<<actorOrigin[0]<<","<<actorOrigin[1]<<","<<actorOrigin[2]<<" eye "<<seatedEye[0]<<","<<seatedEye[1]<<","<<seatedEye[2]<<" chair "<<placement.x<<","<<placement.y<<","<<placement.z<<" chair var "<<(listened.Snapshot().pipboy.dialogueVariables.count(chairState)?listened.Snapshot().pipboy.dialogueVariables.at(chairState):0)<<std::endl;
        assert(closing);
        assert(listened.Snapshot().pipboy.dialogueVariables.at(chairState)==1);
      }
    }
    assert(listened.Snapshot().pipboy.dialogueVariables.at(chairState)==2);
    std::cout<<"Original pod chair marker, close animation, and 14-second countdown reached the simulation handoff\n";
    const auto simulation=id("mq04");
    const auto simScript=catalog.pipboy.quests.at(simulation).script;
    listened.SetScriptEventDiagnostic([&](const std::string& diagnostic){
      if(diagnostic.find("SCPT "+std::to_string(simScript)+" ")!=std::string::npos)std::cerr<<diagnostic<<std::endl;
    });
    auto runSimulation=[&](int tick){
      listened.SetScriptRuntimeClock(17+tick*.25,.25);
      listened.PumpWorldAwareGameMode(.25,{},[&](auto plan){
        fo3script::ScriptMasterMatch match;std::atomic<bool> cancel{false};
        if(!fo3script::MatchScriptProgramsToMaster(path,plan->ExecutedScripts(),match,cancel,error)){
          std::cerr<<"Vault 112 world publication: "<<error<<std::endl;return false;
        }
        return listened.CommitScriptTransaction(*plan);
      });
    };
    for(int tick=0;tick<4&&!listened.Snapshot().playerMove.pending;++tick)runSimulation(tick);
    const auto entry=listened.Snapshot().playerMove;
    assert(entry.pending&&entry.marker==id("mq04startbench")&&entry.world==id("tranquilitylane"));
    assert(listened.Snapshot().pipboy.dialogueVariables.at(suitOn)==0);
    const auto outfit=id("mq04playeroutfit");
    bool wearing=false;assert(listened.ScriptEquippedItem(fo3player::PlayerRef,outfit,wearing)&&wearing);
    listened.SetFurnitureUse(0,0,0);
    listened.SetScriptPlayerLocation(entry.position[0],entry.position[1],entry.position[2],entry.cell,entry.world);
    listened.AcknowledgePlayerMove();
    for(int tick=4;tick<48;++tick)runSimulation(tick);
    assert(listened.Snapshot().pipboy.quests.at(pursuits).stage==200&&listened.Snapshot().pipboy.quests.at(simulation).stage==5);
    std::cout<<"Original simulation entry equipped the outfit, completed Scientific Pursuits and started Tranquility Lane\n";
    // Returning belongings from the same original storage is independently exercised.
    fo3pipdata::ResultScript transfer;
    const auto locker=id("mq04playercontainerref");
    auto stored=listened.ContainerContents(locker);
    assert(stored&&!stored->empty());
    const auto belongings=*stored;
    assert(listened.Save(save,error));
    fo3player::Player returning(catalog);assert(returning.Restore(save,error));unlink(save.c_str());
    transfer.source="MQ04PlayerContainerRef.RemoveAllItems Player";
    assert(returning.ExecuteDialogueResult(transfer,error)&&returning.ContainerContents(locker)->empty());
    assert(returning.Snapshot().pipboy.quests.at(pursuits).stage==200&&returning.Snapshot().pipboy.quests.at(simulation).stage==5);
    const auto& returned=returning.Snapshot().inventory;
    for(const auto& stack:belongings){
      const auto it=std::find_if(returned.begin(),returned.end(),[&](const auto& current){return current.id==stack.id;});
      assert(it!=returned.end()&&it->formId==stack.formId&&it->count==stack.count&&it->condition==stack.condition&&
             it->loadedRounds==stack.loadedRounds&&it->needsAction==stack.needsAction&&!it->equipped);
    }
    std::cout<<"Vault 112 storage preserved inventory identities, condition and weapon state through save and return\n";
    std::cout<<"Vault 112 robot supplied the suit, shell checked equipped gear, and chair delegated native seating\n";
  }

}
int main(int argc,char** argv){if(argc>1)Original(argv[1],argc>2?argv[2]:"");else {std::cout<<"Original MQ01 checkpoint requires Fallout3.esm; no original-data verification performed\n";return 77;}}
