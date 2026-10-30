#include "player/fo3-player-state.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace fo3player {
namespace {
float Damage(const fo3weapon::Definition& d,const fo3weapon::Definitions& r,float skill,float condition) {
  return d.damage*(r.skillBase+r.skillMult*std::clamp(skill,0.f,100.f)/100)*(r.conditionBase+r.conditionMult*condition);
}
Stack *Find(State &state, uint64_t id) {
  for (auto &s : state.inventory)
    if (s.id == id) return &s;
  return nullptr;
}
}
const Stack *Player::Weapon(uint64_t id) const {
  for (const auto &s : state_.inventory)
    if (s.id == id && catalog_->items.at(s.formId).kind == ItemKind::Weapon)
      return &s;
  return nullptr;
}
const Stack *Player::EquippedWeapon() const {
  for (const auto &s : state_.inventory)
    if (s.equipped && catalog_->items.at(s.formId).kind == ItemKind::Weapon)
      return &s;
  return nullptr;
}
int32_t Player::AmmoReserve(uint32_t ammo) const {
  const auto item = catalog_->items.find(ammo);
  if (item == catalog_->items.end() || item->second.kind != ItemKind::Ammo) return 0;
  int64_t count = 0;
  for (const auto &s : state_.inventory)
    if (s.formId == ammo) count += s.count;
  return static_cast<int32_t>(std::min<int64_t>(count, INT32_MAX));
}
bool Player::PickupWeapon(uint32_t reference) {
  if (!CanPickup(reference)) return false;
  const auto &ref = catalog_->references.at(reference);
  const auto &item = catalog_->items.at(ref.base);
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
  const auto &item = catalog_->items.at(s->formId);
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
  const auto item = catalog_->items.find(it->instance.formId);
  if (item == catalog_->items.end() || item->second.kind != ItemKind::Weapon ||
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
  const auto &item = catalog_->items.at(found->formId);
  const auto &def = item.weapon;
  if (!found->equipped || !def.Firearm() || found->needsAction ||
      found->condition <= 0 || found->loadedRounds < def.ammoUse) return false;
  auto *s = Find(state_, id);
  s->loadedRounds -= def.ammoUse;
  float degradation = catalog_->weapons.damageGun;
  if (def.skill == 34) degradation = catalog_->weapons.damageEnergy;
  if (def.animation >= 8) degradation = catalog_->weapons.damageLauncher;
  if (def.flags2 & 128) degradation = def.degradationOverride;
  if (item.maxCondition > 0 && std::isfinite(degradation) && degradation >= 0)
    s->condition = std::max(0.0f, s->condition - def.damage * degradation / item.maxCondition);
  ++revision_;
  return true;
}
bool Player::CanReloadWeapon(uint64_t id)const{
  const auto* s=Weapon(id);if(!s||!s->equipped||s->condition<=0)return false;
  const auto& d=catalog_->items.at(s->formId).weapon;
  return d.Firearm()&&(s->needsAction||s->loadedRounds<d.clip)&&(AmmoReserve(d.ammo)>0||(s->needsAction&&s->loadedRounds));
}
bool Player::ReloadWeapon(uint64_t id){
  if(!CanReloadWeapon(id))return false;
  Player candidate=*this;const auto* current=candidate.Weapon(id);
  if((!current->needsAction||current->loadedRounds)&&!candidate.EjectMagazine(id))return false;
  if(!candidate.LoadMagazine(id)||!candidate.ChamberWeapon(id))return false;
  state_=std::move(candidate.state_);++revision_;return true;
}
bool Player::EjectMagazine(uint64_t id) {
  const auto *s = Weapon(id);
  if (!s || !s->equipped || (s->needsAction && !s->loadedRounds)) return false;
  const auto &def = catalog_->items.at(s->formId).weapon;
  const auto ammo = catalog_->items.find(def.ammo);
  if (!def.Firearm() || ammo == catalog_->items.end() || ammo->second.kind != ItemKind::Ammo)
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
  const auto &def = catalog_->items.at(s->formId).weapon;
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
      !catalog_->items.at(s->formId).weapon.Firearm()) return false;
  Find(state_, id)->needsAction = false;
  ++revision_;
  return true;
}
bool Player::BootstrapDevelopmentWeapon() {
  if (state_.developmentWeaponGranted) return true;
  constexpr uint32_t pistol = 0x434f;
  const auto item = catalog_->items.find(pistol);
  if (item == catalog_->items.end() || !item->second.weapon.Firearm() ||
      item->second.editorId != "Weap10mmPistol") return false;
  // Transaction on a copy: failed capacity/ammo/definition checks change nothing.
  Player transaction = *this;
  uint64_t id = 0;
  for (const auto &s : transaction.state_.inventory)
    if (s.formId == pistol && (!id || s.condition > transaction.Weapon(id)->condition)) id = s.id;
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
      if (catalog_->items.at(s.formId).kind == ItemKind::Weapon)
        extra += static_cast<size_t>(s.count - 1);
    if (extra > limit - original || extra > UINT64_MAX - state.nextStackId) return false;
    stacks.reserve(original + extra);
    for (size_t i = 0; i < original; ++i) {
      if (catalog_->items.at(stacks[i].formId).kind != ItemKind::Weapon) continue;
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

namespace fo3player {
float Player::WeaponDamage(uint64_t id) const {
  const auto* stack=Weapon(id);return stack?WeaponDamage(*stack):0;
}
float Player::WeaponDamage(const Stack& stack) const {
  const auto item=catalog_->items.find(stack.formId);if(item==catalog_->items.end()||item->second.kind!=ItemKind::Weapon)return 0;
  const auto& d=item->second.weapon;const auto& r=catalog_->weapons;
  const float skill=d.skill>=32&&d.skill<=45?std::min<float>(100,state_.skills[d.skill-32]):0;
  return Damage(d,r,skill,stack.condition);
}

uint32_t Player::ActorTemplate(uint32_t reference,uint32_t list)const {
  const auto it=state_.actorTemplateChoices.find((uint64_t(reference)<<32)|list);
  return it==state_.actorTemplateChoices.end()?0:it->second;
}
const fo3weapon::Definitions::Actor* Player::ActorStatistics(uint32_t reference)const {
  const auto target=catalog_->pipboy.targets.find(reference);if(target==catalog_->pipboy.targets.end())return nullptr;
  if(!state_.actorSpawnLevels.count(reference))return fo3weapon::ActorStatistics(catalog_->weapons,target->second.base);
  return fo3weapon::ActorStatistics(catalog_->weapons,target->second.base,[&](uint32_t list){return ActorTemplate(reference,list);});
}
const fo3pipdata::ActorDefinition* Player::ActorCategory(uint32_t reference,uint16_t category)const {
  const auto target=catalog_->pipboy.targets.find(reference);if(target==catalog_->pipboy.targets.end())return nullptr;
  if(!state_.actorSpawnLevels.count(reference))return fo3pipdata::ActorCategory(catalog_->pipboy,target->second.base,category);
  return fo3pipdata::ActorCategory(catalog_->pipboy,target->second.base,category,[&](uint32_t list){return ActorTemplate(reference,list);});
}
uint32_t Player::ActorVoice(uint32_t reference)const{
  const auto* a=ActorCategory(reference,1);if(!a)return 0;if(a->voice)return a->voice;
  const auto r=catalog_->pipboy.raceVoices.find(a->race);return r==catalog_->pipboy.raceVoices.end()?0:r->second[a->female?1:0];
}
bool Player::RealizeActor(uint32_t reference){
  const auto placed=catalog_->actorPlacements.find(reference);if(placed==catalog_->actorPlacements.end())return false;
  if(state_.actorSpawnLevels.count(reference))return ActorStatistics(reference)!=nullptr;
  std::unordered_map<uint64_t,uint32_t> choices;std::unordered_set<uint32_t> visiting;
  const uint16_t level=state_.level;
  std::function<bool(uint32_t,unsigned)> visit=[&](uint32_t id,unsigned depth){
    if(depth>=32||!visiting.insert(id).second)return false;
    const auto actor=catalog_->weapons.actors.find(id);
    bool valid=true;
    if(actor!=catalog_->weapons.actors.end()){
      if(actor->second.templateId&&actor->second.templates)valid=visit(actor->second.templateId,depth+1);
    }else{
      const auto entry=catalog_->weapons.levelledActors.find(id);
      if(entry==catalog_->weapons.levelledActors.end()||!entry->second.valid)return false;
      const auto& list=entry->second;if(list.flags&~3u)return false;
      float chance=list.chanceNone;if(list.chanceGlobal&&!GlobalValue(list.chanceGlobal,chance))return false;
      if(!std::isfinite(chance)||chance<0||chance>100)return false;
      uint32_t hash=reference^id;hash^=hash>>16;hash*=0x7feb352d;hash^=hash>>15;hash*=0x846ca68b;hash^=hash>>16;
      std::vector<uint32_t> eligible;uint16_t best=0;
      for(const auto& e:list.entries)if(e.level<=level){if(e.count!=1)return false;best=std::max(best,e.level);}
      for(const auto& e:list.entries)if(e.level<=level&&((list.flags&1)||e.level==best))eligible.push_back(e.actor);
      const uint32_t selected=(hash%100u<uint32_t(chance)||eligible.empty())?0:eligible[(hash/100u)%eligible.size()];
      choices[(uint64_t(reference)<<32)|id]=selected;
      if(selected)valid=visit(selected,depth+1);
    }
    visiting.erase(id);return valid;
  };
  if(!visit(placed->second.base,0))return false;
  state_.actorSpawnLevels[reference]=level;
  state_.actorTemplateChoices.insert(choices.begin(),choices.end());++revision_;
  return ActorStatistics(reference)!=nullptr;
}
bool Player::ActorVariable(uint32_t reference,uint32_t slot,float& value)const {
  if(slot>=10||!catalog_->actorPlacements.count(reference))return false;
  const auto it=state_.actorVariables.find(reference);value=it==state_.actorVariables.end()?0:it->second[slot];return true;
}
bool Player::SetActorVariable(uint32_t reference,uint32_t slot,float value){
  float old=0;if(!std::isfinite(value)||!ActorVariable(reference,slot,old))return false;
  if(old!=value){state_.actorVariables[reference][slot]=value;++revision_;}return true;
}
bool Player::KillActor(uint32_t reference){
  if(Essential(reference)||ActorHealth(reference)<0)return false;
  auto it=state_.actors.find(reference);if(it==state_.actors.end())return false;
  auto state=it->second;state.dead=true;state.hostile=0;return UpdateActor(reference,state);
}
bool Player::SetFactionRelation(uint32_t from,uint32_t to,bool ally,bool firstFlag,bool secondFlag){
  if(!catalog_->factionFlags.count(from)||!catalog_->factionFlags.count(to))return false;
  bool changed=false;
  auto set=[&](uint32_t a,uint32_t b,bool flag){const uint64_t key=(uint64_t(a)<<32)|b;
    const uint8_t reaction=ally?(flag?3:2):(flag?0:1);
    const auto old=state_.factionRelations.find(key);
    if(old==state_.factionRelations.end()||old->second!=reaction){state_.factionRelations[key]=reaction;changed=true;}};
  set(from,to,firstFlag);set(to,from,secondFlag);if(changed)++revision_;return true;
}
bool Player::SetEssential(uint32_t base,bool essential){
  if(!catalog_->weapons.actors.count(base))return false;
  const auto old=state_.essentialFlags.find(base);
  if(old==state_.essentialFlags.end()||old->second!=essential){state_.essentialFlags[base]=essential;++revision_;}
  return true;
}
bool Player::ActorAIValue(uint32_t reference,uint32_t valueId,float& value) const {
  if(valueId!=0&&valueId!=57)return false;
  const auto found=state_.actorAIValues.find((uint64_t(reference)<<32)|valueId);
  if(found!=state_.actorAIValues.end())value=found->second;
  else{
    const auto* actor=ActorCategory(reference,8);
    if(!actor||actor->aiData.size()!=20)return false;
    value=actor->aiData[valueId==0?0:14];
  }
  return std::isfinite(value)&&value>=0&&value<=(valueId==0?3:2);
}
bool Player::SetActorAIValue(uint32_t reference,uint32_t valueId,float value){
  float old=0;if(!catalog_->actorPlacements.count(reference)||!std::isfinite(value)||std::floor(value)!=value||value<0||value>(valueId==0?3:2)||
      !ActorAIValue(reference,valueId,old))return false;
  if(old!=value){state_.actorAIValues[(uint64_t(reference)<<32)|valueId]=value;++revision_;}
  return true;
}
bool Player::ResetActorHealth(uint32_t reference) {
  if(reference==PlayerRef)return Health()<=0||RestoreHealth(MaxHealth());
  const float health=ActorHealth(reference);
  if(health<0)return false;
  if(health==0)return true;
  // Preserve death, combat and package state. ResetHealth is not Resurrect.
  // The original also restores condition AVs; no limb damage state exists yet.
  if(state_.actorDamage.erase(reference))++revision_;
  return true;
}
float Player::ActorHealth(uint32_t reference) const {
  if(reference==0x14)return Health();
  const auto saved=state_.actors.find(reference);
  if(saved!=state_.actors.end()&&saved->second.dead)return 0;
  const auto target=catalog_->pipboy.targets.find(reference);
  if(target==catalog_->pipboy.targets.end())return -1;
  const auto* actor=ActorStatistics(reference);if(!actor)return -1;
  const auto&a=*actor;
  float health=a.health;
  if(!a.creature&&(a.flags&0x10)){
    float level=a.level;
    if(a.flags&0x80){level=state_.level*(a.level/1000.f);level=std::max<float>(a.minLevel,level);if(a.maxLevel)level=std::min<float>(a.maxLevel,level);}
    health+=a.endurance*catalog_->weapons.npcHealthEndurance+level*catalog_->weapons.npcHealthLevel;
  }
  const auto damage=state_.actorDamage.find(reference);
  return std::max(0.f,health-(damage==state_.actorDamage.end()?0:damage->second));
}
bool Player::WeaponHit(uint64_t instance,uint32_t target,float fraction) {
  const auto*s=Weapon(instance);
  if(!s||!s->equipped||!std::isfinite(fraction)||fraction<=0||fraction>1)return false;
  return ApplyWeaponHit(s->formId,target,WeaponDamage(instance)*fraction);
}
bool Player::ApplyWeaponHit(uint32_t base,uint32_t target,float damage) {
  return ApplyAttack(0x14,base,target,damage);
}
bool Player::Essential(uint32_t reference) const {
  const auto t=catalog_->pipboy.targets.find(reference);if(t==catalog_->pipboy.targets.end())return false;
  // Essential belongs to the actor's Traits category, not inherited Stats.
  uint32_t base=t->second.base;
  for(unsigned depth=0;depth<32;++depth){
    const auto a=catalog_->weapons.actors.find(base);
    if(a==catalog_->weapons.actors.end()){base=ActorTemplate(reference,base);if(!base)return false;continue;}
    if(a->second.templates&1){base=a->second.templateId;continue;}
    const auto scripted=state_.essentialFlags.find(base);
    return scripted==state_.essentialFlags.end()?(a->second.flags&2)!=0:scripted->second;
  }
  return false;
}
float Player::DamageResistance(uint32_t target) const {
  const auto* contents=target==0x14?&state_.inventory:ContainerContents(target);
  if(!contents)return 0;
  float dr=0;uint32_t mask=0;
  for(const auto& s:*contents){const auto& i=catalog_->items.at(s.formId);
    if(i.kind!=ItemKind::Armour||(target==0x14&&!s.equipped)||(mask&i.bipedMask))continue;
    const auto found=catalog_->weapons.armourDR.find(s.formId);
    if(found!=catalog_->weapons.armourDR.end()){mask|=i.bipedMask;dr+=found->second;}
  }
  // Condition/skill/effects modifying armour rating are not yet supported.
  return std::clamp(dr,0.f,catalog_->weapons.drMax);
}
bool Player::ApplyAttack(uint32_t attacker,uint32_t base,uint32_t target,float damage) {
  const auto weapon=catalog_->items.find(base);const float health=ActorHealth(target);
  if(attacker==target||weapon==catalog_->items.end()||!weapon->second.weapon.valid||health<=0||!std::isfinite(damage)||damage<=0)return false;
  if(attacker!=0x14&&ActorHealth(attacker)<0)return false;
  damage*=1-DamageResistance(target)/100;
  // Essential actors retain health until unconscious behaviour is supported.
  const float applied=std::min(damage,std::max(0.f,health-(Essential(target)?1.f:0.f)));
  if(applied<=0)return false;
  if(target==0x14)return DamageHealth(applied);
  state_.actorDamage[target]+=applied;
  SetActorHostile(target,attacker);
  const bool died=applied>=health&&!Essential(target);
  if(died){if(auto* actor=state_.actors.Edit(target)){actor->dead=true;actor->packageWaitSeconds=0;}}
  ++revision_;
  // Source scripts receive gameplay events once, after the canonical damage
  // transition. Unsupported scripts never undo physical combat damage.
  std::string scriptError;
  DispatchReferenceEvent(target,"OnHit",attacker,scriptError);
  if(died)DispatchReferenceEvent(target,"OnDeath",attacker,scriptError);
  return true;
}
bool Player::SetCorpsePose(uint32_t reference,const CorpsePose& pose){
 const auto actor=state_.actors.find(reference);
 if(actor==state_.actors.end()||!actor->second.dead||ActorHealth(reference)!=0)return false;
 if(actor->second.corpse&&*actor->second.corpse==pose)return true;
 if(!ValidCorpsePose(pose))return false;
 state_.actors.Edit(reference)->corpse=std::make_shared<const CorpsePose>(pose);++revision_;++actorPoseRevisions_;return true;
}
bool Player::UpdateActor(uint32_t reference,const ActorState& incoming) {
  ActorState a=incoming;
  const auto prior=state_.actors.find(reference);
  if(a.dead&&!a.corpse&&prior!=state_.actors.end())a.corpse=prior->second.corpse;
  const auto t=catalog_->pipboy.targets.find(reference);
  if(t==catalog_->pipboy.targets.end()||!catalog_->weapons.actors.count(t->second.base)||
     (!a.cell&&!a.world)||!std::isfinite(a.yaw)||!std::isfinite(a.packageWaitSeconds)||a.packageWaitSeconds<0||
     !std::all_of(a.position.begin(),a.position.end(),[](float v){return std::isfinite(v);})||
     (a.package&&!catalog_->pipboy.packages.count(a.package))||
     (a.hostile&&a.hostile!=0x14&&!catalog_->pipboy.targets.count(a.hostile)))return false;
  const auto old=state_.actors.find(reference);
  bool poseOnly=false;
  if(old!=state_.actors.end()){const auto& o=old->second;
    if(o.cell==a.cell&&o.world==a.world&&o.position==a.position&&o.yaw==a.yaw&&o.package==a.package&&o.sequence==a.sequence&&o.hostile==a.hostile&&o.equippedWeapon==a.equippedWeapon&&o.dead==a.dead&&o.packageWaitSeconds==a.packageWaitSeconds&&o.corpse==a.corpse)return true;
    poseOnly=o.cell==a.cell&&o.world==a.world&&o.package==a.package&&o.hostile==a.hostile&&o.equippedWeapon==a.equippedWeapon&&o.dead==a.dead;
  }else if(state_.actors.size()>=10000)return false;
  state_.actors[reference]=a;++revision_;if(poseOnly)++actorPoseRevisions_;return true;
}
bool Player::MoveActorTo(uint32_t reference,uint32_t destination,const std::array<float,3>& offset) {
  // Original MoveToMarker/MoveTo (109E) copies the destination CELL, position
  // and rotation, then adds world-axis offsets. Canonical actor state already
  // persists across residency; it must change before any native pose is touched.
  const auto source=catalog_->pipboy.targets.find(reference),target=catalog_->pipboy.targets.find(destination);
  if(reference==PlayerRef||destination==PlayerRef||source==catalog_->pipboy.targets.end()||
     target==catalog_->pipboy.targets.end()||!catalog_->actorPlacements.count(reference)||
     (source->second.flags&0x20u)||(target->second.flags&0x20u)||
     !std::all_of(offset.begin(),offset.end(),[](float x){return std::isfinite(x);}))return false;
  ActorState moved;const auto old=state_.actors.find(reference);
  if(old!=state_.actors.end())moved=old->second;
  moved.cell=target->second.cell;moved.world=target->second.world;
  moved.position={target->second.x,target->second.y,target->second.z};moved.yaw=target->second.rz;
  const auto live=state_.actors.find(destination);
  if(live!=state_.actors.end()){moved.cell=live->second.cell;moved.world=live->second.world;moved.position=live->second.position;moved.yaw=live->second.yaw;}
  for(size_t i=0;i<3;++i)moved.position[i]+=offset[i];
  moved.sequence=0;moved.packageWaitSeconds=0;
  if(!UpdateActor(reference,moved))return false;
  // Repeated MoveTo to the same coordinates still invalidates an old route.
  // This signal is copied/swapped with script transactions. Saved ActorState
  // supplies the destination after reload; transient epochs are cleared there.
  scriptActorMoves_[reference]=++revision_;return true;
}
bool Player::SetActorHostile(uint32_t reference,uint32_t target) {
  const auto t=catalog_->pipboy.targets.find(reference);if(t==catalog_->pipboy.targets.end())return false;
  ActorState a;const auto old=state_.actors.find(reference);
  if(old!=state_.actors.end())a=old->second;
  else {a.cell=t->second.cell;a.world=t->second.world;a.position={t->second.x,t->second.y,t->second.z};}
  a.hostile=target;return UpdateActor(reference,a);
}
const Stack* Player::ActorWeapon(uint32_t reference) const {
  const auto* c=ContainerContents(reference);if(!c)return nullptr;
  for(const auto& s:*c)if(s.equipped&&catalog_->items.at(s.formId).kind==ItemKind::Weapon)return &s;
  return nullptr;
}
bool Player::EquipActorWeapon(uint32_t reference,uint64_t instance) {
  if(ActorHealth(reference)<=0)return false;
  auto c=state_.containers.find(reference);if(c==state_.containers.end())return false;
  auto s=std::find_if(c->second.begin(),c->second.end(),[&](const Stack& v){return v.id==instance;});
  if(s==c->second.end()||catalog_->items.at(s->formId).kind!=ItemKind::Weapon||s->condition<=0)return false;
  for(auto& v:c->second)if(catalog_->items.at(v.formId).kind==ItemKind::Weapon)v.equipped=v.id==instance;
  ActorState a;const auto old=state_.actors.find(reference);
  if(old!=state_.actors.end())a=old->second;else{const auto& t=catalog_->pipboy.targets.at(reference);a.cell=t.cell;a.world=t.world;a.position={t.x,t.y,t.z};}
  a.equippedWeapon=instance;UpdateActor(reference,a);++revision_;return true;
}
float Player::ActorWeaponDamage(uint32_t reference) const {
  const auto* s=ActorWeapon(reference);const auto t=catalog_->pipboy.targets.find(reference);
  if(!s||t==catalog_->pipboy.targets.end())return 0;
  const auto* a=ActorStatistics(reference);if(!a)return 0;
  const auto& d=catalog_->items.at(s->formId).weapon;const auto& r=catalog_->weapons;
  const float skill=d.skill>=32&&d.skill<=45?std::min<float>(100,a->skills[d.skill-32]):0;
  return Damage(d,r,skill,s->condition);
}
bool Player::ReloadActorWeapon(uint32_t reference,uint64_t instance) {
  const auto* w=ActorWeapon(reference);if(!w||w->id!=instance||ActorHealth(reference)<=0)return false;
  const auto& d=catalog_->items.at(w->formId).weapon;if(!d.Firearm()||w->loadedRounds>=d.clip)return false;
  auto& contents=state_.containers.at(reference);int reserve=0;
  for(const auto& s:contents)if(s.formId==d.ammo)reserve+=std::min(s.count,10000);
  // WEAP DNAM Flags2 bit 1 explicitly opts NPCs into finite ammunition.
  // Otherwise at least one compatible round authorizes an engine magazine;
  // those virtual rounds never become lootable inventory ammunition.
  const bool finite=(d.flags2&2)!=0;
  int remaining=finite?std::min<int>(d.clip-w->loadedRounds,reserve):(reserve?d.clip-w->loadedRounds:0);
  const int loaded=remaining;if(!loaded)return false;
  if(finite)for(auto& s:contents)if(s.formId==d.ammo){int used=std::min(remaining,s.count);s.count-=used;remaining-=used;}
  for(auto& s:contents)if(s.id==instance){s.loadedRounds+=loaded;s.needsAction=false;}
  contents.erase(std::remove_if(contents.begin(),contents.end(),[](const Stack&s){return s.count==0;}),contents.end());
  ++revision_;return true;
}
bool Player::FireActorWeapon(uint32_t reference,uint64_t instance) {
  const auto* w=ActorWeapon(reference);if(!w||w->id!=instance||w->condition<=0||w->needsAction||ActorHealth(reference)<=0)return false;
  const auto& d=catalog_->items.at(w->formId).weapon;
  if(!d.Firearm()||w->loadedRounds<d.ammoUse)return false;
  for(auto& s:state_.containers.at(reference))if(s.id==instance){s.loadedRounds-=d.ammoUse;++revision_;return true;}
  return false;
}
}
