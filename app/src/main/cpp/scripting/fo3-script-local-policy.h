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
}
