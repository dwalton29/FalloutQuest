#include "fo3-scene-preparation.h"
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
    BeginFo3Loading(0x151e3u, 0u);
    Require(ShouldDelayFo3TransitionConsume(), "loading presentation gate was skipped");
    for (uint32_t frame = 0u; frame < FO3_LOADING_MIN_PRESENT_FRAMES - 1u; ++frame)
        MarkFo3LoadingFramePresented();
    Require(ShouldDelayFo3TransitionConsume(), "loading presentation ended early");
    MarkFo3LoadingFramePresented();
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
    std::cout << "scene preparation tests passed\n";
}
