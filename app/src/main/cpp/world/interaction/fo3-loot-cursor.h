#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fo3loot {
struct Cursor {
  uint32_t reference = 0;
  size_t selected = 0;
  int direction = 0;
  bool armed = false;
  double repeatAt = 0;
  void Target(uint32_t ref, size_t count) {
    if (ref != reference) {
      reference = ref;
      selected = 0;
      direction = 0;
      armed = false;
      repeatAt = 0;
    }
    selected = count ? std::min(selected, count - 1) : 0;
  }
  void Scroll(float y, double time, size_t count) {
    if (!std::isfinite(y) || !std::isfinite(time))
      return;
    if (std::fabs(y) < .3f) {
      armed = true;
      direction = 0;
      return;
    }
    if (!armed || std::fabs(y) < .65f || !count)
      return;
    const int next = y > 0 ? -1 : 1;
    const bool edge = next != direction;
    if (edge || time >= repeatAt) {
      if (next < 0 && selected > 0)
        --selected;
      else if (next > 0 && selected + 1 < count)
        ++selected;
      direction = next;
      repeatAt = time + (edge ? .35 : .12);
    }
  }
};
} // namespace fo3loot
