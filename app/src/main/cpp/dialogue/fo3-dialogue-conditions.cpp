#include "fo3-dialogue-conditions.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cctype>
namespace fo3dialogue {
bool Compare(float a,float b,uint8_t flags) {
  if(!std::isfinite(a)||!std::isfinite(b))return false;
  switch(flags>>5){case 0:return a==b;case 1:return a!=b;case 2:return a>b;case 3:return a>=b;case 4:return a<b;case 5:return a<=b;default:return false;}
}
uint64_t VariableKey(const fo3pipdata::Definitions& d,uint32_t owner,uint32_t index,bool quest) {
  uint32_t script=0;
  if(quest){auto q=d.quests.find(owner);if(q!=d.quests.end())script=q->second.script;}
  else {auto ref=d.referenceScripts.find(owner);if(ref!=d.referenceScripts.end()){
    auto a=d.dialogueActors.find(ref->second.second);if(a!=d.dialogueActors.end())script=a->second.script;
  }}
  auto s=d.scripts.find(script);
  return s!=d.scripts.end()&&s->second.variables.count(index)?(uint64_t(owner)<<32)|index:0;
}
uint64_t VariableKey(const fo3pipdata::Definitions& d,const std::string& name) {
  auto key=name;for(auto& c:key)c=char(std::tolower((unsigned char)c));
  auto dot=key.find('.');if(dot==std::string::npos)return 0;
  auto owner=d.formNames.find(key.substr(0,dot));if(owner==d.formNames.end())return 0;
  uint32_t script=0;
  auto q=d.quests.find(owner->second);if(q!=d.quests.end())script=q->second.script;
  else {auto ref=d.referenceScripts.find(owner->second);if(ref!=d.referenceScripts.end()){
    auto a=d.dialogueActors.find(ref->second.second);if(a!=d.dialogueActors.end())script=a->second.script;
  }}
  auto s=d.scripts.find(script);if(s==d.scripts.end())return 0;
  for(auto& v:s->second.variables){auto n=v.second;for(auto& c:n)c=char(std::tolower((unsigned char)c));if(n==key.substr(dot+1))return(uint64_t(owner->second)<<32)|v.first;}
  return 0;
}
static bool Value(const fo3pipdata::Condition& c,const Context& ctx,float& value,std::string& error) {
  if(!ctx.player){error="missing player";return false;}
  const auto& p=*ctx.player;const auto& d=p.Definitions().pipboy;const auto& state=p.Snapshot();
  ActorContext who=ctx.speaker;
  if(c.run==1)who=ctx.target;
  else if(c.run==2){if(c.reference==0x14)who=ctx.target;else if(c.reference!=ctx.speaker.reference){
    auto ref=d.targets.find(c.reference);
    if(ref==d.targets.end()||(c.function!=35&&c.function!=46)){error="reference context unavailable";return false;}
    who.reference=c.reference;who.base=ref->second.base;
  }}
  else if(c.run!=0){error="unsupported run-on context";return false;}
  const bool pc=who.reference==0x14;
  const auto* def=fo3pipdata::ActorCategory(d,pc?7:who.base,(c.function==71||c.function==73)?4:1);
  auto quest=state.pipboy.quests.find(c.a);
  switch(c.function) {
  case 1: {ActorContext other;if(c.a==ctx.speaker.reference)other=ctx.speaker;else if(c.a==ctx.target.reference)other=ctx.target;else {error="distance reference unavailable";return false;}
    value=std::sqrt((who.x-other.x)*(who.x-other.x)+(who.y-other.y)*(who.y-other.y)+(who.z-other.z)*(who.z-other.z));return true;}
  case 14:
    if(!pc){error="NPC actor value unavailable";return false;}
    if(c.a>=5&&c.a<=11)value=state.special[c.a-5];
    else if(c.a>=32&&c.a<=45)value=state.skills[c.a-32];
    else if(c.a==23)value=state.karma;
    else if(c.a==16)value=p.Health();
    else if(c.a==12)value=p.ActionPoints();
    else if(c.a==46)value=float(p.InventoryWeight());
    else {error="actor value unsupported";return false;}return true;
  case 35: {auto ref=d.targets.find(who.reference);
    if(ref==d.targets.end()){error="GetDisabled reference unavailable";return false;}
    if(ref->second.parent){error="GetDisabled enable-parent state unsupported";return false;}
    value=bool(ref->second.flags&0x800);return true;}
  case 84: {
    if(!d.dialogueActors.count(c.a)){error="GetDeadCount actor base unavailable";return false;}
    // Count canonical original ACHRs and persisted deaths, independently of residency.
    // Respawning actors require a cumulative death counter and remain unsupported.
    auto a=p.Definitions().weapons.actors.find(c.a);
    if(a==p.Definitions().weapons.actors.end()||(a->second.flags&8)){error="GetDeadCount respawning/unknown actor unsupported";return false;}
    value=0;for(const auto& ref:p.Definitions().actorPlacements)if(ref.second.base==c.a&&!(ref.second.flags&0x20)) {
      const auto health=p.ActorHealth(ref.first);if(health<0){error="GetDeadCount actor health unavailable";return false;}if(health==0)++value;
    }
    return true;}
  case 46:{const float health=pc?p.Health():p.ActorHealth(who.reference);if(health<0){error="actor health unavailable";return false;}value=health<=0;return true;}
  case 47:
    if(!pc){error="NPC inventory state unavailable";return false;}
    value=0;for(auto& s:state.inventory)if(s.formId==c.a)value+=s.count;return true;
  case 50:value=state.pipboy.talkedActors.count(who.reference);return true;
  case 53:case 79: {auto key=VariableKey(d,c.a,c.b,c.function==79);if(!key){error="script variable unavailable";return false;}
    if(ctx.activationVariables){auto v=ctx.activationVariables->find(key);if(v!=ctx.activationVariables->end()){value=v->second;return true;}}
    auto v=state.pipboy.dialogueVariables.find(key);value=v==state.pipboy.dialogueVariables.end()?0:v->second;return true;}
  case 56:{auto q=d.quests.find(c.a);if(q==d.quests.end()){error="quest unavailable";return false;}value=quest==state.pipboy.quests.end()?(q->second.flags&1)!=0:quest->second.running;return true;}
  case 58:value=quest==state.pipboy.quests.end()?0:quest->second.stage;return d.quests.count(c.a)!=0;
  case 59:value=quest!=state.pipboy.quests.end()&&quest->second.stages.count(c.b);return d.quests.count(c.a)!=0;
  case 67:value=who.cell==c.a;return who.cell!=0;
  case 68:case 69:case 70:case 71:case 73:
    if(!def){error="actor identity unavailable";return false;}
    if(c.function==68)value=def->actorClass==c.a;
    if(c.function==69)value=def->race==c.a;
    if(c.function==70)value=uint32_t(def->female)==c.a;
    if(c.function==71)value=def->factions.count(c.a);
    if(c.function==73){auto rank=def->factions.find(c.a);value=rank==def->factions.end()?-1:rank->second;}return true;
  case 72:value=(pc?7:who.base)==c.a;return true;
  case 74:if(!p.GlobalValue(c.a,value)){error="global unavailable";return false;}return true;
  case 77:if(!std::isfinite(ctx.randomPercent)||ctx.randomPercent<0||ctx.randomPercent>99){error="random percent sample unavailable";return false;}value=ctx.randomPercent;return true;
  case 80:if(!pc){error="NPC effective level unavailable";return false;}value=state.level;return true;
  case 131:value=uint32_t(d.playerFemale)==c.a;return true;
  case 136:value=who.reference==c.a;return true;
  case 141:value=ctx.talking&&who.reference==ctx.speaker.reference;return true;
  case 161:if(!who.packageKnown){error="package not executing";return false;}value=who.package==c.a;return true;
  case 289:value=who.combat;return true;
  case 546:if(!d.quests.count(c.a)){error="quest unavailable";return false;}value=quest!=state.pipboy.quests.end()&&quest->second.status!=fo3pipdata::Completion::Active;return true;
  case 310:value=who.world==c.a;return who.world!=0;
  case 372:{auto list=d.formLists.find(c.a);if(list==d.formLists.end()){error="form list unavailable";return false;}value=std::find(list->second.begin(),list->second.end(),pc?7:who.base)!=list->second.end();return true;}
  case 427:{auto voice=fo3pipdata::ActorVoice(d,who.base);if(!voice){error="voice type unavailable";return false;}value=voice==c.a;return true;}
  default:error="unsupported function "+std::to_string(c.function);return false;
  }
}
bool Conditions(const std::vector<fo3pipdata::Condition>& list,const Context& ctx,std::string& error) {
  error.clear();bool group=false;
  for(const auto& c:list) {
    float value=0,comparison=c.value;
    if((c.flags&0x1a)||!Value(c,ctx,value,error)){if(error.empty())error="unsupported CTDA flags";return false;}
    if(c.flags&4){uint32_t id;std::memcpy(&id,&c.value,4);if(!ctx.player->GlobalValue(id,comparison)){error="comparison global unavailable";return false;}}
    group=group||Compare(value,comparison,c.flags);
    if(!(c.flags&1)){if(!group)return false;group=false;}
  }
  // The record boundary closes the final OR group. Original MSObjectives
  // has valid items with every condition marked OR, including the last one.
  if(!list.empty()&&(list.back().flags&1))return group;
  return true;
}
}
