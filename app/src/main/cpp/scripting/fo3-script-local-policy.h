#pragma once
#include "fo3-event-scripts.h"
#include <algorithm>

namespace fo3script {
// Use after retained-evidence charging. These temporary normalized names are
// private preparation work; the frame-time report comparison never calls this.
template<class Locals> inline bool UnambiguousScriptLocals(const Locals& locals) {
  std::vector<std::string> names;names.reserve(locals.size());
  for(const auto& local:locals){
    if(local.second.empty())return false;
    names.push_back(Lower(local.second));
  }
  std::sort(names.begin(),names.end());
  return std::adjacent_find(names.begin(),names.end())==names.end();
}
// After charging and name-uniqueness validation, bind recognized declarations
// in the same pre-Begin/comment scope used by NumericLocalKey. Extra decoded
// slots are preserved; this does not enable reference-local execution.
template<class Locals> inline bool ConsistentScriptLocalDeclarations(
    const std::string& source,const Locals& locals) {
  if(source.empty())return true;
  std::vector<std::pair<std::string,unsigned>> names;names.reserve(locals.size());
  for(const auto& local:locals)names.emplace_back(Lower(local.second),0);
  std::sort(names.begin(),names.end(),[](const auto& a,const auto& b){return a.first<b.first;});
  std::istringstream lines(source);std::string line;
  while(std::getline(lines,line)){
    const auto comment=line.find(';');if(comment!=std::string::npos)line.resize(comment);
    std::istringstream words(line);std::string op,name;if(!(words>>op))continue;
    op=Lower(op);if(op=="begin")break;
    const unsigned kind=op=="ref"?2u:
      (op=="short"||op=="int"||op=="long"||op=="float")?1u:0u;
    if(!kind)continue;
    if(!(words>>name))return false;
    name=Lower(name);
    auto slot=std::lower_bound(names.begin(),names.end(),name,
      [](const auto& entry,const std::string& value){return entry.first<value;});
    if(slot==names.end()||slot->first!=name||(slot->second&&slot->second!=kind))return false;
    slot->second=kind;
  }
  return true;
}
}
