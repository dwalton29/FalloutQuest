// Included by fo3-pipboy-player.cpp inside namespace fo3player.
namespace {
using namespace fo3pipdata;
constexpr uint32_t HealthValue=16, ActionPointsValue=12, RadsValue=54;
constexpr uint8_t ChemDurationEntry=25, AddictionChanceEntry=23;
bool DamagePool(uint32_t av){
  return av==HealthValue||av==ActionPointsValue||av==RadsValue||(av>=25&&av<=30);
}
bool ModifierValue(uint32_t av){
  return (av>=5&&av<=13)||av==16||av==18||av==20||av==21||
         (av>=32&&av<=45)||av==47||av==49||av==52||av==56;
}
const Effect* SourceEffect(const Definitions& data,const ActiveAid& active){
  const auto source=data.aid.find(active.source);
  return source==data.aid.end()||active.index>=source->second.effects.size()?
      nullptr:&source->second.effects[active.index];
}
bool ValidAidScript(const Catalog& catalog,const Magic& magic,float duration){
  if(magic.temporaryPerk)return duration>0;
  const auto script=catalog.eventPrograms.find(magic.script);
  if(duration!=0||script==catalog.eventPrograms.end()||!script->second.error.empty()||
     script->second.compiledOnly||script->second.blocks.empty())return false;
  return std::all_of(script->second.blocks.begin(),script->second.blocks.end(),[](const auto& block){
    return block.event=="scripteffectstart"&&block.filter.empty();
  });
}
bool AddictionSpell(const Definitions& data,uint32_t spell){
  return data.effectSpells.count(spell)&&std::any_of(data.aid.begin(),data.aid.end(),[&](const auto& item){
    return item.second.withdrawal==spell&&item.second.addictionChance>0;
  });
}
float SpellModifier(const Definitions& data,uint32_t spell,uint32_t av){
  const auto definition=data.effectSpells.find(spell);
  if(definition==data.effectSpells.end())return 0;
  float value=0;
  for(const auto& effect:definition->second.effects){
    const auto magic=data.magic.find(effect.id);
    if(magic!=data.magic.end()&&magic->second.av==av&&magic->second.archetype==MagicValue&&
       (magic->second.flags&MagicRecover)&&effect.conditions.empty())
      value+=(magic->second.flags&MagicDetrimental?-1.f:1.f)*float(effect.magnitude);
  }
  return value;
}
}

bool Player::AidEffects(uint32_t form,std::vector<AidEffect>& out,std::string* reason) const {
  using namespace fo3pipdata;
  out.clear();
  if(reason)reason->clear();
  const auto reject=[&](const char* text){out.clear();if(reason)*reason=text;return false;};
  const auto source=catalog_->pipboy.aid.find(form);
  if(source==catalog_->pipboy.aid.end()||source->second.effects.empty())
    return reject("No consumable effects available");
  fo3dialogue::Context context;
  context.player=this;context.speaker.reference=PlayerRef;context.speaker.base=PlayerBase;
  context.target=context.speaker;
  const auto& item=source->second;
  float medicine=0;
  if(item.flags&IngestibleMedicine)ActorValue(PlayerRef,37,medicine);
  const float scale=(item.flags&IngestibleMedicine)?catalog_->rules.medicineBase+
      catalog_->rules.medicineMult*medicine/100.f:1.f;
  if(item.withdrawal&&(!catalog_->pipboy.effectSpells.count(item.withdrawal)||
      !std::isfinite(item.addictionChance)||item.addictionChance<0||item.addictionChance>1))
    return reject("Unsupported addiction data");
  const bool timed=std::any_of(item.effects.begin(),item.effects.end(),[](const auto& e){
    return e.duration&&e.id!=UsageMonitorEffect;
  });
  const float durationScale=timed?AidEntryPoint(ChemDurationEntry,1.f):1.f;
  for(uint32_t i=0;i<item.effects.size();++i){
    const auto& effect=item.effects[i];
    std::string error;
    if(!fo3dialogue::Conditions(effect.conditions,context,error)){
      if(!error.empty())return reject("Unsupported item effect condition");
      continue;
    }
    if(effect.range||effect.area||effect.magnitude>100000||effect.duration>10000000)
      return reject("Unsupported item effect parameters");
    if(effect.id==UsageMonitorEffect){
      if(effect.duration&&effect.magnitude&&item.withdrawal)
        out.push_back({i,float(effect.magnitude),float(effect.duration)});
      continue;
    }
    const auto found=catalog_->pipboy.magic.find(effect.id);
    if(found==catalog_->pipboy.magic.end())return reject("Unknown item effect");
    const auto& magic=found->second;
    const float duration=float(effect.duration)*durationScale;
    switch(magic.archetype){
      case MagicStimpak:
        if(magic.av!=HealthValue||duration)return reject("Unsupported healing effect");
        break;
      case MagicScript:
        if(!ValidAidScript(*catalog_,magic,duration))return reject("Unsupported item effect script");
        break;
      case MagicValue:case MagicChameleon:
        if(magic.flags&MagicRecover){
          if(!duration||!ModifierValue(magic.av))return reject("Unsupported temporary actor value");
        }else if(!DamagePool(magic.av))return reject("Unsupported item actor value");
        break;
      default:return reject("Unsupported item effect");
    }
    float magnitude=float(effect.magnitude)*scale;
    float resistance=0;
    if((magic.flags&MagicDetrimental)&&magic.resistance!=UINT32_MAX&&
       ActorValue(PlayerRef,magic.resistance,resistance))
      magnitude*=1.f-std::clamp(resistance,0.f,100.f)/100.f;
    out.push_back({i,magnitude,duration});
  }
  return !out.empty()||reject("No applicable item effects");
}

