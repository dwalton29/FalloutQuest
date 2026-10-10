#pragma once
#include "fo3-dialogue-conditions.h"
namespace fo3dialogue {
enum class CommandType { Variable, Global, If, ElseIf, Else, EndIf, StopQuest, EnableControls, StartQuest, Stage, CompleteQuest, ObjectiveDisplay, ObjectiveComplete, AddItem, AddTopic };
struct Command {CommandType type;uint32_t form=0,index=0;uint64_t variable=0;float value=0;std::string expression;};
bool ActivationVariables(const Context&,const fo3player::Player&,std::unordered_map<uint64_t,float>&,std::string&);
bool Activate(const Context&,fo3player::Player&,std::string&);
bool CompileResult(const fo3pipdata::Definitions&,const fo3pipdata::ResultScript&,std::vector<Command>&,std::string&);
}
