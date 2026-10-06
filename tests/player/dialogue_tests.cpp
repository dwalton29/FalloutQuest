#include "dialogue/fo3-dialogue-session.h"
#include <cassert>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include "dialogue/fo3-dialogue-panel.h"
using namespace fo3dialogue;
static void Original(const char* path) {
  fo3player::Catalog catalog;std::string error;
  assert(fo3player::LoadCatalog(path,catalog,error));
  auto& d=catalog.pipboy;
  assert(d.dialogueActors.at(0xa60).name=="Lucas Simms");
  assert(d.dialogueActors.at(0xa60).packages.size()==18);
  assert(d.dialogueTopics.at(0x3b80).text=="Nice town you got here, sheriff. It's a pleasure to meet you.");
  assert(d.dialogueTopics.at(0x3b7f).text=="Pffft. Nice hat, Calamity Jane.");
  assert(d.dialogueTopics.at(0x3b7e).text=="<Say nothing.>");
  assert(d.dialogueTopics.at(0x3b80).editor=="MS11LucasGreet1a"&&d.dialogueTopics.at(0x3b80).type==0);
  fo3player::Player player(std::move(catalog));
  Context ctx;ctx.player=&player;ctx.speaker.reference=0x3b46;ctx.speaker.base=0xa60;ctx.target.reference=0x14;ctx.target.base=7;
  Session s;size_t diagnostics=0;s.diagnostic=[&](const std::string& m){if(++diagnostics<8)std::cerr<<m<<'\n';};
  assert(s.Start(ctx,player));
  assert(player.Snapshot().pipboy.dialogueVariables.at((uint64_t(0x3b46)<<32)|2)==1);
  auto i=s.Current(player.Definitions().pipboy);assert(i);
  std::cout<<"GREETING info="<<std::hex<<i->id<<" quest="<<i->quest<<" voice="<<s.voice<<std::dec<<" responses="<<i->responses.size()<<" conditions="<<i->conditions.size()<<'\n';
  assert(i->responses[0].text.find("Name's Lucas Simms, town sheriff.")==0);
  assert(i->topic==0xc8&&i->quest==0x14e9e&&i->speaker==0&&i->conditions.size()==2);
  assert(i->responses[0].number==3&&i->responses[0].sound==0);
  assert(i->responses[0].emotion==0&&i->responses[0].emotionValue==50&&i->responses[0].flags==1);
  assert(!i->end.compiled.empty()&&i->end.references==std::vector<uint32_t>{0x3b31});
  assert(s.Audio(player.Definitions().pipboy)=="@voice:MaleUniqueSimms:_0003da20_3");
  assert(i->end.source=="set DialogueMegaton.LucasGreet to -1\r\nEnablePlayerControls");
  while(s.phase==Phase::Speaking)s.AudioDone(s.audioToken,true,ctx,player);
  assert(s.choices.size()==3);
  for(auto& c:s.choices)std::cout<<"CHOICE "<<std::hex<<c.topic<<" INFO "<<c.info<<std::dec<<" "<<c.text<<'\n';
  assert(s.choices[0].topic==0x3b80&&s.choices[1].topic==0x3b7f&&s.choices[2].topic==0x3b7e);
  assert(s.Choose(0,ctx,player));assert(s.info==0x3b82);
  while(s.phase==Phase::Speaking)s.AudioDone(s.audioToken,true,ctx,player);
  assert(s.choices.size()==3);
  assert(s.Choose(0,ctx,player));assert(s.info==0x3da07);
  while(s.phase==Phase::Speaking)s.AudioDone(s.audioToken,true,ctx,player);
  std::cout<<"AFTER acknowledgement eligible topics="<<s.choices.size()<<'\n';
  s.End("test B exit");assert(!s.Active());
  const auto save="/tmp/fq-dialogue-"+std::to_string(getpid());assert(player.Save(save,error));
  fo3player::Catalog original;assert(fo3player::LoadCatalog(path,original,error));fo3player::Player restored(std::move(original));assert(restored.Restore(save,error));
  assert(restored.Snapshot().pipboy.talkedActors.count(0x3b46));assert(!restored.Snapshot().pipboy.dialogueVariables.empty());std::remove(save.c_str());
  ctx.player=&restored;assert(s.Start(ctx,restored));assert(s.info!=0x3da20);s.End("repeat greeting checked");
}
static void Isolated() {
  fo3player::Catalog c;c.initial.baseHealth=100;c.initial.karma=50;c.initial.skills[11]=60;c.initial.special[0]=5;
  c.pipboy.dialogueActors[7].female=false;c.pipboy.dialogueActors[10].female=true;c.pipboy.quests[20]={};
  fo3player::Player p(std::move(c));Context ctx;ctx.player=&p;ctx.speaker={100,10};ctx.target={0x14,7};std::string error;
  fo3pipdata::Condition cond;cond.function=72;cond.a=10;cond.value=1;
  assert(Conditions({cond},ctx,error));cond.run=1;assert(!Conditions({cond},ctx,error));cond.a=7;assert(Conditions({cond},ctx,error));
  for(uint8_t op=0;op<6;++op)assert(Compare(2,2,uint8_t(op<<5))==(op==0||op==3||op==5));
  cond.function=14;cond.a=43;cond.flags=0x60;cond.value=60;assert(Conditions({cond},ctx,error));
  cond.a=23;cond.value=50;assert(Conditions({cond},ctx,error));
  cond.function=70;cond.a=0;cond.flags=0;cond.value=1;assert(Conditions({cond},ctx,error));
  auto fail=cond;fail.a=1;cond.flags=1;assert(Conditions({cond,fail},ctx,error));
  cond.flags=0;assert(!Conditions({cond,fail},ctx,error));
  cond.function=600;assert(!Conditions({cond},ctx,error)&&!error.empty());
  cond.function=58;cond.a=20;cond.value=0;assert(Conditions({cond},ctx,error));
  assert(p.StartQuest(20));cond.function=56;cond.value=1;assert(Conditions({cond},ctx,error));
  cond.function=136;cond.a=0x14;cond.value=1;cond.run=2;cond.reference=0x14;assert(Conditions({cond},ctx,error));
  cond.reference=999;assert(!Conditions({cond},ctx,error)&&!error.empty());
  cond.function=72;cond.a=10;cond.value=1;cond.run=0;cond.flags=1;assert(!Conditions({cond},ctx,error)&&!error.empty());

  fo3pipdata::ResultScript script;script.source="player.additem unknown 1";auto revision=p.Revision();assert(!p.ExecuteDialogueResult(script,error)&&p.Revision()==revision);
  Session session;assert(!session.Choose(0,ctx,p));assert(!session.AudioDone(5,true,ctx,p));
}

