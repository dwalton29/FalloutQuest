#pragma once
#include "fo3-scene-publication.h"

namespace fo3scene {
// Canonical rollback restores old effective states under a NEW reference
// revision. Keep both the publication and its fast renderer cache on that same
// precomputed snapshot, never relabel an old snapshot or roll epochs backwards.
inline bool ValidResidentReferenceProjections(const fo3world::ReferenceStateSnapshot& before,
        const fo3world::ReferenceStateSnapshot& candidate,const fo3world::ReferenceStateSnapshot& rollback) {
    if(before.revision>std::numeric_limits<uint64_t>::max()-2||
        candidate.revision!=before.revision+1||rollback.revision!=before.revision+2||
        rollback.states!=before.states)return false;
    for(const auto* snapshot:{&before,&candidate,&rollback})for(const auto& entry:snapshot->states)
        if(!entry.first||(entry.second!=fo3world::ReferenceEnableState::Disabled&&
            entry.second!=fo3world::ReferenceEnableState::Enabled&&
            entry.second!=fo3world::ReferenceEnableState::Unavailable))return false;
    return true;
}
class ReferenceExchange {
public:
    using Snapshot=std::shared_ptr<const fo3world::ReferenceStateSnapshot>;
    ReferenceExchange(const ReferenceExchange&)=delete;
    ReferenceExchange& operator=(const ReferenceExchange&)=delete;
    static std::unique_ptr<ReferenceExchange> Prepare(ReferencePublication& publication,
            Snapshot& cache,Snapshot before,Snapshot candidate,Snapshot rollback) {
        if(!before||!candidate||!rollback||!publication.CanAdvance(2)||
            !publication.Accepts(publication.Stamp(),before->revision)||
            publication.Snapshot()!=before||cache!=before||
            !ValidResidentReferenceProjections(*before,*candidate,*rollback))return {};
        auto plan=std::unique_ptr<ReferenceExchange>(new ReferenceExchange);
        plan->publication_=&publication;plan->identity_=publication.identity_;plan->cache_=&cache;
        plan->stamp_=publication.Stamp();plan->before_=std::move(before);
        plan->candidate_=std::move(candidate);plan->rollback_=std::move(rollback);return plan;
    }
    bool Applied() const noexcept {return phase_==Phase::Applied;}
    // Before canonical commit: a read-only outer-transaction preflight.
    bool CanPublishBefore(const ReferencePublication& publication,const Snapshot& cache,
            uint64_t canonicalRevision) const noexcept {
        return phase_==Phase::Prepared&&canonicalRevision==before_->revision&&
            Matches(publication,cache,before_)&&publication.CanAdvance(2);
    }
    // After canonical commit/rollback: require that exact new revision.
    bool CanCommit(const ReferencePublication& publication,const Snapshot& cache,
            uint64_t canonicalRevision) const noexcept {
        return phase_==Phase::Prepared&&canonicalRevision==candidate_->revision&&
            Matches(publication,cache,before_)&&publication.CanAdvance(2);
    }
    bool CanRollback(const ReferencePublication& publication,const Snapshot& cache,
            uint64_t canonicalRevision) const noexcept {
        // A resource failure can roll canonical state back BEFORE the candidate
        // reference snapshot was published. That path still needs revision +2.
        return canonicalRevision==rollback_->revision&&publication.CanAdvance(1)&&
            ((phase_==Phase::Prepared&&Matches(publication,cache,before_))||
             (phase_==Phase::Applied&&Matches(publication,cache,candidate_)));
    }
    bool Commit(ReferencePublication& publication,Snapshot& cache,uint64_t canonicalRevision) noexcept {
        if(!CanCommit(publication,cache,canonicalRevision))return false;
        publication.Publish(candidate_);cache=candidate_;stamp_=publication.Stamp();phase_=Phase::Applied;return true;
    }
    bool Rollback(ReferencePublication& publication,Snapshot& cache,uint64_t canonicalRevision) noexcept {
        if(!CanRollback(publication,cache,canonicalRevision))return false;
        publication.Publish(rollback_);cache=rollback_;stamp_=publication.Stamp();phase_=Phase::RolledBack;return true;
    }
private:
    ReferenceExchange()=default;
    bool Matches(const ReferencePublication& publication,const Snapshot& cache,const Snapshot& expected) const noexcept {
        return &publication==publication_&&identity_==publication.identity_&& &cache==cache_&&
            publication.Stamp()==stamp_&&publication.Snapshot()==expected&&cache==expected;
    }
    enum class Phase {Prepared,Applied,RolledBack};
    ReferencePublication* publication_=nullptr;
    Snapshot* cache_=nullptr;
    std::shared_ptr<const uint8_t> identity_;
    PublicationStamp stamp_;
    Snapshot before_,candidate_,rollback_;
    Phase phase_=Phase::Prepared;
};
} // namespace fo3scene
