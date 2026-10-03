#pragma once
#include "world/fo3-cell-traversal.h"
#include <array>

struct Fo3InteractionTarget {
  uint32_t reference = 0;
  bool pickup = false, allowed = false;
  float distance = 0;
  Fo3DoorAimQ1700 door;
  std::array<char, 512> prompt{};
};
bool QueryFo3Interaction(float ox, float oy, float oz, float dx, float dy,
                         float dz, Fo3InteractionTarget &target);
// Returns true only for a queued door transition (caller resets VR origin).
bool ActivateFo3Interaction(float ox, float oy, float oz, float dx, float dy,
                            float dz);
