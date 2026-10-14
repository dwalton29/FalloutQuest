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

static void NoticeTransactions() {
  auto c=Catalog();c.pipboy.quests[20].name="Quest name";
  c.pipboy.quests[20].objectives[1].text="Authored objective";
  c.pipboy.formNames["examplequest"]=20;
  c.pipboy.formNames["testcharge"]=400;c.items[400].formId=400;
  c.pipboy.quests[20].stages[10].items.push_back({});
  c.pipboy.quests[20].stages[10].items[0].result.source="SetObjectiveDisplayed ExampleQuest 1 1\r\n";
  fo3player::Player p(c);std::string error;fo3player::QuestNotice n;
  assert(p.StartQuest(20)&&!p.PollQuestNotice(n));
  const auto revision=p.Revision();
  fo3pipdata::ResultScript r;r.source="SetStage ExampleQuest 10\r\n";
  assert(p.PreviewDialogueResult(r,error)&&p.Revision()==revision&&!p.PollQuestNotice(n));
  // A later rejected command rolls back a previously queued fanfare.
  r.source="SetStage ExampleQuest 10\r\nPlayer.RemoveItem TestCharge 1\r\n";
  assert(!p.ExecuteDialogueResult(r,error)&&!p.PollQuestNotice(n));
  assert(!p.Snapshot().pipboy.quests.at(20).announced);
  assert(p.ExecuteQuestStage(20,10,error)&&p.PollQuestNotice(n));
  assert(n.kind==fo3player::QuestNoticeKind::Added&&n.quest==20&&!p.PollQuestNotice(n));
  assert(p.ExecuteQuestStage(20,10,error)&&!p.PollQuestNotice(n));
  assert(p.SetObjective(20,1,true,fo3pipdata::Completion::Complete)&&p.PollQuestNotice(n));
  assert(n.kind==fo3player::QuestNoticeKind::Updated);
  assert(p.SetObjective(20,1,true,fo3pipdata::Completion::Complete)&&!p.PollQuestNotice(n));
  assert(p.SetObjective(20,1,false,fo3pipdata::Completion::Complete));
  assert(p.SetObjective(20,1,true,fo3pipdata::Completion::Complete));
  assert(p.PollQuestNotice(n)&&n.kind==fo3player::QuestNoticeKind::Updated&&!p.PollQuestNotice(n));
  const char* path="/tmp/fq-quest-notice.fqps";assert(p.Save(path,error));
  fo3player::Player restored(c);assert(restored.Restore(path,error)&&!restored.PollQuestNotice(n));
  assert(restored.Snapshot().pipboy.quests.at(20).announced);
  assert(restored.FinishQuest(20,fo3pipdata::Completion::Complete)&&restored.PollQuestNotice(n));
  assert(n.kind==fo3player::QuestNoticeKind::Completed&&!restored.PollQuestNotice(n));
  assert(restored.FinishQuest(20,fo3pipdata::Completion::Complete)&&!restored.PollQuestNotice(n));
  std::remove(path);
}
static void OriginalBomb(const char* path) {
  fo3player::Catalog c;std::string error;assert(fo3player::LoadCatalog(path,c,error));
  assert(c.eventBaseScripts.at(0x3bc8)==0x78cb3);
  // Preserve the actual ESM source (including CRLF), message text, and scripts.
  assert(c.pipboy.scripts.at(0x78cb3).source.find("\r\n")!=std::string::npos);
  fo3player::Player low(c);fo3player::ScriptMessage message;
  assert(low.DispatchReferenceEvent(0x14bc8,"OnActivate",0x14,error)==fo3player::ScriptEventResult::Executed);
  assert(low.PollScriptMessage(message)&&message.form==0x78cb1&&message.text.find("25")!=std::string::npos);
  assert(!low.PollScriptMessage(message));
  assert(low.DispatchReferenceEvent(0x14bc8,"OnActivate",111,error)==fo3player::ScriptEventResult::Executed);
  assert(!low.PollScriptMessage(message)); // IsActionRef gate is false.
  c.initial.skills[static_cast<size_t>(fo3player::Skill::Explosives)]=25;
  fo3player::Player p(c);p.RegisterOriginalActors();
  const auto& target=c.pipboy.targets.at(0x14bc8);
  p.SetScriptPlayerLocation(target.x,target.y,target.z,target.cell,target.world);
  p.SetScriptEventDiagnostic([](const std::string& diagnostic){std::cerr<<diagnostic<<"\n";});
  assert(p.ExecuteQuestStage(0x14e9e,10,error));
  fo3player::QuestNotice notice;assert(p.PollQuestNotice(notice)&&notice.kind==fo3player::QuestNoticeKind::Added);
  assert(p.DispatchReferenceEvent(0x14bc8,"OnActivate",0x14,error)==fo3player::ScriptEventResult::Executed);
  assert(p.PollScriptMessage(message)&&message.form==0x78cb2&&message.buttons.size()==3);
  assert(message.buttons==c.pipboy.messages.at(0x78cb2).buttons);
  assert(p.SubmitScriptMessageButton(0x14bc8,0));p.PumpReferenceGameMode(.25,{0x14bc8});
  assert(p.Snapshot().pipboy.quests.at(0x14e9e).stage==10&&!p.PollScriptMessage(message));
  assert(p.DispatchReferenceEvent(0x14bc8,"OnActivate",0x14,error)==fo3player::ScriptEventResult::Executed);
  assert(p.PollScriptMessage(message)&&message.form==0x78cb2);
  assert(p.SubmitScriptMessageButton(0x14bc8,1));p.PumpReferenceGameMode(.25,{0x14bc8});
  assert(p.Snapshot().pipboy.quests.at(0x14e9e).stage==30);
  assert(p.Snapshot().karma==200);
  assert(p.PollScriptMessage(message)&&message.text==c.pipboy.messages.at(message.form).text);
  uint32_t sound;assert(p.PollScriptSound(sound)&&sound==c.pipboy.formNames.at("objmegatonbombtamperarm"));
  assert(p.PollQuestNotice(notice)&&notice.kind==fo3player::QuestNoticeKind::Updated);
  assert(!p.PollQuestNotice(notice));
  const auto revision=p.Revision();p.PumpReferenceGameMode(.25,{0x14bc8});assert(p.Revision()==revision);
  const char* save="/tmp/fq-original-bomb.fqps";assert(p.Save(save,error));
  fo3player::Player restored(c);assert(restored.Restore(save,error));
  assert(restored.Snapshot().pipboy.quests.at(0x14e9e).stage==30&&restored.Snapshot().karma==200);
  assert(!restored.PollQuestNotice(notice));std::remove(save);

  fo3player::Player rig(c);rig.RegisterOriginalActors();
  rig.SetScriptPlayerLocation(target.x,target.y,target.z,target.cell,target.world);
  assert(rig.ExecuteQuestStage(0x14e9e,10,error));
  assert(rig.PollQuestNotice(notice));
  assert(rig.DispatchReferenceEvent(0x14bc8,"OnActivate",0x14,error)==fo3player::ScriptEventResult::Executed);
  assert(rig.PollScriptMessage(message)&&message.form==0x78cb2);
  assert(rig.SubmitScriptMessageButton(0x14bc8,2));rig.PumpReferenceGameMode(.25,{0x14bc8});
  assert(rig.Snapshot().pipboy.quests.at(0x14e9e).stage==10&&rig.PollScriptMessage(message));
  assert(message.text==c.pipboy.messages.at(message.form).text&&!rig.PollQuestNotice(notice));
  const auto charge=c.pipboy.formNames.at("fusionpulsecharge");assert(rig.Add(charge,1));
  assert(rig.DispatchReferenceEvent(0x14bc8,"OnActivate",0x14,error)==fo3player::ScriptEventResult::Executed);
  assert(rig.PollScriptMessage(message)&&message.form==0x78cb2);
  assert(rig.SubmitScriptMessageButton(0x14bc8,2));
  assert(rig.DispatchReferenceEvent(0x14bc8,"GameMode",0,error)==fo3player::ScriptEventResult::Unsupported);
  assert(error.find(".enable")!=std::string::npos);
  assert(rig.ItemCount(charge)==1&&rig.Snapshot().pipboy.quests.at(0x14e9e).stage==10);
  assert(!rig.PollScriptMessage(message)&&!rig.PollQuestNotice(notice));
  std::cout<<"Original ESM bomb activation, cancel, disarm, Karma, notices and persistence passed\n";
}
int main(int argc,char** argv){
  Parsing();Dispatch();UnsupportedIsAtomic();Filter();NumericLocalsAndRefSafety();BombMenuAndInventory();DisarmStagePersistence();NoticeTransactions();
  if(argc>1)OriginalBomb(argv[1]);
  std::cout<<"Original event source indexing, dispatch, scheduler and rollback passed\n";
}
