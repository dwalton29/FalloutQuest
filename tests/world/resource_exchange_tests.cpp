#include "fo3-resource-exchange.h"
#include <cassert>
#include <cstdlib>
#include <new>
#include <iostream>
#include <map>
#include <string>
static bool failAllocation=false;
void* operator new(std::size_t n){if(failAllocation)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
using Plan=fo3scene::ResourceExchange<std::vector<std::string>,std::map<int,std::string>>;
int main(){
    std::vector<std::string> lights{"original authored light metadata long enough to allocate"};
    std::map<int,std::string> metadata{{1,"before"}};
    fo3scene::VisualPublication publication;
    const auto before=lights.data();const auto entry=&*metadata.begin();
    Plan::Payload incoming{{"incoming authored light metadata long enough to allocate"},{{2,"after"}}};
    const auto staged=std::get<0>(incoming).data();
    failAllocation=true;bool threw=false;
    try{Plan::Prepare(publication,std::tie(lights,metadata),incoming);}catch(const std::bad_alloc&){threw=true;}
    failAllocation=false;assert(threw&&lights.data()==before&&std::get<0>(incoming).data()==staged);
    auto plan=Plan::Prepare(publication,std::tie(lights,metadata),incoming);
    assert(plan&&plan->CanCommit(publication,std::tie(lights,metadata))&&!plan->Applied()&&std::get<0>(incoming).empty()&&lights.data()==before);
    std::vector<std::string> wrong;
    assert(!plan->Commit(publication,std::tie(wrong,metadata)));
    fo3scene::VisualPublication wrongPublication;
    assert(!plan->Commit(wrongPublication,std::tie(lights,metadata)));
    failAllocation=true;const bool applied=plan->Commit(publication,std::tie(lights,metadata));failAllocation=false;
    assert(applied&&plan->CanRollback(publication,std::tie(lights,metadata))&&plan->Applied()&&lights.data()==staged&&metadata.count(2));
    assert(!plan->Commit(publication,std::tie(lights,metadata)));
    failAllocation=true;const bool restored=plan->Rollback(publication,std::tie(lights,metadata));failAllocation=false;
    assert(restored&&!plan->Applied()&&lights.data()==before&&&*metadata.begin()==entry&&metadata.count(1));
    assert(!plan->Rollback(publication,std::tie(lights,metadata)));
    plan.reset();
    // Dropping a prepared plan frees staged values only; finalizing an applied
    // plan retains the new bundle. Legacy mutation invalidates either direction.
    for(bool apply:{false,true}) {
        Plan::Payload next{{"next"},{{3,"next"}}};auto p=Plan::Prepare(publication,std::tie(lights,metadata),next);
        assert(p);if(apply)assert(p->Commit(publication,std::tie(lights,metadata)));
        p.reset();assert(metadata.count(apply?3:1));
    }
    Plan::Payload next{{"stale"},{}};auto stale=Plan::Prepare(publication,std::tie(lights,metadata),next);
    publication.Changed();assert(!stale->Commit(publication,std::tie(lights,metadata)));stale.reset();
    next={{"applied stale"},{}};stale=Plan::Prepare(publication,std::tie(lights,metadata),next);
    assert(stale->Commit(publication,std::tie(lights,metadata)));publication.Changed();
    assert(!stale->Rollback(publication,std::tie(lights,metadata)));stale.reset();
    // References into the incoming tuple and duplicate live owners reject.
    Plan::Payload aliased{{"alias"},{}};
    assert(!Plan::Prepare(publication,std::tie(std::get<0>(aliased),metadata),aliased));
    using Duplicate=fo3scene::ResourceExchange<std::vector<std::string>,std::vector<std::string>>;
    Duplicate::Payload values;assert(!Duplicate::Prepare(publication,std::tie(lights,lights),values));
    fo3scene::VisualPublication exhausted(UINT64_MAX-1);Plan::Payload empty;
    assert(!Plan::Prepare(exhausted,std::tie(lights,metadata),empty));
    std::cout<<"Coupled RAII resource preparation, allocation-free exchange/rollback and stale/alias guards passed\n";
}
