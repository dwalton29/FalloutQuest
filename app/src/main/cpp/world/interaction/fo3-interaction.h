#pragma once
#include "world/fo3-cell-traversal.h"
#include <array>
#include <string>
#include <vector>

struct Fo3InteractionTarget {
  uint32_t reference = 0;
  bool pickup = false, allowed = false;
  bool container = false;
  std::array<float,3> anchor{};
  float distance = 0;
  Fo3DoorAimQ1700 door;
  std::array<char, 512> prompt{};
};
bool QueryFo3Interaction(float ox, float oy, float oz, float dx, float dy,
                         float dz, Fo3InteractionTarget &target);
// Returns true only for a queued door transition (caller resets VR origin).
bool ActivateFo3Interaction(float ox, float oy, float oz, float dx, float dy,
                            float dz);
struct Fo3LootRow {uint64_t stack=0;std::string name;int32_t count=0;bool allowed=false;};
struct Fo3LootPanel {
  uint32_t reference=0;
  std::array<float,3> anchor{};
  std::string title;
  std::vector<Fo3LootRow> rows;
  size_t selected=0;
};
const Fo3LootPanel& GetFo3LootPanel();
void UpdateFo3LootSelection(const Fo3InteractionTarget& target,float stickY,double time);
