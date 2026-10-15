#include "fo3-reference-exchange.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
#include <type_traits>
static bool forbidAllocation=false;
void* operator new(std::size_t n){if(forbidAllocation)throw std::bad_alloc();if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using State=fo3world::ReferenceStateSnapshot;
using Enable=fo3world::ReferenceEnableState;
using Exchange=fo3scene::ReferenceExchange;
Exchange::Snapshot Snapshot(uint64_t revision,bool enabled=true){auto s=std::make_shared<State>();s->revision=revision;s->states[42]=enabled?Enable::Enabled:Enable::Disabled;return s;}
int main(){
 using namespace fo3scene;
 auto before=Snapshot(0),candidate=Snapshot(1,false),rollback=Snapshot(2);
 ReferencePublication publication;publication.Publish(before);auto cache=before;
 auto stamp=publication.Stamp();
 auto prepare=[&]{return Exchange::Prepare(publication,cache,before,candidate,rollback);};
 assert(!Exchange::Prepare(publication,cache,{},candidate,rollback));
 auto wrong=*rollback;wrong.revision=3;
 assert(!ValidResidentReferenceProjections(*before,*candidate,wrong));
 wrong=*rollback;wrong.states[42]=Enable::Disabled;
 assert(!ValidResidentReferenceProjections(*before,*candidate,wrong));
 wrong=*candidate;wrong.states[0]=Enable::Disabled;
 assert(!ValidResidentReferenceProjections(*before,wrong,*rollback));
 wrong=*candidate;wrong.states[42]=static_cast<Enable>(99);
 assert(!ValidResidentReferenceProjections(*before,wrong,*rollback));
 auto overflow=*before;overflow.revision=UINT64_MAX-1;
 assert(!ValidResidentReferenceProjections(overflow,*candidate,*rollback));
 forbidAllocation=true;bool failed=false;try{prepare();}catch(const std::bad_alloc&){failed=true;}forbidAllocation=false;
 assert(failed&&cache==before&&publication.Stamp()==stamp&&publication.Snapshot()==before);
 auto plan=prepare();assert(plan&&plan->CanPublishBefore(publication,cache,0));
 assert(plan->CanFinalize(publication,cache,0,false)&&!plan->CanFinalize(publication,cache,0,true));
 assert(!plan->CanCommit(publication,cache,0)&&!plan->Commit(publication,cache,0));
 assert(!plan->CanRollback(publication,cache,0));
 auto otherCache=cache;ReferencePublication other;other.Publish(before);
 assert(!plan->CanCommit(publication,otherCache,1));
 assert(!plan->CanCommit(other,cache,1));
 // Matching values/revisions are not a replacement for the actual cache owner.
 cache=Snapshot(0);assert(!plan->CanCommit(publication,cache,1));cache=before;
 forbidAllocation=true;
 assert(plan->Commit(publication,cache,1));
 assert(cache==candidate&&publication.Snapshot()==candidate&&plan->Applied());
 assert(plan->CanFinalize(publication,cache,1,true)&&!plan->CanFinalize(publication,cache,1,false));
 assert(!publication.Accepts(stamp,0)&&publication.Accepts(publication.Stamp(),1));
 const auto applied=publication.Stamp();
 assert(!plan->Commit(publication,cache,1)&&!plan->Rollback(publication,cache,1));
 assert(plan->CanRollback(publication,cache,2)&&plan->Rollback(publication,cache,2));
 forbidAllocation=false;
 assert(cache==rollback&&cache!=before&&cache->states==before->states);
 assert(plan->CanFinalize(publication,cache,2,false)&&!plan->CanFinalize(publication,cache,2,true));
 assert(publication.Stamp().scene>applied.scene&&publication.Stamp().references==2);
 assert(!publication.Accepts(stamp,0)&&!publication.Accepts(applied,1));
 assert(publication.Accepts(publication.Stamp(),2)&&!plan->Commit(publication,cache,1)&&!plan->Rollback(publication,cache,2));

 // Prepared abandonment and applied finalization leave the chosen live state.
 publication.Publish(before);cache=before;{auto abandoned=prepare();assert(abandoned);}
 assert(cache==before&&publication.Snapshot()==before);
 {auto finalized=prepare();assert(finalized->Commit(publication,cache,1));}
 assert(cache==candidate&&publication.Snapshot()==candidate);
 publication.Publish(before);cache=before;
 {auto skipped=prepare();const auto old=publication.Stamp();
  assert(skipped->Rollback(publication,cache,2)&&cache==rollback);
  assert(!publication.Accepts(old,0)&&publication.Accepts(publication.Stamp(),2));}
 publication.Publish(before);cache=before;plan=prepare();publication.Publish(before);
 assert(!plan->CanPublishBefore(publication,cache,0)&&!plan->Commit(publication,cache,1));
 assert(!plan->CanFinalize(publication,cache,0,false));
 plan=prepare();assert(plan->Commit(publication,cache,1));publication.Publish(candidate);
 assert(!plan->Rollback(publication,cache,2));

 // Reconstructed publishers at the exact same address must not pass identity.
 std::aligned_storage_t<sizeof(ReferencePublication),alignof(ReferencePublication)> storage;
 auto* reused=new(&storage) ReferencePublication;
 reused->Publish(before);cache=before;
 auto stale=Exchange::Prepare(*reused,cache,before,candidate,rollback);assert(stale);
 reused->~ReferencePublication();reused=new(&storage) ReferencePublication;
 reused->Publish(before);assert(!stale->Commit(*reused,cache,1));reused->~ReferencePublication();

 ReferencePublication nearEnd(UINT64_MAX-3);nearEnd.Publish(before);cache=before;
 auto last=Exchange::Prepare(nearEnd,cache,before,candidate,rollback);assert(last);
 assert(last->Commit(nearEnd,cache,1)&&last->Rollback(nearEnd,cache,2));
 const auto finalStamp=nearEnd.Stamp();assert(finalStamp.scene==UINT64_MAX);
 assert(last->CanFinalize(nearEnd,cache,2,false));
 nearEnd.Publish(rollback);assert(nearEnd.Stamp().scene==0&&!nearEnd.Accepts(finalStamp,2)&&!nearEnd.CanAdvance(1));
 ReferencePublication noRoom(UINT64_MAX-2);noRoom.Publish(before);cache=before;
 assert(!Exchange::Prepare(noRoom,cache,before,candidate,rollback));
 std::cout<<"Coupled reference/cache publication, monotonic rollback, stale identities and exhaustion passed\n";
}
