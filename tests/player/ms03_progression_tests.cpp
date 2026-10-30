#include "dialogue/fo3-dialogue-session.h"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace fo3player;
static void Check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
// Original dialogue/events and durable world projection. This fixture owns no
// renderer resources; its publication adapter commits the real reference plan.
// Physical CELL travel, mesh publication and audio are separate headset checks.
struct Conversation {
 Player& player;fo3dialogue::Session session;fo3dialogue::Context context;
 Conversation(Player& p,uint32_t ref):player(p){
  const auto& t=p.Definitions().pipboy.targets.at(ref);context.player=&p;
  context.speaker={ref,t.base,t.cell,t.world,0,t.x,t.y,t.z,false,false};
  context.target={PlayerRef,7,t.cell,t.world,0,t.x,t.y+40,t.z,false,false};
  p.SetScriptPlayerLocation(t.x,t.y+40,t.z,t.cell,t.world);
  session.previewReferences=[](const auto& p,const auto& plan,std::string&){return p.CanCommitScriptTransaction(plan);};
  session.publishReferences=[](auto& p,auto& plan,std::string&){return p.CommitScriptTransaction(plan);};
 }
 ~Conversation(){session.End("leave area");}
 void Finish(){unsigned count=0;while(session.phase==fo3dialogue::Phase::Speaking){Check(++count<100,"response loop");session.AudioDone(session.audioToken,true,context,player);}}
 void Start(){Check(session.Start(context,player),"Moira greeting: "+session.endReason);Finish();}
 bool Has(uint32_t topic)const{return std::any_of(session.choices.begin(),session.choices.end(),[&](const auto& c){return c.topic==topic;});}
 void Choose(uint32_t topic){
  const auto i=std::find_if(session.choices.begin(),session.choices.end(),[&](const auto& c){return c.topic==topic;});
  if(i==session.choices.end()){for(const auto& c:session.choices)std::cerr<<std::hex<<c.topic<<": "<<c.text<<"\n";}
  Check(i!=session.choices.end(),"missing original topic "+std::to_string(topic));
  Check(session.Choose(size_t(i-session.choices.begin()),context,player),"rejected topic "+session.endReason);Finish();
 }
};
static float Variable(const Player& p,const char* name){auto key=fo3dialogue::VariableKey(p.Definitions().pipboy,name);Check(key!=0,"authored variable exists");const auto i=p.Snapshot().pipboy.dialogueVariables.find(key);return i==p.Snapshot().pipboy.dialogueVariables.end()?0:i->second;}
static uint32_t Form(const Player& p,const char* name){return p.Definitions().pipboy.formNames.at(name);}
static void StartAssignment(Player& p,uint32_t answer=0x5ecc5){
 p.RegisterOriginalActors();Conversation moira(p,Form(p,"moirabrownref"));moira.Start();
 moira.Choose(0x5ecc0);moira.Choose(answer);
 Check(p.ItemCount(Form(p,"ms03vaultsuit101armored"))==1,"original vault-suit gift");
 bool disabled=false;std::string e;Check(p.ReferenceDisabled(Form(p,"ms03armoredvaultsuitref"),disabled,e)&&disabled,"display suit removed");
 moira.Choose(0x3eca7);moira.Choose(0x2d276);moira.Choose(0x2d279);moira.Choose(0x4ba22);
 const auto& q=p.Snapshot().pipboy.quests.at(Form(p,"ms03"));Check(q.stage==5&&q.running,"original assignment stage");
 Check(q.objectives.at(10).displayed&&q.objectives.at(12).displayed,"food and optional medicine shown");
 Check(Variable(p,"MS03.MS03a1")==1,"task accepted through dialogue");
 Check(p.Snapshot().pipboy.revealed.count(Form(p,"superdupermartmapmarkerref")),"authored ShowMap");
}
static uint32_t ContainerRef(const Player& p,const char* name){const auto base=Form(p,name);for(const auto& r:p.Definitions().references)if(r.second.base==base)return r.first;throw std::runtime_error("missing original container");}
static void Open(Player& p,uint32_t ref){
 const auto& c=p.Definitions();const auto cell=c.references.at(ref).cell;
 p.SetScriptPlayerLocation(0,0,0,cell,c.pipboy.cellWorlds.at(cell));
 Check(!p.CanLootContainer(ref),"scripted container cannot bypass its event");
 std::string e;auto plan=p.PrepareReferenceEventTransaction(ref,"OnActivate",PlayerRef,e);Check(bool(plan),"original OnActivate: "+e);
 Check(p.CommitScriptTransaction(*plan),"commit original activation");
 fo3script::Action action;Check(p.PollScriptAction(action)&&action.kind==fo3script::ActionKind::DefaultActivate&&action.reference==ref,"Activate delegates once");
 Check(!p.CanLootContainer(ref)&&p.OpenScriptContainer(ref)&&p.CanLootContainer(ref),"default action opens real loot inventory");
 const auto* rows=p.ContainerContents(ref);Check(rows&&!rows->empty(),"authored inventory contents");
 const auto row=rows->front();const auto before=p.ItemCount(row.formId);
 Check(p.TakeContainerStack(ref,row.id)&&p.ItemCount(row.formId)==before+row.count,"original loot transfers to player");
 p.EndContainerInteraction();Check(!p.CanLootContainer(ref),"leaving ends script delegation");
}
static void Roundtrip(Player& p,const Catalog& c,const std::function<void(Player&)>& next){
 const auto path="/tmp/fq-ms03-"+std::to_string(getpid());std::string e;
 Check(p.Save(path,e),e);Player loaded(c);Check(loaded.Restore(path,e),e);unlink(path.c_str());next(loaded);
}
static void Completion(Player& p,const Catalog& c,bool bonus,uint32_t response){
 const auto food=c.lootLists.at(Form(p,"ms03a1foodreward")).entries.front().form,sanitizer=Form(p,"ms03sanitizer"),note=Form(p,"ms03sanitizernote");
 const int before=p.ItemCount(food);const auto xp=p.Snapshot().experience;
 {Conversation moira(p,Form(p,"moirabrownref"));moira.Start();if(moira.Has(0x2d27e))moira.Choose(0x2d27e);moira.Choose(bonus?0x2d281:0x2d280);moira.Choose(response);}
 const auto& q=p.Snapshot().pipboy.quests.at(Form(p,"ms03"));Check(q.stage==(bonus?14:10)&&q.running,"task completion leaves whole guide running");
 Check(Variable(p,"MS03.MS03a1")==4&&q.objectives.at(16).status==fo3pipdata::Completion::Complete,"authored task turn-in");
 const auto& reward=c.lootLists.at(Form(p,"ms03a1foodreward"));int expected=0;
 for(const auto& row:reward.entries)if(row.level<=p.Snapshot().level)expected=row.count;

 Check(expected>0&&p.ItemCount(food)==before+expected,"original LVLI reward count at player level");
 Check(p.ItemCount(sanitizer)==int(bonus)&&p.ItemCount(note)==int(bonus),"bonus-only sanitizer and instruction note");
 Check(Variable(p,"MS03.bonusObjectives")==int(bonus)&&Variable(p,"MS03.MS03aBonus")==int(bonus),"bonus tally exactly once");
 Check(p.Snapshot().experience==xp,"no invented task XP");
 Check(bonus?q.objectives.at(12).status==fo3pipdata::Completion::Complete:!q.objectives.at(12).displayed,"optional objective completed or hidden per original stage");
 const bool snide=response==0x2d287;
 Check(Variable(p,snide?"MS03.snide":"MS03.standard")==1&&Variable(p,"MS03.currentStyle")==float(snide?4:0),"authored report style tallied once");
 const auto inventory=p.Snapshot().inventory;const auto count=p.ItemCount(food);
 Roundtrip(p,c,[&](Player& loaded){
  Check(loaded.Snapshot().inventory.size()==inventory.size()&&loaded.ItemCount(food)==count,"reward inventory persists");
  Conversation returned(loaded,Form(loaded,"moirabrownref"));returned.Start();
  Check(!returned.Has(0x2d27e)&&loaded.ItemCount(food)==count&&loaded.ItemCount(sanitizer)==int(bonus),"return greeting cannot repeat turn-in");
  Check(loaded.Snapshot().experience==xp&&loaded.ItemCount(Form(loaded,"ms03vaultsuit101armored"))==1,"no repeated XP or suit gift");
 });
}
static void OriginalIntercom(const Catalog& c){
 Player p(c);p.RegisterOriginalActors();std::string e;const uint32_t trigger=0x9e8b5;
 const auto& t=c.pipboy.targets.at(trigger);p.SetScriptPlayerLocation(t.x,t.y,t.z,t.cell,t.world);
 const auto linked=t.linkedReference;Check(linked!=0,"original reinforcement enable-parent link");
 bool disabled=false;Check(p.ReferenceDisabled(linked,disabled,e)&&disabled,"reinforcements initially disabled");
 auto plan=p.PrepareReferenceEventTransaction(trigger,"OnTriggerLeave",PlayerRef,e);Check(bool(plan),"intercom event: "+e);
 Check(p.CommitScriptTransaction(*plan),"intercom publication");
 Check(p.ReferenceDisabled(linked,disabled,e)&&!disabled,"original linked group enabled");
 fo3script::Action action;int lines=0;
 while(p.PollScriptAction(action)){
  Check(action.kind==fo3script::ActionKind::SayTo&&action.info,"original intercom INFO bound");
  fo3dialogue::Context context;context.player=&p;context.talking=true;
  const auto& at=c.pipboy.targets.at(action.reference);
  context.speaker={action.reference,at.base,at.cell,at.world,0,at.x,at.y,at.z,false,false};
  context.target={PlayerRef,7,t.cell,t.world,0,t.x,t.y,t.z,false,false};
  fo3dialogue::Session speech;
  Check(speech.StartSelectedTopic(action.form,action.info,context,p),"non-actor TACT starts authored speech");
  Check(!speech.Audio(c.pipboy).empty(),"original TACT voice path");++lines;
 }
 Check(lines==2,"both original intercom references speak");
 Check(p.DispatchReferenceEvent(trigger,"OnTriggerLeave",PlayerRef,e)==ScriptEventResult::Executed&&!p.PollScriptAction(action),"intercom DoOnce prevents replay");
 Roundtrip(p,c,[&](Player& loaded){
  Check(loaded.ReferenceDisabled(linked,disabled,e)&&!disabled,"reinforcement enable persists");
  Check(loaded.DispatchReferenceEvent(trigger,"OnTriggerLeave",PlayerRef,e)==ScriptEventResult::Executed&&!loaded.PollScriptAction(action),"saved trigger cannot repeat speech");
 });
}
static void Synthetic(){
 Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);c.items[1].formId=1;c.items[1].name="Reward";
 c.pipboy.formNames["reward"]=1;c.lootLists[2].entries={{1,0,3,1},{1,0,5,4}};c.lootLists[2].flags=2;c.pipboy.formNames["rewardlist"]=2;
 Player p(c);std::string e;fo3pipdata::ResultScript body;
 body.source="if(0)\nif player.GetAV FutureUnsupportedAV == 0\nRewardKarma 99\nendif\nelseif(1)\nplayer.AddItem RewardList 1\nelse\nRewardKarma 99\nendif";
 Check(p.ExecuteDialogueResult(body,e),e);Check(p.ItemCount(1)==3&&p.Snapshot().karma==0,"compact if/elseif and inactive nested condition");
 ScriptItemNotice notice;Check(p.PollScriptItemNotice(notice)&&notice.form==1&&notice.count==3&&!p.PollScriptItemNotice(notice),"LVLI notice names actual item");
 body.source="if(1)\nRewardKarma 2\nelseif player.GetAV FutureUnsupportedAV == 0\nRewardKarma 99\nendif";
 Check(p.ExecuteDialogueResult(body,e)&&p.Snapshot().karma==2,"taken branch does not evaluate ElseIf");
 const auto inventory=p.ItemCount(1);const auto karma=p.Snapshot().karma;body.source="player.AddItem RewardList 1\nUnsupportedEffect";
 Check(!p.ExecuteDialogueResult(body,e)&&p.ItemCount(1)==inventory&&p.Snapshot().karma==karma&&!p.PollScriptItemNotice(notice),"failed reward script publishes neither loot nor notice");
 c.initial.level=4;Player higher(c);body.source="player.AddItem RewardList 2";Check(higher.ExecuteDialogueResult(body,e)&&higher.ItemCount(1)==10,"same highest-level selection and per-count rules");
 body.source="if(0)\nif(1 + )\nRewardKarma 99\nendif\nendif";Check(!p.ExecuteDialogueResult(body,e)&&p.Snapshot().karma==karma,"malformed skipped expression is rejected");
 body.source="RewardKarma 99\nif player.GetAV FutureUnsupportedAV == 0\nendif";Check(!p.ExecuteDialogueResult(body,e)&&p.Snapshot().karma==karma,"entered unsupported query rolls back earlier effects");
 c.lootLists[2].entries={{2,0,1,1}};Player cycle(c);Check(!cycle.ExecuteDialogueResult(body,e)&&cycle.ItemCount(1)==0,"cyclic reward list rejected atomically");
}
static void ActorValueAndControlState(){
 Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);c.rules.apBase=100;
 Player p(c);float value=-1;std::string error;
 Check(p.ActorValue(PlayerRef,25,value)&&value==100&&p.ActorValue(PlayerRef,54,value)&&value==0,"original condition/radiation defaults");
 Check(p.ChangeActorValueDamage(PlayerRef,54,600,false)&&p.ChangeActorValueDamage(PlayerRef,25,100,false)&&p.DamageHealth(60),"shared damage modifiers");
 Check(p.HealthPercentage(PlayerRef)==.4f&&p.ActorValue(PlayerRef,25,value)&&value==0,"health percentage and crippled condition");
 fo3pipdata::ResultScript script;script.source="if player.getav radiationrads >= 600 && player.gethealthpercentage <= 0.5\nRewardKarma 1\nendif";
 Check(p.ExecuteDialogueResult(script,error)&&p.Snapshot().karma==1,"source reads canonical radiation and injury");
 script.source="player.RestoreAV RadiationRads 1000\nUnavailableEffect";Check(!p.ExecuteDialogueResult(script,error)&&p.Snapshot().pipboy.radiation==600,"unsupported healing result rolls back");
 script.source="DisablePlayerControls 1 0 1 0 0 0 0\nplayer.SetRestrained 1";Check(p.ExecuteDialogueResult(script,error),error);
 Check(p.PlayerControlDisabled(0)&&!p.PlayerControlDisabled(1)&&p.PlayerControlDisabled(2)&&!p.PlayerControlDisabled(6)&&p.Restrained(PlayerRef),"original seven control argument positions");
 Roundtrip(p,c,[&](Player& loaded){
  Check(loaded.Snapshot().pipboy.radiation==600&&loaded.ActorValue(PlayerRef,25,value)&&value==0&&loaded.PlayerControlDisabled(0)&&loaded.Restrained(PlayerRef),"mid-treatment modifiers/control/restraint persistence");
  fo3pipdata::ResultScript heal;heal.source="player.RestoreAV RadiationRads 1000\nplayer.ResetHealth\nEnablePlayerControls\nplayer.SetRestrained 0";Check(loaded.ExecuteDialogueResult(heal,error),error);
  Check(loaded.Health()==100&&loaded.ActorValue(PlayerRef,25,value)&&value==100&&loaded.Snapshot().pipboy.radiation==0&&!loaded.PlayerControlDisabled(0)&&!loaded.Restrained(PlayerRef),"shared treatment clears damage and restrictions");
 });
 Player lethal(c);Check(lethal.ChangeActorValueDamage(PlayerRef,54,1000,false)&&lethal.Health()==0,"lethal accumulated radiation");
 std::cout<<"Canonical actor values, source healing/control flags, rollback and mid-treatment save/reload passed\n";
}
static void OriginalKeys(const Catalog& c){
 Player p(c);const auto key=Form(p,"pharmacykey");const auto box=ContainerRef(p,"sdmmetalbox01commercial");
 const uint32_t door=0x4135b;Check(p.GetLock(door).locked&&!p.CanOpenDoor(door),"pharmacy door initially locked");
 Check(p.PrepareContainer(box),"original key container opens");
 const auto* rows=p.ContainerContents(box);const auto stack=std::find_if(rows->begin(),rows->end(),[&](const auto& s){return s.formId==key;});
 Check(stack!=rows->end()&&p.TakeContainerStack(box,stack->id)&&p.ItemCount(key)==1,"pharmacy key acquired from original box");
 Check(p.CanOpenDoor(door)&&p.UnlockByKey(door)&&!p.GetLock(door).locked&&p.ItemCount(key)==1,"original key opens pharmacy without consumption");
 Roundtrip(p,c,[&](Player& loaded){Check(!loaded.GetLock(door).locked&&loaded.ItemCount(key)==1&&loaded.CanOpenDoor(door),"key and unlocked door survive reload");});
}
static void Original(const char* path){
 Catalog c;std::string e;Check(LoadCatalog(path,c,e),e);OriginalKeys(c);OriginalIntercom(c);
 // All five original foreword answers, with no forced quest stages/variables.
 for(uint32_t answer:{0x5ecc5u,0x5ecc4u,0x5ecc3u,0x5ecc2u,0x5ecc1u}){
  Player p(c);StartAssignment(p,answer);
  if(answer!=0x5ecc5)continue;
  const auto food=ContainerRef(p,"ms03foodrefrigerator"),meds=ContainerRef(p,"ms03medicinekit");
  Check(c.references.at(food).cell==0x17f37&&c.references.at(meds).cell==0x17f37,"quest containers are in original Super-Duper Mart cell");
  Roundtrip(p,c,[&](Player& loaded){
   Open(loaded,food);Check(Variable(loaded,"MS03.MS03a1")==2,"food script advances task");
   Check(loaded.Snapshot().pipboy.quests.at(Form(loaded,"ms03")).objectives.at(10).status==fo3pipdata::Completion::Complete,"food objective complete");
   Roundtrip(loaded,c,[&](Player& basic){Completion(basic,c,false,0x2d869);});
   Open(loaded,meds);Check(Variable(loaded,"MS03.MS03a1")==3,"medicine script advances bonus");
   Roundtrip(loaded,c,[&](Player& bonus){Completion(bonus,c,true,0x2d868);});
  });
 }
 Player reverse(c);StartAssignment(reverse);Open(reverse,ContainerRef(reverse,"ms03medicinekit"));
 Check(Variable(reverse,"MS03.MS03a1")==1&&Variable(reverse,"MS03.MS03a1Meds")==1,"medicine first records bonus without food completion");
 Open(reverse,ContainerRef(reverse,"ms03foodrefrigerator"));Check(Variable(reverse,"MS03.MS03a1")==3,"food after medicine completes both");
 Completion(reverse,c,true,0x2d287);
 std::cout<<"Original MS03 foreword, assignment, pharmacy key, intercom/reinforcements, food-only and both collection orders, bonus, rewards, return and reload passed; physical traversal not simulated\n";
}
int main(int argc,char** argv){try{Synthetic();ActorValueAndControlState();if(argc<2){std::cout<<"Shared reward/control-flow tests passed; original MS03 requires Fallout3.esm\n";return 0;}Original(argv[1]);}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
