#include "fo3-player-state.h"
#include "player/fo3-levelup-rules.h"
#include "data/fo3-esm-reader.h"
#include "npc/fo3-package-schedule.h"
#include <algorithm>
#include <cmath>
namespace fo3player {
bool Player::RecordDialogue(uint32_t actor,uint32_t info,const std::vector<uint32_t>& topics) {
  if(!catalog_->pipboy.referenceScripts.count(actor))return false;
  bool changed=state_.pipboy.talkedActors.insert(actor).second;
  if(info)changed=state_.pipboy.saidInfos.insert((uint64_t(actor)<<32)|info).second||changed;
  for(auto topic:topics)if(catalog_->pipboy.dialogueTopics.count(topic))changed=state_.pipboy.knownTopics.insert(topic).second||changed;
  if(changed)++revision_;
  return true;
}
bool Player::SetDialogueVariable(uint64_t key,float value) {
  if(!key||!std::isfinite(value))return false;
  state_.pipboy.dialogueVariables[key]=value;++revision_;return true;
}
bool Player::LevelUpPerkSupported(uint32_t id) const {
  const auto it=catalog_->pipboy.perks.find(id);
  if(it==catalog_->pipboy.perks.end())return false;
  const auto& p=it->second;
  // Exceptional vanilla perks with existing native outcomes.
  const auto special=catalog_->pipboy.formNames.find("intensetraining");
  if((special!=catalog_->pipboy.formNames.end()&&id==special->second)||
     (catalog_->infiltrator&&id==catalog_->infiltrator))return true;
  // Swift Learner and Educated are handled by the reward and skill budget
  // entry-points. Other effects must not be silently accepted as functional.
  if(p.effects.empty())return false;
  for(const auto& e:p.effects) {
    if(e.type!=2||(e.entryPoint!=9&&e.entryPoint!=10)||
       e.scripted||e.parameterType!=1||e.parameters.size()!=sizeof(float)||
       (e.function!=1&&e.function!=2&&e.function!=3))return false;
    for(const auto& tab:e.conditions)if(!tab.second.empty())return false;
  }
  return true;
}

bool Player::CommitLevelUp(const std::array<uint8_t,14>& chosenSkills,
                           uint32_t perkForm,int intenseSpecial) {
  const auto progress=ExperienceProgress();
  if(!progress.ready||state_.level>=fo3experience::BaseGameLevelCap)return false;
  const int budget=fo3levelup::SkillBudget(
      state_.special,catalog_->levelSkillPointsBase,
      catalog_->levelSkillPointsInterval,state_.pipboy.perks,
      catalog_->pipboy.perks);
  if(!fo3levelup::SkillAllocationValid(state_.skills,chosenSkills,budget)||
     !LevelUpPerkSupported(perkForm))return false;
  const uint16_t targetLevel=uint16_t(state_.level+1);
  const auto found=catalog_->pipboy.perks.find(perkForm);
  if(found==catalog_->pipboy.perks.end()||
     !fo3levelup::PerkEligible(perkForm,found->second,targetLevel,
         state_.special,chosenSkills,catalog_->pipboy.playerFemale,
         state_.pipboy.perks))return false;
  const auto specialPerk=catalog_->pipboy.formNames.find("intensetraining");
  const bool intense=specialPerk!=catalog_->pipboy.formNames.end()&&
      perkForm==specialPerk->second;
  if((!intense&&intenseSpecial!=-1)||
     (intense&&(intenseSpecial<0||intenseSpecial>=7||
                state_.special[size_t(intenseSpecial)]>=10)))return false;
  auto next=state_;
  next.level=targetLevel;
  next.skills=chosenSkills;
  if(intense)++next.special[size_t(intenseSpecial)];
  const auto owned=next.pipboy.perks.find(perkForm);
  next.pipboy.perks[perkForm]=owned==next.pipboy.perks.end()?1:uint8_t(owned->second+1);
  const auto& rule=catalog_->rules;
  const float health=next.baseHealth+
      (next.special[2]+rule.healthEnduranceOffset)*rule.healthEnduranceMult+
      (next.level-1)*rule.healthLevelMult;
  const float ap=rule.apBase+next.special[5]*rule.apMult;
  if(!std::isfinite(health)||health<=0||next.healthDamage>health||
     !std::isfinite(ap)||ap<0||next.apSpent>ap)return false;
  state_=std::move(next);
  ++revision_;
  return true;
}

bool Player::GrantPerk(uint32_t id, uint8_t rank) {
  auto p = catalog_->pipboy.perks.find(id);
  if (p == catalog_->pipboy.perks.end() || !rank || rank > p->second.ranks)
    return false;
  state_.pipboy.perks[id] = rank;
  ++revision_;
  return true;
}
bool Player::EnsureQuestInstance(uint32_t id) {
  auto def=catalog_->pipboy.quests.find(id);if(def==catalog_->pipboy.quests.end())return false;
  if(!state_.pipboy.quests.count(id)) {
    state_.pipboy.quests[id]={};state_.pipboy.quests.at(id).running=bool(def->second.flags&1);++revision_;
  }
  return true;
}
bool Player::StartQuest(uint32_t id) {
  if (!catalog_->pipboy.quests.count(id))
    return false;
  if (!state_.pipboy.quests.count(id)) {
    state_.pipboy.quests[id] = {};
    ++revision_;
  }
  if(!state_.pipboy.quests.at(id).running){state_.pipboy.quests.at(id).running=true;++revision_;}
  return true;
}
bool Player::SetQuestStage(uint32_t id,uint16_t stage) {
  // Compatibility API remains explicit about needing a running instance.
  if(!state_.pipboy.quests.count(id))return false;
  std::string error;return ExecuteQuestStage(id,stage,error);
}
bool Player::StopQuest(uint32_t id) {
  if(!catalog_->pipboy.quests.count(id))return false;
  if(!state_.pipboy.quests.count(id))state_.pipboy.quests[id]={};
  state_.pipboy.quests.at(id).running=false;++revision_;return true;
}
bool Player::SetGlobalValue(uint32_t id,float value) {
  if(!catalog_->globals.count(id)||!std::isfinite(value))return false;
  // Clock globals have canonical clock setters; do not shadow those systems.
  if(id==catalog_->hourGlobal||id==catalog_->dayGlobal||id==catalog_->monthGlobal||id==catalog_->yearGlobal||id==catalog_->daysPassedGlobal||id==catalog_->timeScaleGlobal)return false;
  state_.pipboy.mutableGlobals[id]=value;++revision_;return true;
}
bool Player::SetObjective(uint32_t id, uint32_t index, bool displayed,
                          fo3pipdata::Completion status) {
  auto q = catalog_->pipboy.quests.find(id);
  if (q == catalog_->pipboy.quests.end() || !q->second.objectives.count(index) ||
      !state_.pipboy.quests.count(id) || int(status) > 2)
    return false;
  auto& instance=state_.pipboy.quests.at(id);
  const auto old=instance.objectives.find(index);
  if(old!=instance.objectives.end()&&old->second.displayed==displayed&&old->second.status==status)return true;
  const bool wasDisplayed=old!=instance.objectives.end()&&old->second.displayed;
  instance.objectives[index] = {displayed, status};
  if(displayed&&!q->second.name.empty()) {
    pendingQuestNotices_.push_back({id,instance.announced?QuestNoticeKind::Updated:QuestNoticeKind::Added});
    instance.announced=true;
  } else if(wasDisplayed&&instance.announced)pendingQuestNotices_.push_back({id,QuestNoticeKind::Updated});
  ++revision_;
  return true;
}
bool Player::FinishQuest(uint32_t id, fo3pipdata::Completion status) {
  if (!state_.pipboy.quests.count(id) ||
      status == fo3pipdata::Completion::Active || int(status) > 2)
    return false;
  auto& quest=state_.pipboy.quests.at(id);
  if(quest.status==status)return true;
  if(status==fo3pipdata::Completion::Complete&&quest.announced)
    pendingQuestNotices_.push_back({id,QuestNoticeKind::Completed});
  quest.status = status;
  if (state_.pipboy.selectedQuest == id)
    state_.pipboy.selectedQuest = 0;
  ++revision_;
  return true;
}
bool Player::PreviewExperienceAward(uint32_t amount,uint32_t& earned) const {
  float reward=float(amount);
  struct Entry {uint32_t perk;size_t index;const fo3pipdata::PerkEffect* effect;};
  std::vector<Entry> effects;
  for(const auto& owned:state_.pipboy.perks){
    const auto perk=catalog_->pipboy.perks.find(owned.first);if(perk==catalog_->pipboy.perks.end())return false;
    for(size_t i=0;i<perk->second.effects.size();++i){const auto& e=perk->second.effects[i];
      if(e.type==2&&e.entryPoint==9&&e.rank+1==owned.second)effects.push_back({owned.first,i,&e});
    }
  }
  std::sort(effects.begin(),effects.end(),[](const Entry& a,const Entry& b){
    if(a.effect->priority!=b.effect->priority)return a.effect->priority<b.effect->priority;
    return a.perk!=b.perk?a.perk<b.perk:a.index<b.index;
  });
  for(const auto& entry:effects){const auto& e=*entry.effect;
    for(const auto& tab:e.conditions)if(!tab.second.empty())return false;
    if(e.scripted||e.parameterType!=1||e.parameters.size()!=4)return false;
    const float value=fo3esm::ReadF32(e.parameters.data());if(!std::isfinite(value))return false;
    switch(e.function){case 1:reward=value;break;case 2:reward+=value;break;case 3:reward*=value;break;default:return false;}
  }
  if(!std::isfinite(reward)||reward<0||double(reward)>UINT32_MAX-state_.experience)return false;
  earned=uint32_t(reward);return true;
}
bool Player::RewardExperience(uint32_t amount) {
  uint32_t earned=0;
  if(!PreviewExperienceAward(amount,earned))return false;
  if(earned){
    const auto before=ExperienceProgress();
    state_.experience+=earned;
    const auto after=ExperienceProgress();
    // A transient effect, not saved. Queued only after real (or transaction
    // candidate) state mutation, and rolled back with the candidate.
    pendingExperienceNotices_.push_back({earned,state_.experience,
        after.pendingLevels>before.pendingLevels,after.level,
        after.currentThreshold,after.nextThreshold});
    ++revision_;
  }
  return true;
}
bool Player::CompleteAllObjectives(uint32_t id) {
  const auto q=catalog_->pipboy.quests.find(id);
  if(q==catalog_->pipboy.quests.end()||!EnsureQuestInstance(id))return false;
  // Completion does not display previously hidden objectives or stop the quest.
  for(const auto& def:q->second.objectives){
    const auto& objectives=state_.pipboy.quests.at(id).objectives;
    const auto old=objectives.find(def.first);
    if(!SetObjective(id,def.first,old!=objectives.end()&&old->second.displayed,fo3pipdata::Completion::Complete))return false;
  }
  return true;
}
bool Player::SetCellFullName(uint32_t cell,uint32_t message) {
  const auto m=catalog_->pipboy.messages.find(message);
  if(!catalog_->cellOwnership.count(cell)||m==catalog_->pipboy.messages.end()||m->second.title.empty())return false;
  if(state_.cellNameMessages.count(cell)&&state_.cellNameMessages.at(cell)==message)return true;
  state_.cellNameMessages[cell]=message;++revision_;return true;
}
std::string Player::CellFullName(uint32_t cell) const {
  const auto saved=state_.cellNameMessages.find(cell);
  if(saved!=state_.cellNameMessages.end())return catalog_->pipboy.messages.at(saved->second).title;
  const auto def=catalog_->cellOwnership.find(cell);
  return def==catalog_->cellOwnership.end()?std::string{}:def->second.name;
}
bool Player::AddAchievement(uint32_t id) {
  // Persist the source event locally. FalloutQuest has no Xbox/GFWL integration.
  if(id>255)return false;
  if(state_.achievements.insert(id).second)++revision_;
  return true;
}
bool Player::ScriptInSameCell(uint32_t reference,bool& result,std::string& error) const {
  if(!scriptPlayerPositionKnown_||!scriptPlayerCell_){error="GetInSameCell player location unavailable";return false;}
  uint32_t cell=0;
  const auto actor=state_.actors.find(reference);
  if(actor!=state_.actors.end())cell=actor->second.cell;
  else {const auto ref=catalog_->references.find(reference);if(ref!=catalog_->references.end())cell=ref->second.cell;}
  if(!cell){error="GetInSameCell reference location unavailable";return false;}
  result=cell==scriptPlayerCell_;return true;
}
bool Player::PollExperienceNotice(ExperienceNotice& out) {
  if(pendingExperienceNotices_.empty())return false;
  out=pendingExperienceNotices_.front();
  pendingExperienceNotices_.erase(pendingExperienceNotices_.begin());
  ++scriptEffectRevision_;
  return true;
}
bool Player::PollScriptItemNotice(ScriptItemNotice& out) {
  if(pendingScriptItems_.empty())return false;
  out=pendingScriptItems_.front();pendingScriptItems_.erase(pendingScriptItems_.begin());++scriptEffectRevision_;return true;
}
bool Player::PollQuestNotice(QuestNotice& out) {
  if(pendingQuestNotices_.empty())return false;
  out=pendingQuestNotices_.front();pendingQuestNotices_.erase(pendingQuestNotices_.begin());
  ++scriptEffectRevision_;
  for(auto it=pendingQuestNotices_.begin();it!=pendingQuestNotices_.end();) {
    if(it->quest!=out.quest){++it;continue;}
    if(it->kind==QuestNoticeKind::Updated||it->kind==out.kind)it=pendingQuestNotices_.erase(it);
    else ++it;
  }
  return true;
}
bool Player::SelectQuest(uint32_t id) {
  if (id &&
      (!state_.pipboy.quests.count(id) ||
       state_.pipboy.quests.at(id).status != fo3pipdata::Completion::Active))
    return false;
  state_.pipboy.selectedQuest = id;
  ++revision_;
  return true;
}
bool Player::Discover(uint32_t id) {
  if (!catalog_->pipboy.markers.count(id))
    return false;
  if (state_.pipboy.discovered.insert(id).second)
    ++revision_;
  return true;
}
bool Player::SetWaypoint(uint32_t world, float x, float y) {
  if (world && (!catalog_->pipboy.worlds.count(world) || !std::isfinite(x) ||
                !std::isfinite(y)))
    return false;
  state_.pipboy.waypoint = {world, {x, y}};
  ++revision_;
  return true;
}
bool Player::TuneRadio(uint32_t ref) {
  if (ref && !catalog_->pipboy.transmitters.count(ref))
    return false;
  if (state_.pipboy.tunedRadio != ref) {
    state_.pipboy.tunedRadio = ref;
    ++revision_;
  }
  return true;
}
bool Player::CanUse(uint64_t id, std::string *why) const {
  auto reject = [&](const char *text) {
    if (why)
      *why = text;
    return false;
  };
  auto s = std::find_if(state_.inventory.begin(), state_.inventory.end(),
                        [&](const Stack &s) { return s.id == id; });
  if (s == state_.inventory.end())
    return reject("Item absent");
  auto item = catalog_->items.find(s->formId);
  auto a = catalog_->pipboy.aid.find(s->formId);
  if (item == catalog_->items.end() || item->second.script ||
      IsQuestObject(s->formId) || item->second.cannotDrop ||
      a == catalog_->pipboy.aid.end() || a->second.effects.empty())
    return reject("Requires item script/effect runtime");
  for (auto &e : a->second.effects) {
    auto m = catalog_->pipboy.magic.find(e.id);
    if (m == catalog_->pipboy.magic.end() || e.conditional || e.area ||
        e.duration || e.range || m->second.archetype != 0 ||
        (m->second.flags & 0x100000) ||
        ((m->second.av != 16) && (m->second.av != 12)))
      return reject("Requires timed, conditional or unsupported actor effect");
  }
  return true;
}
bool Player::Use(uint64_t id) {
  if (!CanUse(id))
    return false;
  auto s = std::find_if(state_.inventory.begin(), state_.inventory.end(),
                        [&](const Stack &s) { return s.id == id; });
  auto effects = catalog_->pipboy.aid.at(s->formId)
                     .effects; // validate all before consuming
  for (auto &e : effects) {
    auto &m = catalog_->pipboy.magic.at(e.id);
    float amount = float(e.magnitude);
    if (m.av == 16) {
      if (m.flags & 4)
        DamageHealth(amount);
      else
        RestoreHealth(amount);
    } else if (m.av == 12) {
      if (m.flags & 4) {
        state_.apSpent = std::min(MaxActionPoints(), state_.apSpent + amount);
      } else
        RestoreActionPoints(amount);
    }
  }
  --s->count;
  if (!s->count)
    state_.inventory.erase(s);
  ++state_.pipboy.aidUsed;
  ++revision_;
  return true;
}
} // namespace fo3player

