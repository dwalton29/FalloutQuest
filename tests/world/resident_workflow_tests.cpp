#include "fo3-resident-workflow.h"
#include "fo3-resident-preparation-lane.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace fo3scene;
struct Script {
    int& destroyed;int value=10;
    ~Script(){++destroyed;}
};
struct Lease {int& destroyed;~Lease(){++destroyed;}};
struct Packet {std::unique_ptr<Lease> lease;int value=0;};
struct Request {int expected=10;};
using Workflow=ResidentWorkflow<Script,Packet,Request>;
using Phase=ResidentWorkflowPhase;
template<class Predicate> void wait(Predicate predicate) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!predicate()){assert(std::chrono::steady_clock::now()<end);std::this_thread::yield();}
}
struct Adapter {
    ResidentPreparationLane<Packet> lane;
    std::unique_ptr<Packet> gpu;
    std::atomic<bool> entered{false},finish{true},cancelObserved{false};
    bool current=true,workersBusy=false,holdCancellation=false,throwStart=false;
    int fail=0,started=0,parts=0,cancels=0,executions=0,packetDestroyed=0;
    bool finalizeCurrent=true,reconcileReady=true,throwReconcile=false,releaseFails=false,fatal=false;
    bool staleAfterReconcile=false;
    int reconciled=0,retired=0,releases=0;
    ResidentExchangeStatus outcome=ResidentExchangeStatus::Committed;
    Workflow* workflow=nullptr;
    bool Current(uint64_t ticket,const Script& script) const noexcept {
        return current&&lane.Owns(ticket)&&script.value==10;
    }
    bool Drain(uint64_t ticket){ResidentAssetActivity activity;activity.npc=workersBusy;return lane.Poll(ticket,activity);}
    bool StartCpu(uint64_t ticket,const Request& request,const Script& script) {
        assert(request.expected==script.value);
        if(throwStart)throw std::bad_alloc();
        if(fail==1)return false;
        ++started;const int value=script.value;
        return lane.Start(ticket,[this,value](Packet& packet,const std::atomic<bool>& cancelled){
            entered=true;
            while(!finish.load()){
                if(cancelled.load()){cancelObserved=true;if(!holdCancellation)return false;}
                std::this_thread::yield();
            }
            if(cancelled.load()||fail==2)return false;
            packet.value=value;packet.lease=std::unique_ptr<Lease>(new Lease{packetDestroyed});return true;
        });
    }
    ResidentCpuProgress CpuProgress(uint64_t ticket) {
        if(!lane.Ready(ticket))return ResidentCpuProgress::Pending;
        return lane.Successful(ticket)?ResidentCpuProgress::Ready:ResidentCpuProgress::Failed;
    }
    bool TakeCpu(uint64_t ticket,std::unique_ptr<Packet>& packet){
        if(fail==3||!lane.Take(ticket,packet))return false;
        if(fail==7)throw std::bad_alloc();
        return true;
    }
    bool BeginGpu(std::unique_ptr<Packet>& packet){
        if(fail==4)return false;
        gpu=std::move(packet);
        if(fail==8)throw std::bad_alloc();
        return true;
    }
    GpuStageResult AdvanceGpu(std::unique_ptr<Packet>& packet) {
        if(fail==5)return GpuStageResult::Failed;
        if(fail==9)throw std::runtime_error("partial GPU upload");
        assert(gpu&&gpu->value==10);
        if(++parts<2)return GpuStageResult::Pending;
        packet=std::move(gpu);return GpuStageResult::Ready;
    }
    bool Cancel(uint64_t ticket){++cancels;gpu.reset();return lane.Cancel(ticket);}
    ResidentExchangeResult Execute(Packet& packet,Script& script) noexcept {
        ++executions;assert(workflow&&workflow->OwnsExchange(workflow->Ticket(),packet,script));
        assert(!workflow->OwnsExchange(workflow->Ticket()+1,packet,script));
        assert(!workflow->RequestCancel(workflow->Ticket()));
        assert(!workflow->Interrupt(*this));
        assert(workflow->Execute(workflow->Ticket(),*this).status==ResidentExchangeStatus::Rejected);
        workflow->Pump(*this);assert(executions==1);
        return {outcome,ResidentExchangeStage::Complete,0,0};
    }
    bool CanFinalize(uint64_t ticket,const Packet&,const Script&,const ResidentExchangeResult&) const noexcept {
        return finalizeCurrent&&lane.CanRelease(ticket);
    }
    bool Reconcile(const ResidentExchangeResult&) {
        ++reconciled;
        assert(workflow->State()==Phase::Finalizing&&!workflow->RequestCancel(workflow->Ticket()));
        assert(!workflow->Interrupt(*this));
        assert(!workflow->Finalize(workflow->Ticket(),*this));
        if(throwReconcile)throw std::bad_alloc();
        if(staleAfterReconcile)finalizeCurrent=false;
        return reconcileReady;
    }
    void Retire(Packet& packet) noexcept {++retired;packet.lease.reset();}
    bool Release(uint64_t ticket) noexcept {++releases;return !releaseFails&&lane.Release(ticket);}
    void FinalizationFailed() noexcept {fatal=true;}
    void Reset(Workflow& work){lane.Reset();gpu.reset();work.ResetAfterDrain();}
};
static void ready(Workflow& work,Adapter& adapter) {
    wait([&]{work.Pump(adapter);return work.State()==Phase::Ready;});
}
int main() {
    // Drain without losing the logical owner. Ready does not auto-commit,
    // dispose owners, release the lane, save or deliver any effects.
    for(auto outcome:{ResidentExchangeStatus::Committed,ResidentExchangeStatus::RolledBack,
                     ResidentExchangeStatus::Rejected,ResidentExchangeStatus::RecoveryFailed}) {
        int destroyed=0;Workflow work;Adapter adapter;adapter.workflow=&work;
        auto script=std::unique_ptr<Script>(new Script{destroyed});const auto* identity=script.get();
        const auto ticket=adapter.lane.Acquire();adapter.workersBusy=true;
        assert(!work.Begin(0,script,{},adapter)&&script.get()==identity);
        assert(!work.Begin(ticket+1,script,{},adapter)&&script.get()==identity);
        assert(work.Begin(ticket,script,{},adapter)&&!script&&work.Active());
        auto other=std::unique_ptr<Script>(new Script{destroyed});
        assert(!work.Begin(ticket,other,{},adapter)&&other);other.reset();assert(destroyed==1);
        for(int i=0;i<4;++i)work.Pump(adapter);
        assert(adapter.started==0&&work.State()==Phase::Draining&&adapter.lane.Blocked());
        assert(!work.RequestCancel(ticket+1));adapter.workersBusy=false;ready(work,adapter);
        assert(destroyed==1&&adapter.started==1&&adapter.parts==2&&adapter.packetDestroyed==0);
        for(int i=0;i<4;++i)work.Pump(adapter);
        assert(adapter.executions==0&&adapter.lane.Blocked());
        assert(work.Execute(ticket+1,adapter).status==ResidentExchangeStatus::Rejected);
        adapter.outcome=outcome;assert(work.Execute(ticket,adapter).status==outcome);
        assert(adapter.executions==1&&work.Outcome().status==outcome);
        assert(work.State()==(outcome==ResidentExchangeStatus::RecoveryFailed?Phase::RecoveryFailed:Phase::Settled));
        assert(!work.RequestCancel(ticket));work.Pump(adapter);
        assert(work.Execute(ticket,adapter).status==ResidentExchangeStatus::Rejected);
        assert(destroyed==1&&adapter.packetDestroyed==0&&adapter.lane.Blocked());
        if(outcome==ResidentExchangeStatus::RecoveryFailed){
            assert(!work.Finalize(ticket,adapter)&&adapter.reconciled==0);
            adapter.Reset(work);
        }else{
            assert(!work.Finalize(ticket+1,adapter));
            adapter.finalizeCurrent=false;
            assert(!work.Finalize(ticket,adapter)&&adapter.reconciled==0&&adapter.retired==0);
            adapter.finalizeCurrent=true;adapter.reconcileReady=false;
            assert(!work.Finalize(ticket,adapter)&&adapter.retired==0&&destroyed==1&&adapter.lane.Blocked());
            adapter.reconcileReady=true;adapter.throwReconcile=true;
            assert(!work.Finalize(ticket,adapter)&&work.State()==Phase::Settled&&adapter.retired==0);
            adapter.throwReconcile=false;
            adapter.staleAfterReconcile=true;
            assert(!work.Finalize(ticket,adapter)&&adapter.retired==0&&adapter.lane.Blocked());
            adapter.staleAfterReconcile=false;adapter.finalizeCurrent=true;
            assert(work.Finalize(ticket,adapter)&&adapter.retired==1&&adapter.releases==1&&!adapter.fatal);
            assert(!work.Finalize(ticket,adapter));
        }
        assert(!work.Active()&&destroyed==2&&adapter.packetDestroyed==1);
    }
    // Every private preparation failure and thrown start is cancellable with
    // no canonical execution and exactly-once reclamation.
    for(int failure=1;failure<=9;++failure){
        int destroyed=0;Workflow work;Adapter adapter;adapter.workflow=&work;
        adapter.fail=failure;adapter.throwStart=failure==6;
        auto script=std::unique_ptr<Script>(new Script{destroyed});
        const auto ticket=adapter.lane.Acquire();assert(work.Begin(ticket,script,{},adapter));
        wait([&]{work.Pump(adapter);return !work.Active();});
        assert(!adapter.lane.Blocked()&&destroyed==1&&adapter.executions==0);
        assert(adapter.packetDestroyed==((failure>=3&&failure!=6)?1:0));
        adapter.Reset(work);assert(destroyed==1);
    }
    // Cancellation cannot return script ownership while a worker still holds
    // immutable inputs. A finish race must not resurrect a cancelled result.
    {
        int destroyed=0;Workflow work;Adapter adapter;adapter.finish=false;adapter.holdCancellation=true;
        auto script=std::unique_ptr<Script>(new Script{destroyed});
        const auto ticket=adapter.lane.Acquire();assert(work.Begin(ticket,script,{},adapter));
        work.Pump(adapter);wait([&]{return adapter.entered.load();});
        assert(work.RequestCancel(ticket));work.Pump(adapter);
        wait([&]{return adapter.cancelObserved.load();});
        assert(work.State()==Phase::Cancelling&&destroyed==0&&adapter.lane.Blocked());
        adapter.finish=true;wait([&]{work.Pump(adapter);return !work.Active();});
        assert(destroyed==1&&adapter.executions==0&&adapter.packetDestroyed==0);
    }
    // A stale canonical/scene context during any preparation phase cancels;
    // it cannot reach the executor even if a GPU packet already exists.
    for(Phase phase:{Phase::Draining,Phase::Cpu,Phase::Gpu,Phase::Ready}){
        int destroyed=0;Workflow work;Adapter adapter;
        auto script=std::unique_ptr<Script>(new Script{destroyed});
        const auto ticket=adapter.lane.Acquire();assert(work.Begin(ticket,script,{},adapter));
        wait([&]{if(work.State()!=phase)work.Pump(adapter);return work.State()==phase;});
        adapter.current=false;work.Pump(adapter);
        assert(work.State()==Phase::Cancelling&&destroyed==0);
        wait([&]{work.Pump(adapter);return !work.Active();});
        assert(destroyed==1&&!adapter.lane.Blocked()&&adapter.executions==0);
    }
    // Teardown joins a cooperative worker before dropping the script owner.
    {
        int destroyed=0;Workflow work;Adapter adapter;adapter.finish=false;
        auto script=std::unique_ptr<Script>(new Script{destroyed});
        const auto ticket=adapter.lane.Acquire();assert(work.Begin(ticket,script,{},adapter));
        work.Pump(adapter);wait([&]{return adapter.entered.load();});adapter.Reset(work);
        assert(adapter.cancelObserved&&destroyed==1&&!work.Active()&&!adapter.lane.Blocked());
    }
    // An unexpected release failure after no-throw retirement latches failure;
    // it cannot report resumed gameplay or retry against already retired plans.
    {
        int destroyed=0;Workflow work;Adapter adapter;adapter.workflow=&work;
        auto script=std::unique_ptr<Script>(new Script{destroyed});
        const auto ticket=adapter.lane.Acquire();assert(work.Begin(ticket,script,{},adapter));
        ready(work,adapter);work.Execute(ticket,adapter);adapter.releaseFails=true;
        assert(!work.Finalize(ticket,adapter)&&adapter.fatal&&work.State()==Phase::RecoveryFailed);
        assert(destroyed==0&&adapter.packetDestroyed==1&&adapter.lane.Blocked());
        assert(!work.Finalize(ticket,adapter)&&!work.RequestCancel(ticket));
        adapter.Reset(work);assert(destroyed==1);
    }
    // External interruption cancels every preparation phase, never executing a
    // Ready candidate; settled outcomes instead take the guarded finalizer.
    for(auto phase:{Phase::Draining,Phase::Cpu,Phase::Gpu,Phase::Ready}){
        int destroyed=0;Workflow work;Adapter adapter;adapter.workflow=&work;
        auto script=std::unique_ptr<Script>(new Script{destroyed});
        const auto ticket=adapter.lane.Acquire();assert(work.Begin(ticket,script,{},adapter));
        wait([&]{if(work.State()!=phase)work.Pump(adapter);return work.State()==phase;});
        wait([&]{return work.Interrupt(adapter);});
        assert(!work.Active()&&!adapter.lane.Blocked()&&destroyed==1&&adapter.executions==0);
        assert(work.Interrupt(adapter));
    }
    for(auto outcome:{ResidentExchangeStatus::Committed,ResidentExchangeStatus::RolledBack,
                     ResidentExchangeStatus::Rejected,ResidentExchangeStatus::RecoveryFailed}){
        int destroyed=0;Workflow work;Adapter adapter;adapter.workflow=&work;
        auto script=std::unique_ptr<Script>(new Script{destroyed});
        const auto ticket=adapter.lane.Acquire();assert(work.Begin(ticket,script,{},adapter));
        ready(work,adapter);adapter.outcome=outcome;work.Execute(ticket,adapter);
        adapter.reconcileReady=false;
        assert(!work.Interrupt(adapter)&&destroyed==0&&work.Active());
        adapter.reconcileReady=true;
        if(outcome==ResidentExchangeStatus::RecoveryFailed){
            assert(!work.Interrupt(adapter)&&destroyed==0);adapter.Reset(work);
        }else{
            assert(work.Interrupt(adapter)&&destroyed==1&&!adapter.lane.Blocked());
            assert(adapter.retired==1&&adapter.releases==1&&adapter.executions==1);
        }
    }
    {
        int destroyed=0;Workflow work;Adapter adapter;adapter.workflow=&work;
        adapter.finish=false;adapter.holdCancellation=true;
        auto script=std::unique_ptr<Script>(new Script{destroyed});
        const auto ticket=adapter.lane.Acquire();assert(work.Begin(ticket,script,{},adapter));
        work.Pump(adapter);wait([&]{return adapter.entered.load();});
        assert(!work.Interrupt(adapter));
        wait([&]{return adapter.cancelObserved.load();});
        for(int i=0;i<5;++i)assert(!work.Interrupt(adapter));
        assert(destroyed==0&&work.Active()&&adapter.lane.Blocked()&&adapter.executions==0);
        adapter.finish=true;wait([&]{return work.Interrupt(adapter);});
        assert(destroyed==1&&!adapter.lane.Blocked());
    }
    std::cout<<"resident workflow retains logical/private owners through async preparation, cancellation and settled outcomes\n";
}
