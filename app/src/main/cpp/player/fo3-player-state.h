#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fo3player {
constexpr uint32_t PlayerBase = 0x7u;
enum class Special : uint8_t {
  Strength,
  Perception,
  Endurance,
  Charisma,
  Intelligence,
  Agility,
  Luck
};
// DNAM order; Throwing is an unused original slot, not New Vegas Survival.
enum class Skill : uint8_t {
  Barter,
  BigGuns,
  EnergyWeapons,
  Explosives,
  Lockpick,
  Medicine,
  MeleeWeapons,
  Repair,
  Science,
  SmallGuns,
  Sneak,
  Speech,
  Throwing,
  Unarmed
};
enum class ItemKind : uint8_t {
  Weapon,
  Armour,
  Ammo,
  Aid,
  Ingredient,
  Misc,
  Key,
  Book,
  Note
};

struct Item {
  uint32_t formId = 0, recordFlags = 0, bipedMask = 0, script = 0,
           enchantment = 0;
  ItemKind kind = ItemKind::Misc;
  std::string editorId, name, model, femaleModel, icon, femaleIcon;
  int32_t value = 0, maxCondition = 0;
  float weight = 0;
  bool playable = true, questItem = false, cannotDrop = false,
       valueKnown = true;
};
struct Stack {
  uint64_t id = 0;
  uint32_t formId = 0;
  int32_t count = 0;
  float condition =
      1; // normalized instance condition, never merged across conditions
  bool equipped = false;
};
struct Rules {
  float healthEnduranceMult = 0, healthEnduranceOffset = 0, healthLevelMult = 0;
  float apBase = 0, apMult = 0, carryBase = 0, carryMult = 0;
};
struct Reference {
  uint32_t base = 0, flags = 0, owner = 0, key = 0, cell = 0;
  int32_t count = 1;
  float condition = 1;
  bool locked = false, valid = true;
};
struct LootEntry {
  uint32_t form = 0, owner = 0;
  int32_t count = 1;
  uint16_t level = 1;
  float condition = 1;
  bool extra = false;
};
struct Container {
  std::string name;
  uint32_t script = 0;
  bool valid = true, respawns = false;
  std::vector<LootEntry> entries;
};
struct LootList {
  uint8_t flags = 0, chanceNone = 0;
  uint32_t global = 0;
  bool valid = true;
  std::vector<LootEntry> entries;
};
struct State {
  std::array<uint8_t, 7> special{};
  std::array<uint8_t, 14> skills{}, skillOffsets{};
  uint16_t level = 1;
  int32_t baseHealth = 0;
  float karma = 0, healthDamage = 0, apSpent = 0;
  uint64_t nextStackId = 1;
  std::vector<Stack> inventory;
  std::unordered_set<uint32_t> collected;
  std::unordered_map<uint32_t, std::vector<Stack>> containers;
};
struct Catalog {
  std::unordered_map<uint32_t, Item> items;
  std::unordered_map<uint32_t, Reference> references;
  std::unordered_set<uint32_t> scriptedBases;
  std::unordered_map<std::string, std::string> strings;
  std::unordered_map<uint32_t, Container> containers;
  std::unordered_map<uint32_t, LootList> lootLists;
  std::unordered_map<uint32_t, float> globals;
  int32_t lootLevelDifference = 0;
  Rules rules;
  State initial;
  uint32_t fingerprint = 0;
  uint32_t worldFingerprint = 0;
  uint32_t lootFingerprint = 0;
};
// All-or-nothing, single-master Fallout3.esm loader. No plugins, scripts or
// save import.
bool LoadCatalog(const std::string &esmPath, Catalog &out, std::string &error);

class Player {
public:
  explicit Player(Catalog catalog);
  const Catalog &Definitions() const { return catalog_; }
  const State &Snapshot() const { return state_; }
  float MaxHealth() const;
  float Health() const;
  float MaxActionPoints() const;
  float ActionPoints() const;
  float CarryCapacity() const;
  double InventoryWeight() const;
  bool Overencumbered() const { return InventoryWeight() > CarryCapacity(); }
  uint64_t Revision() const { return revision_; }
  // Invalid requests are atomic no-ops. Removal respects quest/cannot-drop
  // flags.
  bool Add(uint32_t formId, int32_t count, float condition = 1);
  bool Remove(uint64_t stackId, int32_t count);
  bool Equip(uint64_t stackId);
  bool Unequip(uint64_t stackId);
  bool CanPickup(uint32_t reference) const;
  bool Pickup(uint32_t reference);
  bool CanOpenDoor(uint32_t reference) const;
  bool CanLootContainer(uint32_t reference) const;
  bool PrepareContainer(uint32_t reference);
  const std::vector<Stack> *ContainerContents(uint32_t reference) const;
  bool CanTakeContainerStack(uint32_t reference, uint64_t stack) const;
  bool TakeContainerStack(uint32_t reference, uint64_t stack);
  bool IsCollected(uint32_t reference) const {
    return state_.collected.count(reference) != 0;
  }
  bool DamageHealth(float amount);
  bool RestoreHealth(float amount);
  bool SpendActionPoints(float amount);
  bool RestoreActionPoints(float amount);
  // Versioned Quest state, NOT a Bethesda .fos. Restore never partially changes
  // state.
  bool Save(const std::string &path, std::string &error) const;
  bool Restore(const std::string &path, std::string &error);

private:
  Catalog catalog_;
  State state_;
  uint64_t revision_ = 0;
};

// Native integration owns one session for the application, across CELL/GL
// lifetimes. Access and mutations belong to the render thread; initial loading
// belongs to its worker.
struct Session {
  explicit Session(Catalog catalog) : player(std::move(catalog)) {}
  Player player;
  std::string savePath;
  bool saveBlocked = false;
  uint64_t savedRevision = UINT64_MAX;
  bool Flush(std::string &error);
};
} // namespace fo3player

// Null until the first scene publishes. No mutable global state exposed to
// workers.
fo3player::Session *GetFo3PlayerSession();
