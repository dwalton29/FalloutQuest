#include "dialogue/fo3-dialogue-session.h"
#include "scripting/fo3-nonresident-script.h"
#include <cassert>
#include <iostream>
#include <unistd.h>

// Original-data gameplay checkpoint. The only quest-state fixture is valid
// post-Escape entry. Locations substitute for world traversal, not quest gates.
// No stage is advanced by the harness; original INFO results own progression.
struct Conversation {
  fo3player::Player& player;
  fo3dialogue::Session session;
  fo3dialogue::Context context;
  explicit Conversation(fo3player::Player& p,uint32_t reference):player(p){
    const auto& placed=p.Definitions().pipboy.targets.at(reference);
    context.player=&p;
    context.speaker={reference,placed.base,placed.cell,placed.world,0,placed.x,placed.y,placed.z,false,false};
    context.target={fo3player::PlayerRef,7,placed.cell,placed.world,0,placed.x,placed.y+40,placed.z,false,false};
    p.SetScriptPlayerLocation(placed.x,placed.y+40,placed.z,placed.cell,placed.world);
    session.previewReferences=[this,reference](const auto& current,const auto& plan,std::string& error){
      return fo3script::NonresidentActorRemoval(current,plan,context.target.cell,context.target.world,{reference},error);
    };
    session.publishReferences=[this](auto& current,auto& plan,std::string& error){
      return session.previewReferences(current,plan,error)&&current.CommitScriptTransaction(plan);
    };
  }
  ~Conversation(){session.End("leave area");}
  void Finish(){
    unsigned responses=0;
    while(session.phase==fo3dialogue::Phase::Speaking){
      assert(++responses<100);const auto token=session.audioToken;
      (void)session.AudioDone(token,true,context,player);
    }
  }
  void Start(){assert(session.Start(context,player));Finish();}
  void Choose(uint32_t topic){
    const auto choice=std::find_if(session.choices.begin(),session.choices.end(),[&](const auto& row){return row.topic==topic;});
    assert(choice!=session.choices.end());
    assert(session.Choose(size_t(choice-session.choices.begin()),context,player));Finish();
  }
};
static void Original(const char* path){
  fo3player::Catalog catalog;std::string error;assert(fo3player::LoadCatalog(path,catalog,error));
  auto& initial=catalog.initial.pipboy.quests[0x14e87];
  initial.stage=10;initial.running=true;initial.stages.insert(10);initial.objectives[10].displayed=true;
  const auto save="/tmp/fq-mq01-progression-"+std::to_string(getpid());
  fo3player::Player first(catalog);first.RegisterOriginalActors();assert(first.Add(0xf,100));
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
  assert(relocated.cell==destination.cell&&relocated.world==destination.world&&relocated.yaw==destination.rz);
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

}
int main(int argc,char** argv){if(argc>1)Original(argv[1]);else {std::cout<<"Original MQ01 checkpoint requires Fallout3.esm; no original-data verification performed\n";return 77;}}
