#include "world/fo3-resident-pause-clock.h"
#include "ui/interaction/fo3-item-notifications.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>
namespace {
bool blocked=false;
int64_t rawSteady=10000000000LL;
bool Fo3ResidentFrameBlocked(){return blocked;}
std::chrono::steady_clock::time_point Fo3ResidentRawSteadyNow() noexcept {
    return std::chrono::steady_clock::time_point{std::chrono::nanoseconds{rawSteady}};
}
#include "world/fo3-resident-time-runtime.inc"
int64_t now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(Fo3GameplayNow().time_since_epoch()).count();}
}
int main(){
    using fo3scene::ResidentPauseClock;
    ResidentPauseClock clock;
    assert(clock.Sample(100,false)==100);
    assert(clock.Sample(150,false)==150);
    // Exact acquisition includes the elapsed interval before the pause.
    assert(clock.Sample(175,true)==175);
    assert(clock.Sample(900,true)==175);
    assert(clock.Sample(1200,false)==175); // No catch-up on first resumed sample.
    assert(clock.Sample(1210,false)==185);
    clock.Pause(); // Reservation can start and finish between frame samples.
    assert(clock.Paused());
    assert(clock.Sample(2000,false)==185);
    assert(!clock.Paused());
    assert(clock.Sample(2010,false)==195);
    assert(clock.Sample(100,false)==195); // Clock reset never rewinds deadlines.
    assert(clock.Sample(110,false)==205);
    assert(clock.Sample(0,false)==205);
    assert(clock.Sample(1000,false)==205); // Invalid sample requires a new baseline.
    assert(clock.Sample(1010,false)==215);
    ResidentPauseClock limit;
    const auto max=std::numeric_limits<int64_t>::max();
    assert(limit.Sample(max-2,false)==max-2);
    assert(limit.Sample(1,false)==max-2);
    assert(limit.Sample(9,false)==max);
    assert(limit.Sample(100,false)==max);

    // Actual render-thread bridge has independent XR and steady epochs.
    assert(now()==10000000000LL);
    assert(Fo3GameplayXrTime(1000000000000LL)==1000000000000LL);
    rawSteady+=1000000000LL;
    const auto animationStart=Fo3GameplayNow();
    const auto deadline=now()+2000000000LL;
    const auto xrBefore=Fo3GameplayXrTime(1001000000000LL);
    fo3notify::Queue queue;queue.Advance(double(xrBefore)*1e-9);
    queue.Enqueue("Frozen notice");
    const auto age=queue.Age();const auto revision=queue.Revision();
    blocked=true;PauseFo3ResidentGameplayTime();
    const auto frozen=now();
    // Draining, CPU, GPU, Ready, Settled, Finalizing and fatal recovery all
    // retain the same block. No timer state is reset or mutated to compensate.
    for(int phase=0;phase<7;++phase){
        rawSteady+=60000000000LL;
        assert(now()==frozen);
        assert(Fo3GameplayXrTime(1061000000000LL+phase*60000000000LL)==xrBefore);
        assert(Fo3GameplayNow()==animationStart);
        assert(queue.Age()==age&&queue.Revision()==revision);
    }
    blocked=false; // Complete release, cancellation, rejection or rollback.
    assert(now()==frozen);
    assert(Fo3GameplayXrTime(1500000000000LL)==xrBefore);
    rawSteady+=500000000LL;
    assert(now()==frozen+500000000LL&&now()<deadline);
    const auto resumed=Fo3GameplayXrTime(1500500000000LL);
    assert(resumed==xrBefore+500000000LL);
    queue.Advance(double(resumed)*1e-9);
    assert(queue.Size()==1&&queue.Age()>=age+.499&&queue.Age()<=age+.501);

    // No submitted frame during the complete reservation.
    blocked=true;PauseFo3ResidentGameplayTime();
    rawSteady+=3600000000000LL;
    blocked=false;
    assert(now()==frozen+500000000LL);
    assert(Fo3GameplayXrTime(5100500000000LL)==resumed);
    rawSteady+=1000000000LL;
    assert(now()==frozen+1500000000LL);
    std::cout<<"Resident gameplay clock bridge tests passed\n";
}
