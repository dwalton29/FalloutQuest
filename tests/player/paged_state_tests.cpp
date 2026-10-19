#include "player/fo3-paged-state.h"
#include <cassert>
#include <type_traits>
struct Record {
    static inline unsigned copies=0;
    int value=0;
    Record()=default;
    Record(const Record& b):value(b.value){++copies;}
    Record& operator=(const Record&)=default;
};
int main(){
    using Map=fo3player::PagedStateMap<Record>;
    static_assert(std::is_nothrow_swappable_v<Map>);
    Map live;
    for(unsigned i=0;i<10000;++i)live[i].value=int(i);
    Record::copies=0;
    auto saved=live;
    assert(!Record::copies&&saved.size()==10000);
    for(const auto& entry:live)assert(entry.second.value==int(entry.first));
    assert(live.find(123)->second.value==123&&!Record::copies);
    assert(!live.Edit(10001)&&!Record::copies);
    live.Edit(123)->value=-1;
    assert(Record::copies>0&&Record::copies<500);
    assert(saved.at(123).value==123&&live.at(123).value==-1);
    auto candidate=live;
    candidate[124].value=-2;
    candidate.emplace(10001,Record{});
    assert(!live.count(10001)&&live.at(124).value==124);
    swap(live,candidate);
    assert(live.at(124).value==-2&&candidate.at(124).value==124);
    swap(live,candidate); // rollback keeps both maps intact
    assert(live.at(124).value==124&&!live.count(10001));
    candidate.clear();assert(candidate.empty()&&live.size()==10000);
    auto moved=std::move(saved);assert(saved.empty()&&saved.begin()==saved.end());
    assert(moved.at(123).value==123);
    live=std::move(moved);assert(moved.empty()&&live.size()==10000);
    unsigned count=0;for(auto it=live.begin();it!=live.end();it++)++count;
    assert(count==live.size());
}
