#pragma once
#include <cstdint>
#include <jni.h>
namespace fo3audio {
void Start(JavaVM *vm, jobject activity);
void Shutdown();
void Context(uint32_t cell, bool focused);
void Pickup(uint32_t base);
void Open(uint32_t base);
void Close(uint32_t base);
void Scroll();
} // namespace fo3audio