uint32_t Player::RadiationSickness() const {
  return RadiationSickness(state_.pipboy.radiation);
}
uint32_t Player::RadiationSickness(float rads) const {
  uint32_t threshold=0,spell=0;
  for(const auto& row:catalog_->pipboy.radiationStages){
    const auto& stage=row.second;
    if(stage.threshold>threshold&&stage.threshold<=rads){
      threshold=stage.threshold;spell=stage.spell;
    }
  }
  return spell;
}

bool Player::WithdrawalActive(uint32_t spell) const {
  return WithdrawalActive(state_,spell);
}
bool Player::WithdrawalActive(const State& state,uint32_t spell) const {
  if(!state.addictions.count(spell))return false;
  const auto& data=catalog_->pipboy;
  return std::none_of(state.activeAid.begin(),state.activeAid.end(),[&](const auto& active){
    const auto* effect=SourceEffect(data,active);
    return effect&&effect->id!=UsageMonitorEffect&&data.aid.at(active.source).withdrawal==spell;
  });
}

bool Player::SetAddiction(uint32_t spell,bool enabled) {
  const auto& data=catalog_->pipboy;
  if(!AddictionSpell(data,spell))return false;
  const bool changed=enabled?state_.addictions.insert(spell).second:state_.addictions.erase(spell)!=0;
  if(changed){ClampAidResources();++revision_;}
  return true;
}

float Player::AidModifier(const State& state,uint32_t av) const {
  const auto& data=catalog_->pipboy;
  float result=0;
  for(const auto& active:state.activeAid){
    const auto* effect=SourceEffect(data,active);
    if(!effect||effect->id==UsageMonitorEffect)continue;
    const auto magic=data.magic.find(effect->id);
    if(magic!=data.magic.end()&&(magic->second.flags&fo3pipdata::MagicRecover)&&
       magic->second.av==av&&magic->second.archetype!=fo3pipdata::MagicScript)
      result+=(magic->second.flags&fo3pipdata::MagicDetrimental?-1.f:1.f)*active.magnitude;
  }
  if(state.pipboy.radiation>0)
    result+=SpellModifier(data,RadiationSickness(state.pipboy.radiation),av);
  for(auto spell:state.addictions)
    if(WithdrawalActive(state,spell))result+=SpellModifier(data,spell,av);
  return result;
}

float Player::EffectiveSpecial(const State& state,unsigned index) const {
  const float base=state.special[index];
  return std::clamp(base+AidModifier(state,5+index),base>0?1.f:0.f,10.f);
}

float Player::EffectiveSkill(unsigned index) const {
  static constexpr unsigned governing[14]={3,2,1,1,1,4,0,4,4,5,5,3,5,2};
  const auto attribute=governing[index];
  const auto& rules=catalog_->rules;
  const float primary=(EffectiveSpecial(attribute)-state_.special[attribute])*rules.skillPrimaryMult;
  const float luck=std::ceil(EffectiveSpecial(6)*rules.skillLuckMult)-
                   std::ceil(state_.special[6]*rules.skillLuckMult);
  return std::clamp(state_.skills[index]+primary+luck+AidModifier(32+index),0.f,100.f);
}

void Player::ClampAidResources(){
  state_.healthDamage=std::min(state_.healthDamage,MaxHealth());
  state_.apSpent=std::min(state_.apSpent,MaxActionPoints());
}

