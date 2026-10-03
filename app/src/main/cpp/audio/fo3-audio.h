#pragma once
#include <cstdint>
#include <jni.h>
#include <string>
namespace fo3audio {
void Start(JavaVM *vm, jobject activity);
void Shutdown();
void Context(uint32_t cell, bool focused);
void Pickup(uint32_t base);
void Open(uint32_t base);
void Close(uint32_t base);
void Scroll();
void NamedSound(const std::string &editorId);
} // namespace fo3audio
