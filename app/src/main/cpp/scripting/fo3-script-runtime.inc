#include "fo3-script-evidence-budget.h"
#include "fo3-script-local-policy.h"
#include "player/fo3-actor-values.h"
#include <cstring>
// Shared source-script execution for dialogue results and QUST stage items.
// Compiled-only bytecode and event blocks are retained, never guessed or skipped.
namespace fo3script {
class Expression {
  const fo3player::Player& p; uint32_t owner,scriptId;
  std::vector<std::string> tokens; size_t at=0; std::string& error;
  fo3dialogue::CommandFormCapture* forms;
  bool syntaxOnly;
  bool Fail(const std::string& text){if(error.empty())error=text;return false;}
  bool Atom(float& value) {
    if(at==tokens.size())return Fail("missing expression operand");
    auto t=tokens[at++];
    if(t=="("){if(!Or(value)||at==tokens.size()||tokens[at++]!=")")return Fail("unbalanced expression");return true;}
    if(t=="-"||t=="+"){if(!Atom(value))return false;if(t=="-")value=-value;return true;}
    char* end=nullptr;value=std::strtof(t.c_str(),&end);
    if(end!=t.c_str()&&!*end)return std::isfinite(value)||Fail("nonfinite literal");
    if(syntaxOnly){
      const auto dot=t.find('.');const auto query=dot==std::string::npos?t:t.substr(dot+1);
      size_t arguments=0;
      if(query=="getstage"||query=="getquestrunning"||query=="isactionref"||query=="getav"||
         query=="getitemcount"||query=="getinsamecell"||query=="getincell"||query=="getdistance")arguments=1;
      if(query=="getstagedone"||query=="getobjectivedisplayed"||query=="getobjectivecompleted")arguments=2;
      for(size_t n=0;n<arguments;++n){
        if(at==tokens.size()||tokens[at]=="("||tokens[at]==")"||tokens[at]=="&&"||tokens[at]=="||"||tokens[at]=="==")return Fail("missing expression query argument");
        ++at;
      }
      value=0;return true;
    }
    const auto& d=p.Definitions().pipboy;
    auto form=[&](const std::string& n){
      if(n=="player")return fo3player::PlayerRef;
      const auto f=d.formNames.find(n);if(f==d.formNames.end()||!f->second)return uint32_t(0);
      if(forms&&!forms->Add(f->second,n))return uint32_t(0);
      return f->second;
    };
    if(t=="getstage"||t=="getstagedone"||t=="getquestrunning"||t=="getobjectivedisplayed"||t=="getobjectivecompleted") {
      if(at==tokens.size())return Fail("missing query quest");
      const auto id=form(tokens[at++]);auto def=d.quests.find(id);if(def==d.quests.end())return Fail("invalid query quest");
      const auto& qs=p.Snapshot().pipboy.quests;auto q=qs.find(id);
      if(t=="getstage"){value=q==qs.end()?0:q->second.stage;return true;}
      if(t=="getquestrunning"){value=q==qs.end()?bool(def->second.flags&1):q->second.running;return true;}
      float n=0;if(at==tokens.size())return Fail("missing query index");
      auto index=tokens[at++];n=std::strtof(index.c_str(),&end);
      if(end==index.c_str()||*end||!std::isfinite(n)||n<0||n>65535||std::floor(n)!=n)return Fail("invalid query index");
      if(t=="getstagedone"){if(!def->second.stages.count(uint16_t(n)))return Fail("invalid stage query");value=q!=qs.end()&&q->second.stages.count(uint16_t(n));return true;}
      if(!def->second.objectives.count(uint32_t(n)))return Fail("invalid objective query");
      value=0;if(q!=qs.end()){auto o=q->second.objectives.find(uint32_t(n));if(o!=q->second.objectives.end())value=t=="getobjectivedisplayed"?o->second.displayed:o->second.status==fo3pipdata::Completion::Complete;}return true;
    }
    if(t=="isactionref"){
      if(at==tokens.size()||tokens[at++]!="player")return Fail("unsupported action reference");
      value=p.ScriptActivator()==fo3player::PlayerRef;return true;
    }
    if(t=="player.getitemcount"){
      if(at==tokens.size())return Fail("missing item form");
      const auto id=form(tokens[at++]);if(!id)return Fail("unresolved item form");
      value=p.ItemCount(id);return true;
    }
    const auto dot=t.find('.');const auto query=dot==std::string::npos?t:t.substr(dot+1);
    if(query=="getinsamecell") {
      if(at==tokens.size()||tokens[at++]!="player")return Fail("unsupported GetInSameCell target");
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));bool same=false;
      if(!p.ScriptInSameCell(id,same,error))return false;
      value=same;return true;
    }
    if(query=="gettalkedtopc"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));value=p.Snapshot().pipboy.talkedActors.count(id);return true;
    }
    if(query=="getincell"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(at==tokens.size())return Fail("missing cell query");
      const auto cell=form(tokens[at++]);fo3pipdata::Location location;
      if(!d.cellWorlds.count(cell)||!p.ScriptReferenceLocation(id,location))return Fail("cell query location unavailable");
      value=location.cell==cell;return true;
    }
    if(query=="getopenstate"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      const auto placed=d.targets.find(id);if(placed==d.targets.end())return Fail("open-state reference unavailable");
      const auto saved=p.Snapshot().doorStates.find(id);value=saved==p.Snapshot().doorStates.end()?placed->second.openByDefault:saved->second;return true;
    }
    if(query=="getdistance"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(at==tokens.size())return Fail("missing distance target");
      const auto target=form(tokens[at++]);
      fo3pipdata::Location a,b;if(!p.ScriptReferenceLocation(id,a)||!p.ScriptReferenceLocation(target,b))return Fail("distance location unavailable");
      if(a.cell!=b.cell&&(!a.world||a.world!=b.world)){value=1e9f;return true;}
      const float dx=a.x-b.x,dy=a.y-b.y;
      // Location carries planar map coordinates; fetch canonical actor height too.
      const auto height=[&](uint32_t ref){if(ref==fo3player::PlayerRef)return p.ScriptPlayerHeight();const auto actor=p.Snapshot().actors.find(ref);if(actor!=p.Snapshot().actors.end())return actor->second.position[2];const auto placed=d.targets.find(ref);return placed==d.targets.end()?0.f:placed->second.z;};
      const float dz=height(id)-height(target);value=std::sqrt(dx*dx+dy*dy+dz*dz);return true;
    }
    if(query=="getdead"||query=="isincombat"||query=="istalking"||query=="issneaking"||query=="getsitting"||query=="getfurnituremarkerid"||query=="getcurrentfurnitureref"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(query=="getsitting"||query=="getfurnituremarkerid"||query=="getcurrentfurnitureref"){if(id!=fo3player::PlayerRef)return Fail("NPC furniture query unavailable");value=query=="getsitting"?p.SittingState():query=="getfurnituremarkerid"?p.FurnitureMarker():p.CurrentFurniture();return true;}
      if(query=="issneaking"){if(id!=fo3player::PlayerRef)return Fail("NPC sneak posture unavailable");value=p.IsSneaking();return true;}
      if(query=="istalking"){value=p.ScriptActorTalking(id);return true;}
      if(query=="getdead"){const auto health=p.ActorHealth(id);if(health<0)return Fail("actor health unavailable");value=health<=0;return true;}
      const auto actor=p.Snapshot().actors.find(id);if(actor==p.Snapshot().actors.end())return Fail("combat actor unavailable");
      value=actor->second.hostile!=0;return true;
    }
    if(query=="getav"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(at==tokens.size())return Fail("missing actor value");
      const auto name=tokens[at++];
      uint32_t valueId=fo3av::ID(name);
      if(name=="aggression")valueId=0;
      if(name=="assistance")valueId=57;
      if(name.size()==10&&name.substr(0,8)=="variable"&&name[8]>='0'&&name[8]<='1'&&name[9]>='0'&&name[9]<='9'){
        const unsigned slot=(name[8]-'0')*10+name[9]-'0';if(slot>=1&&slot<=10)valueId=61+slot;
      }
      return p.ActorValue(id,valueId,value)||Fail("actor value unavailable "+name);
    }
    if(query=="gethealthpercentage"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));value=p.HealthPercentage(id);return value>=0||Fail("actor health unavailable");
    }
    if(query=="getdisabled"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));bool disabled=false;
      if(!p.ReferenceDisabled(id,disabled,error))return false;
      value=disabled;return true;
    }
    if(query=="getlocked"||query=="getlocklevel"||query=="getislockbroken") {
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));auto lock=p.GetLock(id);
      if(!lock.known||lock.leveled)return Fail("unresolved lock query");
      value=query=="getlocked"?float(lock.locked):query=="getlocklevel"?float(lock.level):float(lock.broken);return true;
    }
    if(t=="getcurrenttime"){value=p.Snapshot().gameHour;return true;}
    if(t=="getsecondspassed"){value=float(p.ScriptSecondsPassed());return true;}
    if(query=="isanimplaying"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));bool playing=false;
      if(!p.ScriptAnimationPlaying(id,playing))return Fail("animation reference unavailable");
      value=playing;return true;
    }
    if(t=="getbuttonpressed"){value=p.ScriptButton();return true;}
    if(t=="getdistance"){
      if(at==tokens.size()||tokens[at++]!="player")return Fail("unsupported distance target");
      return p.ScriptDistance(value)||Fail("player/reference distance unavailable");
    }
    uint64_t key=fo3dialogue::VariableKey(d,t,forms);
    if(!key&&owner&&t.find('.')==std::string::npos){auto q=d.quests.find(owner);if(q!=d.quests.end())key=fo3dialogue::VariableKey(d,q->second.editor+"."+t,forms);}
    if(!key)key=fo3dialogue::CaptureNumericLocalKey(d,owner,scriptId,t,forms);
    if(key){auto v=p.Snapshot().pipboy.dialogueVariables.find(key);value=v==p.Snapshot().pipboy.dialogueVariables.end()?0:v->second;return true;}
    const auto id=form(t);if(id&&p.GlobalValue(id,value)){
      if(forms){
        const auto global=p.Definitions().globals.find(id);
        if(global==p.Definitions().globals.end()||!std::isfinite(global->second))return Fail("script provenance global default unavailable");
        uint32_t bits;std::memcpy(&bits,&global->second,4);
        if(!forms->AddGlobal(id,bits))return Fail("script provenance byte limit exceeded");
      }
      return true;
    }
    return Fail("unsupported expression operand: "+t);
  }
  bool Sum(float& value){if(!Atom(value))return false;while(at<tokens.size()&&(tokens[at]=="+"||tokens[at]=="-")){auto op=tokens[at++];float b;if(!Atom(b))return false;value=op=="+"?value+b:value-b;}return std::isfinite(value)||Fail("expression overflow");}
  bool Compare(float& value){if(!Sum(value))return false;if(at<tokens.size()){auto op=tokens[at];if(op=="=="||op=="!="||op=="<"||op==">"||op=="<="||op==">="){++at;float b;if(!Sum(b))return false;value=op=="=="?value==b:op=="!="?value!=b:op=="<"?value<b:op==">"?value>b:op=="<="?value<=b:value>=b;}}return true;}
  bool And(float& value){if(!Compare(value))return false;while(at<tokens.size()&&tokens[at]=="&&"){++at;float b;if(!Compare(b))return false;value=value!=0&&b!=0;}return true;}
  bool Or(float& value){if(!And(value))return false;while(at<tokens.size()&&tokens[at]=="||"){++at;float b;if(!And(b))return false;value=value!=0||b!=0;}return true;}
