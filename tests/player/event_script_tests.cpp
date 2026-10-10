#include "player/fo3-player-state.h"
#include <cassert>
#include <cstdio>
#include <iostream>

static void Parsing() {
  fo3pipdata::Script source;
  source.source="scn Example\nshort counter\n; original comment\n"
      "Begin OnActivate Player\n set ExampleQuest.value to 1\nEnd\n"
      "Begin GameMode\n SetStage ExampleQuest 10\nEnd\n";
  auto p=fo3script::ParseEvents(source);
  assert(p.error.empty()&&p.blocks.size()==2);
  assert(p.blocks[0].event=="onactivate"&&p.blocks[0].filter=="player");
  assert(p.blocks[0].sourceLine==4);
  assert(p.blocks[1].event=="gamemode");
  assert(p.blocks[1].body.find("SetStage ExampleQuest 10")!=std::string::npos);
  source.source="Begin OnDeath\nBegin GameMode\nEnd\n";
  assert(!fo3script::ParseEvents(source).error.empty());
  source.source.clear();source.compiled={1,2,3};
  auto compiled=fo3script::ParseEvents(source);
  assert(compiled.compiledOnly&&!compiled.error.empty());
}
static fo3player::Catalog Catalog() {
  fo3player::Catalog c;
  c.initial.baseHealth=100;
  c.pipboy.quests[20].editor="ExampleQuest";
  c.pipboy.quests[20].stages[10]={};
  c.pipboy.formNames["examplequest"]=20;
  c.pipboy.quests[20].script=101;
  c.pipboy.scripts[101].source="scn Q\nBegin GameMode\nSetStage ExampleQuest 10\nEnd";
  c.eventPrograms[101]=fo3script::ParseEvents(c.pipboy.scripts[101]);
  c.references[100].base=30;
  c.eventBaseScripts[30]=102;
  c.pipboy.scripts[102].source="scn NPC\nBegin OnDeath\nStartQuest ExampleQuest\nEnd";
  c.eventPrograms[102]=fo3script::ParseEvents(c.pipboy.scripts[102]);
  return c;
}
static void Dispatch() {
  fo3player::Player p(Catalog());std::string error;
  assert(p.HasReferenceEvent(100,"OnDeath"));
  assert(!p.HasReferenceEvent(100,"OnHit"));
  assert(p.DispatchReferenceEvent(100,"OnHit",0x14,error)==fo3player::ScriptEventResult::NoHandler);
  assert(p.DispatchReferenceEvent(100,"OnDeath",0x14,error)==fo3player::ScriptEventResult::Executed);
  assert(p.Snapshot().pipboy.quests.count(20));
  p.PumpQuestGameMode(0.10);p.PumpQuestGameMode(0.16);
  assert(p.Snapshot().pipboy.quests.at(20).stage==10);
}
static void UnsupportedIsAtomic() {
  auto c=Catalog();
  c.pipboy.scripts[102].source="scn NPC\nBegin OnActivate Player\n"
      "StartQuest ExampleQuest\nEnable\nEnd";
  c.eventPrograms[102]=fo3script::ParseEvents(c.pipboy.scripts[102]);
  fo3player::Player p(std::move(c));std::string error;
  const auto revision=p.Revision();unsigned reports=0;
  p.SetScriptEventDiagnostic([&](const std::string&){++reports;});
  assert(p.DispatchReferenceEvent(100,"OnActivate",0x14,error)==fo3player::ScriptEventResult::Unsupported);
  assert(!error.empty()&&p.Revision()==revision&&p.Snapshot().pipboy.quests.empty());
  assert(p.DispatchReferenceEvent(100,"OnActivate",0x14,error)==fo3player::ScriptEventResult::Unsupported);
  assert(reports==1);
}
static void Filter() {
  auto c=Catalog();
  c.pipboy.scripts[102].source="Begin OnActivate Player\nStartQuest ExampleQuest\nEnd";
  c.eventPrograms[102]=fo3script::ParseEvents(c.pipboy.scripts[102]);
  fo3player::Player p(std::move(c));std::string error;
  assert(p.DispatchReferenceEvent(100,"OnActivate",111,error)==fo3player::ScriptEventResult::Unsupported);
  assert(p.Snapshot().pipboy.quests.empty());
  assert(p.DispatchReferenceEvent(100,"OnActivate",0x14,error)==fo3player::ScriptEventResult::Executed);
  assert(p.Snapshot().pipboy.quests.count(20));
}
static void NumericLocalsAndRefSafety() {
  auto c=Catalog();
  c.pipboy.scripts[102].variables[1]="counter";
  c.pipboy.scripts[102].source="scn Sample\nshort counter\nref other\nBegin OnActivate\nset counter to 7\nEnd";
  c.eventPrograms[102]=fo3script::ParseEvents(c.pipboy.scripts[102]);
  fo3player::Player p(c);std::string error;
  assert(p.DispatchReferenceEvent(100,"OnActivate",0x14,error)==fo3player::ScriptEventResult::Executed);
  assert(p.Snapshot().pipboy.dialogueVariables.at((uint64_t(100)<<32)|1)==7);
  c.pipboy.scripts[102].variables[2]="other";
  c.pipboy.scripts[102].source="scn Sample\nref other\nBegin OnActivate\nset other to 1\nEnd";
  c.eventPrograms[102]=fo3script::ParseEvents(c.pipboy.scripts[102]);
  fo3player::Player r(std::move(c));
  assert(r.DispatchReferenceEvent(100,"OnActivate",0x14,error)==fo3player::ScriptEventResult::Unsupported);
  assert(r.Snapshot().pipboy.dialogueVariables.empty());
}
static void BombMenuAndInventory() {
  auto c=Catalog();
  c.pipboy.messages[500].title="Original test bomb";
  c.pipboy.messages[500].text="Select a response.";
  c.pipboy.messages[500].buttons={"Leave","Consume charge"};
  c.pipboy.formNames["testbombmessage"]=500;
  c.pipboy.formNames["testcharge"]=400;
  c.pipboy.formNames["testsound"]=600;
  c.items[400].formId=400;c.items[400].name="Test charge";
  c.items[400].kind=fo3player::ItemKind::Misc;
  c.items[400].questItem=true;
  c.initial.skills[static_cast<size_t>(fo3player::Skill::Explosives)]=25;
  c.pipboy.scripts[102].variables={{1,"capturebutton"},{2,"button"}};
  c.pipboy.scripts[102].source=
    "scn TestBomb\nshort captureButton\nshort button\n"
    "Begin OnActivate\nif IsActionRef player == 1\n"
    "set captureButton to 1\nshowmessage TestBombMessage\n"
    "endif\nEnd\n"
    "Begin GameMode\nif captureButton == 1\n"
    "set button to getButtonPressed\nif button > -1\n"
    "set captureButton to 0\nif button == 1\n"
    "if player.getItemCount TestCharge >= 1\n"
    "player.removeitem TestCharge 1\nplaysound TestSound\n"
    "endif\nendif\nendif\nendif\nEnd\n";
  c.eventPrograms[102]=fo3script::ParseEvents(c.pipboy.scripts[102]);
  assert(c.eventPrograms[102].error.empty());
  fo3player::Player p(std::move(c));std::string error;
  assert(p.Add(400,1));
  assert(p.DispatchReferenceEvent(100,"OnActivate",fo3player::PlayerRef,error)==fo3player::ScriptEventResult::Executed);
  fo3player::ScriptMessage m;
  assert(p.PollScriptMessage(m)&&m.form==500&&m.owner==100&&m.buttons.size()==2);
  assert(p.ItemCount(400)==1);
  p.PumpReferenceGameMode(.25,{100});
  assert(p.ItemCount(400)==1&&!p.PollScriptMessage(m));
  assert(p.SubmitScriptMessageButton(100,1));
  p.PumpReferenceGameMode(.25,{100});
  assert(p.ItemCount(400)==0&&p.ScriptButton()==-1);
  uint32_t sound=0;assert(p.PollScriptSound(sound)&&sound==600&&!p.PollScriptSound(sound));
  assert(!p.SubmitScriptMessageButton(100,-1));
  // A failed gameplay event must preserve inventory and local variables,
  // and must not repeatedly retry the same button at every script tick.
  auto broken=Catalog();
  broken.pipboy.scripts[102].variables={{1,"capturebutton"}};
  broken.pipboy.scripts[102].source=
    "scn B\nshort captureButton\nBegin GameMode\n"
    "if getButtonPressed == 1\n"
    "set captureButton to 7\nplayer.removeitem TestCharge 1\n"
    "Enable\nendif\nEnd\n";
  broken.pipboy.formNames["testcharge"]=400;
  broken.items[400].formId=400;
  broken.eventPrograms[102]=fo3script::ParseEvents(broken.pipboy.scripts[102]);
  fo3player::Player rejected(std::move(broken));
  assert(rejected.Add(400,1)&&rejected.SubmitScriptMessageButton(100,1));
  const auto previous=rejected.Revision();
  rejected.PumpReferenceGameMode(.25,{100});
  assert(rejected.ItemCount(400)==1&&rejected.ScriptButton()==-1);
  assert(rejected.Snapshot().pipboy.dialogueVariables.empty()&&rejected.Revision()==previous);
}

