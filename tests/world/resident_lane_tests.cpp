#include "fo3-resident-preparation-lane.h"
#include "fo3-resident-input-barrier.h"
#include "fo3-visual-exchange.h"
#include <cassert>
#include <cstdlib>
#include <new>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
// Fail only this thread's packet allocation; the joined worker owns its inputs.
static thread_local bool rejectAllocation=false;
void* operator new(std::size_t size){
    if(rejectAllocation)throw std::bad_alloc();
    if(void* p=std::malloc(size?size:1))return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
using namespace fo3scene;
struct Result {std::vector<int> values;};
template<class Predicate> void wait(Predicate predicate){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!predicate()){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
}
int main(){
    ResidentInputBarrier input;
    rejectAllocation=true;
    assert(input.Allow(false,false)); // Normal held controls remain unchanged.
    assert(!input.Allow(true,false)&&!input.Allow(true,true));
    assert(!input.Allow(false,false)&&!input.Allow(false,false));
    assert(!input.Allow(false,true)); // Release frame never creates an action.
    assert(input.Allow(false,false));
    assert(!input.Allow(true,false)&&!input.Allow(false,true));
    assert(!input.Allow(true,true)&&!input.Allow(false,false)); // New reservation rearms.
    assert(!input.Allow(false,true)&&input.Allow(false,false));
    rejectAllocation=false;
    ResidentPreparationLane<Result> lane;
    assert(!lane.Blocked()&&!lane.Release(0)&&!lane.Cancel(0)&&!lane.Poll(0,{}));
    const auto token=lane.Acquire();assert(token&&lane.Blocked()&&!lane.Acquire());
    for(int bit=0;bit<6;++bit){
        ResidentAssetActivity a;
        switch(bit){case 0:a.cells=true;break;case 1:a.collision=true;break;
            case 2:a.lod=true;break;case 3:a.terrain=true;break;
            case 4:a.npc=true;break;case 5:a.legacy=true;break;}
        assert(!a.Quiescent()&&!lane.Poll(token,a)&&!lane.Exclusive(token));
    }
    int calls=0;
    auto work=[&](Result& result,const std::atomic<bool>&){++calls;result.values={1,2};return true;};
    assert(!lane.Start(token,work)&&calls==0);
    assert(!lane.Poll(token+1,{})&&!lane.Release(token+1)&&!lane.Cancel(token+1));
    assert(lane.Poll(token,{})&&lane.Exclusive(token)&&lane.Blocked());
    assert(lane.Start(token,work));
    wait([&]{return lane.Ready(token);});
    assert(lane.Successful(token)&&!lane.Start(token,work)&&!lane.Release(token));
    std::unique_ptr<Result> out=std::make_unique<Result>();out->values={99};
    assert(!lane.Take(token+1,out)&&out->values==std::vector<int>({99}));
    rejectAllocation=true;
    const bool allocationFailed=!lane.Take(token,out);
    rejectAllocation=false;
    assert(allocationFailed&&lane.Successful(token)&&out->values==std::vector<int>({99}));
    assert(lane.Take(token,out)&&out->values==std::vector<int>({1,2})&&calls==1);
    assert(lane.Blocked()&&!lane.Ready(token)&&!lane.Start(token,work)&&!lane.Take(token,out));
    assert(lane.Release(token)&&!lane.Blocked());
    // Stale handles cannot release, cancel or start a later owner's work.
    const auto newer=lane.Acquire();assert(newer>token);
    assert(!lane.Poll(token,{})&&!lane.Release(token)&&!lane.Cancel(token)&&!lane.Start(token,work));
    assert(lane.Cancel(newer)&&!lane.Blocked());
    // Cancellation remains nonblocking even when the worker deliberately stays
    // inside a decode after acknowledging cancellation. No sleeps/timing guesses.
    const auto cancelling=lane.Acquire();assert(lane.Poll(cancelling,{}));
    std::atomic<bool> entered{false},observed{false},release{false};
    assert(lane.Start(cancelling,[&](Result& result,const std::atomic<bool>& cancelled){
        entered.store(true,std::memory_order_release);
        while(!cancelled.load(std::memory_order_acquire))std::this_thread::yield();
        observed.store(true,std::memory_order_release);
        while(!release.load(std::memory_order_acquire))std::this_thread::yield();
        result.values={77};return true; // finish race cannot resurrect cancelled output
    }));
    wait([&]{return entered.load(std::memory_order_acquire);});
    assert(!lane.Cancel(cancelling));
    wait([&]{return observed.load(std::memory_order_acquire);});
    assert(lane.Blocked()&&!lane.Acquire()&&!lane.Exclusive(cancelling));
    assert(!lane.Poll(cancelling,{})&&!lane.Release(cancelling)&&!lane.Take(cancelling,out));
    assert(!lane.Cancel(cancelling));
    release.store(true,std::memory_order_release);
    wait([&]{return lane.Cancel(cancelling);});
    assert(!lane.Blocked()&&out->values==std::vector<int>({1,2}));
    // Failure/exception stay reserved until explicit cancellation, with no
    // output publication; a successful taken result still holds the GPU lane.
    for(bool throws:{false,true}){
        const auto t=lane.Acquire();assert(lane.Poll(t,{}));
        assert(lane.Start(t,[&](Result&,const auto&)->bool{if(throws)throw std::runtime_error("decode");return false;}));
        wait([&]{return lane.Ready(t);});
        assert(!lane.Successful(t)&&!lane.Take(t,out)&&!lane.Release(t)&&lane.Blocked());
        assert(lane.Cancel(t));
    }
    // Parked old results are not consumed while preparing. Cancellation resumes
    // the same ordinary scheduler payload; a later owner captures fresh inputs.
    std::vector<int> parked{5,6};
    const auto parkedToken=lane.Acquire();assert(lane.Poll(parkedToken,{}));
    assert(lane.Start(parkedToken,[](Result& r,const auto&){r.values={8};return true;}));
    wait([&]{return lane.Ready(parkedToken);});
    assert(parked==std::vector<int>({5,6}));
    assert(lane.Cancel(parkedToken)&&parked==std::vector<int>({5,6}));
    // Context teardown joins cooperative CPU cancellation before its captured
    // immutable inputs may disappear, and never reuses an old reservation ID.
    const auto teardown=lane.Acquire();assert(lane.Poll(teardown,{}));
    entered=false;observed=false;
    assert(lane.Start(teardown,[&](Result&,const auto& cancelled){
        entered=true;while(!cancelled.load(std::memory_order_acquire))std::this_thread::yield();observed=true;return false;
    }));
    wait([&]{return entered.load();});lane.Reset();assert(observed&& !lane.Blocked());
    assert(lane.Acquire()>teardown);lane.Reset();
    ResidentPreparationLane<Result> exhausted(UINT64_MAX-1);
    assert(exhausted.Acquire()==UINT64_MAX&&exhausted.Cancel(UINT64_MAX));
    assert(!exhausted.Acquire()&&!exhausted.Blocked());
    std::cout<<"resident lane drain, ownership, worker transfer and nonblocking cancellation passed\n";
}
