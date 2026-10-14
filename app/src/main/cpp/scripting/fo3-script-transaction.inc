#include <type_traits>

namespace fo3player {
Player::Player(const Player& source,ScriptCandidateTag)
    : referencePlanIdentity_(source.referencePlanIdentity_),catalog_(source.catalog_),
      state_(source.state_),revision_(source.revision_),actorPoseRevisions_(source.actorPoseRevisions_),
      scriptActivator_(source.scriptActivator_),scriptReference_(source.scriptReference_),
      scriptPlayerCell_(source.scriptPlayerCell_),scriptPlayerWorld_(source.scriptPlayerWorld_),
      scriptPlayerPosition_(source.scriptPlayerPosition_),scriptPlayerPositionKnown_(source.scriptPlayerPositionKnown_),
      scriptEffectRevision_(source.scriptEffectRevision_),scriptButtons_(source.scriptButtons_),
      pendingScriptMessages_(source.pendingScriptMessages_),pendingQuestNotices_(source.pendingQuestNotices_),
      pendingScriptSounds_(source.pendingScriptSounds_),pendingBarters_(source.pendingBarters_),pendingKarmaEvents_(source.pendingKarmaEvents_),
      pendingTheftEvents_(source.pendingTheftEvents_),containerInteraction_(source.containerInteraction_),
      containerKarmaCommitted_(source.containerKarmaCommitted_) {}
std::unique_ptr<ScriptTransactionPlan> Player::PrepareScriptTransaction(
    const std::function<bool(Player&,std::string&)>& execute,std::string& error) const {
  error.clear();
  auto plan=std::unique_ptr<ScriptTransactionPlan>(new ScriptTransactionPlan());
  plan->owner_=this;plan->ownerIdentity_=referencePlanIdentity_;
  plan->expectedRevision_=revision_;plan->expectedEffects_=scriptEffectRevision_;
  // Player copies share a const ESM catalogue but own every mutable value/queue.
  plan->candidate_=std::unique_ptr<Player>(new Player(*this,ScriptCandidateTag{}));
  auto& candidate=*plan->candidate_;
  if(!execute(candidate,error))return {};
  if(candidate.revision_<revision_ || candidate.revision_==UINT64_MAX ||
     scriptEffectRevision_>UINT64_MAX-2){error="script transaction revision exhausted";return {};}
  if(candidate.state_.referenceEnabled!=state_.referenceEnabled){
    std::vector<uint32_t> roots;
    roots.reserve(candidate.state_.referenceEnabled.size()+state_.referenceEnabled.size());
    for(const auto& entry:candidate.state_.referenceEnabled)roots.push_back(entry.first);
    for(const auto& entry:state_.referenceEnabled)roots.push_back(entry.first);
    std::sort(roots.begin(),roots.end());roots.erase(std::unique(roots.begin(),roots.end()),roots.end());
    std::vector<ReferenceEnableChange> changes;changes.reserve(roots.size());
    for(uint32_t root:roots){
      const auto after=candidate.state_.referenceEnabled.find(root);
      const auto ref=catalog_->references.find(root);
      if(ref==catalog_->references.end()){error="script reference unavailable";return {};}
      changes.push_back({root,after!=candidate.state_.referenceEnabled.end()?after->second:!(ref->second.flags&0x800)});
    }
    plan->references_=PrepareReferenceEnablePlan(changes,error);
    if(!plan->references_)return {};
    candidate.state_.referenceEnabled=plan->references_->roots_;
    if(!plan->references_->HasChanges())plan->references_.reset();
  }
  // Revisions describe the final projection, not intermediate script toggles.
  candidate.state_.referenceStateRevision=plan->references_
      ?plan->references_->CandidateSnapshot()->revision:state_.referenceStateRevision;
  return plan;
}
std::unique_ptr<ScriptTransactionPlan> Player::PrepareDialogueTransaction(
    const fo3pipdata::ResultScript& begin,const fo3pipdata::ResultScript& end,std::string& error,uint32_t speaker) const {
  return PrepareScriptTransaction([&](Player& candidate,std::string& e){
    unsigned budget=4096;
    return candidate.RunScript(begin,speaker,0,budget,e)&&candidate.RunScript(end,speaker,0,budget,e);
  },error);
}
std::unique_ptr<ScriptTransactionPlan> Player::PrepareQuestStageTransaction(
    uint32_t quest,uint16_t stage,std::string& error) const {
  return PrepareScriptTransaction([=](Player& candidate,std::string& e){
    unsigned budget=4096;return candidate.RunQuestStage(quest,stage,0,budget,e);
  },error);
}
std::unique_ptr<ScriptTransactionPlan> Player::PrepareReferenceEventTransaction(
    uint32_t reference,const char* event,uint32_t activator,std::string& error) const {
  error.clear();
  if(!HasReferenceEvent(reference,event)){error="reference event unavailable";return {};}
  const auto script=catalog_->eventBaseScripts.at(catalog_->references.at(reference).base);
  return PrepareScriptTransaction([=](Player& candidate,std::string& e){
    return candidate.RunSourceEventDirect(script,event,reference,activator,e);
  },error);
}
bool Player::ScriptPlanContextMatches(const ScriptTransactionPlan& plan) const noexcept {
  const auto& candidate=*plan.candidate_;
  return scriptPlayerPositionKnown_==candidate.scriptPlayerPositionKnown_ &&
    scriptPlayerPosition_==candidate.scriptPlayerPosition_ && scriptPlayerCell_==candidate.scriptPlayerCell_ &&
    scriptPlayerWorld_==candidate.scriptPlayerWorld_ && scriptActivator_==candidate.scriptActivator_ &&
    scriptReference_==candidate.scriptReference_;
}
bool Player::CanCommitScriptTransaction(const ScriptTransactionPlan& plan) const noexcept {
  return plan.owner_==this && plan.ownerIdentity_==referencePlanIdentity_ &&
    plan.phase_==ScriptTransactionPlan::Phase::Prepared && plan.expectedRevision_==revision_ &&
    plan.expectedEffects_==scriptEffectRevision_ && ScriptPlanContextMatches(plan);
}
bool Player::CanRollbackScriptTransaction(const ScriptTransactionPlan& plan) const noexcept {
  return plan.owner_==this && plan.ownerIdentity_==referencePlanIdentity_ &&
    plan.phase_==ScriptTransactionPlan::Phase::Applied && plan.expectedRevision_==revision_ &&
    plan.expectedEffects_==scriptEffectRevision_ && ScriptPlanContextMatches(plan);
}
void Player::SwapScriptPayload(Player& other) noexcept {
  static_assert(std::is_nothrow_swappable_v<State>,"script state publication must not allocate");
  using std::swap;
  swap(state_,other.state_);swap(actorPoseRevisions_,other.actorPoseRevisions_);
  scriptButtons_.swap(other.scriptButtons_);
  pendingScriptMessages_.swap(other.pendingScriptMessages_);
  pendingScriptSounds_.swap(other.pendingScriptSounds_);
  pendingBarters_.swap(other.pendingBarters_);
  pendingQuestNotices_.swap(other.pendingQuestNotices_);
  pendingKarmaEvents_.swap(other.pendingKarmaEvents_);
  pendingTheftEvents_.swap(other.pendingTheftEvents_);
}
bool Player::CommitScriptTransaction(ScriptTransactionPlan& plan) noexcept {
  if(!CanCommitScriptTransaction(plan))return false;
  const uint64_t nextRevision=plan.candidate_->revision_;
  SwapScriptPayload(*plan.candidate_);revision_=nextRevision;
  ++scriptEffectRevision_;
  plan.expectedRevision_=revision_;plan.expectedEffects_=scriptEffectRevision_;
  plan.phase_=ScriptTransactionPlan::Phase::Applied;return true;
}
bool Player::RollbackScriptTransaction(ScriptTransactionPlan& plan) noexcept {
  if(!CanRollbackScriptTransaction(plan))return false;
  SwapScriptPayload(*plan.candidate_);++revision_;++scriptEffectRevision_;
  if(plan.HasReferenceChanges())state_.referenceStateRevision=plan.references_->RollbackSnapshot()->revision;
  plan.phase_=ScriptTransactionPlan::Phase::RolledBack;return true;
}
} // namespace fo3player
