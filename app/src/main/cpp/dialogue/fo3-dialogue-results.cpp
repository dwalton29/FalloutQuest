#include "fo3-dialogue-results.h"
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <functional>
#include <algorithm>
#include <cstdio>
#include <limits>
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
bool CompileResult(const fo3pipdata::Definitions& d,const fo3pipdata::ResultScript& script,std::vector<Command>& out,std::string& error,uint32_t owner,uint32_t scriptId,CommandFormCapture* forms) {
  out.clear();error.clear();
  if(forms)forms->Reset();
  if(script.source.empty()&&!script.compiled.empty()){error="compiled-only result";return false;}
  auto lower=[](std::string s){for(auto& c:s)c=char(std::tolower((unsigned char)c));return s;};
  std::unordered_map<std::string,uint32_t> referenceLocals;
  std::unordered_set<std::string> referenceDeclarations;
  bool referencePreamble=true;
  if(scriptId){const auto sc=d.scripts.find(scriptId);if(sc!=d.scripts.end()){
    std::istringstream source(lower(sc->second.source));std::string decl;
    while(std::getline(source,decl)){std::istringstream row(decl);std::string op,name;row>>op>>name;
      if(op=="ref"||op=="reference")referenceDeclarations.insert(name);
      if(op=="begin")break;
    }
  }}
  auto form=[&](std::string n){
    if(lower(n)=="player")return fo3player::PlayerRef;
    auto local=referenceLocals.find(lower(n));if(local!=referenceLocals.end())return local->second;
    n=lower(n);auto f=d.formNames.find(n);if(f==d.formNames.end()||!f->second)return uint32_t(0);
    if(forms&&!forms->Add(f->second,n))return uint32_t(0);
    return f->second;
  };
  auto number=[](const std::string& s,float& n){char* end=nullptr;n=std::strtof(s.c_str(),&end);return end&&end!=s.c_str()&&!*end&&std::isfinite(n);};
  std::istringstream lines(lower(script.source));std::string line;
  while(std::getline(lines,line)) {
    auto comment=line.find(';');if(comment!=std::string::npos)line.resize(comment);
    std::istringstream in(line);std::string op,a,b,c,extra;Command cmd{};cmd.type=CommandType::Variable;
    if(!(in>>op))continue;
    if(op=="ref"||op=="reference"){
      if(!referencePreamble||!(in>>a)||(in>>extra)||!referenceDeclarations.insert(a).second){error="invalid reference local declaration";return false;}
      continue;
    }
    if(op=="set"){
      std::istringstream assignment(line);std::string verb,target,to,value;
      assignment>>verb>>target>>to>>value;
      if(referenceDeclarations.count(target)){
        if(to!="to"||value!="getlinkedref"||(assignment>>extra)){error="unsupported reference assignment";return false;}
        auto placed=d.targets.find(owner);if(placed==d.targets.end()||!placed->second.linkedReference){error="terminal linked reference unavailable";return false;}
        referenceLocals[target]=placed->second.linkedReference;continue;
      }
    }
    referencePreamble=false;
    if(op=="if"||op=="elseif") {
      cmd.type=op=="if"?CommandType::If:CommandType::ElseIf;
      std::getline(in,cmd.expression);if(cmd.expression.empty()){error="missing conditional expression";return false;}
    } else if(op=="else"||op=="endif")cmd.type=op=="else"?CommandType::Else:CommandType::EndIf;
    else if(op=="enableplayercontrols")cmd.type=CommandType::EnableControls; // VR keeps controls live throughout.
    else if(op=="set") {
      if(!(in>>a>>b)||b!="to"){error="invalid assignment: "+line;return false;}
      std::getline(in,cmd.expression);cmd.variable=VariableKey(d,a,forms);
      if(!cmd.variable)cmd.variable=CaptureNumericLocalKey(d,owner,scriptId,a,forms);
      if(!cmd.variable){cmd.form=form(a);cmd.type=CommandType::Global;if(!cmd.form){error="unresolved assignment: "+a;return false;}}
    } else if(op=="return")cmd.type=CommandType::Return;
    else if(op=="sayto"||op=="say"||(op.size()>4&&op.substr(op.size()-4)==".say")){
      cmd.type=CommandType::SayTo;cmd.form=op.find('.')==std::string::npos?owner:form(op.substr(0,op.find('.')));cmd.index=fo3player::PlayerRef;
      if(op=="sayto"){if(!(in>>a)||!(cmd.index=form(a))){error="invalid SayTo target";return false;}}
      if(!(in>>a)||!(cmd.variable=form(a))){error="invalid speech topic";return false;}
    } else if(op=="kill"||(op.size()>5&&op.substr(op.size()-5)==".kill")){
      cmd.type=CommandType::Kill;cmd.form=op=="kill"?owner:form(op.substr(0,op.size()-5));
      if(!cmd.form){error="invalid Kill receiver";return false;}
    } else if(op=="setav"||(op.size()>6&&op.substr(op.size()-6)==".setav")){
      cmd.type=CommandType::SetActorValue;cmd.form=op=="setav"?owner:form(op.substr(0,op.size()-6));
      if(!(in>>a>>b)||a.size()!=10||a.substr(0,8)!="variable"||a[8]<'0'||a[8]>'1'||a[9]<'0'||a[9]>'9'||!number(b,cmd.value)){error="unsupported actor value";return false;}
      cmd.index=uint32_t((a[8]-'0')*10+a[9]-'0');if(cmd.index<1||cmd.index>10||!cmd.form){error="invalid variable actor value";return false;}--cmd.index;
    } else if(op=="stopquest"||op=="startquest"||op=="setstage"||op=="completequest"||op=="completeallobjectives"||op=="setobjectivedisplayed"||op=="setobjectivecompleted"||op=="addtopic") {
      if(!(in>>a)||(cmd.form=form(a))==0){error="unresolved form: "+line;return false;}
      if(op=="startquest")cmd.type=CommandType::StartQuest;
      if(op=="stopquest")cmd.type=CommandType::StopQuest;
      if(op=="completequest")cmd.type=CommandType::CompleteQuest;
      if(op=="completeallobjectives")cmd.type=CommandType::CompleteAllObjectives;
      if(op=="addtopic")cmd.type=CommandType::AddTopic;
      if(op=="setstage"||op=="setobjectivedisplayed"||op=="setobjectivecompleted") {
        float n;if(!(in>>b)||!number(b,n)||n<0||n>65535||std::floor(n)!=n){error="invalid index: "+line;return false;}cmd.index=uint32_t(n);
        if(op=="setstage")cmd.type=CommandType::Stage;
        else {cmd.type=op=="setobjectivedisplayed"?CommandType::ObjectiveDisplay:CommandType::ObjectiveComplete;
          if(!(in>>c)||!number(c,cmd.value)||(cmd.value!=0&&cmd.value!=1)){error="invalid objective flag";return false;}}
      }
    } else if(op=="moddisposition"||(op.size()>15&&op.compare(op.size()-15,15,".moddisposition")==0)) {
      const auto dot=op.find('.');cmd.form=dot==std::string::npos?owner:form(op.substr(0,dot));
      cmd.type=CommandType::ModDisposition;
      if(!(in>>a)){error="missing ModDisposition target/value";return false;}
      if(a=="player"){if(!(in>>a)){error="missing ModDisposition value";return false;}}
      if(!cmd.form||!number(a,cmd.value)||std::floor(cmd.value)!=cmd.value||cmd.value<-2147483648.0||double(cmd.value)>2147483647.0){error="invalid ModDisposition";return false;}
    } else if(op=="showbartermenu"||op=="sbm"||
        (op.size()>15&&op.compare(op.size()-15,15,".showbartermenu")==0)||
        (op.size()>4&&op.compare(op.size()-4,4,".sbm")==0)) {
      const auto dot=op.find('.');cmd.form=dot==std::string::npos?owner:form(op.substr(0,dot));
      if(!cmd.form){error="ShowBarterMenu requires actor reference";return false;}
      cmd.type=CommandType::ShowBarterMenu;std::getline(in,cmd.expression);
      cmd.expression=fo3script::Trim(cmd.expression);
    } else if(op=="addnote"||op=="player.addnote"||op=="removenote"||op=="player.removenote") {
      if(!(in>>a)||!(cmd.form=form(a))||!d.notes.count(cmd.form)){error="invalid terminal note";return false;}
      cmd.type=op.find("remove")!=std::string::npos?CommandType::RemoveItem:CommandType::AddItem;cmd.value=1;
    } else if(op=="showmap") {
      if(!(in>>a)||!(cmd.form=form(a))||!d.markers.count(cmd.form)){error="invalid map marker";return false;}
      cmd.type=CommandType::ShowMap;cmd.value=0;
      if(in>>b){float travel;if(!number(b,travel)||(travel!=0&&travel!=1)){error="invalid map marker flag";return false;}cmd.value=travel;}
      else in.clear();
    } else if(op=="enablefasttravel") {
      if(!(in>>a)||!number(a,cmd.value)||(cmd.value!=0&&cmd.value!=1)){error="invalid EnableFastTravel flag";return false;}
      cmd.type=CommandType::FastTravel;
    } else if(op=="forceterminalback") {
      cmd.type=CommandType::TerminalBack;cmd.form=owner;
    } else if(op=="setopenstate"||(op.size()>13&&op.compare(op.size()-13,13,".setopenstate")==0)) {
      const auto dot=op.find('.');cmd.form=dot==std::string::npos?owner:form(op.substr(0,dot));
      if(!cmd.form||!(in>>a)||!number(a,cmd.value)||(cmd.value<0||cmd.value>255||std::floor(cmd.value)!=cmd.value)){error="invalid SetOpenState";return false;}
      cmd.type=CommandType::DoorState;
    } else if(op=="showmessage") {
      cmd.type=CommandType::ShowMessage;
      if(!(in>>a)||(cmd.form=form(a))==0){error="invalid showmessage form";return false;}
      std::getline(in,cmd.expression);
      // ESM SCTX uses CRLF: a bare ShowMessage leaves a trailing CR.
      // It is whitespace, not a formatting argument/expression.
      cmd.expression=fo3script::Trim(cmd.expression);
    } else if(op=="setquestobject") {
      cmd.type=CommandType::SetQuestObject;
      if(!(in>>a>>b)||(cmd.form=form(a))==0||!number(b,cmd.value)||(cmd.value!=0&&cmd.value!=1)){error="invalid SetQuestObject";return false;}
    } else if(op=="lock"||op=="unlock"|| (op.size()>5&&op.compare(op.size()-5,5,".lock")==0)|| (op.size()>7&&op.compare(op.size()-7,7,".unlock")==0)) {
      const auto dot=op.find('.');const auto verb=dot==std::string::npos?op:op.substr(dot+1);
      cmd.form=dot==std::string::npos?owner:form(op.substr(0,dot));
      cmd.type=verb=="lock"?CommandType::Lock:CommandType::Unlock;
      if(!cmd.form){error="missing lock reference";return false;}
      if(verb=="lock"){if(in>>a){if(!number(a,cmd.value)||cmd.value<0||cmd.value>255||std::floor(cmd.value)!=cmd.value){error="invalid lock level";return false;}}else {in.clear();cmd.value=0;}}
    } else if(op=="setcellfullname") {
      cmd.type=CommandType::SetCellFullName;
      if(!(in>>a>>b)||!(cmd.form=form(a))||!(cmd.index=form(b))){error="invalid SetCellFullName";return false;}
    } else if(op=="rewardxp"||op=="addachievement") {
      cmd.type=op=="rewardxp"?CommandType::RewardXP:CommandType::AddAchievement;
      if(!(in>>a)||!number(a,cmd.value)||cmd.value<0||double(cmd.value)>2147483647.0||std::floor(cmd.value)!=cmd.value){error="invalid XP/achievement integer";return false;}
    } else if(op=="disable"||op=="enable"||
        (op.size()>8&&op.compare(op.size()-8,8,".disable")==0)||
        (op.size()>7&&op.compare(op.size()-7,7,".enable")==0)) {
      const auto dot=op.find('.');
      const auto verb=dot==std::string::npos?op:op.substr(dot+1);
      cmd.form=dot==std::string::npos?owner:form(op.substr(0,dot));
      cmd.type=verb=="enable"?CommandType::Enable:CommandType::Disable;
      if(!cmd.form){error="missing Enable/Disable reference";return false;}
      if(in>>a){float fade=0;if(!number(a,fade)||(fade!=0&&fade!=1)){error="invalid Enable/Disable fade flag";return false;}}
      else in.clear();
      // Only the resident workflow may publish world changes.
    } else if(op=="rewardkarma") {
      cmd.type=CommandType::RewardKarma;
      if(!(in>>a)||!number(a,cmd.value)){error="invalid RewardKarma";return false;}
    } else if(op.size()>4&&op.compare(op.size()-4,4,".evp")==0) {
      cmd.type=CommandType::EvaluatePackage;
      cmd.form=form(op.substr(0,op.size()-4));
      if(!cmd.form){error="unresolved EVP reference";return false;}
    } else if(op=="playgroup"||(op.size()>10&&op.substr(op.size()-10)==".playgroup")){
      cmd.type=CommandType::PlayGroup;
      cmd.form=op=="playgroup"?owner:form(op.substr(0,op.size()-10));
      if(!cmd.form||!(in>>a)){error="invalid PlayGroup receiver/group";return false;}
      cmd.expression=a;cmd.index=0;
      if(in>>b){float flags=0;if(!number(b,flags)||flags<0||flags>5||std::floor(flags)!=flags){error="invalid PlayGroup flags";return false;}cmd.index=uint32_t(flags);}
      in.clear();
    } else if(op=="imod"||op=="applyimagespacemodifier"){
      cmd.type=CommandType::ImageModifier;
      if(!(in>>a)||!(cmd.form=form(a))){error="invalid image modifier";return false;}
    } else if(op=="activate") {
      cmd.type=CommandType::DefaultActivate;cmd.form=owner;
      if(!owner){error="Activate requires a reference owner";return false;}
      if(in>>a){if(a!="player"){error="unsupported Activate actor";return false;}cmd.index=fo3player::PlayerRef;
        if(in>>b){if(b!="0"){error="Activate event re-entry unsupported";return false;}}
      }
      in.clear();
    } else if(op=="player.scaonactor"||op=="forcesave") {
      // Recognize these commands without pretending their effects exist.
      // Unentered character-generation branches do not execute them; entered
      // branches still reject the entire transaction explicitly.
      cmd.type=CommandType::Unavailable;cmd.expression=op;
    } else if(op=="playsound") {
      cmd.type=CommandType::PlaySound;
      if(!(in>>a)||(cmd.form=form(a))==0){error="invalid playsound form";return false;}
      if(in>>b){if(b!="0"&&b!="1"){error="invalid PlaySound wait flag";return false;}cmd.index=uint32_t(b=="1");}
      in.clear();
    } else if(op=="player.removeitem") {
      cmd.type=CommandType::RemoveItem;
      if(!(in>>a>>b)||(cmd.form=form(a))==0||!number(b,cmd.value)||cmd.value<1||cmd.value>100000||std::floor(cmd.value)!=cmd.value){error="invalid removeitem";return false;}
    } else if(op=="player.additem") {
      cmd.type=CommandType::AddItem;if(!(in>>a>>b)||!(cmd.form=form(a))||!number(b,cmd.value)||cmd.value<1||cmd.value>100000||std::floor(cmd.value)!=cmd.value){error="invalid additem";return false;}
    } else {error="unsupported command: "+op;return false;}
    if(in>>extra){error="unsupported command arguments: "+line;return false;}
    if(forms&&forms->exhausted){error="script binding byte limit exceeded";return false;}
    out.push_back(cmd);
  }
  return true;
}
}

#include "scripting/fo3-script-runtime.inc"