public:
  Expression(const fo3player::Player& player,uint32_t o,const std::string& text,std::string& e,uint32_t sid=0,fo3dialogue::CommandFormCapture* capture=nullptr,bool syntax=false):p(player),owner(o),scriptId(sid),error(e),forms(capture),syntaxOnly(syntax) {
    for(size_t i=0;i<text.size();) {
      if(std::isspace((unsigned char)text[i])){++i;continue;}
      const size_t start=i;const auto c=text[i];
      if(std::isalnum((unsigned char)c)||c=='_'||c=='.'){while(i<text.size()&&(std::isalnum((unsigned char)text[i])||text[i]=='_'||text[i]=='.'))++i;}
      else {++i;if(i<text.size()&&((text[i]=='='&&(c=='='||c=='!'||c=='<'||c=='>'))||(text[i]==c&&(c=='&'||c=='|'))))++i;}
      auto token=text.substr(start,i-start);for(auto& ch:token)ch=char(std::tolower((unsigned char)ch));tokens.push_back(token);
    }
  }
  bool Evaluate(float& value){return Or(value)&&(at==tokens.size()||Fail("unsupported expression syntax"));}
};
}
#include "fo3-script-transaction.inc"
namespace fo3player {
bool Player::PreviewDialogueResult(const fo3pipdata::ResultScript& script,std::string& error,uint32_t speaker) const {
  return PreviewDialogueResults(script,{},error,speaker);
}
bool Player::PreviewDialogueResults(const fo3pipdata::ResultScript& begin,const fo3pipdata::ResultScript& end,std::string& error,uint32_t speaker) const {
  auto plan=PrepareDialogueTransaction(begin,end,error,speaker);
  if(plan&&plan->HasReferenceChanges()){error="world mutation requires resident coordinator";return false;}
  return bool(plan);
}
bool Player::ExecuteDialogueResult(const fo3pipdata::ResultScript& script,std::string& error,uint32_t speaker) {
  auto plan=PrepareDialogueTransaction(script,{},error,speaker);
  if(!plan)return false;
  if(plan->HasReferenceChanges()){error="world mutation requires resident coordinator";return false;}
  if(CommitScriptTransaction(*plan))return true;
  error="script transaction changed before publication";return false;
}
bool Player::ExecuteQuestStage(uint32_t id,uint16_t stage,std::string& error) {
  auto plan=PrepareQuestStageTransaction(id,stage,error);
  if(!plan)return false;
  if(plan->HasReferenceChanges()){error="world mutation requires resident coordinator";return false;}
  if(CommitScriptTransaction(*plan))return true;
  error="quest transaction changed before publication";return false;
}
bool Player::RunQuestStage(uint32_t id,uint16_t index,unsigned depth,unsigned& budget,std::string& error) {
  auto fail=[&](const std::string& s){error="QUST "+std::to_string(id)+" stage "+std::to_string(index)+": "+s;return false;};
  if(depth>=32||!budget)return fail("stage recursion/budget exceeded");
  --budget;auto q=catalog_->pipboy.quests.find(id);
  if(q==catalog_->pipboy.quests.end()||!q->second.stages.count(index))return fail("unresolved stage");
  if(!q->second.diagnostics.empty())return fail(q->second.diagnostics.front());
  auto previous=state_.pipboy.quests.find(id);
  if(previous!=state_.pipboy.quests.end()&&previous->second.stages.count(index)&&!(q->second.flags&8))return true;
  const auto& stage=q->second.stages.at(index);
  if(stage.items.empty()&&(stage.scripted||stage.conditional))return fail("stage item data unavailable");
  if(!StartQuest(id))return fail("cannot start quest");
  // Mark visited before result scripts; nested GetStage/GetStageDone see this change.
  auto& s=state_.pipboy.quests.at(id);s.stage=std::max(s.stage,index);s.stages.insert(index);++revision_;
  fo3dialogue::Context ctx;ctx.player=this;ctx.target.reference=0x14;ctx.target.base=7;
  for(size_t i=0;i<stage.items.size();++i) {
    const auto& item=stage.items[i];std::string conditionError;
    if(!fo3dialogue::Conditions(item.conditions,ctx,conditionError)) {
      if(!conditionError.empty())return fail("item "+std::to_string(i)+": "+conditionError);
      continue;
    }
    if(item.flags&~3u)return fail("unsupported stage item flags");
    ScriptProgramOrigin origin;origin.kind=ScriptProgramKind::QuestStageItem;
    origin.record=id;origin.stage=index;origin.item=uint32_t(i);
    if(!RunScript(item.result,id,depth+1,budget,error,0,std::move(origin)))return fail("item "+std::to_string(i)+": "+error);
    // Reacquire after scripts: nested stages can rehash the quest instance map.
    if(!item.log.empty())state_.pipboy.quests.at(id).journal.push_back({index,uint32_t(i)});
    if(item.flags&3)if(!FinishQuest(id,(item.flags&2)?fo3pipdata::Completion::Failed:fo3pipdata::Completion::Complete))return fail("completion rejected");
  }
  return true;
}
bool Player::RunScript(const fo3pipdata::ResultScript& script,uint32_t owner,unsigned depth,unsigned& budget,std::string& error,uint32_t scriptId,ScriptProgramOrigin origin) {
  if(depth>=32){error="script nesting exceeded";return false;}
  if(!scriptId && owner){
    const auto quest=catalog_->pipboy.quests.find(owner);
    if(quest!=catalog_->pipboy.quests.end())scriptId=quest->second.script;
  }
  fo3dialogue::CommandFormCapture forms;
  if(scriptTrace_){
    if(scriptTraceBytes_>fo3script::ScriptEvidenceMaxBytes){error="script provenance byte limit exceeded";return false;}
    forms.limit=fo3script::ScriptEvidenceMaxBytes-scriptTraceBytes_;
  }
  if(!scriptCompileCache_)scriptCompileCache_=std::make_shared<fo3script::CompileCache>(catalog_->pipboy);
  const auto compiled=scriptCompileCache_->Get(script,owner,scriptId,bool(scriptTrace_),forms.limit);
  forms=compiled->forms;
  error=compiled->error;
  if(!compiled->valid){
    if(forms.exhausted)error="script provenance byte limit exceeded";
    return false;
  }
  const auto& commands=compiled->commands;
  using C=fo3dialogue::CommandType;
  if(scriptTrace_)for(const auto& command:commands)if(command.type==C::Global){
    const auto global=catalog_->globals.find(command.form);
    if(!command.form||global==catalog_->globals.end()||!std::isfinite(global->second)){
      error="script provenance global default unavailable";return false;
    }
    uint32_t bits;std::memcpy(&bits,&global->second,4);
    if(!forms.AddGlobal(command.form,bits)){error="script provenance byte limit exceeded";return false;}
  }
  fo3dialogue::CommandFormCapture expressionForms;
  if(scriptTrace_)expressionForms.limit=forms.limit-forms.used;
  // Validate expression syntax and block structure, including unreachable branches.
  struct Gate {bool parent,taken,active,hasElse;};std::vector<Gate> gates;
  for(const auto& c:commands) {
    if(c.type==C::If)gates.push_back({true,false,false,false});
    if(c.type==C::ElseIf||c.type==C::Else){if(gates.empty()||gates.back().hasElse){error="invalid else/elseif";return false;}if(c.type==C::Else)gates.back().hasElse=true;}
    if(c.type==C::EndIf){if(gates.empty()){error="orphan endif";return false;}gates.pop_back();}
    if(c.type==C::If||c.type==C::ElseIf||c.type==C::Variable||c.type==C::Global||((c.type==C::ShowMessage||c.type==C::ShowBarterMenu)&&!c.expression.empty())){
      float v;
      if(!fo3script::Expression(*this,owner,c.expression,error,scriptId,nullptr,true).Evaluate(v))return false;
      // Capture resolvable original bindings without demanding capabilities
      // from a branch that may never run. Entered operands are resolved below,
      // against the state after preceding commands, inside the transaction.
      if(scriptTrace_){
        std::string capabilityError;
        (void)fo3script::Expression(*this,owner,c.expression,capabilityError,scriptId,&expressionForms).Evaluate(v);
        if(capabilityError.rfind("script provenance",0)==0){error=capabilityError;return false;}
        if(expressionForms.exhausted){error="script provenance byte limit exceeded";return false;}
      }
    }
  }
  if(!gates.empty()){error="unterminated if";return false;}
  struct RestoreTraceParent {Player& p;uint32_t previous;
    ~RestoreTraceParent(){p.scriptTraceParent_=previous;}} traceParent{*this,scriptTraceParent_};
  if(scriptTrace_){
    if(scriptTrace_->size()>=fo3script::ScriptEvidenceMaxEntries){error="script provenance entry limit exceeded";return false;}
    const std::vector<fo3pipdata::Condition>* conditions=nullptr;
    const std::vector<fo3pipdata::Condition>* questConditions=nullptr;
    uint32_t owningQuest=0;
    if(origin.kind==ScriptProgramKind::QuestStageItem){
      const auto q=catalog_->pipboy.quests.find(origin.record);
      if(q==catalog_->pipboy.quests.end()){error="script provenance quest unavailable";return false;}
      const auto stage=q->second.stages.find(origin.stage);
      if(stage==q->second.stages.end()||origin.item>=stage->second.items.size()){
        error="script provenance stage item unavailable";return false;
      }
      conditions=&stage->second.items[origin.item].conditions;
    }else if(origin.kind==ScriptProgramKind::TerminalEntry){
      const auto terminal=catalog_->pipboy.terminals.find(origin.record);
      if(terminal==catalog_->pipboy.terminals.end()||origin.item>=terminal->second.entries.size()){error="terminal provenance unavailable";return false;}
      conditions=&terminal->second.entries[origin.item].conditions;
    }else if(origin.kind==ScriptProgramKind::DialogueInfo){
      const auto location=catalog_->dialogueInfos.find(origin.record);
      const auto topic=catalog_->pipboy.topics.find(origin.topic);
      if(location==catalog_->dialogueInfos.end()||location->second.first!=origin.topic||
          topic==catalog_->pipboy.topics.end()||location->second.second>=topic->second.size()){
        error="script provenance INFO unavailable";return false;
      }
      const auto& info=topic->second[location->second.second];
      conditions=&info.conditions;owningQuest=info.quest;
      const auto quest=catalog_->pipboy.quests.find(owningQuest);
      if(quest!=catalog_->pipboy.quests.end())questConditions=&quest->second.conditions;
    }
    const fo3pipdata::ResultScript* container=nullptr;
    if(origin.kind==ScriptProgramKind::ReferenceEvent||
        (origin.kind==ScriptProgramKind::QuestStageItem&&scriptId)){
      const auto found=catalog_->pipboy.scripts.find(scriptId);
      if(found!=catalog_->pipboy.scripts.end())container=&found->second;
    }
    auto nextBytes=scriptTraceBytes_;
    if(!fo3script::ChargeScriptEvidence(origin.event,script,container,nextBytes,conditions,questConditions)){
      error="script provenance byte limit exceeded";return false;
    }
    if(!fo3script::ChargeScriptForms(forms.bindings,nextBytes)||
        !fo3script::ChargeScriptForms(expressionForms.bindings,nextBytes)||
        !fo3script::ChargeScriptVariables(forms.variables,nextBytes)||
        !fo3script::ChargeScriptVariables(expressionForms.variables,nextBytes)||
        !fo3script::ChargeScriptLocals(forms.locals,nextBytes)||
        !fo3script::ChargeScriptLocals(expressionForms.locals,nextBytes)||
        !fo3script::ChargeScriptGlobals(forms.globals.size(),nextBytes)||
        !fo3script::ChargeScriptGlobals(expressionForms.globals.size(),nextBytes)){
      error="script provenance byte limit exceeded";return false;
    }
    const fo3pipdata::Script* localScript=nullptr;
    if(scriptId){
      const auto found=catalog_->pipboy.scripts.find(scriptId);
      if(found==catalog_->pipboy.scripts.end()){error="script provenance local declarations unavailable";return false;}
      localScript=&found->second;
      if(!fo3script::ChargeScriptLocals(localScript->variables,nextBytes)){
        error="script provenance byte limit exceeded";return false;
      }
      if(container&&!fo3script::CompleteScriptLocalRecords(*container)){
        error="script provenance malformed local records";return false;
      }
      if(!fo3script::UnambiguousScriptLocals(localScript->variables)){
        error="script provenance local names are empty or ambiguous";return false;
      }
      if(container&&!fo3script::ConsistentScriptLocalDeclarations(container->source,localScript->variables)){
        error="script provenance local declarations do not match source";return false;
      }
    }
    fo3dialogue::CommandFormCapture conditionForms,questConditionForms;
    auto captureConditions=[&](const std::vector<fo3pipdata::Condition>* list,
        fo3dialogue::CommandFormCapture& sink){
      sink.limit=fo3script::ScriptEvidenceMaxBytes-nextBytes;
      if(list)for(const auto& condition:*list)if(condition.function==53||condition.function==79){
        if(!fo3dialogue::VariableKey(catalog_->pipboy,condition.a,condition.b,condition.function==79,&sink)){
          error=sink.exhausted?"script provenance byte limit exceeded":"script provenance condition variable unavailable";
          return false;
        }
      }
      if(!fo3script::ChargeScriptVariables(sink.variables,nextBytes)){
        error="script provenance byte limit exceeded";return false;
      }
      return true;
    };
    if(!captureConditions(conditions,conditionForms)||!captureConditions(questConditions,questConditionForms))return false;
    std::vector<std::pair<uint32_t,uint32_t>> conditionGlobals,questConditionGlobals;
    auto captureGlobals=[&](const std::vector<fo3pipdata::Condition>* list,auto& bindings){
      auto add=[&](uint32_t id){
        const auto global=catalog_->globals.find(id);
        if(!id||global==catalog_->globals.end()||!std::isfinite(global->second)){
          error="script provenance condition global unavailable";return false;
        }
        auto charged=nextBytes;
        if(!fo3script::ChargeScriptGlobals(1,charged)){
          error="script provenance byte limit exceeded";return false;
        }
        uint32_t bits;std::memcpy(&bits,&global->second,4);
        bindings.emplace_back(id,bits);nextBytes=charged;return true;
      };
      if(list)for(const auto& condition:*list){
        if(condition.function==74&&!add(condition.a))return false;
        if(condition.flags&4){uint32_t id;std::memcpy(&id,&condition.value,4);if(!add(id))return false;}
      }
      return true;
    };
    if(!captureGlobals(conditions,conditionGlobals)||!captureGlobals(questConditions,questConditionGlobals))return false;
    ScriptExecution execution;execution.origin=std::move(origin);
    execution.owner=owner;execution.activator=scriptActivator_;execution.localScript=scriptId;
    execution.parent=scriptTraceParent_;execution.reference=scriptReference_;
    if(execution.origin.kind==ScriptProgramKind::TerminalEntry){
      const auto ref=catalog_->pipboy.targets.find(owner);
      if(ref!=catalog_->pipboy.targets.end())execution.terminalLinkedReference=ref->second.linkedReference;
    }
    execution.program=script;
    execution.commandForms=std::move(forms.bindings);
    execution.expressionForms=std::move(expressionForms.bindings);
    execution.commandVariables=std::move(forms.variables);
    execution.expressionVariables=std::move(expressionForms.variables);
    execution.commandLocals=std::move(forms.locals);
    execution.expressionLocals=std::move(expressionForms.locals);
    execution.commandGlobals=std::move(forms.globals);
    execution.expressionGlobals=std::move(expressionForms.globals);
    execution.conditionVariables=std::move(conditionForms.variables);
    execution.questConditionVariables=std::move(questConditionForms.variables);
    execution.conditionGlobals=std::move(conditionGlobals);
    execution.questConditionGlobals=std::move(questConditionGlobals);
    execution.owningQuest=owningQuest;
    if(conditions)execution.conditions=*conditions;
    if(questConditions)execution.questConditions=*questConditions;
    if(container)execution.container=*container;
    if(localScript){
      execution.localVariables.reserve(localScript->variables.size());
      for(const auto& variable:localScript->variables)execution.localVariables.emplace_back(variable.first,variable.second);
      std::sort(execution.localVariables.begin(),execution.localVariables.end(),
        [](const auto& a,const auto& b){return a.first<b.first;});
    }
    scriptTrace_->push_back(std::move(execution));
    scriptTraceBytes_=nextBytes;
    scriptTraceParent_=uint32_t(scriptTrace_->size()-1);
  }
  for(const auto& c:commands) {
    if(!budget){error="script instruction budget exceeded";return false;}--budget;
    const bool active=gates.empty()||gates.back().active;
    float value=0;
    if(c.type==C::If||c.type==C::ElseIf) {
      // Native control flow never evaluates nested conditions in a skipped
      // branch, or an ElseIf after a branch has already been taken.
      const bool evaluate=c.type==C::If?active:(gates.back().parent&&!gates.back().taken);
      if(evaluate&&!fo3script::Expression(*this,owner,c.expression,error,scriptId).Evaluate(value))return false;
      if(c.type==C::If)gates.push_back({active,value!=0,active&&value!=0,false});
      else {auto& g=gates.back();g.active=evaluate&&value!=0;g.taken|=g.active;}
      continue;
    }
    if(c.type==C::Else){auto& g=gates.back();g.active=g.parent&&!g.taken;g.taken=true;continue;}
    if(c.type==C::EndIf){gates.pop_back();continue;}
    if(!active)continue;
    bool ok=true;
    switch(c.type) {
    case C::Return:return true;
    case C::Kill:ok=KillActor(c.form);break;
    case C::MoveTo:ok=MoveActorTo(c.form,c.index,c.offset);break;
    case C::ResetHealth:ok=ResetActorHealth(c.form);break;
    case C::ActorValueDamage:ok=ChangeActorValueDamage(c.form,c.index,c.value,c.variable!=0);break;
    case C::SetActorAIValue:ok=SetActorAIValue(c.form,c.index,c.value);break;
    case C::SetQuestDelay:ok=SetQuestScriptDelay(c.form,c.value);break;
    case C::FactionRelation:ok=SetFactionRelation(c.form,c.index,c.value!=0,(c.variable&1)!=0,(c.variable&2)!=0);break;
    case C::SetEssential:ok=SetEssential(c.form,c.value!=0);break;
    case C::SetActorValue:ok=SetActorVariable(c.form,c.index,c.value);break;
    case C::Look:{fo3pipdata::Location location;ok=ScriptReferenceLocation(c.form,location)&&(!c.index||ScriptReferenceLocation(c.index,location));if(ok){fo3script::Action action;action.kind=fo3script::ActionKind::Look;action.reference=c.form;action.form=c.index;ok=QueueScriptAction(std::move(action));}break;}
    case C::ScriptPackage:ok=ScriptPackage(c.form,c.index,c.value!=0);break;
    case C::StartConversation:ok=QueueScriptSpeech(c.form,uint32_t(c.variable),c.index,true);break;
    case C::SayTo:ok=QueueScriptSpeech(c.form,uint32_t(c.variable),c.index);break;
    case C::Variable:case C::Global:
      ok=fo3script::Expression(*this,owner,c.expression,error,scriptId).Evaluate(value);
      if(ok)ok=c.type==C::Variable?SetDialogueVariable(c.variable,value):SetGlobalValue(c.form,value);
      break;
    case C::EnableControls:ok=SetPlayerControls(127,true);break;
    case C::PlayerControls:ok=SetPlayerControls(c.index,c.value!=0);break;
    case C::Restrain:ok=SetRestrained(c.form,c.value!=0);break;
    case C::StartQuest:ok=StartQuest(c.form);break;
    case C::StopQuest:ok=StopQuest(c.form);break;
    case C::Stage:ok=RunQuestStage(c.form,uint16_t(c.index),depth+1,budget,error);break;
    case C::CompleteQuest:ok=EnsureQuestInstance(c.form)&&FinishQuest(c.form,fo3pipdata::Completion::Complete);break;
    case C::ObjectiveDisplay:case C::ObjectiveComplete: {
      auto q=state_.pipboy.quests.find(c.form);fo3pipdata::ObjectiveState existing;
      if(q!=state_.pipboy.quests.end()){auto o=q->second.objectives.find(c.index);if(o!=q->second.objectives.end())existing=o->second;}
      ok=EnsureQuestInstance(c.form)&&(!(c.type==C::ObjectiveDisplay&&c.value)||StartQuest(c.form))&&SetObjective(c.form,c.index,c.type==C::ObjectiveDisplay?bool(c.value):existing.displayed,
        c.type==C::ObjectiveComplete?(c.value?fo3pipdata::Completion::Complete:fo3pipdata::Completion::Active):existing.status);break;
    }
    case C::AddItem:
      ok=AddScriptItem(c.form,int32_t(c.value));
      break;
    case C::AddNote:
      if(!catalog_->pipboy.notes.count(c.form)){ok=false;break;}
      // The original maintains a unique player note list, independent of a
      // script's receiver. Existing Pip-Boy DATA/Notes consumes NOTE inventory.
      if(!ItemCount(c.form)){
        ok=pendingScriptItems_.size()<128&&Add(c.form,1);
        if(ok){pendingScriptItems_.push_back({c.form,1});++scriptEffectRevision_;}
      }
      break;
    case C::RemoveItem:ok=ScriptRemoveItem(c.form,int32_t(c.value));break;
    case C::DoorState:ok=ScriptDoorState(c.form,c.value!=0);break;
    case C::TerminalBack:ok=QueueTerminalBack(c.form);break;
    case C::ShowMap:ok=RevealMapMarker(c.form,c.value!=0);break;
    case C::FastTravel:ok=SetFastTravelEnabled(c.value!=0);break;
    case C::SetQuestObject:ok=SetQuestObject(c.form,c.value!=0);break;
    case C::Lock:ok=ScriptLock(c.form,int(c.value));break;
    case C::Unlock:ok=ScriptUnlock(c.form);break;
    case C::RewardKarma:ok=RewardKarma(c.value);break;
    case C::RewardXP:ok=RewardExperience(uint32_t(c.value));break;
    case C::CompleteAllObjectives:ok=CompleteAllObjectives(c.form);break;
    case C::SetCellFullName:ok=SetCellFullName(c.form,c.index);break;
    case C::AddAchievement:ok=AddAchievement(uint32_t(c.value));break;
    case C::Disable:ok=StageReferenceEnable(c.form,false,error);break;
    case C::Enable:ok=StageReferenceEnable(c.form,true,error);break;
    case C::EvaluatePackage:ok=ReevaluatePackage(c.form);break;
    case C::ShowMessage: {
      if(!c.expression.empty())ok=fo3script::Expression(*this,owner,c.expression,error,scriptId).Evaluate(value);
      if(ok)ok=QueueScriptMessage(c.form,value,!c.expression.empty(),owner);
      break;
    }
    case C::ModDisposition:ok=ModActorDisposition(c.form,int32_t(c.value));break;
    case C::ShowBarterMenu:
      if(!c.expression.empty())ok=fo3script::Expression(*this,owner,c.expression,error,scriptId).Evaluate(value);
      if(ok)ok=std::isfinite(value)&&value>=-100&&value<=100&&std::floor(value)==value&&QueueBarter(c.form,int(value));
      break;
    case C::Unavailable:error="unsupported executed command: "+c.expression;return false;
    case C::DefaultActivate:ok=QueueScriptDefaultActivate(c.form,c.index?c.index:scriptActivator_);break;
    case C::PlayGroup:ok=QueueScriptPlayGroup(c.form,c.expression,c.index);break;
    case C::ImageModifier:{fo3script::Action action;action.kind=fo3script::ActionKind::ImageModifier;action.reference=owner;action.form=c.form;ok=QueueScriptAction(std::move(action));break;}
    case C::PlaySound:ok=QueueScriptSound(c.form);break;
    case C::AddTopic:if(!catalog_->pipboy.dialogueTopics.count(c.form))ok=false;else if(state_.pipboy.knownTopics.insert(c.form).second)++revision_;break;
    default:ok=false;break;
    }
    if(!ok){if(error.empty())error="script command rejected by canonical Player API";return false;}
  }
  return true;
}
}


