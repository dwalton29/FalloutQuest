// Original definitions are immutable. All world/UI/script consumers use this overlay.
namespace fo3player {
float Player::LockSetting(const std::string& name,float fallback) const {
  auto s=catalog_->lockSettings.find(name);
  return s!=catalog_->lockSettings.end()&&std::isfinite(s->second)?s->second:fallback;
}
LockInfo Player::GetLock(uint32_t id) const {
  LockInfo out;auto r=catalog_->references.find(id);if(r==catalog_->references.end()||!r->second.valid)return out;
  const auto& ref=r->second;out.known=true;out.locked=ref.locked;out.level=ref.lockLevel;out.flags=ref.lockFlags;out.key=ref.key;
  auto o=state_.locks.find(id);
  if(o!=state_.locks.end()) {const auto& l=o->second;out.level=l.level;out.forceFailures=l.forceFailures;
    out.broken=l.change==LockChange::Broken;out.locked=l.change==LockChange::Locked||out.broken;
  } else out.leveled=bool(ref.lockFlags&4);
  // Leveled locks cannot be safely resolved until the original level formula is recovered.
  return out;
}
int Player::LockRequirement(uint32_t id) const {
  const auto lock=GetLock(id);if(!lock.known||lock.leveled||lock.level>100)return -1;
  for(int n:{0,25,50,75,100})if(lock.level<=n)return n;
  return -1;
}
PickAccess Player::CanPickLock(uint32_t id) const {
  auto r=catalog_->references.find(id);if(r==catalog_->references.end()||!r->second.valid||(r->second.flags&0x20u)||
    catalog_->scriptedBases.count(r->second.base)||
    (catalog_->containers.count(r->second.base)&&(!catalog_->containers.at(r->second.base).valid||catalog_->containers.at(r->second.base).script))||
    (!catalog_->containers.count(r->second.base)&&!catalog_->doorBases.count(r->second.base)))return PickAccess::Unsupported;
  bool disabled=false;std::string error;
  if(!ReferenceDisabled(id,disabled,error)||disabled)return PickAccess::Unsupported;
  if(!ResolveOwnership(id).known)return PickAccess::Unsupported;
  auto l=GetLock(id);if(!l.locked)return PickAccess::Unlocked;
  if(l.leveled)return PickAccess::Unsupported;
  if(LockRequirement(id)<0)return PickAccess::RequiresKey;
  auto perk=state_.pipboy.perks.find(catalog_->infiltrator);
  if(l.broken&&(l.forceFailures>=2||perk==state_.pipboy.perks.end()||!perk->second))return PickAccess::Broken;
  if(state_.skills[4]<LockRequirement(id))return PickAccess::Skill;
  if(!catalog_->bobbyPin||ItemCount(catalog_->bobbyPin)<=0)return PickAccess::NoPins;
  return PickAccess::Ready;
}
int Player::ForceLockChance(uint32_t id) const {
  const int requirement=LockRequirement(id);if(requirement<0)return 0;
  // Fallout3.exe 0x601AD0: trunc(base) + trunc(skill) - tier, clamp 0..100.
  return std::clamp(int(state_.skills[4])+int(LockSetting("fLockSkillBase",10))-requirement,0,100);
}
bool Player::UnlockByKey(uint32_t id) {
  auto l=GetLock(id);if(!l.known||!l.locked||!l.key||ItemCount(l.key)<=0||!CanOpenDoor(id))return false;
  auto& o=state_.locks[id];o.level=l.level;o.change=LockChange::Key;o.forceFailures=l.forceFailures;++revision_;return true;
}
bool Player::CompleteLockpick(uint32_t id,bool forced) {
  if(CanPickLock(id)!=PickAccess::Ready)return false;
  const auto lock=GetLock(id);const int tier=LockRequirement(id)/25;
  auto& o=state_.locks[id];const bool reward=!o.rewarded;o.level=lock.level;o.forceFailures=lock.forceFailures;
  o.change=forced?LockChange::Forced:LockChange::Picked;o.rewarded=true;
  if(reward){
    static const char* tiers[]={"VeryEasy","Easy","Average","Hard","VeryHard"};
    const int xp=std::max(0,int(LockSetting(std::string("iXPRewardPickLock")+tiers[tier],0)));
    state_.experience+=std::min(uint32_t(xp),UINT32_MAX-state_.experience);
    // 0x942BDD..0x942C7C checks original ownership alignment on success.
    // Use the shared v206 resolved owner and feedback queue; no proximity/witness guess.

  }
  const auto owner=ResolveOwnership(id);CommitTheft(id,0,0,0,owner);
  if(owner.Theft()&&!pendingTheftEvents_.empty())pendingTheftEvents_.back().action=forced?CrimeAction::ForceLock:CrimeAction::PickLock;
  ++revision_;return true;
}
bool Player::ForceLock(uint32_t id,uint32_t roll) {
  if(roll>=100||CanPickLock(id)!=PickAccess::Ready)return false;
  if(roll<uint32_t(ForceLockChance(id)))return CompleteLockpick(id,true);
  const auto lock=GetLock(id);auto& o=state_.locks[id];o.level=lock.level;o.change=LockChange::Broken;
  o.forceFailures=uint8_t(std::min(2,int(lock.forceFailures)+1));++revision_;return true;
}
bool Player::BreakBobbyPin(uint32_t id) {
  if(CanPickLock(id)!=PickAccess::Ready)return false;
  for(const auto& s:state_.inventory)if(s.formId==catalog_->bobbyPin&&s.count>0)return Remove(s.id,1);
  return false;
}
bool Player::ScriptLock(uint32_t id,int level) {
  auto l=GetLock(id);if(!l.known||level<0||level>255)return false;
  auto& o=state_.locks[id];o.level=uint8_t(level);o.change=LockChange::Locked;++revision_;return true;
}
bool Player::ScriptUnlock(uint32_t id) {
  auto l=GetLock(id);if(!l.known)return false;
  auto& o=state_.locks[id];o.level=l.level;o.change=LockChange::ScriptUnlocked;++revision_;return true;
}
} // namespace fo3player
