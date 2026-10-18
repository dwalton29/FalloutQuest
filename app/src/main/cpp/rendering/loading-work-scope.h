#pragma once
#include "loading-work-budget.h"
#include <android/log.h>
#include <chrono>
namespace fqloading {
inline uint64_t Now(){return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
struct WorkScope {
    const char* label;uint64_t started=Now();
    explicit WorkScope(const char* name):label(name){}
    ~WorkScope(){const auto end=Now();budget.Charge(started,end);
        if(budget.loading && end-started>2000)
            __android_log_print(ANDROID_LOG_WARN,"FalloutQuest","LOADING LONG OP: name=%s cpuUs=%llu indivisibleDriverWorkMayExceedBudget=1",label,(unsigned long long)(end-started));}
};
inline bool Allow(){return budget.Allow(Now());}
} // namespace fqloading
