#pragma once
// Physical device policy and canonical player view. No XR, GL, or asset I/O.
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
enum class Action { NextTab, PreviousTab, Up, Down, Accept, Back };
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
            i->second.playable && Category(i->second.kind) == page)
          rows.push_back(s.id);
      }
    std::stable_sort(rows.begin(), rows.end(), [&](uint64_t a, uint64_t b) {
      auto sa = Stack(p, a), sb = Stack(p, b);
      const auto &ia = p.Definitions().items.at(sa->formId);
      const auto &ib = p.Definitions().items.at(sb->formId);
      return ia.name == ib.name ? a < b : ia.name < ib.name;
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
  // Return true only for a canonical persisted mutation.
  bool Invoke(Action a, fo3player::Player &p) {
    bool mutation = false;
    dirty = true;
    if (a == Action::NextTab || a == Action::PreviousTab) {
      tab = Tab((int(tab) + (a == Action::NextTab ? 1 : 2)) % 3);
      page = 0;
      selected = 0;
      inPage = false;
      RebuildRows(p);
    } else if (a == Action::Back) {
      inPage = false;
      selected = 0;
    } else if (a == Action::Up || a == Action::Down) {
      int delta = a == Action::Up ? -1 : 1;
      if (!inPage) {
        page = (page + delta + 5) % 5;
        selected = 0;
        RebuildRows(p);
      } else if (tab == Tab::Items && !rows.empty()) {
        if (delta < 0 && selected > 0)
          --selected;
        if (delta > 0 && selected + 1 < rows.size())
          ++selected;
      } else if (tab == Tab::Stats) {
        size_t count = page == 1 ? 7 : page == 2 ? 13 : 1;
        if (delta < 0 && selected > 0)
          --selected;
        if (delta > 0 && selected + 1 < count)
          ++selected;
      }
    } else if (a == Action::Accept) {
      if (!inPage)
        inPage = true;
      else if (tab == Tab::Items && selected < rows.size()) {
        const auto *s = Stack(p, rows[selected]);
        if (s) {
          auto i = p.Definitions().items.find(s->formId);
          if (i != p.Definitions().items.end() && Equippable(i->second))
            mutation = s->equipped ? p.Unequip(s->id) : p.Equip(s->id);
        }
      }
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
       worldBlocked = false;
  double repeat = 0;
  template <class Invoke>
  void Step(bool owns, float x, float y, bool a, bool b, double now,
            Invoke invoke) {
    if (owns != focus) {
      focus = owns;
      aLatch = a;
      bLatch = b;
      stickLatch = std::max(std::fabs(x), std::fabs(y)) > .30f;
      worldBlocked = a;
      repeat = now + .35;
      return;
    }
    if (!a)
      worldBlocked = false;
    if (!focus)
      return;
    if (a && !aLatch)
      invoke(Action::Accept);
    if (b && !bLatch)
      invoke(Action::Back);
    aLatch = a;
    bLatch = b;
    const float strength = std::max(std::fabs(x), std::fabs(y));
    if (strength < .30f) {
      stickLatch = false;
      return;
    }
    if (strength < .65f)
      return;
    if (!stickLatch || now >= repeat) {
      bool first = !stickLatch;
      stickLatch = true;
      repeat = now + (first ? .35 : .15);
      if (std::fabs(x) > std::fabs(y))
        invoke(x > 0 ? Action::NextTab : Action::PreviousTab);
      else
        invoke(y > 0 ? Action::Up : Action::Down);
    }
  }
  bool WorldA(bool down) const { return down && !focus && !worldBlocked; }
};
} // namespace fo3pip