float Player::AidEntryPoint(uint8_t entryPoint,float value) const {
  const auto& data=catalog_->pipboy;
  std::vector<std::pair<uint32_t,uint8_t>> owned(state_.pipboy.perks.begin(),state_.pipboy.perks.end());
  for(const auto& active:state_.activeAid){
    const auto* effect=SourceEffect(data,active);
    if(!effect)continue;
    const auto magic=data.magic.find(effect->id);
    if(magic!=data.magic.end()&&magic->second.temporaryPerk)
      owned.emplace_back(magic->second.temporaryPerk,1);
  }
  // A temporary grant never duplicates or revokes a separately owned perk.
  std::sort(owned.rbegin(),owned.rend());
  std::vector<const fo3pipdata::PerkEffect*> entries;
  uint32_t previous=0;
  for(auto [id,rank]:owned){
    if(id==previous)continue;
    previous=id;
    const auto perk=data.perks.find(id);
    if(perk==data.perks.end())continue;
    for(const auto& effect:perk->second.effects){
      if(effect.type!=2||effect.entryPoint!=entryPoint||effect.rank+1!=rank||
         effect.scripted||effect.parameterType!=1||effect.parameters.size()!=4||
         std::any_of(effect.conditions.begin(),effect.conditions.end(),[](const auto& c){return !c.second.empty();}))continue;
      entries.push_back(&effect);
    }
  }
  std::stable_sort(entries.begin(),entries.end(),[](auto a,auto b){return a->priority>b->priority;});
  for(const auto* e:entries){
    const float n=fo3esm::ReadF32(e->parameters.data());
    if(!std::isfinite(n))continue;
    if(e->function==1)value=n;
    else if(e->function==2)value+=n;
    else if(e->function==3)value*=n;
  }
  return std::clamp(value,0.f,100000.f);
}

bool Player::CanUse(uint64_t id,std::string* reason) const {
  if(reason)reason->clear();
  const auto reject=[&](const char* text){if(reason)*reason=text;return false;};
  const auto stack=std::find_if(state_.inventory.begin(),state_.inventory.end(),
                               [&](const Stack& row){return row.id==id;});
  if(stack==state_.inventory.end()||stack->count<=0)return reject("Item absent");
  const auto item=catalog_->items.find(stack->formId);
  if(item==catalog_->items.end()||!item->second.playable||
     (item->second.kind!=ItemKind::Aid&&item->second.kind!=ItemKind::Ingredient&&item->second.bookSkill<0))
    return reject("Item cannot be consumed");
  if(IsQuestObject(stack->formId)||item->second.cannotDrop)
    return reject("Quest item cannot be consumed");
  if(Health()<=0)return reject("Cannot use Aid while dead");
  if(item->second.bookSkill>=0)return true;
  std::vector<AidEffect> effects;
  return AidEffects(stack->formId,effects,reason);
}

bool Player::Use(uint64_t id,std::string* reason) {
  if(!CanUse(id,reason))return false;
  const auto found=std::find_if(state_.inventory.begin(),state_.inventory.end(),
                               [&](const Stack& stack){return stack.id==id;});
  const uint32_t form=found->formId;
  const int bookSkill=catalog_->items.at(form).bookSkill;
  if(bookSkill>=0){
    auto skills=state_.skills;
    skills[size_t(bookSkill)]=uint8_t(std::min(100,int(skills[size_t(bookSkill)])+int(AidEntryPoint(11,1))));
    if(!SetPlayerAttributes(state_.special,skills)){
      if(reason)*reason="Could not read skill book";
      return false;
    }
    if(--found->count==0)state_.inventory.erase(found);
    ++revision_;
    return true;
  }
  std::vector<AidEffect> effects;
  if(!AidEffects(form,effects,reason))return false;
  std::string error;
  auto plan=PrepareScriptTransaction([&](Player& p,std::string& failure){
    using namespace fo3pipdata;
    auto& state=p.state_;
    const auto& data=catalog_->pipboy;
    const auto& source=data.aid.at(form);
    auto stack=std::find_if(state.inventory.begin(),state.inventory.end(),
                            [&](const Stack& row){return row.id==id;});
    if(--stack->count==0)state.inventory.erase(stack);
    bool monitored=false;
    for(const auto& applied:effects){
      const auto& effect=source.effects[applied.index];
      if(applied.seconds>0){
        if(effect.id==UsageMonitorEffect)monitored=true;
        else{
          // Reapplying the same authored MGEF refreshes it. Distinct effects
          // on the same actor value remain independent.
          auto same=std::find_if(state.activeAid.begin(),state.activeAid.end(),[&](const ActiveAid& a){
            const auto* prior=SourceEffect(data,a);
            return prior&&prior->id==effect.id;
          });
          if(same!=state.activeAid.end()){
            if(same->magnitude<=applied.magnitude)
              *same={form,applied.index,applied.magnitude,applied.seconds};
            continue;
          }
        }
        if(state.activeAid.size()>=MaxActiveAid){failure="Too many active item effects";return false;}
        state.activeAid.push_back({form,applied.index,applied.magnitude,applied.seconds});
        continue;
      }
      const auto& magic=data.magic.at(effect.id);
      if(magic.archetype==MagicScript){
        if(!p.RunSourceEventDirect(magic.script,"ScriptEffectStart",PlayerRef,PlayerRef,failure))return false;
      }else if(magic.archetype==MagicStimpak){
        if(!p.RestoreHealth(applied.magnitude))return false;
        // General Aid use distributes a small part of the dose to the limbs.
        for(uint32_t av=25;av<=30;++av)p.ChangeActorValueDamage(PlayerRef,av,applied.magnitude*.1f,true);
      }else{
        if(!p.ChangeActorValueDamage(PlayerRef,magic.av,applied.magnitude,(magic.flags&MagicDetrimental)==0))return false;
      }
    }
    if(source.withdrawal&&source.addictionChance>0&&!state.addictions.count(source.withdrawal)){
      float chance=source.addictionChance;
      if(monitored){
        float usage=0;
        for(const auto& active:state.activeAid)
          if(active.source==form&&source.effects[active.index].id==UsageMonitorEffect)usage+=active.magnitude;
        chance=(usage-catalog_->rules.addictionThreshold)/100.f;
      }
      if(chance>0&&p.DrawScriptRandomPercent()<p.AidEntryPoint(AddictionChanceEntry,chance)*100.f)
        p.SetAddiction(source.withdrawal,true);
    }
    p.ClampAidResources();
    if(state.pipboy.aidUsed<UINT32_MAX)++state.pipboy.aidUsed;
    ++p.revision_;
    return true;
  },error);
  if(plan&&CommitScriptTransaction(*plan))return true;
  if(reason)*reason=error.empty()?"Could not consume item":error;
  return false;
}

