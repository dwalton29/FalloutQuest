#include "player/fo3-new-game.h"
#include "scripting/fo3-script-master-match.h"
#include "ui/chargen/fo3-name-entry.h"
#include "ui/chargen/fo3-special-book.h"
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
static void OriginalIdentity(const fo3player::Catalog& catalog,bool female){
  fo3player::Player p(catalog);p.RegisterOriginalActors();std::string error;
  const auto& d=catalog.pipboy;const auto quest=d.formNames.at("cg00");
  auto boot=p.PrepareNewGameTransaction(quest,error);Check(bool(boot)&&p.CommitScriptTransaction(*boot),error);
  const auto start=p.Snapshot().playerMove;p.AcknowledgePlayerMove();
  p.SetScriptPlayerLocation(start.position[0],start.position[1],start.position[2],start.cell,start.world);
  const std::vector<uint32_t> residents={d.formNames.at("cg00dadref"),d.formNames.at("cg00momref"),d.formNames.at("cg00doctorliref")};
  auto registry=std::make_shared<fo3script::AnimationRegistry>();
  // Renderer-capability fixture, not a claim that the original NIF was loaded.
  (*registry)[d.formNames.at("cg00geneprojectorref")]={{"forward",1,false},{"backward",1,false}};p.SetScriptAnimationRegistry(registry);
  bool chose=false,genderLine=false,enteredName=false;
  unsigned modalTicks=0;
  for(unsigned frame=0;frame<1600;++frame){
    if(p.Snapshot().pipboy.quests.at(quest).stage==90)break;
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
    fo3script::Action action;
    while(p.PollScriptAction(action))if(action.kind==fo3script::ActionKind::SayTo){
      if(std::find(residents.begin(),residents.end(),action.reference)==residents.end())continue;
      fo3dialogue::Context ctx;ctx.player=&p;ctx.talking=true;
      Check(fo3dialogue::ReferenceContext(p,action.reference,ctx.speaker),"birth speaker context");
      Check(fo3dialogue::ReferenceContext(p,action.flags,ctx.target),"birth listener context");
      fo3dialogue::Session voice;
      voice.previewReferences=[](const auto&,const auto&,auto&){return true;};
      voice.publishReferences=[](auto& player,auto& plan,auto&){return player.CommitScriptTransaction(plan);};
      voice.diagnostic=[&](const std::string& message){error=message;};
      const bool selected=voice.StartSelectedTopic(action.form,action.info,ctx,p);
      Check(selected,"original birth INFO "+std::to_string(action.info)+" stage "+std::to_string(p.Snapshot().pipboy.quests.at(quest).stage)+": "+error);
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
  assert(chose&&genderLine&&enteredName&&p.PlayerFemale()==female&&!p.Snapshot().deferredAppearanceOwner);
  const auto& birth=p.Snapshot().pipboy.quests.at(quest);
  if(birth.stage!=90)std::cerr<<"Birth stopped at "<<birth.stage<<'\n';
  assert(birth.stage==90&&birth.stages.count(61)&&birth.stages.count(62)&&birth.stages.count(65)&&birth.stages.count(80));
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
  Synthetic();Identity();ScriptInventory();CharacterMenus();
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
  OriginalIdentity(catalog,false);OriginalIdentity(catalog,true);OriginalInventory(catalog);OriginalSpecialBook(catalog);
  std::cout<<"Original birth through deferred appearance to stage 90, both dialogue branches, SPECIAL book, inventory and save round trips passed\n";
}