namespace fo3player {
bool Player::HasReferenceEvent(uint32_t reference,const char* event) const {
  if(!event||!reference)return false;
  const auto placed=catalog_->references.find(reference);
  if(placed==catalog_->references.end())return false;
  const auto script=catalog_->eventBaseScripts.find(placed->second.base);
  if(script==catalog_->eventBaseScripts.end())return false;
  const auto program=catalog_->eventPrograms.find(script->second);
  if(program==catalog_->eventPrograms.end())return false;
  const auto name=fo3script::Lower(event);
  for(const auto& b:program->second.blocks)if(b.event==name)return true;
  return false;
}
void Player::LogScriptEventError(uint32_t scriptId,const char* event,const std::string& error) {
  const auto key=std::to_string(scriptId)+":"+fo3script::Lower(event?event:"")+":"+error;
  if(reportedScriptEvents_.size()<4096 && !reportedScriptEvents_.insert(key).second)return;
  if(eventDiagnostic_)
    eventDiagnostic_("SCPT "+std::to_string(scriptId)+" event="+(event?event:"")+" rejected: "+error);
}
bool Player::RunSourceEvent(uint32_t scriptId,const char* event,uint32_t owner,
                            uint32_t activator,std::string& error) {
  auto plan=PrepareScriptTransaction([=](Player& candidate,std::string& e){
    return candidate.RunSourceEventDirect(scriptId,event,owner,activator,e);
  },error);
  if(!plan)return false;
  if(plan->HasReferenceChanges()){error="world mutation requires resident coordinator";return false;}
  if(CommitScriptTransaction(*plan))return true;
  error="event transaction changed before publication";return false;
}
bool Player::RunSourceEventDirect(uint32_t scriptId,const char* event,uint32_t owner,
                            uint32_t activator,std::string& error) {
  error.clear();
  const auto def=catalog_->eventPrograms.find(scriptId);
  if(def==catalog_->eventPrograms.end()) {error="script definition unavailable";return false;}
  const auto& program=def->second;
  if(!program.error.empty()) {error=program.error;return false;}
  if(program.compiledOnly) {error="compiled-only event not supported";return false;}
  const auto previousActivator=scriptActivator_,previousReference=scriptReference_;
  scriptActivator_=activator;scriptReference_=owner;
  struct RestoreContext {Player& p;uint32_t activator,reference;
    ~RestoreContext(){p.scriptActivator_=activator;p.scriptReference_=reference;}} context{*this,previousActivator,previousReference};
  unsigned budget=4096;
  for(const auto& block:program.blocks){
    if(block.event!=fo3script::Lower(event))continue;
    // A filtered event must not execute without a correctly resolved actor.
    // Other filter forms need their original reference/context evaluator.
    const auto filter=catalog_->pipboy.formNames.find(block.filter);
    if(!block.filter.empty()&&block.filter!="player"&&filter!=catalog_->pipboy.formNames.end()&&filter->second!=activator)continue;
    if(!fo3script::EventFilterMatches(block.filter,activator,PlayerRef) && !(filter!=catalog_->pipboy.formNames.end()&&filter->second==activator)){
      error="unresolved event filter '"+block.filter+"' at line "+std::to_string(block.sourceLine);
      return false;
    }
    fo3pipdata::ResultScript body;body.source=block.body;
    ScriptProgramOrigin origin;origin.kind=ScriptProgramKind::ReferenceEvent;
    origin.record=scriptId;origin.event=block.event;origin.sourceLine=block.sourceLine;
    if(!RunScript(body,owner,0,budget,error,scriptId,std::move(origin))){
      error="line "+std::to_string(block.sourceLine)+": "+error;
      return false;
    }
  }
  if(fo3script::Lower(event)=="gamemode")scriptButtons_.erase(owner);
  return true;
}
ScriptEventResult Player::DispatchReferenceEvent(uint32_t reference,const char* event,
                                                  uint32_t activator,std::string& error) {
  error.clear();
  if(!HasReferenceEvent(reference,event))return ScriptEventResult::NoHandler;
  const auto& ref=catalog_->references.at(reference);
  const auto scriptId=catalog_->eventBaseScripts.at(ref.base);
  if(!RunSourceEvent(scriptId,event,reference,activator,error)){
    LogScriptEventError(scriptId,event,error);
    return ScriptEventResult::Unsupported;
  }
  return ScriptEventResult::Executed;
}
void Player::PumpQuestGameMode(double realSeconds) {
  (void)PumpWorldAwareGameMode(realSeconds,{},[](auto){return false;});
}
bool Player::SetQuestScriptDelay(uint32_t quest,float delay){
  if(!catalog_->pipboy.quests.count(quest)||!std::isfinite(delay)||delay<0)return false;
  const auto found=state_.questScriptDelays.find(quest);
  if(found==state_.questScriptDelays.end()||found->second!=delay){state_.questScriptDelays[quest]=delay;++revision_;}
  return true;
}
float Player::QuestScriptDelay(uint32_t quest)const{
  const auto q=catalog_->pipboy.quests.find(quest);if(q==catalog_->pipboy.quests.end())return -1;
  const auto found=state_.questScriptDelays.find(quest);
  const float delay=found==state_.questScriptDelays.end()?q->second.scriptDelay:found->second;
  return delay==0?5.f:delay;
}

} // namespace fo3player

