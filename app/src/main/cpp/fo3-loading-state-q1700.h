#pragma once

#include <atomic>
#include <cstdint>

// Q16.0 loading-screen state shared by the OpenXR host and the CELL loader.
// Q16.10 extends the original one-frame gate into a real multi-frame loading
// lifetime so the headset can continue submitting/animating frames while the
// destination scene is prepared in slices.
enum Fo3LoadingPhaseQ1700 : int {
    FO3_LOADING_IDLE_Q1700 = 0,
    FO3_LOADING_PRESENT_Q1700 = 1,
    FO3_LOADING_READY_Q1700 = 2,
    FO3_LOADING_WORK_Q1700 = 3,
    FO3_LOADING_POST_Q1700 = 4,
};

inline std::atomic<int> gFo3LoadingPhaseQ1700{FO3_LOADING_IDLE_Q1700};
inline std::atomic<uint32_t> gFo3LoadingCellQ1700{0u};
inline std::atomic<uint32_t> gFo3LoadingWorldspaceQ1700{0u};
inline std::atomic<uint64_t> gFo3LoadingGenerationQ1700{0u};
inline std::atomic<uint32_t> gFo3LoadingPresentedFramesQ1700{0u};

// At Quest refresh rates this is roughly 0.33-0.42 seconds. The Q16.8 visual
// fade is 0.35s, so the loader can no longer flash for a single submitted frame
// and disappear into a blocking scene build before the presentation is visible.
constexpr uint32_t FO3_LOADING_MIN_PRESENT_FRAMES_Q1700 = 30u;

inline void BeginFo3LoadingQ1700(uint32_t cellFormId, uint32_t worldspaceFormId) {
    gFo3LoadingCellQ1700.store(cellFormId, std::memory_order_release);
    gFo3LoadingWorldspaceQ1700.store(worldspaceFormId, std::memory_order_release);
    gFo3LoadingPresentedFramesQ1700.store(0u, std::memory_order_release);
    gFo3LoadingGenerationQ1700.fetch_add(1u, std::memory_order_acq_rel);
    gFo3LoadingPhaseQ1700.store(FO3_LOADING_PRESENT_Q1700, std::memory_order_release);
}

inline bool IsFo3LoadingVisibleQ1700() {
    return gFo3LoadingPhaseQ1700.load(std::memory_order_acquire) != FO3_LOADING_IDLE_Q1700;
}

inline bool ShouldDelayFo3TransitionConsumeQ1700() {
    return gFo3LoadingPhaseQ1700.load(std::memory_order_acquire) == FO3_LOADING_PRESENT_Q1700;
}

inline uint32_t GetFo3LoadingCellQ1700() {
    return gFo3LoadingCellQ1700.load(std::memory_order_acquire);
}

inline uint32_t GetFo3LoadingWorldspaceQ1700() {
    return gFo3LoadingWorldspaceQ1700.load(std::memory_order_acquire);
}

inline uint64_t GetFo3LoadingGenerationQ1700() {
    return gFo3LoadingGenerationQ1700.load(std::memory_order_acquire);
}

inline uint32_t GetFo3LoadingPresentedFramesQ1700() {
    return gFo3LoadingPresentedFramesQ1700.load(std::memory_order_acquire);
}

// Called immediately after the queued transition is consumed by the phased
// scene builder. WORK deliberately remains visible across any number of
// xrEndFrame calls; only completion can advance it to POST.
inline void MarkFo3TransitionWorkStartedQ1700() {
    int expected = FO3_LOADING_READY_Q1700;
    (void)gFo3LoadingPhaseQ1700.compare_exchange_strong(
        expected, FO3_LOADING_WORK_Q1700,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

inline void NotifyFo3TransitionCompleteQ1700() {
    int expected = FO3_LOADING_WORK_Q1700;
    if (gFo3LoadingPhaseQ1700.compare_exchange_strong(
            expected, FO3_LOADING_POST_Q1700,
            std::memory_order_acq_rel, std::memory_order_acquire)) {
        return;
    }

    // Compatibility with an immediate/synchronous transition path.
    expected = FO3_LOADING_READY_Q1700;
    (void)gFo3LoadingPhaseQ1700.compare_exchange_strong(
        expected, FO3_LOADING_POST_Q1700,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

inline void CancelFo3LoadingQ1700() {
    gFo3LoadingPhaseQ1700.store(FO3_LOADING_IDLE_Q1700, std::memory_order_release);
    gFo3LoadingPresentedFramesQ1700.store(0u, std::memory_order_release);
}

// Called only after xrEndFrame succeeds.
//
// PRESENT stays alive for a short authored-looking dwell so the screen cannot
// be a one-frame flash. READY remains visible until the render-thread loader
// consumes the request. WORK remains visible for the entire phased build.
// POST -> IDLE happens only after a submitted frame containing the finished
// destination underneath the loading presentation.
inline void MarkFo3LoadingFramePresentedQ1700() {
    const int phase = gFo3LoadingPhaseQ1700.load(std::memory_order_acquire);
    if (phase == FO3_LOADING_PRESENT_Q1700) {
        const uint32_t presented =
            gFo3LoadingPresentedFramesQ1700.fetch_add(1u, std::memory_order_acq_rel) + 1u;
        if (presented >= FO3_LOADING_MIN_PRESENT_FRAMES_Q1700) {
            int expected = FO3_LOADING_PRESENT_Q1700;
            (void)gFo3LoadingPhaseQ1700.compare_exchange_strong(
                expected, FO3_LOADING_READY_Q1700,
                std::memory_order_acq_rel, std::memory_order_acquire);
        }
    } else if (phase == FO3_LOADING_POST_Q1700) {
        gFo3LoadingPhaseQ1700.store(FO3_LOADING_IDLE_Q1700, std::memory_order_release);
    }
}
