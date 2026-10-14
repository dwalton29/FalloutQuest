#pragma once
#include <cstdint>
#include <unordered_map>

namespace fo3world {
enum class ReferenceEnableState : uint8_t { Disabled, Enabled, Unavailable };
// Value-only scene-worker input. It owns its data and never references Player,
// mutable save state, scene objects, GL resources or a thread-local callback.
struct ReferenceStateSnapshot {
  uint64_t revision=0;
  std::unordered_map<uint32_t,ReferenceEnableState> states;
  bool Enabled(uint32_t reference,bool authoredEnabled) const {
    const auto found=states.find(reference);
    return found==states.end()?authoredEnabled:found->second==ReferenceEnableState::Enabled;
  }
};
} // namespace fo3world