namespace fo3player {
void Player::SetScriptPlayerLocation(float x,float y,float z,uint32_t cell,uint32_t world){
  if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return;
  scriptPlayerPosition_={x,y,z};scriptPlayerCell_=cell;scriptPlayerWorld_=world;scriptPlayerPositionKnown_=true;TouchMerchantCells(cell);
}
bool Player::ScriptDistance(float& distance) const {
  const auto found=catalog_->pipboy.targets.find(scriptReference_);
  if(!scriptPlayerPositionKnown_||found==catalog_->pipboy.targets.end())return false;
  const auto& ref=found->second;
  if(ref.cell!=scriptPlayerCell_ && (!ref.world||ref.world!=scriptPlayerWorld_))return false;
  const double dx=double(ref.x)-scriptPlayerPosition_[0],dy=double(ref.y)-scriptPlayerPosition_[1],dz=double(ref.z)-scriptPlayerPosition_[2];
  const double d=std::sqrt(dx*dx+dy*dy+dz*dz);
  if(!std::isfinite(d)||d>std::numeric_limits<float>::max())return false;
  distance=float(d);return true;
}
int Player::ScriptButton() const {
  const auto i=scriptButtons_.find(scriptReference_);
  return i==scriptButtons_.end()?-1:i->second;
}
bool Player::IsQuestObject(uint32_t form) const {
  const auto item=catalog_->items.find(form);if(item==catalog_->items.end())return false;
  const auto override=state_.questObjectFlags.find(form);
  return override==state_.questObjectFlags.end()?item->second.questItem:override->second;
}
bool Player::SetQuestObject(uint32_t form,bool quest) {
  const auto item=catalog_->items.find(form);if(item==catalog_->items.end())return false;
  if(IsQuestObject(form)==quest)return true;
  if(item->second.questItem==quest)state_.questObjectFlags.erase(form);
  else state_.questObjectFlags[form]=quest;
  ++revision_;return true;
}
bool Player::RewardKarma(float amount) {
  if(!std::isfinite(amount)||!std::isfinite(state_.karma))return false;
  const float karma=std::clamp(state_.karma+amount,-1000.f,1000.f);
  const float change=karma-state_.karma;
  if(karma!=state_.karma){state_.karma=karma;++revision_;}
  if(amount!=0){pendingKarmaEvents_.push_back({change,amount});++scriptEffectRevision_;}
  return true;
}
bool Player::ReevaluatePackage(uint32_t reference){
  const auto p=catalog_->actorPlacements.find(reference);
  if(p==catalog_->actorPlacements.end()||!catalog_->weapons.actors.count(p->second.base))return false;
  // Q240's existing PackageRevision cache is invalidated by this revision.
  // The actual authored PACK selection happens in Q240SelectPackage.
  ++revision_;return true;
}
int32_t Player::ItemCount(uint32_t form) const {
  int64_t n=0;for(const auto& stack:state_.inventory)if(stack.formId==form)n+=stack.count;
  return int32_t(std::min<int64_t>(INT32_MAX,n));
}
bool Player::ScriptRemoveItem(uint32_t form,int32_t count){
  if(count<=0||!catalog_->items.count(form)||ItemCount(form)<count)return false;
  int64_t available=0;
  for(const auto& stack:state_.inventory)if(stack.formId==form&&!stack.equipped)available+=stack.count;
  if(available<count)return false;
  // Script-origin quest item consumption is explicitly distinct from player Drop.
  int32_t remaining=count;
  for(auto it=state_.inventory.begin();it!=state_.inventory.end()&&remaining;){
    if(it->formId!=form||it->equipped){++it;continue;}
    const int32_t n=std::min(remaining,it->count);remaining-=n;it->count-=n;
    if(!it->count)it=state_.inventory.erase(it);else ++it;
  }
  if(remaining)return false; // caller's event transaction restores partial removal.
  ++revision_;return true;
}
bool Player::QueueScriptMessage(uint32_t form,float argument,bool hasArgument,uint32_t owner){
  const auto m=catalog_->pipboy.messages.find(form);
  if(m==catalog_->pipboy.messages.end()||pendingScriptMessages_.size()>=16||
     (!scriptReference_&&!owner))return false;
  ScriptMessage request;request.form=form;request.owner=scriptReference_?scriptReference_:owner;request.title=m->second.title;
  request.text=m->second.text;request.buttons=m->second.buttons;
  if(hasArgument){
    const auto marker=request.text.find("%.0f");
    if(marker==std::string::npos||!std::isfinite(argument))return false;
    char number[48];std::snprintf(number,sizeof(number),"%.0f",double(argument));
    request.text.replace(marker,4,number);
  }
  if(request.text.find('%')!=std::string::npos)return false; // unknown formatting cannot be guessed
  pendingScriptMessages_.push_back(std::move(request));++scriptEffectRevision_;return true;
}
void Player::SetScriptRuntimeClock(double now,double elapsed){
  if(std::isfinite(now)&&std::isfinite(elapsed)&&elapsed>=0){scriptRuntimeSeconds_=now;scriptElapsedSeconds_=elapsed;}
}
void Player::SetScriptAnimationRegistry(std::shared_ptr<const fo3script::AnimationRegistry> registry){
  scriptAnimationDefinitions_=std::move(registry);++scriptEffectRevision_;
}
bool Player::ScriptAnimationPlaying(uint32_t reference,bool& playing) const {
  if(!scriptAnimationDefinitions_||!scriptAnimationDefinitions_->count(reference))return false;
  const auto i=scriptAnimations_.find(reference);
  playing=i!=scriptAnimations_.end()&&fo3script::Playing(i->second,scriptRuntimeSeconds_);return true;
}
bool Player::QueueScriptPlayGroup(uint32_t reference,const std::string& group,uint32_t flags){
  // The object bridge supports immediate start (1) and the authored object
  // restart mode (5). Normal (0) is admitted only when no sequence is active;
  // queued/loop-entry initialization needs a separate animation queue.
  if(!scriptAnimationDefinitions_||(flags!=0&&flags!=1&&flags!=5))return false;
  if(flags==0){bool playing=false;if(ScriptAnimationPlaying(reference,playing)&&playing)return false;}
  const auto found=scriptAnimationDefinitions_->find(reference);if(found==scriptAnimationDefinitions_->end())return false;
  const auto name=fo3script::Lower(group);
  for(const auto& clip:found->second)if(fo3script::Lower(clip.name)==name){
    if(!std::isfinite(clip.duration)||clip.duration<0)return false;
    fo3script::Action action;action.kind=fo3script::ActionKind::PlayGroup;action.reference=reference;
    action.group=name;action.flags=flags;action.started=scriptRuntimeSeconds_;
    if(!QueueScriptAction(std::move(action)))return false;
    scriptAnimations_[reference]={name,scriptRuntimeSeconds_,clip.duration,clip.loop};return true;
  }
  return false;
}
bool Player::QueueScriptDefaultActivate(uint32_t reference,uint32_t activator){
  if(reference!=scriptReference_||activator!=PlayerRef)return false;
  if(!CanScriptActivateDoor(reference)){
    if(!CanScriptActivateContainer(reference)||!PrepareInventory(reference,false))return false;
  }
  fo3script::Action action;action.kind=fo3script::ActionKind::DefaultActivate;action.reference=reference;
  return QueueScriptAction(std::move(action));
}
bool Player::QueueScriptAction(fo3script::Action action){
  if(pendingScriptActions_.size()>=64)return false;
  if(action.kind==fo3script::ActionKind::ImageModifier&&(!catalog_->imageModifiers.count(action.form)||!catalog_->imageModifiers.at(action.form).valid))return false;
  pendingScriptActions_.push_back(std::move(action));++scriptEffectRevision_;return true;
}
bool Player::PollScriptAction(fo3script::Action& out){
  if(pendingScriptActions_.empty())return false;
  out=std::move(pendingScriptActions_.front());pendingScriptActions_.erase(pendingScriptActions_.begin());++scriptEffectRevision_;return true;
}
std::vector<std::pair<uint32_t,bool>> Player::ScriptTriggerChanges() const {
  std::vector<std::pair<uint32_t,bool>> changes;
  if(!scriptPlayerPositionKnown_)return changes;
  for(const auto& entry:catalog_->pipboy.triggerPrimitives){
    const auto target=catalog_->pipboy.targets.find(entry.first);if(target==catalog_->pipboy.targets.end())continue;
    const auto& p=target->second;
    const bool resident=scriptPlayerWorld_?p.world==scriptPlayerWorld_:p.cell==scriptPlayerCell_;
    bool disabled=true;std::string error;
    const bool inside=resident&&ReferenceDisabled(entry.first,disabled,error)&&!disabled&&fo3script::Contains(p,entry.second,scriptPlayerPosition_);
    if(inside!=bool(scriptInsideTriggers_.count(entry.first)))changes.push_back({entry.first,inside});
  }
  std::sort(changes.begin(),changes.end(),[](const auto& a,const auto& b){return a.first<b.first;});return changes;
}
void Player::AcknowledgeScriptTrigger(uint32_t reference,bool inside){
  if(inside)scriptInsideTriggers_.insert(reference);else scriptInsideTriggers_.erase(reference);
}
bool Player::QueueScriptSound(uint32_t form){
  if(!form||pendingScriptSounds_.size()>=16)return false;
  pendingScriptSounds_.push_back(form);++scriptEffectRevision_;return true;
}
bool Player::PollScriptMessage(ScriptMessage& out){
  if(pendingScriptMessages_.empty())return false;
  out=std::move(pendingScriptMessages_.front());pendingScriptMessages_.erase(pendingScriptMessages_.begin());++scriptEffectRevision_;return true;
}
bool Player::SubmitScriptMessageButton(uint32_t owner,int index){
  if(!owner||index<0||index>31||scriptButtons_.count(owner))return false;
  scriptButtons_.emplace(owner,index);++scriptEffectRevision_;return true;
}
bool Player::PollScriptSound(uint32_t& out){
  if(pendingScriptSounds_.empty())return false;
  out=pendingScriptSounds_.front();pendingScriptSounds_.erase(pendingScriptSounds_.begin());++scriptEffectRevision_;return true;
}
bool Player::SourceEventCompileReady(uint32_t script,const char* event,uint32_t owner,
    uint32_t activator,std::string& error) const {
  const auto found=catalog_->eventPrograms.find(script);
  if(found==catalog_->eventPrograms.end()){error="script definition unavailable";return false;}
  const auto& program=found->second;
  if(!program.error.empty()){error=program.error;return false;}
  if(program.compiledOnly){error="compiled-only event not supported";return false;}
  // Only the first matching block can be checked ahead of execution. A later
  // block must retain the earlier block's dynamic errors and effect ordering.
  for(const auto& block:program.blocks)if(block.event==fo3script::Lower(event)){
    const auto filter=catalog_->pipboy.formNames.find(block.filter);
    if(!block.filter.empty()&&block.filter!="player"&&filter!=catalog_->pipboy.formNames.end()&&filter->second!=activator)continue;
    if(!fo3script::EventFilterMatches(block.filter,activator,PlayerRef) && !(filter!=catalog_->pipboy.formNames.end()&&filter->second==activator)){
      error="unresolved event filter '"+block.filter+"' at line "+std::to_string(block.sourceLine);return false;
    }
    if(!scriptCompileCache_)scriptCompileCache_=std::make_shared<fo3script::CompileCache>(catalog_->pipboy);
    fo3pipdata::ResultScript body;body.source=block.body;
    const auto compiled=scriptCompileCache_->Get(body,owner,script,false);
    if(!compiled->valid){error="line "+std::to_string(block.sourceLine)+": "+compiled->error;return false;}
    break;
  }
  return true;
}
bool Player::PumpWorldAwareGameMode(double seconds,const std::vector<uint32_t>& resident,
    const std::function<bool(std::unique_ptr<ScriptTransactionPlan>)>& reserveWorld) {
  if(!std::isfinite(seconds)||seconds<=0||seconds>1||!reserveWorld)return false;
  // Reference scripts retain their bounded cadence. Quest scripts use their
  // original DATA/SetQuestDelay interval, with at most one run per frame.
  questScriptAccumulator_=std::min(.5,questScriptAccumulator_+seconds);
  const bool referencesDue=questScriptAccumulator_>=.25;
  const double referenceElapsed=questScriptAccumulator_;
  if(referencesDue)questScriptAccumulator_=0;
  questScriptClock_+=seconds;
  auto run=[&](uint32_t script,const char* event,uint32_t owner,bool reference)->bool {
    std::string error;
    // The ordinary compiler rejects the same immutable unsupported body every
    // tick. Do that before copying the full mutable player state. No runtime
    // expression outcome or successful execution is cached or skipped.
    if(!SourceEventCompileReady(script,event,owner,0,error)){
      if(reference&&scriptButtons_.erase(owner))++scriptEffectRevision_;
      LogScriptEventError(script,event,error);return false;
    }
    // Capture the *same* original event that would be committed. An
    // uncommitted candidate has no world, inventory or effect side effects.
    auto plan=PrepareScriptTransaction([=](Player& candidate,std::string& e){
      return candidate.RunSourceEventDirect(script,event,owner,0,e);
    },error,true);
    if(!plan){
      // Some existing non-world scripts are source-supported but cannot
      // capture the stricter resident provenance yet. Preserve their existing
      // transactional gameplay path; it never publishes world changes.
      if(RunSourceEvent(script,event,owner,0,error))return false;
    }else if(!plan->HasReferenceChanges()){
      if(CommitScriptTransaction(*plan))return false;
      error="GameMode script transaction changed before publication";
    }else {
      if(reserveWorld(std::move(plan)))return true;
      error="resident GameMode scene reservation unavailable";
    }
    if(reference&&scriptButtons_.erase(owner))++scriptEffectRevision_;
    LogScriptEventError(script,event,error);
    return false;
  };
  // Q230: immutable ESM quest/script association is indexed once per Player.
  // Construct the live *starting* eligible set each tick: a quest started by
  // another GameMode result must not run until the next tick (source ordering).
  if(!q230GameModeQuestIndexReady_){
    q230GameModeQuestIds_.clear();
    for(const auto& q:catalog_->pipboy.quests){
      if(!q.second.script)continue;
      const auto events=catalog_->eventPrograms.find(q.second.script);
      if(events==catalog_->eventPrograms.end())continue;
      for(const auto& block:events->second.blocks)if(block.event=="gamemode"){
        q230GameModeQuestIds_.push_back(q.first);break;
      }
    }
    std::sort(q230GameModeQuestIds_.begin(),q230GameModeQuestIds_.end());
    q230GameModeQuestIndexReady_=true;
  }
  std::vector<uint32_t> quests;
  quests.reserve(q230GameModeQuestIds_.size());
  for(uint32_t id:q230GameModeQuestIds_){
    const auto q=catalog_->pipboy.quests.find(id);
    if(q==catalog_->pipboy.quests.end())continue;
    const auto saved=state_.pipboy.quests.find(id);
    if(saved!=state_.pipboy.quests.end()?saved->second.running:bool(q->second.flags&1))
      quests.push_back(id);
    else questScriptLastTicks_.erase(id);
  }
  for(const auto id:quests){
    const auto q=catalog_->pipboy.quests.find(id);
    if(q==catalog_->pipboy.quests.end())continue;
    const auto saved=state_.pipboy.quests.find(id);
    if(!(saved!=state_.pipboy.quests.end()?saved->second.running:bool(q->second.flags&1))){questScriptLastTicks_.erase(id);continue;}
    auto tick=questScriptLastTicks_.emplace(id,questScriptClock_-seconds).first;
    const double elapsed=questScriptClock_-tick->second;
    const float interval=QuestScriptDelay(id);
    if(!std::isfinite(interval)||interval<0){LogScriptEventError(q->second.script,"GameMode","invalid quest processing interval");continue;}
    if(elapsed+1e-7<interval)continue;
    tick->second=questScriptClock_;scriptElapsedSeconds_=elapsed;
    if(run(q->second.script,"GameMode",id,false))return true;
  }
  if(!referencesDue)return false;
  scriptElapsedSeconds_=referenceElapsed;
  auto refs=resident;std::sort(refs.begin(),refs.end());
  refs.erase(std::unique(refs.begin(),refs.end()),refs.end());
  for(const auto id:refs){
    if(!HasReferenceEvent(id,"GameMode"))continue;
    const auto ref=catalog_->references.find(id);
    if(ref==catalog_->references.end()||(ref->second.flags&0x20u))continue;
    bool disabled=false;std::string error;
    if(!ReferenceDisabled(id,disabled,error)||disabled)continue;
    const auto script=catalog_->eventBaseScripts.at(ref->second.base);
    if(run(script,"GameMode",id,true))return true;
  }
  return false;
}
void Player::PumpReferenceGameMode(double realSeconds,const std::vector<uint32_t>& resident){
  if(!std::isfinite(realSeconds)||realSeconds<=0||realSeconds>1)return;
  referenceScriptAccumulator_=std::min(.5,referenceScriptAccumulator_+realSeconds);
  if(referenceScriptAccumulator_<.25)return;
  scriptElapsedSeconds_=referenceScriptAccumulator_;
  referenceScriptAccumulator_=0;
  auto ids=resident;std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
  for(uint32_t id:ids){
    if(!HasReferenceEvent(id,"GameMode"))continue;
    const auto ref=catalog_->references.find(id);
    if(ref==catalog_->references.end()||(ref->second.flags&0x820u))continue;
    std::string error;
    const auto sid=catalog_->eventBaseScripts.at(ref->second.base);
    if(!RunSourceEvent(sid,"GameMode",id,0,error)){
      // Reject the side effects, but consume the delivered input exactly once.
      // Otherwise unsupported stage/world commands repeat on every 4 Hz tick.
      if(scriptButtons_.erase(id))++scriptEffectRevision_;
      LogScriptEventError(sid,"GameMode",error);
    }
  }
}
} // namespace fo3player

