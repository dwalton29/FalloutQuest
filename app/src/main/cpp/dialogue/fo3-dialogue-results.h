#pragma once
#include "fo3-dialogue-conditions.h"
namespace fo3dialogue {
enum class CommandType { Variable, EnableControls, StartQuest, Stage, CompleteQuest, ObjectiveDisplay, ObjectiveComplete, AddItem, AddTopic };
struct Command {CommandType type;uint32_t form=0,index=0;uint64_t variable=0;float value=0;};
bool Activate(const Context&,fo3player::Player&,std::string&);
bool CompileResult(const fo3pipdata::Definitions&,const fo3pipdata::ResultScript&,std::vector<Command>&,std::string&);
}
