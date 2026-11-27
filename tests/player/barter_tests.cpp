#include "player/fo3-player-state.h"
#include "dialogue/fo3-dialogue-results.h"
#include "dialogue/fo3-dialogue-session.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <cstring>
using namespace fo3player;
void Check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
Catalog Fixture(){
 Catalog c;c.initial.baseHealth=100;c.initial.special.fill(1);c.initial.skills.fill(0);
 c.fingerprint=17;c.worldFingerprint=18;c.lootFingerprint=19;
 c.barterSettings={{"fBarterBuyBase",1.55f},{"fBarterBuyMult",-.45f},{"fBarterSellBase",.45f},{"fBarterSellMult",.45f},{"fItemConditionValueBase",0},{"fItemConditionValueMult",.0316f}};
 Item caps;caps.formId=0xf;caps.name="Bottle Caps";caps.value=1;c.items[0xf]=caps;
 Item cup;cup.formId=100;cup.name="Cup";cup.value=100;c.items[100]=cup;
 Item gun=cup;gun.formId=101;gun.name="Gun";gun.kind=ItemKind::Weapon;gun.maxCondition=100;c.items[101]=gun;
 c.references[1].base=200;c.references[2].base=201;
 c.weapons.actors[200].health=100;
 c.pipboy.targets[1].base=200;c.pipboy.dialogueActors[200]={};
 c.merchantContainers[1]=2;c.merchantServices[200]=0x401;
 c.containers[201].entries={{100,0,10},{0xf,0,1000},{101,0,1,1,.5f}};
 c.pipboy.formNames["merchant"]=1;
 return c;
}
#include "dialogue/fo3-dialogue-panel.h"
#include "world/interaction/fo3-loot-cursor.h"
struct UiSession {fo3player::Player player;};
UiSession *gPlayerSession=nullptr;
fo3dialogue::Panel gDialoguePanel;
uint32_t gDialoguePanelInfo=0;
fo3dialogue::Phase gDialoguePanelPhase=fo3dialogue::Phase::Inactive;
int uiFlushes=0;
void FlushFo3PlayerState(){++uiFlushes;}
namespace fo3audio {void NamedSound(const std::string&){} void Scroll(){}}
#include "../../app/src/main/cpp/dialogue/fo3-barter-runtime.inc"
#include "../../app/src/main/cpp/dialogue/fo3-repair-runtime.inc"
void RepairServices() {
 auto c=Fixture();
 c.actorInventories[200]={};c.weapons.actors[200].skills[size_t(Skill::Repair)]=50;
 c.barterSettings["fRepairSkillMax"]=10;c.barterSettings["fItemRepairCostMult"]=2;
 Item armor=c.items.at(101);armor.kind=ItemKind::Armour;armor.formId=102;armor.name="Test armour";
 c.items[102]=armor;c.weapons.armourDR[102]=4;
 UiSession live{Player(std::move(c))};auto& p=live.player;gPlayerSession=&live;
 Check(p.Add(0xf,100)&&p.Add(102,2,.5f)&&p.Add(101,1,.3f),"repair inventory");
 fo3pipdata::ResultScript script;script.source="ShowRepairMenu 30";script.compiled={0x7a,0x11,0,0};
 std::string error;uint32_t actor=0;
 auto plan=p.PrepareDialogueTransaction(script,{},error,1);
 Check(plan&&!p.PollRepair(actor),"preview does not open repairs");
 Check(p.CommitScriptTransaction(*plan)&&p.RollbackScriptTransaction(*plan)&&!p.PollRepair(actor),"rollback removes repair request");
 Check(p.ExecuteDialogueResult(script,error,1)&&p.PollRepair(actor)&&actor==1,"compiled operand-free service");
 const auto before=uiFlushes;
 Check(RefreshFo3Repair(actor)&&gRepair.session.offers.size()==2,"repair UI rows");
 auto quote=gRepair.session;
 Check(std::abs(quote.target-.7f)<.00001f&&quote.offers.front().price==40&&quote.total==160,"per-copy and all-item quotes");
 UpdateFo3Repair(1,0,false,false,true,true);
 Check(p.ItemCount(0xf)==100&&!p.ContainerContents(1)&&uiFlushes==before,"unaffordable repair-all is atomic");
 UpdateFo3Repair(2,0,true,false,false,true);
 Check(p.ItemCount(0xf)==60&&uiFlushes==before+1&&gRepair.session.offers[0].count==1,"purchase repairs one copy");
 Check(p.ScriptItemCount(1,0xf)==40,"repairer receives caps personally");
 Check(!p.ConfirmRepair(quote,quote.offers.front().stack,error)&&p.ItemCount(0xf)==60,"expired quote cannot charge twice");
 Check(p.Add(0xf,60),"fund remaining repairs");
 UpdateFo3Repair(3,0,false,false,true,true);
 Check(gRepair.session.offers.empty()&&p.ItemCount(0xf)==0&&p.ScriptItemCount(1,0xf)==160,"repair all settles remaining copies");
 UpdateFo3Repair(4,0,false,false,true,true);
 Check(uiFlushes==before+2,"holding button does not repeat payment");
 UpdateFo3Repair(5,0,false,true,false,true);Check(!gRepair.panel.visible,"repair UI closes");
 Check(p.Save("/tmp/fq-repair-services.fqps",error),error.c_str());Player restored(p.Definitions());
 Check(restored.Restore("/tmp/fq-repair-services.fqps",error)&&restored.ScriptItemCount(1,0xf)==160&&
       restored.OpenRepair(1,quote)&&quote.offers.empty(),"paid repairs survive reload");
 auto bad=script;bad.compiled.clear();
 Check(!p.PreviewDialogueResult(bad,error,actor),"source-only repair rejects spurious argument");
 fo3pipdata::ResultScript self;self.source="Player.srm";
 Check(p.Add(101,1,.1f)&&p.ExecuteDialogueResult(self,error)&&p.PollRepair(actor)&&actor==PlayerRef&&
       p.OpenRepair(actor,quote)&&!p.ConfirmRepair(quote,quote.offers.front().stack,error),"self repair needs funds");
 Check(p.Add(0xf,500)&&p.ConfirmRepair(quote,quote.offers.front().stack,error)&&p.ItemCount(0xf)==500,"self repair retains caps");
 auto unavailable=Fixture();unavailable.actorInventories[200].valid=false;Player failed(std::move(unavailable));
 Check(failed.Add(101,1,.1f)&&failed.Add(0xf,200)&&failed.OpenRepair(1,quote),"unavailable repairer quote");
 Check(!failed.ConfirmRepair(quote,quote.offers.front().stack,error)&&failed.ItemCount(0xf)==200&&
       !failed.ContainerContents(1)&&failed.Snapshot().inventory.front().condition==.1f,"failed payment rolls back both inventories");
 gPlayerSession=nullptr;
}