static void DisarmStagePersistence() {
  auto c=Catalog();
  c.pipboy.formNames["testcharge"]=400;
  c.pipboy.formNames["simmsref"]=201;
  c.items[400].formId=400;c.items[400].name="Fusion test charge";
  c.items[400].questItem=true;
  c.actorPlacements[201].base=300;
  c.weapons.actors[300].health=100;
  c.pipboy.quests[20].stages[30].items.push_back({});
  c.pipboy.quests[20].stages[30].items[0].result.source=
      "SetQuestObject TestCharge 0\n"
      "simmsref.evp\n"
      "RewardKarma 200\n";
  fo3player::Player p(c);std::string error;
  assert(p.IsQuestObject(400));
  assert(p.ExecuteQuestStage(20,30,error)&&error.empty());
  assert(!p.IsQuestObject(400)&&p.Snapshot().karma==200);
  assert(p.Snapshot().pipboy.quests.at(20).stage==30);
  const std::string path="/tmp/fq-script-stage-test.fqps";
  assert(p.Save(path,error));
  fo3player::Player loaded(std::move(c));
  assert(loaded.Restore(path,error)&&!loaded.IsQuestObject(400));
  assert(loaded.Snapshot().karma==200);
  std::remove(path.c_str());
}
int main(){
  Parsing();Dispatch();UnsupportedIsAtomic();Filter();NumericLocalsAndRefSafety();BombMenuAndInventory();DisarmStagePersistence();
  std::cout<<"Original event source indexing, dispatch, scheduler and rollback passed\n";
}
