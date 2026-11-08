bool Player::IsPlayerTagSkill(uint32_t value)const{return value>=32&&value<=45&&(fo3tags::Mask(state_.taggedSkills)&(1u<<(value-32)));}
bool Player::SetPlayerTagSkill(uint32_t value,unsigned slot){
  if(value<32||!fo3tags::Skill(value-32)||slot>=4||state_.tagMenuOwner)return false;
  auto next=state_.taggedSkills;next[slot]=uint8_t(value-32);if(next==state_.taggedSkills)return true;
  const auto bonus=catalog_->rules.tagSkillBonus;if(!std::isfinite(bonus)||bonus<0||bonus>100)return false;
  fo3tags::Apply(state_,next,unsigned(std::lround(bonus)));++revision_;return true;
}
bool Player::OpenTagSkills(uint32_t owner,uint32_t total,bool showExisting){
  if(!owner||total<1||total>4||state_.movie.owner||state_.nameMenuOwner||state_.specialMenuOwner||state_.appearanceMenuOwner||
     (!catalog_->pipboy.quests.count(owner)&&!catalog_->references.count(owner)))return false;
  if(state_.tagMenuOwner)return state_.tagMenuOwner==owner&&state_.tagMenuTotal==total;
  const auto mask=showExisting?fo3tags::Mask(state_.taggedSkills):0;
  if(fo3tags::Count(mask)>total)return false;
  state_.tagDraft=mask;state_.tagMenuOwner=owner;state_.tagMenuTotal=total;++revision_;return true;
}
int Player::TagSkillsRemaining()const{return int(state_.tagMenuTotal)-int(fo3tags::Count(state_.tagDraft));}
bool Player::ToggleTagSkill(unsigned skill){
  if(!state_.tagMenuOwner||!fo3tags::Skill(skill))return false;
  const auto bit=uint16_t(1u<<skill);if(!(state_.tagDraft&bit)&&TagSkillsRemaining()<=0)return false;
  state_.tagDraft^=bit;++revision_;return true;
}
bool Player::ConfirmTagSkills(){
  if(!state_.tagMenuOwner||TagSkillsRemaining()!=0)return false;
  const auto bonus=catalog_->rules.tagSkillBonus;if(!std::isfinite(bonus)||bonus<0||bonus>100)return false;
  std::array<uint8_t,4> slots{{255,255,255,255}};unsigned n=0;
  for(unsigned i=0;i<14;++i)if(state_.tagDraft&(1u<<i))slots[n++]=uint8_t(i);
  fo3tags::Apply(state_,slots,unsigned(std::lround(bonus)));
  state_.tagMenuOwner=state_.tagMenuTotal=0;state_.tagDraft=0;++revision_;return true;
}
