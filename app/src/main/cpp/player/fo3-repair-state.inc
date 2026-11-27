namespace fo3player {
bool Player::QueueRepair(uint32_t actor) {
  // A placed provider can inherit Repair from a leveled NPC template.
  if(actor!=PlayerRef&&!ActorStatistics(actor)&&!RealizeActor(actor))return false;
  if(pendingRepairs_.size()>=8)return false;
  pendingRepairs_.push_back(actor);++scriptEffectRevision_;return true;
}
bool Player::PollRepair(uint32_t& actor) {
  if(pendingRepairs_.empty())return false;
  actor=pendingRepairs_.front();pendingRepairs_.erase(pendingRepairs_.begin());
  ++scriptEffectRevision_;return true;
}
bool Player::OpenRepair(uint32_t actor,RepairSession& out) const {
  if(ActorHealth(actor)<=0)return false;
  float skill=0;
  if(actor==PlayerRef)skill=EffectiveSkill(size_t(Skill::Repair));
  else {
    const auto* stats=ActorStatistics(actor);
    bool disabled=false;std::string error;
    const auto saved=state_.actors.find(actor);
    if(!stats||!ReferenceDisabled(actor,disabled,error)||disabled||
       (saved!=state_.actors.end()&&saved->second.hostile))return false;
    skill=stats->skills[size_t(Skill::Repair)];
  }
  const auto setting=[&](const char* key,float fallback){
    auto it=catalog_->barterSettings.find(key);return it==catalog_->barterSettings.end()?fallback:it->second;
  };
  // Retail 0x601bf0 uses condition in tenths, a linear difference and half-up cap prices.
  const float base=setting("fRepairSkillBase",4),max=setting("fRepairSkillMax",9);
  const float mult=setting("fItemRepairCostMult",1);
  const float limit=base+(max-base)*int(skill)/100.f;
  if(!std::isfinite(limit)||!std::isfinite(mult)||mult<0)return false;
  RepairSession quote;quote.actor=actor;quote.owner=this;quote.target=std::clamp(limit/10.f,0.f,1.f);
  for(const auto& s:state_.inventory){
    if(s.count<=0||!std::isfinite(s.condition)||s.condition>=quote.target)continue;
    const auto entry=catalog_->items.find(s.formId);
    if(entry==catalog_->items.end())continue;
    const auto& item=entry->second;
    if(!item.playable||item.maxCondition<=0||!item.valueKnown||item.value<=0)continue;
    if(item.kind==ItemKind::Armour){
      const auto dr=catalog_->weapons.armourDR.find(s.formId);
      if(dr==catalog_->weapons.armourDR.end()||dr->second<=0)continue;
    }else if(item.kind!=ItemKind::Weapon)continue;
    const float cost=(limit-s.condition*100.f/10.f)/10.f*(mult*float(item.value));
    if(!std::isfinite(cost)||cost<=.5f||double(cost)>INT32_MAX)continue;
    const auto price=int32_t(std::round(cost));
    quote.offers.push_back({s.id,s.formId,s.condition,s.count,price});
    const int64_t row=int64_t(price)*s.count;
    quote.total+=std::min(row,INT64_MAX-quote.total);
  }
  out=std::move(quote);return true;
}
bool Player::ConfirmRepair(const RepairSession& quote,uint64_t stack,std::string& error) {
  error.clear();
  RepairSession current;
  if(quote.owner!=this||!OpenRepair(quote.actor,current)||current.target!=quote.target||
     current.offers!=quote.offers){error="Repair quote changed";return false;}
  if(current.offers.empty()){error="No items to repair";return false;}
  int64_t cost=current.total;
  if(stack){
    const auto row=std::find_if(current.offers.begin(),current.offers.end(),[&](const RepairOffer& r){return r.stack==stack;});
    if(row==current.offers.end()){error="Item unavailable";return false;}
    cost=row->price;
  }
  if(cost>ItemCount(0xf)||cost>INT32_MAX){error="Not enough caps";return false;}
  // Prepare payment and instance changes together. A failed inventory load or split
  // must leave both the repairer's purse and the player's equipment untouched.
  Player candidate(*this,ScriptCandidateTag{});
  if(quote.actor!=PlayerRef){
    if(!candidate.PrepareActorInventory(quote.actor)){error="Repairer inventory unavailable";return false;}
    auto& purse=candidate.state_.containers.at(quote.actor);
    if(InventoryCaps(purse)>INT32_MAX-cost){error="Repairer cannot accept payment";return false;}
    auto caps=std::find_if(purse.begin(),purse.end(),[](const Stack& s){return s.formId==0xf&&!s.ownership.explicitOwner;});
    if(caps!=purse.end())caps->count+=int32_t(cost);
    else if(purse.size()>=10000||!candidate.ScriptAddItem(quote.actor,0xf,int32_t(cost),true)){
      error="Repairer cannot accept payment";return false;
    }
    if(!candidate.ScriptRemoveItem(0xf,int32_t(cost))){error="Repair payment unavailable";return false;}
  }
  auto& inventory=candidate.state_.inventory;
  auto row=current.offers.begin();
  for(auto& s:inventory){
    // Removing caps preserves the relative order of every quoted instance.
    if(row==current.offers.end())break;
    if(row->stack!=s.id)continue;
    ++row;
    if(stack&&s.id!=stack)continue;
    if(stack&&s.count>1){
      if(inventory.size()>=10000||candidate.state_.nextStackId==UINT64_MAX||s.scriptInstance){
        error="Cannot split item stack";return false;
      }
      Stack remainder=s;remainder.id=candidate.state_.nextStackId++;--remainder.count;
      remainder.equipped=false;remainder.equipmentLocked=false;
      s.count=1;s.condition=current.target;inventory.push_back(std::move(remainder));break;
    }
    s.condition=current.target;
  }
  using std::swap;swap(state_,candidate.state_);++revision_;return true;
}
} // namespace fo3player
