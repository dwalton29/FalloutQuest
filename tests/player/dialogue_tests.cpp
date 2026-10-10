#include "dialogue/fo3-dialogue-session.h"
#include <cassert>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include "dialogue/fo3-dialogue-panel.h"
#include "npc/fo3-npc-state.h"
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
  auto choose=[&](uint32_t topic) {
    auto choice=std::find_if(s.choices.begin(),s.choices.end(),[&](const auto& v){return v.topic==topic;});
    if(choice==s.choices.end()){std::cerr<<"Missing quest choice "<<std::hex<<topic<<std::dec<<" available:";for(auto v:s.choices)std::cerr<<" "<<v.text;std::cerr<<'\n';assert(false);}
    assert(s.Choose(size_t(choice-s.choices.begin()),ctx,player));
    while(s.phase==Phase::Speaking)s.AudioDone(s.audioToken,true,ctx,player);
  };
  choose(0x3b77);choose(0x1e35d);choose(0x3d9e0);choose(0x3d9dd);choose(0x1e355);
  assert(player.Snapshot().pipboy.quests.at(0x14e9e).objectives.at(10).displayed);
  std::cout<<"Full original Lucas dialogue acceptance path passed\n";
  s.End("test B exit");assert(!s.Active());
  const auto save="/tmp/fq-dialogue-"+std::to_string(getpid());assert(player.Save(save,error));
  fo3player::Catalog original;assert(fo3player::LoadCatalog(path,original,error));fo3player::Player restored(std::move(original));assert(restored.Restore(save,error));
  assert(restored.Snapshot().pipboy.talkedActors.count(0x3b46));assert(!restored.Snapshot().pipboy.dialogueVariables.empty());std::remove(save.c_str());
  ctx.player=&restored;
  fo3npc::RuntimeState actor;actor.reference=0x3b46;actor.activity=fo3npc::Activity::Package;actor.package=0x55471;actor.packageKnown=true;
  ctx.speaker.package=actor.package;ctx.speaker.packageKnown=true;ctx.speaker.cell=0xa96;ctx.speaker.world=0xa74;
  for(int repeat=0;repeat<3;++repeat) {
    assert(s.CanStart(ctx));assert(s.Start(ctx,restored));assert(s.info!=0x3da20);
    actor.BeginDialogue();assert(actor.dialogue);const auto stale=s.audioToken;
    assert(!s.Start(ctx,restored)); // An active session cannot be replaced.
    while(s.phase==Phase::Speaking)s.AudioDone(s.audioToken,true,ctx,restored);
    s.End("repeat original conversation complete");actor.EndDialogue();
    assert(!actor.dialogue&&!actor.speaking&&actor.activity==fo3npc::Activity::Package&&actor.package==0x55471);
    assert(!s.Active()&&s.choices.empty()&&s.info==0&&s.response==0);
    assert(!s.AudioDone(stale,true,ctx,restored));
    assert(restored.Snapshot().pipboy.talkedActors.count(0x3b46));
  }
  actor.BeginCombat(0x14);actor.EndCombat();assert(actor.activity==fo3npc::Activity::Package);
  assert(s.Start(ctx,restored));s.End("reentry after nonlethal combat");
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
  cond.function=72;cond.a=10;cond.value=1;cond.run=0;cond.flags=1;assert(Conditions({cond},ctx,error)&&error.empty());

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
  c.initial.baseHealth=100;c.pipboy.targets[100].base=10;c.weapons.actors[10].health=100;c.pipboy.greetings={40};c.pipboy.quests[30].priority=50;c.pipboy.quests[30].flags=1;
  fo3pipdata::Info greeting;greeting.id=50;greeting.topic=40;greeting.quest=30;greeting.responses={{1,1,"Activation greeting"}};
  fo3pipdata::Condition gate;gate.function=53;gate.a=100;gate.b=1;gate.value=1;greeting.conditions={gate};c.pipboy.topics[40]={greeting};
  fo3player::Player p(c);Context ctx;ctx.player=&p;ctx.speaker={100,10};ctx.target={0x14,7};std::string error;
  Session session;assert(!session.CanStart(ctx));auto previewRevision=p.Revision();
  assert(session.CanActivate(ctx)&&p.Revision()==previewRevision&&p.Snapshot().pipboy.dialogueVariables.empty());
  assert(session.Start(ctx,p));session.End("activation regression");
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
static void QuestParser() {
  std::vector<uint8_t> bytes;
  auto sub=[&](const char* tag,std::vector<uint8_t> payload){bytes.insert(bytes.end(),tag,tag+4);bytes.push_back(uint8_t(payload.size()));bytes.push_back(uint8_t(payload.size()>>8));bytes.insert(bytes.end(),payload.begin(),payload.end());};
  sub("EDID",{'q',0});sub("DATA",{8,50});sub("INDX",{10,0});sub("QSDT",{0});sub("CNAM",{'a',0});
  sub("SCHR",std::vector<uint8_t>(20));sub("SCDA",{1,2});sub("SCTX",{'s','e','t',0});sub("SCRO",{1,0,0,0});
  sub("QSDT",{1});std::vector<uint8_t> condition(28);condition[8]=58;condition[12]=1;sub("CTDA",condition);sub("CNAM",{'b',0});
  sub("QOBJ",{1,0,0,0});sub("NNAM",{'o',0});sub("QSTA",{4,0,0,0,1,2,3,4});sub("CTDA",condition);
  sub("QSTA",{5,0,0,0,0,0,0,0});
  fo3pipdata::Definitions d;fo3pipdata::Decode(d,"QUST",1,0,bytes,0,0,0);
  const auto& q=d.quests.at(1);assert(q.diagnostics.empty()&&q.stageOrder==std::vector<uint16_t>{10});
  const auto& items=q.stages.at(10).items;assert(items.size()==2&&items[0].log=="a"&&items[1].log=="b");
  assert(items[0].result.compiled==std::vector<uint8_t>({1,2})&&items[0].result.references==std::vector<uint32_t>{1});
  assert(items[0].conditions.empty()&&items[1].conditions.size()==1&&items[1].flags==1);
  const auto& targets=q.objectives.at(1).targetItems;assert(targets.size()==2&&targets[0].conditions.size()==1&&targets[1].conditions.empty());
  sub("QSDT",{});fo3pipdata::Decode(d,"QUST",2,0,bytes,0,0,0);assert(!d.quests.at(2).diagnostics.empty());
  fo3player::Catalog c;c.pipboy=d;fo3player::Player p(c);std::string error;assert(!p.ExecuteQuestStage(2,10,error)&&!error.empty());
}
static void QuestFoundation() {
  fo3player::Catalog c;c.initial.baseHealth=100;
  c.pipboy.formNames={{"q",1},{"other",2},{"global",3}};c.globals[3]=5;
  auto& q=c.pipboy.quests[1];q.editor="q";q.script=10;q.objectives[1].text="authored objective";
  c.pipboy.scripts[10].variables[1]="value";
  fo3pipdata::StageItem first;first.log="authored journal";
  first.result.source="set q.value to ( global + 2 )\nSetStage other 20";
  q.stages[10].items={first};
  fo3pipdata::StageItem second;second.result.source="SetObjectiveDisplayed q 1 1";
  fo3pipdata::Condition cond;cond.function=79;cond.a=1;cond.b=1;cond.value=7;second.conditions={cond};
  q.stages[10].items.push_back(second);
  c.pipboy.quests[2].stages[20].items.push_back({});
  c.pipboy.quests[2].stages[20].items[0].result.source="if GetStageDone q 10 == 1 && GetQuestRunning q == 1\nset global to 9\nelse\nset global to 100\nendif";
  fo3player::Player p(c);std::string error;
  assert(p.ExecuteQuestStage(1,10,error));
  assert(p.Snapshot().pipboy.quests.at(1).objectives.at(1).displayed);
  assert(p.Snapshot().pipboy.quests.at(1).journal.size()==1);
  float global;assert(p.GlobalValue(3,global)&&global==9);
  auto revision=p.Revision();assert(p.ExecuteQuestStage(1,10,error)&&p.Revision()==revision);
  assert(p.StopQuest(1));assert(!p.Snapshot().pipboy.quests.at(1).running);
  assert(p.FinishQuest(1,fo3pipdata::Completion::Complete));assert(!p.Snapshot().pipboy.quests.at(1).running);
  fo3pipdata::ResultScript completed;completed.source="CompleteQuest q\nSetObjectiveDisplayed q 1 0\nSetObjectiveCompleted q 1 1";
  assert(p.ExecuteDialogueResult(completed,error)&&!p.Snapshot().pipboy.quests.at(1).running);
  completed.source="SetObjectiveDisplayed q 1 1";assert(p.ExecuteDialogueResult(completed,error)&&p.Snapshot().pipboy.quests.at(1).running);assert(p.StopQuest(1));
  std::vector<uint8_t> encoded;fo3pipdata::EncodeState(p.Snapshot().pipboy,encoded);
  fo3pipdata::SessionState restored;assert(fo3pipdata::DecodeState(restored,c.pipboy,encoded.data(),encoded.size(),error));
  assert(!restored.quests.at(1).running&&restored.quests.at(1).journal.size()==1&&restored.mutableGlobals.at(3)==9);
  // Read the previous extension format without resetting existing objectives/variables.
  const std::vector<uint8_t> tag{0x31,0x54,0x53,0x51};
  auto marker=std::search(encoded.begin(),encoded.end(),tag.begin(),tag.end());assert(marker!=encoded.end());
  assert(fo3pipdata::DecodeState(restored,c.pipboy,encoded.data(),size_t(marker-encoded.begin()),error));
  assert(restored.quests.at(1).objectives.at(1).displayed&&!restored.dialogueVariables.empty());
  q.flags|=8;q.stages[10].items[0].result.source="SetStage q 10";
  fo3player::Player recursive(c);assert(!recursive.ExecuteQuestStage(1,10,error)&&recursive.Snapshot().pipboy.quests.empty());
  q.stages[10].items[0].result.source="set q.value to 4\nunsupportedCommand";
  fo3player::Player unsupported(c);assert(!unsupported.ExecuteQuestStage(1,10,error)&&unsupported.Snapshot().pipboy.dialogueVariables.empty());
  q.stages[10].items[0].result.source.clear();q.stages[10].items[0].result.compiled={1};
  fo3player::Player compiled(c);assert(!compiled.ExecuteQuestStage(1,10,error)&&compiled.Snapshot().pipboy.quests.empty());
  fo3pipdata::ResultScript bad;bad.source="if 1 == 1\nset global to 2";
  assert(!p.ExecuteDialogueResult(bad,error)&&p.GlobalValue(3,global)&&global==9);
  bad.source="SetStage missing 10";assert(!p.ExecuteDialogueResult(bad,error));
  std::cout<<"Quest transaction, conditions, nesting, journal, globals, stop/completion and rejection passed\n";
}
static void OriginalQuest(const char* path) {
  fo3player::Catalog c;std::string error;assert(fo3player::LoadCatalog(path,c,error));
  const auto& d=c.pipboy;
  assert(d.quests.at(0x14e9e).stages.at(10).items.size()==1);
  assert(d.quests.at(0xc0f66).stages.at(11).items.size()==15);
  assert(d.quests.at(0x14e9e).stages.at(60).items.size()==7);
  assert(d.quests.at(0x14e9e).diagnostics.empty());
  fo3player::Player p(c);
  const auto& infos=d.topics.at(0x1e355);
  const auto info=std::find_if(infos.begin(),infos.end(),[](const auto& i){return i.id==0x1e367;});assert(info!=infos.end());
  const auto revision=p.Revision();
  if(!p.PreviewDialogueResult(info->begin,error)||!p.PreviewDialogueResult(info->end,error)){std::cerr<<"MS11 PREVIEW "<<error<<'\n';assert(false);}
  assert(p.Revision()==revision&&p.Snapshot().pipboy.quests.empty());
  if(!p.ExecuteDialogueResult(info->begin,error)||!p.ExecuteDialogueResult(info->end,error)){std::cerr<<"MS11 EXECUTION "<<error<<'\n';assert(false);}
  const auto& q=p.Snapshot().pipboy.quests.at(0x14e9e);
  assert(q.stage==10&&q.stages.count(10)&&q.running);
  assert(q.objectives.at(10).displayed);
  assert(d.quests.at(0x14e9e).objectives.at(10).text=="Disarm Megaton's atomic bomb.");
  assert(d.quests.at(0x14e9e).objectives.at(10).targets==std::vector<uint32_t>{0x14bc8});
  assert(p.SelectQuest(0x14e9e));
  auto save="/tmp/fq-ms11-"+std::to_string(getpid());assert(p.Save(save,error));
  fo3player::Player restored(c);assert(restored.Restore(save,error));unlink(save.c_str());
  assert(restored.Snapshot().pipboy.quests.at(0x14e9e).objectives.at(10).displayed);
  assert(restored.Snapshot().pipboy.dialogueVariables==p.Snapshot().pipboy.dialogueVariables);
  assert(restored.ExecuteDialogueResult(info->end,error));
  assert(restored.Snapshot().pipboy.quests.at(0x14e9e).stage==10);
  std::cout<<"Original MS11 acceptance, nested MSObjectives, target and save/reload passed\n";
}
int main(int argc,char** argv){Isolated();ConsequencesAndOrder();Lifecycle();ResultConsequences();ActivationEvent();PresentationAndOverride();QuestParser();QuestFoundation();if(argc>1){Original(argv[1]);OriginalQuest(argv[1]);}
  if(argc>2){std::ifstream voice(std::string(argv[2])+"/ms11_greeting_0003da20_3.ogg",std::ios::binary);char header[4]{};voice.read(header,4);assert(std::string(header,4)=="OggS");
    std::ifstream lip(std::string(argv[2])+"/ms11_greeting_0003da20_3.lip",std::ios::binary);assert(lip&&lip.peek()==1);std::cout<<"Original Lucas voice and companion LIP exist\n";}
  std::cout<<"Dialogue tests passed\n";}
