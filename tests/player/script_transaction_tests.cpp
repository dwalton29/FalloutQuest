#include "player/fo3-player-state.h"
#include "world/fo3-reference-exchange.h"
#include "world/fo3-resident-exchange.h"
#include "world/fo3-resident-workflow.h"
#include "scripting/fo3-script-master-match.h"
#include <array>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <new>
#include <unistd.h>

static bool forbidAllocations=false;
void* operator new(std::size_t size){
  if(forbidAllocations)throw std::bad_alloc();
  if(void* p=std::malloc(size?size:1))return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
static fo3pipdata::ResultScript Script(const std::string& source){fo3pipdata::ResultScript s;s.source=source;return s;}
static fo3player::Catalog Fixture(){
  fo3player::Catalog c;c.initial.special.fill(1);c.initial.baseHealth=100;
  c.items[400].formId=400;c.items[400].name="Fixture item";
  c.pipboy.formNames={{"questa",10},{"questb",11},{"testitem",400},{"testmessage",500},{"testsound",600}};
  c.pipboy.messages[500].text="Fixture message";c.pipboy.messages[500].buttons={"OK"};
  c.pipboy.quests[10].editor="QuestA";c.pipboy.quests[10].name="Fixture quest";
  c.pipboy.quests[10].objectives[1].text="Fixture objective";
  auto& a=c.pipboy.quests[10].stages[10].items.emplace_back();
  a.result=Script("player.AddItem TestItem 1\nSetStage QuestB 20\nRewardKarma 5\nShowMessage TestMessage\nPlaySound TestSound");
  a.log="Fixture journal";
  c.pipboy.quests[11].editor="QuestB";
  c.pipboy.quests[11].stages[20].items.emplace_back().result=
    Script("player.AddItem TestItem 2\nSetObjectiveDisplayed QuestA 1 1");
  c.references[100].base=200;c.eventBaseScripts[200]=300;
  c.pipboy.scripts[300].source="Begin OnActivate Player\nSetStage QuestA 10\nEnd\nBegin GameMode\nif GetButtonPressed == 1\nRewardKarma 7\nendif\nEnd";
  c.eventPrograms[300]=fo3script::ParseEvents(c.pipboy.scripts[300]);
  assert(c.eventPrograms[300].error.empty());return c;
}
static std::string Bytes(const std::string& path){
  std::ifstream in(path,std::ios::binary);assert(in);
  return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
static void NoEffects(fo3player::Player& p){
  uint32_t sound=0;fo3player::ScriptMessage message;fo3player::QuestNotice notice;fo3player::KarmaEvent karma;
  assert(!p.PollScriptSound(sound)&&!p.PollScriptMessage(message)&&!p.PollQuestNotice(notice)&&!p.PollKarmaEvent(karma));
}
static void Provenance(){
  using Kind=fo3player::ScriptProgramKind;
  constexpr auto capture=fo3player::ScriptProgramCapture::EnteredPrograms;
  std::string error;auto catalog=Fixture();
  catalog.pipboy.scripts[300].compiled={1,2,3};
  catalog.pipboy.scripts[300].header={4,5};
  catalog.pipboy.scripts[300].references={100,10};
  catalog.pipboy.scripts[300].records={{"SCRO",{100,0,0,0}},{"SCRV",{7,0,0,0}}};
  fo3player::Player p(std::move(catalog));const auto revision=p.Revision();
  auto quest=p.PrepareQuestStageTransaction(10,10,error,capture);assert(quest&&quest->HasProgramProvenance());
  const auto& entered=quest->ExecutedScripts();assert(entered.size()==2);
  assert(entered[0].origin.kind==Kind::QuestStageItem&&entered[0].origin.record==10);
  assert(entered[0].origin.stage==10&&entered[0].origin.item==0&&entered[0].owner==10);
  assert(entered[1].origin.kind==Kind::QuestStageItem&&entered[1].origin.record==11);
  assert(entered[1].origin.stage==20&&entered[1].origin.item==0&&entered[1].owner==11);
  assert(entered[0].parent==fo3player::ScriptExecution::NoParent&&entered[1].parent==0);
  assert(!entered[0].reference&&!entered[1].reference);
  assert(entered[0].program.source==p.Definitions().pipboy.quests.at(10).stages.at(10).items[0].result.source);
  auto event=p.PrepareReferenceEventTransaction(100,"OnActivate",fo3player::PlayerRef,error,capture);assert(event&&event->HasProgramProvenance());
  const auto& scripts=event->ExecutedScripts();assert(scripts.size()==3);
  const auto& first=scripts.front();
  assert(first.origin.kind==Kind::ReferenceEvent&&first.origin.record==300);
  assert(first.origin.event=="onactivate"&&first.origin.sourceLine==1);
  assert(first.owner==100&&first.activator==fo3player::PlayerRef&&first.localScript==300);
  assert(first.reference==100&&first.parent==fo3player::ScriptExecution::NoParent);
  const auto& original=p.Definitions().pipboy.scripts.at(300);
  assert(first.container.source==original.source&&first.container.header==original.header);
  assert(first.container.compiled==original.compiled&&first.container.references==original.references);
  assert(first.container.records==original.records);
  assert(first.program.source=="SetStage QuestA 10\n"&&first.program.compiled.empty());
  assert(scripts[1].origin.record==10&&scripts[2].origin.record==11);
  assert(scripts[1].activator==fo3player::PlayerRef&&scripts[2].activator==fo3player::PlayerRef);
  assert(scripts[1].reference==100&&scripts[2].reference==100);
  assert(scripts[1].parent==0&&scripts[2].parent==1);
  auto begin=Script("RewardKarma 1");begin.compiled={8,9};begin.header={10};
  begin.references={100};begin.records={{"SCRO",{100,0,0,0}}};
  auto dialogue=p.PrepareDialogueTransaction(begin,Script("RewardKarma 2"),error,100,capture);assert(dialogue&&dialogue->HasProgramProvenance());
  const auto& fragments=dialogue->ExecutedScripts();assert(fragments.size()==2);
  assert(fragments[0].origin.kind==Kind::UnboundResult&&fragments[1].origin.kind==Kind::UnboundResult);
  assert(!fragments[0].origin.record&&!fragments[1].origin.record);
  assert(fragments[0].origin.item==0&&fragments[1].origin.item==1);
  assert(fragments[0].owner==100&&fragments[1].owner==100);
  for(const auto& fragment:fragments)
    assert(fragment.parent==fo3player::ScriptExecution::NoParent&&!fragment.reference&&!fragment.activator);
  assert(fragments[0].program.compiled==begin.compiled&&fragments[0].program.records==begin.records);
  assert(fragments[0].program.header==begin.header&&fragments[0].program.references==begin.references);
  begin.source.clear();begin.compiled.clear();begin.records.clear();
  assert(fragments[0].program.source=="RewardKarma 1"&&fragments[0].program.compiled.size()==2);
  assert(p.Revision()==revision&&p.Snapshot().karma==0);NoEffects(p);
  const auto* identity=fragments.data();
  forbidAllocations=true;
  assert(p.CommitScriptTransaction(*dialogue)&&p.RollbackScriptTransaction(*dialogue));
  assert(dialogue->ExecutedScripts().data()==identity&&dialogue->ExecutedScripts().size()==2);
  forbidAllocations=false;
  assert(p.Snapshot().karma==0);NoEffects(p);
  auto nested=p.PrepareDialogueTransaction(Script("SetStage QuestA 10"),{},error,0,capture);assert(nested);
  assert(nested->ExecutedScripts().size()==4);
  assert(nested->ExecutedScripts()[0].origin.kind==Kind::UnboundResult);
  assert(nested->ExecutedScripts()[1].origin.record==10&&nested->ExecutedScripts()[2].origin.record==11);
  assert(nested->ExecutedScripts()[3].origin.kind==Kind::UnboundResult&&nested->ExecutedScripts()[3].origin.item==1);
  assert(nested->ExecutedScripts()[1].parent==0&&nested->ExecutedScripts()[2].parent==1);
  assert(nested->ExecutedScripts()[3].parent==fo3player::ScriptExecution::NoParent);
  assert(p.CommitScriptTransaction(*nested));
  auto repeated=p.PrepareQuestStageTransaction(10,10,error,capture);
  assert(repeated&&repeated->HasProgramProvenance()&&repeated->ExecutedScripts().empty());
  auto unaudited=p.PrepareDialogueTransaction({}, {},error);
  assert(unaudited&&!unaudited->HasProgramProvenance()&&unaudited->ExecutedScripts().empty());
  // Empty event blocks consume no command budget, but capture must still be
  // bounded. A failed capture leaves the original Player and later traces clean.
  auto many=Fixture();std::string source;
  for(unsigned n=0;n<4097;++n)source+="Begin OnActivate\nEnd\n";
  many.pipboy.scripts[300].source=source;
  many.eventPrograms[300]=fo3script::ParseEvents(many.pipboy.scripts[300]);
  fo3player::Player limited(many);const auto before=limited.Revision();
  assert(!limited.PrepareReferenceEventTransaction(100,"OnActivate",fo3player::PlayerRef,error,capture));
  assert(error.find("script provenance byte limit exceeded")!=std::string::npos&&limited.Revision()==before);
  auto uncaptured=limited.PrepareReferenceEventTransaction(100,"OnActivate",fo3player::PlayerRef,error);
  assert(uncaptured&&!uncaptured->HasProgramProvenance());NoEffects(limited);
  // Small stage fragments reach the independent entry-count bound without
  // repeated large SCPT containers exhausting the byte budget first.
  many=Fixture();many.pipboy.quests[10].stages[30].items.resize(4097);
  fo3player::Player tooMany(many);
  assert(!tooMany.PrepareQuestStageTransaction(10,30,error,capture));
  assert(error.find("script provenance entry limit exceeded")!=std::string::npos);
  NoEffects(tooMany);
  many.pipboy.quests[10].stages[30].items.resize(4096);
  fo3player::Player atLimit(many);
  auto bounded=atLimit.PrepareQuestStageTransaction(10,30,error,capture);
  assert(bounded&&bounded->ExecutedScripts().size()==4096);
  for(const auto& entry:bounded->ExecutedScripts())
    assert(entry.parent==fo3player::ScriptExecution::NoParent&&!entry.reference);
  // The byte guard runs before deep-copying oversized retained metadata. The
  // same scripts remain valid when evidence capture was not requested.
  auto large=Fixture();large.pipboy.quests[10].stages[10].items[0].result.header.resize(16u*1024u*1024u);
  fo3player::Player oversized(large);const auto initial=oversized.Revision();
  assert(!oversized.PrepareQuestStageTransaction(10,10,error,capture));
  assert(error.find("script provenance byte limit exceeded")!=std::string::npos&&oversized.Revision()==initial);
  assert(oversized.PrepareQuestStageTransaction(10,10,error));NoEffects(oversized);
}
static void DialogueProvenance(){
  using Phase=fo3player::DialogueScriptPhase;using Kind=fo3player::ScriptProgramKind;
  constexpr auto capture=fo3player::ScriptProgramCapture::EnteredPrograms;
  auto catalog=Fixture();fo3pipdata::Info info;info.id=801;info.topic=800;
  info.begin=Script("RewardKarma 1\nSetStage QuestA 10");info.end=Script("RewardKarma 2");
  info.begin.header={1,2};info.begin.compiled={3,4};info.begin.references={100,10};
  info.begin.records={{"SCRO",{100,0,0,0}},{"SCRV",{2,0,0,0}}};
  info.end.header={5};info.end.compiled={6};info.end.records={{"SCRO",{10,0,0,0}}};
  catalog.pipboy.topics[800]={info};
  catalog.dialogueInfos[801]={999,999}; // untrusted derived data must be rebuilt
  fo3player::Player p(catalog);std::string error;const auto revision=p.Revision();
  auto both=p.PrepareDialogueInfoTransaction(800,801,Phase::Both,error,100,capture);
  assert(both&&both->HasProgramProvenance());const auto& scripts=both->ExecutedScripts();
  assert(scripts.size()==4);
  for(size_t n:{size_t(0),size_t(3)}){
    const auto& script=scripts[n];const auto& authored=n?info.end:info.begin;
    assert(script.origin.kind==Kind::DialogueInfo&&script.origin.record==801&&script.origin.topic==800);
    assert(script.origin.item==(n?1u:0u)&&!script.origin.stage&&script.origin.event.empty());
    assert(script.owner==100&&!script.localScript);
    assert(script.parent==fo3player::ScriptExecution::NoParent&&!script.reference&&!script.activator);
    assert(script.program.source==authored.source&&script.program.header==authored.header);
    assert(script.program.compiled==authored.compiled&&script.program.references==authored.references);
    assert(script.program.records==authored.records);
  }
  assert(scripts[1].origin.kind==Kind::QuestStageItem&&scripts[1].origin.record==10);
  assert(scripts[2].origin.kind==Kind::QuestStageItem&&scripts[2].origin.record==11);
  assert(scripts[1].parent==0&&scripts[2].parent==1);
  assert(!scripts[1].reference&&!scripts[2].reference);
  auto begin=p.PrepareDialogueInfoTransaction(800,801,Phase::Begin,error,100,capture);
  auto end=p.PrepareDialogueInfoTransaction(800,801,Phase::End,error,100,capture);
  assert(begin&&begin->ExecutedScripts().size()==3);
  assert(end&&end->ExecutedScripts().size()==1&&end->ExecutedScripts()[0].origin.item==1);
  assert(p.Revision()==revision&&p.Snapshot().karma==0);NoEffects(p);
  const auto* storage=scripts.data();forbidAllocations=true;
  assert(p.CommitScriptTransaction(*both)&&p.Snapshot().karma==8);
  assert(p.RollbackScriptTransaction(*both)&&p.Snapshot().karma==0);
  assert(scripts.data()==storage&&scripts.size()==4);forbidAllocations=false;NoEffects(p);
  auto plain=p.PrepareDialogueInfoTransaction(800,801,Phase::End,error);
  assert(plain&&!plain->HasProgramProvenance()&&plain->ExecutedScripts().empty());
  for(auto ids:{std::make_pair(0u,801u),std::make_pair(800u,0u),std::make_pair(999u,801u),std::make_pair(800u,999u)})
    assert(!p.PrepareDialogueInfoTransaction(ids.first,ids.second,Phase::Both,error)&&!error.empty());
  assert(!p.PrepareDialogueInfoTransaction(800,801,static_cast<Phase>(255),error));
  // Copies can change/reorder vectors; Player reconstruction reindexes values,
  // never pointers or caller-provided ordinal claims.
  auto copied=p.Definitions();auto other=info;other.id=802;
  copied.pipboy.topics[800].insert(copied.pipboy.topics[800].begin(),other);
  copied.pipboy.topics[800][1].begin=Script("RewardKarma 9");
  fo3player::Player reordered(copied);
  auto fresh=reordered.PrepareDialogueInfoTransaction(800,801,Phase::Begin,error,0,capture);
  assert(fresh&&fresh->ExecutedScripts().size()==1&&fresh->ExecutedScripts()[0].program.source=="RewardKarma 9");
  assert(scripts[0].program.source==info.begin.source);
  for(int malformed=0;malformed<4;++malformed){
    auto bad=catalog;
    if(malformed==0)bad.pipboy.topics[800].push_back(info);
    if(malformed==1){auto duplicate=info;duplicate.topic=900;bad.pipboy.topics[900].push_back(duplicate);}
    if(malformed==2)bad.pipboy.topics[800][0].topic=900;
    if(malformed==3)bad.pipboy.topics[800][0].id=0;
    fo3player::Player invalid(std::move(bad));
    assert(!invalid.PrepareDialogueInfoTransaction(800,801,Phase::Both,error,0,capture));
    assert(invalid.Snapshot().karma==0);NoEffects(invalid);
  }
  auto failure=catalog;failure.pipboy.topics[800][0].end=Script("UnsupportedDialogueCommand");
  fo3player::Player rejected(std::move(failure));
  assert(!rejected.PrepareDialogueInfoTransaction(800,801,Phase::Both,error,0,capture));
  assert(rejected.Snapshot().karma==0&&rejected.Snapshot().pipboy.quests.empty());NoEffects(rejected);
}
static void OriginalProvenance(const char* path){
  constexpr auto capture=fo3player::ScriptProgramCapture::EnteredPrograms;
  fo3player::Catalog catalog;std::string error;assert(fo3player::LoadCatalog(path,catalog,error));
  uint32_t quest=0;
  for(const auto& definition:catalog.pipboy.quests)
    if(fo3script::Lower(definition.second.editor)=="ms11")quest=definition.first;
  assert(quest);
  fo3player::Player p(std::move(catalog));const auto revision=p.Revision();
  auto stage=p.PrepareQuestStageTransaction(quest,10,error,capture);
  if(!stage)std::cerr<<error<<'\n';
  assert(stage&&!stage->ExecutedScripts().empty());
  for(const auto& execution:stage->ExecutedScripts()){
    assert(execution.origin.kind==fo3player::ScriptProgramKind::QuestStageItem);
    const auto& authored=p.Definitions().pipboy.quests.at(execution.origin.record)
      .stages.at(execution.origin.stage).items.at(execution.origin.item).result;
    assert(execution.program.source==authored.source&&execution.program.compiled==authored.compiled);
    assert(execution.program.header==authored.header&&execution.program.references==authored.references);
    assert(execution.program.records==authored.records);
  }
  // Original bomb activation fixture is shared with the existing MS11 regression.
  auto event=p.PrepareReferenceEventTransaction(0x14bc8,"OnActivate",fo3player::PlayerRef,error,capture);
  assert(event&&!event->ExecutedScripts().empty());
  const auto& execution=event->ExecutedScripts().front();
  assert(execution.origin.kind==fo3player::ScriptProgramKind::ReferenceEvent&&execution.origin.event=="onactivate");
  const auto& authored=p.Definitions().pipboy.scripts.at(execution.origin.record);
  assert(execution.container.source==authored.source&&execution.container.records==authored.records);
  assert(execution.owner==0x14bc8&&execution.activator==fo3player::PlayerRef&&execution.origin.sourceLine);
  // Lucas Simms' original MS11 greeting is already exercised by the full
  // dialogue regression. Bind its begin fragment by canonical identity here.
  uint32_t topic=0;for(const auto& group:p.Definitions().pipboy.topics)
    for(const auto& info:group.second)if(info.id==0x1501d)topic=group.first;
  assert(topic);
  auto dialogue=p.PrepareDialogueInfoTransaction(topic,0x1501d,fo3player::DialogueScriptPhase::Begin,error,0x3b46,capture);
  assert(dialogue&&dialogue->ExecutedScripts().size()==1);
  const auto& fragment=dialogue->ExecutedScripts()[0];
  const auto& location=p.Definitions().dialogueInfos.at(0x1501d);
  const auto& body=p.Definitions().pipboy.topics.at(topic).at(location.second).begin;
  assert(fragment.origin.kind==fo3player::ScriptProgramKind::DialogueInfo);
  assert(fragment.origin.record==0x1501d&&fragment.origin.topic==topic&&fragment.origin.item==0);
  assert(fragment.program.source==body.source&&fragment.program.header==body.header);
  assert(fragment.program.compiled==body.compiled&&fragment.program.references==body.references&&fragment.program.records==body.records);
  const auto& greeting=p.Definitions().dialogueInfos.at(0x3da20);
  auto ending=p.PrepareDialogueInfoTransaction(greeting.first,0x3da20,fo3player::DialogueScriptPhase::End,error,0x3b46,capture);
  assert(ending&&ending->ExecutedScripts().size()==1);
  const auto& end=ending->ExecutedScripts()[0];
  const auto& endBody=p.Definitions().pipboy.topics.at(greeting.first).at(greeting.second).end;
  assert(end.origin.kind==fo3player::ScriptProgramKind::DialogueInfo&&end.origin.record==0x3da20&&end.origin.item==1);
  assert(!endBody.source.empty()&&!endBody.compiled.empty()&&!endBody.records.empty());
  assert(end.program.source==endBody.source&&end.program.compiled==endBody.compiled);
  assert(end.program.header==endBody.header&&end.program.references==endBody.references&&end.program.records==endBody.records);
  // Match the complete real entry sequence, not just individually decoded fields.
  auto programs=stage->ExecutedScripts();
  for(const auto* plan:{event.get(),dialogue.get(),ending.get()}){
    const auto offset=uint32_t(programs.size());
    for(auto entry:plan->ExecutedScripts()){
      if(entry.parent!=fo3player::ScriptExecution::NoParent)entry.parent+=offset;
      programs.push_back(std::move(entry));
    }
  }
  fo3script::ScriptMasterMatch matched;std::atomic<bool> cancelled{false};
  const bool valid=fo3script::MatchScriptProgramsToMaster(path,programs,matched,cancelled,error);
  if(!valid)std::cerr<<"Original master match: "<<error<<'\n';
  assert(valid&&matched.Verified());
  forbidAllocations=true;assert(matched.Matches(programs));forbidAllocations=false;
  auto forgedOwner=programs;
  forgedOwner[stage->ExecutedScripts().size()].owner=fo3player::PlayerRef;
  forgedOwner[stage->ExecutedScripts().size()].reference=fo3player::PlayerRef;
  assert(!fo3script::MatchScriptProgramsToMaster(path,forgedOwner,matched,cancelled,error));
  assert(matched.Matches(programs)); // authentic body, unauthenticated owner
  auto forgedSpeaker=programs;
  const auto infoIndex=stage->ExecutedScripts().size()+event->ExecutedScripts().size();
  forgedSpeaker[infoIndex].owner=0x14bc8; // genuine bomb REFR, not an actor
  assert(!fo3script::MatchScriptProgramsToMaster(path,forgedSpeaker,matched,cancelled,error));
  assert(error=="dialogue speaker is not an authored actor placement"&&matched.Matches(programs));
  forgedSpeaker[infoIndex].owner=0;
  assert(!fo3script::MatchScriptProgramsToMaster(path,forgedSpeaker,matched,cancelled,error)&&matched.Matches(programs));
  // Find an original explicit ANAM restriction and an authored matching actor.
  // This checks metadata admission, not execution/eligibility of that fragment.
  const auto lucasBase=p.Definitions().actorPlacements.at(0x3b46).base;
  fo3player::ScriptExecution restricted;bool foundRestricted=false;
  for(const auto& group:p.Definitions().pipboy.topics){
    for(const auto& info:group.second){
      if(!info.speaker||info.speaker==lucasBase||(info.recordFlags&0x20)||
          !p.Definitions().dialogueInfos.count(info.id))continue;
      for(const auto& actor:p.Definitions().actorPlacements){
        if(actor.second.base!=info.speaker||(actor.second.flags&0x20))continue;
        restricted.origin.kind=fo3player::ScriptProgramKind::DialogueInfo;
        restricted.origin.topic=info.topic;restricted.origin.record=info.id;
        restricted.owner=actor.first;restricted.program=info.begin;
        foundRestricted=true;break;
      }
      if(foundRestricted)break;
    }
    if(foundRestricted)break;
  }
  assert(foundRestricted);
  std::vector<fo3player::ScriptExecution> restrictedEvidence={restricted};
  fo3script::ScriptMasterMatch restrictedReport;
  assert(fo3script::MatchScriptProgramsToMaster(path,restrictedEvidence,restrictedReport,cancelled,error));
  restrictedEvidence[0].owner=0x3b46; // real actor, different authored base
  assert(!fo3script::MatchScriptProgramsToMaster(path,restrictedEvidence,restrictedReport,cancelled,error));
  assert(error=="dialogue speaker differs from original INFO ANAM"&&restrictedReport.Matches({restricted}));
  std::cout<<"Original INFO "<<restricted.origin.record<<" explicit ANAM accepts its actor and rejects Lucas\n";
  programs[0].program.source+=' ';
  assert(!matched.Matches(programs));
  assert(!fo3script::MatchScriptProgramsToMaster(path,programs,matched,cancelled,error));
  assert(p.Revision()==revision&&p.Snapshot().pipboy.quests.empty());NoEffects(p);
  std::cout<<"Original MS11 stage/bomb/INFO provenance passes strict master matching without publication\n";
}
struct ScriptExchangeAdapter {
  fo3player::Player& player;fo3player::ScriptTransactionPlan& script;
  fo3scene::ReferencePublication& publication;
  fo3scene::ReferenceExchange::Snapshot& cache;
  fo3scene::ReferenceExchange& references;
  int fail=-1;
  std::array<bool,4> resources{};
  bool Preflight() noexcept {return player.CanCommitScriptTransaction(script)&&references.CanPublishBefore(publication,cache,player.Snapshot().referenceStateRevision);}
  bool Apply(fo3scene::ResidentExchangeStage stage) noexcept {
    using Stage=fo3scene::ResidentExchangeStage;
    if(int(stage)==fail)return false;
    if(stage==Stage::Canonical)return player.CommitScriptTransaction(script);
    if(stage==Stage::Publication)return references.Commit(publication,cache,player.Snapshot().referenceStateRevision);
    resources[static_cast<size_t>(stage)-1]=true;return true;
  }
  bool Undo(fo3scene::ResidentExchangeStage stage) noexcept {
    using Stage=fo3scene::ResidentExchangeStage;
    if(stage==Stage::Canonical)return player.RollbackScriptTransaction(script);
    if(stage==Stage::Publication)return references.Rollback(publication,cache,player.Snapshot().referenceStateRevision);
    resources[static_cast<size_t>(stage)-1]=false;return true;
  }
  bool Published() noexcept {return fail!=6&&player.CanRollbackScriptTransaction(script)&&publication.Accepts(publication.Stamp(),player.Snapshot().referenceStateRevision);}
};
struct ScriptWorkflowPacket {};
struct ScriptWorkflowRequest {};
using ScriptWorkflow=fo3scene::ResidentWorkflow<fo3player::ScriptTransactionPlan,ScriptWorkflowPacket,ScriptWorkflowRequest>;
struct ScriptWorkflowAdapter {
  fo3player::Player& player;
  fo3scene::ReferencePublication& publication;
  fo3scene::ReferenceExchange::Snapshot& cache;
  fo3scene::ReferenceExchange& references;
  ScriptWorkflow& workflow;
  int fail=-1;
  std::array<bool,4> resources{};
  bool reserved=true;
  std::unique_ptr<ScriptWorkflowPacket> gpu;
  bool Current(uint64_t ticket,const fo3player::ScriptTransactionPlan& script) const noexcept {
    return reserved&&ticket==1&&script.HasProgramProvenance()&&script.HasReferenceChanges()&&player.CanCommitScriptTransaction(script);
  }
  bool Drain(uint64_t){return true;}
  bool StartCpu(uint64_t,const ScriptWorkflowRequest&,const fo3player::ScriptTransactionPlan& script) {
    return script.CandidateReferences()&&script.RollbackReferences()&&!script.AffectedReferences().empty();
  }
  fo3scene::ResidentCpuProgress CpuProgress(uint64_t){return fo3scene::ResidentCpuProgress::Ready;}
  bool TakeCpu(uint64_t,std::unique_ptr<ScriptWorkflowPacket>& packet){packet=std::make_unique<ScriptWorkflowPacket>();return true;}
  bool BeginGpu(std::unique_ptr<ScriptWorkflowPacket>& packet){gpu=std::move(packet);return true;}
  fo3scene::GpuStageResult AdvanceGpu(std::unique_ptr<ScriptWorkflowPacket>& packet){packet=std::move(gpu);return fo3scene::GpuStageResult::Ready;}
  bool Cancel(uint64_t){gpu.reset();reserved=false;return true;}
  fo3scene::ResidentExchangeResult Execute(ScriptWorkflowPacket& packet,fo3player::ScriptTransactionPlan& script) noexcept {
    assert(workflow.OwnsExchange(1,packet,script));
    ScriptExchangeAdapter adapter{player,script,publication,cache,references,fail,{}};
    const auto result=fo3scene::ExecuteResidentExchange(adapter);resources=adapter.resources;return result;
  }
  bool CanFinalize(uint64_t ticket,const ScriptWorkflowPacket&,const fo3player::ScriptTransactionPlan& script,
      const fo3scene::ResidentExchangeResult& outcome) const noexcept {
    const bool committed=outcome.status==fo3scene::ResidentExchangeStatus::Committed;
    return reserved&&ticket==1&&player.CanFinalizeScriptTransaction(script,committed)&&
      references.CanFinalize(publication,cache,player.Snapshot().referenceStateRevision,committed);
  }
  bool Reconcile(const fo3scene::ResidentExchangeResult&){return true;}
  void Retire(ScriptWorkflowPacket&) noexcept {}
  bool Release(uint64_t) noexcept {reserved=false;return true;}
  void FinalizationFailed() noexcept {assert(false);}
};
int main(int argc,char** argv){
  Provenance();
  DialogueProvenance();
  if(argc>1)OriginalProvenance(argv[1]);
  std::string error;fo3player::Player p(Fixture());
  fo3player::Player copy(p);assert(&copy.Definitions()==&p.Definitions());
  const auto path="/tmp/fq-script-plan-"+std::to_string(getpid())+".fqps";
  assert(p.Save(path,error));const auto originalSave=Bytes(path);const auto revision=p.Revision();
  auto plan=p.PrepareQuestStageTransaction(10,10,error);assert(plan&&error.empty());
  assert(plan->CandidateState()&&plan->CandidateState()->karma==5);
  assert(plan->CandidateState()->pipboy.quests.at(10).stage==10&&plan->CandidateState()->pipboy.quests.at(11).stage==20);
  assert(plan->CandidateState()->pipboy.quests.at(10).journal.size()==1);
  assert(!plan->HasReferenceChanges()&&!plan->CandidateReferences()&&plan->AffectedReferences().empty());
  assert(p.ItemCount(400)==0&&p.Snapshot().pipboy.quests.empty()&&p.Revision()==revision);
  assert(p.Save(path,error)&&Bytes(path)==originalSave);NoEffects(p);
  assert(p.CanCommitScriptTransaction(*plan)); // unsuccessful polls do not invalidate plans
  auto competing=p.PrepareQuestStageTransaction(10,10,error);assert(competing);
  forbidAllocations=true;
  const bool committed=p.CommitScriptTransaction(*plan),rolledBack=p.RollbackScriptTransaction(*plan);
  forbidAllocations=false;
  assert(committed&&rolledBack&&p.ItemCount(400)==0&&p.Snapshot().pipboy.quests.empty());
  assert(!plan->CandidateState()&&!p.CommitScriptTransaction(*plan)&&!p.RollbackScriptTransaction(*plan));
  assert(!p.CommitScriptTransaction(*competing));NoEffects(p);

  // Both dialogue fragments share one candidate and one instruction budget.
  auto bad=p.PrepareDialogueTransaction(Script("player.AddItem TestItem 1"),Script("player.RemoveItem TestItem 2"),error);
  assert(!bad&&!error.empty()&&p.ItemCount(400)==0);
  const fo3player::Player frozen(Fixture());
  assert(frozen.PreviewDialogueResults(Script("player.AddItem TestItem 1"),{},error)&&frozen.ItemCount(400)==0);
  auto broken=Fixture();broken.pipboy.quests[11].stages[20].items[0].result=Script("Enable");
  fo3player::Player rejected(std::move(broken));
  assert(!rejected.PrepareQuestStageTransaction(10,10,error)&&rejected.Snapshot().pipboy.quests.empty());
  NoEffects(rejected);

  // Existing effects stay ordered; delivered effects can never be rolled back/replayed.
  assert(p.QueueScriptSound(777));
  auto ready=p.PrepareQuestStageTransaction(10,10,error);assert(ready);
  fo3player::Player wrong(Fixture());assert(!wrong.CommitScriptTransaction(*ready));
  assert(p.CommitScriptTransaction(*ready)&&p.ItemCount(400)==3&&p.Snapshot().karma==5);
  assert(p.Save(path,error));fo3player::Player restored(Fixture());assert(restored.Restore(path,error)&&restored.ItemCount(400)==3);
  uint32_t sound=0;assert(p.PollScriptSound(sound)&&sound==777);
  assert(!p.CanFinalizeScriptTransaction(*ready,true));
  assert(!p.CanRollbackScriptTransaction(*ready)&&!p.RollbackScriptTransaction(*ready));
  assert(p.PollScriptSound(sound)&&sound==600&&!p.PollScriptSound(sound));
  fo3player::ScriptMessage message;fo3player::QuestNotice notice;fo3player::KarmaEvent karma;
  assert(p.PollScriptMessage(message)&&message.owner==10&&message.form==500&&!p.PollScriptMessage(message));
  assert(p.PollQuestNotice(notice)&&notice.quest==10&&!p.PollQuestNotice(notice));
  assert(p.PollKarmaEvent(karma)&&karma.requested==5&&!p.PollKarmaEvent(karma));

  fo3player::Player q(Fixture());
  assert(q.QueueScriptSound(700));auto stale=q.PrepareQuestStageTransaction(10,10,error);assert(stale);
  const auto unchangedRevision=q.Revision();assert(q.PollScriptSound(sound)&&q.Revision()==unchangedRevision);
  assert(!q.CommitScriptTransaction(*stale));
  auto moved=q.PrepareQuestStageTransaction(10,10,error);assert(moved);
  q.SetScriptPlayerLocation(1,2,3,4,5);assert(!q.CommitScriptTransaction(*moved));
  auto input=q.PrepareQuestStageTransaction(10,10,error);assert(input);
  assert(q.SubmitScriptMessageButton(100,1)&&!q.CommitScriptTransaction(*input));
  auto event=q.PrepareReferenceEventTransaction(100,"GameMode",0,error);assert(event);
  assert(q.Snapshot().karma==0&&event->CandidateState()->karma==7);
  assert(q.CommitScriptTransaction(*event)&&q.RollbackScriptTransaction(*event)&&q.Snapshot().karma==0);
  assert(!q.SubmitScriptMessageButton(100,1)); // rollback restored the unconsumed button
  event=q.PrepareReferenceEventTransaction(100,"GameMode",0,error);
  assert(event&&q.CommitScriptTransaction(*event)&&q.Snapshot().karma==7);
  assert(q.SubmitScriptMessageButton(100,1)); // successful event consumed it once
  unsigned diagnostics=0;q.SetScriptEventDiagnostic([&](const std::string&){++diagnostics;});
  assert(!q.PrepareReferenceEventTransaction(100,"OnActivate",999,error)&&diagnostics==0);
  assert(q.DispatchReferenceEvent(100,"OnActivate",999,error)==fo3player::ScriptEventResult::Unsupported&&diagnostics==1);

  // Every effect-delivery channel invalidates plans without changing saved state.
  for(int channel=0;channel<3;++channel){
    fo3player::Player delivery(Fixture());assert(delivery.ExecuteQuestStage(10,10,error));
    auto pending=delivery.PrepareDialogueTransaction({}, {},error);assert(pending);
    const auto savedRevision=delivery.Revision();
    if(channel==0)assert(delivery.PollScriptMessage(message));
    if(channel==1)assert(delivery.PollQuestNotice(notice));
    if(channel==2)assert(delivery.PollKarmaEvent(karma));
    assert(delivery.Revision()==savedRevision&&!delivery.CommitScriptTransaction(*pending)&&!delivery.CanFinalizeScriptTransaction(*pending,false));
  }
  auto failedInput=Fixture();
  failedInput.pipboy.scripts[300].source="Begin GameMode\nEnable\nEnd";
  failedInput.eventPrograms[300]=fo3script::ParseEvents(failedInput.pipboy.scripts[300]);
  fo3player::Player failing(std::move(failedInput));assert(failing.SubmitScriptMessageButton(100,1));
  auto pending=failing.PrepareDialogueTransaction({}, {},error);assert(pending);
  const auto savedRevision=failing.Revision();failing.PumpReferenceGameMode(.25,{100});
  assert(failing.Revision()==savedRevision&&!failing.CommitScriptTransaction(*pending));
  assert(failing.SubmitScriptMessageButton(100,1)); // failure still consumed delivered input
  std::remove(path.c_str());
  // The canonical whole-script rollback and its renderer publication must use
  // the same precomputed revision, rather than restoring an old worker stamp.
  for(bool publishCandidate:{false,true}) {
  auto worldCatalog=Fixture();worldCatalog.pipboy.formNames["fixtureref"]=100;
  worldCatalog.pipboy.quests[10].stages[30].items.emplace_back().result=
      Script("FixtureRef.Disable\nplayer.AddItem TestItem 1\nShowMessage TestMessage");
  fo3player::Player world(std::move(worldCatalog));
  auto worldPlan=world.PrepareQuestStageTransaction(10,30,error,fo3player::ScriptProgramCapture::EnteredPrograms);assert(worldPlan&&worldPlan->HasReferenceChanges());
  auto before=std::make_shared<const fo3world::ReferenceStateSnapshot>(world.CaptureReferenceState());
  fo3scene::ReferencePublication publication;publication.Publish(before);auto cache=before;
  const auto oldStamp=publication.Stamp();
  auto exchange=fo3scene::ReferenceExchange::Prepare(publication,cache,before,
      worldPlan->CandidateReferences(),worldPlan->RollbackReferences());
  assert(exchange&&exchange->CanPublishBefore(publication,cache,world.Snapshot().referenceStateRevision));
  assert(!exchange->Commit(publication,cache,world.Snapshot().referenceStateRevision));
  forbidAllocations=true;
  assert(world.CommitScriptTransaction(*worldPlan));
  if(publishCandidate)assert(exchange->Commit(publication,cache,world.Snapshot().referenceStateRevision));
  const auto committedStamp=publication.Stamp();
  assert(world.CanRollbackScriptTransaction(*worldPlan)&&world.RollbackScriptTransaction(*worldPlan));
  assert(world.CanFinalizeScriptTransaction(*worldPlan,false)&&!world.CanFinalizeScriptTransaction(*worldPlan,true));
  assert(exchange->Rollback(publication,cache,world.Snapshot().referenceStateRevision));
  forbidAllocations=false;
  assert(cache==worldPlan->RollbackReferences()&&cache->states==before->states);
  assert(world.ItemCount(400)==0&&world.Snapshot().pipboy.quests.empty());NoEffects(world);
  assert(publication.Accepts(publication.Stamp(),world.Snapshot().referenceStateRevision));
  assert(!publication.Accepts(oldStamp,0)&&!publication.Accepts(committedStamp,1));
  }
  // Exercise the composite executor with a real canonical script/effect owner,
  // including failure after publication, and no allocations during any turn.
  for(int failure=-1;failure<=6;++failure){
    auto catalog=Fixture();catalog.pipboy.formNames["fixtureref"]=100;
    catalog.pipboy.quests[10].stages[30].items.emplace_back().result=
        Script("FixtureRef.Disable\nplayer.AddItem TestItem 1\nShowMessage TestMessage");
    fo3player::Player owner(std::move(catalog));
    auto logical=owner.PrepareQuestStageTransaction(10,30,error,fo3player::ScriptProgramCapture::EnteredPrograms);assert(logical&&logical->HasReferenceChanges());
    auto before=std::make_shared<const fo3world::ReferenceStateSnapshot>(owner.CaptureReferenceState());
    fo3scene::ReferencePublication publication;publication.Publish(before);auto cache=before;
    const auto stamp=publication.Stamp();
    auto references=fo3scene::ReferenceExchange::Prepare(publication,cache,before,logical->CandidateReferences(),logical->RollbackReferences());
    assert(references);
    ScriptWorkflow workflow;
    ScriptWorkflowAdapter adapter{owner,publication,cache,*references,workflow,failure,{},true,{}};
    auto uncaptured=owner.PrepareQuestStageTransaction(10,30,error);
    assert(uncaptured&&!uncaptured->HasProgramProvenance());
    assert(!workflow.Begin(1,uncaptured,{},adapter)&&uncaptured&&!workflow.Active());
    const auto* owned=logical.get();const auto originalRevision=owner.Revision();
    assert(workflow.Begin(1,logical,{},adapter)&&!logical);
    for(int i=0;i<3;++i){
      workflow.Pump(adapter);
      assert(owner.Revision()==originalRevision&&owner.ItemCount(400)==0&&cache==before);
      NoEffects(owner);
    }
    assert(workflow.State()==fo3scene::ResidentWorkflowPhase::Ready);
    forbidAllocations=true;const auto result=workflow.Execute(1,adapter);forbidAllocations=false;
    assert(adapter.reserved&&!workflow.RequestCancel(1));
    using Status=fo3scene::ResidentExchangeStatus;
    if(failure<0){assert(result.status==Status::Committed&&owner.ItemCount(400)==1&&owner.PendingScriptMessageCount()==1&&cache==owned->CandidateReferences());}
    else {
      assert(result.status==(failure?Status::RolledBack:Status::Rejected));
      for(bool applied:adapter.resources)assert(!applied);
      assert(owner.ItemCount(400)==0&&owner.Snapshot().pipboy.quests.empty());NoEffects(owner);
      assert(cache==(failure?owned->RollbackReferences():before));
      assert(publication.Accepts(publication.Stamp(),owner.Snapshot().referenceStateRevision));
      if(failure)assert(!publication.Accepts(stamp,0));
    }
    forbidAllocations=true;assert(workflow.Finalize(1,adapter));forbidAllocations=false;
    assert(!workflow.Active()&&!adapter.reserved);
    if(failure<0){fo3player::ScriptMessage delivered;assert(owner.PollScriptMessage(delivered)&&!owner.PollScriptMessage(delivered));}
  }
  // The owner validates the actual Player plan on each pump and at execution.
  // Canonical revision, input/effect epoch and location changes during the
  // reserved interval cannot publish a previously prepared private candidate.
  for(int stale=0;stale<3;++stale){
    auto catalog=Fixture();catalog.pipboy.formNames["fixtureref"]=100;
    catalog.pipboy.quests[10].stages[30].items.emplace_back().result=Script("FixtureRef.Disable\nplayer.AddItem TestItem 1");
    fo3player::Player owner(std::move(catalog));
    auto logical=owner.PrepareQuestStageTransaction(10,30,error,fo3player::ScriptProgramCapture::EnteredPrograms);assert(logical);
    auto before=std::make_shared<const fo3world::ReferenceStateSnapshot>(owner.CaptureReferenceState());
    fo3scene::ReferencePublication publication;publication.Publish(before);auto cache=before;
    auto references=fo3scene::ReferenceExchange::Prepare(publication,cache,before,logical->CandidateReferences(),logical->RollbackReferences());assert(references);
    ScriptWorkflow workflow;
    ScriptWorkflowAdapter adapter{owner,publication,cache,*references,workflow,-1,{},true,{}};
    assert(workflow.Begin(1,logical,{},adapter));
    for(int i=0;i<3;++i)workflow.Pump(adapter);
    if(stale==0){const auto revision=owner.Revision();assert(owner.DamageHealth(1)&&owner.Revision()!=revision);}
    if(stale==1)assert(owner.SubmitScriptMessageButton(100,1));
    if(stale==2)owner.SetScriptPlayerLocation(100,200,300,400,0);
    workflow.Pump(adapter);assert(workflow.State()==fo3scene::ResidentWorkflowPhase::Cancelling);
    assert(workflow.Execute(1,adapter).status==fo3scene::ResidentExchangeStatus::Rejected);
    workflow.Pump(adapter);assert(!workflow.Active()&&!adapter.reserved&&cache==before&&owner.ItemCount(400)==0);
  }
  std::cout<<"Detached whole-script state/effect transactions, previews, publication, rollback and delivery/input guards passed\n";
}
