// Shared source-script execution for dialogue results and QUST stage items.
// Compiled-only bytecode and event blocks are retained, never guessed or skipped.
namespace fo3script {
class Expression {
  const fo3player::Player& p; uint32_t owner,scriptId;
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
    if(t=="isactionref"){
      if(at==tokens.size()||tokens[at++]!="player")return Fail("unsupported action reference");
      value=p.ScriptActivator()==fo3player::PlayerRef;return true;
    }
    if(t=="player.getav"){
      if(at==tokens.size())return Fail("missing actor value");
      const auto name=tokens[at++];
      static const char* skills[]={"barter","bigguns","energyweapons","explosives","lockpick","medicine","meleeweapons","repair","science","smallguns","sneak","speech","throwing","unarmed"};
      for(size_t i=0;i<14;++i)if(name==skills[i]){value=p.Snapshot().skills[i];return true;}
      return Fail("unsupported player actor value "+name);
    }
    if(t=="player.getitemcount"){
      if(at==tokens.size())return Fail("missing item form");
      const auto id=form(tokens[at++]);if(!id)return Fail("unresolved item form");
      value=p.ItemCount(id);return true;
    }
    if(t=="getbuttonpressed"){value=p.ScriptButton();return true;}
    if(t=="getdistance"){
      if(at==tokens.size()||tokens[at++]!="player")return Fail("unsupported distance target");
      return p.ScriptDistance(value)||Fail("player/reference distance unavailable");
    }
    uint64_t key=fo3dialogue::VariableKey(d,t);
    if(!key&&owner&&t.find('.')==std::string::npos){auto q=d.quests.find(owner);if(q!=d.quests.end())key=fo3dialogue::VariableKey(d,q->second.editor+"."+t);}
    if(!key)key=fo3script::NumericLocalKey(d,owner,scriptId,t);
    if(key){auto v=p.Snapshot().pipboy.dialogueVariables.find(key);value=v==p.Snapshot().pipboy.dialogueVariables.end()?0:v->second;return true;}
    const auto id=form(t);if(id&&p.GlobalValue(id,value))return true;
    return Fail("unsupported expression operand: "+t);
  }
  bool Sum(float& value){if(!Atom(value))return false;while(at<tokens.size()&&(tokens[at]=="+"||tokens[at]=="-")){auto op=tokens[at++];float b;if(!Atom(b))return false;value=op=="+"?value+b:value-b;}return std::isfinite(value)||Fail("expression overflow");}
  bool Compare(float& value){if(!Sum(value))return false;if(at<tokens.size()){auto op=tokens[at];if(op=="=="||op=="!="||op=="<"||op==">"||op=="<="||op==">="){++at;float b;if(!Sum(b))return false;value=op=="=="?value==b:op=="!="?value!=b:op=="<"?value<b:op==">"?value>b:op=="<="?value<=b:value>=b;}}return true;}
  bool And(float& value){if(!Compare(value))return false;while(at<tokens.size()&&tokens[at]=="&&"){++at;float b;if(!Compare(b))return false;value=value!=0&&b!=0;}return true;}
  bool Or(float& value){if(!And(value))return false;while(at<tokens.size()&&tokens[at]=="||"){++at;float b;if(!And(b))return false;value=value!=0||b!=0;}return true;}
public:
  Expression(const fo3player::Player& player,uint32_t o,const std::string& text,std::string& e,uint32_t sid=0):p(player),owner(o),scriptId(sid),error(e) {
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
bool Player::RunScript(const fo3pipdata::ResultScript& script,uint32_t owner,unsigned depth,unsigned& budget,std::string& error,uint32_t scriptId) {
  if(depth>=32){error="script nesting exceeded";return false;}
  if(!scriptId && owner){
    const auto quest=catalog_.pipboy.quests.find(owner);
    if(quest!=catalog_.pipboy.quests.end())scriptId=quest->second.script;
  }
  std::vector<fo3dialogue::Command> commands;
  if(!fo3dialogue::CompileResult(catalog_.pipboy,script,commands,error,owner,scriptId))return false;
  using C=fo3dialogue::CommandType;
  // Validate all expressions and block structure, including unreachable branches.
  struct Gate {bool parent,taken,active,hasElse;};std::vector<Gate> gates;
  for(const auto& c:commands) {
    if(c.type==C::If)gates.push_back({true,false,false,false});
    if(c.type==C::ElseIf||c.type==C::Else){if(gates.empty()||gates.back().hasElse){error="invalid else/elseif";return false;}if(c.type==C::Else)gates.back().hasElse=true;}
    if(c.type==C::EndIf){if(gates.empty()){error="orphan endif";return false;}gates.pop_back();}
    if(c.type==C::If||c.type==C::ElseIf||c.type==C::Variable||c.type==C::Global||(c.type==C::ShowMessage&&!c.expression.empty())){float v;if(!fo3script::Expression(*this,owner,c.expression,error,scriptId).Evaluate(v))return false;}
  }
  if(!gates.empty()){error="unterminated if";return false;}
  for(const auto& c:commands) {
    if(!budget){error="script instruction budget exceeded";return false;}--budget;
    const bool active=gates.empty()||gates.back().active;
    float value=0;
    if(c.type==C::If||c.type==C::ElseIf) {
      if(!fo3script::Expression(*this,owner,c.expression,error,scriptId).Evaluate(value))return false;
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
      ok=fo3script::Expression(*this,owner,c.expression,error,scriptId).Evaluate(value);
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
    case C::RemoveItem:ok=ScriptRemoveItem(c.form,int32_t(c.value));break;
    case C::SetQuestObject:ok=SetQuestObject(c.form,c.value!=0);break;
    case C::RewardKarma:ok=RewardKarma(c.value);break;
    case C::EvaluatePackage:ok=ReevaluatePackage(c.form);break;
    case C::ShowMessage: {
      if(!c.expression.empty())ok=fo3script::Expression(*this,owner,c.expression,error,scriptId).Evaluate(value);
      if(ok)ok=QueueScriptMessage(c.form,value,!c.expression.empty());
      break;
    }
    case C::PlaySound:ok=QueueScriptSound(c.form);break;
    case C::AddTopic:if(!catalog_.pipboy.dialogueTopics.count(c.form))ok=false;else if(state_.pipboy.knownTopics.insert(c.form).second)++revision_;break;
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
  const auto placed=catalog_.references.find(reference);
  if(placed==catalog_.references.end())return false;
  const auto script=catalog_.eventBaseScripts.find(placed->second.base);
  if(script==catalog_.eventBaseScripts.end())return false;
  const auto program=catalog_.eventPrograms.find(script->second);
  if(program==catalog_.eventPrograms.end())return false;
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
  error.clear();
  const auto def=catalog_.eventPrograms.find(scriptId);
  if(def==catalog_.eventPrograms.end()) {error="script definition unavailable";return false;}
  const auto& program=def->second;
  if(!program.error.empty()) {error=program.error;return false;}
  if(program.compiledOnly) {error="compiled-only event not supported";return false;}
  const auto before=state_;const auto revision=revision_;
  const auto messageCount=pendingScriptMessages_.size(),soundCount=pendingScriptSounds_.size();
  const auto previousActivator=scriptActivator_,previousReference=scriptReference_;
  scriptActivator_=activator;scriptReference_=owner;
  struct RestoreContext {Player& p;uint32_t activator,reference;
    ~RestoreContext(){p.scriptActivator_=activator;p.scriptReference_=reference;}} context{*this,previousActivator,previousReference};
  unsigned budget=4096;
  for(const auto& block:program.blocks){
    if(block.event!=fo3script::Lower(event))continue;
    // A filtered event must not execute without a correctly resolved actor.
    // Other filter forms need their original reference/context evaluator.
    if(!block.filter.empty() && !(block.filter=="player"&&activator==PlayerRef)){
      error="unresolved event filter '"+block.filter+"' at line "+std::to_string(block.sourceLine);
      state_=before;revision_=revision;
      pendingScriptMessages_.resize(messageCount);pendingScriptSounds_.resize(soundCount);return false;
    }
    fo3pipdata::ResultScript body;body.source=block.body;
    if(!RunScript(body,owner,0,budget,error,scriptId)){
      error="line "+std::to_string(block.sourceLine)+": "+error;
      state_=before;revision_=revision;
      pendingScriptMessages_.resize(messageCount);pendingScriptSounds_.resize(soundCount);return false;
    }
  }
  if(fo3script::Lower(event)=="gamemode")scriptButtons_.erase(owner);
  return true;
}
ScriptEventResult Player::DispatchReferenceEvent(uint32_t reference,const char* event,
                                                  uint32_t activator,std::string& error) {
  error.clear();
  if(!HasReferenceEvent(reference,event))return ScriptEventResult::NoHandler;
  const auto& ref=catalog_.references.at(reference);
  const auto scriptId=catalog_.eventBaseScripts.at(ref.base);
  if(!RunSourceEvent(scriptId,event,reference,activator,error)){
    LogScriptEventError(scriptId,event,error);
    return ScriptEventResult::Unsupported;
  }
  return ScriptEventResult::Executed;
}
void Player::PumpQuestGameMode(double realSeconds) {
  if(!std::isfinite(realSeconds)||realSeconds<=0||realSeconds>1)return;
  // Script cadence belongs to the simulation, never each stereo eye.
  // 4 Hz is a bounded Quest adaptation, not a claim of PC GameMode parity.
  questScriptAccumulator_=std::min(0.5,questScriptAccumulator_+realSeconds);
  if(questScriptAccumulator_<0.25)return;
  questScriptAccumulator_-=0.25;
  std::vector<uint32_t> ids;
  for(const auto& q:catalog_.pipboy.quests){
    if(!q.second.script)continue;
    const auto script=catalog_.eventPrograms.find(q.second.script);
    if(script==catalog_.eventPrograms.end())continue;
    bool hasGameMode=false;
    for(const auto& b:script->second.blocks)if(b.event=="gamemode"){hasGameMode=true;break;}
    if(!hasGameMode)continue;
    const auto saved=state_.pipboy.quests.find(q.first);
    if(saved!=state_.pipboy.quests.end()?saved->second.running:bool(q.second.flags&1))
      ids.push_back(q.first);
  }
  std::sort(ids.begin(),ids.end());
  for(const auto id:ids){
    const auto q=catalog_.pipboy.quests.find(id);
    if(q==catalog_.pipboy.quests.end())continue;
    // Nested result scripts can stop another quest in the selected set.
    const auto saved=state_.pipboy.quests.find(id);
    if(!(saved!=state_.pipboy.quests.end()?saved->second.running:bool(q->second.flags&1)))continue;
    std::string error;
    if(!RunSourceEvent(q->second.script,"GameMode",id,0,error))
      LogScriptEventError(q->second.script,"GameMode",error);
  }
}
} // namespace fo3player

namespace fo3player {
void Player::SetScriptPlayerLocation(float x,float y,float z,uint32_t cell,uint32_t world){
  if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return;
  scriptPlayerPosition_={x,y,z};scriptPlayerCell_=cell;scriptPlayerWorld_=world;scriptPlayerPositionKnown_=true;
}
bool Player::ScriptDistance(float& distance) const {
  const auto found=catalog_.pipboy.targets.find(scriptReference_);
  if(!scriptPlayerPositionKnown_||found==catalog_.pipboy.targets.end())return false;
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
  const auto item=catalog_.items.find(form);if(item==catalog_.items.end())return false;
  const auto override=state_.questObjectFlags.find(form);
  return override==state_.questObjectFlags.end()?item->second.questItem:override->second;
}
bool Player::SetQuestObject(uint32_t form,bool quest) {
  const auto item=catalog_.items.find(form);if(item==catalog_.items.end())return false;
  if(IsQuestObject(form)==quest)return true;
  if(item->second.questItem==quest)state_.questObjectFlags.erase(form);
  else state_.questObjectFlags[form]=quest;
  ++revision_;return true;
}
bool Player::RewardKarma(float amount) {
  if(!std::isfinite(amount)||!std::isfinite(state_.karma))return false;
  const float karma=std::clamp(state_.karma+amount,-1000.f,1000.f);
  if(karma!=state_.karma){state_.karma=karma;++revision_;}
  return true;
}
bool Player::ReevaluatePackage(uint32_t reference){
  const auto p=catalog_.actorPlacements.find(reference);
  if(p==catalog_.actorPlacements.end()||!catalog_.weapons.actors.count(p->second.base))return false;
  // Q240's existing PackageRevision cache is invalidated by this revision.
  // The actual authored PACK selection happens in Q240SelectPackage.
  ++revision_;return true;
}
int32_t Player::ItemCount(uint32_t form) const {
  int64_t n=0;for(const auto& stack:state_.inventory)if(stack.formId==form)n+=stack.count;
  return int32_t(std::min<int64_t>(INT32_MAX,n));
}
bool Player::ScriptRemoveItem(uint32_t form,int32_t count){
  if(count<=0||!catalog_.items.count(form)||ItemCount(form)<count)return false;
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
bool Player::QueueScriptMessage(uint32_t form,float argument,bool hasArgument){
  const auto m=catalog_.pipboy.messages.find(form);
  if(m==catalog_.pipboy.messages.end()||pendingScriptMessages_.size()>=16||!scriptReference_)return false;
  ScriptMessage request;request.form=form;request.owner=scriptReference_;request.title=m->second.title;
  request.text=m->second.text;request.buttons=m->second.buttons;
  if(hasArgument){
    const auto marker=request.text.find("%.0f");
    if(marker==std::string::npos||!std::isfinite(argument))return false;
    char number[48];std::snprintf(number,sizeof(number),"%.0f",double(argument));
    request.text.replace(marker,4,number);
  }
  if(request.text.find('%')!=std::string::npos)return false; // unknown formatting cannot be guessed
  pendingScriptMessages_.push_back(std::move(request));return true;
}
bool Player::QueueScriptSound(uint32_t form){
  if(!form||pendingScriptSounds_.size()>=16)return false;
  pendingScriptSounds_.push_back(form);return true;
}
bool Player::PollScriptMessage(ScriptMessage& out){
  if(pendingScriptMessages_.empty())return false;
  out=std::move(pendingScriptMessages_.front());pendingScriptMessages_.erase(pendingScriptMessages_.begin());return true;
}
bool Player::SubmitScriptMessageButton(uint32_t owner,int index){
  if(!owner||index<0||index>31||scriptButtons_.count(owner))return false;
  scriptButtons_.emplace(owner,index);return true;
}
bool Player::PollScriptSound(uint32_t& out){
  if(pendingScriptSounds_.empty())return false;
  out=pendingScriptSounds_.front();pendingScriptSounds_.erase(pendingScriptSounds_.begin());return true;
}
void Player::PumpReferenceGameMode(double realSeconds,const std::vector<uint32_t>& resident){
  if(!std::isfinite(realSeconds)||realSeconds<=0||realSeconds>1)return;
  referenceScriptAccumulator_=std::min(.5,referenceScriptAccumulator_+realSeconds);
  if(referenceScriptAccumulator_<.25)return;
  referenceScriptAccumulator_-=.25;
  auto ids=resident;std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
  for(uint32_t id:ids){
    if(!HasReferenceEvent(id,"GameMode"))continue;
    const auto ref=catalog_.references.find(id);
    if(ref==catalog_.references.end()||(ref->second.flags&0x820u))continue;
    std::string error;
    const auto sid=catalog_.eventBaseScripts.at(ref->second.base);
    if(!RunSourceEvent(sid,"GameMode",id,0,error))LogScriptEventError(sid,"GameMode",error);
  }
}
} // namespace fo3player
