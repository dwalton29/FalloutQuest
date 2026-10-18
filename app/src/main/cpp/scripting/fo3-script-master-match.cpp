#include "fo3-script-master-match.h"
#include "fo3-script-evidence-budget.h"
#include "fo3-script-local-policy.h"
#include "data/fo3-esm-reader.h"
#include "dialogue/fo3-dialogue-info-policy.h"
#include <cstring>
#include <memory>
#include <unordered_map>

namespace fo3script {
namespace {
using Execution=fo3player::ScriptExecution;
using Kind=fo3player::ScriptProgramKind;
bool Is(const uint8_t* p,const char* tag){return std::memcmp(p,tag,4)==0;}
bool Same(const fo3pipdata::ResultScript& a,const fo3pipdata::ResultScript& b) noexcept {
  return a.source==b.source&&a.compiled==b.compiled&&a.header==b.header&&
    a.references==b.references&&a.records==b.records;
}
bool Empty(const fo3pipdata::ResultScript& s) noexcept {
  return s.source.empty()&&s.compiled.empty()&&s.header.empty()&&s.references.empty()&&s.records.empty();
}
bool Same(const std::vector<fo3pipdata::Condition>& a,const std::vector<fo3pipdata::Condition>& b) noexcept {
  if(a.size()!=b.size())return false;
  for(size_t n=0;n<a.size();++n){
    const auto& x=a[n];const auto& y=b[n];
    // Compare float bits, preserving signed zero and NaN payload identity.
    if(x.flags!=y.flags||x.function!=y.function||std::memcmp(&x.value,&y.value,sizeof(x.value))||
        x.a!=y.a||x.b!=y.b||x.run!=y.run||x.reference!=y.reference)return false;
  }
  return true;
}
bool Same(const Execution& a,const Execution& b) noexcept {
  const auto& x=a.origin;const auto& y=b.origin;
  return x.kind==y.kind&&x.record==y.record&&x.item==y.item&&x.sourceLine==y.sourceLine&&
    x.topic==y.topic&&x.stage==y.stage&&x.event==y.event&&a.owner==b.owner&&
    a.activator==b.activator&&a.localScript==b.localScript&&a.parent==b.parent&&
    a.reference==b.reference&&Same(a.program,b.program)&&Same(a.container,b.container)&&
    a.owningQuest==b.owningQuest&&Same(a.conditions,b.conditions)&&Same(a.questConditions,b.questConditions)&&
    a.localVariables==b.localVariables&&a.commandForms==b.commandForms&&a.expressionForms==b.expressionForms&&
    a.commandVariables==b.commandVariables&&a.expressionVariables==b.expressionVariables&&
    a.commandLocals==b.commandLocals&&a.expressionLocals==b.expressionLocals;
}
bool Contexts(const std::vector<Execution>& programs){
  std::array<size_t,32> active{};size_t depth=0;
  for(size_t n=0;n<programs.size();++n){
    const auto& e=programs[n];
    if(e.parent==Execution::NoParent){
      depth=0;
      if(e.origin.kind==Kind::ReferenceEvent){if(e.reference!=e.owner)return false;}
      else if(e.reference||e.activator)return false;
    }else{
      if(e.parent>=n||e.origin.kind!=Kind::QuestStageItem)return false;
      // An already-returned call cannot become a parent again in this preorder.
      while(depth&&active[depth-1]!=e.parent)--depth;
      if(!depth)return false;
      const auto& parent=programs[e.parent];
      if(e.reference!=parent.reference||e.activator!=parent.activator)return false;
    }
    if(depth==active.size())return false;
    active[depth++]=n;
  }
  return true;
}
const char* Type(const Execution& e){
  const auto& o=e.origin;
  if(!o.record)return nullptr;
  if(!e.localScript&&!e.localVariables.empty())return nullptr;
  for(size_t n=1;n<e.localVariables.size();++n)
    if(e.localVariables[n-1].first>=e.localVariables[n].first)return nullptr;
  switch(o.kind){
    case Kind::QuestStageItem:
      return e.owner==o.record&&!o.topic&&!o.sourceLine&&o.event.empty()&&(e.localScript||Empty(e.container))&&
        !e.owningQuest&&e.questConditions.empty()?"QUST":nullptr;
    case Kind::DialogueInfo:
      return e.owner&&o.topic&&o.item<2&&!o.stage&&!o.sourceLine&&o.event.empty()&&!e.localScript&&Empty(e.container)?"INFO":nullptr;
    case Kind::ReferenceEvent:
      return e.owner&&e.localScript==o.record&&o.sourceLine&&!o.event.empty()&&!o.item&&!o.stage&&!o.topic&&
        !e.owningQuest&&e.conditions.empty()&&e.questConditions.empty()?"SCPT":nullptr;
    default:return nullptr; // raw bodies can never claim an authored identity
  }
}
// Validate framing before the tolerant gameplay decoder. In particular XXXX
// cannot hide a truncated field, and duplicate script scalars are ambiguous.
bool Complete(const std::vector<uint8_t>& b,const std::string& type){
  size_t p=0;uint32_t extended=0;unsigned scalars=0,next=0,tpic=0,scri=0,quest=0,speaker=0,data=0;
  ScriptLocalRecordFraming locals;
  while(p<b.size()){
    if(b.size()-p<6)return false;
    const auto* tag=b.data()+p;const auto shortSize=fo3esm::ReadU16(tag+4);p+=6;
    if(Is(tag,"XXXX")){
      if(extended||shortSize!=4||b.size()-p<4)return false;
      extended=fo3esm::ReadU32(b.data()+p);p+=4;if(!extended)return false;
      continue;
    }
    const uint32_t n=extended?extended:shortSize;extended=0;
    if(n>b.size()-p)return false;
    if(type=="SCPT"&&!locals.Accept(reinterpret_cast<const char*>(tag),b.data()+p,n))return false;
    unsigned bit=0;
    if(Is(tag,"SCHR")){if(n!=20)return false;bit=1;}
    if(Is(tag,"SCDA"))bit=2;
    if(Is(tag,"SCTX"))bit=4;
    if(bit&&(scalars&bit))return false;
    scalars|=bit;
    if((Is(tag,"SCRO")||Is(tag,"SCRV"))&&n!=4)return false;
    if(type=="INFO"&&Is(tag,"NEXT")){if(n||++next>1)return false;scalars=0;}
    if(type=="INFO"&&Is(tag,"TPIC")){if(n!=4||++tpic>1)return false;}
    if(type=="INFO"&&Is(tag,"QSTI")){if(n!=4||++quest>1)return false;}
    if(type=="INFO"&&Is(tag,"ANAM")){if(n!=4||++speaker>1)return false;}
    if(type=="INFO"&&Is(tag,"DATA")){if((n!=3&&n!=4)||++data>1)return false;}
    if(type=="INFO"&&Is(tag,"TRDT")&&n<16)return false;
    if(type=="QUST"&&Is(tag,"SCRI")){if(n!=4||++scri>1)return false;}
    if((type=="QUST"||type=="INFO")&&Is(tag,"CTDA")&&n!=20&&n!=24&&n!=28)return false;
    if(type=="QUST"&&(Is(tag,"QSDT")||Is(tag,"INDX")||Is(tag,"QOBJ")))scalars=0;
    p+=n;
  }
  return !extended&&locals.Complete()&&(type!="INFO"||data==1);
}
bool Authored(const Execution& e,const fo3pipdata::Definitions& d){
  const auto& o=e.origin;
  if(o.kind==Kind::QuestStageItem){
    const auto q=d.quests.find(o.record);
    if(q==d.quests.end()||!q->second.diagnostics.empty()||e.localScript!=q->second.script)return false;
    const auto stage=q->second.stages.find(o.stage);
    return stage!=q->second.stages.end()&&o.item<stage->second.items.size()&&
      Same(e.conditions,stage->second.items[o.item].conditions)&&Same(e.program,stage->second.items[o.item].result);
  }
  if(o.kind==Kind::DialogueInfo){
    const auto t=d.topics.find(o.topic);
    return t!=d.topics.end()&&t->second.size()==1&&t->second[0].id==o.record&&
      fo3dialogue::SupportedInfoStructure(t->second[0])&&
      e.owningQuest==t->second[0].quest&&Same(e.conditions,t->second[0].conditions)&&
      Same(e.program,o.item?t->second[0].end:t->second[0].begin);
  }
  const auto s=d.scripts.find(o.record);if(s==d.scripts.end()||!Same(e.container,s->second))return false;
  const auto events=ParseEvents(s->second);if(!events.error.empty())return false;
  for(const auto& block:events.blocks)if(block.sourceLine==o.sourceLine&&block.event==o.event){
    if(!EventFilterMatches(block.filter,e.activator,fo3player::PlayerRef))return false;
    fo3pipdata::ResultScript body;body.source=block.body;return Same(e.program,body);
  }
  return false;
}
struct Close{void operator()(FILE* f)const{std::fclose(f);}};
// Only direct authored associations supported by the catalog are evidence.
// This deliberately does not infer template inheritance or runtime rebinding.
bool EventBase(const std::string& t){
  return t=="ACTI"||t=="CREA"||t=="NPC_"||t=="FURN"||t=="MSTT"||
    t=="CONT"||t=="LIGH"||t=="FLOR"||t=="STAT"||t=="TERM"||t=="DOOR"||
    t=="WEAP"||t=="ARMO"||t=="BOOK"||t=="MISC"||t=="ALCH"||t=="KEYM";
}
bool Association(const std::vector<uint8_t>& bytes,const std::string& type,
    const char* field,uint32_t& value){
  if(!Complete(bytes,type))return false;
  unsigned count=0;bool valid=true;
  fo3esm::WalkSubrecords(bytes,[&](const char* tag,const uint8_t* p,uint32_t n){
    if(std::memcmp(tag,field,4)==0){
      if(++count!=1||n!=4)valid=false;
      else value=fo3esm::ReadU32(p);
    }
  });
  return valid&&count==1&&value;
}
// Strict generic framing and at most one complete EDID; requested targets must
// have one. Other live records may have none, but cannot hide a competing name.
bool EditorName(const std::vector<uint8_t>& bytes,std::string& name,bool required){
  size_t at=0;uint32_t extended=0;bool found=false;
  while(at<bytes.size()){
    if(bytes.size()-at<6)return false;
    const auto* tag=bytes.data()+at;const auto shortSize=fo3esm::ReadU16(tag+4);at+=6;
    if(Is(tag,"XXXX")){
      if(extended||shortSize!=4||bytes.size()-at<4)return false;
      extended=fo3esm::ReadU32(bytes.data()+at);at+=4;if(!extended)return false;
      continue;
    }
    const uint32_t n=extended?extended:shortSize;extended=0;
    if(n>bytes.size()-at)return false;
    if(Is(tag,"EDID")){
      if(found||!n||bytes[at+n-1]!=0||std::memchr(bytes.data()+at,0,n-1))return false;
      name=Lower(std::string(reinterpret_cast<const char*>(bytes.data()+at),n-1));found=true;
    }
    at+=n;
  }
  return !extended&&(!required||found);
}
}
bool ScriptMasterMatch::Matches(const std::vector<Execution>& programs) const noexcept {
  if(!Verified()||programs_.size()!=programs.size())return false;
  for(size_t n=0;n<programs.size();++n)if(!Same(programs_[n],programs[n]))return false;
  return true;
}
bool MatchScriptProgramsToMaster(const std::string& master,const std::vector<Execution>& programs,
    ScriptMasterMatch& out,const std::atomic<bool>& cancel,std::string& error){
  error.clear();auto fail=[&](const std::string& why){error=why;return false;};
  if(programs.empty()||programs.size()>ScriptEvidenceMaxEntries)return fail("invalid script master evidence count");
  size_t evidenceBytes=0;
  std::unordered_map<uint32_t,std::vector<size_t>> wanted,owners,bases;
  std::unordered_map<uint32_t,std::vector<std::string>> forms;
  std::unordered_map<std::string,uint32_t> formNames;
  using Variable=fo3player::ScriptVariableBinding;
  std::unordered_map<uint32_t,std::vector<const Variable*>> variableOwners,variableBases,variableScripts;
  for(size_t n=0;n<programs.size();++n){
    const auto& e=programs[n];
    if(!ChargeScriptEvidence(e.origin.event,e.program,&e.container,evidenceBytes,&e.conditions,&e.questConditions)||
        !ChargeScriptLocals(e.localVariables,evidenceBytes)||!ChargeScriptForms(e.commandForms,evidenceBytes)||
        !ChargeScriptForms(e.expressionForms,evidenceBytes)||!ChargeScriptVariables(e.commandVariables,evidenceBytes)||
        !ChargeScriptVariables(e.expressionVariables,evidenceBytes)||
        !ChargeScriptLocals(e.commandLocals,evidenceBytes)||!ChargeScriptLocals(e.expressionLocals,evidenceBytes))
      return fail("script master evidence byte limit exceeded");
    if(!Type(programs[n]))return fail("unbound or malformed script origin");
    if(!CompleteScriptLocalRecords(e.container))return fail("malformed local records");
    if(!UnambiguousScriptLocals(e.localVariables))return fail("local names are empty or ambiguous");
    std::vector<std::pair<std::string,unsigned>> declarations;
    if(!ConsistentScriptLocalDeclarations(e.container.source,e.localVariables,&declarations))
      return fail("local declarations do not match source");
    for(const auto* bindings:{&e.commandLocals,&e.expressionLocals})for(const auto& binding:*bindings){
      if(cancel.load(std::memory_order_acquire))return fail("script master match cancelled");
      if(!e.owner||!e.localScript||binding.second.empty()||Lower(binding.second)!=binding.second||
          binding.second.find('.')!=std::string::npos||binding.second.find('\0')!=std::string::npos)
        return fail("malformed bare local binding");
      const auto slot=std::lower_bound(e.localVariables.begin(),e.localVariables.end(),binding.first,
        [](const auto& entry,uint32_t index){return entry.first<index;});
      const auto declared=std::lower_bound(declarations.begin(),declarations.end(),binding.second,
        [](const auto& entry,const std::string& name){return entry.first<name;});
      if(slot==e.localVariables.end()||slot->first!=binding.first||Lower(slot->second)!=binding.second||
          declared==declarations.end()||declared->first!=binding.second||declared->second!=1)
        return fail("bare local numeric slot differs");
    }
    for(const auto* bindings:{&e.commandForms,&e.expressionForms})for(const auto& binding:*bindings){
      if(!binding.first||binding.second.empty()||Lower(binding.second)!=binding.second||binding.second.find('\0')!=std::string::npos)
        return fail("malformed script form binding");
      const auto named=formNames.emplace(binding.second,binding.first);
      if(!named.second&&named.first->second!=binding.first)return fail("conflicting script form name bindings");
      forms[binding.first].push_back(binding.second);
    }
    for(const auto* bindings:{&e.commandVariables,&e.expressionVariables})for(const auto& binding:*bindings){
      if(!binding.owner||!binding.script||(binding.quest?bool(binding.base):!binding.base)||
          binding.ownerName.empty()||binding.variableName.empty()||Lower(binding.ownerName)!=binding.ownerName||
          Lower(binding.variableName)!=binding.variableName||binding.ownerName.find('.')!=std::string::npos||
          binding.ownerName.find('\0')!=std::string::npos||binding.variableName.find('\0')!=std::string::npos)
        return fail("malformed qualified variable binding");
      const auto named=formNames.emplace(binding.ownerName,binding.owner);
      if(!named.second&&named.first->second!=binding.owner)return fail("conflicting script form name bindings");
      forms[binding.owner].push_back(binding.ownerName);
      variableOwners[binding.owner].push_back(&binding);
      if(!binding.quest)variableBases[binding.base].push_back(&binding);
      variableScripts[binding.script].push_back(&binding);
    }
    wanted[programs[n].origin.record].push_back(n);
    if(e.origin.kind==Kind::ReferenceEvent||e.origin.kind==Kind::DialogueInfo)owners[e.owner].push_back(n);
  }
  if(!Contexts(programs))return fail("invalid script execution context tree");
  std::unique_ptr<FILE,Close> file(std::fopen(master.c_str(),"rb"));
  if(!file)return fail("script master unavailable");
  const auto size=fo3esm::FileSize(file.get());if(size<24)return fail("invalid script master size");
  struct Group{uint64_t end;uint32_t label,type;};std::vector<Group> stack;
  std::unordered_map<uint32_t,bool> seen,formsSeen;std::vector<uint8_t> bytes;
  std::unordered_map<uint32_t,std::string> variableOwnerTypes,variableBaseTypes;
  std::unordered_map<uint32_t,bool> variableScriptsSeen;
  std::unordered_map<uint32_t,bool> ownersSeen,basesSeen;
  std::unordered_map<uint32_t,std::vector<size_t>> locals;
  std::unordered_map<uint32_t,bool> localsSeen;
  for(size_t n=0;n<programs.size();++n)if(programs[n].localScript)locals[programs[n].localScript].push_back(n);
  std::unordered_map<uint32_t,std::vector<size_t>> requiredQuests;
  std::unordered_map<uint32_t,bool> questsSeen;
  std::vector<std::string> placementTypes(programs.size());
  std::vector<uint32_t> requiredSpeakers(programs.size());
  // The first complete scan authenticates programs and resolves placed owners.
  // A second header scan finds bases, INFO quests and local SCPTs independently
  // of order, including quest-only evidence with no placed owners.
  for(unsigned pass=0;pass<(owners.empty()&&locals.empty()?1u:2u);++pass){
  stack.clear();
  if(fseeko(file.get(),0,SEEK_SET))return fail("script master seek failed");
  uint64_t p=0;
  while(p<uint64_t(size)){
    if(cancel.load(std::memory_order_acquire))return fail("script master match cancelled");
    while(!stack.empty()&&p==stack.back().end)stack.pop_back();
    const uint64_t limit=stack.empty()?uint64_t(size):stack.back().end;
    if(p>limit||limit-p<24)return fail("invalid script master record boundary");
    uint8_t h[24];if(!fo3esm::ReadExact(file.get(),h,24))return fail("truncated script master header");
    const uint32_t n=fo3esm::ReadU32(h+4);
    if(Is(h,"GRUP")){
      if(n<24||n>limit-p||stack.size()>=128)return fail("invalid script master group");
      stack.push_back({p+n,fo3esm::ReadU32(h+8),fo3esm::ReadU32(h+12)});p+=24;continue;
    }
    if(n>limit-p-24)return fail("invalid script master record size");
    const auto id=fo3esm::ReadU32(h+12);const auto found=wanted.find(id);
    const bool formTarget=pass==0&&forms.count(id);
    const auto recordFlags=fo3esm::ReadU32(h+8);
    if(formTarget){
      if(!formsSeen.emplace(id,true).second)return fail("duplicate command form identity");
      if(recordFlags&0x20)return fail("deleted command form identity");
    }
    // A matching EDID on the claimed ID is insufficient if another live record
    // supplies that same normalized name. Scan every class, not just targets or
    // catalog-selected types. No additional header pass is needed.
    if(pass==0&&!formNames.empty()&&!(recordFlags&0x20)){
      std::string name;
      if(n){
        if(!fo3esm::ReadPayload(file.get(),{p+24,n,recordFlags,std::string(reinterpret_cast<const char*>(h),4)},bytes))
          return fail("malformed script form namespace payload");
      }else {
        if(recordFlags&fo3esm::FLAG_COMPRESSED)return fail("malformed script form namespace payload");
        bytes.clear();
      }
      if(!EditorName(bytes,name,formTarget))return fail("malformed script form namespace EDID");
      const auto named=formNames.find(name);
      if(named!=formNames.end()&&named->second!=id)return fail("ambiguous script form name in master");
      if(formTarget)for(const auto& expected:forms.at(id))
        if(name!=expected)return fail("command form name differs from original master");
    }
    if(pass==0&&variableOwners.count(id)){
      const std::string type(reinterpret_cast<const char*>(h),4);
      if(!variableOwnerTypes.emplace(id,type).second)return fail("duplicate qualified variable owner");
      if(recordFlags&0x20)return fail("deleted qualified variable owner");
      for(const auto* binding:variableOwners.at(id))
        if(binding->quest?type!="QUST":(type!="ACHR"&&type!="ACRE"))return fail("wrong-type qualified variable owner");
      uint32_t association=0;const bool quest=variableOwners.at(id).front()->quest;
      if(!fo3esm::ReadPayload(file.get(),{p+24,n,recordFlags,type},bytes)||
          !Association(bytes,type,quest?"SCRI":"NAME",association))return fail("malformed qualified variable owner association");
      for(const auto* binding:variableOwners.at(id))
        if(association!=(binding->quest?binding->script:binding->base))return fail("qualified variable owner association differs");
    }
    if(pass==0&&variableBases.count(id)){
      const std::string type(reinterpret_cast<const char*>(h),4);
      if(!variableBaseTypes.emplace(id,type).second)return fail("duplicate qualified variable actor base");
      if((recordFlags&0x20)||(type!="NPC_"&&type!="CREA"))return fail("deleted or wrong-type qualified variable actor base");
      uint32_t script=0;
      if(!fo3esm::ReadPayload(file.get(),{p+24,n,recordFlags,type},bytes)||!Association(bytes,type,"SCRI",script))
        return fail("malformed qualified variable actor script association");
      for(const auto* binding:variableBases.at(id))if(script!=binding->script)return fail("qualified variable actor script differs");
    }
    if(pass==0&&variableScripts.count(id)){
      const std::string type(reinterpret_cast<const char*>(h),4);
      if(!variableScriptsSeen.emplace(id,true).second)return fail("duplicate qualified variable script");
      if((recordFlags&0x20)||type!="SCPT")return fail("deleted or wrong-type qualified variable script");
      if(!fo3esm::ReadPayload(file.get(),{p+24,n,recordFlags,type},bytes)||!Complete(bytes,type))
        return fail("malformed qualified variable script");
      fo3pipdata::Definitions d;fo3pipdata::Decode(d,type,id,recordFlags,bytes,0,0,0);
      const auto script=d.scripts.find(id);
      std::vector<std::pair<std::string,unsigned>> declarations;
      if(script==d.scripts.end()||!UnambiguousScriptLocals(script->second.variables)||
          !ConsistentScriptLocalDeclarations(script->second.source,script->second.variables,&declarations))
        return fail("ambiguous or inconsistent qualified variable declarations");
      for(const auto* binding:variableScripts.at(id)){
        if(cancel.load(std::memory_order_acquire))return fail("script master match cancelled");
        const auto slot=script->second.variables.find(binding->index);
        const auto declared=std::lower_bound(declarations.begin(),declarations.end(),binding->variableName,
          [](const auto& entry,const std::string& name){return entry.first<name;});
        if(slot==script->second.variables.end()||Lower(slot->second)!=binding->variableName||
            binding->variableName.find('.')!=std::string::npos||declared==declarations.end()||
            declared->first!=binding->variableName||declared->second!=1)
          return fail("qualified variable numeric slot differs");
      }
    }
    if(pass==0&&found!=wanted.end()){
      const std::string type(reinterpret_cast<const char*>(h),4);const auto flags=fo3esm::ReadU32(h+8);
      if(!seen.emplace(id,true).second)return fail("duplicate script master identity");
      if((flags&0x20)||type!=Type(programs[found->second[0]]))return fail("deleted or wrong-type script master identity");
      if(!fo3esm::ReadPayload(file.get(),{p+24,n,flags,type},bytes)||!Complete(bytes,type))
        return fail("malformed script master payload");
      uint32_t topic=0;
      for(auto g=stack.rbegin();g!=stack.rend();++g)if(g->type==7){topic=g->label;break;}
      fo3pipdata::Definitions d;fo3pipdata::Decode(d,type,id,flags,bytes,0,0,topic);
      for(const auto index:found->second){
        if(type!=Type(programs[index])||!Authored(programs[index],d))
          return fail("script program differs from original master "+type+" "+std::to_string(id)+" entry "+std::to_string(index));
        if(type=="INFO"){
          const auto& info=d.topics.at(programs[index].origin.topic).front();
          if(!info.quest)return fail("original INFO owning quest missing");
          requiredQuests[info.quest].push_back(index);requiredSpeakers[index]=info.speaker;
        }
      }
    }
    if(pass==1&&locals.count(id)){
      if(!localsSeen.emplace(id,true).second)return fail("duplicate local script identity");
      const std::string type(reinterpret_cast<const char*>(h),4);const auto flags=fo3esm::ReadU32(h+8);
      if(type!="SCPT"||(flags&0x20))return fail("deleted or wrong-type local script");
      if(!fo3esm::ReadPayload(file.get(),{p+24,n,flags,type},bytes)||!Complete(bytes,type))
        return fail("malformed local script payload");
      fo3pipdata::Definitions d;fo3pipdata::Decode(d,type,id,flags,bytes,0,0,0);
      const auto script=d.scripts.find(id);
      if(script==d.scripts.end())return fail("local script definition missing");
      for(const auto index:locals.at(id)){
        if(!Same(programs[index].container,script->second))return fail("local script differs from original master");
        const auto& variables=programs[index].localVariables;
        if(variables.size()!=script->second.variables.size())return fail("local declarations differ from original master");
        for(const auto& variable:variables){
          const auto original=script->second.variables.find(variable.first);
          if(original==script->second.variables.end()||original->second!=variable.second)
            return fail("local declarations differ from original master");
        }
      }
    }
    if(pass==1&&requiredQuests.count(id)){
      if(!questsSeen.emplace(id,true).second)return fail("duplicate INFO owning quest identity");
      const std::string type(reinterpret_cast<const char*>(h),4);const auto flags=fo3esm::ReadU32(h+8);
      if(type!="QUST"||(flags&0x20))return fail("deleted or wrong-type INFO owning quest");
      if(!fo3esm::ReadPayload(file.get(),{p+24,n,flags,type},bytes)||!Complete(bytes,type))
        return fail("malformed INFO owning quest payload");
      fo3pipdata::Definitions d;fo3pipdata::Decode(d,type,id,flags,bytes,0,0,0);
      const auto quest=d.quests.find(id);
      if(quest==d.quests.end()||!quest->second.diagnostics.empty())
        return fail("malformed INFO owning quest definition");
      for(const auto index:requiredQuests.at(id))
        if(!Same(programs[index].questConditions,quest->second.conditions))
          return fail("INFO owning quest conditions differ from original master");
    }
    const auto binding=(pass==0?owners:bases).find(id);
    const auto& bindings=pass==0?owners:bases;
    if(binding!=bindings.end()){
      auto& observed=pass==0?ownersSeen:basesSeen;
      if(!observed.emplace(id,true).second)return fail("duplicate script owner association identity");
      const std::string type(reinterpret_cast<const char*>(h),4);const auto flags=fo3esm::ReadU32(h+8);
      const bool placed=pass==0&&(type=="REFR"||type=="ACHR"||type=="ACRE");
      if((flags&0x20)||(pass==0?(!placed&&type!="QUST"):!EventBase(type)))
        return fail("deleted or wrong-type script owner association");
      bool needsScript=false;
      for(const auto index:binding->second){
        const bool info=programs[index].origin.kind==Kind::DialogueInfo;
        if(pass==0&&info&&type!="ACHR"&&type!="ACRE")return fail("dialogue speaker is not an authored actor placement");
        needsScript=needsScript||!info;
      }
      uint32_t association=0;
      if(!fo3esm::ReadPayload(file.get(),{p+24,n,flags,type},bytes)||
          ((placed||needsScript)?!Association(bytes,type,placed?"NAME":"SCRI",association):!Complete(bytes,type)))
        return fail("malformed or missing script owner association");
      for(const auto index:binding->second){
        if(placed){bases[association].push_back(index);placementTypes[index]=type;}
        else {
          if(pass==1){
            const auto& placement=placementTypes[index];
            if((placement=="ACHR"&&type!="NPC_")||(placement=="ACRE"&&type!="CREA")||
                (placement=="REFR"&&(type=="NPC_"||type=="CREA")))
              return fail("script owner placement and base types differ");
            // ANAM constrains an INFO's actor base, not the placed reference.
            // Check on pass two: the speaker may precede its INFO on pass one.
            if(requiredSpeakers[index]&&requiredSpeakers[index]!=id)
              return fail("dialogue speaker differs from original INFO ANAM");
          }
          if(programs[index].origin.kind==Kind::ReferenceEvent&&association!=programs[index].origin.record)
            return fail("script owner SCRI differs from original master");
        }
      }
    }
    p+=24+n;if(fseeko(file.get(),p,SEEK_SET))return fail("script master seek failed");
  }
  }
  if(seen.size()!=wanted.size())return fail("script master identity missing");
  if(formsSeen.size()!=forms.size())return fail("command form identity missing");
  if(ownersSeen.size()!=owners.size()||basesSeen.size()!=bases.size())return fail("script owner association identity missing");
  if(questsSeen.size()!=requiredQuests.size())return fail("INFO owning quest identity missing");
  if(localsSeen.size()!=locals.size())return fail("local script identity missing");
  if(variableOwnerTypes.size()!=variableOwners.size()||variableBaseTypes.size()!=variableBases.size()||
      variableScriptsSeen.size()!=variableScripts.size())return fail("qualified variable identity missing");
  for(const auto& entry:programs)for(const auto* bindings:{&entry.commandVariables,&entry.expressionVariables})
    for(const auto& binding:*bindings)if(!binding.quest){
      const auto& ownerType=variableOwnerTypes.at(binding.owner);const auto& baseType=variableBaseTypes.at(binding.base);
      if((ownerType=="ACHR"&&baseType!="NPC_")||(ownerType=="ACRE"&&baseType!="CREA"))
        return fail("qualified variable placement and base types differ");
    }
  if(cancel.load(std::memory_order_acquire))return fail("script master match cancelled");
  ScriptMasterMatch next;next.programs_=programs;next.verified_=true;
  out=std::move(next);return true;
}
}
