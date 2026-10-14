#pragma once
#include "fo3-scene-preparation.h"
#include <cstdint>
#include <limits>
#include <type_traits>

namespace fo3scene {
// Render-thread reservation. Worker flags are acquired by the platform caller;
// completed results remain parked in their original queues, never published by
// this lane. A busy bit means CPU execution, not a queued or completed payload.
struct ResidentAssetActivity {
    bool cells=false,collision=false,lod=false,terrain=false,npc=false,legacy=false;
    bool Quiescent()const noexcept{return !(cells||collision||lod||terrain||npc||legacy);}
};
template<class Result>
class ResidentPreparationLane {
public:
    using Ticket=uint64_t;
    explicit ResidentPreparationLane(Ticket initial=0):serial_(initial){}
    ResidentPreparationLane(const ResidentPreparationLane&)=delete;
    ResidentPreparationLane& operator=(const ResidentPreparationLane&)=delete;
    Ticket Acquire()noexcept {
        if(Blocked()||serial_==std::numeric_limits<Ticket>::max())return 0;
        ticket_=++serial_;phase_=Phase::Draining;started_=false;return ticket_;
    }
    bool Blocked()const noexcept{return phase_!=Phase::Idle;}
    bool Owns(Ticket ticket)const noexcept{return ticket&&ticket==ticket_&&Blocked();}
    bool Exclusive(Ticket ticket)const noexcept{return Owns(ticket)&&phase_==Phase::Exclusive;}
    bool Poll(Ticket ticket,ResidentAssetActivity activity)noexcept {
        if(!Owns(ticket)||phase_==Phase::Cancelling)return false;
        if(phase_==Phase::Draining&&activity.Quiescent())phase_=Phase::Exclusive;
        return Exclusive(ticket);
    }
    template<class Work> bool Start(Ticket ticket,Work work) {
        if(!Exclusive(ticket)||started_)return false;
        if(!job_.Start(std::move(work)))return false;
        started_=true;return true;
    }
    bool Ready(Ticket ticket)const {return Exclusive(ticket)&&job_.Ready();}
    bool Successful(Ticket ticket)const{return Ready(ticket)&&job_.Successful();}
    bool Take(Ticket ticket,std::unique_ptr<Result>& out) {
        static_assert(std::is_nothrow_move_constructible_v<Result>);
        if(!Successful(ticket))return false;
        // Allocate before moving so allocation failure leaves the result owned.
        std::unique_ptr<Result> next;
        try {next=std::make_unique<Result>(std::move(job_.Get()));}
        catch(...){return false;}
        job_.Reset();out=std::move(next);return true;
    }
    bool Release(Ticket ticket)noexcept {
        if(!Exclusive(ticket)||job_.Active())return false;
        Clear();return true;
    }
    // Cooperative cancellation never joins unfinished work on an ordinary
    // frame. Poll Cancel with the same ticket until it returns true. Scheduling
    // stays blocked throughout, including when cancellation loses a finish race.
    bool Cancel(Ticket ticket) {
        if(!Owns(ticket))return false;
        phase_=Phase::Cancelling;job_.RequestCancel();
        if(!job_.TryReset())return false;
        Clear();return true;
    }
    // Scene/context teardown may join; it must precede referenced cache teardown.
    void Reset(){job_.Reset();Clear();}
private:
    enum class Phase{Idle,Draining,Exclusive,Cancelling};
    void Clear()noexcept{phase_=Phase::Idle;ticket_=0;started_=false;}
    Preparation<Result> job_;
    Ticket serial_=0,ticket_=0;
    Phase phase_=Phase::Idle;
    bool started_=false;
};
} // namespace fo3scene
