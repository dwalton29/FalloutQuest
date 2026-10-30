// Shared source-driven legality. Public CELL is a trespass flag only.
namespace fo3player {
namespace {
const fo3pipdata::ActorDefinition* OwnershipActor(const Catalog& c,uint32_t id,uint16_t templateFlag) {
  std::unordered_set<uint32_t> seen;
  for(unsigned depth=0;depth<16;++depth) {
    if(!seen.insert(id).second)return nullptr;
    const auto a=c.pipboy.dialogueActors.find(id);
    if(a==c.pipboy.dialogueActors.end())return nullptr;
    if(!(a->second.templateFlags&templateFlag))return &a->second;
    id=a->second.templateActor;
  }
  return nullptr;
}
}
OwnershipResult Player::ResolveOwnership(uint32_t id,const OwnershipData* item) const {
  OwnershipResult out;
  const auto ref=catalog_->references.find(id);
  if(ref==catalog_->references.end()){out.known=false;out.permitted=false;out.kind=OwnershipKind::Unknown;return out;}
  out.cell=ref->second.cell;
  out.data=ref->second.ownership;
  // owner retained for existing callers/fixtures; loader no longer flattens CELL.
  if(ref->second.owner){out.data.owner=ref->second.owner;out.data.explicitOwner=true;}
  if(!out.data.explicitOwner || !out.data.owner) {
    const auto cell=catalog_->cellOwnership.find(out.cell);
    if(cell!=catalog_->cellOwnership.end()){out.data=cell->second.data;out.inherited=out.data.explicitOwner;}
  }
  if(item&&item->explicitOwner){out.data=*item;out.inherited=false;}
  const auto owner=out.data.owner;
  if(!out.data.valid){out.known=false;out.permitted=false;out.kind=OwnershipKind::Unknown;return out;}
  if(!owner)return out;
  out.permitted=false;
  if(owner==PlayerBase){out.kind=OwnershipKind::Player;out.permitted=true;return out;}
  const auto faction=catalog_->factionFlags.find(owner);
  if(faction!=catalog_->factionFlags.end()) {
    out.kind=OwnershipKind::Faction;
    const auto* pc=OwnershipActor(*catalog_,PlayerBase,2);
    if(pc){const auto rank=pc->factions.find(owner);out.permitted=rank!=pc->factions.end()&&rank->second>=0&&rank->second>=out.data.rank;}
    // Fallout3.exe 0x7F6707..0x7F678F tests Evil independently of
    // witness lookup/Track Crime; moral consequences do not need a witness.
    out.karmaEligible=!out.permitted&&!(faction->second&2);
  } else {
    const auto* actor=OwnershipActor(*catalog_,owner,0x80); // Use Base Data (Karma)
    if(!actor||!std::isfinite(actor->karma)){
      out.kind=OwnershipKind::Unknown;out.known=false;return out;
    }
    out.kind=OwnershipKind::NPC;
    // The NPC-owner branch tests the owner's alignment, not all memberships.
    out.karmaEligible=actor->karma>catalog_->evilMaxKarma;
  }
  // XGLB/COED NPC global permission semantics are not established. Keep
  // unresolved interactions blocked rather than charge or exempt arbitrarily.
  if(out.data.global){out.known=false;out.permitted=false;out.karmaEligible=false;}
  return out;
}
bool Player::DoorInteractionCrime(uint32_t reference) const {
  // Activating an unlocked door is not taking CELL-owned property. The red
  // crime indication belongs to picking/forcing an owned lock.
  return GetLock(reference).locked&&ResolveOwnership(reference).Theft();
}
OwnershipResult Player::ContainerStackOwnership(uint32_t id,uint64_t stack) const {
  const auto* contents=ContainerContents(id);
  if(contents)for(const auto& s:*contents)if(s.id==stack){
    const auto ref=catalog_->references.find(id);
    // Keep the existing corpse inventory path separate from CELL-owned world
    // property. An actor's personal CNTO inventory is not a CELL REFR item.
    if(ref!=catalog_->references.end()&&catalog_->actorInventories.count(ref->second.base)&&
       ActorHealth(id)==0&&!s.ownership.explicitOwner)return {};
    return ResolveOwnership(id,&s.ownership);
  }
  OwnershipResult out;out.known=false;out.permitted=false;out.kind=OwnershipKind::Unknown;return out;
}
void Player::BeginContainerInteraction(uint32_t id) {
  if(containerInteraction_!=id){containerInteraction_=id;containerKarmaCommitted_=false;}
}
void Player::EndContainerInteraction(){containerInteraction_=0;containerKarmaCommitted_=false;}
void Player::CommitTheft(uint32_t ref,uint32_t item,int32_t count,uint32_t container,const OwnershipResult& o) {
  if(!o.Theft())return;
  float delta=0;
  if(o.karmaEligible&&(!container||!containerKarmaCommitted_)) {
    const float before=state_.karma;
    RewardKarma(catalog_->theftKarma);
    delta=state_.karma-before;
    if(container)containerKarmaCommitted_=true;
  }
  TheftEvent event;event.reference=ref;event.item=item;event.count=count;
  event.container=container;event.owner=o.data.owner;event.cell=o.cell;
  event.world=scriptPlayerWorld_;event.position=scriptPlayerPosition_;
  event.positionKnown=scriptPlayerPositionKnown_;event.karmaDelta=delta;
  if(pendingTheftEvents_.size()==64)pendingTheftEvents_.erase(pendingTheftEvents_.begin());
  pendingTheftEvents_.push_back(event);++scriptEffectRevision_;
}
bool Player::PollTheftEvent(TheftEvent& out){
  if(pendingTheftEvents_.empty())return false;
  out=pendingTheftEvents_.front();pendingTheftEvents_.erase(pendingTheftEvents_.begin());++scriptEffectRevision_;return true;
}
bool Player::PollKarmaEvent(KarmaEvent& out){
  if(pendingKarmaEvents_.empty())return false;
  out=pendingKarmaEvents_.front();pendingKarmaEvents_.erase(pendingKarmaEvents_.begin());++scriptEffectRevision_;return true;
}
} // namespace fo3player
