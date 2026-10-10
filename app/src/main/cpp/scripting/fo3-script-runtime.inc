// Shared source-script execution for dialogue results and QUST stage items.
// Compiled-only bytecode and event blocks are retained, never guessed or skipped.
namespace fo3script {
class Expression {
  const fo3player::Player& p; uint32_t owner;
  std::vector<std::string> tokens; size_t at=0; std::string& error;
  bool Fail(const std::string& text){if(error.empty())error=text;return false;}
  bool Atom(float& value) {
    if(at==tokens.size())return Fail("missing expression operand");
    auto t=tokens[at++];
    if(t=="("){if(!Or(value)||at==tokens.size()||tokens[at++]!=")")return Fail("unbalanced expression");return true;}
    if(t=="-"||t=="+"){if(!Atom(value))return false;if(t=="-")value=-value;return true;}
    char* end=nullptr;value=std::strtof(t.c_str(),&end);
    if(end!=t.c_str()&&!*end)return std::isfinite(value)||Fail("nonfinite literal");
    const auto& d=p.Definitions().pipboy;
    auto form=[&](const std::string& n){auto f=d.formNames.find(n);return f==d.formNames.end()?0:f->second;};
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
    uint64_t key=fo3dialogue::VariableKey(d,t);
    if(!key&&owner&&t.find('.')==std::string::npos){auto q=d.quests.find(owner);if(q!=d.quests.end())key=fo3dialogue::VariableKey(d,q->second.editor+"."+t);}
    if(key){auto v=p.Snapshot().pipboy.dialogueVariables.find(key);value=v==p.Snapshot().pipboy.dialogueVariables.end()?0:v->second;return true;}
    const auto id=form(t);if(id&&p.GlobalValue(id,value))return true;
    return Fail("unsupported expression operand: "+t);
  }
  bool Sum(float& value){if(!Atom(value))return false;while(at<tokens.size()&&(tokens[at]=="+"||tokens[at]=="-")){auto op=tokens[at++];float b;if(!Atom(b))return false;value=op=="+"?value+b:value-b;}return std::isfinite(value)||Fail("expression overflow");}
  bool Compare(float& value){if(!Sum(value))return false;if(at<tokens.size()){auto op=tokens[at];if(op=="=="||op=="!="||op=="<"||op==">"||op=="<="||op==">="){++at;float b;if(!Sum(b))return false;value=op=="=="?value==b:op=="!="?value!=b:op=="<"?value<b:op==">"?value>b:op=="<="?value<=b:value>=b;}}return true;}
  bool And(float& value){if(!Compare(value))return false;while(at<tokens.size()&&tokens[at]=="&&"){++at;float b;if(!Compare(b))return false;value=value!=0&&b!=0;}return true;}
  bool Or(float& value){if(!And(value))return false;while(at<tokens.size()&&tokens[at]=="||"){++at;float b;if(!And(b))return false;value=value!=0||b!=0;}return true;}
public:
  Expression(const fo3player::Player& player,uint32_t o,const std::string& text,std::string& e):p(player),owner(o),error(e) {
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
namespace fo3player {
bool Player::PreviewDialogueResult(const fo3pipdata::ResultScript& script,std::string& error) const {
  return PreviewDialogueResults(script,{},error);
}
bool Player::PreviewDialogueResults(const fo3pipdata::ResultScript& begin,const fo3pipdata::ResultScript& end,std::string& error) const {
  // Session APIs are render-thread owned. Only mutable state/revision are touched
  // by the admitted commands. RAII restores them even if compilation throws.
  // Do not copy the immutable ESM catalogue for every visible dialogue INFO.
  struct Restore {State& state;uint64_t& revision;State before;uint64_t old;
    ~Restore(){state=std::move(before);revision=old;}} restore{state_,revision_,state_,revision_};
  auto& preview=*const_cast<Player*>(this);error.clear();unsigned budget=4096;
  return preview.RunScript(begin,0,0,budget,error)&&preview.RunScript(end,0,0,budget,error);
}
bool Player::ExecuteDialogueResult(const fo3pipdata::ResultScript& script,std::string& error) {
  error.clear();const auto before=state_;const auto revision=revision_;unsigned budget=4096;
  if(RunScript(script,0,0,budget,error))return true;
  state_=before;revision_=revision;return false;
}
bool Player::ExecuteQuestStage(uint32_t id,uint16_t stage,std::string& error) {
  error.clear();const auto before=state_;const auto revision=revision_;unsigned budget=4096;
  if(RunQuestStage(id,stage,0,budget,error))return true;
  state_=before;revision_=revision;return false;
}
bool Player::RunQuestStage(uint32_t id,uint16_t index,unsigned depth,unsigned& budget,std::string& error) {
  auto fail=[&](const std::string& s){error="QUST "+std::to_string(id)+" stage "+std::to_string(index)+": "+s;return false;};
  if(depth>=32||!budget)return fail("stage recursion/budget exceeded");
  --budget;auto q=catalog_.pipboy.quests.find(id);
  if(q==catalog_.pipboy.quests.end()||!q->second.stages.count(index))return fail("unresolved stage");
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
    if(!RunScript(item.result,id,depth+1,budget,error))return fail("item "+std::to_string(i)+": "+error);
    // Reacquire after scripts: nested stages can rehash the quest instance map.
    if(!item.log.empty())state_.pipboy.quests.at(id).journal.push_back({index,uint32_t(i)});
    if(item.flags&3)if(!FinishQuest(id,(item.flags&2)?fo3pipdata::Completion::Failed:fo3pipdata::Completion::Complete))return fail("completion rejected");
  }
  return true;
}
bool Player::RunScript(const fo3pipdata::ResultScript& script,uint32_t owner,unsigned depth,unsigned& budget,std::string& error) {
  if(depth>=32){error="script nesting exceeded";return false;}
  std::vector<fo3dialogue::Command> commands;
  if(!fo3dialogue::CompileResult(catalog_.pipboy,script,commands,error))return false;
  using C=fo3dialogue::CommandType;
  // Validate all expressions and block structure, including unreachable branches.
  struct Gate {bool parent,taken,active,hasElse;};std::vector<Gate> gates;
  for(const auto& c:commands) {
    if(c.type==C::If)gates.push_back({true,false,false,false});
    if(c.type==C::ElseIf||c.type==C::Else){if(gates.empty()||gates.back().hasElse){error="invalid else/elseif";return false;}if(c.type==C::Else)gates.back().hasElse=true;}
    if(c.type==C::EndIf){if(gates.empty()){error="orphan endif";return false;}gates.pop_back();}
    if(c.type==C::If||c.type==C::ElseIf||c.type==C::Variable||c.type==C::Global){float v;if(!fo3script::Expression(*this,owner,c.expression,error).Evaluate(v))return false;}
  }
  if(!gates.empty()){error="unterminated if";return false;}
  for(const auto& c:commands) {
    if(!budget){error="script instruction budget exceeded";return false;}--budget;
    const bool active=gates.empty()||gates.back().active;
    float value=0;
    if(c.type==C::If||c.type==C::ElseIf) {
      if(!fo3script::Expression(*this,owner,c.expression,error).Evaluate(value))return false;
      if(c.type==C::If)gates.push_back({active,value!=0,active&&value!=0,false});
      else {auto& g=gates.back();g.active=g.parent&&!g.taken&&value!=0;g.taken|=value!=0;}
      continue;
    }
    if(c.type==C::Else){auto& g=gates.back();g.active=g.parent&&!g.taken;g.taken=true;continue;}
    if(c.type==C::EndIf){gates.pop_back();continue;}
    if(!active)continue;
    bool ok=true;
    switch(c.type) {
    case C::Variable:case C::Global:
      ok=fo3script::Expression(*this,owner,c.expression,error).Evaluate(value);
      if(ok)ok=c.type==C::Variable?SetDialogueVariable(c.variable,value):SetGlobalValue(c.form,value);
      break;
    case C::EnableControls:break; // Existing VR interaction keeps controls enabled.
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
    case C::AddItem:ok=Add(c.form,int32_t(c.value));break;
    case C::AddTopic:if(!catalog_.pipboy.dialogueTopics.count(c.form))ok=false;else if(state_.pipboy.knownTopics.insert(c.form).second)++revision_;break;
    default:ok=false;break;
    }
    if(!ok){if(error.empty())error="script command rejected by canonical Player API";return false;}
  }
  return true;
}
}
