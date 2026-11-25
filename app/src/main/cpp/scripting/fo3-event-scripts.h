#pragma once
// Source-script event block index. No Bethesda bytecode is interpreted here.
// A block is retained verbatim and executed by the same guarded interpreter as
// dialogue and QUST result scripts. Unknown events/filters remain explicit.
#include "pipboy/fo3-pipboy-data.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace fo3script {
struct EventBlock {
  std::string event;
  std::string filter;
  std::string body;
  unsigned sourceLine = 0;
};
struct EventProgram {
  std::vector<EventBlock> blocks;
  std::string error;
  bool compiledOnly = false;
};
inline std::string Lower(std::string s) {
  for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
inline std::string Trim(const std::string& s) {
  const auto a=s.find_first_not_of(" \t\r\n");
  if(a==std::string::npos)return {};
  const auto b=s.find_last_not_of(" \t\r\n");
  return s.substr(a,b-a+1);
}
inline bool EventFilterMatches(const std::string& filter,uint32_t activator,uint32_t playerReference){
  return filter.empty()||(filter=="player"&&activator==playerReference);
}
inline EventProgram ParseEvents(const fo3pipdata::Script& script) {
  EventProgram out;
  if(script.source.empty()){
    out.compiledOnly=!script.compiled.empty();
    if(out.compiledOnly)out.error="compiled-only SCPT event source";
    return out;
  }
  std::istringstream input(script.source);
  std::string line;bool inside=false;unsigned number=0;
  while(std::getline(input,line)){
    ++number;
    const auto comment=line.find(';');
    const auto clean=Trim(line.substr(0,comment));
    if(clean.empty())continue;
    std::istringstream words(Lower(clean));std::string op;words>>op;
    if(op=="begin"){
      std::string name;words>>name;
      if(inside||name.empty()){out.error="malformed/nested Begin at line "+std::to_string(number);return out;}
      std::string filter;std::getline(words,filter);
      out.blocks.push_back({name,Trim(filter),{},number});
      inside=true;
    }else if(op=="end"){
      std::string trailing;if(!inside||(words>>trailing)){
        out.error="unexpected/malformed End at line "+std::to_string(number);return out;
      }
      inside=false;
    }else if(inside){
      out.blocks.back().body+=line+"\n";
    }else if(op!="scn"&&op!="scriptname"&&op!="short"&&op!="int"&&
             op!="long"&&op!="float"&&op!="ref"){
      // Unknown top-level commands cannot safely be treated as event bodies.
      out.error="unrecognized script preamble at line "+std::to_string(number)+": "+op;
      return out;
    }
  }
  if(inside)out.error="unterminated Begin at line "+std::to_string(out.blocks.back().sourceLine);
  return out;
}

// Reference locals retain the full FormID. Only a matching SCPT declaration
// and variable-table entry may address a slot on the current owner.
inline uint64_t DeclaredLocalKey(const fo3pipdata::Definitions& d,uint32_t owner,
                                 uint32_t scriptId,const std::string& token,bool reference){
  if(!owner||!scriptId||token.find('.')!=std::string::npos)return 0;
  const auto def=d.scripts.find(scriptId);
  if(def==d.scripts.end()||def->second.source.empty())return 0;
  const auto name=Lower(token);
  const auto slot=std::find_if(def->second.variables.begin(),def->second.variables.end(),
      [&](const auto& entry){return Lower(entry.second)==name;});
  if(slot==def->second.variables.end())return 0;
  std::istringstream source(def->second.source);std::string line;
  while(std::getline(source,line)){
    std::istringstream words(Lower(line.substr(0,line.find(';'))));
    std::string op,var;if(!(words>>op))continue;
    if(op=="begin")break;
    const bool type=reference?(op=="ref"||op=="reference"):(op=="short"||op=="int"||op=="long"||op=="float");
    if(type&&words>>var&&var==name)return (uint64_t(owner)<<32)|slot->first;
  }
  return 0;
}
inline uint64_t NumericLocalKey(const fo3pipdata::Definitions& d,uint32_t owner,
                                uint32_t scriptId,const std::string& token){
  return DeclaredLocalKey(d,owner,scriptId,token,false);
}
} // namespace fo3script
