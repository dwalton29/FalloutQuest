#include "fo3-player-state.h"
#include <algorithm>
#include <cmath>
namespace fo3player {
bool Player::RecordDialogue(uint32_t actor,uint32_t info,const std::vector<uint32_t>& topics) {
  if(!catalog_.pipboy.referenceScripts.count(actor))return false;
  bool changed=state_.pipboy.talkedActors.insert(actor).second;
  if(info)changed=state_.pipboy.saidInfos.insert((uint64_t(actor)<<32)|info).second||changed;
  for(auto topic:topics)if(catalog_.pipboy.dialogueTopics.count(topic))changed=state_.pipboy.knownTopics.insert(topic).second||changed;
  if(changed)++revision_;
  return true;
}
bool Player::SetDialogueVariable(uint64_t key,float value) {
  if(!key||!std::isfinite(value))return false;
  state_.pipboy.dialogueVariables[key]=value;++revision_;return true;
}
bool Player::GrantPerk(uint32_t id, uint8_t rank) {
  auto p = catalog_.pipboy.perks.find(id);
  if (p == catalog_.pipboy.perks.end() || !rank || rank > p->second.ranks)
    return false;
  state_.pipboy.perks[id] = rank;
  ++revision_;
  return true;
}
bool Player::StartQuest(uint32_t id) {
  if (!catalog_.pipboy.quests.count(id))
    return false;
  if (!state_.pipboy.quests.count(id)) {
    state_.pipboy.quests[id] = {};
    ++revision_;
  }
  return true;
}
bool Player::SetQuestStage(uint32_t id, uint16_t stage) {
  auto q = catalog_.pipboy.quests.find(id);
  if (q == catalog_.pipboy.quests.end() || !q->second.stages.count(stage) ||
      !state_.pipboy.quests.count(id))
    return false;
  auto &s = state_.pipboy.quests.at(id);
  s.stage = stage;
  s.stages.insert(stage);
  ++revision_;
  return true;
}
bool Player::SetObjective(uint32_t id, uint32_t index, bool displayed,
                          fo3pipdata::Completion status) {
  auto q = catalog_.pipboy.quests.find(id);
  if (q == catalog_.pipboy.quests.end() || !q->second.objectives.count(index) ||
      !state_.pipboy.quests.count(id) || int(status) > 2)
    return false;
  state_.pipboy.quests.at(id).objectives[index] = {displayed, status};
  ++revision_;
  return true;
}
bool Player::FinishQuest(uint32_t id, fo3pipdata::Completion status) {
  if (!state_.pipboy.quests.count(id) ||
      status == fo3pipdata::Completion::Active || int(status) > 2)
    return false;
  state_.pipboy.quests.at(id).status = status;
  if (state_.pipboy.selectedQuest == id)
    state_.pipboy.selectedQuest = 0;
  ++revision_;
  return true;
}
bool Player::SelectQuest(uint32_t id) {
  if (id &&
      (!state_.pipboy.quests.count(id) ||
       state_.pipboy.quests.at(id).status != fo3pipdata::Completion::Active))
    return false;
  state_.pipboy.selectedQuest = id;
  ++revision_;
  return true;
}
bool Player::Discover(uint32_t id) {
  if (!catalog_.pipboy.markers.count(id))
    return false;
  if (state_.pipboy.discovered.insert(id).second)
    ++revision_;
  return true;
}
bool Player::SetWaypoint(uint32_t world, float x, float y) {
  if (world && (!catalog_.pipboy.worlds.count(world) || !std::isfinite(x) ||
                !std::isfinite(y)))
    return false;
  state_.pipboy.waypoint = {world, {x, y}};
  ++revision_;
  return true;
}
bool Player::TuneRadio(uint32_t ref) {
  if (ref && !catalog_.pipboy.transmitters.count(ref))
    return false;
  if (state_.pipboy.tunedRadio != ref) {
    state_.pipboy.tunedRadio = ref;
    ++revision_;
  }
  return true;
}
bool Player::CanUse(uint64_t id, std::string *why) const {
  auto reject = [&](const char *text) {
    if (why)
      *why = text;
    return false;
  };
  auto s = std::find_if(state_.inventory.begin(), state_.inventory.end(),
                        [&](const Stack &s) { return s.id == id; });
  if (s == state_.inventory.end())
    return reject("Item absent");
  auto item = catalog_.items.find(s->formId);
  auto a = catalog_.pipboy.aid.find(s->formId);
  if (item == catalog_.items.end() || item->second.script ||
      item->second.questItem || item->second.cannotDrop ||
      a == catalog_.pipboy.aid.end() || a->second.effects.empty())
    return reject("Requires item script/effect runtime");
  for (auto &e : a->second.effects) {
    auto m = catalog_.pipboy.magic.find(e.id);
    if (m == catalog_.pipboy.magic.end() || e.conditional || e.area ||
        e.duration || e.range || m->second.archetype != 0 ||
        (m->second.flags & 0x100000) ||
        ((m->second.av != 16) && (m->second.av != 12)))
      return reject("Requires timed, conditional or unsupported actor effect");
  }
  return true;
}
bool Player::Use(uint64_t id) {
  if (!CanUse(id))
    return false;
  auto s = std::find_if(state_.inventory.begin(), state_.inventory.end(),
                        [&](const Stack &s) { return s.id == id; });
  auto effects = catalog_.pipboy.aid.at(s->formId)
                     .effects; // validate all before consuming
  for (auto &e : effects) {
    auto &m = catalog_.pipboy.magic.at(e.id);
    float amount = float(e.magnitude);
    if (m.av == 16) {
      if (m.flags & 4)
        DamageHealth(amount);
      else
        RestoreHealth(amount);
    } else if (m.av == 12) {
      if (m.flags & 4) {
        state_.apSpent = std::min(MaxActionPoints(), state_.apSpent + amount);
      } else
        RestoreActionPoints(amount);
    }
  }
  --s->count;
  if (!s->count)
    state_.inventory.erase(s);
  ++state_.pipboy.aidUsed;
  ++revision_;
  return true;
}
} // namespace fo3player
