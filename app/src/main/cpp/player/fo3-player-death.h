#pragma once
#include <algorithm>
#include <cstdint>

namespace fo3death {
// fPlayerDeathReloadTime from Fallout3.exe, in seconds.
inline constexpr int64_t ReloadNanoseconds = 5'000'000'000;

struct Sequence {
  bool Begin() {
    if (started_) return false;
    started_ = true;
    return true;
  }
  void Suspend() { previous_ = 0; }
  bool Ready(int64_t predictedTime) {
    if (!started_ || predictedTime <= 0) return false;
    if (previous_ > 0 && predictedTime >= previous_ && elapsed_ < ReloadNanoseconds)
      elapsed_ += std::min(ReloadNanoseconds - elapsed_, predictedTime - previous_);
    previous_ = predictedTime;
    return elapsed_ >= ReloadNanoseconds;
  }

private:
  bool started_ = false;
  int64_t previous_ = 0;
  int64_t elapsed_ = 0;
};
} // namespace fo3death
