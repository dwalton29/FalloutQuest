#include "fo3-pipboy-session.h"
#include "data/fo3-esm-reader.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace fo3pipdata {
namespace {
void Put(std::vector<uint8_t> &b, uint32_t n) {
  for (int i = 0; i < 4; ++i)
    b.push_back(uint8_t(n >> (i * 8)));
}
uint32_t Bits(float f) {
  uint32_t n;
  std::memcpy(&n, &f, 4);
  return n;
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 0;
  bool ok = true;
  uint32_t U() {
    if (n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = fo3esm::ReadU32(p + at);
    at += 4;
    return v;
  }
  float F() {
    auto v = U();
    float f;
    std::memcpy(&f, &v, 4);
    return f;
  }
  uint32_t Count(uint32_t max) {
    auto v = U();
    if (v > max || uint64_t(v) * 4 > n - at) {
      ok = false;
      return 0;
    }
    return v;
  }
};
template <class Map> std::vector<uint32_t> Keys(const Map &m) {
  std::vector<uint32_t> v;
  for (auto &p : m)
    v.push_back(p.first);
  std::sort(v.begin(), v.end());
  return v;
}
} // namespace
void EncodeState(const SessionState &s, std::vector<uint8_t> &b) {
  Put(b, 0x50495034);
  Put(b, s.selectedQuest);
  Put(b, s.tunedRadio);
  Put(b, s.aidUsed);
  Put(b, Bits(s.radiation));
  Put(b, s.waypoint.world);
  Put(b, Bits(s.waypoint.point.x));
  Put(b, Bits(s.waypoint.point.y));
  Put(b, s.perks.size());
  for (auto i : Keys(s.perks)) {
    Put(b, i);
    Put(b, s.perks.at(i));
  }
  std::vector<uint32_t> v(s.discovered.begin(), s.discovered.end());
  std::sort(v.begin(), v.end());
  Put(b, v.size());
  for (auto i : v)
    Put(b, i);
  Put(b, s.quests.size());
  for (auto i : Keys(s.quests)) {
    auto &q = s.quests.at(i);
    Put(b, i);
    Put(b, q.stage);
    Put(b, uint32_t(q.status));
    std::vector<uint16_t> stages(q.stages.begin(), q.stages.end());
    std::sort(stages.begin(), stages.end());
    Put(b, stages.size());
    for (auto stage : stages)
      Put(b, stage);
    Put(b, q.objectives.size());
    for (auto o : Keys(q.objectives)) {
      Put(b, o);
      auto &s = q.objectives.at(o);
      Put(b, (uint32_t(s.status) << 1) | s.displayed);
    }
  }
  Put(b,0x444c4731);
  for(const auto* set:{&s.talkedActors,&s.knownTopics}) {
    std::vector<uint32_t> ids(set->begin(),set->end());std::sort(ids.begin(),ids.end());
    Put(b,ids.size());for(auto id:ids)Put(b,id);
  }
  std::vector<uint64_t> said(s.saidInfos.begin(),s.saidInfos.end());std::sort(said.begin(),said.end());
  Put(b,said.size());for(auto id:said){Put(b,uint32_t(id>>32));Put(b,uint32_t(id));}
  std::vector<uint64_t> vars;for(auto& v:s.dialogueVariables)vars.push_back(v.first);std::sort(vars.begin(),vars.end());
  Put(b,vars.size());for(auto id:vars){Put(b,uint32_t(id>>32));Put(b,uint32_t(id));Put(b,Bits(s.dialogueVariables.at(id)));}
  Put(b,0x51535431); // Optional QST1 extension; old saves end after DLG1.
  Put(b,s.quests.size());
  for(auto id:Keys(s.quests)) {
    const auto& q=s.quests.at(id);Put(b,id);Put(b,q.running);Put(b,q.journal.size());
    for(auto entry:q.journal){Put(b,entry.first);Put(b,entry.second);}
  }
  Put(b,s.mutableGlobals.size());
  for(auto id:Keys(s.mutableGlobals)){Put(b,id);Put(b,Bits(s.mutableGlobals.at(id)));}

}
bool DecodeState(SessionState &out, const Definitions &d, const uint8_t *p,
                 size_t n, std::string &error) {
  Reader r{p, n};
  SessionState s;
  auto fail = [&]() {
    error = "Invalid Pip-Boy save extension";
    return false;
  };
  if (r.U() != 0x50495034)
    return fail();
  s.selectedQuest = r.U();
  s.tunedRadio = r.U();
  s.aidUsed = r.U();
  s.radiation = r.F();
  s.waypoint.world = r.U();
  s.waypoint.point = {r.F(), r.F()};
  if ((s.tunedRadio && !d.transmitters.count(s.tunedRadio)) ||
      (s.waypoint.world && !d.worlds.count(s.waypoint.world)) ||
      !std::isfinite(s.radiation) || s.radiation < 0 ||
      !std::isfinite(s.waypoint.point.x) || !std::isfinite(s.waypoint.point.y))
    return fail();
  auto count = r.Count(1000);
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto id = r.U(), rank = r.U();
    auto f = d.perks.find(id);
    if (f == d.perks.end() || rank == 0 || rank > f->second.ranks ||
        !s.perks.emplace(id, rank).second)
      return fail();
  }
  count = r.Count(10000);
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto id = r.U();
    if (!d.markers.count(id) || !s.discovered.insert(id).second)
      return fail();
  }
  count = r.Count(1000);
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto id = r.U();
    auto def = d.quests.find(id);
    if (def == d.quests.end() || s.quests.count(id))
      return fail();
    QuestState q;
    auto stage = r.U(), status = r.U();
    if (stage > 65535 || status > 2 ||
        (stage && !def->second.stages.count(stage)))
      return fail();
    q.stage = stage;
    q.status = Completion(status);
    q.running = q.status == Completion::Active; // Legacy format had no stopped state.
    auto stages = r.Count(10000);
    for (uint32_t j = 0; j < stages && r.ok; ++j) {
      auto v = r.U();
      if (v > 65535 || !def->second.stages.count(v) ||
          !q.stages.insert(v).second)
        return fail();
    }
    auto num = r.Count(10000);
    for (uint32_t j = 0; j < num && r.ok; ++j) {
      auto o = r.U(), v = r.U();
      if (v > 5 || !def->second.objectives.count(o) ||
          !q.objectives
               .emplace(o, ObjectiveState{bool(v & 1), Completion(v >> 1)})
               .second)
        return fail();
    }
    s.quests.emplace(id, std::move(q));
  }
  if(r.ok&&r.at<n) {
    if(r.U()!=0x444c4731)return fail();
    for(auto* set:{&s.talkedActors,&s.knownTopics}) {
      const auto size=r.Count(100000);for(uint32_t i=0;i<size&&r.ok;++i){auto id=r.U();if(!id||!set->insert(id).second)return fail();}
    }
    const auto said=r.Count(100000);for(uint32_t i=0;i<said&&r.ok;++i){auto actor=r.U(),info=r.U();if(!actor||!info||!s.saidInfos.insert((uint64_t(actor)<<32)|info).second)return fail();}
    const auto size=r.Count(100000);for(uint32_t i=0;i<size&&r.ok;++i) {
      auto owner=r.U(),index=r.U();auto value=r.F();
      if(!owner||!index||!std::isfinite(value)||!s.dialogueVariables.emplace((uint64_t(owner)<<32)|index,value).second)return fail();
    }
  }
  if(r.ok&&r.at<n) {
    if(r.U()!=0x51535431)return fail();
    auto size=r.Count(1000);if(size!=s.quests.size())return fail();
    std::unordered_set<uint32_t> seen;
    for(uint32_t i=0;i<size&&r.ok;++i) {
      auto id=r.U(),running=r.U();if(!s.quests.count(id)||running>1||!seen.insert(id).second)return fail();
      auto& q=s.quests.at(id);q.running=running!=0;
      auto entries=r.Count(100000);
      for(uint32_t j=0;j<entries&&r.ok;++j) {
        auto stage=r.U(),item=r.U();const auto& def=d.quests.at(id);
        auto st=def.stages.find(uint16_t(stage));
        if(stage>65535||st==def.stages.end()||item>=st->second.items.size()||!q.stages.count(uint16_t(stage)))return fail();
        q.journal.push_back({uint16_t(stage),item});
      }
    }
    size=r.Count(100000);
    for(uint32_t i=0;i<size&&r.ok;++i){auto id=r.U();float value=r.F();if(!id||!std::isfinite(value)||!s.mutableGlobals.emplace(id,value).second)return fail();}
  }
  if (!r.ok || r.at != n ||
      (s.selectedQuest &&
       (!s.quests.count(s.selectedQuest) ||
        s.quests.at(s.selectedQuest).status != Completion::Active)))
    return fail();
  out = std::move(s);
  return true;
}
} // namespace fo3pipdata
