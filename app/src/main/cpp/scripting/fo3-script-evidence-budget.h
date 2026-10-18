#pragma once
#include "player/fo3-player-state.h"

namespace fo3script {
inline constexpr size_t ScriptEvidenceMaxEntries=4096;
inline constexpr size_t ScriptEvidenceMaxBytes=16u*1024u*1024u;

// Account retained value bytes before copying. This includes fixed entry and
// raw-record objects, strings (with terminators), byte arrays and FormIDs.
// Allocator bookkeeping, spare capacity and other preparation work are separate.
// Failed charges leave the caller's running total unchanged.
inline bool ChargeScriptEvidence(const std::string& event,
    const fo3pipdata::ResultScript& program,const fo3pipdata::ResultScript* container,
    size_t& used,const std::vector<fo3pipdata::Condition>* conditions=nullptr,
    const std::vector<fo3pipdata::Condition>* questConditions=nullptr) noexcept {
  if(used>ScriptEvidenceMaxBytes)return false;
  size_t remaining=ScriptEvidenceMaxBytes-used;
  auto bytes=[&](size_t n){if(n>remaining)return false;remaining-=n;return true;};
  auto array=[&](size_t n,size_t width){
    if(n>remaining/width)return false;
    remaining-=n*width;return true;
  };
  auto string=[&](const std::string& s){return bytes(s.size())&&bytes(1);};
  auto script=[&](const fo3pipdata::ResultScript& s){
    if(!string(s.source)||!bytes(s.compiled.size())||!bytes(s.header.size())||
        !array(s.references.size(),sizeof(uint32_t))||
        !array(s.records.size(),sizeof(s.records[0])))return false;
    for(const auto& record:s.records)
      if(!string(record.first)||!bytes(record.second.size()))return false;
    return true;
  };
  if(!bytes(sizeof(fo3player::ScriptExecution))||!string(event)||!script(program)||
      (container?!script(*container):!bytes(1))||
      (conditions&&!array(conditions->size(),sizeof(fo3pipdata::Condition)))||
      (questConditions&&!array(questConditions->size(),sizeof(fo3pipdata::Condition))))return false;
  used=ScriptEvidenceMaxBytes-remaining;return true;
}
// Accept the catalog map before capture and the canonical vector before matching.
// Fixed vector storage is already included in sizeof(ScriptExecution) above.
template<class Locals> inline bool ChargeScriptLocals(const Locals& locals,size_t& used) noexcept {
  if(used>ScriptEvidenceMaxBytes)return false;
  size_t remaining=ScriptEvidenceMaxBytes-used;
  for(const auto& local:locals){
    if(sizeof(std::pair<uint32_t,std::string>)>remaining)return false;
    remaining-=sizeof(std::pair<uint32_t,std::string>);
    if(local.second.size()>=remaining)return false; // include string terminator
    remaining-=local.second.size()+1;
  }
  used=ScriptEvidenceMaxBytes-remaining;return true;
}
inline bool ChargeScriptForms(const std::vector<std::pair<uint32_t,std::string>>& forms,size_t& used) noexcept {
  return ChargeScriptLocals(forms,used);
}
inline bool ChargeScriptVariables(const std::vector<fo3player::ScriptVariableBinding>& variables,size_t& used) noexcept {
  if(used>ScriptEvidenceMaxBytes)return false;
  size_t remaining=ScriptEvidenceMaxBytes-used;
  for(const auto& variable:variables){
    if(sizeof(variable)>remaining)return false;
    remaining-=sizeof(variable);
    for(const auto* name:{&variable.ownerName,&variable.variableName}){
      if(name->size()>=remaining)return false;
      remaining-=name->size()+1;
    }
  }
  used=ScriptEvidenceMaxBytes-remaining;return true;
}
inline bool ChargeScriptGlobals(size_t count,size_t& used) noexcept {
  constexpr size_t width=sizeof(std::pair<uint32_t,uint32_t>);
  if(used>ScriptEvidenceMaxBytes||count>(ScriptEvidenceMaxBytes-used)/width)return false;
  used+=count*width;return true;
}
}