void UiAdapter(){
 UiSession live{Player(Fixture())};gPlayerSession=&live;auto &p=live.player;
 Check(p.Add(0xf,1500)&&p.Add(100,5),"UI seed");gBarter={};
 Check(p.OpenBarter(1,0,gBarter.session),"UI merchant opens");gBarter.active=true;BarterBuildPanels();
 double now=1;
 auto input=[&](float horizontal,float left,float right,bool x,bool a,bool b){now+=.25;UpdateFo3Barter(now,0,horizontal,left,right,x,a,b,true);};
 input(0,1,0,false,false,false);Check(!gBarter.buying,"LT selects player");
 input(0,0,1,false,false,false);Check(gBarter.buying,"RT selects merchant");
 input(1,0,0,false,false,false);Check(gBarter.controls.filters[1]==0&&gBarter.rows.size()==1,"weapon category filters real rows");
 input(-1,0,0,false,false,false);Check(gBarter.controls.filters[1]==-1,"return to all");
 size_t cup=0;for(size_t i=0;i<gBarter.rows.size();++i)for(const auto &s:gBarter.session.merchant)if(s.id==gBarter.rows[i]&&s.formId==100)cup=i;
 gBarter.cursor.selected=cup;input(0,0,0,false,true,false);
 Check(gBarter.controls.Editing()&&gBarter.controls.quantity==10,"stack opens original quantity control with max");
 input(-1,0,0,false,false,false);Check(gBarter.controls.quantity==9,"stick moves amount slider");
 input(0,0,0,true,false,false);Check(gBarter.controls.Editing()&&p.ItemCount(100)==5,"X cannot bypass amount confirmation");
 input(0,0,0,false,false,true);Check(!gBarter.controls.Editing()&&gBarter.session.selections.empty(),"B cancels quantity without staging");
 input(0,0,0,false,true,false);for(int i=0;i<8;++i)input(-1,0,0,false,false,false);
 Check(gBarter.controls.quantity==2,"slider reaches desired count");input(0,0,0,false,true,false);
 Check(gBarter.session.selections.size()==1&&gBarter.session.selections[0].count==2,"A stages exact amount");
 input(0,0,0,true,false,false);Check(p.ItemCount(100)==7&&p.ItemCount(0xf)==1190&&uiFlushes==1,"X commits canonical trade once");
 input(0,0,0,true,false,false);Check(uiFlushes==1&&gBarter.status.empty(),"held X has no duplicate trade or success popup");
 input(0,0,0,false,false,true);Check(!gBarter.active,"B returns from barter");gPlayerSession=nullptr;
}
void Synthetic(){
 Player p(Fixture());Check(p.Add(0xf,1000)&&p.Add(100,5),"seed");
 BarterSession s;Check(p.OpenBarter(1,0,s),"open");
 auto gun=s.merchant.back();float price=0;
 Check(p.BarterPrice(gun,true,0,price)&&price==55,"condition price");
 Stack full=gun;full.condition=1;Check(p.BarterPrice(full,true,0,price)&&price==155,"full condition GMST precision");
 auto cup=s.merchant.front();Check(p.BarterPrice(cup,true,0,price)&&price==155,"buy zero skill");
 Check(p.BarterPrice(cup,false,0,price)&&price==45,"sell zero skill");
 Check(p.BarterPrice(cup,true,50,price)&&price==100,"discount bound");
 Check(p.BarterPrice(cup,false,50,price)&&price==68,"discount sell");
 s.selections={{cup.id,2,true},{s.player.back().id,1,false}};
 int64_t net=0;std::string e;Check(p.BarterBalance(s,net,e)&&net==265,"mixed arithmetic");
 Check(p.ItemCount(100)==5&&p.ItemCount(0xf)==1000,"preview unchanged");
 Check(p.ConfirmBarter(s,e)&&p.ItemCount(100)==6&&p.ItemCount(0xf)==735,"atomic commit");
 Check(!p.ConfirmBarter(s,e),"no repeat");
 Check(p.Save("/tmp/fq-barter.fqps",e),e.c_str());Player loaded(Fixture());
 Check(loaded.Restore("/tmp/fq-barter.fqps",e)&&loaded.ItemCount(100)==6&&loaded.ItemCount(0xf)==735,"persist player");
 BarterSession after;Check(loaded.OpenBarter(1,0,after)&&after.merchant.front().count==8,"persist merchant");
 s.selections={{s.merchant.front().id,1,true}};Check(p.Add(100,1)&&!p.ConfirmBarter(s,e)&&p.ItemCount(0xf)==735,"stale rejects");
 Check(p.OpenBarter(1,0,s),"reopen");s.selections={{s.merchant.front().id,999,true}};Check(!p.ConfirmBarter(s,e),"quantity rejects");
 Check(p.OpenBarter(1,0,s),"reopen");s.selections={{gun.id,1,true}};Check(p.ConfirmBarter(s,e),e.c_str());
 Check(p.Snapshot().inventory.back().formId==0xf,"caps normalization");
 bool preserved=false;for(const auto& x:p.Snapshot().inventory)if(x.id==gun.id)preserved=x.condition==.5f&&!x.equipped;
 Check(preserved,"weapon identity preserved");
 fo3pipdata::ResultScript script;script.source="ShowBarterMenu";
 Check(p.PreviewDialogueResult(script,e,1),"script eligible");BarterRequest request;
 Check(!p.PollBarter(request),"preview never opens");
 Check(p.ExecuteDialogueResult(script,e,1)&&p.PollBarter(request)&&request.actor==1,"commit queue");
 script.source="merchant.SBM -15";Check(p.ExecuteDialogueResult(script,e)&&p.PollBarter(request)&&request.discount==-15,"alias reference discount");
 for(auto text:{"SBM 101","SBM 1.5","SBM 0 extra"}){script.source=text;Check(!p.ExecuteDialogueResult(script,e,1)&&!p.PollBarter(request),"bad argument atomic");}
 script.source="SBM\nunsupported";Check(!p.ExecuteDialogueResult(script,e,1)&&!p.PollBarter(request),"later failure no menu");
}
void Safety(){
 Player p(Fixture());Check(p.Add(0xf,100)&&p.Add(100,50),"safety seed");
 BarterSession s;Check(p.OpenBarter(1,0,s),"safety open");std::string e;
 s.selections={{s.merchant.front().id,1,true}};const auto before=p.Snapshot().inventory;
 Check(!p.ConfirmBarter(s,e)&&p.ItemCount(0xf)==100&&p.ItemCount(100)==50,"insufficient player caps atomic");
 Check(p.OpenBarter(1,0,s),"safety reopen");s.selections={{s.player.back().id,50,false}};
 Check(!p.ConfirmBarter(s,e)&&p.ItemCount(0xf)==100,"insufficient merchant caps atomic");
 Check(p.OpenBarter(1,0,s),"safety reopen");const auto id=s.player.back().id;
 for(int quantity:{0,-1,51}){s.selections={{id,quantity,false}};Check(!p.ConfirmBarter(s,e),"invalid quantity");}
 s.selections={{id,1,false},{id,1,false}};Check(!p.ConfirmBarter(s,e),"duplicate selection");
 s.selections={{UINT64_MAX,1,true}};Check(!p.ConfirmBarter(s,e),"missing stack");
 Check(p.OpenBarter(1,0,s),"cancel reopen");s.selections={{id,1,false}};s={};
 Check(p.ItemCount(0xf)==100&&p.ItemCount(100)==50,"cancellation unchanged");
 Check(p.OpenBarter(1,0,s),"stale skill open");s.selections={{id,1,false}};
 auto special=p.Snapshot().special;auto skills=p.Snapshot().skills;skills[0]=100;
 Check(p.SetPlayerAttributes(special,skills)&&!p.ConfirmBarter(s,e),"stale quote rejected");
 auto c=Fixture();c.merchantContainers[1]=999;Player missing(c);Check(!missing.OpenBarter(1,0,s),"missing XMRC target");
 c=Fixture();c.references[1].flags|=0x800;Player disabled(c);Check(!disabled.OpenBarter(1,0,s),"disabled merchant");
 c=Fixture();c.merchantServices[200]=0;Player noService(c);Check(!noService.OpenBarter(1,0,s),"no merchant categories");
 c=Fixture();c.items[100].questItem=true;Player quest(c);Check(quest.OpenBarter(1,0,s)&&!quest.BarterItemAllowed(1,s.merchant.front()),"quest objects excluded");
 fo3pipdata::ResultScript script;script.source="SBM 10 + 10";BarterRequest request;
 Check(p.PreviewDialogueResult(script,e,1)&&!p.PollBarter(request),"discount expression preview private");
 Check(p.ExecuteDialogueResult(script,e,1)&&p.PollBarter(request)&&request.discount==20,"discount expression committed");
 TheftEvent theft;KarmaEvent karma;Check(!p.PollTheftEvent(theft)&&!p.PollKarmaEvent(karma),"legal trade no crime effects");
 Check(p.ModActorDisposition(1,10)&&p.Save("/tmp/fq-barter-disposition.fqps",e),"authored disposition save");
 Player restored(Fixture());Check(restored.Restore("/tmp/fq-barter-disposition.fqps",e)&&restored.Snapshot().actorDispositionModifiers.at(1)==10,"disposition restore");
}
void RestockAndModifiers(){
 auto c=Fixture();c.containers[201].respawns=true;c.references[1].cell=10;c.references[2].cell=11;c.gameTimeScale=3600;
 Player p(c);Check(p.Add(0xf,1000),"seed caps");p.SetScriptPlayerLocation(0,0,0,10,0);
 BarterSession s;Check(p.OpenBarter(1,0,s),"open restock");const auto cup=s.merchant.front().id;
 s.selections={{cup,1,true}};std::string e;Check(p.ConfirmBarter(s,e),"deplete");
 Check(p.OpenBarter(1,0,s)&&s.merchant.front().count==9,"reopen no reset");
 p.SetScriptPlayerLocation(0,0,0,20,0);
 for(int i=0;i<73;++i)Check(p.AdvanceGameClock(1),"advance one game hour");
 Check(p.Save("/tmp/fq-barter-restock.fqps",e),e.c_str());Player restored(c);
 Check(restored.Restore("/tmp/fq-barter-restock.fqps",e),e.c_str());
 restored.SetScriptPlayerLocation(0,0,0,10,0);
 Check(restored.OpenBarter(1,0,s)&&s.merchant.front().count==10,"72 hour unloaded respawn after reload");
 Check(restored.OpenBarter(1,0,s)&&s.merchant.front().count==10,"no duplicate restock");
 Check(restored.ItemCount(0xf)==845,"respawn never restores spent player caps");
 c.containers[201].respawns=false;Player stable(c);Check(stable.OpenBarter(1,0,s),"no respawn source");
 auto& perk=c.pipboy.perks[500];perk.ranks=1;fo3pipdata::PerkEffect effect;
 effect.type=2;effect.entryPoint=17;effect.function=3;effect.parameterType=1;
 float multiplier=.75f;effect.parameters.resize(4);std::memcpy(effect.parameters.data(),&multiplier,4);perk.effects={effect};
 c.initial.pipboy.perks[500]=1;Player discounted(c);float price=0;
 Stack item{1,100,1};Check(discounted.BarterPrice(item,true,0,price)&&price==116,"buy price original entry point");
 Check(discounted.BarterPrice(item,false,0,price)&&price==45,"perk never changes sell price");
 auto attrs=c.initial.special;auto skills=c.initial.skills;skills[0]=100;Check(discounted.SetPlayerAttributes(attrs,skills),"barter skill");
 Check(discounted.BarterPrice(item,true,0,price)&&price==100,"price lower bound after perk");
 Check(discounted.BarterPrice(item,false,0,price)&&price==90,"sell hundred skill");
}
void AuthoredServices(){
 auto c=Fixture();c.pipboy.packages[77].flags=0;
 c.pipboy.voices[90]="Voice";c.pipboy.dialogueActors[200].voice=90;c.pipboy.quests[9].flags=1;
 fo3pipdata::Info info;info.id=10;info.topic=8;info.quest=9;info.responses={{0,1,"Authored response"}};info.begin.source="ShowBarterMenu";
 c.pipboy.topics[8]={info};Player p(c);ActorState actor;actor.cell=11;actor.package=77;
 Check(p.UpdateActor(1,actor),"saved merchant nonservice package");
 fo3dialogue::Context ctx;ctx.player=&p;ctx.speaker={1,200,11,0,77,0,0,0,false,true};ctx.target={PlayerRef,PlayerBase,11};
 fo3dialogue::Session session;
 Check(session.Resolve(8,ctx)!=nullptr,"authored ShowBarterMenu survives ordinary nonservice package");
 fo3pipdata::Condition service;service.function=255;service.value=1;
 std::string error;Check(!fo3dialogue::Conditions({service},ctx,error)&&error.empty(),"original nonservice condition remains false");
 c.pipboy.topics[8][0].conditions={service};Player gated(c);Check(gated.UpdateActor(1,actor),"saved restricted merchant");ctx.player=&gated;
 Check(session.Resolve(8,ctx)==nullptr,"authored GetOffersServicesNow still gates topics");
 c.pipboy.packages[77].flags=1;Player offering(c);Check(offering.UpdateActor(1,actor),"saved offering merchant");ctx.player=&offering;
 Check(session.Resolve(8,ctx)!=nullptr,"authored service condition succeeds when offering");
 actor.hostile=PlayerRef;Check(offering.UpdateActor(1,actor)&&!offering.MerchantAvailable(1),"hostile merchant still blocked");
}
void ScriptMerchantContainers(){
 auto c=Fixture();
 c.gameTimeScale=3600;
 c.actorPlacements[1]=c.pipboy.targets.at(1);
 c.references[2].flags=0x400;c.references[2].cell=12;
 c.references[3].base=202;c.references[3].flags=0x400;c.references[3].cell=13;
 c.containers[202].entries={{100,0,2},{0xf,0,500}};
 c.containers[202].respawns=true;
 c.references[4]=c.references.at(3);c.references[4].flags=0;
 c.pipboy.formNames["otherstock"]=3;c.pipboy.formNames["temporary"]=4;
 Player p(c);Check(p.Add(0xf,1000),"seed trading caps");
 std::string error;
 BarterSession original,switched;
 Check(p.OpenBarter(1,0,original)&&original.container==2,"authored stock");
 Check(p.Save("/tmp/fq-before-stock-switch.fqps",error),"save earlier stock state");
 fo3pipdata::ResultScript script;
 script.source="Merchant.SetMerchantContainer OtherStock\nMerchant.SBM -5";
 auto plan=p.PrepareDialogueTransaction(script,{},error);
 Check(plan&&p.MerchantContainer(1)==2,"preview leaves stock alone");
 Check(p.CommitScriptTransaction(*plan)&&p.MerchantContainer(1)==3,"committed chest link");
 Check(p.RollbackScriptTransaction(*plan)&&p.MerchantContainer(1)==2,"rolled-back chest link");
 BarterRequest request;
 Check(!p.PollBarter(request),"rolled-back menu");
 Check(p.QueueBarter(1,0),"request original merchant menu");
 Check(p.ExecuteDialogueResult(script,error)&&p.PollBarter(request)&&request.discount==-5&&!p.PollBarter(request),"only replacement stock menu survives");
 original.selections={{original.merchant.front().id,1,true}};
 Check(!p.ConfirmBarter(original,error)&&p.ItemCount(0xf)==1000,"old quote cannot charge the wrong chest");
 Check(p.OpenBarter(request.actor,request.discount,switched)&&switched.container==3,"new chest stock");
 switched.selections={{switched.merchant.front().id,1,true}};
 Check(p.ConfirmBarter(switched,error)&&p.ScriptItemCount(3,100)==1&&p.ScriptItemCount(2,100)==10,"only selected stock depleted");
 script.source="SetMerchantContainer Temporary";
 Check(p.ExecuteDialogueResult(script,error,1)&&p.MerchantContainer(1)==3,"transient reference leaves link alone");
 script.source="RemoveMerchantContainer\nSetMerchantContainer Merchant";
 Check(!p.ExecuteDialogueResult(script,error,1)&&p.MerchantContainer(1)==3,"invalid target aborts script atomically");
 Check(p.Save("/tmp/fq-scripted-merchant.fqps",error),"save merchant link");
 Player saved(c);
 Check(saved.Restore("/tmp/fq-scripted-merchant.fqps",error)&&saved.OpenBarter(1,0,switched)&&
       switched.container==3&&switched.merchant.front().count==1,"restored link and stock");
 saved.SetScriptPlayerLocation(0,0,0,99,0);
 for(int i=0;i<73;++i)Check(saved.AdvanceGameClock(1),"advance restock clock");
 saved.SetScriptPlayerLocation(0,0,0,13,0);
 Check(saved.ScriptItemCount(3,100)==2,"scripted merchant stock respawns on return");
 script={};script.compiled={0xb8,0x11,0,0};
 Check(saved.ExecuteDialogueResult(script,error,1)&&!saved.MerchantContainer(1)&&
       !saved.OpenBarter(1,0,switched),"removed link cannot reopen old chest");
 Check(saved.Save("/tmp/fq-scripted-merchant.fqps",error)&&p.Restore("/tmp/fq-scripted-merchant.fqps",error)&&
       !p.MerchantContainer(1),"removed authored link survives reload");
 Check(p.Restore("/tmp/fq-before-stock-switch.fqps",error)&&p.MerchantContainer(1)==2,"older save retains the authored XMRC");
 c.merchantContainers.erase(1);
 c.pipboy.quests[80].script=81;
 auto& questScript=c.pipboy.scripts[81];
 questScript.source="scn ShopController\nref Seller\nref Stock\nBegin GameMode\nEnd";
 questScript.variables={{1,"Seller"},{2,"Stock"}};
 Player newVendor(c);
 script={};script.source="set Seller to Merchant\nset Stock to OtherStock\nSeller.SetMerchantContainer Stock\nSeller.SBM";
 Check(newVendor.ExecuteDialogueResult(script,error,80)&&newVendor.PollBarter(request)&&
       newVendor.OpenBarter(request.actor,request.discount,switched)&&switched.container==3,"script-local actor and chest establish a new merchant");
 script.source="set Stock to 0\nSeller.SetMerchantContainer Stock";
 Check(!newVendor.ExecuteDialogueResult(script,error,80)&&newVendor.MerchantContainer(1)==3,"unset reference argument cannot remove the current stock");
}

