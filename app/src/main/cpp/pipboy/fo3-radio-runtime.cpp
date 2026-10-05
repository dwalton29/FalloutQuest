#include "fo3-radio-runtime.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
namespace fo3pipdata {
namespace {
std::string Lower(std::string s) {
  for (auto &c : s)
    c = char(std::tolower((unsigned char)c));
  return s;
}
std::string Key(const Definitions &d, uint32_t quest, uint32_t variable) {
  auto q = d.quests.find(quest);
  if (q == d.quests.end())
    return {};
  auto s = d.scripts.find(q->second.script);
  if (s == d.scripts.end())
    return {};
  auto v = s->second.variables.find(variable);
  return v == s->second.variables.end()
             ? ""
             : Lower(q->second.editor + "." + v->second);
}
bool Compare(float a, float b, uint8_t op) {
  switch (op >> 5) {
  case 0:
    return a == b;
  case 1:
    return a != b;
  case 2:
    return a > b;
  case 3:
    return a >= b;
  case 4:
    return a < b;
  case 5:
    return a <= b;
  default:
    return false;
  }
}
} // namespace
bool Enabled(const Definitions &d, const Placement &p) {
  if (p.flags & 0x820)
    return false;
  auto parent = p.parent;
  bool negate = p.opposite;
  std::unordered_set<uint32_t> seen;
  while (parent) {
    auto f = d.targets.find(parent);
    if (f == d.targets.end() || !seen.insert(parent).second)
      return false;
    bool enabled = !(f->second.flags & 0x820);
    if (negate ? enabled : !enabled)
      return false;
    negate = f->second.opposite;
    parent = f->second.parent;
  }
  return true;
}
bool InRange(const Definitions &d, const Transmitter &t, const Location &l) {
  if (!Enabled(d, t))
    return false;
  if (t.range == 1)
    return true;
  if (t.range == 4)
    return t.cell == l.cell;
  if (t.range == 2)
    return t.world && t.world == l.world;
  if (t.range == 3)
    return t.cell == l.cell;
  auto position = d.targets.find(t.position);
  const Placement &p = position == d.targets.end()
                           ? static_cast<const Placement &>(t)
                           : position->second;
  auto root = [&](uint32_t world, float x, float y) {
    for (int depth = 0; depth < 8; ++depth) {
      auto w = d.worlds.find(world);
      if (w == d.worlds.end() || !w->second.parent ||
          !(w->second.parentFlags & 4))
        break;
      x = (x / 4096.f * w->second.scale + w->second.offsetX) * 4096.f;
      y = (y / 4096.f * w->second.scale + w->second.offsetY) * 4096.f;
      world = w->second.parent;
    }
    return Location{0, world, x, y, 0};
  };
  auto from = root(p.world, p.x, p.y), to = root(l.world, l.x, l.y);
  if (from.world != to.world)
    return false;
  float dx = from.x - to.x, dy = from.y - to.y;
  return t.radius > 0 && dx * dx + dy * dy <= t.radius * t.radius;
}
std::vector<uint32_t> AvailableStations(const Definitions &d,
                                        const Location &l) {
  std::vector<uint32_t> out;
  std::unordered_set<uint32_t> bases;
  for (auto &t : d.transmitters) {
    auto s = d.stations.find(t.second.base);
    if (s != d.stations.end() && !s->second.name.empty() &&
        InRange(d, t.second, l) && bases.insert(t.second.base).second)
      out.push_back(t.first);
  }
  std::sort(out.begin(), out.end(), [&](auto a, auto b) {
    return d.stations.at(d.transmitters.at(a).base).name <
           d.stations.at(d.transmitters.at(b).base).name;
  });
  return out;
}
bool Broadcast::Tune(const Definitions &d, uint32_t id) {
  station = quest = topic = 0;
  next.clear();
  variables.clear();
  if (!id)
    return true;
  auto t = d.transmitters.find(id);
  if (t == d.transmitters.end() || !d.stations.count(t->second.base))
    return false;
  station = t->second.base;
  for (auto &q : d.quests) {
    for (auto &c : q.second.conditions)
      if (c.function == 72 && c.a == station && c.run == 0 && c.value == 1) {
        quest = q.first;
        break;
      }
    if (quest)
      break;
  }
  topic = d.radioHello;
  return d.stations.at(station).sound || (quest && topic);
}
bool Broadcast::Conditions(const Definitions &d, const SessionState &s,
                           const std::vector<Condition> &conditions) const {
  bool group = false;
  for (auto &c : conditions) {
    bool known = true;
    float value = 0;
    if (c.flags & 4 || c.run != 0 || !std::isfinite(c.value))
      return false;
    switch (c.function) {
    case 72:
      value = c.a == station;
      break;
    case 79: {
      auto key = Key(d, c.a, c.b);
      if (key.empty())
        known = false;
      else {
        auto v = variables.find(key);
        auto persistent=s.dialogueVariables.find((uint64_t(c.a)<<32)|c.b);
        value = persistent!=s.dialogueVariables.end()?persistent->second:(v == variables.end() ? 0 : v->second);
      }
      break;
    }
    case 58: {
      auto q = s.quests.find(c.a);
      if (q == s.quests.end())
        known = false;
      else
        value = q->second.stage;
      break;
    }
    case 59: {
      auto q = s.quests.find(c.a);
      if (q == s.quests.end())
        known = false;
      else
        value = q->second.stages.count(c.b);
      break;
    }
    default:
      known = false;
    }
    if (!known)
      return false;
    group = group || Compare(value, c.value, c.flags);
    if (!(c.flags & 1)) {
      if (!group)
        return false;
      group = false;
    }
  }
  return conditions.empty() || !(conditions.back().flags & 1);
}
bool Broadcast::Assign(const Definitions &d,
                       const std::vector<std::string> &scripts, bool apply) {
  auto vars = variables;
  auto recognized = [&](std::string token) {
    auto dot = token.find('.');
    if (dot == std::string::npos)
      return false;
    for (auto &q : d.quests)
      if (Lower(q.second.editor) == token.substr(0, dot)) {
        auto s = d.scripts.find(q.second.script);
        if (s != d.scripts.end())
          for (auto &v : s->second.variables)
            if (Lower(v.second) == token.substr(dot + 1))
              return true;
      }
    return false;
  };
  auto value = [&](const std::string &token, float &out) {
    char *end = nullptr;
    out = std::strtof(token.c_str(), &end);
    if (end && end != token.c_str() && !*end)
      return std::isfinite(out);
    if (!recognized(token))
      return false;
    auto v = vars.find(token);
    out = v == vars.end() ? 0 : v->second;
    return true;
  };
  for (auto &src : scripts) {
    std::istringstream lines(Lower(src));
    std::string line;
    while (std::getline(lines, line)) {
      auto comment = line.find(';');
      if (comment != std::string::npos)
        line.resize(comment);
      for (auto &c : line)
        if (c == '(' || c == ')' || c == '\r')
          c = ' ';
      std::istringstream in(line);
      std::string set, key, to, a, op, b, extra;
      if (!(in >> set))
        continue;
      if (set != "set" || !(in >> key >> to >> a) || to != "to" ||
          !recognized(key))
        return false;
      float n = 0;
      if (!value(a, n))
        return false;
      if (in >> op) {
        float rhs;
        if (!(in >> b) || !value(b, rhs) || (op != "+" && op != "-") ||
            (in >> extra))
          return false;
        n += op == "+" ? rhs : -rhs;
      }
      if (!std::isfinite(n))
        return false;
      vars[key] = n;
    }
  }
  if (apply)
    variables = std::move(vars);
  return true;
}
std::vector<std::string> Broadcast::Advance(const Definitions &d,
                                            const SessionState &s,
                                            std::string &error) {
  error.clear();
  auto stationDef = d.stations.find(station);
  if (stationDef == d.stations.end()) {
    error = "Station missing";
    return {};
  }
  auto direct = d.sounds.find(stationDef->second.sound);
  if (direct != d.sounds.end())
    return {direct->second};
  for (int hop = 0; hop < 16; ++hop) {
    std::vector<uint32_t> choices =
        next.empty() ? std::vector<uint32_t>{topic} : next;
    std::shuffle(choices.begin(), choices.end(), random);
    const Info *chosen = nullptr;
    for (auto t : choices) {
      auto list = d.topics.find(t);
      if (list == d.topics.end())
        continue;
      std::vector<const Info *> eligible;
      for (auto &i : list->second)
        if (i.type == 7 && i.quest == quest && !(i.flags & 4) &&
            !i.compiledOnly && Conditions(d, s, i.conditions) &&
            Assign(d, i.scripts, false))
          eligible.push_back(&i);
      if (!eligible.empty()) {
        chosen = eligible.front();
        if (chosen->flags & 2)
          chosen = eligible[std::uniform_int_distribution<size_t>(
              0, eligible.size() - 1)(random)];
        break;
      }
    }
    if (!chosen) {
      error = "No evaluable original broadcast branch (quest/condition/script "
              "runtime required)";
      return {};
    }
    auto paths = BroadcastAudio(d, *chosen, stationDef->second.voice);
    if (paths.empty()) {
      error = "Original voice type/audio response unresolved";
      return {};
    }
    Assign(d, chosen->scripts, true);
    next = chosen->links;
    topic = (chosen->flags & 1) ? d.radioHello : chosen->topic;
    if (chosen->flags & 1)
      next.clear();
    return paths;
  }
  error = "Broadcast graph exceeded bounded traversal";
  return {};
}
} // namespace fo3pipdata
