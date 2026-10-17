#pragma once
#include "fo3-dialogue-conditions.h"
namespace fo3dialogue {
enum class CommandType { Variable, Global, If, ElseIf, Else, EndIf, StopQuest, EnableControls, StartQuest, Stage, CompleteQuest, ObjectiveDisplay, ObjectiveComplete, AddItem, AddTopic, ShowMessage, PlaySound, RemoveItem, SetQuestObject, RewardKarma, EvaluatePackage, Lock, Unlock, ShowBarterMenu, ModDisposition, CompleteAllObjectives, RewardXP, SetCellFullName, AddAchievement, Disable };
struct Command {CommandType type;uint32_t form=0,index=0;uint64_t variable=0;float value=0;std::string expression;};
// Shared bounded form-name sink for command compilation and expression preflight.
struct CommandFormCapture {
  std::vector<std::pair<uint32_t,std::string>> bindings;
  size_t limit=0,used=0;
  bool exhausted=false;
  void Reset(){bindings.clear();used=0;exhausted=false;}
  bool Add(uint32_t id,const std::string& name){
    const auto fixed=sizeof(std::pair<uint32_t,std::string>);
    if(used>limit||fixed>limit-used||name.size()>=limit-used-fixed){exhausted=true;return false;}
    bindings.emplace_back(id,name);used+=fixed+name.size()+1;return true;
  }
};
bool ActivationVariables(const Context&,const fo3player::Player&,std::unordered_map<uint64_t,float>&,std::string&);
bool Activate(const Context&,fo3player::Player&,std::string&);
bool CompileResult(const fo3pipdata::Definitions&,const fo3pipdata::ResultScript&,std::vector<Command>&,std::string&,uint32_t owner=0,uint32_t scriptId=0,CommandFormCapture* forms=nullptr);
}
