#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstring>
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_WARN 5
#define ANDROID_LOG_ERROR 6
inline int __android_log_print(int, const char*, const char* format, ...) {
    // Bounded world-level diagnostic output for original-data probes.
    if (std::strncmp(format,"INTERIOR COLLISION SOURCE:",26) &&
        std::strncmp(format,"INTERIOR COLLISION READY:",25)) return 0;
    va_list args;va_start(args,format);int n=std::vprintf(format,args);va_end(args);
    std::puts("");return n;
}