static void ConsequencesAndOrder() {
  fo3player::Catalog c;c.initial.baseHealth=100;c.pipboy.quests[20].stages[10]={};c.pipboy.quests[20].objectives[1]={};
  c.pipboy.formNames["quest"]=20;c.pipboy.formNames["item"]=30;c.items[30].name="Authored Item";
  c.pipboy.formNames["topic"]=40;c.pipboy.dialogueTopics[40].text="Authored Topic";
  c.pipboy.quests[20].script=21;c.pipboy.scripts[21].variables[1]="value";
  fo3player::Player p(c);std::string error;fo3pipdata::ResultScript script;
  script.source="StartQuest quest\nSetStage quest 10\nSetObjectiveDisplayed quest 1 1\nSetObjectiveCompleted quest 1 1\nplayer.AddItem item 2\nset quest.value to -1\nAddTopic topic";
  const auto revision=p.Revision();assert(p.ExecuteDialogueResult(script,error)&&p.Revision()>revision);
  assert(p.Snapshot().pipboy.quests.at(20).stage==10&&p.Snapshot().pipboy.quests.at(20).objectives.at(1).displayed);
  assert(p.Snapshot().pipboy.dialogueVariables.at((uint64_t(20)<<32)|1)==-1&&p.Snapshot().pipboy.knownTopics.count(40));
  assert(p.Snapshot().inventory.size()==1&&p.Snapshot().inventory[0].count==2);
  script.source="player.AddItem item 1\nSetStage quest 99";auto saved=p.Revision();assert(!p.ExecuteDialogueResult(script,error));assert(p.Revision()==saved&&p.Snapshot().inventory[0].count==2);
  script.source.clear();script.compiled={1};assert(!p.ExecuteDialogueResult(script,error));
  script.compiled.clear();script.source="CompleteQuest quest";assert(p.ExecuteDialogueResult(script,error));
  Context ctx;ctx.player=&p;ctx.speaker={100,10};ctx.target={0x14,7};fo3pipdata::Condition condition;condition.function=546;condition.a=20;condition.value=1;assert(Conditions({condition},ctx,error));
  fo3pipdata::Definitions d;d.dialogueTopics[40].type=0;
  fo3pipdata::Info a,b;a.id=1;a.topic=40;b.id=2;b.topic=40;b.previous=1;d.topics[40]={b,a};fo3pipdata::Finalize(d);
  assert(d.topics[40][0].id==1&&d.topics[40][1].id==2);
  d.dialogueActors[10].templateActor=11;d.dialogueActors[10].templateFlags=1;d.dialogueActors[11].voice=30;
  assert(fo3pipdata::ActorVoice(d,10)==30);d.dialogueActors[11].voice=0;d.dialogueActors[11].race=60;d.raceVoices[60]={31,32};
  assert(fo3pipdata::ActorVoice(d,10)==31);d.dialogueActors[11].female=true;assert(fo3pipdata::ActorVoice(d,10)==32);
  d.dialogueActors[11].templateActor=10;d.dialogueActors[11].templateFlags=1;assert(fo3pipdata::ActorVoice(d,10)==0);
}
static void Lifecycle() {
  fo3player::Catalog c;c.initial.baseHealth=100;
  c.pipboy.referenceScripts[100]={"",10};c.pipboy.referenceScripts[101]={"",10};c.pipboy.targets[100].base=10;c.pipboy.targets[101].base=10;c.weapons.actors[10].health=100;
  c.pipboy.dialogueActors[10].voice=30;c.pipboy.voices[30]="AuthoredVoice";
  c.pipboy.quests[20].flags=1;c.pipboy.greetings={40};
  fo3pipdata::Info greeting;greeting.id=50;greeting.topic=40;greeting.quest=20;greeting.flags=4;
  greeting.responses={{0,1,"First authored response"},{0,2,"Second authored response"}};greeting.links={41};
  c.pipboy.topics[40]={greeting};c.pipboy.dialogueTopics[41].text="Authored player response";
  auto goodbye=greeting;goodbye.id=51;goodbye.topic=41;goodbye.flags=1;goodbye.responses.resize(1);goodbye.links.clear();c.pipboy.topics[41]={goodbye};
  fo3player::Player p(std::move(c));Context ctx;ctx.player=&p;ctx.speaker={100,10};ctx.target={0x14,7};Session session;
  assert(session.Start(ctx,p));const auto token=session.audioToken;
  assert(!session.AudioDone(token-1,true,ctx,p)&&session.response==0);
  assert(!session.Choose(0,ctx,p));assert(session.AudioDone(token,true,ctx,p));
  assert(session.response==1&&session.phase==Phase::Speaking&&session.audioToken!=token);
  session.AudioDone(session.audioToken,true,ctx,p);
  assert(session.phase==Phase::Choices&&session.choices.size()==1);
  assert(p.Snapshot().pipboy.saidInfos.count((uint64_t(100)<<32)|50));
  assert(session.Choose(0,ctx,p));session.AudioDone(session.audioToken,true,ctx,p);
  assert(!session.Active()&&session.actor==0&&session.choices.empty());
  assert(!session.CanStart(ctx));ctx.speaker.reference=101;assert(session.Start(ctx,p));
  auto failedToken=session.audioToken;
  assert(session.AudioDone(failedToken,false,ctx,p));
  assert(session.Active()&&session.phase==Phase::Speaking&&session.response==1&&session.audioToken!=failedToken);
  session.AudioDone(session.audioToken,false,ctx,p);
  assert(session.Active()&&session.phase==Phase::Choices);
  assert(p.Snapshot().pipboy.saidInfos.count((uint64_t(101)<<32)|50));
  session.End("B");assert(!session.Active());
  fo3pipdata::SessionState encoded;std::vector<uint8_t> bytes;
  fo3pipdata::EncodeState(p.Snapshot().pipboy,bytes);std::string error;
  assert(fo3pipdata::DecodeState(encoded,p.Definitions().pipboy,bytes.data(),bytes.size(),error));
  assert(encoded.saidInfos==p.Snapshot().pipboy.saidInfos);
}
static void ResultConsequences() {
  fo3player::Catalog catalog;catalog.initial.baseHealth=100;
  catalog.pipboy.formNames["testquest"]=20;catalog.pipboy.quests[20].stages[10]={};
  catalog.pipboy.quests[20].objectives[1]={};catalog.pipboy.formNames["testitem"]=30;
  catalog.items[30].formId=30;catalog.items[30].name="Test item";
  fo3player::Player p(std::move(catalog));std::string error;fo3pipdata::ResultScript result;
  result.source="StartQuest TestQuest\nSetStage TestQuest 10\nSetObjectiveDisplayed TestQuest 1 1\nplayer.AddItem TestItem 2";
  assert(p.ExecuteDialogueResult(result,error));assert(p.Snapshot().pipboy.quests.at(20).stage==10);
  assert(p.Snapshot().pipboy.quests.at(20).objectives.at(1).displayed&&p.Snapshot().inventory.size()==1&&p.Snapshot().inventory[0].count==2);
  auto before=p.Revision();result.source="player.AddItem TestItem 2\nSetStage TestQuest 11";
  assert(!p.ExecuteDialogueResult(result,error)&&p.Revision()==before&&p.Snapshot().inventory[0].count==2);
  result.source.clear();result.compiled={1,2};assert(!p.ExecuteDialogueResult(result,error)&&p.Revision()==before);
}
static void ActivationEvent() {
  fo3player::Catalog c;c.pipboy.dialogueActors[10].script=20;
  c.pipboy.referenceScripts[100]={"actor",10};c.pipboy.scripts[20].variables[1]="Greet";
  c.pipboy.scripts[20].source="Begin GameMode\nuseWeapon unsupported\nEnd\nBegin OnActivate\nif Greet == 0\nif GetActionRef == player\nset Greet to 1\nendif\nendif\nactivate\nEnd";
  fo3player::Player p(c);Context ctx;ctx.player=&p;ctx.speaker={100,10};ctx.target={0x14,7};std::string error;
  assert(Activate(ctx,p,error));assert(p.Snapshot().pipboy.dialogueVariables.at((uint64_t(100)<<32)|1)==1);
  auto revision=p.Revision();assert(Activate(ctx,p,error)&&p.Revision()==revision);
  fo3player::Player other(c);ctx.player=&other;ctx.target.reference=101;assert(Activate(ctx,other,error));assert(other.Snapshot().pipboy.dialogueVariables.empty());
  c.pipboy.scripts[20].source="Begin OnActivate\nset Greet to 1\nuseWeapon unsupported\nEnd";
  fo3player::Player unsupported(c);ctx.player=&unsupported;ctx.target.reference=0x14;
  assert(!Activate(ctx,unsupported,error)&&unsupported.Snapshot().pipboy.dialogueVariables.empty());
}
static void PresentationAndOverride() {
  fo3font::Metrics font;font.baseLine=10;
  for(auto& g:font.glyphs){g.width=1;g.spacing=1;}
  const std::string longText="One authored sentence with a verylongwordandmore\nAnother line.";
  auto lines=Wrap(font,longText,12);assert(lines.size()>3);
  std::string joined;for(auto& line:lines)for(char c:line)if(c!=' '&&c!='\n')joined+=c;
  std::string original;for(char c:longText)if(c!=' '&&c!='\n')original+=c;assert(joined==original);
  assert(Wrap(font,"",12).empty());
  Panel panel;SmoothAnchor(panel,{1,2,3},.01,true);assert(panel.anchor[0]==1);
  SmoothAnchor(panel,{1.01f,2,3},.01,false);assert(panel.anchor[0]>1&&panel.anchor[0]<1.01f);
  fo3loot::Cursor cursor;cursor.Target(100,12);cursor.Scroll(-1,0,12);assert(cursor.selected==0);
  cursor.Scroll(0,0,12);cursor.Scroll(-1,.1,12);assert(cursor.selected==1);
  cursor.Scroll(-1,.2,12);assert(cursor.selected==1);cursor.Scroll(-1,.5,12);assert(cursor.selected==2);
  cursor.Target(100,0);assert(cursor.selected==0);
  fo3npc::RuntimeState npc;npc.activity=fo3npc::Activity::Package;npc.package=99;npc.speed=1;npc.yaw=.2f;npc.authoredYaw=.2f;
  npc.BeginDialogue();assert(npc.dialogue&&npc.speed==0&&npc.package==99);
  npc.Face(1.5f,.1f);assert(npc.animation==fo3npc::Animation::TurnLeft&&npc.yaw>.2f&&npc.yaw<.32f);
  npc.Face(npc.yaw+.1f,.1f);assert(npc.animation==fo3npc::Animation::Conversation&&npc.headYaw>0);
  npc.speaking=true;npc.Face(npc.yaw+.1f,.1f);assert(npc.animation==fo3npc::Animation::Speaking);
  npc.speaking=false;npc.Face(npc.yaw+.1f,.1f);assert(npc.animation==fo3npc::Animation::Conversation);
  npc.EndDialogue();assert(!npc.speaking);assert(!npc.dialogue&&npc.activity==fo3npc::Activity::Package&&npc.package==99&&npc.speed==1);
  assert(npc.authoredYaw==.2f);for(int n=0;n<20;++n)npc.Face(0,.1f);assert(std::fabs(npc.yaw-.2f)<.01f);
  npc.BeginDialogue();npc.activity=fo3npc::Activity::Combat;npc.EndDialogue();assert(npc.activity==fo3npc::Activity::Combat);
  assert(!fo3npc::Interrupted(false,true,true,false,4));
  assert(fo3npc::Interrupted(false,true,true,false,6));assert(fo3npc::Interrupted(false,true,false,false,2));
  assert(fo3npc::Interrupted(false,false,true,false,2));assert(fo3npc::Interrupted(true,true,true,false,2));assert(fo3npc::Interrupted(false,true,true,true,2));
}
int main(int argc,char** argv){Isolated();ConsequencesAndOrder();Lifecycle();ResultConsequences();ActivationEvent();PresentationAndOverride();if(argc>1)Original(argv[1]);
  if(argc>2){std::ifstream voice(std::string(argv[2])+"/ms11_greeting_0003da20_3.ogg",std::ios::binary);char header[4]{};voice.read(header,4);assert(std::string(header,4)=="OggS");
    std::ifstream lip(std::string(argv[2])+"/ms11_greeting_0003da20_3.lip",std::ios::binary);assert(lip&&lip.peek()==1);std::cout<<"Original Lucas voice and companion LIP exist\n";}
  std::cout<<"Dialogue tests passed\n";}
