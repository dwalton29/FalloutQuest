#include "ui/pipboy/fo3-pipboy-state.h"
#include <cassert>
#include <iostream>
using namespace fo3pip;
static fo3player::Player Player() {
  fo3player::Catalog c;
  fo3player::Item w;
  w.formId = 1;
  w.name = "Weapon";
  w.kind = fo3player::ItemKind::Weapon;
  c.items[1] = w;
  fo3player::Item misc;
  misc.formId = 2;
  misc.name = "Abraxo Cleaner";
  c.items[2] = misc;
  return fo3player::Player(std::move(c));
}
int main() {
  const View good{true, true, .4f, 1, 1};
  Activation a;
  auto down = good;
  down.raised = false;
  a.Step(down, 0);
  assert(a.phase == Phase::Dormant); // 1
  auto away = good;
  away.facing = -1;
  a.Step(away, 0);
  assert(!a.Focus()); // 2
  auto far = good;
  far.distance = 1;
  a.Step(far, 0);
  assert(a.phase == Phase::Dormant); // 3
  a.Step(good, 1);
  assert(a.phase == Phase::Candidate); // 4
  a.Step(good, 1.151);
  assert(a.Focus()); // 5
  auto jitter = good;
  jitter.distance = .7f;
  jitter.facing = .65f;
  jitter.cone = .7f;
  a.Step(jitter, 2);
  assert(a.Focus()); // 6
  a.Step(away, 3);
  assert(!a.Focus());
  a.Step(good, 4);
  a.Step(good, 4.2);
  a.Step(down, 5);
  assert(!a.Focus()); // 7
  a.Step(good, 6);
  a.Step(good, 6.2);
  a.Step({}, 7);
  assert(!a.Focus()); // 8
  auto p = Player();
  Menu m;
  assert(m.BeginFrame(1));
  assert(!m.BeginFrame(1)); // 9
  assert(!m.BeginFrame(1));
  assert(m.BeginFrame(2)); // 10 stereo cannot advance
  m.Invoke(Action::NextTab, p);
  m.Refresh(p);
  auto n = m.rebuilds;
  p.Add(1, 1);
  m.Refresh(p);
  assert(m.rebuilds == n + 1 && m.rows.size() == 1); // 11
  n = m.rebuilds;
  for (int i = 0; i < 100; ++i)
    m.Refresh(p);
  assert(n == m.rebuilds); // 12
  m.Invoke(Action::Accept, p);
  for (int i = 0; i < 100; ++i)
    m.Invoke(Action::Down, p);
  assert(m.selected == 0); // 13
  assert(m.Invoke(Action::Accept, p));
  assert(p.Snapshot().inventory[0].equipped);
  assert(m.Invoke(Action::Accept, p));
  assert(!p.Snapshot().inventory[0].equipped); // 14
  Input input;
  int actions = 0;
  auto action = [&](Action) { ++actions; };
  input.Step(true, 0, 0, false, false, 1, action);
  assert(!input.WorldA(true)); // 15
  input.Step(false, 0, 0, false, false, 2, action);
  assert(input.WorldA(true)); // 16
  m.dirty = true;
  assert(!m.NeedsRedraw(false));
  assert(m.NeedsRedraw(true));
  m.dirty = false;
  assert(!m.NeedsRedraw(true)); // 18
  input.Step(true, .8f, 0, true, false, 3, action);
  input.Step(true, .8f, 0, true, false, 3.1, action);
  assert(actions == 0);
  input.Step(false, .8f, 0, true, false, 4, action);
  assert(!input.WorldA(true));
  input.Step(false, 0, 0, false, false, 4.1, action);
  assert(input.WorldA(true));
  auto view =
      Measure(true, {0, 1.3f, -.4f}, {0, 0, 1}, {0, 1.5f, 0}, {0, 0, -1},
              {0, 1.3f, -.4f}, {-.2f, 1.4f, 0}, {0, 1.35f, -.4f}, true);
  assert(Enter(view));
  auto torsoLag =
      Measure(true, {0, 1.3f, -.4f}, {0, 0, 1}, {0, 1.5f, 0}, {0, 0, -1},
              {0, 1.3f, .5f}, {-.2f, 1.4f, 0}, {0, 1.35f, .5f}, true);
  assert(Enter(torsoLag));
  auto behind =
      Measure(true, {0, 1.3f, .4f}, {0, 0, -1}, {0, 1.5f, 0}, {0, 0, -1},
              {0, 1.3f, .4f}, {-.2f, 1.4f, 0}, {0, 1.35f, .4f}, true);
  assert(!Enter(behind));
  m.Invoke(Action::Back, p);
  for (int i = 0; i < 3; ++i)
    m.Invoke(Action::Down, p);
  assert(m.page == 3);
  p.Add(2, 2);
  m.Refresh(p);
  assert(m.rows.size() == 1);
  assert(Menu::Stack(p, m.rows[0])->count == 2);
  std::cout << "Pip-Boy pose, hysteresis, stereo, revision, canonical equip "
               "and focus tests passed\n";
}
