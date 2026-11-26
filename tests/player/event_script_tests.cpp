#include "player/fo3-player-state.h"
#include "player/fo3-new-game.h"
#include <cassert>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <map>
#include <iterator>
#include <chrono>
#include <unistd.h>
#include <zlib.h>
#include "dialogue/fo3-dialogue-results.h"
#include "scripting/fo3-script-compile-cache.h"
#include "scripting/fo3-result-bytecode.h"

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
  c.initial.baseHealth=100;c.initial.special.fill(1);
  c.pipboy.quests[20].editor="ExampleQuest";
  c.pipboy.quests[20].stages[10]={};
  c.pipboy.formNames["examplequest"]=20;
  c.pipboy.quests[20].script=101;
  c.pipboy.quests[20].scriptDelay=.25f; // Explicit cadence for this synthetic fixture.
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
static void EquipmentInstanceEvents(){
  auto c=Catalog();
  c.items[40].formId=40;c.items[40].kind=fo3player::ItemKind::Armour;c.items[40].bipedMask=4;
  auto& script=c.pipboy.scripts[103];script.variables={{1,"uses"},{2,"wearer"}};
  script.source="scn Wearable\nshort uses\nref wearer\nBegin OnEquip Player\nset uses to uses + 1\nset wearer to GetActionRef\nEnd\nBegin OnUnequip Player\nset uses to uses + 10\nset wearer to 0\nEnd";
  c.eventBaseScripts[40]=103;c.eventPrograms[103]=fo3script::ParseEvents(script);
  fo3player::Player p(c);p.SetScriptEventDiagnostic([](const auto& message){std::cerr<<message<<std::endl;});
  assert(p.Add(40,2));const auto first=p.Snapshot().inventory.front().id;
  assert(p.Equip(first));auto second=p.Snapshot().inventory.back().id;
  assert(first!=second&&p.Equip(second));
  auto scoped=[&](const fo3player::Player& player,uint64_t id){
    for(const auto& item:player.Snapshot().inventory)if(item.id==id)return item.scriptInstance;
    return std::shared_ptr<const fo3player::ItemScriptInstance>{};
  };
  auto a=scoped(p,first),b=scoped(p,second);
  assert(a&&b&&a->reference!=b->reference&&a->numbers.at(1)==11&&b->numbers.at(1)==1);
  assert(a->references.empty()&&b->references.at(2)==fo3player::PlayerRef);
  assert(p.Snapshot().pipboy.dialogueVariables.empty()&&p.Snapshot().pipboy.referenceVariables.empty());
  char save[]="/tmp/fq-equipment-XXXXXX";int fd=mkstemp(save);assert(fd>=0);close(fd);
  std::string error;assert(p.Save(save,error));fo3player::Player restored(c);
  if(!restored.Restore(save,error)){std::cerr<<error<<std::endl;std::abort();}unlink(save);
  assert(scoped(restored,first)->reference==a->reference&&scoped(restored,first)->numbers.at(1)==11);
  assert(restored.Unequip(second)&&scoped(restored,second)->numbers.at(1)==11);
}
static void Resurrection(){
  auto catalog=Catalog();
  auto& placement=catalog.actorPlacements[100];
  placement.base=30;placement.cell=77;placement.x=42;placement.scale=1;
  catalog.pipboy.targets[100]=placement;catalog.pipboy.cellWorlds[77]=0;
  catalog.weapons.actors[30].health=80;catalog.authoredCorpseBases.insert(30);
  catalog.pipboy.formNames["bodyref"]=100;
  fo3player::Player player(catalog);player.RegisterOriginalActors();
  assert(player.ActorHealth(100)==0);
  // Exact 108c stream with the receiver in SCRO slot 1, animation flag 0.
  fo3pipdata::ResultScript restore;
  restore.references={100};restore.compiled={0x1c,0,1,0,0x8c,0x10,7,0,1,0,'n',0,0,0,0};
  std::string error;
  if(!player.ExecuteDialogueResult(restore,error)){std::cerr<<"Resurrection: "<<error<<std::endl;std::abort();}
  assert(player.ActorHealth(100)==80&&player.Snapshot().actors.at(100).position[0]==42);
  const auto revision=player.Revision();
  assert(player.ExecuteDialogueResult(restore,error)&&player.Revision()==revision);
  char save[]="/tmp/fq-revived-XXXXXX";const int fd=mkstemp(save);assert(fd>=0);close(fd);
  assert(player.Save(save,error));fo3player::Player reloaded(catalog);
  if(!reloaded.Restore(save,error)){std::cerr<<"Resurrection save: "<<error<<std::endl;std::abort();}
  unlink(save);reloaded.RegisterOriginalActors();assert(reloaded.ActorHealth(100)==80);
  assert(reloaded.KillActor(100)&&reloaded.ActorHealth(100)==0);
  restore.compiled.clear();restore.source="BodyRef.Resurrect 0";
  assert(reloaded.ExecuteDialogueResult(restore,error)&&reloaded.ActorHealth(100)==80);
}
static void DeathEventOrdering(){
  auto c=Catalog();
  auto& placed=c.actorPlacements[100];placed.cell=77;placed.base=30;placed.scale=1;
  c.pipboy.targets[100]=placed;c.weapons.actors[30].health=80;
  c.pipboy.formNames["victim"]=100;
  c.pipboy.scripts[102].source="scn NPC\nBegin OnDeath Player\nif GetStage ExampleQuest == 10\nRewardKarma 20\nendif\nEnd";
  c.eventPrograms[102]=fo3script::ParseEvents(c.pipboy.scripts[102]);
  fo3player::Player player(c);player.RegisterOriginalActors();
  fo3pipdata::ResultScript action;std::string error;
  action.source="victim.kill player\nsetstage ExampleQuest 10\nreturn\nRewardKarma 100";
  assert(player.ExecuteDialogueResult(action,error));
  assert(player.ActorHealth(100)==0&&player.Snapshot().karma==20);
  assert(player.ExecuteDialogueResult(action,error)&&player.Snapshot().karma==20);
}
static void GoreSettingAndBlastPresentation(){
  for(bool disabled:{false,true}){
    std::istringstream settings(disabled?"[Other]\nbDisableAllGore=0\n[GENERAL]\nbDisableAllGore = 1 ; installed setting\n":
                                       "[Other]\nbDisableAllGore=1\n[General]\nsLanguage=ENGLISH\n");
    auto c=Catalog();c.goreDisabled=fo3newgame::ReadGoreDisabled(settings);
    assert(c.goreDisabled==disabled);fo3player::Player p(c);std::string error;
    fo3pipdata::ResultScript command;command.source="if IsGoreDisabled == 0\nRewardKarma 10\nendif";
    assert(p.ExecuteDialogueResult(command,error)&&p.Snapshot().karma==(disabled?0:10));
    fo3dialogue::Context context;context.player=&p;context.speaker.reference=fo3player::PlayerRef;
    fo3pipdata::Condition condition;condition.function=550;condition.value=disabled?1:0;
    assert(fo3dialogue::Conditions({condition},context,error));
  }
  fo3script::ImageModifier modifier;modifier.valid=true;modifier.duration=1;
  modifier.fade.keys.push_back({0,{.25f,.5f,1.f,1.f}});
  modifier.blur.keys.push_back({0,{8.f,0,0,0}});
  modifier.contrastMult.keys.push_back({0,{2.f,0,0,0}});
  fo3script::ImageStack stack;
  const float fraction=fo3player::ExplosionDefinition::RadialFraction(50,100);
  stack.Apply(1,modifier,0,fraction);
  const auto presentation=stack.Sample(0);
  assert(fraction==.75f&&presentation.blur==6&&presentation.fade[3]==.75f&&presentation.contrast==1.75f);
}
static void ScriptedHealthDamage(){
  auto c=Catalog();
  auto& placed=c.actorPlacements[100];placed.cell=77;placed.base=30;placed.scale=1;
  c.pipboy.targets[100]=placed;c.weapons.actors[30].health=80;
  c.pipboy.formNames["patient"]=100;
  c.pipboy.scripts[102].source="scn NPC\nBegin OnDeath\nif GetStage ExampleQuest == 10\nRewardKarma 20\nendif\nEnd";
  c.eventPrograms[102]=fo3script::ParseEvents(c.pipboy.scripts[102]);
  fo3player::Player p(c);p.RegisterOriginalActors();
  fo3pipdata::ResultScript command;std::string error;
  command.source="Patient.DamageAV Health 12\nPatient.RestoreAV Health 5";
  assert(p.ExecuteDialogueResult(command,error)&&p.ActorHealth(100)==73);
  command.source="Patient.DamageAV Health 73\nSetStage ExampleQuest 10";
  assert(p.ExecuteDialogueResult(command,error)&&p.ActorHealth(100)==0&&p.Snapshot().karma==20);
  command.source="Patient.RestoreAV Health 100\nPatient.DamageAV Health 50";
  assert(p.ExecuteDialogueResult(command,error)&&p.ActorHealth(100)==0&&p.Snapshot().karma==20);
}
static void EssentialKnockout(){
  auto catalog=Catalog();
  auto& placement=catalog.actorPlacements[100];placement.cell=77;placement.base=30;placement.scale=1;
  catalog.pipboy.targets[100]=placement;catalog.pipboy.cellWorlds[77]=0;
  catalog.weapons.actors[30].flags=2;catalog.weapons.actors[30].health=80;
  catalog.pipboy.formNames["patient"]=100;
  fo3player::Player p(catalog);p.RegisterOriginalActors();
  fo3pipdata::ResultScript command;command.source="Patient.Kill";std::string error;
  assert(p.ExecuteDialogueResult(command,error)&&p.ActorKnockedOut(100)&&!p.ActorUnconscious(100));
  bool dead=true;assert(p.ActorDeathState(100,dead)&&!dead&&!p.Snapshot().actors.at(100).dead);
  assert(!p.Snapshot().pipboy.quests.count(20)); // OnDeath is exclusive to a death.
  p.AdvanceActorRecovery(4);
  command.source="Patient.SetUnconscious 1";assert(p.ExecuteDialogueResult(command,error)&&p.ActorUnconscious(100));
  command.source="Patient.SetGhost 1";assert(p.ExecuteDialogueResult(command,error)&&p.ActorGhost(100));
  char path[]="/tmp/fq-knockout-XXXXXX";const int fd=mkstemp(path);assert(fd>=0);close(fd);
  assert(p.Save(path,error));fo3player::Player returned(catalog);
  if(!returned.Restore(path,error)){std::cerr<<"knockout save: "<<error<<std::endl;std::abort();}
  unlink(path);assert(returned.ActorKnockedOut(100)&&returned.ActorUnconscious(100)&&returned.ActorGhost(100));
  returned.AdvanceActorRecovery(5);assert(returned.ActorKnockedOut(100));
  returned.AdvanceActorRecovery(1);assert(!returned.ActorKnockedOut(100)&&returned.ActorUnconscious(100));
  assert(std::fabs(returned.ActorHealth(100)-24)<.001);
  command.source="if Patient.GetIsGhost\nPatient.SetGhost 0\nendif";assert(returned.ExecuteDialogueResult(command,error)&&!returned.ActorGhost(100));
  command.source="Patient.SetUnconscious 0";assert(returned.ExecuteDialogueResult(command,error)&&!returned.ActorUnconscious(100));
}
static void WeaponFilteredHit(){
  auto c=Catalog();c.items[40].formId=40;c.items[40].kind=fo3player::ItemKind::Weapon;c.items[40].weapon.valid=true;
  c.items[41]=c.items[40];c.items[41].formId=41;c.pipboy.formNames["specialweapon"]=40;
  c.pipboy.targets[100].base=30;c.weapons.actors[30].health=100;
  c.pipboy.scripts[102].source="scn NPC\nBegin OnHit Player\nRewardKarma 1\nEnd\nBegin OnHitWith SpecialWeapon\nif IsActionRef Player\nRewardKarma 10\nendif\nEnd";
  c.eventPrograms[102]=fo3script::ParseEvents(c.pipboy.scripts[102]);fo3player::Player p(c);
  assert(p.ApplyAttack(fo3player::PlayerRef,41,100,1)&&p.Snapshot().karma==1);
  assert(p.ApplyAttack(fo3player::PlayerRef,40,100,1)&&p.Snapshot().karma==12&&p.ActorHealth(100)==98);
}
static void QuestCadence(){
  auto c=Catalog();c.pipboy.quests[20].scriptDelay=5;
  auto& script=c.pipboy.scripts[101];script.variables[1]="ticks";script.variables[2]="elapsed";
  script.source="scn Q\nshort ticks\nfloat elapsed\nBegin GameMode\nset ticks to ticks + 1\nset elapsed to GetSecondsPassed\nEnd";
  c.eventPrograms[101]=fo3script::ParseEvents(script);
  fo3player::Player p(c);std::string error;assert(p.StartQuest(20));
  const uint64_t ticks=(uint64_t(20)<<32)|1,elapsed=(uint64_t(20)<<32)|2;
  auto value=[&](uint64_t key){const auto& vars=p.Snapshot().pipboy.dialogueVariables;auto i=vars.find(key);return i==vars.end()?0:i->second;};
  p.PumpQuestGameMode(.1);assert(value(ticks)==0);
  fo3pipdata::ResultScript fast;fast.source="SetQuestDelay ExampleQuest .01";
  assert(p.PreviewDialogueResult(fast,error)&&p.QuestScriptDelay(20)==5);
  assert(p.ExecuteDialogueResult(fast,error));p.PumpQuestGameMode(.01);assert(value(ticks)==1);
  p.PumpQuestGameMode(.01);assert(value(ticks)==2&&std::fabs(value(elapsed)-.01f)<1e-6f);
  fo3pipdata::ResultScript reset;reset.source="SetQuestDelay ExampleQuest 0";
  assert(p.ExecuteDialogueResult(reset,error)&&p.QuestScriptDelay(20)==5);
  for(int i=0;i<4;++i){p.PumpQuestGameMode(1);}
  assert(value(ticks)==2);
  p.PumpQuestGameMode(1);assert(value(ticks)==3&&value(elapsed)==5);
  assert(p.ExecuteDialogueResult(fast,error));
  auto save="/tmp/fq-quest-delay-"+std::to_string(getpid());assert(p.Save(save,error));
  fo3player::Player restored(c);assert(restored.Restore(save,error));unlink(save.c_str());
  assert(std::fabs(restored.QuestScriptDelay(20)-.01f)<1e-6f);
  restored.PumpQuestGameMode(.009);assert(restored.Snapshot().pipboy.dialogueVariables.at(ticks)==3);
  restored.PumpQuestGameMode(.002);assert(restored.Snapshot().pipboy.dialogueVariables.at(ticks)==4);
}
static void UnenteredCommands() {
  fo3player::Player p(Catalog());std::string error;
  fo3pipdata::ResultScript body;
  body.source="if 0\nUnsupportedFutureEffect argument\nendif\nRewardKarma 3";
  assert(p.ExecuteDialogueResult(body,error)&&p.Snapshot().karma==3);
  const auto revision=p.Revision();
  body.source="RewardKarma 7\nif 1\nUnsupportedFutureEffect argument\nendif";
  assert(!p.ExecuteDialogueResult(body,error));
  assert(error.find("UnsupportedFutureEffect")==std::string::npos); // Compiler normalises source.
  assert(error.find("unsupportedfutureeffect")!=std::string::npos);
  assert(p.Revision()==revision&&p.Snapshot().karma==3);
  body.source="if 0\nUnsupportedFutureEffect\nelse\nelse\nendif";
  assert(!p.ExecuteDialogueResult(body,error)&&p.Revision()==revision);
  body.source="if 0\nUnsupportedFutureEffect";
  assert(!p.ExecuteDialogueResult(body,error)&&p.Revision()==revision);
  body.source="UnsupportedFutureEffect";
  assert(!p.ExecuteDialogueResult(body,error)&&p.Revision()==revision);
}
static void UnsupportedIsAtomic() {
  auto c=Catalog();
  // Enable is supported now, but a genuine reference change must still be
  // rejected by Player-only event dispatch without the native scene owner.
  // Make the original reference initially disabled so Enable is not a no-op.
  c.references[100].flags|=0x800;
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
    // PlaceAtMe is an original FO3 operation not yet in the supported
    // source subset. It must reject the whole event atomically.
    "Player.PlaceAtMe TestCharge 1\nendif\nEnd\n";
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

static void WorldAwareGameMode() {
  auto c=Catalog();
  c.pipboy.quests[20].flags|=1u; // Start-game-enabled original quest semantics.
  c.references[200].base=400;c.references[200].flags=0x800u;
  c.pipboy.formNames["targetref"]=200;
  c.pipboy.scripts[101].source="scn Q\nBegin GameMode\nTargetRef.Enable\nEnd";
  c.pipboy.scripts[102].source="scn NPC\nBegin GameMode\nTargetRef.Disable\nEnd";
  c.eventPrograms[101]=fo3script::ParseEvents(c.pipboy.scripts[101]);
  c.eventPrograms[102]=fo3script::ParseEvents(c.pipboy.scripts[102]);
  fo3player::Player p(std::move(c));std::string error;bool disabled=false;
  const auto initial=p.Revision();
  std::unique_ptr<fo3player::ScriptTransactionPlan> pending;
  int reserved=0;
  auto reserve=[&](std::unique_ptr<fo3player::ScriptTransactionPlan> plan){
    ++reserved;pending=std::move(plan);return true;
  };
  assert(!p.PumpWorldAwareGameMode(.12,{100},reserve)&&reserved==0);
  assert(p.Revision()==initial);
  assert(p.PumpWorldAwareGameMode(.13,{100},reserve)&&reserved==1&&pending);
  assert(pending->HasReferenceChanges()&&pending->HasProgramProvenance());
  assert(pending->ExecutedScripts().size()==1);
  assert(pending->ExecutedScripts()[0].origin.record==101);
  assert(pending->ExecutedScripts()[0].owner==20);
  assert(p.ReferenceDisabled(200,disabled,error)&&disabled);
  assert(p.Revision()==initial); // Pending is not an early success.
  assert(p.CommitScriptTransaction(*pending));
  assert(p.ReferenceDisabled(200,disabled,error)&&!disabled);
  pending.reset();
  assert(p.PumpWorldAwareGameMode(.25,{100},reserve));
  assert(reserved==2&&pending&&pending->ExecutedScripts().size()==1);
  assert(pending->ExecutedScripts()[0].origin.record==102);
  assert(pending->ExecutedScripts()[0].owner==100);
  assert(p.ReferenceDisabled(200,disabled,error)&&!disabled);
  assert(p.CommitScriptTransaction(*pending));
  assert(p.ReferenceDisabled(200,disabled,error)&&disabled);
  pending.reset();
  // A refused resident reservation cannot publish any part of the script.
  const auto revision=p.Revision();
  auto reject=[](std::unique_ptr<fo3player::ScriptTransactionPlan>){return false;};
  assert(!p.PumpWorldAwareGameMode(.25,{100},reject));
  assert(p.Revision()==revision&&p.ReferenceDisabled(200,disabled,error)&&disabled);
  // Non-world scripts commit in the same tick without a resident ticket.
  auto normal=Catalog();normal.pipboy.quests[20].flags|=1u;
  normal.pipboy.scripts[101].source="Begin GameMode\nRewardKarma 4\nEnd";
  normal.eventPrograms[101]=fo3script::ParseEvents(normal.pipboy.scripts[101]);
  normal.pipboy.scripts[102].source="Begin OnDeath\nStartQuest ExampleQuest\nEnd";
  normal.eventPrograms[102]=fo3script::ParseEvents(normal.pipboy.scripts[102]);
  fo3player::Player q(std::move(normal));int calls=0;
  assert(!q.PumpWorldAwareGameMode(.25,{},[&](auto){++calls;return false;}));
  assert(q.Snapshot().karma==4&&calls==0);
  std::cout<<"World-aware GameMode gates original quest/reference world transactions without early publication\n";
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
  r.source="SetStage ExampleQuest 10\r\nPlayer.EquipItem TestCharge\r\n";
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
static void EnableParentQueries() {
  auto c=Catalog();c.references[200].flags=0x800;
  c.enableParents[100]={200,0,true};
  c.references[201].flags=0x800;c.enableParents[201]={200,1,true};
  c.references[202].flags=0x800;c.enableParents[202]={201,3,true};
  c.references[203].flags=0;c.enableParents[203]={200,2,true};
  c.references[204].flags=0x20;
  c.references[205].flags=0;c.enableParents[205]={999,0,true};
  c.references[206].flags=0;c.enableParents[206]={206,0,true};
  c.references[207].flags=0;c.enableParents[207]={200,4,true};
  c.references[208].flags=0;c.enableParents[208]={200,0,false};
  c.references[209].flags=0;c.enableParents[209]={0,0,true};
  c.references[210].flags=0;c.references[211].flags=0;
  c.enableParents[210]={211,1,true};c.enableParents[211]={210,1,true};
  for(uint32_t i=300;i<429;++i){c.references[i].flags=0;if(i<428)c.enableParents[i]={i+1,0,true};}
  fo3player::Player p(c);std::string error;bool disabled=false;const auto revision=p.Revision();
  for(auto id:{100u,200u,202u,203u})assert(p.ReferenceDisabled(id,disabled,error)&&disabled&&error.empty());
  assert(p.ReferenceDisabled(201,disabled,error)&&!disabled); // Child's own disabled flag is ignored.
  for(auto id:{0u,999u,204u,205u,206u,207u,208u,209u,210u,300u})
    assert(!p.ReferenceDisabled(id,disabled,error)&&!error.empty());
  assert(p.Revision()==revision&&p.Snapshot().pipboy.quests.empty());
  // CTDA run-on-reference works even when the Pip-Boy pruned this target.
  assert(!c.pipboy.targets.count(100));fo3dialogue::Context ctx;ctx.player=&p;
  ctx.speaker.reference=500;ctx.target.reference=fo3player::PlayerRef;
  fo3pipdata::Condition condition;condition.function=35;condition.run=2;condition.reference=100;condition.value=1;
  assert(fo3dialogue::Conditions({condition},ctx,error));condition.reference=201;
  assert(!fo3dialogue::Conditions({condition},ctx,error)&&error.empty());
  condition.reference=206;assert(!fo3dialogue::Conditions({condition},ctx,error)&&!error.empty());
  c.pipboy.formNames["childref"]=201;
  c.pipboy.scripts[102].source="Begin OnActivate\nif GetDisabled == 1 && childref.GetDisabled == 0\nStartQuest ExampleQuest\nendif\nEnd";
  c.eventPrograms[102]=fo3script::ParseEvents(c.pipboy.scripts[102]);
  fo3player::Player source(c);
  assert(source.DispatchReferenceEvent(100,"OnActivate",0x14,error)==fo3player::ScriptEventResult::Executed);
  assert(source.Snapshot().pipboy.quests.count(20));
  c.pipboy.formNames["childref"]=206;fo3player::Player bad(c);
  const auto before=bad.Revision();
  assert(bad.DispatchReferenceEvent(100,"OnActivate",0x14,error)==fo3player::ScriptEventResult::Unsupported);
  assert(bad.Revision()==before&&bad.Snapshot().pipboy.quests.empty());
}
static void PersistentReferenceState() {
  auto c=Catalog();c.references[200].flags=0x800;c.references[300].flags=0;
  c.enableParents[100]={200,1,true};c.references[201].flags=0;
  c.enableParents[201]={999,0,true};c.items[400].formId=400;
  c.pipboy.formNames["missingitem"]=400;
  c.references[200].base=400;c.references[300].base=500;c.containers[500].valid=true;
  fo3player::Player p(c);std::string error;bool disabled=false;
  const auto path="/tmp/fq-reference-state-"+std::to_string(getpid())+".fqps";
  auto read=[&](){std::ifstream in(path,std::ios::binary);return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),{});};
  auto u32=[](const std::vector<uint8_t>& b,size_t at){uint32_t n=0;for(unsigned i=0;i<4;++i)n|=uint32_t(b[at+i])<<(i*8);return n;};
  auto put=[](std::vector<uint8_t>& b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(n>>(i*8));};
  auto write=[&](std::vector<uint8_t> b){put(b,12,uint32_t(b.size()-20));put(b,16,uint32_t(crc32(0,b.data()+20,b.size()-20)));
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),b.size());assert(out);};
  assert(!p.CanPickup(200)&&p.CanOpenDoor(300)&&p.CanLootContainer(300));
  assert(p.CanPickLock(300)==fo3player::PickAccess::Unlocked);
  assert(p.Save(path,error));const auto legacy=read();assert(u32(legacy,4)==14);
  const auto revision=p.Revision();
  assert(p.StageReferenceEnable(200,false,error)&&p.Revision()==revision);
  assert(!p.StageReferenceEnable(999,true,error)&&p.Revision()==revision);
  assert(!p.StageReferenceEnable(201,true,error)&&p.Revision()==revision);
  assert(p.StageReferenceEnable(100,false,error)&&p.Revision()==revision); // Parent-controlled no-op.
  assert(p.StageReferenceEnable(200,true,error)&&p.StageReferenceEnable(300,false,error));
  assert(p.CanPickup(200)&&!p.CanOpenDoor(300)&&!p.CanLootContainer(300));
  assert(p.CanPickLock(300)==fo3player::PickAccess::Unsupported);
  auto immutable=p.CaptureReferenceState();
  assert(immutable.Enabled(200,false)&&!immutable.Enabled(300,true)&&!immutable.Enabled(100,true));
  assert(!immutable.Enabled(201,true)); // Invalid graph fails closed.
  assert(immutable.Enabled(987,true)&&!immutable.Enabled(987,false)); // Authored fallback.
  std::array<uint8_t,7> special;special.fill(10);std::array<uint8_t,14> skills;skills.fill(100);
  assert(p.SetPlayerAttributes(special,skills));
  const auto worldRevision=p.Snapshot().referenceStateRevision;
  assert(!p.ExecuteDialogueResult({"StartQuest ExampleQuest\nplayer.equipitem missingitem",{},{},{},{}},error));
  assert(p.Snapshot().referenceEnabled.size()==2&&p.Snapshot().referenceStateRevision==worldRevision&&p.Snapshot().pipboy.quests.empty());
  assert(p.PreviewDialogueResult({"StartQuest ExampleQuest",{},{},{},{}},error));
  assert(p.Snapshot().referenceEnabled.size()==2&&p.Snapshot().pipboy.quests.empty());
  assert(p.Save(path,error));const auto good=read();
  assert(good.size()==legacy.size()+43&&u32(good,legacy.size())==0x31454652&&u32(good,legacy.size()+4)==2);
  assert(p.StageReferenceEnable(200,false,error)&&p.StageReferenceEnable(300,true,error));
  assert(p.Snapshot().referenceEnabled.empty()&&immutable.Enabled(200,false));
  assert(p.Restore(path,error)&&p.Snapshot().referenceEnabled.size()==2);
  assert(p.Snapshot().special==special&&p.Snapshot().skills==skills);
  auto legacyOverlay=good;legacyOverlay.resize(legacyOverlay.size()-25);put(legacyOverlay,4,14);write(legacyOverlay);
  assert(p.Restore(path,error)&&p.Snapshot().referenceEnabled.size()==2&&p.Snapshot().special==c.initial.special);
  write(good);assert(p.Restore(path,error)&&p.Snapshot().special==special);
  assert(p.ReferenceDisabled(200,disabled,error)&&!disabled);
  assert(p.ReferenceDisabled(100,disabled,error)&&disabled);
  const auto before=p.Revision();const auto beforeState=p.Snapshot().referenceEnabled;
  const size_t ext=legacy.size();
  auto bad=good;bad[ext+12]=2;write(bad);assert(!p.Restore(path,error)); // Invalid bool.
  bad=good;put(bad,ext+13,u32(good,ext+8));write(bad);assert(!p.Restore(path,error)); // Duplicate ID.
  bad=good;put(bad,ext+8,999);write(bad);assert(!p.Restore(path,error)); // Unknown reference.
  bad=good;put(bad,ext+8,100);write(bad);assert(!p.Restore(path,error)); // Child override.
  bad=good;put(bad,ext+4,100001);write(bad);assert(!p.Restore(path,error)); // Oversized count.
  bad=good;bad.pop_back();write(bad);assert(!p.Restore(path,error)); // Truncation.
  bad=good;put(bad,ext,0);write(bad);assert(!p.Restore(path,error)); // Unknown extension.
  assert(p.Revision()==before&&p.Snapshot().referenceEnabled==beforeState);
  write(legacy);assert(p.Restore(path,error)&&p.Snapshot().referenceEnabled.empty());
  assert(p.ReferenceDisabled(200,disabled,error)&&disabled);
  assert(!p.CanPickup(200)&&p.CanLootContainer(300));
  std::remove(path.c_str());
}
static void ReferenceLocalBranches(){
  auto c=Catalog();const uint32_t target=0xfa000001u;
  c.pipboy.formNames["placedactor"]=target;c.references[target].valid=true;
  auto& script=c.pipboy.scripts[101];script.variables[7]="Listener";
  script.source="scn Q\nref Listener\nBegin GameMode\nEnd";
  fo3player::Player player(c);std::string error;
  fo3pipdata::ResultScript body;
  body.source="if 1\nset Listener to placedactor\nelse\nset Listener to 0\nendif";
  const auto key=(uint64_t(20)<<32)|7;
  assert(player.ExecuteDialogueResult(body,error,20));
  assert(player.Snapshot().pipboy.referenceVariables.at(key)==target);
  const auto path="/tmp/fq-ref-local-"+std::to_string(getpid());
  assert(player.Save(path,error));fo3player::Player restored(c);assert(restored.Restore(path,error));unlink(path.c_str());
  assert(restored.Snapshot().pipboy.referenceVariables.at(key)==target);
  body.source="set Listener to 0";
  assert(restored.ExecuteDialogueResult(body,error,20)&&!restored.Snapshot().pipboy.referenceVariables.count(key));
}
static void ResultBytecodeBranches(){
  auto catalog=Catalog();
  catalog.globals[400]=0;catalog.pipboy.formNames["testglobal"]=400;
  fo3player::Player player(catalog);std::string error;
  fo3pipdata::ResultScript body;
  body.references={20,400};
  auto& bytes=body.compiled;
  auto gate=[&](uint8_t op,uint8_t limit){
    const std::vector<uint8_t> data{op,0,19,0,1,0,15,0,' ','X',0x3a,0x10,5,0,1,0,'r',1,0,' ',limit,' ','<'};
    bytes.insert(bytes.end(),data.begin(),data.end());
  };
  auto assign=[&](uint8_t digit){
    bytes.insert(bytes.end(),{0x15,0,7,0,'G',2,0,2,0,' ',digit});
  };
  gate(0x16,'1');assign('3');gate(0x18,'5');assign('4');
  bytes.insert(bytes.end(),{0x17,0,2,0,1,0});assign('7');
  bytes.insert(bytes.end(),{0x19,0,0,0});
  float value=0;
  for(unsigned stage:{0u,2u,6u}){
    auto state=catalog;state.pipboy.quests[20].stages[stage]={};
    fo3player::Player p(std::move(state));
    assert(p.ExecuteQuestStage(20,stage,error)&&p.ExecuteDialogueResult(body,error));
    assert(p.GlobalValue(400,value)&&value==(stage==0?3:stage==2?4:7));
  }
  auto malformed=body;malformed.compiled.back()=1;
  assert(!player.ExecuteDialogueResult(malformed,error)&&player.GlobalValue(400,value)&&value==0);
  // The postfix stack computes (2+3)*4 even with source removed.
  auto arithmetic=body;
  arithmetic.compiled={0x15,0,15,0,'G',2,0,10,0,' ','2',' ','3',' ','+',' ','4',' ','*'};
  assert(player.ExecuteDialogueResult(arithmetic,error)&&player.GlobalValue(400,value)&&value==20);
  arithmetic.compiled.back()='?';
  assert(!player.ExecuteDialogueResult(arithmetic,error)&&player.GlobalValue(400,value)&&value==20);
  // The original SetGhost opcode on a global has no actor receiver.
  // Rejecting it rolls back the preceding global write.
  auto unavailable=body;unavailable.compiled.clear();
  unavailable.compiled={0x15,0,7,0,'G',2,0,2,0,' ','9',0x1c,0,2,0,0xec,0x10,7,0,1,0,'n',1,0,0,0};
  assert(!player.ExecuteDialogueResult(unavailable,error)&&!error.empty());
  assert(player.GlobalValue(400,value)&&value==20);
  // One skipped logical line includes both the receiver prefix and opcode.
  bytes.clear();gate(0x16,'0');
  bytes.insert(bytes.end(),{0x1c,0,2,0,0x22,0x10,2,0,0,0,0x19,0,0,0});
  std::vector<fo3dialogue::Command> commands;
  assert(fo3dialogue::CompileResult(catalog.pipboy,body,commands,error));
  assert(commands.size()==3&&commands[1].type==fo3dialogue::CommandType::Disable&&commands[1].form==400);
  assert(player.ExecuteDialogueResult(body,error));
}
static void AdditionalQuestPoints(){
  auto catalog=Catalog();catalog.initial.special.fill(4);
  fo3player::Player player(catalog);std::string error;
  fo3pipdata::ResultScript fragment;
  fragment.compiled={0x19,0x12,7,0,1,0,'n',1,0,0,0};
  assert(player.ExecuteDialogueResult(fragment,error,20));
  assert(player.SpecialBookRemaining()==1&&!player.AdjustSpecialBook(0,-1));
  auto path="/tmp/fq-fragment-points-"+std::to_string(getpid());
  assert(player.Save(path,error));
  fo3player::Player saved(catalog);assert(saved.Restore(path,error));
  assert(!saved.AdjustSpecialBook(0,-1)&&saved.AdjustSpecialBook(1,1)&&saved.ConfirmSpecialBook());
  assert(saved.Snapshot().special[1]==5&&saved.SetPlayerTagSkill(32,0));
  fragment.compiled[0]=0x1b;
  assert(saved.ExecuteDialogueResult(fragment,error,20)&&saved.TagSkillsRemaining()==1);
  assert(saved.Save(path,error)&&player.Restore(path,error));unlink(path.c_str());
  assert(!player.ToggleTagSkill(0)&&player.ToggleTagSkill(1)&&player.ConfirmTagSkills());
  assert(player.IsPlayerTagSkill(32)&&player.IsPlayerTagSkill(33));
}
static void CompareOriginalResultPrograms(const fo3pipdata::Definitions& defs){
  unsigned decoded=0,total=0;
  auto normalized=[](std::string expression){
    // Ignore source layout here. Grouping is exercised by the execution test.
    expression.erase(std::remove_if(expression.begin(),expression.end(),[](unsigned char c){
      return std::isspace(c)||c=='('||c==')';
    }),expression.end());return expression;
  };
  for(const auto& quest:defs.quests)for(const auto& stage:quest.second.stages)for(const auto& item:stage.second.items){
    if(item.result.compiled.empty())continue;
    ++total;
    std::string nativeText;
    assert(fo3script::StageDecoder(defs,item.result,quest.first).decode(nativeText));
    std::vector<fo3dialogue::Command> bytes,source;
    auto compiledOnly=item.result;compiledOnly.source.clear();
    std::string diagnostic;
    assert(fo3dialogue::CompileResult(defs,compiledOnly,bytes,diagnostic,quest.first,quest.second.script));
    if(std::any_of(bytes.begin(),bytes.end(),[](const auto& c){return c.type==fo3dialogue::CommandType::Unavailable;}))continue;
    ++decoded;
    auto text=item.result;text.compiled.clear();std::string error;
    // Three retail stages retain an argument in SCTX for the parameterless
    // RemoveScriptPackage opcode. Assert exactly what the compiler discarded.
    const std::string spelling="SammyRef.RemoveScriptPackage MQ07FetchSquirrel";
    if(auto at=text.source.find(spelling);at!=std::string::npos){
      text.source.replace(at,spelling.size(),"SammyRef.RemoveScriptPackage");
      const auto sammy=defs.formNames.at("sammyref");
      assert(std::any_of(bytes.begin(),bytes.end(),[&](const auto& c){return c.type==fo3dialogue::CommandType::ScriptPackage&&c.form==sammy&&!c.index&&!c.value;}));
    }
    assert(fo3dialogue::CompileResult(defs,text,source,error,quest.first,quest.second.script));
    assert(source.size()==bytes.size());
    for(size_t i=0;i<bytes.size();++i){
      const auto& a=bytes[i];const auto& b=source[i];
      assert(a.type==b.type&&a.form==b.form&&a.index==b.index&&a.variable==b.variable&&a.value==b.value&&a.offset==b.offset&&a.noTargetLook==b.noTargetLook);
      assert(normalized(a.expression)==normalized(b.expression));
    }
  }
  assert(total==825&&decoded>=803);
  std::cout<<decoded<<" / "<<total<<" original quest result programs matched source commands and operands\n";
}
// Replay original MQ01 result bytes with SCTX removed. The other quest's
// stage is controlled independently so both outcomes use the same program.
static void OriginalStageBytecode(const fo3player::Catalog& original){
  const auto& defs=original.pipboy;
  const auto mq=defs.formNames.at("mq01"),other=defs.formNames.at("mq03");
  auto fixture=Catalog();
  fixture.pipboy.formNames=defs.formNames;
  fixture.pipboy.markers=defs.markers;
  fixture.pipboy.quests[mq]=defs.quests.at(mq);
  auto& prerequisite=fixture.pipboy.quests[other];
  prerequisite.editor=defs.quests.at(other).editor;
  prerequisite.stages[9]={};prerequisite.stages[10]={};
  for(unsigned stage:{10u,20u,40u}){
    const auto& item=defs.quests.at(mq).stages.at(stage).items.at(0);
    auto bytes=item.result;bytes.source.clear();
    auto source=item.result;source.compiled.clear();
    for(unsigned before:{9u,10u}){
      fixture.pipboy.quests[mq].stages.at(stage).items.at(0).result=bytes;
      fo3player::Player actual(fixture);
      fixture.pipboy.quests[mq].stages.at(stage).items.at(0).result=source;
      fo3player::Player expected(fixture);std::string error;
      assert(actual.ExecuteQuestStage(other,before,error)&&expected.ExecuteQuestStage(other,before,error));
      assert(actual.ExecuteQuestStage(mq,stage,error)&&expected.ExecuteQuestStage(mq,stage,error));
      const auto& a=actual.Snapshot().pipboy;const auto& b=expected.Snapshot().pipboy;
      assert(a.revealed==b.revealed&&a.travelEnabled==b.travelEnabled);
      assert(a.quests.count(mq)==b.quests.count(mq));
      if(a.quests.count(mq)){
        const auto& objectives=a.quests.at(mq).objectives;
        assert(objectives.size()==b.quests.at(mq).objectives.size());
        assert((objectives.count(stage)&&objectives.at(stage).displayed)==(before==9));
        for(const auto& entry:objectives){
          const auto& rhs=b.quests.at(mq).objectives.at(entry.first);
          assert(entry.second.displayed==rhs.displayed&&entry.second.status==rhs.status);
        }
      }
    }
    fo3script::CompileCache cache(fixture.pipboy);
    assert(cache.Get(bytes,mq,0,false)->valid);
    auto invalid=bytes;invalid.compiled.push_back(0xff);
    assert(!cache.Get(invalid,mq,0,false)->valid);
    invalid=bytes;invalid.references.front()=0;
    assert(!cache.Get(invalid,mq,0,false)->valid);
    invalid=bytes;++invalid.compiled[4];
    assert(!cache.Get(invalid,mq,0,false)->valid);
  }
  std::cout<<"Six original MQ01 SCDA-only outcomes matched source execution\n";
}
static void OriginalBomb(const char* path) {
  fo3player::Catalog c;std::string error;assert(fo3player::LoadCatalog(path,c,error));
  CompareOriginalResultPrograms(c.pipboy);
  OriginalStageBytecode(c);
  assert(c.eventBaseScripts.at(0x3bc8)==0x78cb3);
  assert(c.enableParents.size()==5502);
  fo3player::Player graph(c);bool disabled=false;
  assert(graph.ReferenceDisabled(0x1e99f,disabled,error)&&disabled);
  assert(graph.ReferenceDisabled(0x4e325,disabled,error)&&disabled);
  assert(graph.ReferenceDisabled(0x37ef7,disabled,error)&&!disabled);
  assert(graph.ReferenceDisabled(0x14f77,disabled,error)&&!disabled);
  size_t checked=0;
  for(const auto& child:c.enableParents){
    if(!graph.ReferenceDisabled(child.first,disabled,error)){std::cerr<<error<<"\n";assert(false);}
    ++checked;
  }
  assert(checked==5502);
  const auto beforePlanRevision=graph.Revision();
  auto plan=graph.PrepareReferenceEnablePlan({{0x1e99f,true},{0x4e325,true}},error);
  assert(plan&&plan->HasChanges()&&graph.Revision()==beforePlanRevision);
  assert(graph.ReferenceDisabled(0x1e99f,disabled,error)&&disabled);
  const auto candidate=plan->CandidateSnapshot();
  assert(candidate->Enabled(0x1e99f,false)&&!candidate->Enabled(0x37ef7,true));
  assert(std::binary_search(plan->AffectedReferences().begin(),plan->AffectedReferences().end(),0x1e99fu));
  assert(std::binary_search(plan->AffectedReferences().begin(),plan->AffectedReferences().end(),0x37ef7u));
  assert(graph.CommitReferenceEnablePlan(*plan));
  for(const auto& child:c.enableParents){
    assert(graph.ReferenceDisabled(child.first,disabled,error));
    assert(candidate->Enabled(child.first,false)==!disabled);
  }
  assert(graph.RollbackReferenceEnablePlan(*plan));
  assert(graph.ReferenceDisabled(0x1e99f,disabled,error)&&disabled);
  std::cout<<"Original reference plan projects all 5502 XESP children without live staging; commit/rollback passed\n";

  assert(graph.StageReferenceEnable(0x1e99f,true,error));
  assert(graph.StageReferenceEnable(0x4e325,true,error));
  const auto snapshot=graph.CaptureReferenceState();
  assert(snapshot.Enabled(0x1e99f,false)&&!snapshot.Enabled(0x37ef7,true));
  const char* referenceSave="/tmp/fq-original-reference-enable.fqps";
  assert(graph.Save(referenceSave,error));
  assert(graph.StageReferenceEnable(0x1e99f,false,error));
  assert(graph.Restore(referenceSave,error)&&graph.ReferenceDisabled(0x1e99f,disabled,error)&&!disabled);
  assert(c.references.at(0x1e99f).flags&0x800);std::remove(referenceSave);
  std::cout<<"Original master enable graph: "<<checked<<" children resolved independently of residency\n";
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
  const auto burke=c.pipboy.formNames.at("misterburkeref");
  const auto beforeMove=rig.Snapshot().actors.at(burke);
  assert(rig.DispatchReferenceEvent(0x14bc8,"GameMode",0,error)==fo3player::ScriptEventResult::Unsupported);
  // MoveToMarker is supported now. The later Enable still requires the native
  // resident coordinator, so Player-only dispatch must roll back both the move
  // and the preceding inventory/quest/effect changes.
  assert(error.find("resident coordinator")!=std::string::npos);
  assert(rig.Snapshot().actors.at(burke).position==beforeMove.position&&!rig.ScriptActorMoveEpoch(burke));
  assert(rig.ItemCount(charge)==1&&rig.Snapshot().pipboy.quests.at(0x14e9e).stage==10);
  assert(!rig.PollScriptMessage(message)&&!rig.PollQuestNotice(notice));
  std::cout<<"Original ESM bomb activation, cancel, disarm, Karma, notices and persistence passed\n";
}
// Compile original event bodies without claiming execution compatibility.
static void OriginalCompileInventory(const char* path,const char* report) {
  fo3player::Catalog c;std::string error;assert(fo3player::LoadCatalog(path,c,error));
  fo3script::CompileCache cache(c.pipboy);size_t cacheChecks=0;
  size_t parsed=0,blocks=0,accepted=0;std::map<std::string,size_t> failures;
  for(const auto& entry:c.pipboy.scripts){
    auto program=fo3script::ParseEvents(entry.second);
    if(!program.error.empty()){++failures[program.error];continue;}
    ++parsed;
    for(const auto& block:program.blocks){
      ++blocks;fo3pipdata::ResultScript result;result.source=block.body;
      std::vector<fo3dialogue::Command> commands;
      for(const bool capture:{false,true}){
        fo3dialogue::CommandFormCapture forms;forms.limit=1024*1024;
        std::vector<fo3dialogue::Command> baseline;std::string baselineError;
        const bool valid=fo3dialogue::CompileResult(c.pipboy,result,baseline,baselineError,
            entry.first,entry.first,capture?&forms:nullptr);
        const auto cached=cache.Get(result,entry.first,entry.first,capture,capture?forms.limit:0);
        assert(cached->valid==valid&&cached->error==baselineError&&cached->commands.size()==baseline.size());
        for(size_t i=0;i<baseline.size();++i){const auto& a=baseline[i];const auto& b=cached->commands[i];
          assert(a.type==b.type&&a.form==b.form&&a.index==b.index&&a.variable==b.variable&&
              a.value==b.value&&a.expression==b.expression);
        }
        assert(cached->forms.bindings==forms.bindings&&cached->forms.variables==forms.variables&&
            cached->forms.locals==forms.locals&&cached->forms.globals==forms.globals&&
            cached->forms.used==forms.used&&cached->forms.exhausted==forms.exhausted);
        assert(cache.Get(result,entry.first,entry.first,capture,capture?forms.limit:0)==cached);
        ++cacheChecks;
      }
      if(fo3dialogue::CompileResult(c.pipboy,result,commands,error,entry.first,entry.first))++accepted;
      else {
        const auto colon=error.find(":");
        const auto key=error.rfind("unsupported command:",0)==0?error:error.substr(0,colon);
        ++failures[key];
      }
    }
  }
  auto quote=[](const std::string& s){std::string out="\"";for(unsigned char ch:s){
    if(ch=='\\'||ch=='\"'){out+='\\';out+=char(ch);}
    else if(ch=='\n')out+="\\n";else if(ch=='\r')out+="\\r";
    else if(ch=='\t')out+="\\t";else if(ch>=32)out+=char(ch);
  }return out+"\"";};
  std::ofstream out(report);assert(out);
  out<<"{\n  \"schema\": 1,\n  \"scpt_records\": "<<c.pipboy.scripts.size()
     <<",\n  \"event_programs_parsed\": "<<parsed<<",\n  \"event_blocks\": "<<blocks
     <<",\n  \"blocks_command_compiled\": "<<accepted
     <<",\n  \"executed_by_inventory\": 0,\n  \"gameplay_validated\": 0,\n  \"first_failures\": {";
  bool first=true;for(const auto& f:failures){if(!first)out<<",";first=false;out<<"\n    "<<quote(f.first)<<": "<<f.second;}
  out<<"\n  }\n}\n";out.close();assert(out);
  std::cout<<"SCPT corpus: "<<parsed<<" parsed programs, "<<accepted<<"/"<<blocks
           <<" command-compiled blocks; this is not execution coverage\n";
  std::cout<<"Original cache parity: "<<cacheChecks<<" captured/plain compiler outcomes and bindings match\n";
  fo3player::Player player(std::move(c));player.RegisterOriginalActors();
  player.SetScriptPlayerLocation(0,0,0,0xa96,0xa74);
  const auto started=std::chrono::steady_clock::now();
  for(unsigned i=0;i<32;++i)player.PumpWorldAwareGameMode(.25,{},[](auto){return false;});
  std::cout<<"Original GameMode: 32 ticks, "<<player.Snapshot().actors.size()<<" tracked actors, "
      <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<" host ms\n";
}
static void CompiledProgramsRemainLive() {
  auto c=Catalog();fo3script::CompileCache cache(c.pipboy);
  fo3pipdata::ResultScript body;body.source="SetStage ExampleQuest 10";
  const auto first=cache.Get(body,20,101,true,4096);
  assert(first->valid&&first->forms.bindings.size()==1);
  assert(cache.Get(body,20,101,true,4096)==first&&cache.hits==1);
  // Capture budgets, source text, implicit owner and local-script identity
  // are inputs, not interchangeable cache entries.
  assert(!cache.Get(body,20,101,true,0)->valid);
  body.source="Enable";
  assert(cache.Get(body,100,102,false)->commands[0].form==100);
  assert(cache.Get(body,200,102,false)->commands[0].form==200);
  const auto misses=cache.misses;cache.Get(body,200,103,false);assert(cache.misses==misses+1);
  body.source="UnsupportedOriginalCommand";
  const auto rejected=cache.Get(body,100,102,false);assert(!rejected->valid);
  assert(cache.Get(body,100,102,false)==rejected);
  body.source.clear();body.compiled={1};assert(!cache.Get(body,0,0,false)->valid);
  body.compiled.clear();assert(cache.Get(body,0,0,false)->valid);
  for(unsigned i=0;i<600;++i){
    body.source="RewardKarma 1 ; "+std::to_string(i);cache.Get(body,i,0,false);
    assert(cache.Size()<=cache.MaxEntries&&cache.retainedBytes<=cache.MaxBytes);
  }
  assert(first->valid&&first->forms.bindings[0].first==20); // Held across eviction.
  auto other=c.pipboy;other.formNames["examplequest"]=21;
  fo3script::CompileCache separate(other);body.source="SetStage ExampleQuest 10";
  assert(separate.Get(body,20,101,true,4096)->commands[0].form==21);

  c.pipboy.quests[20].flags|=1u;
  c.items[400].formId=400;c.items[400].kind=fo3player::ItemKind::Weapon;c.pipboy.formNames["existingitem"]=400;
  c.pipboy.scripts[101].source="Begin GameMode\nif GetStage ExampleQuest == 10\nRewardKarma 4\nendif\nEnd";
  c.eventPrograms[101]=fo3script::ParseEvents(c.pipboy.scripts[101]);
  fo3player::Player p(std::move(c));auto rejectWorld=[](auto){return false;};
  p.PumpWorldAwareGameMode(.25,{},rejectWorld);assert(p.Snapshot().karma==0);
  std::string error;assert(p.ExecuteQuestStage(20,10,error));
  p.PumpWorldAwareGameMode(.25,{},rejectWorld);assert(p.Snapshot().karma==4);
  p.PumpWorldAwareGameMode(.25,{},rejectWorld);assert(p.Snapshot().karma==8);
  // A later runtime failure still discards earlier commands on every attempt.
  fo3pipdata::ResultScript fail;fail.source="RewardKarma 7\nPlayer.EquipItem ExistingItem";
  const auto revision=p.Revision();assert(!p.ExecuteDialogueResult(fail,error));
  assert(!p.ExecuteDialogueResult(fail,error));
  assert(p.Snapshot().karma==8&&p.Revision()==revision);
  assert(p.Add(400,1));assert(p.ExecuteDialogueResult(fail,error));
  assert(p.Snapshot().karma==15&&p.ItemCount(400)==1&&p.EquippedWeapon());
  std::cout<<"Bounded compiled programs preserve live conditions, catalogue identity, evidence limits and atomic failure\n";
}
int main(int argc,char** argv){
  ResultBytecodeBranches();
  ReferenceLocalBranches();
  AdditionalQuestPoints();
  CompiledProgramsRemainLive();
  Parsing();Resurrection();DeathEventOrdering();GoreSettingAndBlastPresentation();ScriptedHealthDamage();EssentialKnockout();EquipmentInstanceEvents();WeaponFilteredHit();QuestCadence();UnenteredCommands();EnableParentQueries();PersistentReferenceState();Dispatch();UnsupportedIsAtomic();Filter();NumericLocalsAndRefSafety();BombMenuAndInventory();WorldAwareGameMode();DisarmStagePersistence();NoticeTransactions();
  if(argc>1)OriginalBomb(argv[1]);
  if(argc>2)OriginalCompileInventory(argv[1],argv[2]);
  std::cout<<"Original event source indexing, dispatch, scheduler and rollback passed\n";
}
