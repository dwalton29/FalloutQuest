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
  if(const auto* et=Find(subs,"ETYP");et&&et->size==4)item.equipType=I32(et->data);
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
    item.ingestibleFlags=en->data[4];
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
      e.ownerParameter=fo3esm::ReadU32(sub.data + 4);
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

static void FinalizePlayerCatalog(Catalog& catalog) {
  const auto perk=catalog.pipboy.formNames.find("infiltrator");
  if(perk!=catalog.pipboy.formNames.end())catalog.infiltrator=perk->second;
  // Rebuild even for copied/user-supplied Catalogs: stale indexes are not proof
  // of identity. Duplicate IDs anywhere in the catalogue remain ambiguous.
  catalog.dialogueInfos.clear();
  for(const auto& topic:catalog.pipboy.topics)for(size_t n=0;n<topic.second.size();++n){
    const auto& info=topic.second[n];
    auto inserted=catalog.dialogueInfos.emplace(info.id,std::make_pair(topic.first,n));
    if(!inserted.second||!info.id||!topic.first||info.topic!=topic.first)
      inserted.first->second={0,0};
  }
}

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
    if (ftello(file.get()) != static_cast<off_t>(at) &&
        fseeko(file.get(), static_cast<off_t>(at), SEEK_SET))
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
    const bool extra = type=="FACT"||fo3pipdata::Relevant(type)||type=="ACHR"||type=="ACRE"||fo3weapon::Relevant(type);
    // Decode scripted base objects even if they have no other runtime data.
    // SCRI is on the base definition; the placed REFR/ACHR/ACRE supplies identity.
    const bool eventBase = type=="ACTI"||type=="CREA"||type=="FURN"||
        type=="MSTT"||type=="CONT"||type=="LIGH"||type=="FLOR"||
        type=="STAT"||type=="TERM"||type=="DOOR"||type=="IMAD"||type=="TACT";
    const bool selected = extra || eventBase || lootRecord || worldRecord || type == "TES4" ||
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
      if(type=="CREA")next.deferredBases.insert(form);
      if(type=="IMAD")next.imageModifiers.emplace(form,fo3script::DecodeImageModifier(payload));
      if(type=="REFR"||type=="ACHR"||type=="ACRE") {
        if(Find(subs,"XEZN"))next.encounterZoneReferences.insert(form);
        // Keep the complete graph, including parents outside the resident cell.
        // Do not expand each of the hundreds of thousands of Reference objects.
        const auto* parent=Find(subs,"XESP");
        if(parent){EnableParent link;link.valid=parent->size==8;
          if(link.valid){link.reference=fo3esm::ReadU32(parent->data);link.flags=parent->data[4];
            link.valid=link.reference!=0&&(link.flags&~3u)==0;}
          next.enableParents.emplace(form,link);
        }
      }
      if(eventBase||type=="NPC_"||type=="WEAP"||type=="ARMO"||
         type=="BOOK"||type=="MISC"||type=="ALCH"||type=="KEYM"){
        if(const auto* script=Find(subs,"SCRI");script&&script->size==4)
          next.eventBaseScripts[form]=fo3esm::ReadU32(script->data);
        if(type=="ACTI"||type=="TERM"||type=="FURN"||type=="DOOR")
          next.eventBaseNames[form]=Text(subs,"FULL");
      }
      if(type=="FACT") {
        const auto* data=Find(subs,"DATA");
        if(data&&data->size>=1&&data->size<=4){uint32_t bits=0;for(size_t i=0;i<data->size;++i)bits|=uint32_t(data->data[i])<<(8*i);next.factionFlags[form]=bits;}
      }
      if(type=="NPC_"||type=="CREA") {
        if(const auto* ai=Find(subs,"AIDT");ai&&ai->size==20)next.merchantServices[form]=fo3esm::ReadU32(ai->data+8);
        Container inventory;inventory.name=Text(subs,"FULL");
        inventory.valid=DecodeLoot(subs,false,inventory.entries);
        next.actorInventories[form]=std::move(inventory);
      }
      if(type=="ACHR"||type=="ACRE") {
        if(const auto* merchant=Find(subs,"XMRC")){
          next.merchantContainers[form]=merchant->size==4?fo3esm::ReadU32(merchant->data):0;
        }
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
      } else if (!worldRecord && !eventBase && (!extra || (type=="NPC_"&&form==PlayerBase))) {
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
          if (editor == "TimeScale" && globalValue > 0.0f && globalValue <= 1000.0f) {
            next.gameTimeScale = globalValue;next.timeScaleGlobal=form;
          }
          if (editor == "GameHour" && globalValue >= 0.0f && globalValue < 24.0f) {
            next.initial.gameHour = globalValue;next.hourGlobal=form;
          }
          if (editor == "GameDaysPassed" && globalValue >= 0.0f &&
              globalValue < 1000000.0f)
            {next.initial.gameDaysPassed = static_cast<uint32_t>(globalValue);next.daysPassedGlobal=form;}
          if (editor == "GameYear" && globalValue>=1&&globalValue<=9999) {
            next.initial.gameYear = static_cast<uint32_t>(globalValue);next.yearGlobal=form;
          }
          if (editor == "GameMonth" && globalValue>=0&&globalValue<12) {
            next.initial.gameMonth = static_cast<uint32_t>(globalValue);next.monthGlobal=form;
          }
          if (editor == "GameDay" && globalValue>=1&&globalValue<=31) {
            next.initial.gameDay = static_cast<uint32_t>(globalValue);next.dayGlobal=form;
          }
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
            ref.ownership.owner=ref.owner;ref.ownership.explicitOwner=true;
          } else if(sub.type=="XRNK") {
            if(sub.size!=4)ref.ownership.valid=false;
            else ref.ownership.rank=I32(sub.data);
          } else if(sub.type=="XGLB") {
            if(sub.size!=4)ref.ownership.valid=false;
            else ref.ownership.global=fo3esm::ReadU32(sub.data);
          } else if (sub.type == "XLOC") {
            if (sub.size < 12)
              ref.valid = false;
            else {
              ref.locked =
                  true; // XLOC authors a lock, including key-only locks.
              ref.key = fo3esm::ReadU32(sub.data + 4);
              ref.lockLevel=sub.data[0];ref.lockFlags=sub.data[8];
              if(sub.size>=20)std::copy_n(sub.data+12,8,ref.lockMetadata.begin());
            }
          }
        }
        ref.valid = ref.valid && ref.count > 0 &&
                    std::isfinite(ref.condition) && ref.condition >= 0 &&
                    ref.condition <= 1;
        next.references[form] = ref;
      } else if (type == "CELL") {
        CellOwnership cell;cell.editor=Text(subs,"EDID");cell.name=Text(subs,"FULL");
        const auto *owner=Find(subs,"XOWN"),*rank=Find(subs,"XRNK"),*global=Find(subs,"XGLB"),*data=Find(subs,"DATA");
        if(owner){cell.data.explicitOwner=true;cell.data.valid=owner->size==4;if(cell.data.valid)cell.data.owner=fo3esm::ReadU32(owner->data);}
        if(rank){if(rank->size==4)cell.data.rank=I32(rank->data);else cell.data.valid=false;}
        if(global){if(global->size==4)cell.data.global=fo3esm::ReadU32(global->data);else cell.data.valid=false;}
        if(data&&data->size)cell.flags=data->data[0];
        next.cellOwnership[form]=cell;
      } else if (type == "DOOR") {
        next.doorBases.insert(form);
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
          {settings[id] = fo3esm::ReadF32(d->data);next.barterSettings[id]=settings[id];}
        if (!id.empty() && id[0] == 's' && d)
          next.strings[id] = fo3esm::ZString(d->data, d->size);
        if(id=="iHoursToRespawnCell"&&d&&d->size==4&&I32(d->data)>0)next.merchantRespawnHours=uint32_t(I32(d->data));
        if(id=="iKarmaChangeThreshold"&&d&&d->size==4&&I32(d->data)>0)next.karmaMajorThreshold=I32(d->data);
        if(id=="iXPBumpBase"&&d&&d->size==4&&I32(d->data)>0)
          next.xpBumpBase=uint32_t(I32(d->data));
        if(id=="iLevelUpSkillPointsBase"&&d&&d->size==4&&I32(d->data)>0)
          next.levelSkillPointsBase=I32(d->data);
        if(id=="iLevelUpSkillPointsInterval"&&d&&d->size==4&&I32(d->data)>0)
          next.levelSkillPointsInterval=I32(d->data);
        if(!id.empty() && d && d->size==4 && (id[0]=='i'||id[0]=='f') &&
           (id.find("Lock")!=std::string::npos||id.find("SweetSpot")!=std::string::npos||id.find("Concentric")!=std::string::npos||id=="fPickBreakSecs"||id.find("Hack")!=std::string::npos))
          next.lockSettings[id]=id[0]=='f'?fo3esm::ReadF32(d->data):float(I32(d->data));
        if (id == "iLevItemLevelDifferenceMax" && d && d->size == 4)
          next.lootLevelDifference = std::max(0, I32(d->data));
      } else if (item) {
        Item definition;
        if (!form || !DecodeItem(form, flags, kind, subs, definition))
          return fail("Invalid inventory item definition");
        if (kind == ItemKind::Weapon)
          fo3weapon::DecodeWeapon(payload, definition.weapon);
        if(Text(subs,"EDID")=="Lockpick")next.bobbyPin=form;
        next.items[form] = std::move(definition);
      } else if(type=="NPC_"&&form==PlayerBase) {
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
  // Fast NPC flee locomotion uses the actual Fallout3.esm run multiplier;
  // absent/invalid source fails closed to current walk speed.
  const auto runSetting=settings.find("fMoveRunMult");
  if(runSetting!=settings.end()&&std::isfinite(runSetting->second)&&
     runSetting->second>=1.f&&runSetting->second<=10.f)
    next.npcRunMultiplier=runSetting->second;
  const auto baseSpeed=settings.find("fMoveBaseSpeed");
  if(baseSpeed!=settings.end()&&std::isfinite(baseSpeed->second)&&baseSpeed->second>0)
    next.npcBaseSpeed=baseSpeed->second;
  for(const char* name:{"fConfidenceCautious","fConfidenceAverage","fConfidenceBrave",
      "fCombatFleeNormalDistance","fCombatFleeWaitTime","fCombatFleeMaxCoverDistance",
      "fCombatFleeMaxAllyDistance","fCombatFleeMinAllyDistance"}) {
    const auto setting=settings.find(name);
    if(setting!=settings.end()&&std::isfinite(setting->second)&&setting->second>=0)
      next.npcCombatSettings.emplace(name,setting->second);
  }
  fo3weapon::FinalizeStatistics(next.weapons);
  fo3pipdata::FinalizeLevelledCategories(next.pipboy,next.weapons);
  for(const auto& reference:next.references){
    const auto script=next.eventBaseScripts.find(reference.second.base);
    if(script!=next.eventBaseScripts.end()&&script->second)
      next.pipboy.objectReferenceScripts[reference.first]={script->second,reference.second.base};
  }
  fo3pipdata::Finalize(next.pipboy);
  // Immutable event indices are generated once from source definitions.
  // The original compiled bytes remain attached and are never inferred.
  for(const auto& script:next.pipboy.scripts)
    next.eventPrograms.emplace(script.first,fo3script::ParseEvents(script.second));
  // All eligible original ACHR identities must remain resolvable by actor
  // health, hostility, persistence and the unloaded scheduler. Keep their
  // full authored pose even if no quest, patrol or door currently points here.
  for(const auto& entry:next.actorPlacements){
    const auto& placement=entry.second;
    if((placement.flags&0x20u)||!next.weapons.actors.count(placement.base))continue;
    next.pipboy.targets.emplace(entry.first,placement);
  }
  next.fingerprint = fingerprint;
  if(settings.count("fKarmaModStealing")&&std::isfinite(settings.at("fKarmaModStealing")))next.theftKarma=settings.at("fKarmaModStealing");
  if(settings.count("fAlignEvilMaxKarma")&&std::isfinite(settings.at("fAlignEvilMaxKarma")))next.evilMaxKarma=settings.at("fAlignEvilMaxKarma");
  // Validate the exact parsed catalogue through Player without cloning every
  // quest, script, reference and NPC back out of Definitions afterwards.
  // Only this builder owns the mutable alias, and it is used again strictly
  // after the validation Player has been destroyed.
  FinalizePlayerCatalog(next);
  auto parsed=std::make_shared<Catalog>(std::move(next));
  State initialState;
  {
    Player initial{std::shared_ptr<const Catalog>(parsed)};
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
    initialState=initial.Snapshot();
  }
  parsed->initial=std::move(initialState);
  out=std::move(*parsed);
  return true;
}

