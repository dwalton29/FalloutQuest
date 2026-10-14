#include "fo3-player-state.h"
#include "data/fo3-esm-reader.h"
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
bool Player::RewardExperience(uint32_t amount) {
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
  const auto earned=uint32_t(reward);
  if(earned){state_.experience+=earned;++revision_;}
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
