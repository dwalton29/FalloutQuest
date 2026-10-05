#pragma once
#include "fo3-pipboy-data.h"
namespace fo3pipdata {
enum class Completion : uint8_t { Active, Complete, Failed };
struct ObjectiveState {
  bool displayed = false;
  Completion status = Completion::Active;
};
struct QuestState {
  uint16_t stage = 0;
  Completion status = Completion::Active;
  std::unordered_set<uint16_t> stages;
  std::unordered_map<uint32_t, ObjectiveState> objectives;
};
struct Waypoint {
  uint32_t world = 0;
  Point point;
};
struct SessionState {
  std::unordered_map<uint32_t, uint8_t> perks;
  std::unordered_map<uint32_t, QuestState> quests;
  std::unordered_set<uint32_t> discovered;
  Waypoint waypoint;
  uint32_t selectedQuest = 0, tunedRadio = 0;
  // Runtime-observed counters. Original General label comes from GMST/exe.
  uint32_t aidUsed = 0;
  float radiation = 0;
};
void EncodeState(const SessionState &, std::vector<uint8_t> &);
bool DecodeState(SessionState &, const Definitions &, const uint8_t *, size_t,
                 std::string &);
} // namespace fo3pipdata