void OriginalMerchantSwitches(Catalog catalog){
 const auto& ids=catalog.pipboy.formNames;
 const auto leo=ids.at("leostahlref"),cutter=ids.at("cutterref");
 // The original drug-sale topic requires this quest local and the water plant.
 catalog.initial.pipboy.dialogueVariables[(uint64_t(0x3b31)<<32)|29]=1;
 Player p(std::move(catalog));p.RegisterOriginalActors();
 std::string error;
 auto trade=[&](uint32_t actor,uint32_t topic,uint32_t cell,uint32_t chest){
   fo3dialogue::Context ctx;ctx.player=&p;
   ctx.speaker={actor,p.Definitions().references.at(actor).base,cell};
   ctx.target={PlayerRef,PlayerBase,cell};
   fo3dialogue::Session conversation;conversation.random.seed(42);
   if(!conversation.StartTopic(topic,ctx,p))throw std::runtime_error("Original merchant topic unavailable: "+std::to_string(topic));
   for(unsigned steps=0;conversation.phase==fo3dialogue::Phase::Speaking;++steps){
     Check(steps<16,"finish merchant response");
     conversation.AudioDone(conversation.audioToken,true,ctx,p);
   }
   BarterRequest request;BarterSession quote;
   Check(p.PollBarter(request)&&request.actor==actor&&p.OpenBarter(actor,request.discount,quote)&&
         quote.container==chest,"dialogue opens the intended stock chest");
   return quote;
 };
 const auto& names=p.Definitions().pipboy.formNames;
 const auto drugs=names.at("vendorchestburiedleostahldrugsref"),diner=names.at("vendorchestburiedbrasslanternref");
 auto stock=trade(leo,names.at("megleobuydrugs"),0x3a34,drugs);
 const auto sale=std::find_if(stock.merchant.begin(),stock.merchant.end(),[&](const Stack& row){return p.BarterItemAllowed(leo,row);});
 Check(sale!=stock.merchant.end(),"original stock offers a tradeable item");
 const auto form=sale->formId;const auto before=p.ScriptItemCount(drugs,form);
 Check(p.Add(0xf,3000)&&p.OpenBarter(leo,0,stock),"seed customer purse");
 auto selected=std::find_if(stock.merchant.begin(),stock.merchant.end(),[&](const Stack& row){return row.formId==form;});
 Check(selected!=stock.merchant.end(),"item still available");
 stock.selections={{selected->id,1,true}};
 Check(p.ConfirmBarter(stock,error)&&p.ScriptItemCount(drugs,form)==before-1,"purchase removes one item from Leo's alternate chest");
 trade(leo,names.at("megleodrinks"),0x3a2f,diner);
 trade(leo,names.at("megleobuydrugs"),0x3a34,drugs);
 Check(p.ScriptItemCount(drugs,form)==before-1,"switching stock does not refill it");
 const auto& defs=p.Definitions().pipboy;
 const auto [topic,index]=p.Definitions().dialogueInfos.at(0x200fb);
 auto begin=defs.topics.at(topic).at(index).begin,end=defs.topics.at(topic).at(index).end;
 begin.source.clear();end.source.clear();
 auto compiled=p.PrepareDialogueTransaction(begin,end,error,leo);
 BarterRequest request;
 Check(compiled&&p.CommitScriptTransaction(*compiled)&&p.PollBarter(request)&&request.actor==leo&&
       p.MerchantContainer(leo)==diner,"compiled-only original result sets stock and opens menu");
 const auto clinic=p.Definitions().references.at(cutter).cell;
 trade(cutter,names.at("pfcuttersupplies"),clinic,names.at("vendorchestburiedcutterref"));
 trade(cutter,names.at("pfcutterchems"),clinic,names.at("vendorchestburiedcutterchemsref"));
}

