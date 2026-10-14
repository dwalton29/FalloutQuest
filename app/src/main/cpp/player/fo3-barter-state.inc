// Barter is a projection of the existing persistent player/container inventories.
// All live mutations occur after a complete candidate has passed validation.
#include <cmath>
#include <set>
#include "data/fo3-esm-reader.h"
namespace fo3player {
namespace {
bool SameBarterStacks(const std::vector<Stack>& a,const std::vector<Stack>& b) {
  if(a.size()!=b.size())return false;
  for(size_t i=0;i<a.size();++i){const auto& x=a[i];const auto& y=b[i];
    if(x.id!=y.id||x.formId!=y.formId||x.count!=y.count||x.condition!=y.condition||
       x.equipped!=y.equipped||x.loadedRounds!=y.loadedRounds||x.needsAction!=y.needsAction||
       x.ownership.owner!=y.ownership.owner||x.ownership.global!=y.ownership.global||
       x.ownership.rank!=y.ownership.rank||x.ownership.explicitOwner!=y.ownership.explicitOwner||
       x.ownership.valid!=y.ownership.valid)return false;
  }return true;
}
const Stack* BarterStack(const std::vector<Stack>& v,uint64_t id) {
  for(const auto& s:v)if(s.id==id)return &s;
  return nullptr;
}
int64_t BarterCaps(const std::vector<Stack>& v) {
  int64_t n=0;for(const auto& s:v)if(s.formId==0xf)n+=s.count;return n;
}
}
bool Player::MerchantAvailable(uint32_t actor) const {
  const auto m=catalog_->merchantContainers.find(actor);
  const auto r=catalog_->references.find(actor);
  if(m==catalog_->merchantContainers.end()||!m->second||r==catalog_->references.end()||!r->second.valid)return false;
  bool disabled=false;std::string error;
  if(!ReferenceDisabled(actor,disabled,error)||disabled||ActorHealth(actor)<=0)return false;
  auto saved=state_.actors.find(actor);if(saved!=state_.actors.end()){
    if(saved->second.dead||saved->second.hostile)return false;
    if(saved->second.package){const auto package=catalog_->pipboy.packages.find(saved->second.package);
      if(package==catalog_->pipboy.packages.end()||!(package->second.flags&1))return false;}
  }
  const auto chest=catalog_->references.find(m->second);
  if(chest==catalog_->references.end()||!chest->second.valid)return false;
  const auto c=catalog_->containers.find(chest->second.base);
  if(c==catalog_->containers.end()||!c->second.valid||c->second.script)return false;
  // Inherited services require template resolution, rather than the actor's
  // personal equipment. Resolve only the authored Use AI Data template bit.
  uint32_t base=r->second.base;std::unordered_set<uint32_t> seen;
  for(unsigned depth=0;depth<16;++depth){
    if(!seen.insert(base).second)return false;
    const auto a=catalog_->weapons.actors.find(base);
    if(a!=catalog_->weapons.actors.end()&&(a->second.templates&0x10)){base=a->second.templateId;continue;}
    const auto service=catalog_->merchantServices.find(base);
    return service!=catalog_->merchantServices.end()&&(service->second&0x47f)!=0;
  }return false;
}
bool Player::QueueBarter(uint32_t actor,int discount) {
  if(discount < -100||discount>100||!MerchantAvailable(actor)||pendingBarters_.size()>=8)return false;
  pendingBarters_.push_back({actor,discount});++scriptEffectRevision_;return true;
}
bool Player::PollBarter(BarterRequest& out) {
  if(pendingBarters_.empty())return false;
  out=pendingBarters_.front();pendingBarters_.erase(pendingBarters_.begin());++scriptEffectRevision_;return true;
}
bool Player::OpenBarter(uint32_t actor,int discount,BarterSession& out) {
  if(discount < -100||discount>100||!MerchantAvailable(actor))return false;
  const auto chest=catalog_->merchantContainers.at(actor);
  // Merchant storage can be locked, buried, and nonresident. This is not a
  // player loot action, so it neither opens the lock nor emits theft events.
  if(!RestockMerchant(actor,chest)||!PrepareInventory(chest,false))return false;
  if(!state_.merchantVisits.count(chest)){state_.merchantVisits[chest]={state_.gameDaysPassed,state_.gameHour};++revision_;}
  BarterSession candidate;candidate.actor=actor;candidate.container=chest;candidate.discount=discount;
  candidate.owner=this;candidate.skill=state_.skills[size_t(Skill::Barter)];candidate.perks=state_.pipboy.perks;
  candidate.player=state_.inventory;candidate.merchant=state_.containers.at(chest);
  out=std::move(candidate);return true;
}
bool Player::BarterItemAllowed(uint32_t actor,const Stack& s) const {
  auto found=catalog_->items.find(s.formId);
  if(found==catalog_->items.end()||s.count<=0||s.formId==0xf||IsQuestObject(s.formId))return false;
  const auto& i=found->second;
  // Item scripts require OnAdd/OnDrop event support before being transferred.
  if(!i.playable||i.cannotDrop||i.script||!i.valueKnown||i.name.empty())return false;
  const auto r=catalog_->references.find(actor);if(r==catalog_->references.end())return false;
  uint32_t base=r->second.base;
  for(unsigned d=0;d<16;++d){const auto a=catalog_->weapons.actors.find(base);
    if(a==catalog_->weapons.actors.end()||!(a->second.templates&0x10))break;
    base=a->second.templateId;if(d==15)return false;}
  const auto services=catalog_->merchantServices.find(base);if(services==catalog_->merchantServices.end())return false;
  uint32_t bit=0;
  switch(i.kind){
  case ItemKind::Weapon:case ItemKind::Ammo:bit=1;break;
  case ItemKind::Armour:bit=2;break;
  case ItemKind::Book:bit=8;break;
  case ItemKind::Misc:bit=0x400;break;
  case ItemKind::Aid:
    switch(i.equipType){case 10:bit=0x20;break;case 11:bit=0x40;break;
      case 12:bit=0x10;break;case 13:bit=4;break;default:return false;}break;
  default:return false;
  }
  return (services->second&bit)!=0;
}
bool Player::BarterPrice(const Stack& s,bool buying,int discount,float& out) const {
  if(discount < -100||discount>100||!std::isfinite(s.condition)||s.condition<0||s.condition>1)return false;
  auto f=catalog_->items.find(s.formId);if(f==catalog_->items.end()||!f->second.valueKnown)return false;
  const auto& item=f->second;const auto& settings=catalog_->barterSettings;
  auto setting=[&](const char* name,float& value){auto v=settings.find(name);if(v==settings.end()||!std::isfinite(v->second))return false;value=v->second;return true;};
  float base=0,mult=0;
  if(!setting(buying?"fBarterBuyBase":"fBarterSellBase",base)||!setting(buying?"fBarterBuyMult":"fBarterSellMult",mult))return false;
  float value=float(item.value);
  if(item.maxCondition){float b=0,m=0,e=1.5f; // Fallout3.exe GMST default at file 0xCFD424.
    if(!setting("fItemConditionValueBase",b)||!setting("fItemConditionValueMult",m))return false;
    if(settings.count("fItemConditionValueExp")&&!setting("fItemConditionValueExp",e))return false;
    value*=b+(1-b)*m*std::pow(s.condition*10,e);
  }
  const float effective=std::max(0.f,value);
  float price=effective*(base+state_.skills[size_t(Skill::Barter)]*mult/100.f);
  price*=buying?(1-discount/100.f):(1+discount/100.f);
  if(buying){
    struct Effect {uint32_t perk;size_t index;const fo3pipdata::PerkEffect* effect;};
    std::vector<Effect> effects;
    for(const auto& owned:state_.pipboy.perks){const auto perk=catalog_->pipboy.perks.find(owned.first);
      if(perk==catalog_->pipboy.perks.end())return false;
      for(size_t index=0;index<perk->second.effects.size();++index){const auto& e=perk->second.effects[index];
        if(e.type==2&&e.entryPoint==17&&e.rank<owned.second)effects.push_back({owned.first,index,&e});}
    }
    std::sort(effects.begin(),effects.end(),[](const Effect& a,const Effect& b){
      if(a.effect->priority!=b.effect->priority)return a.effect->priority<b.effect->priority;
      return a.perk!=b.perk?a.perk<b.perk:a.index<b.index;});
    for(const auto& entry:effects){const auto& e=*entry.effect;
      // Item-context and scripted entry points need a dedicated evaluator.
      for(const auto& tab:e.conditions)if(!tab.second.empty())return false;
      if(e.scripted||e.parameterType!=1||e.parameters.size()!=4)return false;
      const float modifier=fo3esm::ReadF32(e.parameters.data());if(!std::isfinite(modifier))return false;
      switch(e.function){case 1:price=modifier;break;case 2:price+=modifier;break;case 3:price*=modifier;break;default:return false;}
    }
  }
  // Original menu 0x916EF0: half-up quantization at one cap, or one
  // tenth below/equal to one cap, followed by the base-value bound.
  if(!std::isfinite(price)||price<0||double(price)>INT32_MAX)return false;
  const float quantum=price>1.f?1.f:.1f;
  const float scaled=price/quantum;
  if(double(scaled)>INT32_MAX)return false;
  const int32_t integral=int32_t(scaled);
  const float rounded=float(integral)+(scaled-float(integral)>=.5f?1.f:0.f);
  price=rounded*quantum;
  price=buying?std::max(effective,price):std::min(effective,price);
  if(!std::isfinite(price)||double(price)>INT32_MAX||price<0)return false;
  out=price;return true;
}
bool Player::BarterBalance(const BarterSession& session,int64_t& net,std::string& error) const {
  error.clear();net=0;
  if(session.owner!=this||session.skill!=state_.skills[size_t(Skill::Barter)]||session.perks!=state_.pipboy.perks||session.discount < -100||session.discount>100||!MerchantAvailable(session.actor)||
     catalog_->merchantContainers.at(session.actor)!=session.container){error="Merchant unavailable";return false;}
  const auto live=state_.containers.find(session.container);
  if(live==state_.containers.end()||!SameBarterStacks(session.player,state_.inventory)||!SameBarterStacks(session.merchant,live->second)){
    error="Inventory changed. Reopen barter.";return false;}
  float totalPrice=0;
  std::set<std::pair<bool,uint64_t>> unique;
  for(const auto& selected:session.selections){
    const auto* s=BarterStack(selected.buying?session.merchant:session.player,selected.stack);float price=0;
    if(!s||selected.count<=0||selected.count>s->count||!unique.emplace(selected.buying,selected.stack).second||
       !BarterItemAllowed(session.actor,*s)||!BarterPrice(*s,selected.buying,session.discount,price)){
      error="Invalid item, quantity or unsupported price";return false;}
    const float amount=price*float(selected.count);
    totalPrice+=selected.buying?amount:-amount;
    if(!std::isfinite(totalPrice)||double(std::abs(totalPrice))>INT32_MAX){error="Transaction too large";return false;}
  }
  net=int64_t(totalPrice); // Caps are integer currency; round only after stack totals.
  const auto playerCaps=BarterCaps(session.player),merchantCaps=BarterCaps(session.merchant);
  if(playerCaps>INT32_MAX||merchantCaps>INT32_MAX||net>playerCaps||-net>merchantCaps||
     playerCaps-net>INT32_MAX||merchantCaps+net>INT32_MAX){error=net>0?"Not enough player caps":"Not enough merchant caps";return false;}
  return true;
}
bool Player::ConfirmBarter(BarterSession& session,std::string& error) {
  int64_t net=0;if(!BarterBalance(session,net,error))return false;
  if(session.selections.empty()){error="No items selected";return false;}
  State candidate=state_;auto& merchant=candidate.containers.at(session.container);
  auto transfer=[&](std::vector<Stack>& src,std::vector<Stack>& dst,const BarterSelection& selected){
    auto it=std::find_if(src.begin(),src.end(),[&](const Stack& s){return s.id==selected.stack;});
    if(it==src.end()||dst.size()>=10000)return false;
    Stack moved=*it;moved.count=selected.count;moved.equipped=false;moved.ownership={};
    if(it->count>selected.count){if(candidate.nextStackId==UINT64_MAX)return false;moved.id=candidate.nextStackId++;it->count-=selected.count;}
    else src.erase(it);
    dst.push_back(moved);return true;
  };
  for(const auto& selected:session.selections)if(!transfer(selected.buying?merchant:candidate.inventory,selected.buying?candidate.inventory:merchant,selected)){
    error="Inventory capacity exceeded";return false;}
  auto caps=[&](std::vector<Stack>& v,int64_t amount){
    auto first=std::find_if(v.begin(),v.end(),[](const Stack& s){return s.formId==0xf;});
    uint64_t id=first==v.end()?0:first->id;
    v.erase(std::remove_if(v.begin(),v.end(),[](const Stack& s){return s.formId==0xf;}),v.end());
    if(!amount)return true;
    if(v.size()>=10000||(!id&&candidate.nextStackId==UINT64_MAX))return false;
    v.push_back({id?id:candidate.nextStackId++,0xf,int32_t(amount),1,false});return true;
  };
  if(!caps(candidate.inventory,BarterCaps(session.player)-net)||!caps(merchant,BarterCaps(session.merchant)+net)){
    error="Currency capacity exceeded";return false;}
  size_t total=0;for(const auto& c:candidate.containers)total+=c.second.size();
  if(total>30000||revision_==UINT64_MAX){error="Save capacity exceeded";return false;}
  BarterSession refreshed=session;refreshed.player=candidate.inventory;refreshed.merchant=merchant;refreshed.selections.clear();
  // All allocations, including the next UI snapshot, precede publication.
  using std::swap;swap(state_,candidate);swap(session,refreshed);++revision_;
  return true;
}
} // namespace fo3player

