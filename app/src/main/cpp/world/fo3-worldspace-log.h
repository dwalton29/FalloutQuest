#pragma once

#ifdef __ANDROID__
#include <android/log.h>
#define Q75_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FalloutQuest", __VA_ARGS__)
#define Q75_LOGW(...) __android_log_print(ANDROID_LOG_WARN, "FalloutQuest", __VA_ARGS__)
#define Q75_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "FalloutQuest", __VA_ARGS__)
#else
#include <cstdio>
#define Q75_LOGI(...) std::fprintf(stderr, __VA_ARGS__)
#define Q75_LOGW(...) std::fprintf(stderr, __VA_ARGS__)
#define Q75_LOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif
