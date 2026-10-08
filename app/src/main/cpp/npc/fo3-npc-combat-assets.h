#pragma once
// Source-backed NPC combat preparation from original NPC_ CNTO/LVLI records.
// Enumerate only WEAP leaves. The actual owned instance, levelled roll and
// ammunition are always selected from the canonical Player inventory later.
#include "fo3-npc.h"
#include "player/fo3-player-state.h"
#include <array>
#include <cstdint>
#include <functional>
#include <unordered_set>
#include <vector>

namespace fo3npc {
inline std::vector<uint32_t> CombatWeaponCandidates(
    const Fo3NpcActorQ230& source, const fo3player::Catalog& catalog,
    size_t limit=32u) {
  std::vector<uint32_t> forms;
  std::unordered_set<uint32_t> traversed, unique;
  size_t visited=0;
  std::function<void(uint32_t,unsigned)> walk=[&](uint32_t form,unsigned depth){
    if(!form||depth>16||visited++>4096||forms.size()>=limit)return;
    const auto item=catalog.items.find(form);
    if(item!=catalog.items.end()){
      if(item->second.kind==fo3player::ItemKind::Weapon&&
         item->second.weapon.valid&&unique.insert(form).second)
        forms.push_back(form);
      return;
    }
    if(!traversed.insert(form).second)return; // Shared lists and cycles.
    const auto list=catalog.lootLists.find(form);
    if(list==catalog.lootLists.end()||!list->second.valid)return;
    for(const auto& entry:list->second.entries)
      if(entry.count>0)walk(entry.form,depth+1);
  };
  for(const auto& entry:source.inventory)
    if(entry.count>0)walk(entry.formId,0);
  return forms;
}
// xEdit/ESM IDLE EDIDs. These are resolved through the catalog's authored
// EDID -> IDLE MODL lookup; no KF filenames or animations are invented.
inline constexpr std::array<std::array<const char*,3>,6> HitIdleNames{{
  {{"mthitheada","mthitheadb","mthitheadc"}},
  {{"mthittorsoa","mthittorsob","mthittorsoc"}},
  {{"mthitarmleft","mthitarmleftb","mthitarmleftc"}},
  {{"mthitarmright","mthitarmrightb","mthitarmrightc"}},
  {{"mthitlegleft","mthitlegleftb","mthitlegleftc"}},
  {{"mthitlegright","mthitlegrightb","mthitlegrightc"}}
}};
inline size_t HitRegionSlot(uint8_t region) {
  return region==1?0u:region>=2&&region<=6?size_t(region-1):1u;
}
// Cycle available authored variants per hit, not per render frame. If some
// optional Bethesda KF is absent, use a valid sibling without fabricating one.
inline int HitVariant(const std::array<bool,3>& available,uint32_t serial){
  for(unsigned j=0;j<3;++j) {
    const unsigned candidate=(serial+j)%3u;
    if(available[candidate])return int(candidate);
  }
  return -1;
}
} // namespace fo3npc