void Original(const char* path){
 Catalog c;std::string e;Check(LoadCatalog(path,c,e),e.c_str());
 OriginalMerchantSwitches(c);
 Check(c.merchantContainers.at(0x2d2bc)==0xb4207&&c.merchantContainers.at(0x3b3d)==0x20478,"original XMRC");
 Check(c.containers.at(c.references.at(0xb4207).base).respawns,"original respawn flag");
 Player p(std::move(c));p.RegisterOriginalActors();
 // Production daytime selection: Moira's original shop Sandbox, not the
 // vendor-follow interrupt chosen by the previous test's flag shortcut.
 auto moira=p.Snapshot().actors.at(0x2d2bc);moira.package=0x4153;
 Check(p.UpdateActor(0x2d2bc,moira),"original daytime shop package");
 Check(!(p.Definitions().pipboy.packages.at(0x4153).flags&1),"actual shop lacks Offers Services flag");
 for(auto actor:{0x2d2bcu,0x3b3du}){BarterSession s;Check(p.OpenBarter(actor,0,s),"original merchant opens");
  size_t eligible=0;for(const auto& x:s.merchant)if(p.BarterItemAllowed(actor,x))++eligible;
  std::cout<<"merchant "<<std::hex<<actor<<std::dec<<" stock="<<s.merchant.size()<<" eligible="<<eligible<<" caps=";
  int64_t caps=0;for(const auto& x:s.merchant)if(x.formId==0xf)caps+=x.count;std::cout<<caps<<"\n";
  Check(eligible>0,"original eligible stock");
  Check(!p.ContainerContents(actor),"equipment not prepared");
  fo3dialogue::Context ctx;ctx.player=&p;ctx.speaker.reference=actor;
  ctx.speaker.base=p.Definitions().references.at(actor).base;
  ctx.speaker.cell=p.Definitions().references.at(actor).cell;
  ctx.target.reference=PlayerRef;ctx.target.base=PlayerBase;ctx.target.cell=ctx.speaker.cell;
  const auto& npc=p.Definitions().pipboy.dialogueActors.at(ctx.speaker.base);
  if(actor==0x2d2bc){ctx.speaker.package=0x4153;ctx.speaker.packageKnown=true;}
  else for(auto id:npc.packages){const auto& pack=p.Definitions().pipboy.packages.at(id);
    if(pack.flags&1){ctx.speaker.package=id;ctx.speaker.packageKnown=true;break;}}
  fo3dialogue::Session dialogue;
  dialogue.diagnostic=[](const std::string&){};
  Check(dialogue.CanActivate(ctx)&&dialogue.Start(ctx,p),"original merchant greeting");
  for(int budget=0;dialogue.phase==fo3dialogue::Phase::Speaking;++budget){Check(budget<20,"voice response budget");dialogue.AudioDone(dialogue.audioToken,true,ctx,p);}
  bool reachedRepair=false;
  for(int attempt=0;attempt<5;++attempt){
    size_t trade=SIZE_MAX;
    for(size_t index=0;index<dialogue.choices.size();++index){const auto* info=dialogue.Resolve(dialogue.choices[index].topic,ctx);
      if(info&&(info->begin.source.find("BarterMenu")!=std::string::npos||info->end.source.find("BarterMenu")!=std::string::npos||
                info->begin.source.find("barterMenu")!=std::string::npos||info->end.source.find("barterMenu")!=std::string::npos))trade=index;
    }
    if(actor==0x2d2bc&&!reachedRepair){
      for(size_t index=0;index<dialogue.choices.size();++index){
        const auto* info=dialogue.Resolve(dialogue.choices[index].topic,ctx);
        if(!info)continue;
        const std::vector<uint8_t> repairCode{0x7a,0x11,0,0};
        if(info->begin.compiled!=repairCode&&info->end.compiled!=repairCode)continue;
        auto conversation=dialogue;
        Check(conversation.Choose(index,ctx,p),"Moira repair topic selectable");
        for(int budget=0;conversation.phase==fo3dialogue::Phase::Speaking;++budget){
          Check(budget<20,"repair voice budget");conversation.AudioDone(conversation.audioToken,true,ctx,p);
        }
        uint32_t repairer=0;RepairSession repair;
        Check(p.PollRepair(repairer)&&repairer==actor,"Moira's chosen INFO queues the service once");
        const auto pistol=p.Definitions().pipboy.formNames.at("weap10mmpistol");
        Check(p.Add(0xf,500)&&p.Add(pistol,1,.2f)&&p.OpenRepair(repairer,repair),"original repair quote");
        auto item=std::find_if(repair.offers.begin(),repair.offers.end(),[&](const RepairOffer& r){return r.form==pistol;});
        Check(item!=repair.offers.end(),"damaged original pistol listed");
        const auto paid=item->price;const auto caps=p.ItemCount(0xf);
        Check(p.ConfirmRepair(repair,item->stack,e)&&p.ItemCount(0xf)==caps-paid&&!p.PollRepair(repairer),"original service purchase");
        std::cout<<"Moira dialogue INFO "<<std::hex<<info->id<<std::dec<<" repaired a 10mm pistol to "<<repair.target*100<<"% for "<<paid<<" caps\n";
        reachedRepair=true;break;
      }
    }
    if(trade!=SIZE_MAX){Check(dialogue.Choose(trade,ctx,p),"original trade choice");
      for(int budget=0;dialogue.phase==fo3dialogue::Phase::Speaking;++budget){Check(budget<20,"trade voice budget");dialogue.AudioDone(dialogue.audioToken,true,ctx,p);}
      BarterRequest request;Check(p.PollBarter(request)&&request.actor==actor,"authored result activates barter");
      Check(p.OpenBarter(actor,request.discount,s),"authored barter opens");
      std::cout<<"authored trade actor="<<std::hex<<actor<<std::dec<<" discount="<<request.discount<<"\n";break;
    }
    // Original first greetings introduce Moira's book/Gob's appearance. Follow
    // a supported original neutral response, then reenter the authored greeting.
    if(dialogue.choices.size()>1){Check(dialogue.Choose(1,ctx,p),"authored introductory response");
      for(int budget=0;dialogue.phase==fo3dialogue::Phase::Speaking;++budget){Check(budget<20,"intro voice budget");dialogue.AudioDone(dialogue.audioToken,true,ctx,p);}}
    if(attempt==4)throw std::runtime_error("Original trading topic not reached");
    dialogue.End("test reentry");Check(dialogue.Start(ctx,p),"original reentry");
    for(int budget=0;dialogue.phase==fo3dialogue::Phase::Speaking;++budget){Check(budget<20,"reentry voice budget");dialogue.AudioDone(dialogue.audioToken,true,ctx,p);}
  }

  if(actor==0x2d2bc)Check(reachedRepair,"Moira repair offer reached through normal dialogue");
  fo3pipdata::ResultScript result;result.source="ShowBarterMenu";Check(p.PreviewDialogueResult(result,e,actor),e.c_str());
  Check(p.Add(0xf,2000),"test player currency");
  const Stack* offer=nullptr;for(const auto& item:s.merchant)if(p.BarterItemAllowed(actor,item)){offer=&item;break;}
  Check(offer!=nullptr&&p.Add(offer->formId,2,offer->condition),"original test sale item");
  const uint64_t merchantId=offer->id;const uint32_t form=offer->formId;const float condition=offer->condition;
  Check(p.OpenBarter(actor,s.discount,s),"refresh test seed");
  uint64_t playerId=0;for(const auto& item:s.player)if(item.formId==form&&item.condition==condition)playerId=item.id;
  Check(playerId!=0,"sale stack found");s.selections={{merchantId,1,true},{playerId,1,false}};
  int64_t net=0;Check(p.BarterBalance(s,net,e),e.c_str());const int32_t before=p.ItemCount(0xf);
  Check(p.ConfirmBarter(s,e)&&p.ItemCount(0xf)==before-net,"original mixed transaction");
  Check(p.Save("/tmp/fq-original-barter.fqps",e),e.c_str());Player restored(p.Definitions());
  Check(restored.Restore("/tmp/fq-original-barter.fqps",e)&&restored.ItemCount(0xf)==p.ItemCount(0xf),"original barter restore");
  BarterSession persisted;Check(restored.OpenBarter(actor,s.discount,persisted)&&persisted.merchant.size()==s.merchant.size(),"original merchant restore");
 }
}
int main(int argc,char** argv){try{UiAdapter();RepairServices();Synthetic();ScriptMerchantContainers();Safety();RestockAndModifiers();AuthoredServices();if(argc>1)Original(argv[1]);std::cout<<"barter tests passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
