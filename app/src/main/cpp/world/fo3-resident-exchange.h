#pragma once
#include <cstdint>

namespace fo3scene {
enum class ResidentExchangeStage : uint8_t {
    Canonical,Collision,Lights,Visuals,Doors,Publication,Complete,Preflight,Verification
};
enum class ResidentExchangeStatus : uint8_t {Rejected,Committed,RolledBack,RecoveryFailed};
struct ResidentExchangeResult {
    ResidentExchangeStatus status=ResidentExchangeStatus::Rejected;
    ResidentExchangeStage failedAt=ResidentExchangeStage::Preflight;
    uint32_t applied=0,recoveryFailures=0;
};
constexpr uint32_t ResidentExchangeBit(ResidentExchangeStage stage) noexcept {
    return uint32_t{1}<<static_cast<unsigned>(stage);
}
// Single render-thread turn. The concrete adapter must exclude observers and
// effect/save delivery. A false Apply must leave that component unchanged.
// No script interpretation, event pumping or asynchronous work belongs here.
template<class Adapter>
ResidentExchangeResult ExecuteResidentExchange(Adapter& adapter) noexcept {
    static_assert(noexcept(adapter.Preflight())&&noexcept(adapter.Published()));
    static_assert(noexcept(adapter.Apply(ResidentExchangeStage::Canonical))&&
        noexcept(adapter.Undo(ResidentExchangeStage::Canonical)));
    using Stage=ResidentExchangeStage;
    using Status=ResidentExchangeStatus;
    ResidentExchangeResult result;
    if(!adapter.Preflight())return result;
    for(unsigned i=0;i<static_cast<unsigned>(Stage::Complete);++i) {
        const auto stage=static_cast<Stage>(i);
        if(!adapter.Apply(stage)){result.failedAt=stage;break;}
        result.applied|=ResidentExchangeBit(stage);
    }
    if(result.applied==(ResidentExchangeBit(Stage::Complete)-1)) {
        if(adapter.Published()){result.status=Status::Committed;result.failedAt=Stage::Complete;return result;}
        result.failedAt=Stage::Verification;
    }
    if(!(result.applied&ResidentExchangeBit(Stage::Canonical)))return result;
    // Resource owners undo in reverse order. Reference publication must wait
    // until canonical rollback establishes its precomputed revision N+2.
    for(unsigned i=static_cast<unsigned>(Stage::Doors);i>=static_cast<unsigned>(Stage::Collision);--i) {
        const auto stage=static_cast<Stage>(i);
        if((result.applied&ResidentExchangeBit(stage))&&!adapter.Undo(stage))
            result.recoveryFailures|=ResidentExchangeBit(stage);
    }
    if(!adapter.Undo(Stage::Canonical))
        result.recoveryFailures|=ResidentExchangeBit(Stage::Canonical)|ResidentExchangeBit(Stage::Publication);
    else if(!adapter.Undo(Stage::Publication))
        result.recoveryFailures|=ResidentExchangeBit(Stage::Publication);
    result.status=result.recoveryFailures?Status::RecoveryFailed:Status::RolledBack;
    return result;
}
} // namespace fo3scene
