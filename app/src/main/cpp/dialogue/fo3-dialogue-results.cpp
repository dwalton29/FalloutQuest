#include "fo3-dialogue-results.h"
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <cmath>
namespace fo3dialogue {
bool CompileResult(const fo3pipdata::Definitions& d,const fo3pipdata::ResultScript& script,std::vector<Command>& out,std::string& error) {
  out.clear();error.clear();
  if(script.source.empty()&&!script.compiled.empty()){error="compiled-only result";return false;}
  auto lower=[](std::string s){for(auto& c:s)c=char(std::tolower((unsigned char)c));return s;};
  auto form=[&](std::string n){auto f=d.formNames.find(lower(n));return f==d.formNames.end()?0:f->second;};
  auto number=[](const std::string& s,float& n){char* end=nullptr;n=std::strtof(s.c_str(),&end);return end&&end!=s.c_str()&&!*end&&std::isfinite(n);};
  std::istringstream lines(lower(script.source));std::string line;
  while(std::getline(lines,line)) {
    auto comment=line.find(';');if(comment!=std::string::npos)line.resize(comment);
    std::istringstream in(line);std::string op,a,b,c,extra;Command cmd{CommandType::Variable};
    if(!(in>>op))continue;
    if(op=="enableplayercontrols")cmd.type=CommandType::EnableControls; // VR keeps controls live throughout.
    else if(op=="set") {
      if(!(in>>a>>b>>c)||b!="to"||!number(c,cmd.value)||(cmd.variable=VariableKey(d,a))==0){error="unsupported variable assignment: "+line;return false;}
    } else if(op=="startquest"||op=="setstage"||op=="completequest"||op=="setobjectivedisplayed"||op=="setobjectivecompleted"||op=="addtopic") {
      if(!(in>>a)||(cmd.form=form(a))==0){error="unresolved form: "+line;return false;}
      if(op=="startquest")cmd.type=CommandType::StartQuest;
      if(op=="completequest")cmd.type=CommandType::CompleteQuest;
      if(op=="addtopic")cmd.type=CommandType::AddTopic;
      if(op=="setstage"||op=="setobjectivedisplayed"||op=="setobjectivecompleted") {
        float n;if(!(in>>b)||!number(b,n)||n<0||n>65535||std::floor(n)!=n){error="invalid index: "+line;return false;}cmd.index=uint32_t(n);
        if(op=="setstage")cmd.type=CommandType::Stage;
        else {cmd.type=op=="setobjectivedisplayed"?CommandType::ObjectiveDisplay:CommandType::ObjectiveComplete;
          if(!(in>>c)||!number(c,cmd.value)||(cmd.value!=0&&cmd.value!=1)){error="invalid objective flag";return false;}}
      }
    } else if(op=="player.additem") {
      cmd.type=CommandType::AddItem;if(!(in>>a>>b)||!(cmd.form=form(a))||!number(b,cmd.value)||cmd.value<1||cmd.value>100000||std::floor(cmd.value)!=cmd.value){error="invalid additem";return false;}
    } else {error="unsupported command: "+op;return false;}
    if(in>>extra){error="unsupported command arguments: "+line;return false;}
    out.push_back(cmd);
  }
  return true;
}
}
namespace fo3player {
bool Player::ExecuteDialogueResult(const fo3pipdata::ResultScript& script,std::string& error) {
  std::vector<fo3dialogue::Command> commands;
  if(!fo3dialogue::CompileResult(catalog_.pipboy,script,commands,error))return false;
  const auto before=state_;const auto revision=revision_;
  for(auto& c:commands) {
    bool ok=true;using C=fo3dialogue::CommandType;
    switch(c.type) {
    case C::Variable:ok=SetDialogueVariable(c.variable,c.value);break;
    case C::EnableControls:break;
    case C::StartQuest:ok=StartQuest(c.form);break;
    case C::Stage:
      // Stage scripts cannot be skipped. Accept only unconditionally script-free stages.
      {auto q=catalog_.pipboy.quests.find(c.form);
      if(q==catalog_.pipboy.quests.end()||!q->second.stages.count(c.index)||q->second.stages.at(c.index).scripted||q->second.stages.at(c.index).conditional)ok=false;
      else ok=StartQuest(c.form)&&SetQuestStage(c.form,uint16_t(c.index));}break;
    case C::CompleteQuest:ok=StartQuest(c.form)&&FinishQuest(c.form,fo3pipdata::Completion::Complete);break;
    case C::ObjectiveDisplay:case C::ObjectiveComplete:
      {const auto q=state_.pipboy.quests.find(c.form);fo3pipdata::ObjectiveState existing;
      if(q!=state_.pipboy.quests.end()){auto o=q->second.objectives.find(c.index);if(o!=q->second.objectives.end())existing=o->second;}
      ok=StartQuest(c.form)&&SetObjective(c.form,c.index,c.type==C::ObjectiveDisplay?bool(c.value):existing.displayed,
        c.type==C::ObjectiveComplete?(c.value?fo3pipdata::Completion::Complete:fo3pipdata::Completion::Active):existing.status);}break;
    case C::AddItem:ok=Add(c.form,int32_t(c.value));break;
    case C::AddTopic:if(!catalog_.pipboy.dialogueTopics.count(c.form))ok=false;else if(state_.pipboy.knownTopics.insert(c.form).second)++revision_;break;
    }
    if(!ok){state_=before;revision_=revision;error="result rejected by canonical Player API";return false;}
  }
  return true;
}
}
