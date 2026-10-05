#pragma once
#include "fo3-pipboy-session.h"
#include <random>
namespace fo3pipdata {
struct Location {
  uint32_t cell = 0, world = 0;
  float x = 0, y = 0, facing = 0;
};
bool Enabled(const Definitions &, const Placement &);
bool InRange(const Definitions &, const Transmitter &, const Location &);
std::vector<uint32_t> AvailableStations(const Definitions &, const Location &);
struct Broadcast {
  uint32_t station = 0, quest = 0, topic = 0;
  std::vector<uint32_t> next;
  std::unordered_map<std::string, float> variables;
  std::mt19937 random{std::random_device{}()};
  bool Tune(const Definitions &, uint32_t transmitter);
  std::vector<std::string> Advance(const Definitions &, const SessionState &,
                                   std::string &error);
  bool Conditions(const Definitions &, const SessionState &,
                  const std::vector<Condition> &) const;
  bool Assign(const Definitions &, const std::vector<std::string> &,
              bool apply);
};
std::vector<std::string> BroadcastAudio(const Definitions &, const Info &,
                                        uint32_t voice);
} // namespace fo3pipdata
