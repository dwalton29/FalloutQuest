#pragma once
// Physical device policy and canonical player view. No XR, GL, or asset I/O.
#include "pipboy/fo3-map-runtime.h"
#include "player/fo3-player-state.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace fo3pip {
struct V {
  float x = 0, y = 0, z = 0;
};
inline V Sub(V a, V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline float Dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float Length(V a) { return std::sqrt(Dot(a, a)); }
inline V Unit(V a) {
  float n = Length(a);
  return n > 1e-6f ? V{a.x / n, a.y / n, a.z / n} : V{};
}
struct View {
  bool valid = false, raised = false;
  float distance = 0, facing = -1, cone = -1;
  bool keepRaised = false;
};
inline View Measure(bool valid, V screen, V normal, V head, V forward,
                    V screenBody, V shoulderBody, V /*handBody*/, bool solved) {
  View v;
  v.distance = Length(Sub(head, screen));
  v.facing = Dot(Unit(normal), Unit(Sub(head, screen)));
  v.cone = Dot(Unit(forward), Unit(Sub(screen, head)));
  // The physical screen alone establishes raising; hand pivot height is
  // redundant and can veto a correctly presented display during wrist flex.
  // Body-root axes: +Y up, -Z forward. Shoulder/solved hand determine arm
  // placement; head rotation is used only by the view cone above.
  // Height establishes the intentional wrist-raise gesture. Do not gate on
  // torso-root Z: the head-facing/view-cone tests already prove the physical
  // screen is in front of the user, while inferred torso yaw can legitimately
  // lag a real upper-body turn.
  v.raised = screenBody.y > shoulderBody.y - .25f;
  v.keepRaised = screenBody.y > shoulderBody.y - .35f;
  v.valid = valid && solved && std::isfinite(v.distance) &&
            std::isfinite(v.facing) && std::isfinite(v.cone);
  return v;
}
inline bool Enter(const View &v) {
  // Physical-wrist UI should tolerate the natural few degrees of controller
  // and elbow variation produced while looking down at the display.
  return v.valid && v.raised && v.distance >= .16f && v.distance <= .72f &&
         v.facing >= .5f && v.cone >= .5735764f;
}
inline bool Stay(const View &v) {
  return v.valid && (v.raised || v.keepRaised) && v.distance >= .12f &&
         v.distance <= .85f && v.facing >= .3420201f && v.cone >= .4226183f;
}
enum class Phase { Dormant, Candidate, Active };
struct Activation {
  Phase phase = Phase::Dormant;
  double began = 0;
  bool Focus() const { return phase == Phase::Active; }
  void Reset() {
    phase = Phase::Dormant;
    began = 0;
  }
  bool Step(const View &v, double now) {
    bool old = Focus();
    if (!v.valid || !std::isfinite(now)) {
      Reset();
      return old;
    }
    if (phase == Phase::Active) {
      if (!Stay(v))
        Reset();
    } else if (!Enter(v))
      Reset();
    else if (phase == Phase::Dormant) {
      phase = Phase::Candidate;
      began = now;
    } else if (now >= began + .15)
      phase = Phase::Active;
    return old != Focus();
  }
};
enum class Action {
  NextTab,
  PreviousTab,
  PreviousPage,
  NextPage,
  Up,
  Down,
  Accept,
  Back
};
enum class Tab { Stats, Items, Data };
inline int Category(fo3player::ItemKind kind) {
  using K = fo3player::ItemKind;
  switch (kind) {
  case K::Weapon:
    return 0;
  case K::Armour:
    return 1;
  case K::Aid:
  case K::Ingredient:
    return 2;
  case K::Ammo:
    return 4;
  default:
    return 3;
  }
}
inline bool Equippable(const fo3player::Item &i) {
  return i.playable && (i.kind == fo3player::ItemKind::Weapon ||
                        (i.kind == fo3player::ItemKind::Armour && i.bipedMask));
}
struct Menu {
  Tab tab = Tab::Stats;
  int page = 0;
  size_t selected = 0;
  bool inPage = false, dirty = true;
  size_t textScroll = 0;
  uint32_t playingNote = 0, selectedMarker = 0;
  std::vector<uint32_t> stations;
  fo3pipdata::MapContext map;
  float localX = 0, localY = 0;
  std::string actionError;
  uint64_t revision = UINT64_MAX, lastFrame = UINT64_MAX;
  unsigned rebuilds = 0;
  // IDs only: stacks, quantities, equipment and conditions remain in Player.
  std::vector<uint64_t> rows;
  void Refresh(const fo3player::Player &player) {
    if (revision == player.Revision())
      return;
    revision = player.Revision();
    dirty = true;
    ++rebuilds;
    RebuildRows(player);
  }
  void RebuildRows(const fo3player::Player &p) {
    uint64_t keep = selected < rows.size() ? rows[selected] : 0;
    rows.clear();
    if (tab == Tab::Items)
      for (const auto &s : p.Snapshot().inventory) {
        auto i = p.Definitions().items.find(s.formId);
        if (s.count > 0 && i != p.Definitions().items.end() &&
            i->second.playable && i->second.kind != fo3player::ItemKind::Note &&
            Category(i->second.kind) == page)
          rows.push_back(s.id);
      }
    if (tab == Tab::Stats && page == 3)
      for (auto &i : p.Snapshot().pipboy.perks)
        if (!p.Definitions().pipboy.perks.at(i.first).hidden)
          rows.push_back(i.first);
    if (tab == Tab::Data && page == 2)
      for (auto &i : p.Snapshot().pipboy.quests)
        rows.push_back(i.first);
    if (tab == Tab::Data && page == 3)
      for (auto &i : p.Snapshot().inventory)
        if (p.Definitions().pipboy.notes.count(i.formId) &&
            std::find(rows.begin(), rows.end(), i.formId) == rows.end())
          rows.push_back(i.formId);
    if (tab == Tab::Data && page == 4)
      for (auto id : stations)
        rows.push_back(id);
    auto name = [&](uint64_t id) -> std::string {
      if (tab == Tab::Items) {
        auto stack = Stack(p, id);
        return p.Definitions().items.at(stack->formId).name;
      }
      if (tab == Tab::Stats)
        return p.Definitions().pipboy.perks.at(id).name;
      if (page == 2)
        return p.Definitions().pipboy.quests.at(id).name;
      if (page == 3)
        return p.Definitions().pipboy.notes.at(id).name;
      return p.Definitions()
          .pipboy.stations.at(p.Definitions().pipboy.transmitters.at(id).base)
          .name;
    };
    std::stable_sort(rows.begin(), rows.end(), [&](auto a, auto b) {
      auto x = name(a), y = name(b);
      return x == y ? a < b : x < y;
    });
    auto it = std::find(rows.begin(), rows.end(), keep);
    selected =
        it != rows.end()
            ? size_t(it - rows.begin())
            : std::min(selected, rows.empty() ? size_t(0) : rows.size() - 1);
  }
  static const fo3player::Stack *Stack(const fo3player::Player &p,
                                       uint64_t id) {
    for (const auto &s : p.Snapshot().inventory)
      if (s.id == id)
        return &s;
    return nullptr;
  }
  bool BeginFrame(uint64_t frame) {
    if (lastFrame == frame)
      return false;
    lastFrame = frame;
    return true;
  }
  bool MapInteraction() const { return tab == Tab::Data && page < 2 && inPage; }
  void Pan(float x, float y, float seconds) {
    if (!MapInteraction())
      return;
    if (std::max(std::fabs(x), std::fabs(y)) < .18f)
      return;
    float dx = x * seconds * .35f / map.zoom,
          dy = -y * seconds * .35f / map.zoom;
    map.centre.x = std::clamp(map.centre.x + dx, 0.f, 1.f);
    map.centre.y = std::clamp(map.centre.y + dy, 0.f, 1.f);
    dirty = true;
  }
  bool Invoke(Action a, fo3player::Player &p) {
    bool mutation = false;
    dirty = true;
    actionError.clear();
    if (a == Action::NextTab || a == Action::PreviousTab) {
      tab = Tab((int(tab) + (a == Action::NextTab ? 1 : 2)) % 3);
      page = 0;
      selected = 0;
      inPage = false;
      textScroll = 0;
      RebuildRows(p);
    } else if (a == Action::NextPage || a == Action::PreviousPage) {
      if (!MapInteraction()) {
        page = (page + (a == Action::NextPage ? 1 : 4)) % 5;
        selected = 0;
        textScroll = 0;
        inPage = false;
        RebuildRows(p);
        map.centre = map.player;
      }
    } else if (a == Action::Back) {
      inPage = false;
      textScroll = 0;
    } else if (a == Action::Up || a == Action::Down) {
      int delta = a == Action::Up ? -1 : 1;
      if (page == 3 && inPage && (tab == Tab::Data || tab == Tab::Stats)) {
        if (delta < 0 && textScroll)
          --textScroll;
        if (delta > 0)
          ++textScroll;
      } else if (tab == Tab::Data && page == 2 && inPage) {
        if (delta < 0 && textScroll)
          --textScroll;
        if (delta > 0)
          ++textScroll;
      } else {
        size_t count = rows.size();
        if (tab == Tab::Stats)
          count = page == 1 ? 7 : page == 2 ? 13 : page == 4 ? 1 : count;
        if (delta < 0 && selected)
          --selected;
        if (delta > 0 && selected + 1 < count)
          ++selected;
      }
    } else if (a == Action::Accept) {
      if (tab == Tab::Data && page < 2) {
        if (!inPage)
          inPage = true;
        else if (page == 1 && map.mapWorld) {
          auto w = p.Definitions().pipboy.worlds.find(map.mapWorld);
          if (w != p.Definitions().pipboy.worlds.end()) {
            auto v = w->second.Unproject(map.centre);
            if (selectedMarker &&
                p.Definitions().pipboy.markers.count(selectedMarker)) {
              auto &m = p.Definitions().pipboy.markers.at(selectedMarker);
              v = fo3pipdata::WorldPoint(p.Definitions().pipboy,m.world,m.x,m.y);
            }
            auto old = p.Snapshot().pipboy.waypoint;
            mutation = old.world == map.mapWorld &&
                               std::fabs(old.point.x - v.x) < 1 &&
                               std::fabs(old.point.y - v.y) < 1
                           ? p.SetWaypoint(0, 0, 0)
                           : p.SetWaypoint(map.mapWorld, v.x, v.y);
          }
        }
      } else if (tab == Tab::Items && selected < rows.size()) {
        auto s = Stack(p, rows[selected]);
        if (s) {
          auto &i = p.Definitions().items.at(s->formId);
          if (Equippable(i))
            mutation = s->equipped ? p.Unequip(s->id) : p.Equip(s->id);
          else if (i.kind == fo3player::ItemKind::Aid ||
                   i.kind == fo3player::ItemKind::Ingredient) {
            if (p.CanUse(s->id, &actionError))
              mutation = p.Use(s->id);
          }
        }
      } else if (tab == Tab::Data && page == 4 && selected < rows.size()) {
        auto id = uint32_t(rows[selected]);
        mutation = p.TuneRadio(p.Snapshot().pipboy.tunedRadio == id ? 0 : id);
      } else if (tab == Tab::Data && page == 2 && selected < rows.size()) {
        inPage = true;
        mutation = p.SelectQuest(uint32_t(rows[selected]));
      } else if (tab == Tab::Data && page == 3 && selected < rows.size()) {
        if (!inPage) {
          inPage = true;
          textScroll = 0;
          auto id = uint32_t(rows[selected]);
          auto &n = p.Definitions().pipboy.notes.at(id);
          if (n.type == 0 || n.type == 3)
            playingNote = id;
        } else {
          auto id = uint32_t(rows[selected]);
          auto &n = p.Definitions().pipboy.notes.at(id);
          if (n.type == 0 || n.type == 3)
            playingNote = playingNote == id ? 0 : id;
        }
      } else
        inPage = true;
    }
    if (mutation)
      Refresh(p);
    return mutation;
  }
  bool NeedsRedraw(bool focus) const { return dirty && focus; }
};
// Edges require neutral after focus transitions. A held during closing cannot
// fall through to a world door/pickup; stick releases cannot snap-turn.
struct Input {
  bool focus = false, aLatch = false, bLatch = false, stickLatch = false,
       gripLatch = false, worldBlocked = false, mapMode = false;
  double repeat = 0;
  template <class Invoke>
  void Step(bool owns, float x, float y, bool a, bool b, double now,
            Invoke invoke) {
    Step(owns, x, y, a, b, 0, now, false, invoke);
  }
  template <class Invoke>
  void Step(bool owns, float x, float y, bool a, bool b, float grip, double now,
            bool map, Invoke invoke) {
    if (owns != focus) {
      focus = owns;
      mapMode = map;
      aLatch = a;
      bLatch = b;
      stickLatch = std::max(std::fabs(x), std::fabs(y)) > .30f;
      gripLatch = grip > .30f;
      worldBlocked = a;
      repeat = now + .35;
      return;
    }
    if (!a)
      worldBlocked = false;
    if (!focus)
      return;
    if (mapMode != map) {
      mapMode = map;
      stickLatch = std::max(std::fabs(x), std::fabs(y)) > .30f;
      repeat = now + .35;
    }
    if (grip < .30f)
      gripLatch = false;
    if (grip > .70f && !gripLatch) {
      gripLatch = true;
      invoke(Action::NextTab);
    }
    if (a && !aLatch)
      invoke(Action::Accept);
    if (b && !bLatch)
      invoke(Action::Back);
    aLatch = a;
    bLatch = b;
    float strength = std::max(std::fabs(x), std::fabs(y));
    if (strength < .30f) {
      stickLatch = false;
      return;
    }
    if (map || strength < .65f)
      return;
    bool horizontal = std::fabs(x) > std::fabs(y);
    if (!stickLatch || (!horizontal && now >= repeat)) {
      bool first = !stickLatch;
      stickLatch = true;
      repeat = now + (first ? .35 : .15);
      invoke(horizontal ? (x > 0 ? Action::NextPage : Action::PreviousPage)
                        : (y > 0 ? Action::Up : Action::Down));
    }
  }
  bool WorldA(bool down) const { return down && !focus && !worldBlocked; }
};
} // namespace fo3pip
