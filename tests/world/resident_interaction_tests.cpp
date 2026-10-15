#include "fo3-resident-interaction.h"
#include "fo3-resource-exchange.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
static bool failAllocation=false;
static int allocationsRemaining=-1;
void* operator new(std::size_t count) {
    if(failAllocation||allocationsRemaining==0)throw std::bad_alloc();
    if(allocationsRemaining>0)--allocationsRemaining;
    if(void* value=std::malloc(count?count:1))return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept {std::free(value);}
void operator delete(void* value,std::size_t) noexcept {std::free(value);}
int main() {
    using namespace fo3scene;
    using Result=ResidentInteractionAdmissionResult;
    using Hazard=ResidentInteractionHazard;
    ResidentInteractionActivity activity;activity.ready=true;
    std::vector<uint32_t> affected{10,20,30};
    unsigned queries=0;uint32_t blocked=0;Hazard hazard=Hazard::None;
    auto query=[&](uint32_t reference) noexcept {++queries;return reference==blocked?hazard:Hazard::None;};
    assert(ResidentInteractionAdmission::Check(affected,activity,query)==Result::Ready&&queries==3);
    assert(!ResidentInteractionAdmission::Prepare(0,affected,activity,query));
    for(const auto& invalid:std::vector<std::vector<uint32_t>>{{},{0},{10,10},{20,10}}) {
        queries=0;
        assert(ResidentInteractionAdmission::Check(invalid,activity,query)==Result::InvalidAffected&&queries==0);
        assert(!ResidentInteractionAdmission::Prepare(1,invalid,activity,query));
    }
    for(bool ResidentInteractionActivity::*member:{&ResidentInteractionActivity::dialogue,
            &ResidentInteractionActivity::barter,&ResidentInteractionActivity::pipboy,
            &ResidentInteractionActivity::message,&ResidentInteractionActivity::lockpick,
            &ResidentInteractionActivity::lockpickLatch,&ResidentInteractionActivity::loot,
            &ResidentInteractionActivity::weaponHandling,&ResidentInteractionActivity::shoulderTransfer}) {
        activity.*member=true;queries=0;
        assert(ResidentInteractionAdmission::Check(affected,activity,query)==Result::Busy&&queries==0);
        assert(!ResidentInteractionAdmission::Prepare(1,affected,activity,query));
        assert(activity.*member);activity.*member=false;
    }
    activity.ready=false;assert(ResidentInteractionAdmission::Check(affected,activity,query)==Result::Busy);activity.ready=true;
    for(Hazard unsupported:{Hazard::UnknownReference,Hazard::Actor,Hazard::LooseObject,Hazard::HeldObject,
            Hazard::CollectedObject,Hazard::SwingDoor,Hazard::Furniture}) {
        blocked=20;hazard=unsupported;queries=0;
        assert(ResidentInteractionAdmission::Check(affected,activity,query)==Result::UnsupportedReference&&queries==2);
        assert(!ResidentInteractionAdmission::Prepare(1,affected,activity,query));
    }
    // Out-of-scope physical references do not block unrelated static changes.
    blocked=99;hazard=Hazard::HeldObject;
    auto guard=ResidentInteractionAdmission::Prepare(7,affected,activity,query);
    assert(guard&&guard->Ticket()==7);
    affected={99}; // The guard owns its original complete scope, not this buffer.
    failAllocation=true;
    assert(guard->Current(7,activity,query)==Result::Ready);
    assert(guard->Current(0,activity,query)==Result::WrongTicket);
    assert(guard->Current(8,activity,query)==Result::WrongTicket);
    activity.barter=true;assert(guard->Current(7,activity,query)==Result::Busy);activity.barter=false;
    blocked=30;assert(guard->Current(7,activity,query)==Result::UnsupportedReference);
    blocked=99;failAllocation=false;
    failAllocation=true;
    assert(!ResidentInteractionAdmission::Prepare(8,affected,activity,query));
    failAllocation=false;blocked=0;
    // Fail either the owner allocation or the subsequent affected-list copy.
    for(int budget:{0,1}) {
        bool failed=false;allocationsRemaining=budget;
        try {ResidentInteractionAdmission::Prepare(8,affected,activity,query);}
        catch(const std::bad_alloc&){failed=true;}
        allocationsRemaining=-1;
        assert(failed&&affected==std::vector<uint32_t>{99}&&guard->Ticket()==7);
    }

    // A late modal menu blocks the actual exchange gate without moving values;
    // once applied, rollback must remain available even if interaction is busy.
    std::vector<int> live{1,2},incoming{3};const auto* original=live.data();
    VisualPublication publication;using Exchange=ResourceExchange<std::vector<int>>;
    Exchange::Payload payload{std::move(incoming)};
    auto exchange=Exchange::Prepare(publication,std::tie(live),payload);
    auto commit=[&]() noexcept {
        return guard->Current(7,activity,query)==Result::Ready&&exchange->Commit(publication,std::tie(live));
    };
    activity.loot=true;assert(!commit()&&live.data()==original&&live.size()==2);activity.loot=false;
    failAllocation=true;assert(commit());
    activity.lockpick=true;assert(exchange->Rollback(publication,std::tie(live)));failAllocation=false;
    assert(live.data()==original&&live==std::vector<int>({1,2}));
    std::cout<<"Resident interaction preflight, late hazards, owned scope and allocation-free checks passed\n";
}
