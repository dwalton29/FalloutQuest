#pragma once
#include "fo3-resident-exchange.h"
#include "fo3-resident-gpu-stage.h"
#include <memory>
#include <type_traits>
#include <utility>

namespace fo3scene {
enum class ResidentWorkflowPhase {Idle,Draining,Cpu,Gpu,Ready,Cancelling,Executing,Settled,Finalizing,RecoveryFailed};
enum class ResidentCpuProgress {Pending,Ready,Failed};
// One render-thread owner across asynchronous preparation. Adapters retain
// their worker/GPU slots; only immutable projections cross into worker code.
// Ready never means admitted: execution remains an explicit caller decision.
template<class Script,class Packet,class Request>
class ResidentWorkflow {
    static_assert(std::is_nothrow_default_constructible_v<Request>&&std::is_nothrow_move_assignable_v<Request>);
public:
    using Phase=ResidentWorkflowPhase;
    ResidentWorkflow()=default;
    ResidentWorkflow(const ResidentWorkflow&)=delete;
    ResidentWorkflow& operator=(const ResidentWorkflow&)=delete;
    bool Active() const noexcept {return phase_!=Phase::Idle;}
    Phase State() const noexcept {return phase_;}
    uint64_t Ticket() const noexcept {return ticket_;}
    const ResidentExchangeResult& Outcome() const noexcept {return outcome_;}
    template<class Adapter>
    bool Begin(uint64_t ticket,std::unique_ptr<Script>& script,Request request,Adapter& adapter) {
        if(Active()||!ticket||!script||!adapter.Current(ticket,*script))return false;
        request_=std::move(request);script_=std::move(script);ticket_=ticket;
        outcome_={};phase_=Phase::Draining;return true;
    }
    bool RequestCancel(uint64_t ticket) noexcept {
        if(!ticket||ticket!=ticket_||!Active()||phase_==Phase::Executing||phase_==Phase::Finalizing||
            phase_==Phase::Settled||phase_==Phase::RecoveryFailed)return false;
        phase_=Phase::Cancelling;return true;
    }
    template<class Adapter>
    void Pump(Adapter& adapter) noexcept {
        if(!Active()||phase_==Phase::Executing||phase_==Phase::Settled||phase_==Phase::Finalizing||phase_==Phase::RecoveryFailed)return;
        try {
            if(phase_==Phase::Cancelling){
                // Keep the script, packet and ticket until cooperative worker
                // cancellation and private GPU/resource disposal have finished.
                if(adapter.Cancel(ticket_))Clear();
                return;
            }
            if(!adapter.Current(ticket_,*script_)){RequestCancel(ticket_);return;}
            switch(phase_) {
                case Phase::Draining:
                    if(!adapter.Drain(ticket_))return;
                    if(!adapter.StartCpu(ticket_,request_,*script_)){RequestCancel(ticket_);return;}
                    phase_=Phase::Cpu;return;
                case Phase::Cpu: {
                    const auto progress=adapter.CpuProgress(ticket_);
                    if(progress==ResidentCpuProgress::Pending)return;
                    if(progress==ResidentCpuProgress::Failed||!adapter.TakeCpu(ticket_,packet_)||!packet_||
                        !adapter.BeginGpu(packet_)||packet_){RequestCancel(ticket_);return;}
                    phase_=Phase::Gpu;return;
                }
                case Phase::Gpu: {
                    const auto progress=adapter.AdvanceGpu(packet_);
                    if(progress==GpuStageResult::Pending)return;
                    if(progress!=GpuStageResult::Ready||!packet_){RequestCancel(ticket_);return;}
                    phase_=Phase::Ready;return;
                }
                default:return;
            }
        }catch(...){RequestCancel(ticket_);}
    }
    template<class Adapter>
    ResidentExchangeResult Execute(uint64_t ticket,Adapter& adapter) noexcept {
        static_assert(noexcept(adapter.Execute(std::declval<Packet&>(),std::declval<Script&>())));
        if(!ticket||ticket!=ticket_||phase_!=Phase::Ready||!packet_||!script_)return {};
        phase_=Phase::Executing; // No cancellation or reentrant second commit.
        outcome_=adapter.Execute(*packet_,*script_);
        phase_=outcome_.status==ResidentExchangeStatus::RecoveryFailed?Phase::RecoveryFailed:Phase::Settled;
        // Even rejection/rollback retains owners and exclusion. Reconciliation
        // and finalization are a separate boundary, never ordinary cancellation.
        return outcome_;
    }
    bool OwnsExchange(uint64_t ticket,const Packet& packet,const Script& script) const noexcept {
        return phase_==Phase::Executing&&ticket&&ticket==ticket_&&packet_.get()==&packet&&script_.get()==&script;
    }
    // External interruption never executes a Ready candidate. Preparation is
    // cancelled cooperatively; an already settled exchange must finalize.
    template<class Adapter>
    bool Interrupt(Adapter& adapter) noexcept {
        if(!Active())return true;
        if(phase_==Phase::Settled)return Finalize(ticket_,adapter);
        if(!RequestCancel(ticket_))return false;
        Pump(adapter);return !Active();
    }
    template<class Adapter>
    bool Finalize(uint64_t ticket,Adapter& adapter) noexcept {
        static_assert(noexcept(adapter.CanFinalize(ticket,std::declval<Packet&>(),std::declval<Script&>(),outcome_))&&
            noexcept(adapter.Retire(std::declval<Packet&>()))&&noexcept(adapter.Release(ticket))&&noexcept(adapter.FinalizationFailed()));
        if(!ticket||ticket!=ticket_||phase_!=Phase::Settled||!packet_||!script_)return false;
        phase_=Phase::Finalizing;
        // Reconciliation can allocate while only private queues change. A
        // failure keeps all settled owners/exclusion and can be retried.
        try {
            if(!adapter.CanFinalize(ticket,*packet_,*script_,outcome_)||!adapter.Reconcile(outcome_)||
                !adapter.CanFinalize(ticket,*packet_,*script_,outcome_)){
                phase_=Phase::Settled;return false;
            }
        }catch(...){phase_=Phase::Settled;return false;}
        adapter.Retire(*packet_);
        if(!adapter.Release(ticket)){
            phase_=Phase::RecoveryFailed;adapter.FinalizationFailed();return false;
        }
        // The frame gate still sees Finalizing until the canonical displaced
        // payload and packet have been destroyed. No observer runs in this gap.
        Clear();return true;
    }
    // Scene/context teardown only, after joining the adapter's worker and while
    // the original GL context/resource caches are still alive.
    void ResetAfterDrain() noexcept {Clear();outcome_={};}
private:
    void Clear() noexcept {packet_.reset();script_.reset();request_=Request{};ticket_=0;phase_=Phase::Idle;}
    Phase phase_=Phase::Idle;
    uint64_t ticket_=0;
    Request request_{};
    std::unique_ptr<Script> script_;
    std::unique_ptr<Packet> packet_;
    ResidentExchangeResult outcome_;
};
} // namespace fo3scene
