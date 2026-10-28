#pragma once
#include "player/fo3-player-state.h"
namespace fo3weapon {
// One accessible representative per body slot. Preserve exact owned instances;
// switching the active gun must not make the other body slot disappear.
template<class Ready>
const fo3player::Stack* Holstered(const fo3player::Player& player,bool longGun,
                                uint64_t active,uint64_t remembered,Ready&& ready){
  const fo3player::Stack* best=nullptr;
  for(const auto& stack:player.Snapshot().inventory){
    const auto item=player.Definitions().items.find(stack.formId);
    if(item==player.Definitions().items.end()||!item->second.weapon.Firearm()||
       item->second.weapon.LongGun()!=longGun||!ready(stack.formId))continue;
    if(stack.id==active)return &stack;
    if(!best||stack.id==remembered||(best->id!=remembered&&stack.id<best->id))best=&stack;
  }
  return best;
}
}
