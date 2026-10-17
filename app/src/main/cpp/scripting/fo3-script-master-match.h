#pragma once
#include "player/fo3-player-state.h"
#include <atomic>

namespace fo3script {
// Exact source evidence, not dependency/condition ownership or dispatch authority.
// SCPT events also require the original owner's direct NAME/base SCRI (or
// QUST SCRI) association. Activators and INFO speaker eligibility are not proven.
// Output is published only after the complete master scan succeeds.
class ScriptMasterMatch {
public:
  bool Verified() const noexcept {return verified_&&!programs_.empty();}
  bool Matches(const std::vector<fo3player::ScriptExecution>& programs) const noexcept;
private:
  bool verified_=false;
  std::vector<fo3player::ScriptExecution> programs_;
  friend bool MatchScriptProgramsToMaster(const std::string&,
      const std::vector<fo3player::ScriptExecution>&,ScriptMasterMatch&,
      const std::atomic<bool>&,std::string&);
};
bool MatchScriptProgramsToMaster(const std::string& master,
    const std::vector<fo3player::ScriptExecution>& programs,ScriptMasterMatch& out,
    const std::atomic<bool>& cancelled,std::string& error);
}
