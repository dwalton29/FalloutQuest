#include "player/fo3-player-state.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace fo3player {
namespace {
Stack *Find(State &state, uint64_t id) {
  for (auto &s : state.inventory)
    if (s.id == id) return &s;
  return nullptr;
}
}
const Stack *Player::Weapon(uint64_t id) const {
  for (const auto &s : state_.inventory)
    if (s.id == id && catalog_.items.at(s.formId).kind == ItemKind::Weapon)
      return &s;
  return nullptr;
}
const Stack *Player::EquippedWeapon() const {
  for (const auto &s : state_.inventory)
    if (s.equipped && catalog_.items.at(s.formId).kind == ItemKind::Weapon)
      return &s;
  return nullptr;
}
int32_t Player::AmmoReserve(uint32_t ammo) const {
  const auto item = catalog_.items.find(ammo);
  if (item == catalog_.items.end() || item->second.kind != ItemKind::Ammo) return 0;
  int64_t count = 0;
  for (const auto &s : state_.inventory)
    if (s.formId == ammo) count += s.count;
  return static_cast<int32_t>(std::min<int64_t>(count, INT32_MAX));
}
bool Player::PickupWeapon(uint32_t reference) {
  if (!CanPickup(reference)) return false;
  const auto &ref = catalog_.references.at(reference);
  const auto &item = catalog_.items.at(ref.base);
  if (item.kind != ItemKind::Weapon || !item.weapon.Firearm()) return false;
  const auto id = state_.nextStackId;
  if (!Pickup(reference)) return false;
  // Pickup/Add already preflighted capacity and gave every instance a unique ID.
  return Equip(id);
}
bool Player::DropWeapon(uint64_t id, const fo3weapon::WorldPose &pose) {
  const auto *s = Weapon(id);
  if (!s || s->count != 1 || !fo3weapon::ValidPose(pose) ||
      state_.worldWeapons.size() >= 10000) return false;
  const auto &item = catalog_.items.at(s->formId);
  if (!item.weapon.Firearm() || !item.playable || item.script ||
      item.questItem || item.cannotDrop) return false;
  Stack instance = *s;
  instance.equipped = false;
  state_.worldWeapons.push_back({instance, pose});
  const auto it = std::find_if(state_.inventory.begin(), state_.inventory.end(),
                             [&](const Stack &v) {return v.id == id;});
  state_.inventory.erase(it);
  ++revision_;
  return true;
}
bool Player::PickupWorldWeapon(uint64_t id) {
  auto it = std::find_if(state_.worldWeapons.begin(), state_.worldWeapons.end(),
                       [&](const WorldWeapon &w) {return w.instance.id == id;});
  if (it == state_.worldWeapons.end() || state_.inventory.size() >= 10000) return false;
  const auto item = catalog_.items.find(it->instance.formId);
  if (item == catalog_.items.end() || item->second.kind != ItemKind::Weapon ||
      !item->second.weapon.Firearm() || !item->second.playable ||
      it->instance.count != 1) return false;
  const Stack instance = it->instance;
  state_.inventory.push_back(instance);
  state_.worldWeapons.erase(it);
  // Equip only an authored playable firearm; no IDs are allocated or merged.
  return Equip(id);
}
bool Player::UpdateWorldWeapon(uint64_t id, const fo3weapon::WorldPose &pose) {
  if (!fo3weapon::ValidPose(pose)) return false;
  for (auto &w : state_.worldWeapons) if (w.instance.id == id) {
    w.pose = pose;
    ++revision_;
    return true;
  }
  return false;
}
bool Player::FireWeapon(uint64_t id) {
  const auto *found = Weapon(id);
  if (!found) return false;
  const auto &item = catalog_.items.at(found->formId);
  const auto &def = item.weapon;
  if (!found->equipped || !def.Firearm() || found->needsAction ||
      found->condition <= 0 || found->loadedRounds < def.ammoUse) return false;
  auto *s = Find(state_, id);
  s->loadedRounds -= def.ammoUse;
  float degradation = catalog_.weapons.damageGun;
  if (def.skill == 34) degradation = catalog_.weapons.damageEnergy;
  if (def.animation >= 8) degradation = catalog_.weapons.damageLauncher;
  if (def.flags2 & 128) degradation = def.degradationOverride;
  if (item.maxCondition > 0 && std::isfinite(degradation) && degradation >= 0)
    s->condition = std::max(0.0f, s->condition - def.damage * degradation / item.maxCondition);
  ++revision_;
  return true;
}
bool Player::EjectMagazine(uint64_t id) {
  const auto *s = Weapon(id);
  if (!s || !s->equipped) return false;
  const auto &def = catalog_.items.at(s->formId).weapon;
  const auto ammo = catalog_.items.find(def.ammo);
  if (!def.Firearm() || ammo == catalog_.items.end() || ammo->second.kind != ItemKind::Ammo)
    return false;
  const auto rounds = s->loadedRounds;
  if (rounds && !Add(def.ammo, rounds)) return false;
  // Add may reallocate the inventory; never retain the old pointer.
  auto *weapon = Find(state_, id);
  weapon->loadedRounds = 0;
  weapon->needsAction = true;
  ++revision_;
  return true;
}
bool Player::LoadMagazine(uint64_t id) {
  const auto *s = Weapon(id);
  if (!s || !s->equipped || s->loadedRounds || !s->needsAction) return false;
  const auto &def = catalog_.items.at(s->formId).weapon;
  if (!def.Firearm()) return false;
  int32_t rounds = std::min<int32_t>(def.clip, AmmoReserve(def.ammo));
  if (!rounds) return false;
  const int32_t loaded = rounds;
  for (auto it = state_.inventory.begin(); it != state_.inventory.end() && rounds;) {
    if (it->formId != def.ammo) {++it; continue;}
    const int32_t take = std::min(rounds, it->count);
    it->count -= take;
    rounds -= take;
    if (!it->count) it = state_.inventory.erase(it); else ++it;
  }
  Find(state_, id)->loadedRounds = static_cast<uint16_t>(loaded);
  ++revision_;
  return true;
}
bool Player::ChamberWeapon(uint64_t id) {
  const auto *s = Weapon(id);
  if (!s || !s->equipped || !s->needsAction || !s->loadedRounds ||
      !catalog_.items.at(s->formId).weapon.Firearm()) return false;
  Find(state_, id)->needsAction = false;
  ++revision_;
  return true;
}
bool Player::BootstrapDevelopmentWeapon() {
  if (state_.developmentWeaponGranted) return true;
  constexpr uint32_t pistol = 0x434f;
  const auto item = catalog_.items.find(pistol);
  if (item == catalog_.items.end() || !item->second.weapon.Firearm() ||
      item->second.editorId != "Weap10mmPistol") return false;
  // Transaction on a copy: failed capacity/ammo/definition checks change nothing.
  Player transaction = *this;
  uint64_t id = 0;
  for (const auto &s : transaction.state_.inventory)
    if (s.formId == pistol) {id = s.id; break;}
  if (!id) {
    id = transaction.state_.nextStackId;
    if (!transaction.Add(pistol, 1)) return false;
  }
  if (!transaction.Equip(id)) return false;
  const auto &def = item->second.weapon;
  if (!transaction.AmmoReserve(def.ammo) &&
      !transaction.Add(def.ammo, def.clip * 5)) return false;
  if (!transaction.Weapon(id)->loadedRounds) {
    if (!transaction.EjectMagazine(id) || !transaction.LoadMagazine(id) ||
        !transaction.ChamberWeapon(id)) return false;
  }
  transaction.state_.developmentWeaponGranted = true;
  state_ = std::move(transaction.state_);
  ++revision_;
  return true;
}
bool Player::MigrateWeaponInstances(State &state) const {
  auto split = [&](std::vector<Stack> &stacks, size_t limit) {
    const size_t original = stacks.size();
    size_t extra = 0;
    for (const auto &s : stacks)
      if (catalog_.items.at(s.formId).kind == ItemKind::Weapon)
        extra += static_cast<size_t>(s.count - 1);
    if (extra > limit - original || extra > UINT64_MAX - state.nextStackId) return false;
    stacks.reserve(original + extra);
    for (size_t i = 0; i < original; ++i) {
      if (catalog_.items.at(stacks[i].formId).kind != ItemKind::Weapon) continue;
      const int32_t count = stacks[i].count;
      stacks[i].count = 1;
      for (int32_t n = 1; n < count; ++n) {
        Stack s = stacks[i]; s.id = state.nextStackId++; s.equipped = false;
        stacks.push_back(s);
      }
    }
    return true;
  };
  if (!split(state.inventory, 10000)) return false;
  size_t total = 0;
  for (auto &c : state.containers) {
    if (!split(c.second, 10000)) return false;
    total += c.second.size();
    if (total > 30000) return false;
  }
  return true;
}
}
