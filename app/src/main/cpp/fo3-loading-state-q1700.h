#pragma once

#include <atomic>
#include <cstdint>

// Q16.0 loading-screen state shared by the OpenXR host and the CELL loader.
// A user door transition spends one submitted frame in PRESENT before the
// render-thread CELL swap is allowed to consume the queued request. That lets
// the compositor hold a real Fallout loading image while the synchronous NIF /
// collision / LAND build runs.
enum Fo3LoadingPhaseQ1700 : int {
    FO3_LOADING_IDLE_Q1700 = 0,
    FO3_LOADING_PRESENT_Q1700 = 1,
    FO3_LOADING_READY_Q1700 = 2,
    FO3_LOADING_POST_Q1700 = 3,
};

inline std::atomic<int> gFo3LoadingPhaseQ1700{FO3_LOADING_IDLE_Q1700};
inline std::atomic<uint32_t> gFo3LoadingCellQ1700{0u};
inline std::atomic<uint32_t> gFo3LoadingWorldspaceQ1700{0u};
inline std::atomic<uint64_t> gFo3LoadingGenerationQ1700{0u};

inline void BeginFo3LoadingQ1700(uint32_t cellFormId, uint32_t worldspaceFormId) {
    gFo3LoadingCellQ1700.store(cellFormId, std::memory_order_release);
    gFo3LoadingWorldspaceQ1700.store(worldspaceFormId, std::memory_order_release);
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

inline void NotifyFo3TransitionCompleteQ1700() {
    int expected = FO3_LOADING_READY_Q1700;
    (void)gFo3LoadingPhaseQ1700.compare_exchange_strong(
        expected, FO3_LOADING_POST_Q1700,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

inline void CancelFo3LoadingQ1700() {
    gFo3LoadingPhaseQ1700.store(FO3_LOADING_IDLE_Q1700, std::memory_order_release);
}

// Called only after xrEndFrame succeeds. PRESENT -> READY authorizes the CELL
// swap on the next frame. POST -> IDLE removes the loading image only after one
// submitted frame containing the newly loaded destination underneath it. If a
// queued load failed after being consumed, READY -> IDLE prevents a stuck screen.
inline void MarkFo3LoadingFramePresentedQ1700() {
    const int phase = gFo3LoadingPhaseQ1700.load(std::memory_order_acquire);
    if (phase == FO3_LOADING_PRESENT_Q1700) {
        gFo3LoadingPhaseQ1700.store(FO3_LOADING_READY_Q1700, std::memory_order_release);
    } else if (phase == FO3_LOADING_POST_Q1700 || phase == FO3_LOADING_READY_Q1700) {
        gFo3LoadingPhaseQ1700.store(FO3_LOADING_IDLE_Q1700, std::memory_order_release);
    }
}
