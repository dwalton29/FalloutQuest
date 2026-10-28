// Original definitions are immutable. All world/UI/script consumers use this overlay.
namespace fo3player {
float Player::LockSetting(const std::string& name,float fallback) const {
  auto s=catalog_->lockSettings.find(name);
  return s!=catalog_->lockSettings.end()&&std::isfinite(s->second)?s->second:fallback;
}
LockInfo Player::GetLock(uint32_t id) const {
  LockInfo out;auto r=catalog_->references.find(id);if(r==catalog_->references.end()||!r->second.valid)return out;
  const auto& ref=r->second;out.known=true;out.locked=ref.locked;out.level=ref.lockLevel;out.flags=ref.lockFlags;out.key=ref.key;
  const auto terminal=catalog_->pipboy.terminals.find(ref.base);
  if(terminal!=catalog_->pipboy.terminals.end()){
    const auto& t=terminal->second;out.level=t.difficulty<5?uint8_t(t.difficulty*25):255;
    out.locked=!(t.flags&2);out.leveled=bool(t.flags&1);out.key=t.password;
    if(ref.locked){out.level=ref.lockLevel;out.locked=true;out.leveled=bool(ref.lockFlags&4);}
  }
  auto o=state_.locks.find(id);
  if(o!=state_.locks.end()) {const auto& l=o->second;out.level=l.level;out.forceFailures=l.forceFailures;
    out.broken=l.change==LockChange::Broken;out.locked=l.change==LockChange::Locked||out.broken;
  } else if(terminal==catalog_->pipboy.terminals.end())out.leveled=bool(ref.lockFlags&4);
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
  const auto previous=state_.locks.find(id);
  const bool reward=previous==state_.locks.end()||!previous->second.rewarded;
  if(reward){
    static const char* tiers[]={"VeryEasy","Easy","Average","Hard","VeryHard"};
    const int xp=std::max(0,int(LockSetting(std::string("iXPRewardPickLock")+tiers[tier],0)));
    // Apply the same perk-aware XP modifiers as quest rewards, once per lock.
    // Validate the reward before committing a successful unlock.
    if(xp>0&&!RewardExperience(uint32_t(xp)))return false;
  }
  auto& o=state_.locks[id];o.level=lock.level;o.forceFailures=lock.forceFailures;
  o.change=forced?LockChange::Forced:LockChange::Picked;o.rewarded=true;
  // 0x942BDD..0x942C7C checks original ownership alignment on success.
  // Use the shared v206 resolved owner and feedback queue; no witness guess.
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
bool Player::ScriptDoorState(uint32_t id,bool open){
  auto reference=catalog_->references.find(id);
  if(reference==catalog_->references.end()||!reference->second.valid||!catalog_->doorBases.count(reference->second.base))return false;
  const auto existing=state_.doorStates.find(id);if(existing!=state_.doorStates.end()&&existing->second==open)return true;
  state_.doorStates[id]=open;++revision_;return true;
}
bool Player::QueueTerminalBack(uint32_t id){
  auto reference=catalog_->references.find(id);
  if(reference==catalog_->references.end()||!catalog_->pipboy.terminals.count(reference->second.base)||pendingTerminalBack_.size()>=16)return false;
  pendingTerminalBack_.push_back(id);++scriptEffectRevision_;return true;
}
bool Player::PollTerminalBack(uint32_t& id){
  if(pendingTerminalBack_.empty())return false;id=pendingTerminalBack_.front();pendingTerminalBack_.erase(pendingTerminalBack_.begin());++scriptEffectRevision_;return true;
}
int Player::TerminalRequirement(uint32_t id) const {
  auto r=catalog_->references.find(id);if(r==catalog_->references.end())return -1;
  auto t=catalog_->pipboy.terminals.find(r->second.base);
  if(t==catalog_->pipboy.terminals.end()||!t->second.valid)return -1;
  auto lock=GetLock(id);int tier=lock.level>100?5:(lock.level+24)/25;
  // Fallout3.exe 005B3EF0: base tier + trunc(level * fHackLevelMult).
  if(lock.leveled)tier=std::min(4,tier+int(state_.level*LockSetting("fHackLevelMult",.25f)));
  return tier<5?tier*25:-1;
}
bool Player::TerminalPassword(uint32_t id) const {
  const auto lock=GetLock(id);return lock.known&&lock.key&&ItemCount(lock.key)>0;
}
bool Player::CanHackTerminal(uint32_t id) const {
  const int requirement=TerminalRequirement(id);if(requirement<0||state_.skills[8]<requirement)return false;
  const auto lock=GetLock(id);if(!lock.locked)return false;
  bool disabled=false;std::string error;if(!ReferenceDisabled(id,disabled,error)||disabled)return false;
  if(lock.broken){
    auto named=catalog_->pipboy.formNames.find("computerwhiz");
    if(named==catalog_->pipboy.formNames.end()||lock.forceFailures>=2)return false;
    auto perk=state_.pipboy.perks.find(named->second);if(perk==state_.pipboy.perks.end()||!perk->second)return false;
  }
  return true;
}
bool Player::OpenTerminalPassword(uint32_t id){
  if(!TerminalPassword(id))return false;
  auto r=catalog_->references.find(id);if(r==catalog_->references.end()||!catalog_->pipboy.terminals.count(r->second.base))return false;
  return ScriptUnlock(id);
}
bool Player::CompleteTerminalHack(uint32_t id){
  if(!CanHackTerminal(id))return false;
  const auto l=GetLock(id);auto previous=state_.locks.find(id);
  const bool rewarded=previous!=state_.locks.end()&&previous->second.rewarded;
  if(!rewarded){static const char* tiers[]={"VeryEasy","Easy","Average","Hard","VeryHard"};
    const auto xp=uint32_t(std::max(0.f,LockSetting(std::string("iXPRewardHackComputer")+tiers[TerminalRequirement(id)/25],20+10*(TerminalRequirement(id)/25))));
    if(xp&&!RewardExperience(xp))return false;}
  auto& o=state_.locks[id];o.change=LockChange::ScriptUnlocked;o.level=l.level;o.forceFailures=l.forceFailures;o.rewarded=true;
  const auto owner=ResolveOwnership(id);CommitTheft(id,0,0,0,owner);
  if(owner.Theft()&&!pendingTheftEvents_.empty())pendingTheftEvents_.back().action=CrimeAction::Hack;
  ++revision_;return true;
}
bool Player::FailTerminalHack(uint32_t id){
  if(!CanHackTerminal(id))return false;
  const auto l=GetLock(id);auto& o=state_.locks[id];o.change=LockChange::Broken;o.level=l.level;
  o.forceFailures=uint8_t(std::min(2,int(l.forceFailures)+1));++revision_;return true;
}
} // namespace fo3player
