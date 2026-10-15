#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace fo3scene {

// Render-thread structural ownership epoch, distinct from the saved reference
// state and collision epochs. Mark every live visual-layout replacement/retire.
class VisualPublication {
public:
    explicit VisualPublication(uint64_t initial = 1) : revision_(initial) {}
    VisualPublication(const VisualPublication&) = delete;
    VisualPublication& operator=(const VisualPublication&) = delete;
    uint64_t Revision() const noexcept { return exhausted_ ? 0 : revision_; }
    bool CanAdvance(uint64_t count) const noexcept {
        return Revision() != 0 && revision_ <= std::numeric_limits<uint64_t>::max() - count;
    }
    void Changed() noexcept {
        if (!CanAdvance(1)) exhausted_ = true;
        else ++revision_;
    }
private:
    template<class Object> friend class VisualExchange;
    template<class... Values> friend class ResourceExchange;
    std::shared_ptr<const uint8_t> identity_ = std::make_shared<const uint8_t>(0);
    uint64_t revision_;
    bool exhausted_ = false;
};

// Object contains non-RAII resource handles (as GpuObject does). Never copy a
// live Object or duplicate its ownership/refcount for a rollback snapshot.
// Default-constructed Objects must own no resources.
// Render-thread only. The outer coordinator owns scene/input/physics/actor
// guards and excludes simulation/rendering during publication and rollback.
template<class Object>
class VisualExchange {
public:
    using Objects = std::vector<Object>;
    using Dispose = void (*)(Objects&) noexcept;
    VisualExchange(const VisualExchange&) = delete;
    VisualExchange& operator=(const VisualExchange&) = delete;
    ~VisualExchange() { dispose_(payload_); }

    // retain lists strictly increasing original live slots. Incoming owns its
    // resource references already; on success ownership transfers to this plan.
    // Validation/allocation failure leaves live and incoming entirely untouched.
    static std::unique_ptr<VisualExchange> Prepare(
            const Objects& live, const VisualPublication& publication,
            const std::vector<size_t>& retain, Objects& incoming, Dispose dispose) {
        static_assert(std::is_nothrow_swappable_v<Object>, "visual exchange must not allocate");
        if (!dispose || &live == &incoming || !publication.CanAdvance(2) ||
            retain.size() > live.size() || incoming.size() > live.max_size() - retain.size()) return {};
        for (size_t i = 0; i < retain.size(); ++i)
            if (retain[i] >= live.size() || (i && retain[i] <= retain[i-1])) return {};
        auto plan = std::unique_ptr<VisualExchange>(new VisualExchange(dispose));
        plan->owner_ = &live;
        plan->identity_ = publication.identity_;
        plan->publication_ = &publication;
        plan->expectedRevision_ = publication.Revision();
        plan->expectedStorage_ = live.data();
        plan->expectedSize_ = live.size();
        plan->retain_ = retain;
        plan->payload_.resize(retain.size() + incoming.size());
        // No throwing work after resource transfer begins. Retained slots remain
        // default/empty until commit, leaving the entire live world drawable.
        using std::swap;
        for (size_t i = 0; i < incoming.size(); ++i)
            swap(plan->payload_[retain.size()+i], incoming[i]);
        incoming.clear();
        return plan;
    }

    bool Applied() const noexcept { return phase_ == Phase::Applied; }
    bool CanCommit(const Objects& live, const VisualPublication& publication) const noexcept {
        return phase_ == Phase::Prepared && Matches(live, publication) && publication.CanAdvance(2);
    }
    bool CanRollback(const Objects& live, const VisualPublication& publication) const noexcept {
        return phase_ == Phase::Applied && Matches(live, publication) && publication.CanAdvance(1);
    }
    bool CanFinalize(const Objects& live,const VisualPublication& publication,bool committed) const noexcept {
        return (committed?phase_==Phase::Applied:(phase_==Phase::Prepared||phase_==Phase::RolledBack))&&Matches(live,publication);
    }
    bool Commit(Objects& live, VisualPublication& publication) noexcept {
        if (!CanCommit(live, publication)) return false;
        using std::swap;
        for (size_t i = 0; i < retain_.size(); ++i) swap(live[retain_[i]], payload_[i]);
        live.swap(payload_);
        publication.Changed();
        Capture(live, publication);
        phase_ = Phase::Applied;
        return true;
    }
    bool Rollback(Objects& live, VisualPublication& publication) noexcept {
        if (!CanRollback(live, publication)) return false;
        live.swap(payload_);
        using std::swap;
        for (size_t i = 0; i < retain_.size(); ++i) swap(live[retain_[i]], payload_[i]);
        publication.Changed();
        Capture(live, publication);
        phase_ = Phase::RolledBack;
        return true;
    }

private:
    explicit VisualExchange(Dispose dispose) : dispose_(dispose) {}
    enum class Phase { Prepared, Applied, RolledBack };
    bool Matches(const Objects& live, const VisualPublication& publication) const noexcept {
        return owner_ == &live && publication_ == &publication && identity_ == publication.identity_ &&
            expectedRevision_ != 0 && expectedRevision_ == publication.Revision() &&
            expectedStorage_ == live.data() && expectedSize_ == live.size();
    }
    void Capture(const Objects& live, const VisualPublication& publication) noexcept {
        expectedStorage_ = live.data(); expectedSize_ = live.size();
        expectedRevision_ = publication.Revision();
    }
    Dispose dispose_;
    Phase phase_ = Phase::Prepared;
    const Objects* owner_ = nullptr;
    const VisualPublication* publication_ = nullptr;
    std::shared_ptr<const uint8_t> identity_;
    uint64_t expectedRevision_ = 0;
    const Object* expectedStorage_ = nullptr;
    size_t expectedSize_ = 0;
    std::vector<size_t> retain_;
    Objects payload_;
};

} // namespace fo3scene
