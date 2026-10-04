#pragma once
#include <android/log.h>
#include <sys/system_properties.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string_view>
namespace fo3fontdebug {
inline bool Enabled() {
#ifndef NDEBUG
    char value[PROP_VALUE_MAX]{};__system_property_get("debug.falloutquest.font",value);
    return std::atoi(value)==1;
#else
    return false;
#endif
}
// Only called when source/geometry changes. Bounded for a whole process run.
inline bool LogBytes(const char* stage,std::string_view text) {
    if(!Enabled())return false;
    static std::atomic<unsigned> count{0};
    if(count.fetch_add(1,std::memory_order_relaxed)>=96)return false;
    std::array<char,512*3+1> bytes{};const size_t n=std::min(text.size(),size_t(512));
    for(size_t i=0;i<n;++i)std::snprintf(bytes.data()+i*3,4,"%02X ",static_cast<unsigned char>(text[i]));
    __android_log_print(ANDROID_LOG_INFO,"FalloutQuest","HUD TEXT BYTES: stage=%s text=\"%.*s\" bytes=%s",
        stage,int(n),text.data(),bytes.data());
    return true;
}
}