namespace fo3player {
bool Player::DebugReachNextLevel() {
  const auto p=ExperienceProgress();
  if(p.level>=p.maxLevel||p.ready)return false;
  const uint32_t needed=p.nextThreshold-p.earned;
  uint32_t lo=1,hi=UINT32_MAX,earned=0;
  // Invalid high awards overflow canonical XP. They bound the upper search
  // just as an award larger than the deficit does; no trial mutates Player.
  while(lo<hi){const auto mid=lo+(hi-lo)/2;
    if(!PreviewExperienceAward(mid,earned)||earned>=needed)hi=mid;
    else lo=mid+1;
  }
  return PreviewExperienceAward(lo,earned)&&earned>=needed&&RewardExperience(lo);
}
bool Player::DebugSetTime(int hour,int minute) {
  if(hour<0||hour>23||minute<0||minute>59)return false;
  const float target=float(hour)+float(minute)/60.f;
  auto next=state_;
  // Select the next occurrence, retaining the current date at equal time.
  if(target<state_.gameHour){
    if(next.gameDaysPassed==UINT32_MAX)return false;
    ++next.gameDaysPassed;
    if(++next.gameDay>uint32_t(fo3schedule::Days(int(next.gameYear),int(next.gameMonth)))){
      next.gameDay=1;if(++next.gameMonth==12){next.gameMonth=0;++next.gameYear;}
    }
    if(next.gameYear>9999)return false;
  }
  next.gameHour=target;
  state_=std::move(next);++revision_;return true;
}
bool Player::DebugSetPerkPoints(int points) {
  if(points<0||points>1000)return false;
  if(state_.debugPerkPoints!=points){state_.debugPerkPoints=uint16_t(points);++revision_;}
  return true;
}
bool Player::DebugSetPerkRank(uint32_t id,int rank) {
  const auto p=catalog_->pipboy.perks.find(id);
  if(p==catalog_->pipboy.perks.end()||rank<0||rank>p->second.ranks)return false;
  const auto owned=state_.pipboy.perks.find(id);
  const int old=owned==state_.pipboy.perks.end()?0:owned->second;
  if(old==rank)return true;
  // Debug bypasses level/prerequisites, never invents unimplemented effects.
  // Intense Training changes SPECIAL via its normal allocation UI. Editing
  // that acquired rank alone cannot reconstruct which attribute it changed.
  const auto intense=catalog_->pipboy.formNames.find("intensetraining");
  if(rank>old&&(!p->second.playable||p->second.hidden||!LevelUpPerkSupported(id)||
      (intense!=catalog_->pipboy.formNames.end()&&id==intense->second)))return false;
  const int points=int(state_.debugPerkPoints)+old-rank;
  if(points<0||points>1000)return false;
  if(rank)state_.pipboy.perks[id]=uint8_t(rank);else state_.pipboy.perks.erase(id);
  state_.debugPerkPoints=uint16_t(points);++revision_;return true;
}
}
