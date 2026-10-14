#pragma once
#include "fo3-visual-exchange.h"
#include <tuple>
#include <array>

namespace fo3scene {
// RAII-only values, with one publication identity for their coupled resource
// bundle. Preparation allocates; commit/rollback only swap already-owned values.
template<class... Values>
class ResourceExchange {
public:
    using Payload=std::tuple<Values...>;
    using Live=std::tuple<Values&...>;
    ResourceExchange(const ResourceExchange&)=delete;
    ResourceExchange& operator=(const ResourceExchange&)=delete;
    static std::unique_ptr<ResourceExchange> Prepare(VisualPublication& publication,
            Live live,Payload& incoming) {
        static_assert(std::is_nothrow_swappable_v<Payload>);
        if(!publication.CanAdvance(2)||Aliased(live,incoming,std::index_sequence_for<Values...>{}))return {};
        auto plan=std::unique_ptr<ResourceExchange>(new ResourceExchange);
        plan->publication_=&publication;plan->identity_=publication.identity_;
        plan->revision_=publication.Revision();
        plan->owners_=std::apply([](auto&... value){return std::make_tuple(&value...);},live);
        using std::swap;swap(plan->payload_,incoming);return plan;
    }
    bool Applied()const noexcept{return phase_==Phase::Applied;}
    bool CanCommit(const VisualPublication& publication,Live live)const noexcept {
        return phase_==Phase::Prepared&&Matches(publication,live)&&publication.CanAdvance(2);
    }
    bool CanRollback(const VisualPublication& publication,Live live)const noexcept {
        return phase_==Phase::Applied&&Matches(publication,live)&&publication.CanAdvance(1);
    }
    bool Commit(VisualPublication& publication,Live live)noexcept {
        if(!CanCommit(publication,live))return false;
        Exchange(live,std::index_sequence_for<Values...>{});
        publication.Changed();revision_=publication.Revision();phase_=Phase::Applied;return true;
    }
    bool Rollback(VisualPublication& publication,Live live)noexcept {
        if(!CanRollback(publication,live))return false;
        Exchange(live,std::index_sequence_for<Values...>{});
        publication.Changed();revision_=publication.Revision();phase_=Phase::RolledBack;return true;
    }
private:
    ResourceExchange()=default;
    template<size_t... I>static bool Aliased(Live live,Payload& incoming,std::index_sequence<I...>)noexcept {
        const std::array<const void*,sizeof...(Values)> owners{{&std::get<I>(live)...}};
        const std::array<const void*,sizeof...(Values)> staged{{&std::get<I>(incoming)...}};
        for(size_t i=0;i<owners.size();++i) {
            for(size_t j=0;j<i;++j)if(owners[i]==owners[j])return true;
            for(const void* value:staged)if(owners[i]==value)return true;
        }
        return false;
    }
    enum class Phase{Prepared,Applied,RolledBack};
    bool Matches(const VisualPublication& publication,Live live)const noexcept {
        return publication_==&publication&&identity_==publication.identity_&&revision_!=0&&
            revision_==publication.Revision()&&
            owners_==std::apply([](auto&... value){return std::make_tuple(&value...);},live);
    }
    template<size_t... I>void Exchange(Live live,std::index_sequence<I...>)noexcept {
        using std::swap;(swap(std::get<I>(live),std::get<I>(payload_)),...);
    }
    Payload payload_;
    std::tuple<Values*...> owners_{};
    VisualPublication* publication_=nullptr;
    std::shared_ptr<const uint8_t> identity_;
    uint64_t revision_=0;
    Phase phase_=Phase::Prepared;
};
} // namespace fo3scene
