#include "fo3-scene-preparation.h"
#include "fo3-scene-publication.h"
#include "../../app/src/main/cpp/ui/loading/fo3-loading-state.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
void Require(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
template<class Predicate> void Wait(Predicate ready) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!ready()) {
        Require(std::chrono::steady_clock::now() < deadline, "worker timed out");
        std::this_thread::yield();
    }
}
struct Result { std::string text; int value = 0; };
} // namespace

int main() {
    for(int bits=0;bits<8;++bits) {
        Require(Fo3LoadingExteriorReady(bits&1,bits&2,bits&4)==(bits==7),
                "loading revealed an incomplete exterior");
    }

    BeginFo3Loading(0x151e3u, 0u);
    Require(!ShouldDelayFo3TransitionConsume(), "presentation delayed independent scene work");
    MarkFo3TransitionWorkStarted();
    for (int frame = 0; frame < 100; ++frame) MarkFo3LoadingFramePresented();
    Require(IsFo3LoadingVisible(), "loading vanished while CPU work was pending");
    const uint64_t before = GetFo3LoadingElapsedUs();
    NotifyFo3TransitionComplete();
    Require(IsFo3LoadingVisible(), "completion skipped the finished-scene frame");
    MarkFo3LoadingFramePresented();
    Require(!IsFo3LoadingVisible() && GetFo3LoadingElapsedUs() >= before,
            "finished loading presentation or its timing is invalid");

    fo3scene::Preparation<Result> job;
    std::atomic<bool> entered{false}, release{false};
    Require(job.Start([&](Result& result, const std::atomic<bool>& cancelled) {
        entered.store(true, std::memory_order_release);
        while (!release.load(std::memory_order_acquire)) {
            if (cancelled.load(std::memory_order_acquire)) return false;
            std::this_thread::yield();
        }
        result.text = "authored scene";
        result.value = 240;
        return true;
    }), "start failed");
    Wait([&] { return entered.load(std::memory_order_acquire); });
    Require(!job.Ready(), "partial result was published");
    Require(!job.Start([](Result&, const std::atomic<bool>&) { return true; }),
            "overlapping preparation was accepted");
    release.store(true, std::memory_order_release);
    Wait([&] { return job.Ready(); });
    Require(job.Successful() && job.Get().text == "authored scene" &&
            job.Get().value == 240, "result was not published completely");
    job.Reset();
    Require(!job.Ready(), "reset left a stale ready result");

    Require(job.Start([](Result&, const std::atomic<bool>&) { return false; }), "restart failed");
    Wait([&] { return job.Ready(); });
    Require(!job.Successful(), "failed worker marked successful");
    job.Reset();
    Require(job.Start([](Result&, const std::atomic<bool>&) -> bool {
        throw std::runtime_error("bad scene");
    }), "exception job did not start");
    Wait([&] { return job.Ready(); });
    Require(!job.Successful(), "exception escaped failure publication");
    job.Reset();

    entered.store(false);
    std::atomic<bool> exited{false};
    Require(job.Start([&](Result&, const std::atomic<bool>& cancel) {
        entered.store(true, std::memory_order_release);
        while (!cancel.load(std::memory_order_acquire)) std::this_thread::yield();
        exited.store(true, std::memory_order_release);
        return false;
    }), "cancellation job did not start");
    Wait([&] { return entered.load(std::memory_order_acquire); });
    job.Reset();
    Require(exited.load(std::memory_order_acquire) && !job.Ready(),
            "reset returned before cancellation joined the worker");

    entered.store(false); exited.store(false);
    {
        fo3scene::Preparation<Result> scoped;
        Require(scoped.Start([&](Result&, const std::atomic<bool>& cancel) {
            entered.store(true, std::memory_order_release);
            while (!cancel.load(std::memory_order_acquire)) std::this_thread::yield();
            exited.store(true, std::memory_order_release);
            return false;
        }), "scoped job did not start");
        Wait([&] { return entered.load(std::memory_order_acquire); });
    }
    Require(exited.load(std::memory_order_acquire), "destruction did not join the worker");
    // A slow decoder may not observe cancellation until its current source
    // operation ends. Polling cancellation must not block the XR thread.
    entered.store(false);release.store(false);
    Require(job.Start([&](Result&,const std::atomic<bool>& cancelled){
        entered.store(true,std::memory_order_release);
        while(!release.load(std::memory_order_acquire))std::this_thread::yield();
        return !cancelled.load(std::memory_order_acquire);
    }),"slow cancellation job did not start");
    Wait([&]{return entered.load(std::memory_order_acquire);});
    Require(!job.TryReset(),"in-flight decoder was synchronously joined");
    release.store(true,std::memory_order_release);Wait([&]{return job.Ready();});
    Require(!job.Successful(),"cancelled decoder published success");
    Require(job.TryReset()&&!job.Active(),"retired decoder could not reset");

    // Publication regression: slow workers, reused metadata and staged uploads
    // all retain the scene identity which produced their placement selection.
    fo3scene::ReferencePublication publication;
    Require(!publication.Accepts(publication.Stamp(), 0), "unpublished scene accepted work");
    auto source = std::make_shared<fo3world::ReferenceStateSnapshot>();
    source->revision = 7;
    source->states[42] = fo3world::ReferenceEnableState::Enabled;
    publication.Publish(source);
    source.reset();
    const auto oldStamp = publication.Stamp();
    const auto oldSnapshot = publication.Snapshot();
    fo3scene::Preparation<Result> staleJob;
    release.store(false);
    entered.store(false);
    Require(staleJob.Start([&, oldSnapshot](Result& out, const std::atomic<bool>&) {
        entered.store(true, std::memory_order_release);
        while (!release.load(std::memory_order_acquire)) std::this_thread::yield();
        out.value = oldSnapshot->Enabled(42, false) ? 1 : 0;
        return true;
    }), "publication worker failed to start");
    Wait([&] { return entered.load(std::memory_order_acquire); });
    Require(publication.Accepts(oldStamp, 7), "current scene rejected its own metadata");
    Require(!publication.Accepts(oldStamp, 8), "new live revision accepted stale GPU/collision work");
    Require(publication.Stamp() == oldStamp && publication.Snapshot() == oldSnapshot,
            "a live revision query relabelled old scene resources");
    // A failed script may roll back before publication. Its old scene remains
    // usable, whereas a successful same-CELL load must invalidate the old job.
    Require(publication.Accepts(oldStamp, 7), "rollback invalidated the unchanged scene");
    auto replacement = std::make_shared<fo3world::ReferenceStateSnapshot>();
    replacement->revision = 8;
    replacement->states[42] = fo3world::ReferenceEnableState::Disabled;
    publication.Publish(replacement);
    release.store(true, std::memory_order_release);
    Wait([&] { return staleJob.Ready(); });
    Require(staleJob.Successful() && staleJob.Get().value == 1,
            "worker lost its owned old projection");
    Require(!publication.Accepts(oldStamp, 8), "completed stale worker admitted after replacement");
    Require(publication.Accepts(publication.Stamp(), 8) &&
            !publication.Snapshot()->Enabled(42, true), "new projection not admitted");
    const auto sameCellStamp = publication.Stamp();
    publication.Publish(replacement); // same CELL, same revision, different scene load
    Require(!publication.Accepts(sameCellStamp, 8), "same-CELL reload reused a stale scene identity");
    Require(!publication.Accepts(oldStamp, 7), "restored revision resurrected an old scene job");
    staleJob.Reset();
    std::cout << "scene preparation tests passed\n";
}
