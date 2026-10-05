#include "data/fo3-esm-reader.h"
#include "player/fo3-player-state.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unistd.h>
#include <zlib.h>

namespace {
using Bytes = std::vector<uint8_t>;
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void U16(Bytes &b, uint16_t v) {
  b.push_back(v & 255);
  b.push_back(v >> 8);
}
void U32(Bytes &b, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b.push_back((v >> (i * 8)) & 255);
}
void F32(Bytes &b, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  U32(b, v);
}
void Sub(Bytes &b, const char *type, const Bytes &value) {
  b.insert(b.end(), type, type + 4);
  U16(b, static_cast<uint16_t>(value.size()));
  b.insert(b.end(), value.begin(), value.end());
}
void Text(Bytes &b, const char *type, const std::string &value) {
  Bytes v(value.begin(), value.end());
  v.push_back(0);
  Sub(b, type, v);
}
void Record(Bytes &b, const char *type, uint32_t form, Bytes payload,
            uint32_t flags = 0) {
  if (flags & 0x40000) {
    Bytes compressed(compressBound(payload.size()));
    uLongf size = compressed.size();
    Check(compress(compressed.data(), &size, payload.data(), payload.size()) ==
              Z_OK,
          "compress fixture");
    Bytes stored;
    U32(stored, payload.size());
    stored.insert(stored.end(), compressed.begin(), compressed.begin() + size);
    payload = stored;
  }
  b.insert(b.end(), type, type + 4);
  U32(b, payload.size());
  U32(b, flags);
  U32(b, form);
  U32(b, 0);
  U32(b, 0);
  b.insert(b.end(), payload.begin(), payload.end());
}
void Write(const std::string &path, const Bytes &b) {
  std::ofstream f(path, std::ios::binary);
  f.write(reinterpret_cast<const char *>(b.data()), b.size());
  Check(f.good(), "write fixture");
}
Bytes Read(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  return Bytes(std::istreambuf_iterator<char>(f), {});
}
Bytes Fixture(bool levelled = false, bool missingRule = false,
              float startingCondition = 1, bool owned = false) {
  Bytes b, header;
  Bytes hedr(12, 0);
  Sub(header, "HEDR", hedr);
  Record(b, "TES4", 0, header);
  const std::pair<const char *, float> rules[] = {
      {"fAVDHealthEnduranceMult", 20}, {"fAVDHealthEnduranceOffset", 0},
      {"fAVDHealthLevelMult", 10},     {"fAVDActionPointsBase", 65},
      {"fAVDActionPointsMult", 2},     {"fAVDCarryWeightsBase", 150},
      {"fAVDCarryWeightMult", 10}};
  uint32_t form = 1000;
  for (const auto &rule : rules) {
    if (missingRule && form == 1000) {
      ++form;
      continue;
    }
    Bytes r, data;
    Text(r, "EDID", rule.first);
    F32(data, rule.second);
    Sub(r, "DATA", data);
    Record(b, "GMST", form++, r);
  }
  Bytes npc, ac(24, 0), data, skills(28, 0), count;
  ac[8] = 1;
  F32(data, 0);
  data.clear();
  U32(data, 100);
  for (int i = 0; i < 7; ++i)
    data.push_back(5);
  for (int i = 0; i < 14; ++i)
    skills[i] = 10 + i;
  Text(npc, "EDID", "Player");
  Sub(npc, "ACBS", ac);
  Sub(npc, "DATA", data);
  Sub(npc, "DNAM", skills);
  U32(count, levelled ? 900 : 100);
  U32(count, 2);
  Sub(npc, "CNTO", count);
  Bytes extra;
  U32(extra, owned ? 0x1234 : 0);
  U32(extra, 0);
  F32(extra, startingCondition);
  Sub(npc, "COED", extra);
  Record(b, "NPC_", 7, npc, 0x40000);
  for (uint32_t id = 100; id < 108; ++id) {
    Bytes item, d;
    Text(item, "EDID", "Item" + std::to_string(id));
    Text(item, "FULL", "Authored item");
    const char *type = "MISC";
    uint32_t flags = 0;
    if (id == 100 || id == 101) {
      type = "WEAP";
      U32(d, 20);
      U32(d, 100);
      F32(d, 5);
      U16(d, 10);
      d.push_back(6);
      Bytes dn(13, 0);
      if (id == 101)
        dn[12] = 8;
      Sub(item, "DNAM", dn);
    } else if (id == 102 || id == 103 || id == 107) {
      type = "ARMO";
      U32(d, 15);
      U32(d, 50);
      F32(d, 2);
      Bytes bm;
      U32(bm, id == 103 ? 1 : 4);
      bm.push_back(id == 107 ? 0x40 : 0);
      bm.resize(8);
      Sub(item, "BMDT", bm);
      Text(item, "MODL", "male.nif");
      Text(item, "MOD3", "female.nif");
    } else if (id == 104) {
      type = "AMMO";
      F32(d, 100);
      U32(d, 0);
      U32(d, 3);
      d.push_back(1);
    } else if (id == 105) {
      type = "ALCH";
      F32(d, 0.2f);
      Bytes en;
      U32(en, 25);
      U32(en, 0);
      Sub(item, "ENIT", en);
    } else {
      flags = 1u << 10;
      U32(d, 10);
      F32(d, 1);
    }
    Sub(item, "DATA", d);
    Record(b, type, id, item, flags);
  }
  // Wrap records after TES4 in a GRUP, exercising bounded nested traversal.
  for (uint32_t id = 400; id < 402; ++id) {
    Bytes cont, data, item, extra;
    Text(cont, "FULL", "Authored Box");
    data.push_back(2);
    F32(data, 0);
    Sub(cont, "DATA", data);
    U32(item, id == 400 ? 100 : 500);
    U32(item, id == 400 ? 2 : 3);
    Sub(cont, "CNTO", item);
    U32(extra, 0);
    U32(extra, 0);
    F32(extra, .5f);
    if (id == 400)
      Sub(cont, "COED", extra);
    Record(b, "CONT", id, cont);
    Bytes ref, base;
    U32(base, id);
    Sub(ref, "NAME", base);
    Record(b, "REFR", id - 100, ref);
  }
  Bytes list;
  Sub(list, "LVLD", Bytes{0});
  Sub(list, "LVLF", Bytes{4});
  for (uint32_t item : {102u, 104u}) {
    Bytes entry;
    U16(entry, 99);
    U16(entry, 0);
    U32(entry, item);
    U16(entry, 2);
    U16(entry, 0);
    Sub(list, "LVLO", entry);
  }
  Record(b, "LVLI", 500, list);
  for (uint32_t id = 200; id < 203; ++id) {
    Bytes placed, base, n, health;
    U32(base, 100);
    Sub(placed, "NAME", base);
    U32(n, 3);
    Sub(placed, "XCNT", n);
    F32(health, .5f);
    Sub(placed, "XHLP", health);
    if (id == 201) {
      Bytes owner;
      U32(owner, 99);
      Sub(placed, "XOWN", owner);
    }
    if (id == 202) {
      Bytes lock(12, 0);
      lock[4] = 104;
      Sub(placed, "XLOC", lock);
    }
    Record(b, "REFR", id, placed);
  }
  const size_t headerSize = 24 + header.size();
  Bytes grouped(b.begin(), b.begin() + headerSize);
  grouped.insert(grouped.end(), {'G', 'R', 'U', 'P'});
  U32(grouped, static_cast<uint32_t>(24 + b.size() - headerSize));
  for (int i = 0; i < 4; ++i)
    U32(grouped, 0);
  grouped.insert(grouped.end(), b.begin() + headerSize, b.end());
  return grouped;
}
uint64_t Id(const fo3player::Player &p, uint32_t form, bool equipped = false) {
  for (const auto &s : p.Snapshot().inventory)
    if (s.formId == form && s.equipped == equipped)
      return s.id;
  throw std::runtime_error("missing stack");
}
void Rechecksum(Bytes &b) {
  const uint32_t crc = crc32(0, b.data() + 20, b.size() - 20);
  for (int i = 0; i < 4; ++i)
    b[16 + i] = (crc >> (i * 8)) & 255;
}
void Synthetic(const std::string &root) {
  const auto esm = root + ".esm", save = root + ".fqps";
  Write(esm, Fixture());
  fo3player::Catalog c;
  std::string error;
  Check(fo3player::LoadCatalog(esm, c, error), error.c_str());
  Check(c.items.size() == 8 && c.initial.special[0] == 5 &&
            c.initial.skills[13] == 23,
        "authored stats");
  Check(c.items.at(102).femaleModel == "female.nif" &&
            !c.items.at(107).playable && !c.items.at(105).valueKnown,
        "item flags/models/auto value");
  fo3player::Player p(c);
  Check(p.MaxHealth() == 200 && p.MaxActionPoints() == 75 &&
            p.CarryCapacity() == 200 && p.InventoryWeight() == 10,
        "derived baseline");
  Check(!p.Add(900, 1) && !p.Add(100, -1) && !p.Add(100, 1, NAN),
        "reject bad add");
  Check(p.Equip(Id(p, 100)) && p.Snapshot().inventory.size() == 2,
        "split equipment stack");
  Check(p.Snapshot().inventory.front().count == 1 &&
            p.Snapshot().inventory.front().equipped,
        "selected identity remains equipped");
  Check(p.Add(101, 1) && p.Equip(Id(p, 101)), "equip replacement weapon");
  Check(!p.Remove(Id(p, 101, true), 1),
        "authored cannot-drop weapon protected");
  Check(!p.Snapshot().inventory.front().equipped && p.InventoryWeight() == 15,
        "unequip prior weapon without deleting it");
  Check(p.Add(102, 1) && p.Add(103, 1) && p.Equip(Id(p, 102)) &&
            p.Equip(Id(p, 103)),
        "nonoverlapping armour");
  const auto oldArmour = Id(p, 102, true);
  Check(p.Add(102, 1, 0.5f) && p.Equip(p.Snapshot().inventory.back().id),
        "replacement armour condition instance");
  Check(Id(p, 102) == oldArmour && Id(p, 103, true) != 0,
        "only overlapping armour displaced");
  Check(p.Add(107, 1) && !p.Equip(Id(p, 107)),
        "nonplayable armour not equippable");
  Check(p.Add(100, 1, 0) && !p.Equip(p.Snapshot().inventory.back().id),
        "broken weapon");
  Check(p.Add(100, 1, 0.5f) && p.Add(100, 2, 0.5f), "same condition merge");
  Check(p.Snapshot().inventory.back().count == 3, "condition stack count");
  Check(p.Add(106, 1) && !p.Remove(Id(p, 106), 1), "quest item protected");
  const auto before = p.Revision();
  Check(!p.Remove(Id(p, 102, true), 2) && p.Revision() == before,
        "remove atomic");
  Check(p.Add(104, INT32_MAX) && !p.Add(104, 1), "overflow rejected");
  Check(p.Add(100, 100) && p.Overencumbered(),
        "weight over limit includes equipped items");
  Check(p.DamageHealth(60) && p.Health() == 140 && p.RestoreHealth(10) &&
            p.Health() == 150,
        "health changes");
  Check(p.SpendActionPoints(50) && !p.SpendActionPoints(26) &&
            p.ActionPoints() == 25,
        "AP spend atomic");
  Check(!p.DamageHealth(INFINITY) && !p.RestoreActionPoints(NAN) &&
            p.RestoreActionPoints(500) && p.ActionPoints() == 75,
        "resource guards");
  Check(p.Save(save, error), error.c_str());
  fo3player::Player restored(c);
  Check(restored.Restore(save, error), error.c_str());
  Check(restored.Health() == p.Health() &&
            restored.InventoryWeight() == p.InventoryWeight() &&
            restored.Snapshot().inventory.size() ==
                p.Snapshot().inventory.size(),
        "save roundtrip");
  Check(Id(restored, 101, true) == Id(p, 101, true), "equipment restored");
  Check(c.references.at(200).count == 3 &&
            c.references.at(200).condition == .5f,
        "placed count and health decode");
  Check(!p.Pickup(201) && !p.Pickup(202), "owned/locked placed items blocked");
  Check(p.Pickup(200) && !p.Pickup(200) && p.Save(save, error),
        "pickup identity prevents duplication");
  Check(restored.Restore(save, error) && restored.IsCollected(200),
        "collected ref roundtrip");
  // A v1 save has the same stats/inventory prefix, without world-removal data.
  Bytes legacy = Read(save);
  legacy.resize(40 + 21 * fo3esm::ReadU32(legacy.data() + 36));
  legacy[4] = 1;
  const uint32_t legacySize = legacy.size() - 20;
  for (int i = 0; i < 4; ++i)
    legacy[12 + i] = (legacySize >> (8 * i)) & 255;
  Rechecksum(legacy);
  Write(save, legacy);
  Check(restored.Restore(save, error) && !restored.IsCollected(200),
        "v1 player saves migrate without losing inventory");
  Check(p.Save(save, error) && restored.Restore(save, error),
        "restore v2 after migration check");
  const Bytes good = Read(save);
  Bytes version3=good;
  version3.resize(version3.size()-44); // empty v4 Pip-Boy extension
  version3[4]=3;
  const auto v3size=version3.size()-20;
  for(int i=0;i<4;++i)version3[12+i]=(v3size>>(8*i))&255;
  Rechecksum(version3);Write(save,version3);
  Check(restored.Restore(save,error)&&restored.IsCollected(200)&&restored.Snapshot().inventory.size()==p.Snapshot().inventory.size(),"v3 migrates without losing legacy gameplay state");
  Bytes version2 = good;
  const size_t v2worldStart=40+21*fo3esm::ReadU32(good.data()+36);
  version2.resize(v2worldStart+8+4*fo3esm::ReadU32(good.data()+v2worldStart+4));
  version2[4] = 2;
  const auto v2size = version2.size() - 20;
  for (int i = 0; i < 4; ++i)
    version2[12 + i] = (v2size >> (8 * i)) & 255;
  Rechecksum(version2);
  Write(save, version2);
  Check(restored.Restore(save, error) && restored.IsCollected(200),
        "v2 collected saves retain world removals");
  const size_t worldStart = 40 + 21 * fo3esm::ReadU32(good.data() + 36);
  Bytes wrongWorld = good;
  wrongWorld[worldStart] ^= 1;
  Rechecksum(wrongWorld);
  Write(save, wrongWorld);
  Check(!restored.Restore(save, error), "world fingerprint mismatch rejected");
  Bytes unknownRef = good;
  unknownRef[worldStart + 11] ^= 1;
  Rechecksum(unknownRef);
  Write(save, unknownRef);
  Check(!restored.Restore(save, error),
        "unknown collected reference with valid CRC rejected");
  Bytes bad = good;
  bad.back() ^= 1;
  Write(save, bad);
  const auto rev = restored.Revision();
  Check(!restored.Restore(save, error) && restored.Revision() == rev &&
            restored.Health() == 150,
        "CRC failure atomic");
  fo3player::Session blocked(c);
  blocked.savePath = save;
  blocked.saveBlocked = true;
  Check(!blocked.Flush(error) && Read(save) == bad, "rejected save preserved");
  bad = good;
  bad.resize(30);
  Write(save, bad);
  Check(!restored.Restore(save, error), "truncated save");
  bad = good;
  bad[8] ^= 1;
  Write(save, bad);
  Check(!restored.Restore(save, error), "game identity mismatch");
  bad = good;
  bad[40 + 12] = 0;
  bad[40 + 13] = 0;
  bad[40 + 14] = 0;
  bad[40 + 15] = 0;
  Rechecksum(bad);
  Write(save, bad);
  Check(!restored.Restore(save, error),
        "semantic corrupt count with valid checksum");
  bad = good;
  const float nan = NAN;
  std::memcpy(bad.data() + 56, &nan, 4);
  Rechecksum(bad);
  Write(save, bad);
  Check(!restored.Restore(save, error), "nonfinite saved condition");
  fo3player::Player conflict(c);
  Check(conflict.Equip(Id(conflict, 100)) && conflict.Add(101, 1) &&
            conflict.Equip(Id(conflict, 101)) && conflict.Save(save, error),
        "conflicting save fixture");
  bad = Read(save);
  bad[60] = 1; // re-enable the displaced weapon, retaining a valid CRC
  Rechecksum(bad);
  Write(save, bad);
  Check(!restored.Restore(save, error), "conflicting saved weapons");
  Write(save, good);
  Check(!p.Save(root + "/missing/save", error) && Read(save) == good,
        "save failure retains existing file");
  fo3player::Session session(c);
  session.savePath = save;
  Check(session.Flush(error) &&
            session.savedRevision == session.player.Revision(),
        "baseline flush");
  Check(session.player.DamageHealth(1) && session.Flush(error),
        "dirty state flush");
  Write(esm, Fixture(true));
  Check(!fo3player::LoadCatalog(esm, c, error) && c.items.size() == 8,
        "levelled list rejected atomically");
  Write(esm, Fixture(false, true));
  Check(!fo3player::LoadCatalog(esm, c, error), "missing GMST rejected");
  Write(esm, Fixture(false, false, 0.5f));
  Check(fo3player::LoadCatalog(esm, c, error) &&
            c.initial.inventory.front().condition == 0.5f,
        "authored starting item condition retained");
  const auto fingerprint = c.fingerprint;
  Write(esm, Fixture(false, false, NAN));
  Check(!fo3player::LoadCatalog(esm, c, error) && c.fingerprint == fingerprint,
        "invalid authored condition rejected atomically");
  Write(esm, Fixture(false, false, 1, true));
  Check(!fo3player::LoadCatalog(esm, c, error) && c.fingerprint == fingerprint,
        "unresolved starting ownership rejected atomically");
  auto malformed = Fixture();
  malformed.pop_back();
  Write(esm, malformed);
  Check(!fo3player::LoadCatalog(esm, c, error), "malformed group rejected");
  std::remove(esm.c_str());
  std::remove(save.c_str());
}
void Original(const std::string &path) {
  fo3player::Catalog c;
  std::string error;
  Check(fo3player::LoadCatalog(path, c, error), error.c_str());
  fo3player::Player p(c);
  Check(p.MaxHealth() == 200 && p.MaxActionPoints() == 75 &&
            p.CarryCapacity() == 200,
        "original player derived stats");
  Check(p.Snapshot().inventory.size() == 2 &&
            p.Snapshot().inventory[0].formId == 0x15038 &&
            p.Snapshot().inventory[1].formId == 0x25b83,
        "original starting inventory");
  Check(p.Definitions().items.at(0x15038).editorId == "PipBoy" &&
            !p.Definitions().items.at(0x15038).playable,
        "original Pip-Boy record");
  Check(c.defaultActivationDoors.count(0x41714) && p.CanOpenDoor(0x3a14),
        "original Brass Lantern script allows entry");
  Check(p.CanOpenDoor(0x3a37), "original owned Brass Lantern exit allows travel");
  size_t pickups = 0;
  for (const auto &ref : c.references)
    if (p.CanPickup(ref.first))
      ++pickups;
  Check(pickups > 100, "original loose items resolve");
  size_t containerRefs = 0, generated = 0, failed = 0;
  for (const auto &ref : c.references)
    if (c.containers.count(ref.second.base)) {
      ++containerRefs;
      if (p.CanLootContainer(ref.first)) {
        if (p.PrepareContainer(ref.first))
          ++generated;
        else
          ++failed;
      }
    }
  Check(c.containers.size() > 500 && generated > 100,
        "original containers and levelled loot resolve");
  std::cout << "Original ESM: " << c.items.size()
            << " item definitions, HP=" << p.MaxHealth()
            << ", AP=" << p.MaxActionPoints() << ", carry=" << p.CarryCapacity()
            << ", starting stacks=" << p.Snapshot().inventory.size()
            << ", supported pickups=" << pickups
            << ", containers=" << c.containers.size()
            << ", container refs=" << containerRefs
            << ", generated=" << generated << ", rejected=" << failed << '\n';
}
void Containers(const std::string &root) {
  const auto esm = root + ".esm", save = root + ".fqps";
  Write(esm, Fixture());
  fo3player::Catalog c;
  std::string error;
  Check(fo3player::LoadCatalog(esm, c, error), error.c_str());
  Check(c.containers.at(400).respawns && c.lootLists.at(500).flags == 4,
        "CONT/LVLI decoded");
  fo3player::Player p(c);
  Check(p.PrepareContainer(300) &&
            p.ContainerContents(300)->front().count == 2 &&
            p.ContainerContents(300)->front().condition == .5f,
        "fixed authored contents and condition");
  const auto rev = p.Revision();
  Check(p.PrepareContainer(300) && p.Revision() == rev,
        "preview never rerolls");
  Check(p.PrepareContainer(301) && p.ContainerContents(301)->size() == 2 &&
            p.ContainerContents(301)->front().count == 6,
        "use-all includes above-level entries and parent count");
  const auto id = p.ContainerContents(300)->front().id;
  Check(p.TakeContainerStack(300, id) && p.ContainerContents(300)->empty() &&
            !p.TakeContainerStack(300, id),
        "transfer atomic and once-only");
  Check(p.Save(save, error), error.c_str());
  fo3player::Player restored(c);
  Check(restored.Restore(save, error) &&
            restored.ContainerContents(300)->empty() &&
            restored.ContainerContents(301)->size() == 2,
        "empty and remaining container contents persist");
  Check(restored.PrepareContainer(300) &&
            restored.ContainerContents(300)->empty(),
        "emptied container never regenerates");
  Check(p.Add(104, INT32_MAX), "overflow fixture");
  uint64_t ammo = 0;
  for (const auto &s : *p.ContainerContents(301))
    if (s.formId == 104)
      ammo = s.id;
  const auto revision = p.Revision();
  Check(!p.TakeContainerStack(301, ammo) && p.Revision() == revision &&
            p.ContainerContents(301)->size() == 2,
        "failed transfer leaves loot intact");
  c.references[300].owner = 99;
  fo3player::Player owned(c);
  Check(!owned.PrepareContainer(300), "owned container blocked");
  c.references[300].owner = 0;
  c.containers[400].script = 123;
  fo3player::Player scripted(c);
  Check(!scripted.PrepareContainer(300), "scripted container blocked");
  c.containers[400].script = 0;
  c.lootLists[500].flags = 0;
  fo3player::Player level(c);
  Check(level.PrepareContainer(301) && level.ContainerContents(301)->empty(),
        "above-level loot excluded without Use All");
  c.lootLists[500].flags = 4;
  c.lootLists[500].chanceNone = 100;
  fo3player::Player none(c);
  Check(none.PrepareContainer(301) && none.ContainerContents(301)->empty(),
        "chance-none persisted empty");
  c.lootLists[500].chanceNone = 0;
  c.lootLists[500].entries[0].form = 500;
  fo3player::Player cycle(c);
  Check(!cycle.PrepareContainer(301) && !cycle.ContainerContents(301) &&
            cycle.Revision() == 0,
        "nested cycle rejected without partial state");
  std::remove(esm.c_str());
  std::remove(save.c_str());
}
} // namespace
int main(int argc, char **argv) {
  try {
    Synthetic("/tmp/falloutquest-player-" + std::to_string(getpid()));
    Containers("/tmp/falloutquest-containers-" + std::to_string(getpid()));
    if (argc > 1)
      Original(argv[1]);
    std::cout << "Player state tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
