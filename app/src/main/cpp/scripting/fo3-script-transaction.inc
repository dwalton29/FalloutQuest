#include <type_traits>

namespace fo3player {
Player::Player(const Player& source,ScriptCandidateTag)
    : referencePlanIdentity_(source.referencePlanIdentity_),catalog_(source.catalog_),
      state_(source.state_),revision_(source.revision_),actorPoseRevisions_(source.actorPoseRevisions_),
      scriptActivator_(source.scriptActivator_),scriptReference_(source.scriptReference_),
      scriptPlayerCell_(source.scriptPlayerCell_),scriptPlayerWorld_(source.scriptPlayerWorld_),
      scriptPlayerPosition_(source.scriptPlayerPosition_),scriptPlayerPositionKnown_(source.scriptPlayerPositionKnown_),
      scriptEffectRevision_(source.scriptEffectRevision_),scriptButtons_(source.scriptButtons_),
      pendingScriptMessages_(source.pendingScriptMessages_),pendingQuestNotices_(source.pendingQuestNotices_),pendingScriptItems_(source.pendingScriptItems_),
      pendingScriptSounds_(source.pendingScriptSounds_),pendingBarters_(source.pendingBarters_),pendingKarmaEvents_(source.pendingKarmaEvents_),
      pendingTheftEvents_(source.pendingTheftEvents_),containerInteraction_(source.containerInteraction_),
      containerKarmaCommitted_(source.containerKarmaCommitted_) {}
std::unique_ptr<ScriptTransactionPlan> Player::PrepareScriptTransaction(
    const std::function<bool(Player&,std::string&)>& execute,std::string& error,bool capturePrograms) const {
  error.clear();
  if(scriptTrace_){error="nested detached script preparation unsupported";return {};}
  auto plan=std::unique_ptr<ScriptTransactionPlan>(new ScriptTransactionPlan());
  plan->owner_=this;plan->ownerIdentity_=referencePlanIdentity_;
  plan->expectedRevision_=revision_;plan->expectedEffects_=scriptEffectRevision_;
  // Player copies share a const ESM catalogue but own every mutable value/queue.
  plan->candidate_=std::unique_ptr<Player>(new Player(*this,ScriptCandidateTag{}));
  auto& candidate=*plan->candidate_;
  plan->programsCaptured_=capturePrograms;
  candidate.scriptTrace_=capturePrograms?&plan->executions_:nullptr;
  struct TraceScope {Player& candidate;~TraceScope(){candidate.scriptTrace_=nullptr;}} trace{candidate};
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
    const fo3pipdata::ResultScript& begin,const fo3pipdata::ResultScript& end,std::string& error,uint32_t speaker,ScriptProgramCapture capture) const {
  return PrepareScriptTransaction([&](Player& candidate,std::string& e){
    unsigned budget=4096;
    ScriptProgramOrigin first,last;last.item=1;
    return candidate.RunScript(begin,speaker,0,budget,e,0,std::move(first))&&
      candidate.RunScript(end,speaker,0,budget,e,0,std::move(last));
  },error,capture==ScriptProgramCapture::EnteredPrograms);
}
std::unique_ptr<ScriptTransactionPlan> Player::PrepareDialogueInfoTransaction(
    uint32_t topic,uint32_t info,DialogueScriptPhase phase,std::string& error,uint32_t speaker,ScriptProgramCapture capture) const {
  error.clear();
  if(phase!=DialogueScriptPhase::Begin&&phase!=DialogueScriptPhase::End&&phase!=DialogueScriptPhase::Both){
    error="dialogue script phase unavailable";return {};
  }
  const auto found=catalog_->dialogueInfos.find(info);
  if(!topic||!info||found==catalog_->dialogueInfos.end()||found->second.first!=topic){
    error="dialogue INFO identity unavailable or ambiguous";return {};
  }
  const auto group=catalog_->pipboy.topics.find(topic);
  if(group==catalog_->pipboy.topics.end()||found->second.second>=group->second.size()){
    error="dialogue INFO index unavailable";return {};
  }
  const auto& authored=group->second[found->second.second];
  if(authored.id!=info||authored.topic!=topic){error="dialogue INFO index mismatch";return {};}
  return PrepareScriptTransaction([&](Player& candidate,std::string& e){
    unsigned budget=4096;
    ScriptProgramOrigin begin,end;
    begin.kind=end.kind=ScriptProgramKind::DialogueInfo;
    begin.record=end.record=info;begin.topic=end.topic=topic;end.item=1;
    return (phase==DialogueScriptPhase::End||candidate.RunScript(authored.begin,speaker,0,budget,e,0,std::move(begin)))&&
      (phase==DialogueScriptPhase::Begin||candidate.RunScript(authored.end,speaker,0,budget,e,0,std::move(end)));
  },error,capture==ScriptProgramCapture::EnteredPrograms);
}
std::unique_ptr<ScriptTransactionPlan> Player::PrepareQuestStageTransaction(
    uint32_t quest,uint16_t stage,std::string& error,ScriptProgramCapture capture) const {
  return PrepareScriptTransaction([=](Player& candidate,std::string& e){
    unsigned budget=4096;return candidate.RunQuestStage(quest,stage,0,budget,e);
  },error,capture==ScriptProgramCapture::EnteredPrograms);
}
std::unique_ptr<ScriptTransactionPlan> Player::PrepareReferenceEventTransaction(
    uint32_t reference,const char* event,uint32_t activator,std::string& error,ScriptProgramCapture capture) const {
  error.clear();
  if(!HasReferenceEvent(reference,event)){error="reference event unavailable";return {};}
  const auto script=catalog_->eventBaseScripts.at(catalog_->references.at(reference).base);
  return PrepareScriptTransaction([=](Player& candidate,std::string& e){
    return candidate.RunSourceEventDirect(script,event,reference,activator,e);
  },error,capture==ScriptProgramCapture::EnteredPrograms);
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
bool Player::CanFinalizeScriptTransaction(const ScriptTransactionPlan& plan,bool committed) const noexcept {
  return plan.owner_==this&&plan.ownerIdentity_==referencePlanIdentity_&&
    (committed?plan.phase_==ScriptTransactionPlan::Phase::Applied:
      (plan.phase_==ScriptTransactionPlan::Phase::Prepared||plan.phase_==ScriptTransactionPlan::Phase::RolledBack))&&
    plan.expectedRevision_==revision_&&plan.expectedEffects_==scriptEffectRevision_&&ScriptPlanContextMatches(plan);
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
  pendingScriptItems_.swap(other.pendingScriptItems_);
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
  plan.expectedRevision_=revision_;plan.expectedEffects_=scriptEffectRevision_;
  plan.phase_=ScriptTransactionPlan::Phase::RolledBack;return true;
}
} // namespace fo3player
