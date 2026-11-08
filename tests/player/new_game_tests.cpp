#include "player/fo3-new-game.h"
#include "player/fo3-player-presentation.h"
#include "scripting/fo3-script-master-match.h"
#include "world/fo3-resident-authored-admission.h"
#include "ui/chargen/fo3-name-entry.h"
#include "ui/chargen/fo3-special-book.h"
#include "ui/chargen/fo3-tag-skills.h"
#include "player/fo3-vr-body.h"
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
static void HeadingAngles(){
  using namespace fo3player;Catalog c;c.initial.special.fill(5);c.initial.baseHealth=100;
  c.references[100].base=10;c.references[100].cell=1;c.references[100].valid=true;
  c.actorPlacements[100].base=10;c.actorPlacements[100].cell=1;c.weapons.actors[10].health=20;
  c.pipboy.targets[100].base=10;c.pipboy.targets[100].cell=1;
  for(uint32_t id=200;id<=205;++id){c.references[id].base=20;c.references[id].valid=true;c.references[id].cell=1;c.pipboy.targets[id].base=20;c.pipboy.targets[id].cell=1;}
  c.pipboy.targets[200].x=10;c.pipboy.targets[201].y=10;c.pipboy.targets[202].x=-10;c.pipboy.targets[203].y=-10;c.pipboy.targets[205].cell=2;
  c.eventBaseScripts[10]=300;c.pipboy.objectReferenceScripts[100]={300,10};c.pipboy.formNames["target"]=200;
  auto& source=c.pipboy.scripts[300];source.variables[1]="angle";source.source="scn Heading\nfloat angle\nBegin OnTrigger player\nset angle to player.getHeadingAngle target\nEnd";c.eventPrograms[300]=fo3script::ParseEvents(source);
  Player p(c);p.RegisterOriginalActors();p.SetScriptPlayerLocation(0,0,0,1,0);float angle=999;
  assert(!p.ScriptHeadingAngle(PlayerRef,200,angle));p.SetScriptPlayerHeading(0);
  assert(p.ScriptHeadingAngle(PlayerRef,200,angle)&&std::fabs(angle-90)<.001f);
  assert(p.ScriptHeadingAngle(PlayerRef,201,angle)&&std::fabs(angle)<.001f);
  assert(p.ScriptHeadingAngle(PlayerRef,202,angle)&&std::fabs(angle+90)<.001f);
  assert(p.ScriptHeadingAngle(PlayerRef,203,angle)&&std::fabs(std::fabs(angle)-180)<.001f);
  assert(p.ScriptHeadingAngle(PlayerRef,204,angle)&&angle==0);
  assert(!p.ScriptHeadingAngle(PlayerRef,205,angle)&&!p.ScriptHeadingAngle(PlayerRef,999,angle));
  p.SetScriptPlayerHeading(7.85398163397f);assert(p.ScriptHeadingAngle(PlayerRef,202,angle)&&std::fabs(angle)<.001f);
  auto actor=p.Snapshot().actors.at(100);actor.yaw=1.57079632679f;assert(p.UpdateActor(100,actor));
  assert(p.ScriptHeadingAngle(100,201,angle)&&std::fabs(angle-90)<.001f);
  assert(p.ScriptHeadingAngle(200,201,angle)&&angle==0);
  p.SetScriptPlayerHeading(0);std::string error;
  auto plan=p.PrepareReferenceEventTransaction(100,"OnTrigger",PlayerRef,error);Check(bool(plan),error);
  assert(!p.Snapshot().pipboy.dialogueVariables.count(uint64_t(100)<<32|1));
  p.SetScriptPlayerHeading(1);assert(!p.CanCommitScriptTransaction(*plan));p.SetScriptPlayerHeading(0);
  assert(p.CommitScriptTransaction(*plan)&&std::fabs(p.Snapshot().pipboy.dialogueVariables.at(uint64_t(100)<<32|1)-90)<.001f);
  p.SetScriptPlayerHeading(NAN);assert(!p.ScriptHeadingAngle(PlayerRef,200,angle));
  p.SetScriptPlayerHeading(0);const auto path="/tmp/fq-heading-"+std::to_string(getpid());Check(p.Save(path,error),error);Check(p.Restore(path,error),error);unlink(path.c_str());
  assert(!p.ScriptHeadingAngle(PlayerRef,200,angle));
}
static void TagSkills(){
  using namespace fo3player;Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);c.initial.skills.fill(90);c.pipboy.quests[101]={};
  Player p(c);std::string error;fo3pipdata::ResultScript script;
  script.source="SetPlayerTagSkill Barter 0\nSetPlayerTagSkill Science 1";Check(p.ExecuteDialogueResult(script,error,101),error);
  assert(p.IsPlayerTagSkill(32)&&p.IsPlayerTagSkill(40)&&p.Snapshot().skills[0]==100&&p.Snapshot().tagBonuses[0]==10);
  const auto revision=p.Revision();assert(p.SetPlayerTagSkill(32,0)&&p.Revision()==revision);
  assert(p.SetPlayerTagSkill(40,0)&&p.Snapshot().skills[0]==90&&p.Snapshot().skills[8]==100); // Duplicate slots do not stack.
  assert(!p.SetPlayerTagSkill(44,0)&&!p.SetPlayerTagSkill(40,4)&&!p.SetPlayerTagSkill(16,0));
  script.source="SetPlayerTagSkill Medicine 2\nUnsupportedTagCommand";assert(!p.ExecuteDialogueResult(script,error,101)&&!p.IsPlayerTagSkill(37));
  script.source="SetTagSkills 3 0";Check(p.ExecuteDialogueResult(script,error,101),error);
  assert(p.Snapshot().tagMenuOwner==101&&p.TagSkillsRemaining()==3&&p.IsPlayerTagSkill(40)&&!p.ConfirmTagSkills());
  assert(!p.OpenPlayerNameMenu(101)&&!p.OpenSpecialBook(101,40)&&!p.ToggleTagSkill(12));
  fo3tagmenu::Model menu;menu.Step(p,0,0,true,false,true);menu.Step(p,.1,0,true,false,true);assert(p.TagSkillsRemaining()==3);
  menu.Step(p,.2,0,false,false,true);menu.Step(p,.3,0,true,false,true);assert(p.TagSkillsRemaining()==2);
  const auto save="/tmp/fq-tags-"+std::to_string(getpid());Check(p.Save(save,error),error);Player resumed(c);Check(resumed.Restore(save,error),error);unlink(save.c_str());
  assert(resumed.Snapshot().taggedSkills==p.Snapshot().taggedSkills&&resumed.Snapshot().tagDraft==p.Snapshot().tagDraft&&resumed.Snapshot().tagMenuOwner==101&&resumed.Snapshot().skills==p.Snapshot().skills);
  for(unsigned i:{1u,2u}){menu.Step(p,.4+i,0,false,false,true);menu.cursor.selected=i;menu.Step(p,.5+i,0,true,false,true);}
  assert(p.TagSkillsRemaining()==0&&!p.ToggleTagSkill(3));menu.Step(p,4,0,false,false,true);menu.cursor.selected=13;
  assert(menu.Step(p,4.1,0,true,false,true)==fo3tagmenu::Model::Result::Confirmed);
  assert(!p.Snapshot().tagMenuOwner&&p.IsPlayerTagSkill(32)&&p.IsPlayerTagSkill(33)&&p.IsPlayerTagSkill(34)&&!p.IsPlayerTagSkill(40)&&p.Snapshot().skills[8]==90);
  assert(p.OpenTagSkills(101,3,true)&&p.TagSkillsRemaining()==0&&p.ToggleTagSkill(0)&&p.ToggleTagSkill(8)&&p.ConfirmTagSkills());
  assert(p.Snapshot().skills[0]==90&&p.Snapshot().skills[8]==100);Check(p.Save(save,error),error);Check(resumed.Restore(save,error),error);unlink(save.c_str());
  assert(resumed.Snapshot().tagBonuses==p.Snapshot().tagBonuses&&resumed.Snapshot().taggedSkills==p.Snapshot().taggedSkills);
  assert(!resumed.OpenTagSkills(101,5,false)&&!resumed.OpenTagSkills(999,3,false));
}
static void ScriptMathAndPrompts(){
  using namespace fo3player;Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);c.pipboy.formNames["exam"]=101;c.pipboy.quests[101].script=102;c.pipboy.quests[101].scriptDelay=.01f;
  auto& source=c.pipboy.scripts[102];source.variables={{1,"value"},{2,"answer"}};source.source="float value\nshort answer\nBegin GameMode\nset answer to GetButtonPressed\nEnd";c.eventPrograms[102]=fo3script::ParseEvents(source);
  c.pipboy.messages[200]={"Question","Value %.0f",{"One","Two"},1};Player p(c);std::string error;fo3pipdata::ResultScript math;
  math.source="set Exam.value to (2 + 3) * 4 / 2";Check(p.ExecuteDialogueResult(math,error,101),error);const auto key=fo3dialogue::VariableKey(c.pipboy,"Exam.value");assert(p.Snapshot().pipboy.dialogueVariables.at(key)==10);
  math.source="set Exam.value to GetRandomPercent / 100 + .2";auto first=p.PrepareDialogueTransaction(math,{},error,101),second=p.PrepareDialogueTransaction(math,{},error,101);Check(bool(first)&&bool(second),error);
  auto captured=p.PrepareDialogueTransaction(math,{},error,101,ScriptProgramCapture::EnteredPrograms);Check(bool(captured),error);
  assert(captured->CandidateState()->scriptRandom==first->CandidateState()->scriptRandom); // Provenance preflight cannot consume a draw.
  assert(!p.Snapshot().scriptRandom&&first->CandidateState()->scriptRandom==second->CandidateState()->scriptRandom&&first->CandidateState()->pipboy.dialogueVariables.at(key)>=.2f&&first->CandidateState()->pipboy.dialogueVariables.at(key)<=1.2f);
  assert(p.CommitScriptTransaction(*first)&&!p.CanCommitScriptTransaction(*second));const auto seed=p.Snapshot().scriptRandom;
  math.source="set Exam.value to GetRandomPercent\nset Exam.value to 1 / 0";assert(!p.PrepareDialogueTransaction(math,{},error,101)&&p.Snapshot().scriptRandom==seed);
  const auto path="/tmp/fq-prompts-"+std::to_string(getpid());Check(p.StartQuest(101),"prompt owner");
  Check(p.QueueScriptMessage(200,12,true,101),"formatted prompt");ScriptMessage message;assert(p.PollScriptMessage(message)&&message.text=="Value 12");
  Check(p.Save(path,error),error);Player restored(c);Check(restored.Restore(path,error),error);assert(restored.PollScriptMessage(message)&&message.form==200&&message.text=="Value 12"&&message.owner==101&&!restored.PollScriptMessage(message));
  assert(p.DrawScriptRandomPercent()==restored.DrawScriptRandomPercent());
  assert(!restored.SubmitScriptMessageButton(101,2)&&restored.SubmitScriptMessageButton(101,1));Check(restored.Save(path,error),error);Check(restored.Restore(path,error),error);unlink(path.c_str());
  assert(!restored.PollScriptMessage(message));restored.PumpWorldAwareGameMode(.3,{},[&](auto plan){return restored.CommitScriptTransaction(*plan);});
  assert(restored.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(c.pipboy,"Exam.answer"))==1);
  restored.PumpWorldAwareGameMode(.3,{},[&](auto plan){return restored.CommitScriptTransaction(*plan);});assert(restored.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(c.pipboy,"Exam.answer"))==-1);
}
static void NestedActivation(){
  using namespace fo3player;Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);
  c.references[100].base=10;c.actorPlacements[100].base=10;c.pipboy.targets[100].base=10;c.weapons.actors[10].health=20;
  c.references[200].base=20;c.pipboy.targets[200].base=20;c.pipboy.formNames={{"actor",100},{"target",200},{"testquest",101}};c.pipboy.quests[101].script=301;c.pipboy.scripts[301].variables[1]="after";
  c.eventBaseScripts[20]=300;c.pipboy.objectReferenceScripts[200]={300,20};c.pipboy.scripts[300].variables[1]="count";
  auto& source=c.pipboy.scripts[300];source.source="short count\nBegin OnActivate\nif IsActionRef Actor == 1\nset count to count + 1\nendif\nEnd";c.eventPrograms[300]=fo3script::ParseEvents(source);
  Player p(c);p.RegisterOriginalActors();std::string error;fo3pipdata::ResultScript call;call.source="Target.Activate Actor 1\nset TestQuest.after to Target.count";
  auto plan=p.PrepareDialogueTransaction(call,{},error,100,ScriptProgramCapture::EnteredPrograms);Check(bool(plan),error);assert(p.Snapshot().pipboy.dialogueVariables.empty());Check(p.CommitScriptTransaction(*plan),error);
  assert(p.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(c.pipboy,"TestQuest.after"))==1);
  assert(std::any_of(plan->ExecutedScripts().begin(),plan->ExecutedScripts().end(),[](const auto& script){return script.owner==200&&script.origin.kind==ScriptProgramKind::ReferenceEvent&&script.origin.record==300&&script.parent!=ScriptExecution::NoParent;}));
  source.source="short count\nBegin OnActivate\nset count to count + 1\nTarget.Activate Actor 1\nEnd";c.eventPrograms[300]=fo3script::ParseEvents(source);Player recursive(c);recursive.RegisterOriginalActors();
  assert(!recursive.PrepareDialogueTransaction(call,{},error,100)&&error.find("depth")!=std::string::npos&&recursive.Snapshot().pipboy.dialogueVariables.empty());
}
static void ActorFurniture(){
  using namespace fo3player;Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);
  c.references[100].base=10;c.actorPlacements[100].base=10;c.weapons.actors[10].health=20;c.pipboy.targets[100].base=10;
  c.references[200].base=20;c.pipboy.targets[200].base=20;c.pipboy.furniture[20].markers=0x40000001;
  c.pipboy.formNames["actor"]=100;c.pipboy.formNames["seat"]=200;
  c.eventBaseScripts[10]=300;c.pipboy.objectReferenceScripts[100]={300,10};auto& src=c.pipboy.scripts[300];src.variables[1]="sitting";
  src.source="short sitting\nBegin OnActivate\nset sitting to actor.GetSitting\nEnd";c.eventPrograms[300]=fo3script::ParseEvents(src);
  Player p(c);p.RegisterOriginalActors();uint32_t ref;uint8_t marker,state;std::string error;
  assert(!p.ScriptFurniture(100,ref,marker,state));p.SetScriptActorFurniture(100,0,0,0);assert(p.ScriptFurniture(100,ref,marker,state)&&state==0);
  p.SetScriptActorFurniture(100,200,0,3);assert(p.ScriptFurniture(100,ref,marker,state)&&state==3&&ref==200);
  auto plan=p.PrepareReferenceEventTransaction(100,"OnActivate",PlayerRef,error);Check(bool(plan),error);p.SetScriptActorFurniture(100,200,0,4);assert(!p.CanCommitScriptTransaction(*plan));
  plan=p.PrepareReferenceEventTransaction(100,"OnActivate",PlayerRef,error);Check(bool(plan)&&p.CommitScriptTransaction(*plan),error);assert(p.Snapshot().pipboy.dialogueVariables.at((uint64_t(100)<<32)|1)==4);
  fo3dialogue::Context ctx;ctx.player=&p;ctx.speaker.reference=100;fo3pipdata::Condition sitting;sitting.function=159;sitting.value=4;Check(fo3dialogue::Conditions({sitting},ctx,error),error);
  p.SetScriptActorFurniture(100,0,0,255);assert(!p.ScriptFurniture(100,ref,marker,state));p.SetFurnitureUse(200,0,3);assert(p.ScriptFurniture(PlayerRef,ref,marker,state)&&state==3);
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
static void ScriptInventory(){
  using namespace fo3player;
  Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);
  for(uint32_t id:{11,12,13,14,15}){auto& item=c.items[id];item.formId=id;item.name="Item";item.kind=ItemKind::Armour;item.bipedMask=4;}
  c.items[11].playable=false;c.items[11].bipedMask=64; // unplayable Pip-Boy equivalent
  c.items[12].questItem=true;c.items[14].kind=ItemKind::Misc;
  c.items[15].kind=ItemKind::Weapon;c.items[15].maxCondition=100;
  c.pipboy.formNames={{"wrist",11},{"questitem",12},{"suit",13},{"gift",14},{"gun",15},{"dresser",101},{"dad",102}};
  c.references[101].base=201;c.containers[201].valid=true;
  c.references[102].base=202;c.actorInventories[202].valid=true;c.weapons.actors[202]={};
  c.pipboy.transmitters[300]={};c.globals[401]=2;c.pipboy.formNames["amount"]=401;
  Player p(c);std::string error;
  const auto run=[&](const std::string& source){fo3pipdata::ResultScript result;result.source=source;const bool ok=p.ExecuteDialogueResult(result,error);Check(ok,source+"\n"+error);};
  run("player.AddItem Wrist 1 1\nplayer.EquipItem Wrist 0 1\nplayer.AddItem QuestItem 1 1\nplayer.AddItem Suit 2 1\nplayer.EquipItem Suit 0 1\nplayer.AddItem Gift 3 1\nplayer.AddItem Gun 1 1\nplayer.EquipItem Gun");
  assert(p.ItemCount(11)==1&&p.ItemCount(13)==2&&p.EquippedWeapon());
  ScriptItemNotice notice;assert(!p.PollScriptItemNotice(notice));
  assert(!p.Equip(p.Snapshot().inventory.front().id)); // UI still rejects unplayable equipment.
  assert(p.TuneRadio(300));run("PipBoyRadioOff");assert(!p.Snapshot().pipboy.tunedRadio);
  // Equipped quest/unplayable equipment must survive a regular save and restore.
  const auto save="/tmp/fq-script-inventory-"+std::to_string(getpid());Check(p.Save(save,error),error);
  Player restored(c);Check(restored.Restore(save,error),error);unlink(save.c_str());
  assert(restored.ItemCount(11)==1&&restored.EquippedWeapon());
  run("player.RemoveItem Suit 1");assert(p.ItemCount(13)==1);
  for(const auto& stack:p.Snapshot().inventory)if(stack.formId==13)assert(stack.equipped);
  run("player.RemoveItem Suit 1");assert(!p.ItemCount(13));
  run("player.RemoveAllItems");assert(p.ItemCount(11)==1&&p.ItemCount(12)==1&&!p.ItemCount(14)&&!p.EquippedWeapon());
  run("Dresser.AddItem Gift 1 1\nDad.AddItem Gift 2\nDad.RemoveItem Gift 1");
  assert(p.ContainerContents(101)->size()==1&&p.ContainerContents(101)->front().count==1);
  assert(p.ContainerContents(102)->size()==1&&p.ContainerContents(102)->front().count==1);
  assert(!p.PollScriptItemNotice(notice));
  run("Dad.AddItemHealthPercent Gun Amount .25\nplayer.AddItemHealthPercent Gun 1 .9 1");
  bool quarter=false;for(const auto& stack:*p.ContainerContents(102))if(stack.formId==15){assert(stack.condition==.25f);quarter=true;}assert(quarter);
  for(const auto& stack:p.Snapshot().inventory)if(stack.formId==15)assert(std::abs(stack.condition-.9f)<.001f);
  run("player.UnequipItem Gun 0 1\nplayer.RemoveItem Gun 100\nDad.RemoveItem Gift 100\nplayer.AddItem Gift 0");
  assert(!p.ItemCount(15));
  run("player.AddItem Gift 1");assert(p.PollScriptItemNotice(notice)&&notice.form==14&&notice.count==1);
  // Destructive operations, equipment and save requests remain private until commit.
  fo3pipdata::ResultScript begin,end;begin.source="player.RemoveAllItems\nplayer.AddItem Suit 1 1\nplayer.EquipItem Suit 0 1\nautosave";
  const auto before=p.Revision();auto plan=p.PrepareDialogueTransaction(begin,end,error);Check(bool(plan),error);
  assert(p.Revision()==before&&p.ItemCount(14)==1&&!p.ItemCount(13));
  fo3script::Action action;assert(!p.PollScriptAction(action));
  assert(p.CommitScriptTransaction(*plan)&&p.ItemCount(13)==1&&!p.ItemCount(14));
  assert(p.RollbackScriptTransaction(*plan)&&p.ItemCount(14)==1&&!p.ItemCount(13)&&!p.PollScriptAction(action));
  end.source="UnknownOperation";assert(!p.PrepareDialogueTransaction(begin,end,error));assert(p.ItemCount(14)==1&&!p.ItemCount(13));
  for(const char* source:{"player.EquipItem Suit 1", "player.UnequipItem Suit 1", "player.RemoveAllItems Dresser", "player.AddItem Gift 1 2"}){
    fo3pipdata::ResultScript invalid;invalid.source=source;assert(!p.ExecuteDialogueResult(invalid,error));
  }
}
static void CalendarAssignments(){
  using namespace fo3player;Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);
  c.yearGlobal=701;c.monthGlobal=702;c.dayGlobal=703;c.hourGlobal=704;c.daysPassedGlobal=705;c.timeScaleGlobal=706;
  c.pipboy.formNames={{"gameyear",701},{"gamemonth",702},{"gameday",703},{"gamehour",704}};
  for(uint32_t id=701;id<=706;++id)c.globals[id]=0;
  Player p(c);fo3pipdata::ResultScript change,bad;std::string error;
  change.source="set GameYear to 2268\nset GameMonth to 6\nset GameDay to 13\nset GameHour to 16";bad.source="UnsupportedCalendarStep";
  assert(!p.PrepareDialogueTransaction(change,bad,error)&&p.Snapshot().gameYear==2277);
  Check(p.ExecuteDialogueResult(change,error),error);
  assert(p.Snapshot().gameYear==2268&&p.Snapshot().gameMonth==6&&p.Snapshot().gameDay==13&&p.Snapshot().gameHour==16&&p.Snapshot().pipboy.mutableGlobals.empty());
  float value=0;assert(p.GlobalValue(701,value)&&value==2268&&p.GlobalValue(704,value)&&value==16);
  assert(!p.SetGlobalValue(701,0)&&!p.SetGlobalValue(702,12)&&!p.SetGlobalValue(703,32)&&!p.SetGlobalValue(704,24)&&!p.SetGlobalValue(704,NAN)&&!p.SetGlobalValue(705,1)&&!p.SetGlobalValue(706,1));
  assert(p.SetGlobalValue(703,29)&&p.SetGlobalValue(702,1)&&!p.SetGlobalValue(701,2269));
  assert(p.SetGlobalValue(703,28)&&p.SetGlobalValue(701,2269));
  const auto path="/tmp/fq-script-calendar-"+std::to_string(getpid());Check(p.Save(path,error),error);Player restored(c);Check(restored.Restore(path,error),error);unlink(path.c_str());
  assert(restored.Snapshot().gameYear==2269&&restored.Snapshot().gameMonth==1&&restored.Snapshot().gameDay==28&&restored.Snapshot().gameHour==16);
}
static void TalkingActivators(){
  using namespace fo3player;Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);
  c.references[100].base=200;c.references[100].valid=true;c.pipboy.targets[100].base=200;c.pipboy.targets[100].cell=1;c.pipboy.targets[100].x=10;c.pipboy.dialogueActors[200].talkingActivator=true;
  c.references[101].base=201;c.references[101].valid=true;c.actorPlacements[101].base=201;c.weapons.actors[201].health=20;c.pipboy.targets[101].base=201;c.pipboy.targets[101].cell=2;
  c.pipboy.formNames["intercom"]=100;c.pipboy.formNames["speaker"]=101;
  Player p(c);std::string error;fo3pipdata::ResultScript set,bad;set.source="Intercom.SetTalkingActivatorActor Speaker";bad.source="UnsupportedProxyStep";
  Check(p.PreviewDialogueResult(set,error),error);assert(!p.TalkingActivatorActor(100));
  assert(!p.PrepareDialogueTransaction(set,bad,error)&&!p.TalkingActivatorActor(100));
  Check(p.ExecuteDialogueResult(set,error),error);assert(p.TalkingActivatorActor(100)==101);
  fo3dialogue::ActorContext ctx;Check(fo3dialogue::ReferenceContext(p,100,ctx),"mapped context");assert(ctx.reference==101&&ctx.proxy==100&&ctx.base==201&&ctx.cell==1&&ctx.x==10);
  const auto path="/tmp/fq-talking-activator-"+std::to_string(getpid());Check(p.Save(path,error),error);Player restored(c);Check(restored.Restore(path,error),error);unlink(path.c_str());assert(restored.TalkingActivatorActor(100)==101);
  set.source="SetTalkingActivatorActor";Check(restored.ExecuteDialogueResult(set,error,100),error);assert(!restored.TalkingActivatorActor(100));
  set.source="Speaker.SetTalkingActivatorActor Intercom";assert(!p.ExecuteDialogueResult(set,error));
}
static void CrimeFlags(){
  using namespace fo3player;Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);
  c.actorPlacements[1000].base=10;c.actorPlacements[1001].base=10;c.references[1000].base=10;c.references[1001].base=10;
  c.pipboy.targets[1000].base=10;c.pipboy.targets[1001].base=10;c.pipboy.formNames["dad"]=1000;
  Player p(c);bool flag=true;assert(p.GetIgnoreCrime(1000,flag)&&!flag&&!p.SetIgnoreCrime(9999,true));
  std::string error;fo3pipdata::ResultScript change;change.source="Dad.IgnoreCrime 1\nif Dad.GetIgnoreCrime == 1\nRewardKarma 2\nendif";
  Check(p.ExecuteDialogueResult(change,error),error);assert(p.GetIgnoreCrime(1000,flag)&&flag&&p.Snapshot().karma==2);
  assert(p.GetIgnoreCrime(1001,flag)&&!flag);const auto revision=p.Revision();assert(p.SetIgnoreCrime(1000,true)&&p.Revision()==revision);
  fo3dialogue::Context ctx;ctx.player=&p;ctx.speaker.reference=1000;ctx.speaker.base=10;
  fo3pipdata::Condition condition;condition.function=192;condition.value=1;Check(fo3dialogue::Conditions({condition},ctx,error),error);
  fo3pipdata::ResultScript bad;change.source="Dad.IgnoreCrime 0";bad.source="UnsupportedCrimeChange";
  assert(!p.PrepareDialogueTransaction(change,bad,error)&&p.GetIgnoreCrime(1000,flag)&&flag);
  const auto path="/tmp/fq-crime-flags-"+std::to_string(getpid());Check(p.Save(path,error),error);Player saved(c);Check(saved.Restore(path,error),error);unlink(path.c_str());
  assert(saved.GetIgnoreCrime(1000,flag)&&flag&&saved.GetIgnoreCrime(1001,flag)&&!flag);
  Check(saved.ExecuteDialogueResult(change,error),error);assert(saved.GetIgnoreCrime(1000,flag)&&!flag);
  for(const char* invalid:{"Dad.IgnoreCrime 2","Dad.IgnoreCrime","UnknownActor.IgnoreCrime 1"}){change.source=invalid;assert(!saved.ExecuteDialogueResult(change,error));}
}
static void Lifecycle(){
  using namespace fo3player;Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);
  c.pipboy.quests[101].name="Intro";c.pipboy.quests[101].objectives[1].text="Objective";
  c.pipboy.dialogueActors[PlayerBase].race=100;c.pipboy.raceAges[100]={200,300};c.pipboy.raceAges[200]={0,100};c.pipboy.raceAges[300]={100,0};
  c.pipboy.raceHeights[100]={1,1};c.pipboy.raceHeights[200]={.8f,.8f};c.pipboy.raceBodyModels[200][0][0]="original/child/body.nif";
  Player p(c);std::string error;fo3pipdata::ResultScript result;
  result.source="player.SetScale .4\nSetPCToddler 1\nAutoDisplayObjectives 1\nPlayBink \"1 year later.bik\" 0 0 1 0";
  Check(p.ExecuteDialogueResult(result,error,101),error);
  assert(p.Snapshot().playerToddler&&p.Snapshot().autoDisplayObjectives&&p.Snapshot().pipboy.selectedQuest==101&&p.PlayerTrackingScale()==.4f);
  assert(p.Snapshot().movie.path=="1 year later.bik"&&p.Snapshot().movie.flags==4&&p.Snapshot().movie.owner==101);
  assert(!p.OpenPlayerNameMenu(101)&&!p.OpenSpecialBook(101,40)&&!p.DeferAppearanceMenu(101));
  const auto path="/tmp/fq-lifecycle-"+std::to_string(getpid());Check(p.Save(path,error),error);Player restored(c);Check(restored.Restore(path,error),error);unlink(path.c_str());
  assert(restored.Snapshot().playerToddler&&restored.Snapshot().movie.path=="1 year later.bik"&&restored.PlayerTrackingScale()==.4f);
  assert(!restored.CompleteMovie(restored.Snapshot().movie.serial+1));assert(restored.CompleteMovie(restored.Snapshot().movie.serial));
  assert(restored.AgePlayerRace(-1)&&restored.PlayerRace()==200&&restored.PlayerBodyModel(0)=="original/child/body.nif");
  assert(std::abs(restored.PlayerTrackingScale()-.32f)<.0001f&&restored.AgePlayerRace(1)&&restored.PlayerRace()==100);
  assert(!restored.AgePlayerRace(-3)&&restored.PlayerRace()==100);
  assert(!restored.SetPlayerScale(0)&&!restored.SetPlayerScale(INFINITY));
  result.source="player.SetScale 1\nSetPCToddler 0\nAutoDisplayObjectives 0";Check(restored.ExecuteDialogueResult(result,error,101),error);
  assert(restored.PlayerTrackingScale()==1&&!restored.Snapshot().playerToddler&&!restored.Snapshot().autoDisplayObjectives);
  const auto local=fo3scale::Local({.2f,0,.3f},.4f,-1.55f);assert(std::abs(local[1]+.93f)<.0001f);
  const auto floor=fo3scale::Local({0,-1.55f,0},.4f,-1.55f);assert(floor[1]==-1.55f);
  const fo3vr::V hmd{local[0],local[1],local[2]};const auto root=fo3vr::BodyRoot({0,0,0},hmd,0);
  const auto feet=root.Point({0,-1.55f*.4f,0});assert(std::abs(feet.y+1.55f)<.0001f);
  assert(p.SetObjective(101,1,true,fo3pipdata::Completion::Active));
  fo3pipdata::ResultScript kill,broken;kill.source="KillQuestUpdates";broken.source="Unsupported";
  assert(!p.PrepareDialogueTransaction(kill,broken,error,101));QuestNotice notice;assert(p.PollQuestNotice(notice));
  assert(p.SetObjective(101,1,false,fo3pipdata::Completion::Active)&&p.SetObjective(101,1,true,fo3pipdata::Completion::Active));
  Check(p.ExecuteDialogueResult(kill,error,101),error);assert(!p.PollQuestNotice(notice));
  fo3script::Action action;bool cleared=false;while(p.PollScriptAction(action))cleared|=action.kind==fo3script::ActionKind::KillQuestUpdates;assert(cleared);
}
static void CharacterMenus(){
  using namespace fo3player;
  Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);c.initial.skills.fill(15);c.rules.healthEnduranceMult=10;c.rules.apBase=50;c.rules.apMult=10;
  c.pipboy.quests[101].script=201;c.pipboy.quests[101].flags=1;c.pipboy.quests[101].scriptDelay=.01f;
  auto& script=c.pipboy.scripts[201];script.variables={{1,"frames"},{2,"menus"}};
  script.source="short frames\nshort menus\nBegin GameMode\nif frames == 0\nShowRaceMenu\nendif\nset frames to frames + 1\nEnd\nBegin MenuMode\nset menus to menus + 1\nEnd";
  c.eventPrograms[201]=fo3script::ParseEvents(script);
  c.pipboy.quests[102].script=202;c.pipboy.quests[102].flags=1;c.pipboy.quests[102].scriptDelay=.01f;
  c.pipboy.scripts[202].variables={{1,"ticks"}};c.pipboy.scripts[202].source="short ticks\nBegin GameMode\nset ticks to ticks + 1\nEnd";
  c.eventPrograms[202]=fo3script::ParseEvents(c.pipboy.scripts[202]);
  Player p(c);auto reserve=[&](auto plan){return p.CommitScriptTransaction(*plan);};
  p.PumpWorldAwareGameMode(.1,{},reserve);assert(p.Snapshot().deferredAppearanceOwner==101);
  assert(!p.Snapshot().pipboy.dialogueVariables.count((uint64_t(102)<<32)|1));
  const auto save="/tmp/fq-character-menus-"+std::to_string(getpid());std::string error;Check(p.Save(save,error),error);
  Player restored(c);Check(restored.Restore(save,error),error);assert(restored.Snapshot().deferredAppearanceOwner==101);
  restored.PumpWorldAwareGameMode(.1,{},[&](auto plan){return restored.CommitScriptTransaction(*plan);});
  assert(!restored.Snapshot().deferredAppearanceOwner&&restored.Snapshot().pipboy.dialogueVariables.at((uint64_t(101)<<32)|2)==1);
  assert(!restored.Snapshot().pipboy.dialogueVariables.count((uint64_t(102)<<32)|1));
  restored.PumpWorldAwareGameMode(.1,{},[&](auto plan){return restored.CommitScriptTransaction(*plan);});
  assert(restored.Snapshot().pipboy.dialogueVariables.at((uint64_t(102)<<32)|1)==1);
  Player book(c);fo3pipdata::ResultScript request;request.source="ssbmp 40";
  Check(book.ExecuteDialogueResult(request,error,101),error);assert(book.SpecialBookRemaining()==5&&!book.ConfirmSpecialBook());
  assert(!book.OpenPlayerNameMenu(101)&&!book.DeferAppearanceMenu(101));
  assert(book.AdjustSpecialBook(0,1)&&book.AdjustSpecialBook(0,1)&&book.AdjustSpecialBook(2,1)&&book.AdjustSpecialBook(5,1)&&book.AdjustSpecialBook(6,1));
  assert(book.SpecialBookRemaining()==0&&!book.AdjustSpecialBook(0,1)&&book.Snapshot().special[0]==5);
  Check(book.Save(save,error),error);Player resumed(c);Check(resumed.Restore(save,error),error);unlink(save.c_str());
  assert(resumed.Snapshot().specialDraft[0]==7&&resumed.Snapshot().specialMenuOwner==101);
  fo3special::Model controls;controls.Step(resumed,0,0,0,true,false,true);assert(resumed.Snapshot().specialMenuOwner);
  controls.Step(resumed,.1,0,0,false,false,true);controls.Step(resumed,.2,0,0,false,false,true);controls.cursor.selected=7;
  controls.Step(resumed,.3,0,0,true,false,false);controls.Step(resumed,.4,0,0,true,false,true);assert(resumed.Snapshot().specialMenuOwner);
  controls.Step(resumed,.5,0,0,false,false,true);controls.Step(resumed,.6,0,0,false,false,true);
  assert(controls.Step(resumed,.7,0,0,true,false,true)==fo3special::Model::Result::Confirmed);
  assert(!resumed.Snapshot().specialMenuOwner&&resumed.Snapshot().special[0]==7&&resumed.Snapshot().skills[6]==19&&resumed.Snapshot().skills[9]==17);
  assert(resumed.MaxHealth()==160&&resumed.MaxActionPoints()==110);
  // A menu request is part of the same transaction as the triggering script.
  Player atomic(c);fo3pipdata::ResultScript end;end.source="UnknownCommand";
  assert(!atomic.PrepareDialogueTransaction(request,end,error,101)&&!atomic.Snapshot().specialMenuOwner);
}
static void OriginalSpecialBook(const fo3player::Catalog& catalog){
  using namespace fo3player;
  const auto quest=catalog.pipboy.formNames.at("cg01"),script=catalog.pipboy.formNames.at("cg01specialbookscript");
  uint32_t reference=0;
  for(const auto& ref:catalog.references){const auto found=catalog.eventBaseScripts.find(ref.second.base);if(found!=catalog.eventBaseScripts.end()&&found->second==script){reference=ref.first;break;}}
  assert(reference);
  auto fixture=catalog;fixture.initial.pipboy.quests[quest].stage=30;fixture.initial.pipboy.quests[quest].running=true;
  Player p(std::move(fixture));std::string error;
  auto plan=p.PrepareReferenceEventTransaction(reference,"OnActivate",PlayerRef,error);Check(bool(plan),error);
  // The fixture starts with access to the book; no runtime stage shortcut exists.
  assert(!p.Snapshot().specialMenuOwner);Check(p.CommitScriptTransaction(*plan),error);
  assert(p.Snapshot().specialMenuOwner==reference&&p.Snapshot().specialMenuTotal==40&&p.Snapshot().pipboy.quests.at(quest).stage==50);
  while(p.SpecialBookRemaining()>0)for(unsigned i=0;i<7&&p.SpecialBookRemaining()>0;++i)if(p.Snapshot().specialDraft[i]<10)assert(p.AdjustSpecialBook(i,1));
  assert(p.ConfirmSpecialBook()&&p.Snapshot().pipboy.quests.at(quest).stage==50);
  auto repeated=p.PrepareReferenceEventTransaction(reference,"OnActivate",PlayerRef,error);Check(bool(repeated)&&p.CommitScriptTransaction(*repeated),error);
  assert(!p.Snapshot().specialMenuOwner);
}
static void OriginalInventory(const fo3player::Catalog& catalog){
  using namespace fo3player;
  Player p(catalog);p.RegisterOriginalActors();std::string error;
  const auto& d=catalog.pipboy;const auto id=[&](const char* name){return d.formNames.at(name);};
  // Execute unchanged, contiguous inventory sections from the original stage
  // results. These are subsystem tests; they do not claim to play the stages.
  const auto section=[&](const char* quest,uint16_t stage,const char* first,const char* last){
    for(const auto& item:d.quests.at(id(quest)).stages.at(stage).items){
      auto source=fo3script::Lower(item.result.source);const auto begin=source.find(first);
      if(begin==std::string::npos)continue;
      const auto end=source.find(last,begin);assert(end!=std::string::npos);
      const auto newline=source.find('\n',end);fo3pipdata::ResultScript original;
      original.source=item.result.source.substr(begin,(newline==std::string::npos?source.size():newline)-begin);
      const bool ok=p.ExecuteDialogueResult(original,error,id(quest));Check(ok,original.source+"\n"+error);return;
    }assert(false&&"original inventory section unavailable");
  };
  const auto pipboy=id("pipboy");
  Player intro(catalog);const auto arrival=intro.PrepareNewGameTransaction(id("cg00"),error);Check(bool(arrival)&&intro.CommitScriptTransaction(*arrival),error);
  assert(!intro.BootstrapDevelopmentWeapon()&&!intro.Snapshot().developmentWeaponGranted);
  assert(intro.SetCharacterGeneration(false)); // Still protected after leaving chargen.
  assert(!intro.BootstrapDevelopmentWeapon());
  if(!p.ItemCount(pipboy))assert(p.Add(pipboy,1));
  section("cg02",5,"player.removeallitems","player.equipitem pipboy");
  assert(p.ItemCount(id("kidvaultsuitchild101"))==1&&p.ItemCount(pipboy)>=1);
  section("cg02",44,"player.additem cg02weapbbgun","player.equipitem cg02weapbbgun");
  assert(p.EquippedWeapon()&&p.EquippedWeapon()->formId==id("cg02weapbbgun"));
  section("cg02",95,"player.unequipitem cg02weapbbgun","player.unequipitem cg02weapbbgun");
  assert(!p.EquippedWeapon());
  section("cg02",70,"setquestobject cg02weapbbgun","setquestobject cg02weapbbgun");
  section("cg02",100,"player.removeallitems","player.equipitem vaultsuit101");
  assert(!p.ItemCount(id("cg02weapbbgun"))&&!p.ItemCount(id("kidvaultsuitchild101"))&&p.ItemCount(id("vaultsuit101"))==1&&p.ItemCount(pipboy)>=1);
  section("cg03",5,"player.removeallitems","player.equipitem pipboy");
  assert(p.ItemCount(id("vaultsuit101"))==1);
  section("cg04",8,"cg04amataref.removeitem","player.additem ammo10mm");
  assert(p.ItemCount(id("weap10mmpistol"))==1&&p.ItemCount(id("ammo10mm"))==36);
  section("cg04",50,"cg04amataref.removeitem","player.additem ammo10mm");
  bool damaged=false;for(const auto& stack:p.Snapshot().inventory)if(stack.formId==id("weap10mmpistol")&&std::abs(stack.condition-.9f)<.001f)damaged=true;
  assert(damaged);
  const auto save="/tmp/fq-vault-handoffs-"+std::to_string(getpid());Check(p.Save(save,error),error);
  Player restored(catalog);Check(restored.Restore(save,error),error);unlink(save.c_str());
  assert(restored.ItemCount(id("vaultsuit101"))==1&&restored.ItemCount(pipboy)>=1&&restored.ItemCount(id("ammo10mm"))==66);
}
static void OriginalExam(const fo3player::Catalog& original,const char* master,bool skip){
  auto catalog=original;const auto& d=catalog.pipboy;const auto quest=d.formNames.at("cg03"),teacher=d.formNames.at("cg03mrbrotchref"),butch=d.formNames.at("cg03butchref");
  // Boundary fixture: the prior introduction and hallway conversation ended.
  // All subsequent stages/results come from original dialogue/events.
  auto& started=catalog.initial.pipboy.quests[quest];started.stage=30;started.running=true;started.stages={5,6,10,20,25,28,30};
  catalog.initial.referenceEnabled[d.formNames.at("cg03dadref")]=true;catalog.initial.inCharGen=true;
  fo3player::Player p(catalog);p.RegisterOriginalActors();std::string error;
  p.SetScriptEventDiagnostic([&](const std::string& e){if(e.find("DIALOGUE UNSUPPORTED SCRIPT")!=std::string::npos){std::cerr<<e<<"\n";return;}for(const char* script:{"cg03script","cg03mrbrotchscript","cg03butchscript"})if(e.find(std::to_string(d.formNames.at(script)))!=std::string::npos){std::cerr<<"EXAM "<<e<<"\n";break;}});
  const auto& placed=d.targets.at(teacher);p.SetScriptPlayerLocation(placed.x,placed.y+50,placed.z,placed.cell,placed.world);
  auto context=[&](uint32_t ref,uint32_t listener=fo3player::PlayerRef){fo3dialogue::Context ctx;ctx.player=&p;ctx.talking=true;Check(fo3dialogue::ReferenceContext(p,ref,ctx.speaker),"exam speaker");Check(fo3dialogue::ReferenceContext(p,listener,ctx.target),"exam listener");return ctx;};
  auto configure=[&](fo3dialogue::Session& voice){voice.previewReferences=[](const auto&,const auto&,auto&){return true;};voice.publishReferences=[](auto& player,auto& plan,auto&){return player.CommitScriptTransaction(plan);};voice.diagnostic=[&](const std::string& e){if(e.find("UNSUPPORTED SCRIPT")!=std::string::npos)std::cerr<<e<<"\n";error=e;};};
  auto finish=[&](fo3dialogue::Session& voice,const fo3dialogue::Context& ctx){unsigned lines=0;while(voice.phase==fo3dialogue::Phase::Speaking){assert(++lines<32);voice.AudioDone(voice.audioToken,true,ctx,p);}};
  auto choose=[&](fo3dialogue::Session& voice,const fo3dialogue::Context& ctx,const char* topic){const auto id=d.formNames.at(topic);auto option=std::find_if(voice.choices.begin(),voice.choices.end(),[&](const auto& c){return c.topic==id;});
    if(option==voice.choices.end()){std::cerr<<"Missing exam choice "<<topic<<" available:";for(const auto& c:voice.choices)std::cerr<<" "<<c.text;std::cerr<<"\n";}
    Check(option!=voice.choices.end(),"original exam choice");Check(voice.Choose(size_t(option-voice.choices.begin()),ctx,p),error);finish(voice,ctx);};
  auto ctx=context(teacher);fo3dialogue::Session talk;configure(talk);Check(talk.Start(ctx,p),"original exam greeting");finish(talk,ctx);
  if(skip){choose(talk,ctx,"cg03brotchstarta2");choose(talk,ctx,"cg03brotchstartb1");
    assert(p.Snapshot().pipboy.quests.at(quest).stages.count(45)&&p.Snapshot().tagMenuOwner==teacher&&p.TagSkillsRemaining()==3);
    for(unsigned skill:{0u,8u,11u})assert(p.ToggleTagSkill(skill));assert(p.ConfirmTagSkills());
  }else{
    choose(talk,ctx,"cg03brotchstarta3");assert(p.Snapshot().pipboy.quests.at(quest).stage==42);talk.End("presentation fixture");
    uint32_t trigger=0;const auto triggerScript=d.formNames.at("cg03classroomtriggerscript");for(const auto& ref:catalog.references){const auto script=catalog.eventBaseScripts.find(ref.second.base);if(script!=catalog.eventBaseScripts.end()&&script->second==triggerScript){trigger=ref.first;break;}}assert(trigger);
    const auto& at=d.targets.at(trigger);p.SetScriptPlayerLocation(at.x,at.y,at.z,at.cell,at.world);
    auto event=[&](){auto plan=p.PrepareReferenceEventTransaction(trigger,"OnTrigger",fo3player::PlayerRef,error,fo3player::ScriptProgramCapture::EnteredPrograms);Check(bool(plan),error);
      std::atomic<bool> cancel{false};fo3script::ScriptMasterMatch report;Check(fo3script::MatchScriptProgramsToMaster(master,plan->ExecutedScripts(),report,cancel,error),error);Check(p.CommitScriptTransaction(*plan),error);};
    std::vector<uint32_t> chairs;for(const auto& ref:d.targets)if(ref.second.cell==at.cell&&d.furniture.count(ref.second.base))chairs.push_back(ref.first);std::sort(chairs.begin(),chairs.end());assert(chairs.size()>=8);const auto chair=chairs.front();
    p.SetFurnitureUse(chair,0,3);event();assert(p.Snapshot().pipboy.quests.at(quest).stage==50);
    const auto package=d.formNames.at("cg03mrbrotchtoclassroom");
    for(bool done:{false,true}){auto plan=p.PreparePackageEventTransaction(teacher,package,done,error);Check(bool(plan)&&p.CommitScriptTransaction(*plan),error);}
    assert(p.Snapshot().pipboy.quests.at(quest).stage==55);
    const char* students[]={"cg03amataref","cg03butchref","cg03paulhannonref","cg03wallymackref","cg03freddiegomezref","cg03christinekendallref","cg03susiemackref"};
    for(auto name:students)p.SetScriptActorFurniture(d.formNames.at(name),0,0,0);
    event();assert(p.Snapshot().pipboy.quests.at(quest).stage==55);
    for(unsigned i=0;i<7;++i)p.SetScriptActorFurniture(d.formNames.at(students[i]),chairs[i+1],11,3);
    event();assert(p.Snapshot().pipboy.quests.at(quest).stage==60&&p.PlayerControlDisabled(0));
    const auto slides=d.formNames.at("cg03goatslidesref");auto registry=std::make_shared<fo3script::AnimationRegistry>();
    for(const char* group:{"Forward","Backward","FastForward","FastBackward","Left","Right","FastLeft","FastRight","TurnLeft","Equip"})(*registry)[slides].push_back({group,1,false});p.SetScriptAnimationRegistry(registry);
    unsigned answers=0,speechLines=0;bool checkedNested=false;std::unordered_set<uint32_t> messages;
    for(unsigned tick=0;tick<2000&&p.Snapshot().pipboy.quests.at(quest).stage<70;++tick){
      p.SetScriptRuntimeClock(2000+tick*.1,.1);p.PumpWorldAwareGameMode(.1,{teacher,butch},[&](auto plan){return p.CommitScriptTransaction(*plan);});
      fo3script::Action action;while(p.PollScriptAction(action)){if(action.kind!=fo3script::ActionKind::SayTo)continue;assert(++speechLines<100);
        if(!checkedNested&&action.info==0x34901){auto evidence=p.PrepareDialogueInfoTransaction(action.form,action.info,fo3player::DialogueScriptPhase::Both,error,action.reference,fo3player::ScriptProgramCapture::EnteredPrograms);Check(bool(evidence),error);
          std::atomic<bool> cancel{false};fo3script::ScriptMasterMatch report;Check(fo3script::MatchScriptProgramsToMaster(master,evidence->ExecutedScripts(),report,cancel,error),error);checkedNested=true;}
        auto speaking=context(action.reference,action.flags);fo3dialogue::Session line;configure(line);Check(line.StartSelectedTopic(action.form,action.info,speaking,p),"original exam speech: "+error);finish(line,speaking);
        const auto done=p.DispatchReferenceEvent(action.reference,"SayToDone",action.form,error);Check(done==fo3player::ScriptEventResult::Executed||done==fo3player::ScriptEventResult::NoHandler,error);
      }
      fo3player::ScriptMessage message;while(p.PollScriptMessage(message)){
        if(message.owner!=quest)continue;assert(message.buttons.size()==4&&messages.insert(message.form).second);
        const auto checkpoint="/tmp/fq-question-"+std::to_string(getpid());
        if(answers==2){Check(p.Save(checkpoint,error)&&p.Restore(checkpoint,error),error);unlink(checkpoint.c_str());fo3player::ScriptMessage replay;assert(p.PollScriptMessage(replay)&&replay.form==message.form&&replay.owner==message.owner&&replay.buttons==message.buttons);p.SetScriptAnimationRegistry(registry);}
        Check(p.SubmitScriptMessageButton(message.owner,0),"original exam answer");++answers;
        if(answers==4){Check(p.Save(checkpoint,error)&&p.Restore(checkpoint,error),error);unlink(checkpoint.c_str());p.SetScriptAnimationRegistry(registry);}
      }
    }
    if(p.Snapshot().pipboy.quests.at(quest).stage!=70||answers!=10||p.PlayerControlDisabled(0))std::cerr<<"Exam stopped stage="<<p.Snapshot().pipboy.quests.at(quest).stage<<" answers="<<answers<<" lines="<<speechLines<<" controls="<<unsigned(p.Snapshot().disabledControls)<<"\n";
    assert(p.Snapshot().pipboy.quests.at(quest).stage==70&&answers==10&&messages.size()==10&&!p.PlayerControlDisabled(0)&&checkedNested);
    assert(p.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(d,"CG03GoatSlidesREF.stage"))==10);
    assert(p.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(d,"CG03.question"))==-1&&fo3tags::Count(fo3tags::Mask(p.Snapshot().taggedSkills))==3);
    p.SetFurnitureUse(0,0,0);ctx=context(teacher);talk={};configure(talk);Check(talk.Start(ctx,p),"original test result greeting");finish(talk,ctx);choose(talk,ctx,"cg03brotcha1");
    assert(p.Snapshot().tagMenuOwner==teacher&&p.TagSkillsRemaining()==0&&p.ConfirmTagSkills()&&p.Snapshot().pipboy.quests.at(quest).stage==80);
  }
  const auto save="/tmp/fq-exam-"+std::to_string(getpid());Check(p.Save(save,error),error);fo3player::Player resumed(catalog);Check(resumed.Restore(save,error),error);unlink(save.c_str());
  assert(resumed.Snapshot().taggedSkills==p.Snapshot().taggedSkills&&resumed.Snapshot().tagBonuses==p.Snapshot().tagBonuses);
  std::cout<<"Original G.O.A.T. "<<(skip?"skip/selection":"ten questions/results")<<" path passed\n";
}
static void OriginalIdentity(const fo3player::Catalog& catalog,bool female,const char* master){
  fo3player::Player p(catalog);p.RegisterOriginalActors();std::string error;
  const auto& d=catalog.pipboy;const auto quest=d.formNames.at("cg00");
  auto boot=p.PrepareNewGameTransaction(quest,error);Check(bool(boot)&&p.CommitScriptTransaction(*boot),error);
  const auto start=p.Snapshot().playerMove;p.AcknowledgePlayerMove();
  assert(fo3player::EarlyFirstPerson(p));
  p.SetScriptPlayerLocation(start.position[0],start.position[1],start.position[2],start.cell,start.world);
  std::vector<uint32_t> residents={d.formNames.at("cg00dadref"),d.formNames.at("cg00momref"),d.formNames.at("cg00doctorliref"),d.formNames.at("cg01dadref")};
  auto registry=std::make_shared<fo3script::AnimationRegistry>();
  // Renderer-capability fixture, not a claim that the original NIF was loaded.
  (*registry)[d.formNames.at("cg00geneprojectorref")]={{"forward",1,false},{"backward",1,false}};p.SetScriptAnimationRegistry(registry);
  bool chose=false,genderLine=false,enteredName=false,sawMovie=false,sawBirthday=false,radioPrepared=false;const auto baby=d.formNames.at("cg01"),birthday=d.formNames.at("cg02");
  for(const char* name:{"cg02amataref","cg02beatriceref","cg02dadref","cg02oldladypalmerref","cg02stanleyref","cg02paulhannonref","cg02vault101security04ref","cg02overseerref"})residents.push_back(d.formNames.at(name));
  const auto scriptedReference=[&](const char* name){
    const auto script=d.formNames.at(name);
    for(const auto& ref:catalog.references){const auto found=catalog.eventBaseScripts.find(ref.second.base);if(found!=catalog.eventBaseScripts.end()&&found->second==script)return ref.first;}
    assert(false&&"original scripted reference missing");return uint32_t(0);
  };
  auto event=[&](uint32_t ref,const char* name){if(!p.HasReferenceEvent(ref,name))return;auto plan=p.PrepareReferenceEventTransaction(ref,name,fo3player::PlayerRef,error);Check(bool(plan)&&p.CommitScriptTransaction(*plan),error);};
  auto enter=[&](const char* script){const auto ref=scriptedReference(script);const auto& at=d.targets.at(ref);p.SetScriptPlayerLocation(at.x,at.y,at.z,at.cell,at.world);};
  auto actorArrival=[&](const char* reference,const char* package){
    const auto ref=d.formNames.at(reference),id=d.formNames.at(package);
    // Presentation fixture signals an actual package start/arrival boundary;
    // package and actor scripts perform all quest progression themselves.
    for(bool completed:{false,true}){
      auto actor=p.Snapshot().actors.at(ref);actor.package=id;
      if(completed){const auto& at=d.targets.at(d.packages.at(id).location.value);actor.position={at.x,at.y,at.z};actor.cell=at.cell;actor.world=at.world;}
      Check(p.UpdateActor(ref,actor),"original package arrival pose fixture");
      auto plan=p.PreparePackageEventTransaction(ref,id,completed,error,fo3player::ScriptProgramCapture::EnteredPrograms);Check(bool(plan),error);
      if(!plan->ExecutedScripts().empty()){std::atomic<bool> cancel{false};fo3script::ScriptMasterMatch report;Check(fo3script::MatchScriptProgramsToMaster(master,plan->ExecutedScripts(),report,cancel,error),error);}
      Check(p.CommitScriptTransaction(*plan),error);
    }
  };
  auto arrival=[&](const char* package){actorArrival("cg01dadref",package);};
  p.SetScriptEventDiagnostic([&](const std::string& message){if(message.find(std::to_string(d.formNames.at("cg01script")))!=std::string::npos||message.find(std::to_string(d.formNames.at("cg02script")))!=std::string::npos||message.find("DIALOGUE")!=std::string::npos)std::cerr<<message<<"\n";});
  unsigned modalTicks=0;bool allocated=false,followed=false,packageGreeting=false,resetSeen=false,radioSeen=false;
  for(unsigned frame=0;frame<4000;++frame){
    const auto child=p.Snapshot().pipboy.quests.find(baby);
    if(p.Snapshot().pipboy.quests.count(birthday)&&p.Snapshot().pipboy.quests.at(birthday).stage>=12)break;
    if(p.Snapshot().pipboy.quests.count(birthday)&&p.Snapshot().pipboy.quests.at(birthday).stage==10&&!packageGreeting){
      const auto dad=d.formNames.at("cg02dadref");const auto& definition=d.packages.at(d.formNames.at("cg02dadtowardsplayerindiner"));
      assert(definition.type==15&&definition.dialogueValid&&definition.dialogueType==1&&definition.dialogueTopic&&definition.target.value==fo3player::PlayerRef);
      auto actor=p.Snapshot().actors.at(dad);actor.package=d.formNames.at("cg02dadtowardsplayerindiner");
      fo3pipdata::Location location;assert(p.ScriptReferenceLocation(fo3player::PlayerRef,location));actor.cell=location.cell;actor.world=location.world;actor.position={location.x,location.y,p.ScriptPlayerHeight()};Check(p.UpdateActor(dad,actor),"package arrival fixture");
      auto start=p.PreparePackageEventTransaction(dad,actor.package,false,error);Check(bool(start)&&p.CommitScriptTransaction(*start),error);
      Check(p.QueueScriptSpeech(dad,definition.dialogueTopic,fo3player::PlayerRef,false,false,actor.package),"original package-specific Say To");packageGreeting=true;
    }
    if(child!=p.Snapshot().pipboy.quests.end()){
      const auto stage=child->second.stage;

      if(stage==10)enter("cg01dadtriggerscript");
      if(stage==14&&p.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(d,"CG01DadREF.doTalk"))==0)arrival("cg01dadclosegate");
      if(stage==16&&p.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(d,"CG01DadREF.doTalk"))==0)arrival("cg01dadclosedoor");
      if(stage==20)event(d.formNames.at("cg01playpengateref"),"OnActivate");
      if(stage==30)enter("cg01exitcribtriggerscript");
      if(stage==40){event(scriptedReference("cg01specialbookscript"),"OnActivate");
        assert(p.Snapshot().specialMenuTotal==40);
        while(p.SpecialBookRemaining()>0)for(unsigned n=0;n<7&&p.SpecialBookRemaining()>0;++n)if(p.Snapshot().specialDraft[n]<10)assert(p.AdjustSpecialBook(n,1));
        assert(p.ConfirmSpecialBook());allocated=true;
      }
      if(stage==70)arrival("cg01dadreturn");
      if(stage==73&&p.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(d,"CG01DadREF.doTalk"))==0)arrival("cg01dadshowbibleverse");
      if(stage==80){enter("cg01endquesttriggerscript");followed=true;}
      for(const auto& change:p.ScriptTriggerChanges()){event(change.first,change.second?"OnTriggerEnter":"OnTriggerLeave");p.AcknowledgeScriptTrigger(change.first,change.second);}
      for(uint32_t ref:p.ScriptOccupiedTriggers())event(ref,"OnTrigger");

    }
    if(p.Snapshot().movie.owner){
      const auto request=p.Snapshot().movie;
      if(request.owner==baby){assert(!sawMovie&&request.path=="1 year later.bik"&&request.flags==4);assert(p.Snapshot().playerToddler&&p.Snapshot().playerScale==.4f&&p.Snapshot().pipboy.quests.at(baby).stage==5);sawMovie=true;}
      else {assert(request.owner==birthday&&!sawBirthday&&request.path=="9 years later.bik"&&request.flags==4);assert(!p.Snapshot().playerToddler&&p.Snapshot().playerScale==1&&p.Snapshot().pipboy.quests.at(baby).stage==100);sawBirthday=true;bool ignored=false;assert(p.GetIgnoreCrime(d.formNames.at("cg02dadref"),ignored)&&ignored);}

      const auto variables=p.Snapshot().pipboy.dialogueVariables;
      for(unsigned n=0;n<20;++n)p.PumpWorldAwareGameMode(.1,residents,[&](auto plan){return p.CommitScriptTransaction(*plan);});
      assert(p.Snapshot().pipboy.dialogueVariables==variables);
      const auto checkpoint="/tmp/fq-toddler-movie-"+std::to_string(getpid());Check(p.Save(checkpoint,error),error);
      fo3player::Player saved(catalog);Check(saved.Restore(checkpoint,error),error);unlink(checkpoint.c_str());
      assert(saved.Snapshot().movie.path==request.path&&saved.Snapshot().playerScale==p.Snapshot().playerScale&&saved.Snapshot().ignoredCrimeActors==p.Snapshot().ignoredCrimeActors);
      // Host presentation fixture signals actual completion; no quest stage is set.
      assert(p.CompleteMovie(request.serial));const auto moved=p.Snapshot().playerMove;
      p.SetScriptPlayerLocation(moved.position[0],moved.position[1],moved.position[2],moved.cell,moved.world);p.AcknowledgePlayerMove();continue;
    }
    if(p.Snapshot().nameMenuOwner){
      assert(chose&&genderLine);
      if(modalTicks++<20){const auto stage=p.Snapshot().pipboy.quests.at(quest).stage;p.PumpWorldAwareGameMode(.25,residents,[&](auto plan){return p.CommitScriptTransaction(*plan);},true);assert(p.Snapshot().pipboy.quests.at(quest).stage==stage);continue;}
      assert(p.EditPlayerName(female?"Catherine":"James")&&p.ConfirmPlayerName());enteredName=true;
    }
    p.SetScriptRuntimeClock(frame*.1,.1);p.PumpWorldAwareGameMode(.1,residents,[&](auto plan){return p.CommitScriptTransaction(*plan);});
    fo3player::ScriptMessage message;
    while(p.PollScriptMessage(message))if(message.owner==quest){
      assert(!chose&&message.buttons.size()==2);
      const auto before=p.Snapshot().pipboy.dialogueVariables;
      for(int tick=0;tick<20;++tick)p.PumpWorldAwareGameMode(.25,residents,[&](auto plan){return p.CommitScriptTransaction(*plan);},true);
      assert(p.Snapshot().pipboy.dialogueVariables==before);
      assert(p.SubmitScriptMessageButton(message.owner,female?1:0));chose=true;
    }
    if(p.Snapshot().movie.owner)continue;
    fo3script::Action action;
    while(p.PollScriptAction(action)){
      if(action.kind==fo3script::ActionKind::PipboyReset)resetSeen=true;
      if(action.kind==fo3script::ActionKind::RadioRefresh)radioSeen=true;
      if(action.kind!=fo3script::ActionKind::SayTo)continue;
      if(std::find(residents.begin(),residents.end(),action.reference)==residents.end())continue;
      bool disabled=false;if(!p.ReferenceDisabled(action.reference,disabled,error)||disabled)continue;
      fo3dialogue::Context ctx;ctx.player=&p;ctx.talking=true;
      Check(fo3dialogue::ReferenceContext(p,action.reference,ctx.speaker),"birth speaker context");
      Check(fo3dialogue::ReferenceContext(p,action.flags,ctx.target),"birth listener context");
      fo3dialogue::Session voice;
      voice.previewReferences=[](const auto&,const auto&,auto&){return true;};
      voice.publishReferences=[&](auto& player,auto& plan,auto&){
        const auto radio=d.formNames.at("radiovault101ref");
        const auto& changed=plan.AffectedReferences();
        if(changed.size()==1&&changed.front()==radio&&
           !player.Snapshot().pipboy.quests.at(birthday).stages.count(12)){
          const auto cell=d.transmitters.at(radio).cell;
          std::atomic<bool> cancel{false};
          fo3script::ScriptMasterMatch proof;fo3scene::ResidentAuthoredAdmission admission;
          Check(fo3script::MatchScriptProgramsToMaster(master,plan.ExecutedScripts(),proof,cancel,error),error);
          Check(fo3scene::PrepareResidentAuthoredAdmission(master,cell,changed,admission,cancel,error),error);
          radioPrepared=true;
        }
        return player.CommitScriptTransaction(plan);
      };
      voice.diagnostic=[&](const std::string& message){error=message;};
      const bool selected=voice.StartSelectedTopic(action.form,action.info,ctx,p);
      Check(selected,"original birth INFO "+std::to_string(action.info)+" stage "+std::to_string(p.Snapshot().pipboy.quests.at(quest).stage)+": "+error);
      const auto* info=voice.Current(d);assert(info);
      for(const auto& condition:info->conditions)if(!sawBirthday&&(condition.function==70||condition.function==131)){
        assert(chose&&condition.a==uint32_t(female));genderLine=true;
      }
      p.SetScriptActorTalking(action.reference,true);
      unsigned responses=0;while(voice.phase==fo3dialogue::Phase::Speaking){assert(++responses<32);voice.AudioDone(voice.audioToken,true,ctx,p);}
      p.SetScriptActorTalking(action.reference,false);
      if(action.package){auto done=p.PreparePackageEventTransaction(action.reference,action.package,true,error);Check(bool(done)&&p.CommitScriptTransaction(*done),error);}
      else {const auto done=p.DispatchReferenceEvent(action.reference,"SayToDone",action.form,error);Check(done==fo3player::ScriptEventResult::Executed||done==fo3player::ScriptEventResult::NoHandler,error);}
    }
  }
  assert(chose&&genderLine&&enteredName&&p.PlayerFemale()==female&&!p.Snapshot().deferredAppearanceOwner);
  const auto& birth=p.Snapshot().pipboy.quests.at(quest);
  if(birth.stage!=100)std::cerr<<"Birth stopped at "<<birth.stage<<'\n';
  assert(birth.stage==100&&!birth.running&&birth.stages.count(61)&&birth.stages.count(62)&&birth.stages.count(65)&&birth.stages.count(80));
  if(!p.Snapshot().pipboy.quests.count(birthday)||p.Snapshot().pipboy.quests.at(birthday).stage<12)std::cerr<<"Birthday stopped; toddler "<<p.Snapshot().pipboy.quests.at(baby).stage<<" birthday "<<(p.Snapshot().pipboy.quests.count(birthday)?p.Snapshot().pipboy.quests.at(birthday).stage:0)<<"\n";
  assert(sawMovie&&sawBirthday&&allocated&&followed&&p.Snapshot().pipboy.quests.at(baby).stage==100&&!p.Snapshot().playerToddler&&p.Snapshot().playerScale==1);
  assert(packageGreeting&&resetSeen&&radioSeen&&radioPrepared);
  bool radioDisabled=true;Check(p.ReferenceDisabled(d.formNames.at("radiovault101ref"),radioDisabled,error),error);assert(!radioDisabled);
  assert(!p.PlayerControlDisabled(0)&&!p.PlayerControlDisabled(5)&&p.PlayerControlDisabled(2));
  assert(!fo3player::EarlyFirstPerson(p));
  assert(p.Snapshot().gameYear==2268&&p.Snapshot().gameMonth==6&&p.Snapshot().gameDay==13);
  const auto adult=fo3pipdata::ActorCategory(d,fo3player::PlayerBase,1)->race;
  assert(p.PlayerRace()==d.raceAges.at(adult)[0]&&p.ItemCount(d.formNames.at("pipboy"))>=1);
  assert(p.Snapshot().pipboy.quests.at(birthday).stage>=12&&!p.Snapshot().autoDisplayObjectives&&p.Snapshot().pipboy.quests.at(baby).stages.count(50));
  // Continue through the actual party choices, not direct stage commands.
  auto context=[&](uint32_t ref){fo3dialogue::Context ctx;ctx.player=&p;ctx.talking=true;Check(fo3dialogue::ReferenceContext(p,ref,ctx.speaker),"party speaker");Check(fo3dialogue::ReferenceContext(p,fo3player::PlayerRef,ctx.target),"party listener");return ctx;};
  auto finish=[&](fo3dialogue::Session& voice,fo3dialogue::Context& ctx){unsigned n=0;while(voice.phase==fo3dialogue::Phase::Speaking){assert(++n<32);voice.AudioDone(voice.audioToken,true,ctx,p);if(voice.phase==fo3dialogue::Phase::Speaking&&(voice.actor!=ctx.speaker.reference||voice.proxy!=ctx.speaker.proxy))std::swap(ctx.speaker,ctx.target);}};
  auto configure=[&](fo3dialogue::Session& voice){voice.previewReferences=[](const auto&,const auto&,auto&){return true;};voice.publishReferences=[](auto& player,auto& plan,auto&){return player.CommitScriptTransaction(plan);};voice.diagnostic=[&](const std::string& e){error=e;};};
  auto talk=[&](const char* ref,std::initializer_list<const char*> topics){auto ctx=context(d.formNames.at(ref));fo3dialogue::Session voice;configure(voice);Check(voice.Start(ctx,p),std::string("party greeting ")+ref+": "+error);finish(voice,ctx);
    if(std::string(ref)=="cg02butchref"){
      for(const char* name:{"cg02butcha5","cg02butcha6"}){
        const auto topic=d.formNames.at(name);
        const auto choice=std::find_if(voice.choices.begin(),voice.choices.end(),[&](const auto& v){return v.topic==topic;});
        Check(choice!=voice.choices.end(),"original Butch provocation choice");
        auto fight=p.PrepareDialogueInfoTransaction(topic,choice->info,fo3player::DialogueScriptPhase::Both,error,ctx.speaker.reference);
        Check(bool(fight)&&p.CommitScriptTransaction(*fight),error);
        assert(p.Snapshot().actors.at(ctx.speaker.reference).hostile==fo3player::PlayerRef);
        auto reaction=p.PrepareReferenceEventTransaction(ctx.speaker.reference,"OnStartCombat",fo3player::PlayerRef,error);
        Check(bool(reaction),error);
        Check(p.RollbackScriptTransaction(*fight), "restore the pre-fight state for the peaceful route");
      }
    }
    for(const char* name:topics){auto choice=std::find_if(voice.choices.begin(),voice.choices.end(),[&](const auto& v){return v.topic==d.formNames.at(name);});
      if(choice==voice.choices.end()){std::cerr<<"Missing party choice "<<name<<" available:";for(const auto& c:voice.choices)std::cerr<<" "<<c.text;std::cerr<<"\n";}
      Check(choice!=voice.choices.end(),std::string("party choice ")+name+": "+error);Check(voice.Choose(size_t(choice-voice.choices.begin()),ctx,p),error);finish(voice,ctx);}
    voice.End("party fixture finished");};
  auto queuedSpeech=[&](){fo3script::Action action;unsigned n=0;while(p.PollScriptAction(action)){assert(++n<64);if(action.kind!=fo3script::ActionKind::SayTo&&action.kind!=fo3script::ActionKind::Conversation)continue;
    auto ctx=context(action.reference);Check(fo3dialogue::ReferenceContext(p,action.flags,ctx.target),"cake listener");fo3dialogue::Session voice;configure(voice);Check(voice.StartSelectedTopic(action.form,action.info,ctx,p,action.kind==fo3script::ActionKind::Conversation),"original party speech: "+error);finish(voice,ctx);
    if(action.package){auto done=p.PreparePackageEventTransaction(action.reference,action.package,true,error);Check(bool(done)&&p.CommitScriptTransaction(*done),error);}
    else {const auto done=p.DispatchReferenceEvent(action.reference,"SayToDone",action.form,error);Check(done==fo3player::ScriptEventResult::Executed||done==fo3player::ScriptEventResult::NoHandler,error);}
  }};
  talk("cg02oldladypalmerref",{"cg02oldladypalmera2"});
  assert(p.ItemCount(d.formNames.at("cg02sweetroll"))==1&&p.Snapshot().pipboy.quests.at(birthday).stages.count(20));
  queuedSpeech();
  const auto andy=d.formNames.at("cg02andyref"),cut=d.formNames.at("cg02andycutcake"),cake=d.formNames.at("cg02cakeref");
  (*registry)[cake]={{"forward",1,false}};p.SetScriptAnimationRegistry(registry);
  auto cutter=p.Snapshot().actors.at(andy);cutter.package=cut;Check(p.UpdateActor(andy,cutter),"cake package fixture");
  for(bool done:{false,true}){auto plan=p.PreparePackageEventTransaction(andy,cut,done,error,fo3player::ScriptProgramCapture::EnteredPrograms);Check(bool(plan),error);
    if(!plan->ExecutedScripts().empty()){std::atomic<bool> cancel{false};fo3script::ScriptMasterMatch report;Check(fo3script::MatchScriptProgramsToMaster(master,plan->ExecutedScripts(),report,cancel,error),error);}Check(p.CommitScriptTransaction(*plan),error);}
  // Presentation fixture: the authored final idle finishes, then the runtime
  // retires the Must Complete travel package and delivers its real OnChange.
  auto retire=p.PreparePackageEventTransaction(andy,0,false,error,fo3player::ScriptProgramCapture::EnteredPrograms);Check(bool(retire),error);
  {std::atomic<bool> cancel{false};fo3script::ScriptMasterMatch report;Check(fo3script::MatchScriptProgramsToMaster(master,retire->ExecutedScripts(),report,cancel,error),error);}Check(p.CommitScriptTransaction(*retire),error);
  queuedSpeech();assert(p.Snapshot().pipboy.quests.at(birthday).stages.count(16));
  const auto butch=d.formNames.at("cg02butchref"),findPlayer=d.formNames.at("cg02butchfindplayer");
  auto bully=p.Snapshot().actors.at(butch);bully.package=findPlayer;Check(p.UpdateActor(butch,bully),"Butch approach package fixture");
  auto approach=p.PreparePackageEventTransaction(butch,findPlayer,false,error);Check(bool(approach)&&p.CommitScriptTransaction(*approach),error);
  talk("cg02butchref",{"cg02butcha3"});assert(p.ItemCount(d.formNames.at("cg02sweetroll"))==0&&p.Snapshot().pipboy.quests.at(birthday).stages.count(30));
  talk("cg02amataref",{"cg02amatapresenta2","cg02amatapresentb1"});
  talk("cg02stanleyref",{"cg02stanleya1","cg02stanleyb1"});
  assert(p.ItemCount(d.formNames.at("bookskillmelee"))==1&&p.ItemCount(d.formNames.at("kidhatprewarcapkid"))==1);
  assert(p.Snapshot().pipboy.quests.at(birthday).stages.count(34));
  // The original timer assigns Jonas to the intercom; no stage is injected.
  for(unsigned tick=0;tick<160&&p.Snapshot().pipboy.quests.at(birthday).stage<35;++tick){p.SetScriptRuntimeClock(1000+tick*.1,.1);p.PumpWorldAwareGameMode(.1,residents,[&](auto plan){return p.CommitScriptTransaction(*plan);});}
  const auto intercom=d.formNames.at("cg02dinerintercomref"),jonas=d.formNames.at("cg02jonasref"),dad=d.formNames.at("cg02dadref");
  assert(p.Snapshot().pipboy.quests.at(birthday).stage==35&&p.TalkingActivatorActor(intercom)==jonas);
  const auto checkpoint="/tmp/fq-intercom-"+std::to_string(getpid());Check(p.Save(checkpoint,error),error);fo3player::Player intercomRestored(catalog);Check(intercomRestored.Restore(checkpoint,error),error);unlink(checkpoint.c_str());assert(intercomRestored.TalkingActivatorActor(intercom)==jonas);
  fo3dialogue::ActorContext proxy;Check(fo3dialogue::ReferenceContext(p,intercom,proxy),"original intercom identity");assert(proxy.reference==jonas&&proxy.proxy==intercom&&proxy.base==d.targets.at(jonas).base&&proxy.cell==d.targets.at(intercom).cell);
  const auto conversation=d.formNames.at("cg02dadtalktojonasonintercom");auto dadAt=p.Snapshot().actors.at(dad);dadAt.package=conversation;dadAt.position={proxy.x,proxy.y,proxy.z};dadAt.cell=proxy.cell;dadAt.world=proxy.world;Check(p.UpdateActor(dad,dadAt),"intercom approach fixture");
  auto started=p.PreparePackageEventTransaction(dad,conversation,false,error);Check(bool(started)&&p.CommitScriptTransaction(*started),error);
  Check(p.QueueScriptSpeech(dad,d.packages.at(conversation).dialogueTopic,intercom,true,false,conversation),"original specified-topic intercom conversation");queuedSpeech();
  assert(p.Snapshot().pipboy.quests.at(birthday).stage==38&&!p.TalkingActivatorActor(intercom));
  assert((p.Snapshot().actorPackageEvents.at(dad)==std::make_pair(conversation,true)));
  talk("cg02dadref",{});assert(p.Snapshot().pipboy.quests.at(birthday).stage==40);
  talk("cg02jonasref",{"cg02jonasa1"});assert(p.Snapshot().pipboy.quests.at(birthday).stage==42);
  talk("cg02dadref",{"cg02dadtopic1","cg02dadchoice1"});
  assert(p.Snapshot().pipboy.quests.at(birthday).stages.count(44)&&p.ItemCount(d.formNames.at("cg02weapbbgun"))==1&&p.ItemCount(d.formNames.at("ammobb"))>=50);
  assert(!p.PlayerControlDisabled(2)&&!p.GetLock(d.formNames.at("cg02targetrangedoornew")).locked);
  const auto& range=d.targets.at(d.formNames.at("cg02dadtargetrangeinsidemarker"));
  auto arrived=p.Snapshot().actors.at(dad);arrived.position={range.x,range.y,range.z};arrived.cell=range.cell;arrived.world=range.world;
  arrived.package=d.formNames.at("cg02dadwaitinsidetargetrange");Check(p.UpdateActor(dad,arrived),"original range arrival fixture");
  p.SetScriptPlayerLocation(range.x,range.y,range.z,range.cell,range.world);
  talk("cg02dadref",{});
  assert(p.Snapshot().pipboy.quests.at(birthday).stage==50);
  const auto target=d.formNames.at("cg02target1"),bb=d.formNames.at("cg02weapbbgun"),roach=d.formNames.at("cg02radroachref");
  (*registry)[target]={{"Forward",2.5,false}};p.SetScriptAnimationRegistry(registry);
  const auto count=fo3dialogue::VariableKey(d,"CG02.targetCount");
  for(unsigned n=0;n<3;++n){
    auto hit=p.PrepareWeaponHitTransaction(target,fo3player::PlayerRef,bb,error,fo3player::ScriptProgramCapture::EnteredPrograms);Check(bool(hit),error);
    std::atomic<bool> cancel{false};fo3script::ScriptMasterMatch report;Check(fo3script::MatchScriptProgramsToMaster(master,hit->ExecutedScripts(),report,cancel,error),error);
    assert(p.Snapshot().pipboy.dialogueVariables.count(count)==0||p.Snapshot().pipboy.dialogueVariables.at(count)==float(n));
    Check(p.CommitScriptTransaction(*hit),error);assert(p.Snapshot().pipboy.dialogueVariables.at(count)==float(n+1));
  }
  bool hidden=true;Check(p.ReferenceDisabled(roach,hidden,error),error);assert(!hidden&&p.Snapshot().pipboy.quests.at(birthday).stage==55);
  {auto ctx=context(dad);fo3dialogue::Session warning;configure(warning);Check(warning.StartTopic(d.formNames.at("cg02dadspeech"),ctx,p),"original radroach warning");finish(warning,ctx);}
  assert(p.Snapshot().pipboy.quests.at(birthday).stage==60&&p.ActorHealth(roach)>0);
  Check(p.ApplyAttack(fo3player::PlayerRef,bb,roach,10000),"original radroach death event");
  assert(p.Snapshot().pipboy.quests.at(birthday).stage==70&&p.ActorHealth(roach)==0);
  const auto save="/tmp/fq-original-identity-"+std::to_string(getpid());Check(p.Save(save,error),error);
  fo3player::Player restored(catalog);Check(restored.Restore(save,error),error);unlink(save.c_str());
  assert(restored.PlayerFemale()==female&&restored.PlayerName()==(female?"Catherine":"James"));
  assert(restored.Snapshot().pipboy.quests.at(birthday).stage==70&&restored.ActorHealth(roach)==0&&restored.Snapshot().pipboy.dialogueVariables.at(count)==3);
  assert(restored.Snapshot().pipboy.quests.at(birthday).stages.count(34)&&restored.ItemCount(d.formNames.at("bookskillmelee"))==1&&restored.ItemCount(d.formNames.at("kidhatprewarcapkid"))==1);
  // Original photo: readiness comes from package callbacks and the placed
  // trigger's heading check, rather than a stage/result injected by the test.
  const auto wristCount=p.ItemCount(d.formNames.at("pipboy"));
  talk("cg02dadref",{});assert(p.Snapshot().pipboy.quests.at(birthday).stage==80);queuedSpeech();
  const auto photo=scriptedReference("cg02playerpicturetriggerscript");const auto& photoAt=d.targets.at(photo);
  p.SetScriptPlayerLocation(photoAt.x,photoAt.y,photoAt.z,photoAt.cell,photoAt.world);p.SetScriptPlayerHeading(0);
  for(const auto& change:p.ScriptTriggerChanges())p.AcknowledgeScriptTrigger(change.first,change.second);
  const auto occupied=p.ScriptOccupiedTriggers();assert(std::find(occupied.begin(),occupied.end(),photo)!=occupied.end());
  const auto ready=fo3dialogue::VariableKey(d,"CG02.PlayerReady");
  event(photo,"OnTrigger");assert(!p.Snapshot().pipboy.dialogueVariables.count(ready)||p.Snapshot().pipboy.dialogueVariables.at(ready)==0);
  actorArrival("cg02dadref","cg02dadwaitforpicture");actorArrival("cg02jonasref","cg02jonaswaitforpicture");
  assert(p.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(d,"CG02.DadReady"))==1&&p.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(d,"CG02.JonasReady"))==1);
  const auto& photographer=p.Snapshot().actors.at(jonas);
  const auto faceJonas=float(std::atan2(-(photographer.position[0]-photoAt.x),photographer.position[1]-photoAt.y));
  p.SetScriptPlayerHeading(faceJonas+1.57079632679f);event(photo,"OnTrigger");assert(p.Snapshot().pipboy.dialogueVariables.at(ready)==0);
  p.SetScriptPlayerHeading(faceJonas);
  {auto pose=p.PrepareReferenceEventTransaction(photo,"OnTrigger",fo3player::PlayerRef,error,fo3player::ScriptProgramCapture::EnteredPrograms);Check(bool(pose),error);
    std::atomic<bool> cancel{false};fo3script::ScriptMasterMatch report;Check(fo3script::MatchScriptProgramsToMaster(master,pose->ExecutedScripts(),report,cancel,error),error);Check(p.CommitScriptTransaction(*pose),error);}
  assert(p.Snapshot().pipboy.dialogueVariables.at(ready)==1);
  event(photo,"OnTriggerLeave");assert(p.Snapshot().pipboy.dialogueVariables.at(ready)==0);event(photo,"OnTrigger");
  p.PumpWorldAwareGameMode(.1,{},[&](auto plan){return p.CommitScriptTransaction(*plan);});
  assert(p.Snapshot().pipboy.quests.at(birthday).stage==90&&p.PlayerControlDisabled(0));queuedSpeech();
  assert(p.Snapshot().pipboy.quests.at(birthday).stage==95&&!p.EquippedWeapon());
  for(unsigned tick=0;tick<100&&!p.Snapshot().movie.owner;++tick){p.SetScriptRuntimeClock(1200+tick*.1,.1);p.PumpWorldAwareGameMode(.1,{},[&](auto plan){return p.CommitScriptTransaction(*plan);});}
  const auto teen=d.formNames.at("cg03");const auto jump=p.Snapshot().movie;
  assert(jump.owner==teen&&jump.path=="6 years later.bik"&&jump.flags==4);
  assert(p.Snapshot().pipboy.quests.at(birthday).stage==100&&!p.Snapshot().pipboy.quests.at(birthday).running&&p.Snapshot().pipboy.quests.at(birthday).stages.count(98));
  assert(p.Snapshot().pipboy.quests.at(teen).stage==5&&p.PlayerRace()==adult&&p.PlayerControlDisabled(0));
  assert(p.Snapshot().gameYear==2274&&p.Snapshot().gameMonth==7&&p.Snapshot().gameDay==3&&!p.Snapshot().playerToddler);
  assert(!p.ItemCount(bb)&&!p.ItemCount(d.formNames.at("bookskillmelee"))&&p.ItemCount(d.formNames.at("pipboy"))==wristCount);
  const auto* dresser=p.ContainerContents(d.formNames.at("cg04playerdresserref"));assert(dresser&&std::any_of(dresser->begin(),dresser->end(),[&](const auto& item){return item.formId==d.formNames.at("bookskillmelee")&&item.count==1;}));
  bool flash=false;fo3script::Action photoAction;while(p.PollScriptAction(photoAction))if(photoAction.kind==fo3script::ActionKind::ImageModifier&&photoAction.form==d.formNames.at("cgcameraflashisfx"))flash=true;assert(flash);
  const auto jumpSave="/tmp/fq-photo-"+std::to_string(getpid());Check(p.Save(jumpSave,error),error);fo3player::Player photoRestored(catalog);Check(photoRestored.Restore(jumpSave,error),error);unlink(jumpSave.c_str());
  assert(photoRestored.Snapshot().movie.path==jump.path&&photoRestored.Snapshot().playerMove.marker==d.formNames.at("cg03startmarkerref"));
  const auto beforeMovie=p.Snapshot().pipboy.dialogueVariables;
  for(unsigned tick=0;tick<20;++tick)p.PumpWorldAwareGameMode(.1,{d.formNames.at("cg03dadref")},[&](auto plan){return p.CommitScriptTransaction(*plan);});
  assert(p.Snapshot().pipboy.dialogueVariables==beforeMovie);
  assert(p.CompleteMovie(jump.serial));const auto teenStart=p.Snapshot().playerMove;
  p.SetScriptPlayerLocation(teenStart.position[0],teenStart.position[1],teenStart.position[2],teenStart.cell,teenStart.world);p.SetScriptPlayerHeading(teenStart.rotation[2]);p.AcknowledgePlayerMove();
  for(unsigned tick=0;tick<80&&p.Snapshot().pipboy.quests.at(teen).stage==5;++tick){p.SetScriptRuntimeClock(1300+tick*.1,.1);p.PumpWorldAwareGameMode(.1,{d.formNames.at("cg03dadref")},[&](auto plan){return p.CommitScriptTransaction(*plan);});queuedSpeech();}
  assert(p.Snapshot().pipboy.quests.at(teen).stage==6);
  {auto ctx=context(d.formNames.at("cg03dadref"));fo3dialogue::Session voice;configure(voice);Check(voice.Start(ctx,p),"original exam greeting");finish(voice,ctx);
    assert(p.PlayerControlDisabled(0));Check(voice.RequestGoodbye(ctx,p),"original exam farewell");assert(voice.topic==d.formNames.at("goodbye")&&p.PlayerControlDisabled(0));finish(voice,ctx);assert(!voice.Active());}
  assert(p.Snapshot().pipboy.quests.at(teen).stage==10&&!p.PlayerControlDisabled(0)&&p.Snapshot().pipboy.quests.at(teen).objectives.at(10).displayed);
}
int main(int argc,char** argv){
  std::string error;uint32_t quest=0;
  std::istringstream config("[Other]\nSCharGenQuest=bad\n[General]\nSCharGenQuest=0001f388 ; original\n");
  Check(fo3newgame::ReadQuest(config,quest,error),error);assert(quest==0x1f388);
  for(const char* invalid:{"[General]\nSCharGenQuest=0", "[General]\nSCharGenQuest=1z", "[General]\nSCharGenQuest=1\nSCharGenQuest=2", "[Other]\nSCharGenQuest=1"}){
    std::istringstream input(invalid);assert(!fo3newgame::ReadQuest(input,quest,error));
  }
  Synthetic();HeadingAngles();TagSkills();ScriptMathAndPrompts();NestedActivation();ActorFurniture();Identity();ScriptInventory();CharacterMenus();Lifecycle();TalkingActivators();CrimeFlags();CalendarAssignments();
  if(argc<2)return 0;
  fo3player::Catalog catalog;Check(fo3player::LoadCatalog(argv[1],catalog,error),error);
  if(argc>2&&std::string(argv[2])=="--exam"){OriginalExam(catalog,argv[1],false);OriginalExam(catalog,argv[1],true);return 0;}
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
  OriginalIdentity(catalog,false,argv[1]);OriginalIdentity(catalog,true,argv[1]);OriginalInventory(catalog);OriginalSpecialBook(catalog);OriginalExam(catalog,argv[1],false);OriginalExam(catalog,argv[1],true);
  std::cout<<"Original birth, playpen, birthday, intercom, target practice, photo, six-year jump and G.O.A.T. introduction, both sex branches, package callbacks, heading, inventory and saves passed\n";
}
