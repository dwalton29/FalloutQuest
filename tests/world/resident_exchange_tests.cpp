#include "fo3-resident-exchange.h"
#include "fo3-resource-exchange.h"
#include "fo3-reference-exchange.h"
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
static bool forbidAllocation=false;
void* operator new(std::size_t n){if(forbidAllocation)throw std::bad_alloc();if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using Stage=fo3scene::ResidentExchangeStage;
using Status=fo3scene::ResidentExchangeStatus;
using Resource=fo3scene::ResourceExchange<std::vector<int>>;
struct Adapter {
 std::array<fo3scene::VisualPublication,4> epochs;
 std::array<std::vector<int>,4> live;
 std::array<const int*,4> original{};
 std::array<std::unique_ptr<Resource>,4> resources;
 fo3scene::ReferencePublication publication;
 fo3scene::ReferenceExchange::Snapshot before,candidate,rollback,cache;
 std::unique_ptr<fo3scene::ReferenceExchange> references;
 int canonical=0,failApply=-1,failUndo=-1;
 uint64_t revision=0;
 bool ready=true,verify=true;
 std::array<int,32> calls{};size_t count=0;
 Adapter(){
  for(size_t i=0;i<live.size();++i){live[i]={int(i+10)};original[i]=live[i].data();Resource::Payload incoming{std::vector<int>{int(i+20)}};resources[i]=Resource::Prepare(epochs[i],std::tie(live[i]),incoming);assert(resources[i]);}
  auto b=std::make_shared<fo3world::ReferenceStateSnapshot>();b->states[10]=fo3world::ReferenceEnableState::Enabled;before=b;
  auto c=std::make_shared<fo3world::ReferenceStateSnapshot>(*b);c->revision=1;c->states[10]=fo3world::ReferenceEnableState::Disabled;candidate=c;
  auto r=std::make_shared<fo3world::ReferenceStateSnapshot>(*b);r->revision=2;rollback=r;
  publication.Publish(before);cache=before;
  references=fo3scene::ReferenceExchange::Prepare(publication,cache,before,candidate,rollback);assert(references);
 }
 bool Preflight() noexcept {
  if(!ready||!references->CanPublishBefore(publication,cache,revision))return false;
  for(size_t i=0;i<live.size();++i)if(!resources[i]->CanCommit(epochs[i],std::tie(live[i])))return false;
  return true;
 }
 bool Apply(Stage stage) noexcept {
  calls[count++]=int(stage);if(int(stage)==failApply)return false;
  if(stage==Stage::Canonical){canonical=1;revision=1;return true;}
  if(stage==Stage::Publication)return references->Commit(publication,cache,revision);
  const auto i=static_cast<size_t>(stage)-1;return resources[i]->Commit(epochs[i],std::tie(live[i]));
 }
 bool Undo(Stage stage) noexcept {
  calls[count++]=100+int(stage);if(int(stage)==failUndo)return false;
  if(stage==Stage::Canonical){canonical=0;revision=2;return true;}
  if(stage==Stage::Publication)return references->Rollback(publication,cache,revision);
  const auto i=static_cast<size_t>(stage)-1;return resources[i]->Rollback(epochs[i],std::tie(live[i]));
 }
 bool Published() noexcept {calls[count++]=200;return verify&&cache==candidate&&canonical==1&&publication.Accepts(publication.Stamp(),revision);}
 void Restored()const {assert(canonical==0&&revision==2&&cache==rollback&&cache->states==before->states);for(size_t i=0;i<live.size();++i)assert(live[i].data()==original[i]&&live[i][0]==int(i+10));}
};
int main(){
 for(int failure=-1;failure<=6;++failure){
  Adapter a;a.failApply=failure;a.verify=failure!=6;const auto stamp=a.publication.Stamp();
  forbidAllocation=true;const auto result=fo3scene::ExecuteResidentExchange(a);forbidAllocation=false;
  assert(!result.recoveryFailures);
  if(failure<0){assert(result.status==Status::Committed&&a.canonical==1&&a.revision==1&&a.cache==a.candidate);for(size_t i=0;i<4;++i)assert(a.live[i][0]==int(i+20));}
  else if(!failure){assert(result.status==Status::Rejected&&result.failedAt==Stage::Canonical&&a.cache==a.before&&a.publication.Stamp()==stamp);}
  else {assert(result.status==Status::RolledBack);a.Restored();assert(!a.publication.Accepts(stamp,0));}
 }
 // Exact unwind order at publication failure: reverse resources, canonical,
 // then publication recovery (whose candidate was never made live).
 {Adapter a;a.failApply=5;const auto result=fo3scene::ExecuteResidentExchange(a);assert(result.status==Status::RolledBack);
  const std::array<int,12> expected{{0,1,2,3,4,5,104,103,102,101,100,105}};
  assert(a.count==expected.size());for(size_t i=0;i<expected.size();++i)assert(a.calls[i]==expected[i]);}
 for(int undo=0;undo<6;++undo){
  Adapter a;a.verify=false;a.failUndo=undo;
  forbidAllocation=true;const auto result=fo3scene::ExecuteResidentExchange(a);forbidAllocation=false;
  assert(result.status==Status::RecoveryFailed&&(result.recoveryFailures&fo3scene::ResidentExchangeBit(static_cast<Stage>(undo))));
  if(!undo){assert(result.recoveryFailures&fo3scene::ResidentExchangeBit(Stage::Publication));for(size_t i=0;i<a.count;++i)assert(a.calls[i]!=105);}
 }
 for(int stale=0;stale<4;++stale){Adapter a;
  if(stale==0)a.ready=false;
  if(stale==1)a.epochs[2].Changed();
  if(stale==2)a.publication.Publish(a.before);
  if(stale==3)a.revision=7;
  forbidAllocation=true;const auto result=fo3scene::ExecuteResidentExchange(a);forbidAllocation=false;
  assert(result.status==Status::Rejected&&result.failedAt==Stage::Preflight&&!a.count&&a.canonical==0&&a.cache==a.before);
 }
 std::cout<<"Resident composite exchange, every apply/undo failure, exact restoration and allocation-free recovery passed\n";
}
