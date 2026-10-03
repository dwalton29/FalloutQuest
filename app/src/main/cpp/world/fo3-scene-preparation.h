#pragma once

#include <atomic>
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
            worker_ = std::thread([this, work = std::move(work)]() mutable {
                try { success_ = work(*result_, cancelled_); }
                catch (...) { success_ = false; }
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

    void Reset() {
        cancelled_.store(true, std::memory_order_release);
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
};

} // namespace fo3scene