static std::shared_ptr<const Catalog> SharePlayerCatalog(Catalog catalog) {
  FinalizePlayerCatalog(catalog);
  return std::make_shared<const Catalog>(std::move(catalog));
}
Player::Player(Catalog catalog)
    : catalog_(SharePlayerCatalog(std::move(catalog))), state_(catalog_->initial) {}
Player::Player(std::shared_ptr<const Catalog> catalog)
    : catalog_(std::move(catalog)), state_(catalog_->initial) {}
bool Player::SetPlayerAttributes(const std::array<uint8_t,7>& special,
                                 const std::array<uint8_t,14>& skills) {
  if(std::any_of(special.begin(),special.end(),[](uint8_t value){return value<1||value>10;}) ||
     std::any_of(skills.begin(),skills.end(),[](uint8_t value){return value>100;}))return false;
  const auto& r=catalog_->rules;
  const float maxHealth=state_.baseHealth+
      (special[2]+r.healthEnduranceOffset)*r.healthEnduranceMult+
      (state_.level-1)*r.healthLevelMult;
  const float maxAp=r.apBase+special[5]*r.apMult;
  if(!std::isfinite(maxHealth)||maxHealth<=0||state_.healthDamage>maxHealth||
     !std::isfinite(maxAp)||maxAp<0||state_.apSpent>maxAp)return false;
  if(state_.special==special&&state_.skills==skills)return true;
  state_.special=special;state_.skills=skills;++revision_;
  return true;
}
bool Player::AdvanceGameClock(double realSeconds) {
  if (!std::isfinite(realSeconds) || realSeconds <= 0.0 || realSeconds > 1.0 ||
      !std::isfinite(catalog_->gameTimeScale) || catalog_->gameTimeScale <= 0.0f)
    return false;
  return AdvanceGameHours(realSeconds * double(catalog_->gameTimeScale) / 3600.0);
}
bool Player::AdvanceGameHours(double hours) {
  if(!std::isfinite(hours)||hours<=0||hours>24*365||state_.gameYear>9998)return false;
  const double total=double(state_.gameHour)+hours;
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
bool Player::GlobalValue(uint32_t form,float& value) const {
  const auto found=catalog_->globals.find(form);
  if(found==catalog_->globals.end())return false;
  if(form==catalog_->hourGlobal)value=state_.gameHour;
  else if(form==catalog_->dayGlobal)value=float(state_.gameDay);
  else if(form==catalog_->monthGlobal)value=float(state_.gameMonth);
  else if(form==catalog_->yearGlobal)value=float(state_.gameYear);
  else if(form==catalog_->daysPassedGlobal)
    value=float(double(state_.gameDaysPassed)+(double(state_.gameHour)-double(catalog_->initial.gameHour))/24.);
  else if(form==catalog_->timeScaleGlobal)value=catalog_->gameTimeScale;
  else {auto v=state_.pipboy.mutableGlobals.find(form);value=v==state_.pipboy.mutableGlobals.end()?found->second:v->second;}
  return true;
}
ActorCensusReport Player::RegisterOriginalActors() {
  ActorCensusReport report;
  report.authored=catalog_->actorPlacements.size();
  // Original FormID order makes seeding deterministic independent of hash
  // iteration order, device/compiler, or scene-entry timing.
  std::vector<uint32_t> refs;refs.reserve(report.authored);
  for(const auto& entry:catalog_->actorPlacements)refs.push_back(entry.first);
  std::sort(refs.begin(),refs.end());
  for(uint32_t id:refs){
    if(state_.actors.count(id)){++report.alreadyTracked;continue;}
    const auto& p=catalog_->actorPlacements.at(id);
    // Track conditional actors too. Canonical enable evaluation controls
    // residency; tracking their authored state never activates them.
    if(p.flags&0x20u){++report.disabledOrConditional;continue;}
    if(!p.base||!catalog_->weapons.actors.count(p.base)||
       !catalog_->pipboy.targets.count(id)){++report.unsupportedBase;continue;}
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
  const auto &r = catalog_->rules;
  return state_.baseHealth +
         (state_.special[2] + r.healthEnduranceOffset) * r.healthEnduranceMult +
         (state_.level - 1) * r.healthLevelMult;
}
float Player::Health() const {
  return std::max(0.0f, MaxHealth() - state_.healthDamage);
}
float Player::MaxActionPoints() const {
  return catalog_->rules.apBase + state_.special[5] * catalog_->rules.apMult;
}
float Player::ActionPoints() const {
  return std::max(0.0f, MaxActionPoints() - state_.apSpent);
}
float Player::CarryCapacity() const {
  return catalog_->rules.carryBase +
         state_.special[0] * catalog_->rules.carryMult;
}
double Player::InventoryWeight() const {
  double result = 0;
  for (const auto &s : state_.inventory)
    result += static_cast<double>(catalog_->items.at(s.formId).weight) * s.count;
  return result;
}
bool Player::Add(uint32_t form, int32_t count, float condition) {
  if (count <= 0 || !std::isfinite(condition) || condition < 0 ||
      condition > 1 || !catalog_->items.count(form))
    return false;
  const auto &item = catalog_->items.at(form);
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
      (IsQuestObject(it->formId) ||
       catalog_->items.at(it->formId).cannotDrop))
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
  if (it == v.end() || !CanEquip(catalog_->items.at(it->formId)) ||
      (it->condition <= 0 && catalog_->items.at(it->formId).kind != ItemKind::Weapon))
    return false;
  if (it->equipped)
    return true;
  const Item &item = catalog_->items.at(it->formId);
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
    if (s.equipped && Conflicts(item, catalog_->items.at(s.formId)))
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
bool Player::ReferenceDisabled(uint32_t reference,bool& disabled,std::string& error) const {
  return ReferenceDisabledWith(state_.referenceEnabled,reference,disabled,error);
}
bool Player::ReferenceDisabledWith(const std::unordered_map<uint32_t,bool>& roots,
                                  uint32_t reference,bool& disabled,std::string& error) const {
  error.clear();disabled=false;
  // Bounded stack-only traversal. No hot-frame scan, allocation or mutable ESM state.
  std::array<uint32_t,128> visited{};size_t count=0;bool opposite=false;
  auto fail=[&](const char* why){error=std::string("GetDisabled reference ")+std::to_string(reference)+": "+why;return false;};
  while(reference){
    const auto ref=catalog_->references.find(reference);
    if(ref==catalog_->references.end()||(ref->second.flags&0x20))return fail("reference unavailable/deleted");
    if(count==visited.size())return fail("enable-parent depth exceeded");
    for(size_t i=0;i<count;++i)if(visited[i]==reference)return fail("enable-parent cycle");
    visited[count++]=reference;
    const auto parent=catalog_->enableParents.find(reference);
    if(parent==catalog_->enableParents.end()){
      const auto changed=roots.find(reference);
      const bool enabled=changed==roots.end()?!(ref->second.flags&0x800):changed->second;
      disabled=(!enabled)^opposite;return true;
    }
    const auto& link=parent->second;
    if(!link.valid||!link.reference||(link.flags&~3u))return fail("malformed enable parent");
    // XESP bit 0 inverts state. Bit 1 controls presentation (pop-in), not truth.
    opposite^=bool(link.flags&1);reference=link.reference;
  }
  return fail("null reference");
}
bool Player::StageReferenceEnableIn(std::unordered_map<uint32_t,bool>& roots,
                                    uint32_t reference,bool enabled,bool& changed,std::string& error) const {
  changed=false;
  bool disabled=false;
  if(!ReferenceDisabledWith(roots,reference,disabled,error))return false;
  // Original enable-parent children cannot be independently toggled. A valid
  // parent-controlled request is a no-op, not a separate shadow flag.
  if(catalog_->enableParents.count(reference)||enabled==!disabled)return true;
  const bool initial=!(catalog_->references.at(reference).flags&0x800);
  if(enabled==initial)roots.erase(reference);
  else {
    if(roots.size()>=100000&&!roots.count(reference)){
      error="reference override limit exceeded";return false;
    }
    roots[reference]=enabled;
  }
  changed=true;return true;
}
bool Player::StageReferenceEnable(uint32_t reference,bool enabled,std::string& error) {
  bool changed=false;
  if(!StageReferenceEnableIn(state_.referenceEnabled,reference,enabled,changed,error))return false;
  if(changed){++state_.referenceStateRevision;++revision_;}
  return true;
}
fo3world::ReferenceStateSnapshot Player::CaptureReferenceStateWith(
    const std::unordered_map<uint32_t,bool>& roots,uint64_t revision) const {
  fo3world::ReferenceStateSnapshot out;out.revision=revision;
  out.states.reserve(catalog_->enableParents.size()+roots.size());
  auto capture=[&](uint32_t id){bool disabled=false;std::string error;
    const bool valid=ReferenceDisabledWith(roots,id,disabled,error);
    out.states[id]=!valid?fo3world::ReferenceEnableState::Unavailable:
      disabled?fo3world::ReferenceEnableState::Disabled:fo3world::ReferenceEnableState::Enabled;
  };
  for(const auto& child:catalog_->enableParents)capture(child.first);
  for(const auto& changed:roots)capture(changed.first);
  return out;
}
fo3world::ReferenceStateSnapshot Player::CaptureReferenceState() const {
  return CaptureReferenceStateWith(state_.referenceEnabled,state_.referenceStateRevision);
}
std::unique_ptr<ReferenceEnablePlan> Player::PrepareReferenceEnablePlan(
    const std::vector<ReferenceEnableChange>& changes,std::string& error) const {
  error.clear();
  if(changes.size()>100000){error="reference mutation batch limit exceeded";return {};}
  auto plan=std::unique_ptr<ReferenceEnablePlan>(new ReferenceEnablePlan());
  plan->owner_=this;plan->ownerIdentity_=referencePlanIdentity_;
  plan->expectedRevision_=revision_;
  plan->expectedReferenceRevision_=state_.referenceStateRevision;
  plan->roots_=state_.referenceEnabled;
  for(const auto& change:changes){bool changed=false;
    if(!StageReferenceEnableIn(plan->roots_,change.reference,change.enabled,changed,error))return {};
  }
  plan->changed_=plan->roots_!=state_.referenceEnabled;
  if(plan->changed_ && (revision_>UINT64_MAX-2 || state_.referenceStateRevision>UINT64_MAX-2)){
    error="reference mutation revision exhausted";return {};
  }
  const uint64_t candidateRevision=state_.referenceStateRevision+(plan->changed_?1:0);
  plan->candidate_=std::make_shared<const fo3world::ReferenceStateSnapshot>(
      CaptureReferenceStateWith(plan->roots_,candidateRevision));
  if(!plan->changed_){plan->rollback_=plan->candidate_;return plan;}
  plan->rollback_=std::make_shared<const fo3world::ReferenceStateSnapshot>(
      CaptureReferenceStateWith(state_.referenceEnabled,state_.referenceStateRevision+2));
  // Both projections include every XESP child, not just resident references.
  // Include old and new root overrides so removal of an override is not missed.
  std::vector<uint32_t> candidates;
  candidates.reserve(plan->candidate_->states.size()+plan->rollback_->states.size());
  for(const auto& entry:plan->candidate_->states)candidates.push_back(entry.first);
  for(const auto& entry:plan->rollback_->states)candidates.push_back(entry.first);
  std::sort(candidates.begin(),candidates.end());
  candidates.erase(std::unique(candidates.begin(),candidates.end()),candidates.end());
  for(uint32_t id:candidates){
    const auto ref=catalog_->references.find(id);
    const bool authored=ref!=catalog_->references.end() && !(ref->second.flags&0x800);
    if(plan->candidate_->Enabled(id,authored)!=plan->rollback_->Enabled(id,authored))
      plan->affected_.push_back(id);
  }
  return plan;
}
bool Player::CanCommitReferenceEnablePlan(const ReferenceEnablePlan& plan) const noexcept {
  return plan.owner_==this && plan.ownerIdentity_==referencePlanIdentity_ &&
    plan.phase_==ReferenceEnablePlan::Phase::Prepared && plan.expectedRevision_==revision_ &&
    plan.expectedReferenceRevision_==state_.referenceStateRevision;
}
bool Player::CommitReferenceEnablePlan(ReferenceEnablePlan& plan) noexcept {
  if(!CanCommitReferenceEnablePlan(plan))return false;
  if(plan.changed_){
    state_.referenceEnabled.swap(plan.roots_);
    state_.referenceStateRevision=plan.candidate_->revision;
    ++revision_;
  }
  plan.expectedRevision_=revision_;plan.expectedReferenceRevision_=state_.referenceStateRevision;
  plan.phase_=ReferenceEnablePlan::Phase::Applied;return true;
}
bool Player::RollbackReferenceEnablePlan(ReferenceEnablePlan& plan) noexcept {
  if(plan.owner_!=this || plan.ownerIdentity_!=referencePlanIdentity_ ||
     plan.phase_!=ReferenceEnablePlan::Phase::Applied || plan.expectedRevision_!=revision_ ||
     plan.expectedReferenceRevision_!=state_.referenceStateRevision)return false;
  if(plan.changed_){
    state_.referenceEnabled.swap(plan.roots_);
    state_.referenceStateRevision=plan.rollback_->revision;
    ++revision_;
  }
  plan.phase_=ReferenceEnablePlan::Phase::RolledBack;return true;
}
bool Player::CanPickup(uint32_t id) const {
  const auto r = catalog_->references.find(id);
  if (r == catalog_->references.end() || IsCollected(id))
    return false;
  const auto &ref = r->second;
  const auto i = catalog_->items.find(ref.base);
  if(!ref.valid || (ref.flags & 0x20) || !ResolveOwnership(id).known || ref.locked ||
     i==catalog_->items.end() || !i->second.playable || i->second.script ||
     !(i->second.maxCondition>0 || ref.condition==1))return false;
  bool disabled=false;std::string error;
  return ReferenceDisabled(id,disabled,error)&&!disabled;
}
bool Player::Pickup(uint32_t id) {
  if (!CanPickup(id) || state_.collected.size() >= 100000)
    return false;
  const auto &ref = catalog_->references.at(id);
  // Record before Add (which can allocate); undo on an invalid Add.
  state_.collected.insert(id);
  if (!Add(ref.base, ref.count, ref.condition)) {
    state_.collected.erase(id);
    return false;
  }
  CommitTheft(id,ref.base,ref.count,0,ResolveOwnership(id));
  return true;
}
bool Player::CanOpenDoor(uint32_t id) const {return CanActorOpenDoor(PlayerRef,id);}
bool Player::CanActorOpenDoor(uint32_t actor,uint32_t id) const {return CanOpenDoorImpl(actor,id,false);}
bool Player::CanScriptActivateDoor(uint32_t id) const {
  const auto r=catalog_->references.find(id);
  return r!=catalog_->references.end()&&catalog_->doorBases.count(r->second.base)&&CanOpenDoorImpl(PlayerRef,id,true);
}
bool Player::CanOpenDoorImpl(uint32_t actor,uint32_t id,bool sourceDelegated) const {
  const auto r = catalog_->references.find(id);
  if (r == catalog_->references.end())
    return false;
  const auto &ref = r->second;
  // Ownership controls trespass/crime, not whether an unlocked door opens.
  if (!ref.valid || (ref.flags & 0x20u) ||
      (!sourceDelegated && catalog_->scriptedBases.count(ref.base) &&
       (actor!=PlayerRef||!catalog_->defaultActivationDoors.count(ref.base))))
    return false;
  bool disabled=false;std::string error;
  if(!ReferenceDisabled(id,disabled,error)||disabled)return false;
  if(actor!=PlayerRef&&ActorHealth(actor)<=0)return false;
  if (!GetLock(id).locked)
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
  Put32(payload, catalog_->worldFingerprint);
  Put32(payload, static_cast<uint32_t>(state_.collected.size()));
  std::vector<uint32_t> collected(state_.collected.begin(),
                                  state_.collected.end());
  std::sort(collected.begin(), collected.end());
  for (auto id : collected)
    Put32(payload, id);
  Bytes bytes{'F', 'Q', 'P', 'S'};
  Put32(payload, catalog_->lootFingerprint);
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
    if (catalog_->items.at(s.formId).kind == ItemKind::Weapon) weapons.push_back(&s);
  for (auto id : containerIds)
    for (const auto &s : state_.containers.at(id))
      if (catalog_->items.at(s.formId).kind == ItemKind::Weapon) weapons.push_back(&s);
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
  // v12: karma and mutable quest-object flags; older FQPS saves remain readable.
  PutFloat(payload,state_.karma);
  Put32(payload,static_cast<uint32_t>(state_.questObjectFlags.size()));
  std::vector<uint32_t> questFlagIds;
  for(const auto& kv:state_.questObjectFlags)questFlagIds.push_back(kv.first);
  std::sort(questFlagIds.begin(),questFlagIds.end());
  for(uint32_t form:questFlagIds) {
    Put32(payload,form);
    payload.push_back(state_.questObjectFlags.at(form)?1u:0u);
  }
  // v13 sparse ownership metadata belongs only to still-uncollected contents.
  std::vector<const Stack*> ownedContents;
  for(uint32_t id:containerIds)for(const auto& stack:state_.containers.at(id))
    if(stack.ownership.explicitOwner)ownedContents.push_back(&stack);
  Put32(payload,static_cast<uint32_t>(ownedContents.size()));
  for(const auto* stack:ownedContents){Put64(payload,stack->id);Put32(payload,stack->ownership.owner);
    Put32(payload,static_cast<uint32_t>(stack->ownership.rank));Put32(payload,stack->ownership.global);}
  // v14: sparse lock overlays, XP. Interrupted minigames have no overlay.
  Put32(payload,state_.experience);
  std::vector<uint32_t> lockIds;for(const auto& kv:state_.locks)lockIds.push_back(kv.first);
  std::sort(lockIds.begin(),lockIds.end());Put32(payload,uint32_t(lockIds.size()));
  for(auto id:lockIds){const auto& l=state_.locks.at(id);Put32(payload,id);
    payload.push_back(uint8_t(l.change));payload.push_back(l.level);
    payload.push_back(l.forceFailures);payload.push_back(l.rewarded?1:0);}
  // Optional RFE1 after the v14 lock tail. Empty overlays keep existing v14
  // bytes unchanged; older supported saves do not require this extension.
  const bool hasActorVariables=!state_.actorVariables.empty()||!state_.actorSpawnLevels.empty();
  const bool hasDoorStates=!state_.doorStates.empty()||hasActorVariables;
  const bool hasDebugPoints=state_.debugPerkPoints!=0;
  const bool hasLevelAdvancement=state_.level!=catalog_->initial.level||hasDebugPoints||hasDoorStates;
  // v18 retains all v17 tails, even empty ones, before LVP1.
  const bool hasCompletionState=!state_.cellNameMessages.empty()||
      !state_.achievements.empty()||hasLevelAdvancement;
  const bool hasMerchantVisits=hasCompletionState||!state_.merchantVisits.empty()||!state_.actorDispositionModifiers.empty();
  const bool hasPlayerAttributeOverrides=state_.special!=catalog_->initial.special ||
      state_.skills!=catalog_->initial.skills;
  // An overriding v15 save requires RFE1 (including the empty case) before
  // PST1. Unmodified characters retain v14, including synthetic test actors.
  if(hasMerchantVisits||hasPlayerAttributeOverrides||!state_.referenceEnabled.empty()) {
    if(state_.referenceEnabled.size()>100000){error="Too many reference overrides";return false;}
    std::vector<uint32_t> ids;
    for(const auto& entry:state_.referenceEnabled){
      const auto ref=catalog_->references.find(entry.first);
      if(ref==catalog_->references.end()||(ref->second.flags&0x20)||catalog_->enableParents.count(entry.first)){
        error="Invalid reference override";return false;
      }
      ids.push_back(entry.first);
    }
    std::sort(ids.begin(),ids.end());Put32(payload,0x31454652);Put32(payload,uint32_t(ids.size()));
    for(auto id:ids){Put32(payload,id);payload.push_back(state_.referenceEnabled.at(id)?1:0);}
  }
  // v15: player stats are save-owned overrides. Earlier saves inherit original
  // Fallout3.esm NPC_ 00000007 values. Do not mutate the ESM catalogue.
  if(hasMerchantVisits||hasPlayerAttributeOverrides){
    if(std::any_of(state_.special.begin(),state_.special.end(),[](uint8_t v){return v<1||v>10;})||
       std::any_of(state_.skills.begin(),state_.skills.end(),[](uint8_t v){return v>100;})){
      error="Invalid player attributes";return false;
    }
    Put32(payload,0x31545350); // PST1: 7 SPECIAL bytes, 14 base skill bytes.
    payload.insert(payload.end(),state_.special.begin(),state_.special.end());
    payload.insert(payload.end(),state_.skills.begin(),state_.skills.end());
  }
  if(hasMerchantVisits){
    if(state_.merchantVisits.size()>10000){error="Too many merchant visits";return false;}
    Put32(payload,0x31524d42); // BMR1: last observed game time for merchant storage.
    std::vector<uint32_t> refs;for(const auto& visit:state_.merchantVisits)refs.push_back(visit.first);
    std::sort(refs.begin(),refs.end());Put32(payload,uint32_t(refs.size()));
    for(auto ref:refs){const auto& visit=state_.merchantVisits.at(ref);
      const auto placed=catalog_->references.find(ref);
      if(placed==catalog_->references.end()||!catalog_->containers.count(placed->second.base)||
         !state_.containers.count(ref)||!std::isfinite(visit.hour)||visit.hour<0||visit.hour>=24||
         visit.day>state_.gameDaysPassed||(visit.day==state_.gameDaysPassed&&visit.hour>state_.gameHour)){
        error="Invalid merchant visit";return false;}
      Put32(payload,ref);Put32(payload,visit.day);PutFloat(payload,visit.hour);
    }
  }
  if(hasMerchantVisits){
    if(state_.actorDispositionModifiers.size()>10000){error="Too many disposition modifiers";return false;}
    Put32(payload,0x31524442); // BDR1: authored ModDisposition instance deltas.
    std::vector<uint32_t> refs;for(const auto& entry:state_.actorDispositionModifiers)refs.push_back(entry.first);
    std::sort(refs.begin(),refs.end());Put32(payload,uint32_t(refs.size()));
    for(auto id:refs){const auto actor=catalog_->references.find(id);
      if(actor==catalog_->references.end()||!catalog_->pipboy.dialogueActors.count(actor->second.base)){
        error="Invalid disposition actor";return false;}
      Put32(payload,id);Put32(payload,uint32_t(state_.actorDispositionModifiers.at(id)));
    }
  }
  if(hasCompletionState){
    if(state_.cellNameMessages.size()>10000||state_.achievements.size()>256){error="Too many completion overrides";return false;}
    Put32(payload,0x314E4D43);Put32(payload,uint32_t(state_.cellNameMessages.size())); // CMN1
    std::vector<uint32_t> cells;for(const auto& e:state_.cellNameMessages)cells.push_back(e.first);
    std::sort(cells.begin(),cells.end());
    for(auto cell:cells){const auto message=state_.cellNameMessages.at(cell);
      if(!catalog_->cellOwnership.count(cell)||!catalog_->pipboy.messages.count(message)||catalog_->pipboy.messages.at(message).title.empty()){error="Invalid cell name override";return false;}
      Put32(payload,cell);Put32(payload,message);
    }
    std::vector<uint32_t> achievements(state_.achievements.begin(),state_.achievements.end());
    std::sort(achievements.begin(),achievements.end());Put32(payload,uint32_t(achievements.size()));
    for(auto id:achievements){if(id>255){error="Invalid achievement";return false;}Put32(payload,id);}
  }
  if(hasLevelAdvancement){
    if(state_.level<catalog_->initial.level||
       state_.level>fo3experience::BaseGameLevelCap||
       state_.experience<fo3experience::ThresholdForLevel(
           state_.level,catalog_->xpBumpBase)){
      error="Invalid committed level";return false;
    }
    Put32(payload,0x3150564c); // LVP1: committed level, after all v17 tails.
    Put32(payload,state_.level);
  }
  if(hasDebugPoints||hasDoorStates){
    if(state_.debugPerkPoints>1000){error="Invalid debug perk budget";return false;}
    Put32(payload,0x31504244);Put32(payload,state_.debugPerkPoints); // DBP1
  }
  if(hasDoorStates){
    if(state_.doorStates.size()>100000){error="Too many door states";return false;}
    Put32(payload,0x31535244);Put32(payload,uint32_t(state_.doorStates.size())); // DRS1
    std::vector<uint32_t> ids;for(const auto& door:state_.doorStates)ids.push_back(door.first);std::sort(ids.begin(),ids.end());
    for(auto id:ids){const auto r=catalog_->references.find(id);if(r==catalog_->references.end()||!r->second.valid||!catalog_->doorBases.count(r->second.base)){error="Invalid door state";return false;}
      Put32(payload,id);payload.push_back(state_.doorStates.at(id)?1:0);}
  }
  if(hasActorVariables){
    Put32(payload,0x31564141);Put32(payload,uint32_t(state_.actorVariables.size())); // AAV1
    std::vector<uint32_t> ids;for(const auto& entry:state_.actorVariables)ids.push_back(entry.first);std::sort(ids.begin(),ids.end());
    for(auto id:ids){if(!catalog_->actorPlacements.count(id)){error="Invalid actor values reference";return false;}
      Put32(payload,id);for(float v:state_.actorVariables.at(id)){if(!std::isfinite(v)){error="Invalid actor value";return false;}PutFloat(payload,v);}}
  }
  if(hasActorVariables){
    std::vector<uint32_t> ids;for(const auto& e:state_.actorSpawnLevels)ids.push_back(e.first);std::sort(ids.begin(),ids.end());
    Put32(payload,uint32_t(ids.size()));for(auto id:ids){Put32(payload,id);Put32(payload,state_.actorSpawnLevels.at(id));}
    std::vector<uint64_t> keys;for(const auto& e:state_.actorTemplateChoices)keys.push_back(e.first);std::sort(keys.begin(),keys.end());
    Put32(payload,uint32_t(keys.size()));for(auto key:keys){Put64(payload,key);Put32(payload,state_.actorTemplateChoices.at(key));}
  }
  Put32(bytes, hasActorVariables ? 21 : hasDoorStates ? 20 : hasDebugPoints ? 19 : hasLevelAdvancement ? 18 : hasCompletionState ? 17 :
      hasMerchantVisits ? 16 : hasPlayerAttributeOverrides ? 15 : 14);
  Put32(bytes, catalog_->fingerprint);
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
      (version < 1 || version > 21))
    return fail("Unsupported player save format");
  if (fo3esm::ReadU32(h + 8) != catalog_->fingerprint)
    return fail("Player save belongs to different game definitions");
  if (fo3esm::ReadU32(h + 12) != bytes.size() - 20)
    return fail("Invalid player save payload size");
  Bytes payload(bytes.begin() + 20, bytes.end());
  if (fo3esm::ReadU32(h + 16) != Crc(payload))
    return fail("Player save checksum failed");
  const auto *p = payload.data();
  State next = catalog_->initial;
  next.referenceEnabled.clear();
  next.referenceStateRevision=state_.referenceStateRevision+1;
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
      !std::isfinite(next.apSpent) || next.apSpent < 0)
    return fail("Invalid saved player resources");
  next.inventory.clear();
  std::unordered_set<uint64_t> ids;
  for (uint32_t i = 0; i < count; ++i) {
    const auto *s = p + 20 + i * 21;
    Stack stack{U64(s), fo3esm::ReadU32(s + 8), I32(s + 12),
                fo3esm::ReadF32(s + 16), s[20] != 0};
    const auto item = catalog_->items.find(stack.formId);
    if (!stack.id || stack.id >= next.nextStackId ||
        !ids.insert(stack.id).second || item == catalog_->items.end() ||
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
          Conflicts(catalog_->items.at(prior.formId), item->second))
        return fail("Conflicting saved equipment");
    next.inventory.push_back(stack);
  }
  next.collected.clear();
  size_t worldEnd = inventoryEnd;
  if (version >= 2) {
    if (payload.size() - inventoryEnd < 8)
      return fail("Missing collected references");
    if (fo3esm::ReadU32(p + inventoryEnd) != catalog_->worldFingerprint)
      return fail("Collected references belong to different world definitions");
    const auto n = fo3esm::ReadU32(p + inventoryEnd + 4);
    worldEnd = inventoryEnd + 8ull + 4ull * n;
    if (n > 100000 || worldEnd > payload.size() ||
        (version == 2 && worldEnd != payload.size()))
      return fail("Invalid collected reference count");
    for (uint32_t i = 0; i < n; ++i) {
      const auto id = fo3esm::ReadU32(p + inventoryEnd + 8 + i * 4);
      if (!id || !catalog_->references.count(id) ||
          !catalog_->items.count(catalog_->references.at(id).base) ||
          !next.collected.insert(id).second)
        return fail("Invalid collected reference");
    }
  }
  next.containers.clear();next.merchantVisits.clear();next.actorDispositionModifiers.clear();next.cellNameMessages.clear();next.achievements.clear();
  if (version >= 3) {
    if (payload.size() - worldEnd < 8 ||
        fo3esm::ReadU32(p + worldEnd) != catalog_->lootFingerprint)
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
      const auto found = catalog_->references.find(ref);
      if (found == catalog_->references.end() ||
          (!catalog_->containers.count(found->second.base)&&!catalog_->actorInventories.count(found->second.base)) ||
          next.containers.count(ref) || size > 10000 ||
          (totalStacks += size) > 30000 || payload.size() - at < 20ull * size)
        return fail("Invalid saved container");
      std::vector<Stack> contents;
      for (uint32_t i = 0; i < size; ++i, at += 20) {
        Stack stack{U64(p + at), fo3esm::ReadU32(p + at + 8), I32(p + at + 12),
                    fo3esm::ReadF32(p + at + 16), false};
        const auto item = catalog_->items.find(stack.formId);
        if (!stack.id || stack.id >= next.nextStackId ||
            !ids.insert(stack.id).second || item == catalog_->items.end() ||
            stack.count <= 0 || !std::isfinite(stack.condition) ||
            stack.condition < 0 || stack.condition > 1 ||
            (item->second.maxCondition == 0 && stack.condition != 1) ||
            (version >= 5 && item->second.kind == ItemKind::Weapon && stack.count != 1))
          return fail("Invalid container stack");
        contents.push_back(stack);
      }
      next.containers.emplace(ref, std::move(contents));
    }
    if (version == 4 && !fo3pipdata::DecodeState(next.pipboy,catalog_->pipboy,p+at,payload.size()-at,error))return false;
    if (version >= 5) {
      if (payload.size() - at < 4) return fail("Missing Pip-Boy length");
      const uint32_t pipSize = fo3esm::ReadU32(p + at);
      at += 4;
      if (pipSize > payload.size() - at ||
          !fo3pipdata::DecodeState(next.pipboy, catalog_->pipboy, p + at, pipSize, error))
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
        if (catalog_->items.at(s.formId).kind == ItemKind::Weapon) weapons.emplace(s.id, &s);
      for (auto &c : next.containers)
        for (auto &s : c.second)
          if (catalog_->items.at(s.formId).kind == ItemKind::Weapon) weapons.emplace(s.id, &s);
      if (number != weapons.size() || payload.size() - at < 12ull * number)
        return fail("Invalid weapon instance count");
      auto setRounds = [&](Stack &s, uint32_t packed) {
        const auto &def = catalog_->items.at(s.formId).weapon;
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
        const auto item = catalog_->items.find(w.instance.formId);
        if (!w.instance.id || w.instance.id >= next.nextStackId ||
            !ids.insert(w.instance.id).second || item == catalog_->items.end() ||
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
          const auto actor=catalog_->pipboy.targets.find(id);
          if(actor==catalog_->pipboy.targets.end()||!catalog_->weapons.actors.count(actor->second.base)||
             !std::isfinite(damage)||damage<0||!next.actorDamage.emplace(id,damage).second)return fail("Invalid actor damage");
        }
      }
      if(version>=7){
        if(payload.size()-at<4)return fail("Missing actor state count");
        const auto n=fo3esm::ReadU32(p+at);at+=4;
        const size_t actorBytes=version>=9?56:version>=8?52:48;
        const size_t tailBytes = version >= 12 ? 28u : version >= 11 ? 20u : version >= 10 ? 8u : 0u;
        if(n>10000||(version>=12 ? payload.size()-at<actorBytes*n+tailBytes : payload.size()-at!=actorBytes*n+tailBytes))return fail("Invalid actor state count");
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
            if(contents!=next.containers.end())for(auto& s:contents->second)if(s.id==a.equippedWeapon&&catalog_->items.at(s.formId).kind==ItemKind::Weapon){s.equipped=true;found=true;}
            if(!found)return fail("Invalid actor equipped weapon");
          }
          const auto ref=catalog_->pipboy.targets.find(id);
          if(ref==catalog_->pipboy.targets.end()||!catalog_->weapons.actors.count(ref->second.base)||
             (!a.cell&&!a.world)||!std::isfinite(a.yaw)||
             !std::all_of(a.position.begin(),a.position.end(),[](float v){return std::isfinite(v);})||
             (a.package&&!catalog_->pipboy.packages.count(a.package))||
             (a.hostile&&a.hostile!=0x14&&!catalog_->pipboy.targets.count(a.hostile))||
             !next.actors.emplace(id,a).second)return fail("Invalid actor state");
        }
      }
      if (version >= 10) {
        if (version>=12 ? payload.size()-at<28u : payload.size()-at != (version>=11 ? 20u : 8u)) return fail("Missing game clock");
        next.gameHour = fo3esm::ReadF32(p+at);
        next.gameDaysPassed = fo3esm::ReadU32(p+at+4);
        if (!std::isfinite(next.gameHour) || next.gameHour < 0.0f ||
            next.gameHour >= 24.0f || next.gameDaysPassed > 1000000u)
          return fail("Invalid saved game clock");
        at += 8;
      }
      if (version >= 11) {
        if (version>=12 ? payload.size()-at<20u : payload.size()-at != 12u) return fail("Missing game calendar");
        next.gameYear = fo3esm::ReadU32(p+at);
        next.gameMonth = fo3esm::ReadU32(p+at+4);
        next.gameDay = fo3esm::ReadU32(p+at+8);
        if (next.gameYear>9999u ||
            !fo3schedule::Valid({int(next.gameYear),int(next.gameMonth),int(next.gameDay)}))
          return fail("Invalid saved game calendar");
        at += 12;
      }
      if(version>=12){
        if(payload.size()-at<8u)return fail("Missing quest scripting save extension");
        next.karma=fo3esm::ReadF32(p+at);at+=4;
        if(!std::isfinite(next.karma)||next.karma<-1000.f||next.karma>1000.f)
          return fail("Invalid saved karma");
        const auto n=fo3esm::ReadU32(p+at);at+=4;
        if(n>10000||(version>=13 ? payload.size()-at<5ull*n+4 : payload.size()-at!=5ull*n))return fail("Invalid quest-object override count");
        for(uint32_t i=0;i<n;++i,at+=5){
          const uint32_t form=fo3esm::ReadU32(p+at),flag=p[at+4];
          const auto item=catalog_->items.find(form);
          if(item==catalog_->items.end()||flag>1||item->second.questItem==bool(flag)||
             !next.questObjectFlags.emplace(form,flag!=0).second)
            return fail("Invalid quest-object override");
        }
      }
      if(version>=13){
        if(payload.size()-at<4)return fail("Missing ownership extension");
        const auto n=fo3esm::ReadU32(p+at);at+=4;
        if(n>30000||(version>=14 ? payload.size()-at<20ull*n+8 : payload.size()-at!=20ull*n))return fail("Invalid ownership extension size");
        std::unordered_map<uint64_t,Stack*> stacks;
        for(auto& container:next.containers)for(auto& stack:container.second)stacks.emplace(stack.id,&stack);
        std::unordered_set<uint64_t> seen;
        for(uint32_t i=0;i<n;++i,at+=20){
          const auto id=U64(p+at);auto found=stacks.find(id);
          if(found==stacks.end()||!seen.insert(id).second)return fail("Invalid ownership stack identity");
          OwnershipData owner;owner.owner=fo3esm::ReadU32(p+at+8);owner.rank=I32(p+at+12);owner.global=fo3esm::ReadU32(p+at+16);owner.explicitOwner=true;
          if(!owner.owner||(!catalog_->factionFlags.count(owner.owner)&&!catalog_->pipboy.dialogueActors.count(owner.owner))||
             (owner.global&&!catalog_->globals.count(owner.global))||
             (catalog_->factionFlags.count(owner.owner)&&owner.global))return fail("Invalid saved item ownership");
          found->second->ownership=owner;
        }
      }
      if(version>=14){
        if(payload.size()-at<8)return fail("Missing lock extension");
        next.experience=fo3esm::ReadU32(p+at);const auto n=fo3esm::ReadU32(p+at+4);at+=8;
        if(n>100000||payload.size()-at<8ull*n)return fail("Invalid lock extension");
        next.locks.clear();
        for(uint32_t i=0;i<n;++i,at+=8){const auto id=fo3esm::ReadU32(p+at);
          LockOverride l;l.change=LockChange(p[at+4]);l.level=p[at+5];l.forceFailures=p[at+6];l.rewarded=p[at+7]!=0;
          auto ref=catalog_->references.find(id);
          if(ref==catalog_->references.end()||!ref->second.valid||p[at+4]>uint8_t(LockChange::ScriptUnlocked)||l.forceFailures>2||p[at+7]>1||
             (l.change==LockChange::Broken&&!l.forceFailures)||(l.change==LockChange::Key&&!ref->second.key)||
             ((l.change==LockChange::Picked||l.change==LockChange::Forced)&&l.level>100)||!next.locks.emplace(id,l).second)return fail("Invalid saved lock");
        }
      }
      if(version>=14&&at<payload.size()){
        if(payload.size()-at<8||fo3esm::ReadU32(p+at)!=0x31454652)return fail("Invalid reference-state extension");
        const auto n=fo3esm::ReadU32(p+at+4);at+=8;
        if(n>100000||(version>=16 ? payload.size()-at<5ull*n+41u : version>=15 ? payload.size()-at!=5ull*n+25u : payload.size()-at!=5ull*n))return fail("Invalid reference-state extension size");
        for(uint32_t i=0;i<n;++i,at+=5){
          const auto id=fo3esm::ReadU32(p+at);const auto ref=catalog_->references.find(id);
          if(ref==catalog_->references.end()||(ref->second.flags&0x20)||catalog_->enableParents.count(id)||p[at+4]>1||
             !next.referenceEnabled.emplace(id,p[at+4]!=0).second)return fail("Invalid saved reference state");
        }
      }
      if(version>=15){
        // Explicit stats extension follows the required RFE1 reference block.
        if((version>=16?payload.size()-at<41u:payload.size()-at!=25u)||fo3esm::ReadU32(p+at)!=0x31545350)
          return fail("Missing/invalid player attributes extension");
        at+=4;
        std::copy_n(p+at,7,next.special.begin());at+=7;
        std::copy_n(p+at,14,next.skills.begin());at+=14;
        if(std::any_of(next.special.begin(),next.special.end(),[](uint8_t v){return v<1||v>10;})||
           std::any_of(next.skills.begin(),next.skills.end(),[](uint8_t v){return v>100;}))
          return fail("Invalid saved player attributes");
      }
      if(version>=16){
        if(payload.size()-at<8||fo3esm::ReadU32(p+at)!=0x31524d42)return fail("Missing merchant visit extension");
        const auto n=fo3esm::ReadU32(p+at+4);at+=8;
        if(n>10000||payload.size()-at<12ull*n+8u)return fail("Invalid merchant visit extension");
        next.merchantVisits.clear();
        for(uint32_t i=0;i<n;++i,at+=12){const auto ref=fo3esm::ReadU32(p+at);
          MerchantVisit visit{fo3esm::ReadU32(p+at+4),fo3esm::ReadF32(p+at+8)};
          const auto placed=catalog_->references.find(ref);
          if(placed==catalog_->references.end()||!catalog_->containers.count(placed->second.base)||
             !next.containers.count(ref)||!std::isfinite(visit.hour)||visit.hour<0||visit.hour>=24||
             visit.day>next.gameDaysPassed||(visit.day==next.gameDaysPassed&&visit.hour>next.gameHour)||
             !next.merchantVisits.emplace(ref,visit).second)return fail("Invalid saved merchant visit");
        }
      }
      if(version>=16){
        if(payload.size()-at<8||fo3esm::ReadU32(p+at)!=0x31524442)return fail("Missing disposition extension");
        const auto n=fo3esm::ReadU32(p+at+4);at+=8;
        if(n>10000||(version>=17?payload.size()-at<8ull*n+12u:payload.size()-at!=8ull*n))return fail("Invalid disposition extension");
        next.actorDispositionModifiers.clear();
        for(uint32_t i=0;i<n;++i,at+=8){const auto id=fo3esm::ReadU32(p+at);const auto actor=catalog_->references.find(id);
          if(actor==catalog_->references.end()||!catalog_->pipboy.dialogueActors.count(actor->second.base)||
             !next.actorDispositionModifiers.emplace(id,I32(p+at+4)).second)return fail("Invalid saved disposition actor");
        }
      }
      if(version>=17){
        if(payload.size()-at<12||fo3esm::ReadU32(p+at)!=0x314E4D43)return fail("Missing cell-name extension");
        const auto n=fo3esm::ReadU32(p+at+4);at+=8;
        if(n>10000||payload.size()-at<8ull*n+4u)return fail("Invalid cell-name count");
        for(uint32_t i=0;i<n;++i,at+=8){const auto cell=fo3esm::ReadU32(p+at),message=fo3esm::ReadU32(p+at+4);
          if(!catalog_->cellOwnership.count(cell)||!catalog_->pipboy.messages.count(message)||catalog_->pipboy.messages.at(message).title.empty()||!next.cellNameMessages.emplace(cell,message).second)return fail("Invalid saved cell name");
        }
        const auto achievements=fo3esm::ReadU32(p+at);at+=4;
        if(achievements>256||(version>=20 ? payload.size()-at<4ull*achievements+24u : version>=18
            ? payload.size()-at!=4ull*achievements+(version>=19?16u:8u)
            : payload.size()-at!=4ull*achievements))
          return fail("Invalid achievement count");
        for(uint32_t i=0;i<achievements;++i,at+=4){const auto id=fo3esm::ReadU32(p+at);
          if(id>255||!next.achievements.insert(id).second)return fail("Invalid saved achievement");}
      }
      if(version>=18){
        if((version>=20?payload.size()-at<24u:payload.size()-at!=(version>=19?16u:8u))||fo3esm::ReadU32(p+at)!=0x3150564c)
          return fail("Invalid level-up extension");
        const auto level=fo3esm::ReadU32(p+at+4);at+=8;
        if((version>=19?level<catalog_->initial.level:level<=catalog_->initial.level)||
           level>fo3experience::BaseGameLevelCap||
           next.experience<fo3experience::ThresholdForLevel(
               uint16_t(level),catalog_->xpBumpBase))
          return fail("Invalid saved player level");
        next.level=uint16_t(level);
      }
      if(version>=19){
        if((version>=20?payload.size()-at<16u:payload.size()-at!=8)||fo3esm::ReadU32(p+at)!=0x31504244||
           fo3esm::ReadU32(p+at+4)>1000)return fail("Invalid debug perk budget");
        next.debugPerkPoints=uint16_t(fo3esm::ReadU32(p+at+4));at+=8;
      }
      if(version>=20){
        if(payload.size()-at<8||fo3esm::ReadU32(p+at)!=0x31535244)return fail("Missing door state extension");
        const auto n=fo3esm::ReadU32(p+at+4);at+=8;
        if(n>100000||(version>=21?payload.size()-at<5ull*n+8:payload.size()-at!=5ull*n))return fail("Invalid door state count");
        for(uint32_t i=0;i<n;++i,at+=5){const auto id=fo3esm::ReadU32(p+at);const auto r=catalog_->references.find(id);
          if(r==catalog_->references.end()||!r->second.valid||!catalog_->doorBases.count(r->second.base)||p[at+4]>1||!next.doorStates.emplace(id,p[at+4]!=0).second)return fail("Invalid saved door state");}
      }
      if(version>=21){
        if(payload.size()-at<8||fo3esm::ReadU32(p+at)!=0x31564141)return fail("Missing actor values");
        const auto n=fo3esm::ReadU32(p+at+4);at+=8;
        if(n>10000||payload.size()-at<44ull*n+8)return fail("Invalid actor value count");
        for(uint32_t i=0;i<n;++i){const auto id=fo3esm::ReadU32(p+at);at+=4;std::array<float,10> values{};
          if(!catalog_->actorPlacements.count(id)||next.actorVariables.count(id))return fail("Invalid actor values reference");
          for(float& v:values){v=fo3esm::ReadF32(p+at);at+=4;if(!std::isfinite(v))return fail("Invalid actor value");}
          next.actorVariables.emplace(id,values);
        }
        if(payload.size()-at<4)return fail("Missing actor spawn levels");
        const auto count=fo3esm::ReadU32(p+at);at+=4;
        if(count>10000||payload.size()-at<8ull*count+4)return fail("Invalid actor spawn levels");
        for(uint32_t i=0;i<count;++i,at+=8){const auto id=fo3esm::ReadU32(p+at),level=fo3esm::ReadU32(p+at+4);
          if(!catalog_->actorPlacements.count(id)||!level||level>20||!next.actorSpawnLevels.emplace(id,uint16_t(level)).second)return fail("Invalid actor spawn level");}
        const auto selections=fo3esm::ReadU32(p+at);at+=4;
        if(selections>100000||payload.size()-at!=12ull*selections)return fail("Invalid actor template count");
        for(uint32_t i=0;i<selections;++i,at+=12){const auto key=U64(p+at);const auto selected=fo3esm::ReadU32(p+at+8);
          const auto list=catalog_->weapons.levelledActors.find(uint32_t(key));
          if(!next.actorSpawnLevels.count(uint32_t(key>>32))||list==catalog_->weapons.levelledActors.end()||
             (selected&&std::none_of(list->second.entries.begin(),list->second.entries.end(),[&](const auto& e){return e.actor==selected;}))||
             !next.actorTemplateChoices.emplace(key,selected).second)return fail("Invalid actor template selection");}

      }
      if(at!=payload.size())return fail("Trailing player save data");
    }
    if (version == 3 && at != payload.size())
      return fail("Trailing container save data");
  }
  // Resource validation must use the restored SPECIAL values, not the ESM
  // baseline. A character with 10 END/AGI may have higher HP/AP limits.
  const auto& rule=catalog_->rules;
  const float maxHealth=next.baseHealth+(next.special[2]+rule.healthEnduranceOffset)*rule.healthEnduranceMult+
      (next.level-1)*rule.healthLevelMult;
  const float maxAp=rule.apBase+next.special[5]*rule.apMult;
  // Legacy v1-v14 catalogues may have zero maximum health.
  if(!std::isfinite(maxHealth)||(version>=15&&maxHealth<=0)||next.healthDamage>maxHealth||
     !std::isfinite(maxAp)||maxAp<0||next.apSpent>maxAp)
    return fail("Invalid saved player resources");
  for(const auto& global:next.pipboy.mutableGlobals) {
    if(!catalog_->globals.count(global.first)||global.first==catalog_->hourGlobal||global.first==catalog_->dayGlobal||global.first==catalog_->monthGlobal||global.first==catalog_->yearGlobal||global.first==catalog_->daysPassedGlobal||global.first==catalog_->timeScaleGlobal)
      return fail("Invalid mutable script global");
  }
  if (version < 5 && !MigrateWeaponInstances(next))
    return fail("Legacy weapon inventory exceeds instance limits");
  state_ = std::move(next);
  pendingKarmaEvents_.clear();pendingTheftEvents_.clear();pendingQuestNotices_.clear();pendingScriptItems_.clear();
  pendingExperienceNotices_.clear();pendingLocationNotices_.clear();
  pendingScriptActions_.clear();scriptTalking_.clear();scriptAnimations_.clear();scriptInsideTriggers_.clear();++scriptRuntimeGeneration_;
  pendingScriptMessages_.clear();pendingScriptSounds_.clear();pendingBarters_.clear();scriptButtons_.clear();EndContainerInteraction();
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

#include "fo3-ownership-state.inc"
#include "fo3-lock-state.inc"
