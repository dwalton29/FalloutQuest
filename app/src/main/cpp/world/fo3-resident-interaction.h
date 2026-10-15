#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace fo3scene {
// Read-only render-thread admission. This does not close menus, release hands,
// mutate Player, or claim ownership of actor/physics/navigation state.
struct ResidentInteractionActivity {
    bool ready=false,dialogue=false,barter=false,pipboy=false,message=false;
    bool lockpick=false,lockpickLatch=false,loot=false,weaponHandling=false;
    bool shoulderTransfer=false;
    bool Idle() const noexcept {
        return ready&&!dialogue&&!barter&&!pipboy&&!message&&!lockpick&&
            !lockpickLatch&&!loot&&!weaponHandling&&!shoulderTransfer;
    }
};
enum class ResidentInteractionHazard : uint8_t {
    None, UnknownReference, Actor, LooseObject, HeldObject, CollectedObject,
    SwingDoor, Furniture
};
enum class ResidentInteractionAdmissionResult : uint8_t {
    Ready, InvalidAffected, Busy, UnsupportedReference, WrongTicket
};
// Own only the complete affected-reference list, never live resource pointers.
// Check is allocation-free. Query must be a read-only, noexcept lookup.
class ResidentInteractionAdmission {
public:
    ResidentInteractionAdmission(const ResidentInteractionAdmission&)=delete;
    ResidentInteractionAdmission& operator=(const ResidentInteractionAdmission&)=delete;
    uint64_t Ticket() const noexcept {return ticket_;}
    template<class Query>
    static ResidentInteractionAdmissionResult Check(const std::vector<uint32_t>& affected,
            ResidentInteractionActivity activity,Query query) noexcept {
        static_assert(noexcept(query(uint32_t{})));
        if(affected.empty())return ResidentInteractionAdmissionResult::InvalidAffected;
        for(size_t i=0;i<affected.size();++i)
            if(!affected[i]||(i&&affected[i]<=affected[i-1]))
                return ResidentInteractionAdmissionResult::InvalidAffected;
        if(!activity.Idle())return ResidentInteractionAdmissionResult::Busy;
        for(uint32_t reference:affected)
            if(query(reference)!=ResidentInteractionHazard::None)
                return ResidentInteractionAdmissionResult::UnsupportedReference;
        return ResidentInteractionAdmissionResult::Ready;
    }
    template<class Query>
    static std::unique_ptr<ResidentInteractionAdmission> Prepare(uint64_t ticket,
            const std::vector<uint32_t>& affected,ResidentInteractionActivity activity,Query query) {
        if(!ticket||Check(affected,activity,query)!=ResidentInteractionAdmissionResult::Ready)return {};
        auto next=std::unique_ptr<ResidentInteractionAdmission>(new ResidentInteractionAdmission);
        next->ticket_=ticket;next->affected_=affected;return next;
    }
    template<class Query>
    ResidentInteractionAdmissionResult Current(uint64_t ticket,
            ResidentInteractionActivity activity,Query query) const noexcept {
        if(!ticket||ticket!=ticket_)return ResidentInteractionAdmissionResult::WrongTicket;
        return Check(affected_,activity,query);
    }
private:
    ResidentInteractionAdmission()=default;
    uint64_t ticket_=0;
    std::vector<uint32_t> affected_;
};
} // namespace fo3scene
