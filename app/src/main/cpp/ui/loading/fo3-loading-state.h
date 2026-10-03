#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

// loading-screen state shared by the OpenXR host and the CELL loader.
// extends the original one-frame gate into a real multi-frame loading
// lifetime so the headset can continue submitting/animating frames while the
// destination scene is prepared in slices.
enum Fo3LoadingPhase : int {
    FO3_LOADING_IDLE = 0,
    FO3_LOADING_PRESENT = 1,
    FO3_LOADING_READY = 2,
    FO3_LOADING_WORK = 3,
    FO3_LOADING_POST = 4,
};

inline std::atomic<int> gFo3LoadingPhase{FO3_LOADING_IDLE};
inline std::atomic<uint32_t> gFo3LoadingCell{0u};
inline std::atomic<uint32_t> gFo3LoadingWorldspace{0u};
inline std::atomic<uint64_t> gFo3LoadingGeneration{0u};
inline std::atomic<uint32_t> gFo3LoadingPresentedFrames{0u};
inline std::atomic<uint64_t> gFo3LoadingStartedUs{0u};

inline uint64_t Fo3LoadingClockUs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

inline uint64_t GetFo3LoadingElapsedUs() {
    const uint64_t started = gFo3LoadingStartedUs.load(std::memory_order_acquire);
    return started == 0u ? 0u : Fo3LoadingClockUs() - started;
}

// At Quest refresh rates this is roughly 0.33-0.42 seconds. The visual
// fade is 0.35s, so the loader can no longer flash for a single submitted frame
// and disappear into a blocking scene build before the presentation is visible.
constexpr uint32_t FO3_LOADING_MIN_PRESENT_FRAMES = 30u;

inline void BeginFo3Loading(uint32_t cellFormId, uint32_t worldspaceFormId) {
    gFo3LoadingStartedUs.store(Fo3LoadingClockUs(), std::memory_order_release);
    gFo3LoadingCell.store(cellFormId, std::memory_order_release);
    gFo3LoadingWorldspace.store(worldspaceFormId, std::memory_order_release);
    gFo3LoadingPresentedFrames.store(0u, std::memory_order_release);
    gFo3LoadingGeneration.fetch_add(1u, std::memory_order_acq_rel);
    gFo3LoadingPhase.store(FO3_LOADING_PRESENT, std::memory_order_release);
}

inline bool IsFo3LoadingVisible() {
    return gFo3LoadingPhase.load(std::memory_order_acquire) != FO3_LOADING_IDLE;
}

inline bool ShouldDelayFo3TransitionConsume() {
    return gFo3LoadingPhase.load(std::memory_order_acquire) == FO3_LOADING_PRESENT;
}

inline uint32_t GetFo3LoadingCell() {
    return gFo3LoadingCell.load(std::memory_order_acquire);
}

inline uint32_t GetFo3LoadingWorldspace() {
    return gFo3LoadingWorldspace.load(std::memory_order_acquire);
}

inline uint64_t GetFo3LoadingGeneration() {
    return gFo3LoadingGeneration.load(std::memory_order_acquire);
}

inline uint32_t GetFo3LoadingPresentedFrames() {
    return gFo3LoadingPresentedFrames.load(std::memory_order_acquire);
}

// Called immediately after the queued transition is consumed by the phased
// scene builder. WORK deliberately remains visible across any number of
// xrEndFrame calls; only completion can advance it to POST.
inline void MarkFo3TransitionWorkStarted() {
    int expected = FO3_LOADING_READY;
    (void)gFo3LoadingPhase.compare_exchange_strong(
        expected, FO3_LOADING_WORK,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

inline void NotifyFo3TransitionComplete() {
    int expected = FO3_LOADING_WORK;
    if (gFo3LoadingPhase.compare_exchange_strong(
            expected, FO3_LOADING_POST,
            std::memory_order_acq_rel, std::memory_order_acquire)) {
        return;
    }

    // Compatibility with an immediate/synchronous transition path.
    expected = FO3_LOADING_READY;
    (void)gFo3LoadingPhase.compare_exchange_strong(
        expected, FO3_LOADING_POST,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

inline void CancelFo3Loading() {
    gFo3LoadingPhase.store(FO3_LOADING_IDLE, std::memory_order_release);
    gFo3LoadingPresentedFrames.store(0u, std::memory_order_release);
}

// Called only after xrEndFrame succeeds.
//
// PRESENT stays alive for a short authored-looking dwell so the screen cannot
// be a one-frame flash. READY remains visible until the render-thread loader
// consumes the request. WORK remains visible for the entire phased build.
// POST -> IDLE happens only after a submitted frame containing the finished
// destination underneath the loading presentation.
inline void MarkFo3LoadingFramePresented() {
    const int phase = gFo3LoadingPhase.load(std::memory_order_acquire);
    if (phase == FO3_LOADING_PRESENT) {
        const uint32_t presented =
            gFo3LoadingPresentedFrames.fetch_add(1u, std::memory_order_acq_rel) + 1u;
        if (presented >= FO3_LOADING_MIN_PRESENT_FRAMES) {
            int expected = FO3_LOADING_PRESENT;
            (void)gFo3LoadingPhase.compare_exchange_strong(
                expected, FO3_LOADING_READY,
                std::memory_order_acq_rel, std::memory_order_acquire);
        }
    } else if (phase == FO3_LOADING_POST) {
        gFo3LoadingPhase.store(FO3_LOADING_IDLE, std::memory_order_release);
    }
}

// Elapsed time diagnoses a stalled load; it must never expose incomplete cells.
inline bool Fo3LoadingExteriorReady(bool detail, bool nearLod, bool horizon) {
    return detail && nearLod && horizon;
}
