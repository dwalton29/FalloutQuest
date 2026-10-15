#include "player/fo3-player-state.h"
#include "world/fo3-reference-exchange.h"
#include "world/fo3-resident-exchange.h"
#include "world/fo3-resident-workflow.h"
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
    return reserved&&ticket==1&&script.HasReferenceChanges()&&player.CanCommitScriptTransaction(script);
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
int main(){
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
  auto worldPlan=world.PrepareQuestStageTransaction(10,30,error);assert(worldPlan&&worldPlan->HasReferenceChanges());
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
    auto logical=owner.PrepareQuestStageTransaction(10,30,error);assert(logical&&logical->HasReferenceChanges());
    auto before=std::make_shared<const fo3world::ReferenceStateSnapshot>(owner.CaptureReferenceState());
    fo3scene::ReferencePublication publication;publication.Publish(before);auto cache=before;
    const auto stamp=publication.Stamp();
    auto references=fo3scene::ReferenceExchange::Prepare(publication,cache,before,logical->CandidateReferences(),logical->RollbackReferences());
    assert(references);
    ScriptWorkflow workflow;
    ScriptWorkflowAdapter adapter{owner,publication,cache,*references,workflow,failure,{},true,{}};
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
    auto logical=owner.PrepareQuestStageTransaction(10,30,error);assert(logical);
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
