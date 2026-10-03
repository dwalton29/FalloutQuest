#include "fo3-player-state.h"
#include "data/fo3-esm-reader.h"

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
constexpr size_t MaxSaveBytes = 1024 * 1024;
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
    while (!groups.empty() && at == groups.back())
      groups.pop_back();
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
      at += 24;
      continue;
    }
    if (size > boundary - at - 24)
      return fail("Invalid ESM payload size");
    const auto flags = fo3esm::ReadU32(h + 8), form = fo3esm::ReadU32(h + 12);
    ItemKind kind{};
    const bool item = Kind(type, kind);
    const bool selected = type == "TES4" || type == "GMST" || item ||
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
      fingerprint = static_cast<uint32_t>(crc32(fingerprint, h, sizeof(h)));
      fingerprint = static_cast<uint32_t>(crc32(
          fingerprint, payload.data(), static_cast<uInt>(payload.size())));
      if (type == "TES4") {
        if (tes4Found || Find(subs, "MAST"))
          return fail("Only standalone Fallout3.esm is supported");
        tes4Found = true;
      } else if (type == "GMST") {
        const auto id = Text(subs, "EDID");
        const auto *d = Find(subs, "DATA");
        if (!id.empty() && id[0] == 'f' && d && d->size == 4)
          settings[id] = fo3esm::ReadF32(d->data);
      } else if (item) {
        Item definition;
        if (!form || !DecodeItem(form, flags, kind, subs, definition))
          return fail("Invalid inventory item definition");
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
  next.fingerprint = fingerprint;
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
      it->condition <= 0)
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
  Bytes bytes{'F', 'Q', 'P', 'S'};
  Put32(bytes, 1);
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
  if (std::memcmp(h, "FQPS", 4) || fo3esm::ReadU32(h + 4) != 1)
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
  next.healthDamage = fo3esm::ReadF32(p);
  next.apSpent = fo3esm::ReadF32(p + 4);
  next.nextStackId = U64(p + 8);
  const uint32_t count = fo3esm::ReadU32(p + 16);
  if (count > MaxStacks ||
      payload.size() != 20 + static_cast<size_t>(count) * 21 ||
      !next.nextStackId)
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
         (stack.count != 1 || !CanEquip(item->second) || stack.condition == 0)))
      return fail("Invalid saved inventory stack");
    for (const auto &prior : next.inventory)
      if (prior.equipped && stack.equipped &&
          Conflicts(catalog_.items.at(prior.formId), item->second))
        return fail("Conflicting saved equipment");
    next.inventory.push_back(stack);
  }
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
