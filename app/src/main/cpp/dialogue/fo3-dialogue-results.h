#pragma once
#include "fo3-dialogue-conditions.h"
namespace fo3dialogue {
enum class CommandType { Variable, Global, If, ElseIf, Else, EndIf, StopQuest, EnableControls, StartQuest, Stage, CompleteQuest, ObjectiveDisplay, ObjectiveComplete, AddItem, AddTopic, ShowMessage, PlaySound, RemoveItem, SetQuestObject, RewardKarma, EvaluatePackage, Lock, Unlock, ShowBarterMenu, ModDisposition, CompleteAllObjectives, RewardXP, SetCellFullName, AddAchievement, Disable, Enable, ShowMap, DoorState, TerminalBack, PlayGroup, ImageModifier };
struct Command {CommandType type;uint32_t form=0,index=0;uint64_t variable=0;float value=0;std::string expression;};
// Shared bounded form-name sink for command compilation and expression preflight.
struct CommandFormCapture {
  std::vector<std::pair<uint32_t,std::string>> bindings;
  std::vector<fo3player::ScriptVariableBinding> variables;
  std::vector<std::pair<uint32_t,std::string>> locals;
  std::vector<std::pair<uint32_t,uint32_t>> globals;
  size_t limit=0,used=0;
  bool exhausted=false;
  void Reset(){bindings.clear();variables.clear();locals.clear();globals.clear();used=0;exhausted=false;}
  bool AddGlobal(uint32_t id,uint32_t bits){
    const auto fixed=sizeof(std::pair<uint32_t,uint32_t>);
    if(used>limit||fixed>limit-used){exhausted=true;return false;}
    globals.emplace_back(id,bits);used+=fixed;return true;
  }
  bool AddLocal(uint32_t index,const std::string& name){
    const auto fixed=sizeof(std::pair<uint32_t,std::string>);
    if(used>limit||fixed>limit-used||name.size()>=limit-used-fixed){exhausted=true;return false;}
    locals.emplace_back(index,name);used+=fixed+name.size()+1;return true;
  }
  bool Add(uint32_t id,const std::string& name){
    const auto fixed=sizeof(std::pair<uint32_t,std::string>);
    if(used>limit||fixed>limit-used||name.size()>=limit-used-fixed){exhausted=true;return false;}
    bindings.emplace_back(id,name);used+=fixed+name.size()+1;return true;
  }
  bool AddVariable(uint32_t owner,uint32_t script,uint32_t index,uint32_t base,bool quest,
      const std::string& key,size_t dot){
    const auto fixed=sizeof(fo3player::ScriptVariableBinding);
    // Removing one dot and retaining two string terminators costs key.size()+1.
    if(used>limit||fixed>limit-used||key.size()>=limit-used-fixed){exhausted=true;return false;}
    fo3player::ScriptVariableBinding binding;
    binding.owner=owner;binding.script=script;binding.index=index;binding.base=base;binding.quest=quest;
    binding.ownerName=key.substr(0,dot);binding.variableName=key.substr(dot+1);
    variables.push_back(std::move(binding));used+=fixed+key.size()+1;return true;
  }
  bool AddIndexedVariable(uint32_t owner,uint32_t script,uint32_t index,uint32_t base,bool quest,
      const std::string& name){
    const auto fixed=sizeof(fo3player::ScriptVariableBinding);
    if(used>limit||fixed>limit-used||limit-used-fixed<2||name.size()>limit-used-fixed-2){
      exhausted=true;return false;
    }
    fo3player::ScriptVariableBinding binding;
    binding.owner=owner;binding.script=script;binding.index=index;binding.base=base;binding.quest=quest;
    binding.variableName=fo3script::Lower(name);
    variables.push_back(std::move(binding));used+=fixed+name.size()+2;return true;
  }
};
// The underlying local resolver is also used without evidence by other event
// consumers. Keep the bounded optional sink here to avoid a player/event cycle.
inline uint64_t CaptureNumericLocalKey(const fo3pipdata::Definitions& d,uint32_t owner,
    uint32_t script,const std::string& token,CommandFormCapture* capture){
  const auto key=fo3script::NumericLocalKey(d,owner,script,token);
  if(key&&capture&&!capture->AddLocal(uint32_t(key),fo3script::Lower(token)))return 0;
  return key;
}
bool ActivationVariables(const Context&,const fo3player::Player&,std::unordered_map<uint64_t,float>&,std::string&);
bool Activate(const Context&,fo3player::Player&,std::string&);
bool CompileResult(const fo3pipdata::Definitions&,const fo3pipdata::ResultScript&,std::vector<Command>&,std::string&,uint32_t owner=0,uint32_t scriptId=0,CommandFormCapture* forms=nullptr);
}
