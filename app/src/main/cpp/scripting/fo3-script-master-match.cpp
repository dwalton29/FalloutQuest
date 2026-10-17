#include "fo3-script-master-match.h"
#include "fo3-script-evidence-budget.h"
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
bool Same(const Execution& a,const Execution& b) noexcept {
  const auto& x=a.origin;const auto& y=b.origin;
  return x.kind==y.kind&&x.record==y.record&&x.item==y.item&&x.sourceLine==y.sourceLine&&
    x.topic==y.topic&&x.stage==y.stage&&x.event==y.event&&a.owner==b.owner&&
    a.activator==b.activator&&a.localScript==b.localScript&&a.parent==b.parent&&
    a.reference==b.reference&&Same(a.program,b.program)&&Same(a.container,b.container);
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
  switch(o.kind){
    case Kind::QuestStageItem:
      return e.owner==o.record&&!o.topic&&!o.sourceLine&&o.event.empty()&&Empty(e.container)?"QUST":nullptr;
    case Kind::DialogueInfo:
      return e.owner&&o.topic&&o.item<2&&!o.stage&&!o.sourceLine&&o.event.empty()&&!e.localScript&&Empty(e.container)?"INFO":nullptr;
    case Kind::ReferenceEvent:
      return e.owner&&e.localScript==o.record&&o.sourceLine&&!o.event.empty()&&!o.item&&!o.stage&&!o.topic?"SCPT":nullptr;
    default:return nullptr; // raw bodies can never claim an authored identity
  }
}
// Validate framing before the tolerant gameplay decoder. In particular XXXX
// cannot hide a truncated field, and duplicate script scalars are ambiguous.
bool Complete(const std::vector<uint8_t>& b,const std::string& type){
  size_t p=0;uint32_t extended=0;unsigned scalars=0,next=0,tpic=0,scri=0,quest=0,speaker=0,data=0;
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
  return !extended&&(type!="INFO"||data==1);
}
bool Authored(const Execution& e,const fo3pipdata::Definitions& d){
  const auto& o=e.origin;
  if(o.kind==Kind::QuestStageItem){
    const auto q=d.quests.find(o.record);
    if(q==d.quests.end()||!q->second.diagnostics.empty()||e.localScript!=q->second.script)return false;
    const auto stage=q->second.stages.find(o.stage);
    return stage!=q->second.stages.end()&&o.item<stage->second.items.size()&&Same(e.program,stage->second.items[o.item].result);
  }
  if(o.kind==Kind::DialogueInfo){
    const auto t=d.topics.find(o.topic);
    return t!=d.topics.end()&&t->second.size()==1&&t->second[0].id==o.record&&
      fo3dialogue::SupportedInfoStructure(t->second[0])&&
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
  for(size_t n=0;n<programs.size();++n){
    const auto& e=programs[n];
    if(!ChargeScriptEvidence(e.origin.event,e.program,&e.container,evidenceBytes))
      return fail("script master evidence byte limit exceeded");
    if(!Type(programs[n]))return fail("unbound or malformed script origin");
    wanted[programs[n].origin.record].push_back(n);
    if(e.origin.kind==Kind::ReferenceEvent||e.origin.kind==Kind::DialogueInfo)owners[e.owner].push_back(n);
  }
  if(!Contexts(programs))return fail("invalid script execution context tree");
  std::unique_ptr<FILE,Close> file(std::fopen(master.c_str(),"rb"));
  if(!file)return fail("script master unavailable");
  const auto size=fo3esm::FileSize(file.get());if(size<24)return fail("invalid script master size");
  struct Group{uint64_t end;uint32_t label,type;};std::vector<Group> stack;
  std::unordered_map<uint32_t,bool> seen;std::vector<uint8_t> bytes;
  std::unordered_map<uint32_t,bool> ownersSeen,basesSeen;
  std::unordered_map<uint32_t,bool> requiredQuests,questsSeen;
  std::vector<std::string> placementTypes(programs.size());
  std::vector<uint32_t> requiredSpeakers(programs.size());
  // The first complete scan authenticates programs and resolves placed owners.
  // A second header scan finds bases and INFO quests independently of order.
  for(unsigned pass=0;pass<(owners.empty()?1u:2u);++pass){
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
          requiredQuests.emplace(info.quest,true);requiredSpeakers[index]=info.speaker;
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
  if(ownersSeen.size()!=owners.size()||basesSeen.size()!=bases.size())return fail("script owner association identity missing");
  if(questsSeen.size()!=requiredQuests.size())return fail("INFO owning quest identity missing");
  if(cancel.load(std::memory_order_acquire))return fail("script master match cancelled");
  ScriptMasterMatch next;next.programs_=programs;next.verified_=true;
  out=std::move(next);return true;
}
}
