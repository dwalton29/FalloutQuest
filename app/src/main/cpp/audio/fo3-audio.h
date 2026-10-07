#pragma once
#include "pipboy/fo3-radio-runtime.h"
#include <cstdint>
#include <jni.h>
#include <string>
#include "fo3-dialogue-face.h"
namespace fo3audio {
// Catalog outlives the audio worker (application session). Mutations are copied
// into bounded queue messages; worker never reads mutable Player state.
// Token-isolated actor speech. Completion includes asset/decode failures; no timers.
void Dialogue(const std::string& request,uint32_t token);
void DialogueStop();
void DialogueGain(float gain);
uint64_t DialogueCompletion();
void DialogueDone(uint32_t token,bool success);
DialogueFaceSample DialogueFace(uint32_t token);
void Radio(uint32_t transmitter, const fo3pipdata::Definitions &definitions,
           const fo3pipdata::SessionState &state);
void Note(uint32_t note, const std::vector<std::string> &paths);
void RadioState(const fo3pipdata::SessionState &state);
uint32_t PlayingNote();
void BroadcastDone(int channel, uint32_t generation);
void Start(JavaVM *vm, jobject activity);
void Shutdown();
void Context(uint32_t cell, bool focused);
void Pickup(uint32_t base);
void Open(uint32_t base);
void Close(uint32_t base);
void Scroll();
void SoundEvent(uint32_t soundForm);
void PreloadSound(uint32_t soundForm);
void PreloadSound(const std::string& editorId);
void NamedSound(const std::string &editorId);
} // namespace fo3audio
