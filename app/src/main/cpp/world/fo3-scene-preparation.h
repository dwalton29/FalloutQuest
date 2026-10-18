#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>

namespace fo3scene {

// One CPU preparation job. Publication is acquire/release; cancellation and
// destruction join the worker before its payload or referenced caches can go away.
template<class Result>
class Preparation {
public:
    ~Preparation() { Reset(); }
    Preparation() = default;
    Preparation(const Preparation&) = delete;
    Preparation& operator=(const Preparation&) = delete;

    template<class Work>
    bool Start(Work work) {
        if (result_) return false;
        cancelled_.store(false, std::memory_order_release);
        ready_.store(false, std::memory_order_release);
        success_ = false;
        try {
            result_ = std::make_unique<Result>();
            const auto queued=std::chrono::steady_clock::now();
            worker_ = std::thread([this, queued, work = std::move(work)]() mutable {
                const auto started=std::chrono::steady_clock::now();
                queueDelayUs_=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(started-queued).count());
                try { success_ = work(*result_, cancelled_); }
                catch (...) { success_ = false; }
                executionUs_=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count());
                ready_.store(true, std::memory_order_release);
            });
        } catch (...) {
            result_.reset();
            return false;
        }
        return true;
    }

    bool Ready() const { return ready_.load(std::memory_order_acquire); }
    bool Successful() const { return Ready() && success_; }
    Result& Get() { return *result_; } // render thread only, after Ready()

    uint64_t QueueDelayUs() const {return Ready()?queueDelayUs_:0;}
    uint64_t ExecutionUs() const {return Ready()?executionUs_:0;}
    bool Active() const { return bool(result_); } // render thread only
    void RequestCancel() { cancelled_.store(true, std::memory_order_release); }
    bool TryReset() {
        RequestCancel();
        if (Active() && !Ready()) return false;
        Reset(); return true;
    }
    void Reset() {
        RequestCancel();
        if (worker_.joinable()) worker_.join();
        result_.reset();
        ready_.store(false, std::memory_order_release);
    }

private:
    std::unique_ptr<Result> result_;
    std::thread worker_;
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> ready_{false};
    bool success_ = false;
    uint64_t queueDelayUs_=0,executionUs_=0;
};

} // namespace fo3scene
