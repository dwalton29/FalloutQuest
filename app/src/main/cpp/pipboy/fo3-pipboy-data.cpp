#include "fo3-pipboy-data.h"
#include "data/fo3-esm-reader.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace fo3pipdata {
namespace {
struct Sub {
  std::string type;
  const uint8_t *p;
  uint32_t n;
};
using Subs = std::vector<Sub>;
const Sub *Find(const Subs &s, const char *t) {
  for (auto &v : s)
    if (v.type == t)
      return &v;
  return nullptr;
}
uint32_t U(const Sub *s, uint32_t offset = 0) {
  return s && s->n >= offset + 4 ? fo3esm::ReadU32(s->p + offset) : 0;
}
std::string Text(const Subs &s, const char *t) {
  auto v = Find(s, t);
  return v ? fo3esm::ZString(v->p, v->n) : "";
}
Condition Cond(const Sub &s) {
  Condition c;
  if (s.n >= 28) {
    c.flags = s.p[0];
    c.value = fo3esm::ReadF32(s.p + 4);
    c.function = fo3esm::ReadU16(s.p + 8);
    c.a = fo3esm::ReadU32(s.p + 12);
    c.b = fo3esm::ReadU32(s.p + 16);
    c.run = fo3esm::ReadU32(s.p + 20);
    c.reference = fo3esm::ReadU32(s.p + 24);
  }
  return c;
}
Placement Place(uint32_t flags, const Subs &s, uint32_t cell, uint32_t world) {
  Placement p;
  p.flags = flags;
  p.cell = cell;
  p.world = world;
  p.base = U(Find(s, "NAME"));
  auto d = Find(s, "DATA");
  if (d && d->n == 24) {
    p.x = fo3esm::ReadF32(d->p);
    p.y = fo3esm::ReadF32(d->p + 4);
    p.z = fo3esm::ReadF32(d->p + 8);
  }
  auto e = Find(s, "XESP");
  if (e && e->n >= 5) {
    p.parent = U(e);
    p.opposite = e->p[4] & 1;
  }
  return p;
}
std::string VoicePath(const Definitions &d, const Info &i, const Response &r,
                      uint32_t voice) {
  auto v = d.voices.find(voice);
  if (v == d.voices.end())
    return {};
  char tail[40];
  std::snprintf(tail, sizeof(tail), "_%08x_%u", i.id, r.number);
  // Resolve by exact INFO/response identity in the installed voice directory.
  // Never guess/truncate the authored filename prefix or synthesize a playlist.
  return "@voice:" + v->second + ":" + tail;
}
} // namespace
bool Relevant(const std::string &t) {
  return t == "WRLD" || t == "PERK" || t == "QUST" || t == "MGEF" ||
         t == "TACT" || t == "INFO" || t == "DIAL" || t == "SOUN" ||
         t == "VTYP" || t == "SCPT" || t == "NPC_";
}
void Decode(Definitions &d, const std::string &t, uint32_t id, uint32_t flags,
            const std::vector<uint8_t> &bytes, uint32_t cell, uint32_t world,
            uint32_t topic) {
  Subs s;
  fo3esm::WalkSubrecords(bytes,
                         [&](const char *tag, const uint8_t *p, uint32_t n) {
                           s.push_back({std::string(tag, 4), p, n});
                         });
  if (t == "DOOR")
    d.doorBases.insert(id);
  else if (t == "WRLD") {
    World w;
    w.name = Text(s, "FULL");
    w.image = Text(s, "ICON");
    w.parent = U(Find(s, "WNAM"));
    auto f = Find(s, "PNAM");
    if (f && f->n >= 2)
      w.parentFlags = fo3esm::ReadU16(f->p);
    auto m = Find(s, "MNAM");
    if (m && m->n == 16) {
      w.width = int32_t(U(m));
      w.height = int32_t(U(m, 4));
      w.nwX = int16_t(fo3esm::ReadU16(m->p + 8));
      w.nwY = int16_t(fo3esm::ReadU16(m->p + 10));
      w.seX = int16_t(fo3esm::ReadU16(m->p + 12));
      w.seY = int16_t(fo3esm::ReadU16(m->p + 14));
      w.valid = w.seX > w.nwX && w.nwY > w.seY && w.width > 0 && w.height > 0;
    }
    auto o = Find(s, "ONAM");
    if (o && o->n == 12) {
      w.scale = fo3esm::ReadF32(o->p);
      w.offsetX = fo3esm::ReadF32(o->p + 4);
      w.offsetY = fo3esm::ReadF32(o->p + 8);
    }
    f = Find(s, "DATA");
    if (f && f->n)
      w.flags = f->p[0];
    w.valid = w.valid && std::isfinite(w.scale) && w.scale > 0;
    d.worlds[id] = std::move(w);
  } else if (t == "CELL")
    d.cellWorlds[id] = world;
  else if (t == "REFR" || t == "ACHR" || t == "ACRE") {
    auto p = Place(flags, s, cell, world);
    if (p.base == 0x10 && Find(s, "XMRK")) {
      Marker m;
      static_cast<Placement &>(m) = p;
      m.name = Text(s, "FULL");
      auto f = Find(s, "FNAM");
      if (f && f->n)
        m.mapFlags = f->p[0];
      f = Find(s, "TNAM");
      if (f && f->n)
        m.type = f->p[0];
      d.markers[id] = std::move(m);
    }
    auto r = Find(s, "XRDO");
    if (r && r->n == 16) {
      Transmitter x;
      static_cast<Placement &>(x) = p;
      x.radius = fo3esm::ReadF32(r->p);
      x.range = U(r, 4);
      x.staticPercent = fo3esm::ReadF32(r->p + 8);
      x.position = U(r, 12);
      d.transmitters[id] = x;
    } // Retain only quest targets after finalization.
    d.targets[id] = p;
  } else if (t == "QUST") {
    Quest q;
    q.name = Text(s, "FULL");
    q.editor = Text(s, "EDID");
    q.icon = Text(s, "ICON");
    q.script = U(Find(s, "SCRI"));
    auto v = Find(s, "DATA");
    if (v && v->n >= 2) {
      q.flags = v->p[0];
      q.priority = v->p[1];
    }
    Stage *stage = nullptr;
    Objective *obj = nullptr;
    for (auto &a : s) {
      if (a.type == "INDX" && a.n == 2) {
        stage = &q.stages[fo3esm::ReadU16(a.p)];
        obj = nullptr;
      } else if (a.type == "QOBJ" && a.n == 4) {
        obj = &q.objectives[U(&a)];
        stage = nullptr;
      } else if (a.type == "NNAM" && obj)
        obj->text = fo3esm::ZString(a.p, a.n);
      else if (a.type == "QSTA" && obj && a.n >= 4)
        obj->targets.push_back(U(&a));
      else if (a.type == "QSDT" && stage && a.n)
        stage->flags |= a.p[0];
      else if (a.type == "CNAM" && stage)
        stage->logs.push_back(fo3esm::ZString(a.p, a.n));
      else if (a.type == "SCDA" && stage && a.n)
        stage->scripted = true;
      else if (a.type == "CTDA") {
        if (obj)
          obj->conditionalTargets = true;
        else if (stage)
          stage->conditional = true;
        else
          q.conditions.push_back(Cond(a));
      }
    }
    d.questNames[q.editor] = id;
    d.quests[id] = std::move(q);
  } else if (t == "PERK") {
    Perk p;
    p.name = Text(s, "FULL");
    p.description = Text(s, "DESC");
    p.icon = Text(s, "ICON");
    auto a = Find(s, "DATA");
    if (a && a->n >= 4) {
      p.minLevel = a->p[1];
      p.ranks = a->p[2];
      p.hidden = a->n > 4 && a->p[4];
    }
    PerkEffect *e = nullptr;
    uint8_t tab = 0;
    for (auto &v : s) {
      if (v.type == "PRKE" && v.n == 3) {
        p.effects.push_back({});
        e = &p.effects.back();
        e->type = v.p[0];
        e->rank = v.p[1];
        e->priority = v.p[2];
      } else if (v.type == "PRKF")
        e = nullptr;
      else if (e) {
        if (v.type == "DATA") {
          if (e->type == 0 && v.n == 8) {
            e->form = U(&v);
            e->stage = v.p[4];
          } else if (e->type == 1 && v.n == 4)
            e->form = U(&v);
          else if (e->type == 2 && v.n == 3) {
            e->entryPoint = v.p[0];
            e->function = v.p[1];
          }
        } else if (v.type == "PRKC" && v.n)
          tab = v.p[0];
        else if (v.type == "CTDA")
          e->conditions[tab].push_back(Cond(v));
        else if (v.type == "EPFT" && v.n)
          e->parameterType = v.p[0];
        else if (v.type == "EPFD")
          e->parameters.assign(v.p, v.p + v.n);
        else if (v.type == "SCDA" && v.n)
          e->scripted = true;
      }
    }
    d.perks[id] = std::move(p);
  } else if (t == "NOTE") {
    Note n;
    n.name = Text(s, "FULL");
    n.icon = Text(s, "ICON");
    n.image = Text(s, "XNAM");
    auto a = Find(s, "DATA");
    if (a && a->n)
      n.type = a->p[0];
    if (n.type == 1)
      n.text = Text(s, "TNAM");
    if (n.type == 0)
      n.sound = U(Find(s, "SNAM"));
    if (n.type == 3) {
      n.topic = U(Find(s, "TNAM"));
      n.npc = U(Find(s, "SNAM"));
    }
    d.notes[id] = std::move(n);
  } else if (t == "ALCH" || t == "INGR") {
    Ingestible a;
    a.flags = U(Find(s, "ENIT"), 8);
    Effect *e = nullptr;
    for (auto &v : s) {
      if (v.type == "EFID" && v.n == 4) {
        a.effects.push_back({});
        e = &a.effects.back();
        e->id = U(&v);
      } else if (v.type == "EFIT" && v.n == 20 && e) {
        e->magnitude = U(&v);
        e->area = U(&v, 4);
        e->duration = U(&v, 8);
        e->range = U(&v, 12);
        e->av = U(&v, 16);
      } else if (v.type == "CTDA" && e)
        e->conditional = true;
    }
    d.aid[id] = std::move(a);
  } else if (t == "MGEF") {
    auto a = Find(s, "DATA");
    if (a && a->n >= 72)
      d.magic[id] = {U(a), U(a, 64), U(a, 68), U(a, 8)};
  } else if (t == "TACT" && (flags & 0x20000)) {
    d.stations[id] = {Text(s, "FULL"), U(Find(s, "VNAM")), U(Find(s, "SNAM"))};
  } else if (t == "SOUN") {
    auto path = Text(s, "FNAM");
    std::replace(path.begin(), path.end(), '\\', '/');
    std::transform(path.begin(), path.end(), path.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    if (!path.empty() && path.rfind("sound/", 0) != 0)
      path = "sound/" + path;
    d.sounds[id] = std::move(path);
  } else if (t == "VTYP")
    d.voices[id] = Text(s, "EDID");
  else if (t == "NPC_") {
    d.npcVoices[id] = U(Find(s, "VTCK"));
    if (id == 7)
      d.playerFemale = (U(Find(s, "ACBS")) & 1) != 0;
  } else if (t == "DIAL") {
    auto name = Text(s, "EDID");
    d.topicNames[id] = name;
    if (name == "RadioHello")
      d.radioHello = id;
  } else if (t == "SCPT") {
    Script script;
    uint32_t index = 0;
    for (auto &a : s) {
      if (a.type == "SLSD" && a.n >= 4)
        index = U(&a);
      if (a.type == "SCVR")
        script.variables[index] = fo3esm::ZString(a.p, a.n);
    }
    d.scripts[id] = std::move(script);
  } else if (t == "INFO") {
    Info i;
    i.id = id;
    i.topic = topic;
    i.quest = U(Find(s, "QSTI"));
    i.speaker = U(Find(s, "ANAM"));
    auto a = Find(s, "DATA");
    if (a && a->n >= 3) {
      i.type = a->p[0];
      i.flags = a->p[2];
    }
    bool compiled = false, source = false;
    for (auto &v : s) {
      if (v.type == "TRDT" && v.n >= 20)
        i.responses.push_back({U(&v, 16), v.p[12], {}});
      else if (v.type == "NAM1" && !i.responses.empty())
        i.responses.back().text = fo3esm::ZString(v.p, v.n);
      else if (v.type == "CTDA")
        i.conditions.push_back(Cond(v));
      else if (v.type == "TCLT" && v.n == 4)
        i.links.push_back(U(&v));
      else if (v.type == "SCDA" && v.n)
        compiled = true;
      else if (v.type == "SCTX" && v.n) {
        source = true;
        i.scripts.push_back(fo3esm::ZString(v.p, v.n));
      }
    }
    i.compiledOnly = compiled && !source;
    d.topics[topic].push_back(std::move(i));
  }
}
void Finalize(Definitions &d) {
  std::unordered_set<uint32_t> retain;
  for (auto &p : d.targets)
    if (d.doorBases.count(p.second.base))
      d.doors[p.second.cell].push_back(p.second);
  for (auto &q : d.quests)
    for (auto &o : q.second.objectives)
      for (auto id : o.second.targets)
        retain.insert(id);
  for (auto &t : d.transmitters)
    if (t.second.position)
      retain.insert(t.second.position);
  for (auto &t : d.targets)
    if (t.second.parent)
      retain.insert(t.second.parent);
  for (auto i = d.targets.begin(); i != d.targets.end();)
    if (!retain.count(i->first))
      i = d.targets.erase(i);
    else
      ++i;
}
std::vector<std::string> BroadcastAudio(const Definitions &d, const Info &i,
                                        uint32_t voice) {
  std::vector<std::string> paths;
  if (i.speaker) {
    auto v = d.npcVoices.find(i.speaker);
    if (v != d.npcVoices.end())
      voice = v->second;
  }
  for (auto &r : i.responses) {
    auto s = d.sounds.find(r.sound);
    auto path = s != d.sounds.end() ? s->second : VoicePath(d, i, r, voice);
    if (!path.empty())
      paths.push_back(path);
  }
  return paths;
}
std::vector<std::string> NoteAudio(const Definitions &d, uint32_t id) {
  std::vector<std::string> paths;
  auto n = d.notes.find(id);
  if (n == d.notes.end())
    return paths;
  auto sound = d.sounds.find(n->second.sound);
  if (n->second.type == 0 && sound != d.sounds.end())
    paths.push_back(sound->second);
  if (n->second.type == 3) {
    auto v = d.npcVoices.find(n->second.npc);
    auto infos = d.topics.find(n->second.topic);
    if (v == d.npcVoices.end() || infos == d.topics.end())
      return paths;
    for (auto &i : infos->second) {
      bool eligible = true;
      for (auto &c : i.conditions) {
        float value = 0;
        if (c.run != 0 || (c.flags & 5)) {
          eligible = false;
          break;
        }
        if (c.function == 72)
          value = n->second.npc == c.a;
        else if (c.function == 131)
          value = uint32_t(d.playerFemale) == c.a;
        else {
          eligible = false;
          break;
        }
        bool pass = false;
        switch (c.flags >> 5) {
        case 0:
          pass = value == c.value;
          break;
        case 1:
          pass = value != c.value;
          break;
        default:
          break;
        }
        if (!pass) {
          eligible = false;
          break;
        }
      }
      if (!eligible)
        continue; // Authored note topic is fixed; no world
                  // quest conditions are executed here.
      for (auto &r : i.responses) {
        auto s = d.sounds.find(r.sound);
        auto path =
            s != d.sounds.end() ? s->second : VoicePath(d, i, r, v->second);
        if (!path.empty())
          paths.push_back(path);
      }
    }
  }
  return paths;
}
} // namespace fo3pipdata