namespace fo3player {
bool Player::ScriptReferenceLocation(uint32_t id,fo3pipdata::Location& out)const{
  if(id==PlayerRef){if(!scriptPlayerPositionKnown_)return false;out={scriptPlayerCell_,scriptPlayerWorld_,scriptPlayerPosition_[0],scriptPlayerPosition_[1],0};return true;}
  const auto actor=state_.actors.find(id);
  if(actor!=state_.actors.end()){const auto& a=actor->second;out={a.cell,a.world,a.position[0],a.position[1],a.yaw};return true;}
  const auto placed=catalog_->pipboy.targets.find(id);if(placed==catalog_->pipboy.targets.end())return false;
  const auto& p=placed->second;out={p.cell,p.world,p.x,p.y,p.rz};return true;
}
bool Player::ScriptPackage(uint32_t reference,uint32_t package,bool add){
  const auto placed=catalog_->actorPlacements.find(reference);
  if(placed==catalog_->actorPlacements.end()||!catalog_->weapons.actors.count(placed->second.base)||!catalog_->pipboy.packages.count(package))return false;
  if(!add&&!state_.scriptPackages.count(reference))return true;
  if(add&&!state_.scriptPackages.count(reference)&&state_.scriptPackages.size()>=10000)return false;
  if(add&&state_.scriptPackages.count(reference)){const auto& list=state_.scriptPackages.at(reference);if(list.size()>=128&&std::find(list.begin(),list.end(),package)==list.end())return false;}
  auto& packages=state_.scriptPackages[reference];
  const auto old=packages;packages.erase(std::remove(packages.begin(),packages.end(),package),packages.end());
  if(add)packages.insert(packages.begin(),package);
  if(packages==old)return true;
  if(packages.empty())state_.scriptPackages.erase(reference);
  ++revision_;++scriptEffectRevision_;return ReevaluatePackage(reference);
}
}

namespace fo3player {
bool Player::QueueScriptSpeech(uint32_t reference,uint32_t topic,uint32_t target,bool conversation){
  static thread_local unsigned selecting=0;
  if(selecting>=8)return false;
  struct Guard{unsigned& depth;Guard(unsigned& d):depth(d){++depth;}~Guard(){--depth;}} guard(selecting);
  // Say selects an INFO; its results execute later in the voice adapter.
  // The enclosing captured SCPT must not attempt a nested detached publication.
  // Preflight against an isolated, read-only snapshot of this exact command state.
  Player selection(*this,ScriptCandidateTag{});
  fo3dialogue::Context context;context.player=&selection;context.talking=true;
  if(!fo3dialogue::ReferenceContext(*this,reference,context.speaker)||!fo3dialogue::ReferenceContext(*this,target,context.target))return false;
  static thread_local std::mt19937 random{std::random_device{}()};
  // This only binds the authored INFO identity in the script transaction.
  // Scene mutation capability is admitted again by the actual voice adapter.
  const auto preview=[](const auto&,const auto&,std::string&){return true;};
  bool unsupported=false;
  const auto diagnostic=[&](const std::string& message){unsupported=true;if(eventDiagnostic_)eventDiagnostic_(message);};
  const auto selected=fo3dialogue::ResolveTopicInfo(topic,context,[&](const auto& info){return fo3dialogue::EligibleInfo(info,context,diagnostic,preview);},&random);
  // A legitimate empty selection is a no-op. An unsupported preflight must
  // roll back the caller too, rather than saving its waiting flags without speech.
  if(!selected)return !unsupported;
  fo3script::Action action;action.kind=conversation?fo3script::ActionKind::Conversation:fo3script::ActionKind::SayTo;action.reference=reference;action.form=topic;action.flags=target;action.info=selected->id;
  return QueueScriptAction(std::move(action));
}
}