namespace fo3player {
void Player::TouchMerchantCells(uint32_t cell) {
  if(!cell)return;
  for(const auto& merchant:catalog_->merchantContainers){auto visit=state_.merchantVisits.find(merchant.second);
    if(visit==state_.merchantVisits.end())continue;
    const auto actor=catalog_->references.find(merchant.first),chest=catalog_->references.find(merchant.second);
    if(actor==catalog_->references.end()||chest==catalog_->references.end())continue;
    const auto saved=state_.actors.find(merchant.first);
    const auto actorCell=saved==state_.actors.end()?actor->second.cell:saved->second.cell;
    if(cell!=actorCell&&cell!=chest->second.cell)continue;
    if(!RestockMerchant(merchant.first,merchant.second))continue;
    auto current=state_.merchantVisits.find(merchant.second);
    if(current->second.day!=state_.gameDaysPassed||current->second.hour!=state_.gameHour){
      current->second={state_.gameDaysPassed,state_.gameHour};++revision_;++actorPoseRevisions_;
    }
  }
}
bool Player::RestockMerchant(uint32_t actor,uint32_t chest) {
  const auto visit=state_.merchantVisits.find(chest);
  if(visit==state_.merchantVisits.end()||!scriptPlayerPositionKnown_)return true;
  const double now=double(state_.gameDaysPassed)*24+state_.gameHour;
  const double then=double(visit->second.day)*24+visit->second.hour;
  if(now-then<catalog_->merchantRespawnHours)return true;
  const auto& ref=catalog_->references.at(chest);const auto& base=catalog_->containers.at(ref.base);
  // Encounter-zone overrides and scripted containers require the zone/script
  // reset runtime. Do not fabricate stock for those unsupported cases.
  if(!base.respawns||base.script||catalog_->encounterZoneReferences.count(chest)||!actor)return true;
  Player candidate(*this,ScriptCandidateTag{});
  candidate.state_.containers.erase(chest);
  candidate.state_.merchantVisits[chest]={state_.gameDaysPassed,state_.gameHour};
  if(!candidate.PrepareInventory(chest,false))return false;
  using std::swap;swap(state_,candidate.state_);++revision_;return true;
}
}

namespace fo3player {
bool Player::ModActorDisposition(uint32_t actor,int32_t amount) {
  const auto ref=catalog_->references.find(actor);
  if(ref==catalog_->references.end()||!catalog_->pipboy.dialogueActors.count(ref->second.base))return false;
  const auto saved=state_.actorDispositionModifiers.find(actor);
  const int64_t value=int64_t(saved==state_.actorDispositionModifiers.end()?0:saved->second)+amount;
  if(value<INT32_MIN||value>INT32_MAX||(!state_.actorDispositionModifiers.count(actor)&&state_.actorDispositionModifiers.size()>=10000))return false;
  state_.actorDispositionModifiers[actor]=int32_t(value);++revision_;return true;
}
}
