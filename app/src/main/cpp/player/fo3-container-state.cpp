#include "fo3-player-state.h"
#include <algorithm>
#include <functional>
#include <limits>
#include <random>

namespace fo3player {
bool Player::CanLootContainer(uint32_t id) const {
  if(id!=0x14&&catalog_.pipboy.targets.count(id)&&
     catalog_.weapons.actors.count(catalog_.pipboy.targets.at(id).base))
    return ActorHealth(id)==0&&!Essential(id)&&catalog_.references.count(id)&&
      catalog_.actorInventories.count(catalog_.references.at(id).base);
  const auto r = catalog_.references.find(id);
  if (r == catalog_.references.end())
    return false;
  const auto c = catalog_.containers.find(r->second.base);
  return c != catalog_.containers.end() && c->second.valid &&
         !c->second.script &&
         (!r->second.owner || r->second.owner == PlayerBase) &&
         CanOpenDoor(id); // Share lock/key access; container ownership is separate.
}
const std::vector<Stack> *Player::ContainerContents(uint32_t id) const {
  const auto c = state_.containers.find(id);
  return c == state_.containers.end() ? nullptr : &c->second;
}
bool Player::PrepareContainer(uint32_t id) {
  if (!CanLootContainer(id))
    return false;
  const bool actor=catalog_.actorInventories.count(catalog_.references.at(id).base)!=0;
  if(!PrepareInventory(id,actor))return false;
  if(actor){
    auto saved=state_.actors.find(id);if(saved!=state_.actors.end())saved->second.equippedWeapon=0;
  }
  if(actor)for(auto& s:state_.containers.at(id)) {
    const auto& d=catalog_.items.at(s.formId).weapon;
    if(d.Firearm()&&!(d.flags2&2)&&s.loadedRounds){s.loadedRounds=0;++revision_;}
    if(s.equipped){s.equipped=false;++revision_;}
  }
  return true;
}
bool Player::PrepareActorInventory(uint32_t id) {
  const auto ref=catalog_.references.find(id);
  if(ref==catalog_.references.end()||!catalog_.actorInventories.count(ref->second.base))return false;
  return PrepareInventory(id,true);
}
bool Player::PrepareInventory(uint32_t id,bool actor) {
  if (state_.containers.count(id))
    return true;
  if (state_.containers.size() >= 10000)
    return false;
  size_t total = 0;
  for (const auto &c : state_.containers)
    total += c.second.size();
  std::vector<Stack> contents;
  uint64_t nextId = state_.nextStackId;
  // Runtime-owned RNG; outcomes persist once. Not Bethesda save/RNG parity.
  std::mt19937 random(id ^ catalog_.lootFingerprint ^
                      catalog_.worldFingerprint ^ state_.level);
  std::unordered_set<uint32_t> path;
  size_t work = 0;
  std::function<bool(uint32_t, int32_t, float, uint32_t, unsigned)> expand;
  expand = [&](uint32_t form, int32_t count, float condition, uint32_t owner,
               unsigned depth) {
    if (++work > 10000 || depth > 32 || count <= 0 ||
        (owner && owner != PlayerBase))
      return false;
    const auto item = catalog_.items.find(form);
    if (item != catalog_.items.end()) {
      if (item->second.maxCondition == 0 && condition != 1)
        return false;
      if (item->second.kind == ItemKind::Weapon) {
        if (static_cast<size_t>(count) > 10000 - contents.size() ||
            total + contents.size() + count > 30000 ||
            static_cast<uint64_t>(count) > UINT64_MAX - nextId)
          return false;
        for (int32_t i = 0; i < count; ++i)
          contents.push_back({nextId++, form, 1, condition, false});
        return true;
      }
      for (auto &s : contents)
        if (s.formId == form && s.condition == condition) {
          if (count > INT32_MAX - s.count)
            return false;
          s.count += count;
          return true;
        }
      if (total + contents.size() >= 30000 || contents.size() >= 10000 ||
          nextId == UINT64_MAX)
        return false;
      contents.push_back({nextId++, form, count, condition, false});
      return true;
    }
    const auto found = catalog_.lootLists.find(form);
    if (found == catalog_.lootLists.end() || !found->second.valid ||
        path.count(form))
      return false;
    const auto &list = found->second;
    float chance = list.chanceNone;
    if (list.global) {
      const auto g = catalog_.globals.find(list.global);
      if (g == catalog_.globals.end() || g->second < 0 || g->second > 100)
        return false;
      chance = g->second;
    }
    // Use All supersedes level filtering and per-count recalculation.
    const int rolls = (list.flags & 2) && !(list.flags & 4) ? count : 1;
    if (rolls > 10000)
      return false;
    path.insert(form);
    bool ok = true;
    for (int roll = 0; roll < rolls && ok; ++roll) {
      if (std::uniform_real_distribution<float>(0, 100)(random) < chance)
        continue;
      uint16_t highest = 0;
      for (const auto &e : list.entries)
        if (e.level <= state_.level)
          highest = std::max(highest, e.level);
      std::vector<const LootEntry *> eligible;
      for (const auto &e : list.entries) {
        if ((list.flags & 4) ||
            (e.level <= state_.level &&
             ((list.flags & 1)
                  ? (!catalog_.lootLevelDifference ||
                     highest - e.level <= catalog_.lootLevelDifference)
                  : e.level == highest)))
          eligible.push_back(&e);
      }
      if (eligible.empty())
        continue;
      auto child = [&](const LootEntry &e) {
        const int64_t amount = int64_t{e.count} * (rolls == 1 ? count : 1);
        if (amount <= 0 || amount > INT32_MAX)
          return false;
        return expand(e.form, static_cast<int32_t>(amount),
                      e.extra ? e.condition : condition,
                      e.extra ? e.owner : owner, depth + 1);
      };
      if (list.flags & 4) {
        for (const auto *e : eligible)
          if (!child(*e)) {
            ok = false;
            break;
          }
      } else
        ok = child(*eligible[std::uniform_int_distribution<size_t>(
            0, eligible.size() - 1)(random)]);
    }
    path.erase(form);
    return ok;
  };
  uint32_t base=catalog_.references.at(id).base;
  if(actor) {
    for(int depth=0;depth<16;++depth){const auto a=catalog_.weapons.actors.find(base);
      if(a==catalog_.weapons.actors.end())return false;
      if(!(a->second.templates&0x100))break;
      base=a->second.templateId;if(depth==15)return false;
    }
  }
  const auto found=(actor?catalog_.actorInventories:catalog_.containers).find(base);
  if(found==(actor?catalog_.actorInventories:catalog_.containers).end()||!found->second.valid)return false;
  const auto &c=found->second;
  for (const auto &e : c.entries)
    if (!expand(e.form, e.count, e.condition, e.owner, 0))
      return false;
  state_.containers.emplace(id, std::move(contents));
  state_.nextStackId = nextId;
  ++revision_;
  return true;
}
bool Player::CanTakeContainerStack(uint32_t id, uint64_t stack) const {
  if (!CanLootContainer(id))
    return false;
  const auto *contents = ContainerContents(id);
  if (!contents)
    return false;
  for (const auto &s : *contents)
    if (s.id == stack) {
      const auto &item = catalog_.items.at(s.formId);
      return item.playable && !item.script && !item.name.empty();
    }
  return false;
}
bool Player::TakeContainerStack(uint32_t id, uint64_t stack) {
  if (!CanTakeContainerStack(id, stack))
    return false;
  auto &contents = state_.containers.at(id);
  const auto s = std::find_if(contents.begin(), contents.end(),
                              [&](const Stack &s) { return s.id == stack; });
  if (catalog_.items.at(s->formId).kind == ItemKind::Weapon) {
    if (state_.inventory.size() >= 10000 || s->count != 1)
      return false;
    auto actor=state_.actors.find(id);if(actor!=state_.actors.end()&&actor->second.equippedWeapon==stack)actor->second.equippedWeapon=0;
    state_.inventory.push_back(*s);
    state_.inventory.back().equipped=false;
    contents.erase(s);
    ++revision_;
    return true;
  }
  if (!Add(s->formId, s->count, s->condition))
    return false;
  contents.erase(s);
  return true;
}
} // namespace fo3player
