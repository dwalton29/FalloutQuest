#pragma once
#include "fo3-radio-runtime.h"
#include <memory>
namespace fo3pipdata {
struct LocalMap {
  uint32_t cell = 0;
  uint64_t generation = 0;
  float minX = 0, minY = 0, maxX = 1, maxY = 1;
  std::vector<uint8_t> rgba;
  Point Project(float x, float y) const {
    return {(x - minX) / (maxX - minX), (maxY - y) / (maxY - minY)};
  }
};
struct MapContext {
  Location location, exterior;
  Point localOrigin;
  float sceneForward = 0;
  uint32_t mapWorld = 0;
  Point player, centre{.5f, .5f};
  float zoom = 1;
  std::shared_ptr<const LocalMap> local;
};
inline uint32_t MapWorld(const Definitions &d, uint32_t world) {
  for (int depth = 0; depth < 8 && world; ++depth) {
    auto w = d.worlds.find(world);
    if (w == d.worlds.end())
      return 0;
    if (w->second.parent && (w->second.parentFlags & 4))
      world = w->second.parent;
    else
      return w->second.valid ? world : 0;
  }
  return 0;
}
// Convert child-world coordinates through authored ONAM cell-unit transforms.
inline Point WorldPoint(const Definitions &d, uint32_t world, float x,
                        float y) {
  for (int depth = 0; depth < 8; ++depth) {
    auto w = d.worlds.find(world);
    if (w == d.worlds.end() || !w->second.parent ||
        !(w->second.parentFlags & 4))
      break;
    x = (x / 4096.f * w->second.scale + w->second.offsetX) * 4096.f;
    y = (y / 4096.f * w->second.scale + w->second.offsetY) * 4096.f;
    world = w->second.parent;
  }
  return {x, y};
}
inline Point MapProject(const Definitions &d, uint32_t world, float x,
                        float y) {
  const auto point = WorldPoint(d, world, x, y);
  const auto root = MapWorld(d, world);
  return root ? d.worlds.at(root).Project(point.x, point.y) : Point{};
}
inline std::vector<uint32_t> QuestTargets(const Definitions &d,
                                          const SessionState &s) {
  std::vector<uint32_t> out;
  auto q = s.quests.find(s.selectedQuest);
  if (q == s.quests.end() || q->second.status != Completion::Active)
    return out;
  auto def = d.quests.find(q->first);
  if (def == d.quests.end())
    return out;
  for (auto &o : q->second.objectives) {
    auto f = def->second.objectives.find(o.first);
    if (o.second.displayed && o.second.status == Completion::Active &&
        f != def->second.objectives.end() && !f->second.conditionalTargets)
      for (auto id : f->second.targets)
        if (d.targets.count(id))
          out.push_back(id);
  }
  return out;
}
} // namespace fo3pipdata
