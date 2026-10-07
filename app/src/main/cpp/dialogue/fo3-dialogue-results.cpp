#include "fo3-dialogue-results.h"
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <cmath>
namespace fo3dialogue {
// The player's explicit Talk action dispatches the NPC's OnActivate event.
// Interpret only a validated local-variable/player-activator subset. GameMode,
// combat and death events remain outside this dialogue bridge.
bool ActivationVariables(const Context& ctx,const fo3player::Player& player,std::unordered_map<uint64_t,float>& pending,std::string& error) {
  pending.clear();
  error.clear();const auto& d=player.Definitions().pipboy;
  auto actor=d.dialogueActors.find(ctx.speaker.base);if(actor==d.dialogueActors.end())return true;
  auto script=d.scripts.find(actor->second.script);if(script==d.scripts.end()||script->second.source.empty())return true;
  auto lower=[](std::string s){for(auto& c:s)c=char(std::tolower((unsigned char)c));return s;};
  auto variable=[&](const std::string& name){for(const auto& v:script->second.variables)if(lower(v.second)==name)return VariableKey(d,ctx.speaker.reference,v.first,false);return uint64_t(0);};
  auto number=[](const std::string& s,float& value){char* end=nullptr;value=std::strtof(s.c_str(),&end);return end&&end!=s.c_str()&&!*end&&std::isfinite(value);};
  std::istringstream lines(lower(script->second.source));std::string line;bool event=false,found=false;
  std::vector<bool> gates{true};
  auto fail=[&](){error="unsupported OnActivate: "+line;return false;};
  while(std::getline(lines,line)) {
    auto comment=line.find(';');if(comment!=std::string::npos)line.resize(comment);
    std::istringstream in(line);std::string op,a,b,c,extra;if(!(in>>op))continue;
    if(!event){if(op=="begin"&&in>>a&&a=="onactivate"){if(found||in>>extra)return fail();event=found=true;}continue;}
    if(op=="end"){if(gates.size()!=1||in>>extra)return fail();event=false;continue;}
    if(op=="if") {
      if(!(in>>a>>b>>c)||b!="=="||in>>extra)return fail();
      bool pass=false;
      if(a=="getactionref"&&c=="player")pass=ctx.target.reference==0x14;
      else {
        const auto key=variable(a);float comparison=0;if(!key||!number(c,comparison))return fail();
        const auto saved=player.Snapshot().pipboy.dialogueVariables.find(key);
        const float value=pending.count(key)?pending.at(key):saved==player.Snapshot().pipboy.dialogueVariables.end()?0:saved->second;
        pass=value==comparison;
      }
      gates.push_back(gates.back()&&pass);continue;
    }
    if(op=="endif"){if(gates.size()<2||in>>extra)return fail();gates.pop_back();continue;}
    if(op=="set") {
      float value=0;if(!(in>>a>>b>>c)||b!="to"||in>>extra||!number(c,value))return fail();
      const auto key=variable(a);if(!key)return fail();if(gates.back())pending[key]=value;continue;
    }
    if(op=="activate"){if(in>>extra)return fail();continue;} // Talk supplies the default action.
    return fail();
  }
  if(event||gates.size()!=1)return fail();
  return true;
}
bool Activate(const Context& ctx,fo3player::Player& player,std::string& error) {
  std::unordered_map<uint64_t,float> pending;
  if(!ActivationVariables(ctx,player,pending,error))return false;
  for(const auto& v:pending)if(!player.SetDialogueVariable(v.first,v.second))return false;
  return true;
}
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
