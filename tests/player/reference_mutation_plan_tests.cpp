#include "player/fo3-player-state.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <new>
#include <type_traits>
#include <unistd.h>

static bool forbidAllocations=false;
void* operator new(std::size_t size) {
  if(forbidAllocations)throw std::bad_alloc();
  if(void* p=std::malloc(size?size:1))return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}

static fo3player::Catalog Fixture() {
  fo3player::Catalog c;c.initial.baseHealth=100;c.initial.special.fill(1);
  for(uint32_t id=1;id<=9;++id)c.references[id].flags=0;
  c.references[2].flags=0x800;c.references[9].flags=0x20;
  c.enableParents[3]={2,0,true};c.enableParents[4]={2,1,true};
  c.enableParents[5]={3,0,true};c.enableParents[6]={7,0,true};
  c.enableParents[7]={6,0,true};c.enableParents[8]={999,0,true};
  return c;
}
static bool Disabled(const fo3player::Player& p,uint32_t id) {
  bool disabled=false;std::string error;assert(p.ReferenceDisabled(id,disabled,error));return disabled;
}
static std::string Bytes(const std::string& path) {
  std::ifstream in(path,std::ios::binary);assert(in);
  return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
int main() {
  fo3player::Player p(Fixture());std::string error;
  const auto path="/tmp/fq-reference-plan-"+std::to_string(getpid())+".fqps";
  assert(p.Save(path,error));const auto beforeSave=Bytes(path);
  const auto beforeRevision=p.Revision(),beforeWorld=p.Snapshot().referenceStateRevision;
  auto plan=p.PrepareReferenceEnablePlan({{2,true},{1,false}},error);
  assert(plan&&error.empty()&&plan->HasChanges()&&p.CanCommitReferenceEnablePlan(*plan));
  assert(plan->AffectedReferences()==std::vector<uint32_t>({1,2,3,4,5}));
  const auto candidate=plan->CandidateSnapshot(),rollback=plan->RollbackSnapshot();
  assert(candidate->revision==beforeWorld+1&&rollback->revision==beforeWorld+2);
  assert(candidate->Enabled(2,false)&&candidate->Enabled(3,false)&&candidate->Enabled(5,false));
  assert(!candidate->Enabled(1,true)&&!candidate->Enabled(4,true));
  assert(Disabled(p,2)&&!Disabled(p,1)&&p.Revision()==beforeRevision);
  assert(p.Snapshot().referenceEnabled.empty()&&p.Snapshot().referenceStateRevision==beforeWorld);
  assert(p.Save(path,error)&&Bytes(path)==beforeSave); // preparation cannot leak into saves
  auto competing=p.PrepareReferenceEnablePlan({{2,true}},error);assert(competing);
  forbidAllocations=true;
  const bool committed=p.CommitReferenceEnablePlan(*plan);
  const bool rolledBack=p.RollbackReferenceEnablePlan(*plan);
  forbidAllocations=false;
  assert(committed&&rolledBack&&Disabled(p,2)&&!Disabled(p,1));
  assert(p.Revision()==beforeRevision+2&&p.Snapshot().referenceStateRevision==beforeWorld+2);
  assert(p.Snapshot().referenceEnabled.empty()&&candidate->Enabled(2,false));
  assert(!p.CommitReferenceEnablePlan(*plan)&&!p.RollbackReferenceEnablePlan(*plan));
  assert(!p.CommitReferenceEnablePlan(*competing)); // rollback never re-admits an older plan
  plan.reset();assert(candidate->Enabled(2,false)); // owned worker input survives plan retirement

  // Every ordered request is validated before anything live changes.
  const auto revision=p.Revision();
  for(uint32_t bad:{0u,6u,8u,9u,999u}){
    assert(!p.PrepareReferenceEnablePlan({{2,true},{bad,true}},error)&&!error.empty());
    assert(p.Revision()==revision&&Disabled(p,2)&&p.Snapshot().referenceEnabled.empty());
  }
  std::vector<fo3player::ReferenceEnableChange> oversized(100001,{2,true});
  assert(!p.PrepareReferenceEnablePlan(oversized,error));
  auto noop=p.PrepareReferenceEnablePlan({{2,true},{2,false},{3,true}},error);
  assert(noop&&!noop->HasChanges()&&noop->AffectedReferences().empty());
  assert(p.CommitReferenceEnablePlan(*noop)&&p.RollbackReferenceEnablePlan(*noop)&&p.Revision()==revision);

  // Conservative full-Player revision validation protects outer script context.
  auto stale=p.PrepareReferenceEnablePlan({{2,true}},error);assert(stale);
  assert(p.DamageHealth(1)&&!p.CanCommitReferenceEnablePlan(*stale)&&!p.CommitReferenceEnablePlan(*stale));
  auto live=p.PrepareReferenceEnablePlan({{2,true}},error);assert(live);
  fo3player::Player other(Fixture());assert(!other.CommitReferenceEnablePlan(*live));
  assert(p.CommitReferenceEnablePlan(*live)&&!Disabled(p,2));
  assert(p.Save(path,error));
  fo3player::Player restored(Fixture());assert(restored.Restore(path,error)&&!Disabled(restored,2));
  assert(p.DamageHealth(1)&&!p.RollbackReferenceEnablePlan(*live)&&!Disabled(p,2));
  assert(p.Health()==p.MaxHealth()-2);

  // A plan that only removes an existing override still lists the root and all descendants.
  auto remove=p.PrepareReferenceEnablePlan({{2,false}},error);assert(remove&&remove->HasChanges());
  assert(remove->AffectedReferences()==std::vector<uint32_t>({2,3,4,5}));
  assert(p.CommitReferenceEnablePlan(*remove)&&Disabled(p,2)&&p.Snapshot().referenceEnabled.empty());
  assert(p.RollbackReferenceEnablePlan(*remove)&&!Disabled(p,2));

  // Preflight cancellation changes neither live state nor queued effects.
  const auto currentRevision=p.Revision();
  auto cancelled=p.PrepareReferenceEnablePlan({{1,false}},error);assert(cancelled);
  const auto cancelledSnapshot=cancelled->CandidateSnapshot();cancelled.reset();
  assert(p.Revision()==currentRevision&&!Disabled(p,1)&&!cancelledSnapshot->Enabled(1,true));
  assert(p.PendingScriptMessageCount()==0);
  auto exhausted=Fixture();exhausted.initial.referenceStateRevision=UINT64_MAX-1;
  fo3player::Player atLimit(std::move(exhausted));
  assert(!atLimit.PrepareReferenceEnablePlan({{2,true}},error));

  // Session identity survives address reuse: an old plan cannot bind to a new Player.
  std::aligned_storage_t<sizeof(fo3player::Player),alignof(fo3player::Player)> storage;
  auto* old=new(&storage)fo3player::Player(Fixture());
  auto orphan=old->PrepareReferenceEnablePlan({{2,true}},error);assert(orphan);
  old->~Player();
  auto* replacement=new(&storage)fo3player::Player(Fixture());
  assert(!replacement->CommitReferenceEnablePlan(*orphan));replacement->~Player();
  std::remove(path.c_str());
  std::cout<<"Owned reference plans, atomic batch validation, allocation-free commit/rollback and stale-session rejection passed\n";
}
