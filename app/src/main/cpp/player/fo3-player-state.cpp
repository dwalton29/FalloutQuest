#include "fo3-player-state.h"
#include "data/fo3-esm-reader.h"
#include "npc/fo3-package-schedule.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <unistd.h>
#include <unordered_set>
#include <zlib.h>

namespace fo3player {
namespace {
constexpr size_t MaxStacks = 10000;
constexpr size_t MaxSaveBytes = 4 * 1024 * 1024;
using Bytes = std::vector<uint8_t>;
struct CloseFile {
  void operator()(FILE *f) const { std::fclose(f); }
};
struct Sub {
  std::string type;
  const uint8_t *data;
  uint32_t size;
};
bool Subs(const Bytes &bytes, std::vector<Sub> &out) {
  size_t at = 0;
  uint32_t extended = 0;
  while (at < bytes.size()) {
    if (bytes.size() - at < 6)
      return false;
    const auto type = fo3esm::FourCC(bytes.data() + at);
    uint32_t n = fo3esm::ReadU16(bytes.data() + at + 4);
    at += 6;
    if (type == "XXXX") {
      if (extended || n != 4 || bytes.size() - at < 4)
        return false;
      extended = fo3esm::ReadU32(bytes.data() + at);
      if (!extended)
        return false;
      at += 4;
      continue;
    }
    if (extended) {
      n = extended;
      extended = 0;
    }
    if (n > bytes.size() - at)
      return false;
    out.push_back({type, bytes.data() + at, n});
    at += n;
  }
  return !extended;
}
int32_t I32(const uint8_t *p) {
  const auto u = fo3esm::ReadU32(p);
  int32_t result;
  std::memcpy(&result, &u, 4);
  return result;
}
const Sub *Find(const std::vector<Sub> &subs, const char *type) {
  for (const auto &s : subs)
    if (s.type == type)
      return &s;
  return nullptr;
}
std::string Text(const std::vector<Sub> &subs, const char *type) {
  const auto *s = Find(subs, type);
  return s ? fo3esm::ZString(s->data, s->size) : std::string{};
}
bool Kind(const std::string &t, ItemKind &out) {
  static const std::pair<const char *, ItemKind> types[] = {
      {"WEAP", ItemKind::Weapon},     {"ARMO", ItemKind::Armour},
      {"AMMO", ItemKind::Ammo},       {"ALCH", ItemKind::Aid},
      {"INGR", ItemKind::Ingredient}, {"MISC", ItemKind::Misc},
      {"KEYM", ItemKind::Key},        {"BOOK", ItemKind::Book},
      {"NOTE", ItemKind::Note}};
  for (const auto &entry : types)
    if (t == entry.first) {
      out = entry.second;
      return true;
    }
  return false;
}
bool DecodeItem(uint32_t form, uint32_t flags, ItemKind kind,
                const std::vector<Sub> &subs, Item &item) {
  item.formId = form;
  item.recordFlags = flags;
  item.kind = kind;
  item.questItem = (flags & (1u << 10)) != 0;
  item.editorId = Text(subs, "EDID");
  item.name = Text(subs, "FULL");
  item.model = Text(subs, "MODL");
  item.femaleModel = Text(subs, "MOD3");
  item.icon = Text(subs, "ICON");
  item.femaleIcon = Text(subs, "ICO2");
  for (const auto &pair :
       {std::pair<const char *, uint32_t *>{"SCRI", &item.script},
        {"EITM", &item.enchantment}}) {
    if (const auto *s = Find(subs, pair.first)) {
      if (s->size != 4)
        return false;
      *pair.second = fo3esm::ReadU32(s->data);
    }
  }
  const auto *d = Find(subs, "DATA");
  if (kind != ItemKind::Note && !d)
    return false;
  switch (kind) {
  case ItemKind::Weapon:
  case ItemKind::Armour:
    if (d->size < (kind == ItemKind::Weapon ? 15u : 12u))
      return false;
    item.value = I32(d->data);
    item.maxCondition = I32(d->data + 4);
    item.weight = fo3esm::ReadF32(d->data + 8);
    if (item.maxCondition < 0)
      return false;
    if (kind == ItemKind::Weapon) {
      const auto *dn = Find(subs, "DNAM");
      if (!dn || dn->size < 13)
        return false;
      item.playable = !(dn->data[12] & 0x80);
      item.cannotDrop = (dn->data[12] & 8) != 0;
    } else {
      const auto *bm = Find(subs, "BMDT");
      if (!bm || bm->size < 5)
        return false;
      item.bipedMask = fo3esm::ReadU32(bm->data);
      item.playable = !(bm->data[4] & 0x40);
    }
    break;
  case ItemKind::Ammo:
    if (d->size < 13)
      return false;
    item.value = I32(d->data + 8);
    item.playable = !(d->data[4] & 2);
    // FO3 AMMO has no weight field (no New Vegas hardcore weight).
    break;
  case ItemKind::Aid:
  case ItemKind::Ingredient: {
    if (d->size != 4)
      return false;
    item.weight = fo3esm::ReadF32(d->data);
    const auto *en = Find(subs, "ENIT");
    if (!en || en->size < 8)
      return false;
    item.value = I32(en->data);
    item.valueKnown = (en->data[4] & 1) != 0;
    // Auto-calculated effect values require the magic-effect runtime.
    break;
  }
  case ItemKind::Book:
    if (d->size != 10)
      return false;
    item.value = I32(d->data + 2);
    item.weight = fo3esm::ReadF32(d->data + 6);
    item.playable = !(d->data[0] & 2);
    break;
  case ItemKind::Misc:
  case ItemKind::Key:
    if (d->size != 8)
      return false;
    item.value = I32(d->data);
    item.weight = fo3esm::ReadF32(d->data + 4);
    break;
  case ItemKind::Note:
    break; // NOTE has neither authored weight nor value.
  }
  return std::isfinite(item.weight) && item.weight >= 0;
}
bool DecodeLoot(const std::vector<Sub> &subs, bool levelled,
                std::vector<LootEntry> &out) {
  bool extraAllowed = false;
  for (const auto &sub : subs) {
    if (sub.type == (levelled ? "LVLO" : "CNTO")) {
      LootEntry e;
      if (levelled) {
        if (sub.size != 8 && sub.size != 12)
          return false;
        e.level = fo3esm::ReadU16(sub.data);
        e.form = fo3esm::ReadU32(sub.data + 4);
        e.count = sub.size == 12 ? fo3esm::ReadU16(sub.data + 8) : 1;
      } else {
        if (sub.size != 8)
          return false;
        e.form = fo3esm::ReadU32(sub.data);
        e.count = I32(sub.data + 4);
      }
      if (!e.form || e.count <= 0 || out.size() >= 10000)
        return false;
      out.push_back(e);
      extraAllowed = true;
    } else if (sub.type == "COED") {
      if (!extraAllowed || sub.size != 12)
        return false;
      auto &e = out.back();
      e.owner = fo3esm::ReadU32(sub.data);
      e.extra = true;
      // Rank/global ownership is not evaluated by this milestone.
      if (fo3esm::ReadU32(sub.data + 4))
        return false;
      e.condition = fo3esm::ReadF32(sub.data + 8);
      if (!std::isfinite(e.condition) || e.condition < 0 || e.condition > 1)
        return false;
      extraAllowed = false;
    } else
      extraAllowed = false;
  }
  return true;
}
void Put32(Bytes &b, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    b.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void Put64(Bytes &b, uint64_t v) {
  Put32(b, static_cast<uint32_t>(v));
  Put32(b, static_cast<uint32_t>(v >> 32));
}
void PutFloat(Bytes &b, float v) {
  uint32_t u;
  std::memcpy(&u, &v, 4);
  Put32(b, u);
}
uint64_t U64(const uint8_t *p) {
  return fo3esm::ReadU32(p) | (uint64_t{fo3esm::ReadU32(p + 4)} << 32);
}
uint32_t Crc(const Bytes &bytes) {
  return static_cast<uint32_t>(
      crc32(0, bytes.data(), static_cast<uInt>(bytes.size())));
}
bool Amount(float n) { return std::isfinite(n) && n > 0; }
bool Conflicts(const Item &a, const Item &b) {
  return (a.kind == ItemKind::Weapon && b.kind == ItemKind::Weapon) ||
         (a.kind == ItemKind::Armour && b.kind == ItemKind::Armour &&
          (a.bipedMask & b.bipedMask));
}
bool CanEquip(const Item &item) {
  return item.playable &&
         (item.kind == ItemKind::Weapon ||
          (item.kind == ItemKind::Armour && item.bipedMask != 0));
}
} // namespace

bool LoadCatalog(const std::string &path, Catalog &out, std::string &error) {
  error.clear();
  auto fail = [&](const char *why) {
    error = why;
    return false;
  };
  std::unique_ptr<FILE, CloseFile> file(std::fopen(path.c_str(), "rb"));
  if (!file)
    return fail("Fallout3.esm unavailable");
  const auto fileSize = fo3esm::FileSize(file.get());
  if (fileSize < 24)
    return fail("Truncated ESM");
  Catalog next;
  std::vector<uint64_t> groups;
  std::vector<uint32_t> groupCells,groupWorlds,groupTopics;
  std::unordered_map<uint32_t, uint32_t> cellOwners;
  std::unordered_map<std::string, float> settings;
  struct StartingItem {
    uint32_t form;
    int32_t count;
    float condition;
  };
  std::vector<StartingItem> starting;
  bool playerFound = false, tes4Found = false;
  uint32_t fingerprint = 0;
  uint64_t at = 0;
  while (at < static_cast<uint64_t>(fileSize)) {
    while (!groups.empty() && at == groups.back()) {
      groups.pop_back();
      groupCells.pop_back();groupWorlds.pop_back();groupTopics.pop_back();
    }
    const uint64_t boundary =
        groups.empty() ? static_cast<uint64_t>(fileSize) : groups.back();
    if (at > boundary || boundary - at < 24)
      return fail("Invalid ESM record boundary");
    if (fseeko(file.get(), static_cast<off_t>(at), SEEK_SET))
      return fail("ESM seek failed");
    uint8_t h[24];
    if (!fo3esm::ReadExact(file.get(), h, sizeof(h)))
      return fail("Truncated ESM header");
    const auto type = fo3esm::FourCC(h);
    const auto size = fo3esm::ReadU32(h + 4);
    if (type == "GRUP") {
      if (size < 24 || size > boundary - at)
        return fail("Invalid ESM group size");
      groups.push_back(at + size);
      const auto groupType = fo3esm::ReadU32(h + 12);
      const bool cellGroup =
          groupType == 6 || groupType == 8 || groupType == 9 || groupType == 10;
      groupCells.push_back(cellGroup
                               ? fo3esm::ReadU32(h + 8)
                               : (groupCells.empty() ? 0 : groupCells.back()));
      groupWorlds.push_back(groupType==1?fo3esm::ReadU32(h+8):(groupWorlds.empty()?0:groupWorlds.back()));
      groupTopics.push_back(groupType==7?fo3esm::ReadU32(h+8):(groupTopics.empty()?0:groupTopics.back()));
      at += 24;
      continue;
    }
    if (size > boundary - at - 24)
      return fail("Invalid ESM payload size");
    const auto flags = fo3esm::ReadU32(h + 8), form = fo3esm::ReadU32(h + 12);
    ItemKind kind{};
    const bool item = Kind(type, kind);
    const bool worldRecord = type == "REFR" || type == "DOOR" || type == "CELL";
    const bool lootRecord = type == "CONT" || type == "LVLI" || type == "GLOB";
    const bool extra = fo3pipdata::Relevant(type)||type=="ACHR"||type=="ACRE"||fo3weapon::Relevant(type);
    const bool selected = extra || lootRecord || worldRecord || type == "TES4" ||
                          type == "GMST" || item ||
                          (type == "NPC_" && form == PlayerBase);
    if (at == 0 && type != "TES4")
      return fail("Missing TES4 file header");
    if (selected && !(flags & 0x20)) {
      Bytes payload;
      if (!fo3esm::ReadPayload(file.get(), {at + 24, size, flags, type},
                               payload))
        return fail("Cannot decode ESM record");
      std::vector<Sub> subs;
      if (!Subs(payload, subs))
        return fail("Malformed ESM subrecord");
      fo3pipdata::Decode(next.pipboy,type,form,flags,payload,groupCells.empty()?0:groupCells.back(),groupWorlds.empty()?0:groupWorlds.back(),groupTopics.empty()?0:groupTopics.back());
      fo3weapon::Decode(next.weapons,type,form,payload);
      if(type=="NPC_") {
        Container inventory;inventory.name=Text(subs,"FULL");
        inventory.valid=DecodeLoot(subs,false,inventory.entries);
        next.actorInventories[form]=std::move(inventory);
      }
      if(type=="ACHR") {
        Reference ref;ref.flags=flags;ref.cell=groupCells.empty()?0:groupCells.back();
        const auto* base=Find(subs,"NAME");if(base&&base->size==4)ref.base=fo3esm::ReadU32(base->data);
        ref.valid=ref.base!=0;next.references[form]=ref;
        // Preserve the decoded authored placement before Pip-Boy's target
        // finalizer retains only quest/navigation references.
        const auto original=next.pipboy.targets.find(form);
        const auto* authoredPose=Find(subs,"DATA");
        if(ref.valid&&authoredPose&&authoredPose->size==24u&&
           original!=next.pipboy.targets.end())
          next.actorPlacements.emplace(form,original->second);
      }
      // Keep the v1 catalog identity stable so existing player saves migrate.
      if (lootRecord) {
        next.lootFingerprint =
            static_cast<uint32_t>(crc32(next.lootFingerprint, h, sizeof(h)));
        next.lootFingerprint =
            static_cast<uint32_t>(crc32(next.lootFingerprint, payload.data(),
                                        static_cast<uInt>(payload.size())));
      } else if (!worldRecord && (!extra || (type=="NPC_"&&form==PlayerBase))) {
        fingerprint = static_cast<uint32_t>(crc32(fingerprint, h, sizeof(h)));
        fingerprint = static_cast<uint32_t>(crc32(
            fingerprint, payload.data(), static_cast<uInt>(payload.size())));
      } else if (worldRecord) {
        next.worldFingerprint =
            static_cast<uint32_t>(crc32(next.worldFingerprint, h, sizeof(h)));
        next.worldFingerprint =
            static_cast<uint32_t>(crc32(next.worldFingerprint, payload.data(),
                                        static_cast<uInt>(payload.size())));
      }
      if (extra && !(type=="NPC_"&&form==PlayerBase)) {
        // Immutable Pip-Boy-only record; do not alter legacy fingerprints.
      } else if (type == "CONT") {
        Container c;
        c.name = Text(subs, "FULL");
        if (const auto *script = Find(subs, "SCRI")) {
          if (script->size != 4)
            c.valid = false;
          else
            c.script = fo3esm::ReadU32(script->data);
        }
        const auto *data = Find(subs, "DATA");
        if (!data || data->size != 5)
          c.valid = false;
        else
          c.respawns = (data->data[0] & 2) != 0;
        c.valid = DecodeLoot(subs, false, c.entries) && c.valid;
        next.containers[form] = std::move(c);
      } else if (type == "LVLI") {
        LootList list;
        const auto *chance = Find(subs, "LVLD"),
                   *listFlags = Find(subs, "LVLF");
        if (!chance || chance->size != 1 || !listFlags || listFlags->size != 1)
          list.valid = false;
        else {
          list.chanceNone = chance->data[0];
          list.flags = listFlags->data[0];
        }
        if (const auto *global = Find(subs, "LVLG")) {
          if (global->size != 4)
            list.valid = false;
          else
            list.global = fo3esm::ReadU32(global->data);
        }
        list.valid = DecodeLoot(subs, true, list.entries) && list.valid &&
                     list.chanceNone <= 100 && !(list.flags & ~7u);
        next.lootLists[form] = std::move(list);
      } else if (type == "GLOB") {
        const auto *value = Find(subs, "FLTV");
        if (value && value->size == 4 &&
            std::isfinite(fo3esm::ReadF32(value->data))) {
          const float globalValue = fo3esm::ReadF32(value->data);
          next.globals[form] = globalValue;
          const std::string editor = Text(subs, "EDID");
          // Original named engine globals, not guessed package schedules.
          if (editor == "TimeScale" && globalValue > 0.0f && globalValue <= 1000.0f)
            next.gameTimeScale = globalValue;
          if (editor == "GameHour" && globalValue >= 0.0f && globalValue < 24.0f)
            next.initial.gameHour = globalValue;
          if (editor == "GameDaysPassed" && globalValue >= 0.0f &&
              globalValue < 1000000.0f)
            next.initial.gameDaysPassed = static_cast<uint32_t>(globalValue);
          if (editor == "GameYear" && globalValue>=1&&globalValue<=9999)
            next.initial.gameYear = static_cast<uint32_t>(globalValue);
          if (editor == "GameMonth" && globalValue>=0&&globalValue<12)
            next.initial.gameMonth = static_cast<uint32_t>(globalValue);
          if (editor == "GameDay" && globalValue>=1&&globalValue<=31)
            next.initial.gameDay = static_cast<uint32_t>(globalValue);
        }
      } else if (type == "REFR") {
        Reference ref;
        ref.flags = flags;
        ref.cell = groupCells.empty() ? 0 : groupCells.back();
        const auto *base = Find(subs, "NAME");
        if (!base || base->size != 4)
          return fail("Invalid reference base");
        ref.base = fo3esm::ReadU32(base->data);
        for (const auto &sub : subs) {
          if (sub.type == "XCNT") {
            if (sub.size != 4)
              ref.valid = false;
            else
              ref.count = I32(sub.data);
          } else if (sub.type == "XHLP") {
            if (sub.size != 4)
              ref.valid = false;
            else
              ref.condition = fo3esm::ReadF32(sub.data);
          } else if (sub.type == "XOWN") {
            if (sub.size != 4)
              ref.valid = false;
            else
              ref.owner = fo3esm::ReadU32(sub.data);
          } else if (sub.type == "XLOC") {
            if (sub.size < 12)
              ref.valid = false;
            else {
              ref.locked =
                  true; // XLOC authors a lock, including key-only locks.
              ref.key = fo3esm::ReadU32(sub.data + 4);
            }
          }
        }
        ref.valid = ref.valid && ref.count > 0 &&
                    std::isfinite(ref.condition) && ref.condition >= 0 &&
                    ref.condition <= 1;
        next.references[form] = ref;
      } else if (type == "CELL") {
        const auto *owner = Find(subs, "XOWN");
        if (owner && owner->size == 4)
          cellOwners[form] = fo3esm::ReadU32(owner->data);
      } else if (type == "DOOR") {
        const auto* script = Find(subs, "SCRI");
        if (script) {
          next.scriptedBases.insert(form);
          // Original MegBrassLanternFrontDoorSCRIPT calls activate in every
          // branch; its time test only toggles the two customer references.
          if (script->size == 4 && fo3esm::ReadU32(script->data) == 0x00041719u &&
              Text(subs, "EDID") == "MegBrassLanternFrontDoor")
            next.defaultActivationDoors.insert(form);
        }
      } else if (type == "TES4") {
        if (tes4Found || Find(subs, "MAST"))
          return fail("Only standalone Fallout3.esm is supported");
        tes4Found = true;
      } else if (type == "GMST") {
        const auto id = Text(subs, "EDID");
        const auto *d = Find(subs, "DATA");
        if (!id.empty() && id[0] == 'f' && d && d->size == 4)
          settings[id] = fo3esm::ReadF32(d->data);
        if (!id.empty() && id[0] == 's' && d)
          next.strings[id] = fo3esm::ZString(d->data, d->size);
        if (id == "iLevItemLevelDifferenceMax" && d && d->size == 4)
          next.lootLevelDifference = std::max(0, I32(d->data));
      } else if (item) {
        Item definition;
        if (!form || !DecodeItem(form, flags, kind, subs, definition))
          return fail("Invalid inventory item definition");
        if (kind == ItemKind::Weapon)
          fo3weapon::DecodeWeapon(payload, definition.weapon);
        next.items[form] = std::move(definition);
      } else {
        if (playerFound)
          return fail("Duplicate player record");
        playerFound = true;
        const auto *ac = Find(subs, "ACBS");
        const auto *d = Find(subs, "DATA");
        const auto *dn = Find(subs, "DNAM");
        if (!ac || ac->size != 24 || !d || d->size < 11 || !dn ||
            dn->size != 28)
          return fail("Missing player stats");
        if ((fo3esm::ReadU32(ac->data) & (0x10 | 0x80)) ||
            (fo3esm::ReadU16(ac->data + 22) & (2 | 256)))
          return fail("Auto-calculated or inherited player stats/inventory "
                      "unsupported");
        next.initial.level = fo3esm::ReadU16(ac->data + 8);
        next.initial.karma = fo3esm::ReadF32(ac->data + 16);
        next.initial.baseHealth = I32(d->data);
        std::copy_n(d->data + 4, 7, next.initial.special.begin());
        std::copy_n(dn->data, 14, next.initial.skills.begin());
        std::copy_n(dn->data + 14, 14, next.initial.skillOffsets.begin());
        bool inventoryExtraAllowed = false;
        for (const auto &sub : subs) {
          if (sub.type == "CNTO") {
            if (sub.size != 8)
              return fail("Invalid player inventory");
            starting.push_back(
                {fo3esm::ReadU32(sub.data), I32(sub.data + 4), 1});
            inventoryExtraAllowed = true;
          } else if (sub.type == "COED") {
            if (!inventoryExtraAllowed || sub.size != 12)
              return fail("Invalid player inventory extra data");
            // Ownership/rank evaluation requires the faction/quest runtime.
            if (fo3esm::ReadU32(sub.data) || fo3esm::ReadU32(sub.data + 4))
              return fail("Owned starting player inventory unsupported");
            starting.back().condition = fo3esm::ReadF32(sub.data + 8);
            inventoryExtraAllowed = false;
          } else {
            inventoryExtraAllowed = false;
          }
        }
      }
    }
    at += 24ull + size;
  }
  if (!tes4Found || !playerFound || !next.initial.level ||
      next.initial.baseHealth <= 0 || !std::isfinite(next.initial.karma))
    return fail("Invalid player baseline");
  for (auto v : next.initial.special)
    if (v < 1 || v > 10)
      return fail("Invalid player SPECIAL");
  const std::pair<const char *, float *> required[] = {
      {"fAVDHealthEnduranceMult", &next.rules.healthEnduranceMult},
      {"fAVDHealthEnduranceOffset", &next.rules.healthEnduranceOffset},
      {"fAVDHealthLevelMult", &next.rules.healthLevelMult},
      {"fAVDActionPointsBase", &next.rules.apBase},
      {"fAVDActionPointsMult", &next.rules.apMult},
      {"fAVDCarryWeightsBase", &next.rules.carryBase},
      {"fAVDCarryWeightMult", &next.rules.carryMult}};
  for (const auto &entry : required) {
    const auto it = settings.find(entry.first);
    if (it == settings.end() || !std::isfinite(it->second))
      return fail("Missing/nonfinite player game setting");
    *entry.second = it->second;
  }
  fo3weapon::FinalizeStatistics(next.weapons);
  fo3pipdata::FinalizeLevelledCategories(next.pipboy,next.weapons);
  fo3pipdata::Finalize(next.pipboy);
  // All eligible original ACHR identities must remain resolvable by actor
  // health, hostility, persistence and the unloaded scheduler. Keep their
  // full authored pose even if no quest, patrol or door currently points here.
  for(const auto& entry:next.actorPlacements){
    const auto& placement=entry.second;
    if((placement.flags&0x820u)||placement.parent||
       !next.weapons.actors.count(placement.base)||
       !fo3weapon::ActorStatistics(next.weapons,placement.base))continue;
    next.pipboy.targets.emplace(entry.first,placement);
  }
  next.fingerprint = fingerprint;
  for (auto &entry : next.references)
    if (!entry.second.owner && cellOwners.count(entry.second.cell))
      entry.second.owner = cellOwners.at(entry.second.cell);
  Player initial(std::move(next));
  if (!std::isfinite(initial.MaxHealth()) || initial.MaxHealth() <= 0 ||
      !std::isfinite(initial.MaxActionPoints()) ||
      initial.MaxActionPoints() < 0 ||
      !std::isfinite(initial.CarryCapacity()) || initial.CarryCapacity() < 0)
    return fail("Invalid derived player stats");
  for (const auto &entry : starting) {
    if (entry.count <= 0 ||
        !initial.Add(entry.form, entry.count, entry.condition))
      return fail(
          "Unresolved/unsupported player inventory (including levelled lists)");
  }
  next = initial.Definitions();
  next.initial = initial.Snapshot();
  out = std::move(next);
  return true;
}

Player::Player(Catalog catalog)
    : catalog_(std::move(catalog)), state_(catalog_.initial) {}
bool Player::AdvanceGameClock(double realSeconds) {
  if (!std::isfinite(realSeconds) || realSeconds <= 0.0 || realSeconds > 1.0 ||
      !std::isfinite(catalog_.gameTimeScale) || catalog_.gameTimeScale <= 0.0f)
    return false;
  const double total = double(state_.gameHour) +
      realSeconds * double(catalog_.gameTimeScale) / 3600.0;
  if (!std::isfinite(total)) return false;
  const uint32_t days = static_cast<uint32_t>(std::floor(total / 24.0));
  if (days > UINT32_MAX - state_.gameDaysPassed) return false;
  const float nextHour = static_cast<float>(total - double(days) * 24.0);
  if (!std::isfinite(nextHour) || nextHour < 0.0f || nextHour >= 24.0f)
    return false;
  const int beforeMinute = int(state_.gameHour * 60.0f);
  const int afterMinute = int(nextHour * 60.0f);
  if (beforeMinute != afterMinute || days) ++revision_;
  state_.gameHour = nextHour;
  state_.gameDaysPassed += days;
  for(uint32_t i=0;i<days;++i) {
    if(++state_.gameDay>uint32_t(fo3schedule::Days(int(state_.gameYear),int(state_.gameMonth)))) {
      state_.gameDay=1;
      if(++state_.gameMonth>=12){state_.gameMonth=0;++state_.gameYear;}
    }
  }
  return true;
}
ActorCensusReport Player::RegisterOriginalActors() {
  ActorCensusReport report;
  report.authored=catalog_.actorPlacements.size();
  // Original FormID order makes seeding deterministic independent of hash
  // iteration order, device/compiler, or scene-entry timing.
  std::vector<uint32_t> refs;refs.reserve(report.authored);
  for(const auto& entry:catalog_.actorPlacements)refs.push_back(entry.first);
  std::sort(refs.begin(),refs.end());
  for(uint32_t id:refs){
    if(state_.actors.count(id)){++report.alreadyTracked;continue;}
    const auto& p=catalog_.actorPlacements.at(id);
    // Deleted/initially-disabled and XESP-controlled actors must not be
    // spontaneously activated before their enabling quest/script fires.
    if((p.flags&0x820u)||p.parent){++report.disabledOrConditional;continue;}
    if(!p.base||!catalog_.weapons.actors.count(p.base)||
       !fo3weapon::ActorStatistics(catalog_.weapons,p.base)||
       !catalog_.pipboy.targets.count(id)||
       ActorHealth(id)<=0){++report.unsupportedBase;continue;}
    if((!p.cell&&!p.world)||!std::isfinite(p.x)||!std::isfinite(p.y)||
       !std::isfinite(p.z)||!std::isfinite(p.rx)||!std::isfinite(p.ry)||
       !std::isfinite(p.rz)||!std::isfinite(p.scale)||
       p.scale<=0){++report.unsupportedPlacement;continue;}
    if(state_.actors.size()>=10000u){++report.limitReached;continue;}
    ActorState a;
    a.cell=p.cell;a.world=p.world;
    a.position={p.x,p.y,p.z};a.yaw=p.rz;
    if(UpdateActor(id,a))++report.registered;
    else ++report.unsupportedBase;
  }
  return report;
}
float Player::MaxHealth() const {
  const auto &r = catalog_.rules;
  return state_.baseHealth +
         (state_.special[2] + r.healthEnduranceOffset) * r.healthEnduranceMult +
         (state_.level - 1) * r.healthLevelMult;
}
float Player::Health() const {
  return std::max(0.0f, MaxHealth() - state_.healthDamage);
}
float Player::MaxActionPoints() const {
  return catalog_.rules.apBase + state_.special[5] * catalog_.rules.apMult;
}
float Player::ActionPoints() const {
  return std::max(0.0f, MaxActionPoints() - state_.apSpent);
}
float Player::CarryCapacity() const {
  return catalog_.rules.carryBase +
         state_.special[0] * catalog_.rules.carryMult;
}
double Player::InventoryWeight() const {
  double result = 0;
  for (const auto &s : state_.inventory)
    result += static_cast<double>(catalog_.items.at(s.formId).weight) * s.count;
  return result;
}
bool Player::Add(uint32_t form, int32_t count, float condition) {
  if (count <= 0 || !std::isfinite(condition) || condition < 0 ||
      condition > 1 || !catalog_.items.count(form))
    return false;
  const auto &item = catalog_.items.at(form);
  if (item.maxCondition == 0 && condition != 1)
    return false;
  if (item.kind == ItemKind::Weapon) {
    if (static_cast<size_t>(count) > MaxStacks - state_.inventory.size() ||
        static_cast<uint64_t>(count) > UINT64_MAX - state_.nextStackId)
      return false;
    state_.inventory.reserve(state_.inventory.size() + count);
    for (int32_t i = 0; i < count; ++i)
      state_.inventory.push_back({state_.nextStackId++, form, 1, condition, false});
    ++revision_;
    return true;
  }
  for (auto &s : state_.inventory)
    if (s.formId == form && s.condition == condition && !s.equipped) {
      if (count > INT32_MAX - s.count)
        return false;
      s.count += count;
      ++revision_;
      return true;
    }
  if (state_.inventory.size() >= MaxStacks || state_.nextStackId == UINT64_MAX)
    return false;
  state_.inventory.push_back(
      {state_.nextStackId++, form, count, condition, false});
  ++revision_;
  return true;
}
bool Player::Remove(uint64_t id, int32_t count) {
  auto &v = state_.inventory;
  const auto it = std::find_if(v.begin(), v.end(),
                               [&](const Stack &s) { return s.id == id; });
  if (it == v.end() || count <= 0 || count > it->count ||
      (catalog_.items.at(it->formId).questItem ||
       catalog_.items.at(it->formId).cannotDrop))
    return false;
  it->count -= count;
  if (!it->count)
    v.erase(it);
  ++revision_;
  return true;
}
bool Player::Equip(uint64_t id) {
  auto &v = state_.inventory;
  const auto it = std::find_if(v.begin(), v.end(),
                               [&](const Stack &s) { return s.id == id; });
  if (it == v.end() || !CanEquip(catalog_.items.at(it->formId)) ||
      (it->condition <= 0 && catalog_.items.at(it->formId).kind != ItemKind::Weapon))
    return false;
  if (it->equipped)
    return true;
  const Item &item = catalog_.items.at(it->formId);
  const bool split = it->count > 1;
  if (split && (v.size() >= MaxStacks || state_.nextStackId == UINT64_MAX))
    return false;
  Stack remainder = *it;
  if (split) {
    remainder.id = state_.nextStackId++;
    --remainder.count;
    it->count = 1;
  }
  for (auto &s : v)
    if (s.equipped && Conflicts(item, catalog_.items.at(s.formId)))
      s.equipped = false;
  it->equipped = true;
  if (split)
    v.push_back(remainder);
  ++revision_;
  return true;
}
bool Player::Unequip(uint64_t id) {
  for (auto &s : state_.inventory)
    if (s.id == id) {
      if (s.equipped) {
        s.equipped = false;
        ++revision_;
      }
      return true;
    }
  return false;
}
bool Player::CanPickup(uint32_t id) const {
  const auto r = catalog_.references.find(id);
  if (r == catalog_.references.end() || IsCollected(id))
    return false;
  const auto &ref = r->second;
  const auto i = catalog_.items.find(ref.base);
  return ref.valid && !(ref.flags & 0x20) &&
         (!ref.owner || ref.owner == PlayerBase) && !ref.locked &&
         i != catalog_.items.end() && i->second.playable && !i->second.script &&
         (i->second.maxCondition > 0 || ref.condition == 1);
}
bool Player::Pickup(uint32_t id) {
  if (!CanPickup(id) || state_.collected.size() >= 100000)
    return false;
  const auto &ref = catalog_.references.at(id);
  // Record before Add (which can allocate); undo on an invalid Add.
  state_.collected.insert(id);
  if (!Add(ref.base, ref.count, ref.condition)) {
    state_.collected.erase(id);
    return false;
  }
  return true;
}
bool Player::CanOpenDoor(uint32_t id) const {return CanActorOpenDoor(PlayerRef,id);}
bool Player::CanActorOpenDoor(uint32_t actor,uint32_t id) const {
  const auto r = catalog_.references.find(id);
  if (r == catalog_.references.end())
    return false;
  const auto &ref = r->second;
  // Ownership controls trespass/crime, not whether an unlocked door opens.
  if (!ref.valid || (ref.flags & 0x20u) ||
      (catalog_.scriptedBases.count(ref.base) &&
       (actor!=PlayerRef||!catalog_.defaultActivationDoors.count(ref.base))))
    return false;
  if(actor!=PlayerRef&&ActorHealth(actor)<=0)return false;
  if (!ref.locked)
    return true;
  if (!ref.key)
    return false;
  const auto* inventory=actor==PlayerRef?&state_.inventory:ContainerContents(actor);
  if(!inventory)return false;
  for (const auto &s : *inventory)
    if (s.formId == ref.key && s.count > 0)
      return true;
  return false;
}
bool Player::DamageHealth(float amount) {
  if (!Amount(amount))
    return false;
  const float value = std::min(MaxHealth(), state_.healthDamage + amount);
  if (value != state_.healthDamage) {
    state_.healthDamage = value;
    ++revision_;
  }
  return true;
}
bool Player::RestoreHealth(float amount) {
  if (!Amount(amount))
    return false;
  const float value = std::max(0.0f, state_.healthDamage - amount);
  if (value != state_.healthDamage) {
    state_.healthDamage = value;
    ++revision_;
  }
  return true;
}
bool Player::SpendActionPoints(float amount) {
  if (!Amount(amount) || amount > ActionPoints())
    return false;
  state_.apSpent += amount;
  ++revision_;
  return true;
}
bool Player::RestoreActionPoints(float amount) {
  if (!Amount(amount))
    return false;
  const float value = std::max(0.0f, state_.apSpent - amount);
  if (value != state_.apSpent) {
    state_.apSpent = value;
    ++revision_;
  }
  return true;
}

bool Player::Save(const std::string &path, std::string &error) const {
  error.clear();
  Bytes payload;
  PutFloat(payload, state_.healthDamage);
  PutFloat(payload, state_.apSpent);
  Put64(payload, state_.nextStackId);
  Put32(payload, static_cast<uint32_t>(state_.inventory.size()));
  for (const auto &s : state_.inventory) {
    Put64(payload, s.id);
    Put32(payload, s.formId);
    Put32(payload, static_cast<uint32_t>(s.count));
    PutFloat(payload, s.condition);
    payload.push_back(s.equipped ? 1 : 0);
  }
  Put32(payload, catalog_.worldFingerprint);
  Put32(payload, static_cast<uint32_t>(state_.collected.size()));
  std::vector<uint32_t> collected(state_.collected.begin(),
                                  state_.collected.end());
  std::sort(collected.begin(), collected.end());
  for (auto id : collected)
    Put32(payload, id);
  Bytes bytes{'F', 'Q', 'P', 'S'};
  Put32(payload, catalog_.lootFingerprint);
  Put32(payload, static_cast<uint32_t>(state_.containers.size()));
  std::vector<uint32_t> containerIds;
  for (const auto &entry : state_.containers)
    containerIds.push_back(entry.first);
  std::sort(containerIds.begin(), containerIds.end());
  for (auto id : containerIds) {
    Put32(payload, id);
    const auto &contents = state_.containers.at(id);
    Put32(payload, static_cast<uint32_t>(contents.size()));
    for (const auto &stack : contents) {
      Put64(payload, stack.id);
      Put32(payload, stack.formId);
      Put32(payload, stack.count);
      PutFloat(payload, stack.condition);
    }
  }
  Bytes pipboy;
  fo3pipdata::EncodeState(state_.pipboy, pipboy);
  Put32(payload, static_cast<uint32_t>(pipboy.size()));
  payload.insert(payload.end(), pipboy.begin(), pipboy.end());
  Put32(payload, 0x57504e35); // WPN5 extension; runtime IDs, not Bethesda forms.
  Put32(payload, state_.developmentWeaponGranted ? 1 : 0);
  std::vector<const Stack *> weapons;
  for (const auto &s : state_.inventory)
    if (catalog_.items.at(s.formId).kind == ItemKind::Weapon) weapons.push_back(&s);
  for (auto id : containerIds)
    for (const auto &s : state_.containers.at(id))
      if (catalog_.items.at(s.formId).kind == ItemKind::Weapon) weapons.push_back(&s);
  Put32(payload, static_cast<uint32_t>(weapons.size()));
  for (const auto *s : weapons) {
    Put64(payload, s->id);
    Put32(payload, s->loadedRounds | (s->needsAction ? 0x10000u : 0));
  }
  Put32(payload, static_cast<uint32_t>(state_.worldWeapons.size()));
  for (const auto &w : state_.worldWeapons) {
    Put64(payload, w.instance.id);
    Put32(payload, w.instance.formId);
    PutFloat(payload, w.instance.condition);
    Put32(payload, w.instance.loadedRounds | (w.instance.needsAction ? 0x10000u : 0));
    Put32(payload, w.pose.cell);
    Put32(payload, w.pose.world);
    for (float f : w.pose.position) PutFloat(payload, f);
    for (float f : w.pose.rotation) PutFloat(payload, f);
    for (float f : w.pose.velocity) PutFloat(payload, f);
    for (float f : w.pose.angularVelocity) PutFloat(payload, f);
  }
  Put32(payload,static_cast<uint32_t>(state_.actorDamage.size()));
  std::vector<uint32_t> damaged;
  for(const auto&e:state_.actorDamage)damaged.push_back(e.first);
  std::sort(damaged.begin(),damaged.end());
  for(auto id:damaged){Put32(payload,id);PutFloat(payload,state_.actorDamage.at(id));}
  Put32(payload,static_cast<uint32_t>(state_.actors.size()));
  std::vector<uint32_t> actorIds;for(const auto& e:state_.actors)actorIds.push_back(e.first);
  std::sort(actorIds.begin(),actorIds.end());
  for(auto id:actorIds){const auto& a=state_.actors.at(id);Put32(payload,id);
    for(auto v:{a.cell,a.world,a.package,a.sequence,a.hostile})Put32(payload,v);
    for(float v:a.position)PutFloat(payload,v);
    PutFloat(payload,a.yaw);Put64(payload,a.equippedWeapon);Put32(payload,a.dead?1:0);PutFloat(payload,a.packageWaitSeconds);
  }
  // v10: appended after v9 actor records. v1-v9 remain readable verbatim.
  PutFloat(payload,state_.gameHour);
  Put32(payload,state_.gameDaysPassed);
  Put32(payload,state_.gameYear);
  Put32(payload,state_.gameMonth);
  Put32(payload,state_.gameDay);
  Put32(bytes, 11);
  Put32(bytes, catalog_.fingerprint);
  Put32(bytes, static_cast<uint32_t>(payload.size()));
  Put32(bytes, Crc(payload));
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  const auto tmp = path + ".tmp";
  const auto slash = path.find_last_of('/');
  const std::string directory =
      slash == std::string::npos ? "."
                                 : (slash == 0 ? "/" : path.substr(0, slash));
  const int directoryFd = open(directory.c_str(), O_RDONLY | O_DIRECTORY);
  if (directoryFd < 0) {
    error = "Cannot open player save directory";
    return false;
  }
  FILE *f = std::fopen(tmp.c_str(), "wb");
  if (!f) {
    close(directoryFd);
    error = "Cannot open temporary player save";
    return false;
  }
  bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
  if (ok)
    ok = std::fflush(f) == 0;
  if (ok)
    ok = fsync(fileno(f)) == 0;
  if (std::fclose(f) != 0)
    ok = false;
  if (ok)
    ok = std::rename(tmp.c_str(), path.c_str()) == 0;
  if (!ok) {
    std::remove(tmp.c_str());
    error = "Cannot commit player save; existing save retained";
  }
  if (ok && fsync(directoryFd) != 0) {
    ok = false;
    error = "Player save committed, but directory sync failed";
  }
  close(directoryFd);
  return ok;
}
bool Player::Restore(const std::string &path, std::string &error) {
  error.clear();
  auto fail = [&](const char *why) {
    error = why;
    return false;
  };
  std::unique_ptr<FILE, CloseFile> f(std::fopen(path.c_str(), "rb"));
  if (!f)
    return fail("Cannot open player save");
  const auto size = fo3esm::FileSize(f.get());
  if (size < 40 || size > static_cast<int64_t>(MaxSaveBytes))
    return fail("Invalid player save size");
  Bytes bytes(static_cast<size_t>(size));
  if (!fo3esm::ReadExact(f.get(), bytes.data(), bytes.size()))
    return fail("Truncated player save");
  const auto *h = bytes.data();
  const auto version = fo3esm::ReadU32(h + 4);
  if (std::memcmp(h, "FQPS", 4) ||
      (version < 1 || version > 11))
    return fail("Unsupported player save format");
  if (fo3esm::ReadU32(h + 8) != catalog_.fingerprint)
    return fail("Player save belongs to different game definitions");
  if (fo3esm::ReadU32(h + 12) != bytes.size() - 20)
    return fail("Invalid player save payload size");
  Bytes payload(bytes.begin() + 20, bytes.end());
  if (fo3esm::ReadU32(h + 16) != Crc(payload))
    return fail("Player save checksum failed");
  const auto *p = payload.data();
  State next = catalog_.initial;
  next.worldWeapons.clear();
  next.actorDamage.clear();
  next.actors.clear();
  next.developmentWeaponGranted = false;
  next.healthDamage = fo3esm::ReadF32(p);
  next.apSpent = fo3esm::ReadF32(p + 4);
  next.nextStackId = U64(p + 8);
  const uint32_t count = fo3esm::ReadU32(p + 16);
  const size_t inventoryEnd = 20 + static_cast<size_t>(count) * 21;
  if (count > MaxStacks || payload.size() < inventoryEnd ||
      (version == 1 && payload.size() != inventoryEnd) || !next.nextStackId)
    return fail("Invalid player save inventory size");
  if (!std::isfinite(next.healthDamage) || next.healthDamage < 0 ||
      next.healthDamage > MaxHealth() || !std::isfinite(next.apSpent) ||
      next.apSpent < 0 || next.apSpent > MaxActionPoints())
    return fail("Invalid saved player resources");
  next.inventory.clear();
  std::unordered_set<uint64_t> ids;
  for (uint32_t i = 0; i < count; ++i) {
    const auto *s = p + 20 + i * 21;
    Stack stack{U64(s), fo3esm::ReadU32(s + 8), I32(s + 12),
                fo3esm::ReadF32(s + 16), s[20] != 0};
    const auto item = catalog_.items.find(stack.formId);
    if (!stack.id || stack.id >= next.nextStackId ||
        !ids.insert(stack.id).second || item == catalog_.items.end() ||
        stack.count <= 0 || !std::isfinite(stack.condition) ||
        stack.condition < 0 || stack.condition > 1 || s[20] > 1 ||
        (item->second.maxCondition == 0 && stack.condition != 1) ||
        (stack.equipped &&
         (stack.count != 1 || !CanEquip(item->second) ||
          (stack.condition == 0 && item->second.kind != ItemKind::Weapon))) ||
        (version >= 5 && item->second.kind == ItemKind::Weapon && stack.count != 1))
      return fail("Invalid saved inventory stack");
    for (const auto &prior : next.inventory)
      if (prior.equipped && stack.equipped &&
          Conflicts(catalog_.items.at(prior.formId), item->second))
        return fail("Conflicting saved equipment");
    next.inventory.push_back(stack);
  }
  next.collected.clear();
  size_t worldEnd = inventoryEnd;
  if (version >= 2) {
    if (payload.size() - inventoryEnd < 8)
      return fail("Missing collected references");
    if (fo3esm::ReadU32(p + inventoryEnd) != catalog_.worldFingerprint)
      return fail("Collected references belong to different world definitions");
    const auto n = fo3esm::ReadU32(p + inventoryEnd + 4);
    worldEnd = inventoryEnd + 8ull + 4ull * n;
    if (n > 100000 || worldEnd > payload.size() ||
        (version == 2 && worldEnd != payload.size()))
      return fail("Invalid collected reference count");
    for (uint32_t i = 0; i < n; ++i) {
      const auto id = fo3esm::ReadU32(p + inventoryEnd + 8 + i * 4);
      if (!id || !catalog_.references.count(id) ||
          !catalog_.items.count(catalog_.references.at(id).base) ||
          !next.collected.insert(id).second)
        return fail("Invalid collected reference");
    }
  }
  next.containers.clear();
  if (version >= 3) {
    if (payload.size() - worldEnd < 8 ||
        fo3esm::ReadU32(p + worldEnd) != catalog_.lootFingerprint)
      return fail("Invalid container definitions");
    const auto number = fo3esm::ReadU32(p + worldEnd + 4);
    if (number > 10000)
      return fail("Too many saved containers");
    size_t at = worldEnd + 8, totalStacks = 0;
    for (uint32_t c = 0; c < number; ++c) {
      if (payload.size() - at < 8)
        return fail("Truncated container header");
      const auto ref = fo3esm::ReadU32(p + at),
                 size = fo3esm::ReadU32(p + at + 4);
      at += 8;
      const auto found = catalog_.references.find(ref);
      if (found == catalog_.references.end() ||
          (!catalog_.containers.count(found->second.base)&&!catalog_.actorInventories.count(found->second.base)) ||
          next.containers.count(ref) || size > 10000 ||
          (totalStacks += size) > 30000 || payload.size() - at < 20ull * size)
        return fail("Invalid saved container");
      std::vector<Stack> contents;
      for (uint32_t i = 0; i < size; ++i, at += 20) {
        Stack stack{U64(p + at), fo3esm::ReadU32(p + at + 8), I32(p + at + 12),
                    fo3esm::ReadF32(p + at + 16), false};
        const auto item = catalog_.items.find(stack.formId);
        if (!stack.id || stack.id >= next.nextStackId ||
            !ids.insert(stack.id).second || item == catalog_.items.end() ||
            stack.count <= 0 || !std::isfinite(stack.condition) ||
            stack.condition < 0 || stack.condition > 1 ||
            (item->second.maxCondition == 0 && stack.condition != 1) ||
            (version >= 5 && item->second.kind == ItemKind::Weapon && stack.count != 1))
          return fail("Invalid container stack");
        contents.push_back(stack);
      }
      next.containers.emplace(ref, std::move(contents));
    }
    if (version == 4 && !fo3pipdata::DecodeState(next.pipboy,catalog_.pipboy,p+at,payload.size()-at,error))return false;
    if (version >= 5) {
      if (payload.size() - at < 4) return fail("Missing Pip-Boy length");
      const uint32_t pipSize = fo3esm::ReadU32(p + at);
      at += 4;
      if (pipSize > payload.size() - at ||
          !fo3pipdata::DecodeState(next.pipboy, catalog_.pipboy, p + at, pipSize, error))
        return fail("Invalid Pip-Boy extension");
      at += pipSize;
      if (payload.size() - at < 12 || fo3esm::ReadU32(p + at) != 0x57504e35 ||
          fo3esm::ReadU32(p + at + 4) > 1)
        return fail("Invalid weapon extension");
      next.developmentWeaponGranted = fo3esm::ReadU32(p + at + 4) != 0;
      const uint32_t number = fo3esm::ReadU32(p + at + 8);
      at += 12;
      std::unordered_map<uint64_t, Stack *> weapons;
      for (auto &s : next.inventory)
        if (catalog_.items.at(s.formId).kind == ItemKind::Weapon) weapons.emplace(s.id, &s);
      for (auto &c : next.containers)
        for (auto &s : c.second)
          if (catalog_.items.at(s.formId).kind == ItemKind::Weapon) weapons.emplace(s.id, &s);
      if (number != weapons.size() || payload.size() - at < 12ull * number)
        return fail("Invalid weapon instance count");
      auto setRounds = [&](Stack &s, uint32_t packed) {
        const auto &def = catalog_.items.at(s.formId).weapon;
        if ((packed & ~0x1ffffu) || (packed & 0xffffu) > def.clip ||
            (!def.Firearm() && packed)) return false;
        s.loadedRounds = packed & 0xffffu;
        s.needsAction = (packed & 0x10000u) != 0;
        return true;
      };
      for (uint32_t i = 0; i < number; ++i, at += 12) {
        const uint64_t id = U64(p + at);
        const auto found = weapons.find(id);
        if (found == weapons.end() || !setRounds(*found->second, fo3esm::ReadU32(p + at + 8)))
          return fail("Invalid saved weapon ammunition");
        weapons.erase(found);
      }
      if (payload.size() - at < 4) return fail("Missing world weapons");
      const uint32_t worldCount = fo3esm::ReadU32(p + at);
      at += 4;
      if (worldCount > 10000 || payload.size() - at < 80ull * worldCount)
        return fail("Invalid world weapon count");
      for (uint32_t i = 0; i < worldCount; ++i) {
        WorldWeapon w;
        w.instance.id = U64(p + at);
        w.instance.formId = fo3esm::ReadU32(p + at + 8);
        w.instance.count = 1;
        w.instance.condition = fo3esm::ReadF32(p + at + 12);
        const auto item = catalog_.items.find(w.instance.formId);
        if (!w.instance.id || w.instance.id >= next.nextStackId ||
            !ids.insert(w.instance.id).second || item == catalog_.items.end() ||
            item->second.kind != ItemKind::Weapon || !item->second.weapon.Firearm() ||
            !item->second.playable || item->second.script || item->second.questItem ||
            item->second.cannotDrop || !std::isfinite(w.instance.condition) ||
            w.instance.condition < 0 || w.instance.condition > 1 ||
            (item->second.maxCondition == 0 && w.instance.condition != 1) ||
            !setRounds(w.instance, fo3esm::ReadU32(p + at + 16)))
          return fail("Invalid saved world weapon");
        w.pose.cell = fo3esm::ReadU32(p + at + 20);
        w.pose.world = fo3esm::ReadU32(p + at + 24);
        size_t field = at + 28;
        for (float &f : w.pose.position) {f = fo3esm::ReadF32(p + field); field += 4;}
        for (float &f : w.pose.rotation) {f = fo3esm::ReadF32(p + field); field += 4;}
        for (float &f : w.pose.velocity) {f = fo3esm::ReadF32(p + field); field += 4;}
        for (float &f : w.pose.angularVelocity) {f = fo3esm::ReadF32(p + field); field += 4;}
        if (!fo3weapon::ValidPose(w.pose)) return fail("Invalid saved world weapon pose");
        next.worldWeapons.push_back(w);
        at += 80;
      }
      if(version>=6){
        if(payload.size()-at<4)return fail("Missing actor damage extension");
        const auto n=fo3esm::ReadU32(p+at);at+=4;
        if(n>100000||payload.size()-at<8ull*n)return fail("Invalid actor damage count");
        for(uint32_t i=0;i<n;++i,at+=8){
          const auto id=fo3esm::ReadU32(p+at);const auto damage=fo3esm::ReadF32(p+at+4);
          const auto actor=catalog_.pipboy.targets.find(id);
          if(actor==catalog_.pipboy.targets.end()||!catalog_.weapons.actors.count(actor->second.base)||
             !std::isfinite(damage)||damage<0||!next.actorDamage.emplace(id,damage).second)return fail("Invalid actor damage");
        }
      }
      if(version>=7){
        if(payload.size()-at<4)return fail("Missing actor state count");
        const auto n=fo3esm::ReadU32(p+at);at+=4;
        const size_t actorBytes=version>=9?56:version>=8?52:48;
        const size_t tailBytes = version >= 11 ? 20u : version >= 10 ? 8u : 0u;
        if(n>10000||payload.size()-at!=actorBytes*n+tailBytes)return fail("Invalid actor state count");
        for(uint32_t i=0;i<n;++i,at+=actorBytes){
          const auto id=fo3esm::ReadU32(p+at);ActorState a;
          a.cell=fo3esm::ReadU32(p+at+4);a.world=fo3esm::ReadU32(p+at+8);
          a.package=fo3esm::ReadU32(p+at+12);a.sequence=fo3esm::ReadU32(p+at+16);a.hostile=fo3esm::ReadU32(p+at+20);
          for(size_t j=0;j<3;++j)a.position[j]=fo3esm::ReadF32(p+at+24+j*4);
          a.yaw=fo3esm::ReadF32(p+at+36);a.equippedWeapon=U64(p+at+40);
          if(version>=8){const auto life=fo3esm::ReadU32(p+at+48);if(life>1)return fail("Invalid actor lifecycle");a.dead=life!=0;if(a.dead&&Essential(id))return fail("Essential actor cannot be a corpse");}
          if(version>=9)a.packageWaitSeconds=fo3esm::ReadF32(p+at+52);
          if(!std::isfinite(a.packageWaitSeconds)||a.packageWaitSeconds<0)return fail("Invalid actor package wait");
          if(a.equippedWeapon){
            auto contents=next.containers.find(id);bool found=false;
            if(contents!=next.containers.end())for(auto& s:contents->second)if(s.id==a.equippedWeapon&&catalog_.items.at(s.formId).kind==ItemKind::Weapon){s.equipped=true;found=true;}
            if(!found)return fail("Invalid actor equipped weapon");
          }
          const auto ref=catalog_.pipboy.targets.find(id);
          if(ref==catalog_.pipboy.targets.end()||!catalog_.weapons.actors.count(ref->second.base)||
             (!a.cell&&!a.world)||!std::isfinite(a.yaw)||
             !std::all_of(a.position.begin(),a.position.end(),[](float v){return std::isfinite(v);})||
             (a.package&&!catalog_.pipboy.packages.count(a.package))||
             (a.hostile&&a.hostile!=0x14&&!catalog_.pipboy.targets.count(a.hostile))||
             !next.actors.emplace(id,a).second)return fail("Invalid actor state");
        }
      }
      if (version >= 10) {
        if (payload.size()-at != 8u) return fail("Missing game clock");
        next.gameHour = fo3esm::ReadF32(p+at);
        next.gameDaysPassed = fo3esm::ReadU32(p+at+4);
        if (!std::isfinite(next.gameHour) || next.gameHour < 0.0f ||
            next.gameHour >= 24.0f || next.gameDaysPassed > 1000000u)
          return fail("Invalid saved game clock");
        at += 8;
      }
      if (version >= 11) {
        if (payload.size()-at != 12u) return fail("Missing game calendar");
        next.gameYear = fo3esm::ReadU32(p+at);
        next.gameMonth = fo3esm::ReadU32(p+at+4);
        next.gameDay = fo3esm::ReadU32(p+at+8);
        if (next.gameYear>9999u ||
            !fo3schedule::Valid({int(next.gameYear),int(next.gameMonth),int(next.gameDay)}))
          return fail("Invalid saved game calendar");
        at += 12;
      }
      if(at!=payload.size())return fail("Trailing weapon save data");
    }
    if (version == 3 && at != payload.size())
      return fail("Trailing container save data");
  }
  if (version < 5 && !MigrateWeaponInstances(next))
    return fail("Legacy weapon inventory exceeds instance limits");
  state_ = std::move(next);
  ++revision_;
  return true;
}
bool Session::Flush(std::string &error) {
  error.clear();
  if (saveBlocked) {
    error = "Existing player save was rejected and is preserved";
    return false;
  }
  if (savedRevision == player.Revision())
    return true;
  if (!player.Save(savePath, error))
    return false;
  savedRevision = player.Revision();
  return true;
}
} // namespace fo3player
