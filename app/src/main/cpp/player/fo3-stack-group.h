#pragma once
#include "fo3-player-state.h"
#include <map>
#include <tuple>
namespace fo3player {
inline auto InventoryGroup(const Stack& s){
  const auto& o=s.ownership;
  return std::make_tuple(s.formId,s.condition,s.loadedRounds,s.needsAction,s.equipped,s.equipmentLocked,
      o.owner,o.global,o.rank,o.explicitOwner,o.valid,s.scriptInstance?s.id:uint64_t(0));
}
using InventoryGroupKey=decltype(InventoryGroup(Stack{}));
}