void Player::AdvanceAid(double seconds) {
  if(state_.activeAid.empty()||!std::isfinite(seconds)||seconds<=0)return;
  using namespace fo3pipdata;
  const auto& data=catalog_->pipboy;
  for(auto& active:state_.activeAid){
    const auto* effect=SourceEffect(data,active);
    const double elapsed=std::min(seconds,active.remaining);
    if(effect&&effect->id!=UsageMonitorEffect){
      const auto& magic=data.magic.at(effect->id);
      if(magic.archetype==MagicValue&&!(magic.flags&MagicRecover))
        ChangeActorValueDamage(PlayerRef,magic.av,active.magnitude*float(elapsed),(magic.flags&MagicDetrimental)==0);
    }
    active.remaining-=elapsed;
  }
  const auto count=state_.activeAid.size();
  state_.activeAid.erase(std::remove_if(state_.activeAid.begin(),state_.activeAid.end(),
      [](const ActiveAid& active){return active.remaining<=0;}),state_.activeAid.end());
  if(count!=state_.activeAid.size()){ClampAidResources();++revision_;}
}

bool Player::ValidateAidState(const State& state) const {
  const auto& data=catalog_->pipboy;
  if(state.activeAid.size()>MaxActiveAid||state.addictions.size()>data.effectSpells.size())return false;
  std::unordered_set<uint32_t> activeEffects;
  for(const auto& active:state.activeAid){
    const auto* effect=SourceEffect(data,active);
    if(!effect||!catalog_->items.count(active.source)||!effect->duration||effect->area||effect->range||
       !std::isfinite(active.remaining)||active.remaining<=0||
       active.remaining>1e8f||!std::isfinite(active.magnitude)||active.magnitude<0||active.magnitude>1e6f)return false;
    if(effect->id==fo3pipdata::UsageMonitorEffect){
      if(!AddictionSpell(data,data.aid.at(active.source).withdrawal)||active.magnitude!=float(effect->magnitude)||
         active.remaining>float(effect->duration))return false;
      continue;
    }
    const auto magic=data.magic.find(effect->id);
    if(magic==data.magic.end()||!activeEffects.insert(effect->id).second)return false;
    const auto& m=magic->second;
    if(m.archetype==fo3pipdata::MagicScript){if(!m.temporaryPerk)return false;}
    else if((m.archetype!=fo3pipdata::MagicValue&&m.archetype!=fo3pipdata::MagicChameleon)||
            ((m.flags&fo3pipdata::MagicRecover)?!ModifierValue(m.av):!DamagePool(m.av)))return false;
  }
  for(auto spell:state.addictions)
    if(!AddictionSpell(data,spell))return false;
  return true;
}
