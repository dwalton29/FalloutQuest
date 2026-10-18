#pragma once
// Lightweight, opt-in-by-build diagnostic output for scene loading on Quest.
// A separate Android tag and bounded on-device text file survive scene aborts.
// Never log from a per-vertex / per-texture hot path without a threshold.
#include <android/log.h>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <mutex>

namespace fqloadprofile {
inline uint64_t NowUs() noexcept {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

inline void Log(const char* format, ...) {
    char message[840];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    const unsigned long long timestamp = static_cast<unsigned long long>(NowUs());
    __android_log_print(ANDROID_LOG_INFO, "FQLoadProfile",
                        "tUs=%llu %s", timestamp, message);

    static std::mutex fileMutex;
    std::lock_guard<std::mutex> guard(fileMutex);
    static FILE* file = []() -> FILE* {
        constexpr const char* path =
            "/data/user/0/com.falloutquest.app/files/loading-profile.log";
        FILE* out = std::fopen(path, "a+");
        if (!out) return nullptr; // logcat remains available
        if (std::fseek(out, 0, SEEK_END) == 0 && std::ftell(out) > 2L*1024L*1024L) {
            std::fclose(out);
            out = std::fopen(path, "w"); // bounded history, last session preserved
        }
        return out;
    }();
    if (file) {
        std::fprintf(file, "tUs=%llu %s\n", timestamp, message);
        // Stage markers and infrequent progress samples only: survive crashes.
        std::fflush(file);
    }
}
} // namespace fqloadprofile
