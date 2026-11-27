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
    // The editor accepts whitespace after the member separator in scoped reads.
    if(t.size()>1&&t.back()=='.'&&std::isalpha(static_cast<unsigned char>(t.front()))&&at<tokens.size()){
      const char first=tokens[at].front();
      if(std::isalpha(static_cast<unsigned char>(first))||first=='_')t+=tokens[at++];
    }
    if(t=="("){if(!Or(value)||at==tokens.size()||tokens[at++]!=")")return Fail("unbalanced expression");return true;}
    if(t=="-"||t=="+"){if(!Atom(value))return false;if(t=="-")value=-value;return true;}
    char* end=nullptr;value=std::strtof(t.c_str(),&end);
    if(end!=t.c_str()&&!*end)return std::isfinite(value)||Fail("nonfinite literal");
    const auto dot=t.find('.');
    const auto query=dot==std::string::npos?t:t.substr(dot+1);
    std::string animationGroup;
    if(query=="isanimplaying"&&at<tokens.size()&&std::isalpha(static_cast<unsigned char>(tokens[at].front())))
      animationGroup=tokens[at++];
    if(syntaxOnly){
      size_t arguments=0;
      if(query=="getequipped"||query=="gethasnote"||query=="getinworldspace"||query=="getstage"||query=="getquestrunning"||query=="isactionref"||query=="getisreference"||query=="getav"||
         query=="getpermanentactorvalue"||query=="getpcmiscstat"||query=="getisid"||
         query=="getdetected"||query=="getiscurrentpackage"||query=="iscombattarget"||query=="iskiller"||
         query=="getitemcount"||query=="getinsamecell"||query=="getincell"||query=="getdistance"||query=="getheadingangle"||query=="getdeadcount"||query=="isplayertagskill"||query=="getinfaction"||query=="getfactionrank")arguments=1;
      if(query=="getincellparam"||query=="getstagedone"||query=="getobjectivedisplayed"||query=="getobjectivecompleted")arguments=2;
      for(size_t n=0;n<arguments;++n){
        if(at==tokens.size()||tokens[at]=="("||tokens[at]==")"||tokens[at]=="&&"||tokens[at]=="||"||tokens[at]=="==")return Fail("missing expression query argument");
        ++at;
      }
      value=0;return true;
    }
    const auto& d=p.Definitions().pipboy;
    if(query=="isgoredisabled"){value=p.Definitions().goreDisabled;return true;}
    if(t=="getrandompercent"){value=forms?0:p.DrawScriptRandomPercent();return true;} // Binding preflight never draws.
    auto form=[&](const std::string& n){
      if(n=="player")return fo3player::PlayerRef;
      if(n=="getactionref")return p.ScriptActivator();
      if(n=="getself")return owner;
      if(n=="getlinkedref"){
        const auto placed=d.targets.find(owner);
        return placed!=d.targets.end()?placed->second.linkedReference:0u;
      }
      if(auto slot=DeclaredLocalKey(d,owner,scriptId,n,true)){
        if(forms){
          if(!fo3dialogue::VariableKey(d,owner,uint32_t(slot),d.quests.count(owner)!=0,forms))return uint32_t(0);
          forms->variables.back().referenceSlot=true;
        }
        const auto& refs=p.Snapshot().pipboy.referenceVariables;
        const auto stored=refs.find(slot);
        return stored==refs.end()?0u:stored->second;
      }
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
      if(at==tokens.size())return Fail("missing action reference");
      const auto expected=form(tokens[at++]);if(!expected)return Fail("unresolved action reference");
      value=p.ScriptActivator()==expected;return true;
    }
    if(t=="player.getitemcount"){
      if(at==tokens.size())return Fail("missing item form");
      const auto id=form(tokens[at++]);if(!id)return Fail("unresolved item form");
      value=p.ItemCount(id);return true;
    }
    if(t=="getdeadcount"){
      if(at==tokens.size())return Fail("missing death-count actor base");
      return fo3dialogue::DeadCount(p,form(tokens[at++]),value,error);
    }
    if(dot==std::string::npos&&(t=="player"||t=="getactionref"||t=="getself"||t=="getlinkedref")){
      value=float(form(t));return true;
    }
    if(query=="isactor"){
      const auto target=dot==std::string::npos?owner:form(t.substr(0,dot));
      value=target==fo3player::PlayerRef||p.Definitions().actorPlacements.count(target)!=0;
      return true;
    }
    if(query=="getdetected"||query=="getiscurrentpackage"||query=="getisid"||query=="iscombattarget"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(at==tokens.size())return Fail("missing actor query argument");
      const auto argument=form(tokens[at++]);if(!argument)return Fail("unknown actor query argument");
      const auto placed=p.Definitions().references.find(id);
      if(placed==p.Definitions().references.end()&&id!=fo3player::PlayerRef)return Fail("actor query reference unavailable");
      const auto actor=p.Snapshot().actors.find(id);
      if(query=="getisid"){value=(id==fo3player::PlayerRef?fo3player::PlayerBase:placed->second.base)==argument;return true;}
      if(query=="getdetected"){
        if(argument!=fo3player::PlayerRef)return Fail("detection target unavailable");
        value=p.ActorHealth(id)>0&&p.ScriptActorDetectedPlayer(id);return true;
      }
      value=actor!=p.Snapshot().actors.end()&&(query=="getiscurrentpackage"?actor->second.package:actor->second.hostile)==argument;
      return true;
    }
    if(query=="getplayerteammatecount"){value=float(p.Snapshot().playerTeammates.size());return true;}
    if(query=="getplayerteammate"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(id!=fo3player::PlayerRef&&!p.Definitions().actorPlacements.count(id))return Fail("teammate actor unavailable");
      value=p.IsPlayerTeammate(id);return true;
    }
    if(query=="getisghost"||query=="getknockedstate"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(id!=fo3player::PlayerRef&&!p.Definitions().actorPlacements.count(id))return Fail("actor flag receiver unavailable");
      value=query=="getisghost"?p.ActorGhost(id):p.ActorKnockedOut(id);return true;
    }
    if(query=="getcombattarget"||query=="getunconscious"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      const auto actor=p.Snapshot().actors.find(id);
      if(p.ActorHealth(id)<0)return Fail("actor query reference unavailable");
      if(query=="getunconscious")value=p.ActorUnconscious(id);
      else value=actor!=p.Snapshot().actors.end()?float(actor->second.hostile):0.f;
      return true;
    }
    if(query=="getequipped"){
      const auto actor=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(at>=tokens.size())return Fail("GetEquipped needs an item");
      const auto item=form(tokens[at++]);
      if(!p.Definitions().items.count(item))return Fail("equipped item unavailable");
      bool equipped=false;
      if(!p.ScriptEquippedItem(actor,item,equipped))return Fail("equipped inventory unavailable");
      value=equipped;
      return true;
    }
    if(query=="getitemcount"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(at==tokens.size())return Fail("missing item query argument");
      value=p.ScriptItemCount(id,form(tokens[at++]));return value>=0||Fail("item query unavailable");
    }
    if(query=="getisreference"){
      const auto caller=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(at==tokens.size())return Fail("missing reference identity");
      const auto expected=form(tokens[at++]);
      const auto reference=[&](uint32_t id){return id==fo3player::PlayerRef||p.Definitions().references.count(id);};
      if(!reference(caller)||!reference(expected))return Fail("unresolved reference identity");
      value=caller==expected;return true;
    }
    if(query=="getinfaction"||query=="getfactionrank"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(at==tokens.size())return Fail("missing faction query");
      const auto faction=form(tokens[at++]);
      if(!p.ActorFactions(id)||!p.Definitions().factionFlags.count(faction))return Fail("invalid faction query");
      const int rank=p.ActorFactionRank(id,faction);value=query=="getinfaction"?float(rank>=0):float(rank);return true;
    }
    if(query=="getinsamecell") {
      if(at==tokens.size())return Fail("missing GetInSameCell target");
      const auto target=form(tokens[at++]);
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));bool same=false;
      if(!p.ScriptInSameCell(id,same,error,target))return false;
      value=same;return true;
    }
    if(query=="getignorecrime"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));bool ignored=false;
      if(!p.GetIgnoreCrime(id,ignored))return Fail("crime-ignore actor unavailable");
      value=ignored;return true;
    }
    if(query=="gettalkedtopc"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));value=p.Snapshot().pipboy.talkedActors.count(id);return true;
    }
    if(query=="getxpfornextlevel"){
      const auto xp=p.ExperienceProgress();
      value=xp.level>=xp.maxLevel?0.f:float(int64_t(xp.nextThreshold)-xp.earned);
      return true;
    }
    if(query=="gethasnote"){
      if(at==tokens.size())return Fail("missing note");
      const auto note=form(tokens[at++]);
      if(!d.notes.count(note))return Fail("unknown note");
      value=p.ItemCount(note)>0;return true;
    }
    if(query=="getinworldspace"){
      if(at==tokens.size())return Fail("missing worldspace");
      const auto world=form(tokens[at++]);
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      fo3pipdata::Location location;
      if(!d.worlds.count(world)||!p.ScriptReferenceLocation(id,location))return Fail("worldspace query location unavailable");
      value=location.world==world;return true;
    }
    if(query=="getincellparam"){
      if(tokens.size()-at<2)return Fail("missing cell or reference");
      const auto cell=form(tokens[at++]);const auto id=form(tokens[at++]);
      fo3pipdata::Location location;
      if(!d.cellWorlds.count(cell)||!p.ScriptReferenceLocation(id,location))return Fail("cell query location unavailable");
      value=fo3pipdata::InCell(d,location.cell,cell);return true;
    }
    if(query=="getincell"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(at==tokens.size())return Fail("missing cell query");
      const auto cell=form(tokens[at++]);fo3pipdata::Location location;
      if(!d.cellWorlds.count(cell)||!p.ScriptReferenceLocation(id,location))return Fail("cell query location unavailable");
      value=fo3pipdata::InCell(d,location.cell,cell);return true;
    }
    if(query=="iskiller"){
      if(at==tokens.size())return Fail("missing killer reference");
      const auto victim=dot==std::string::npos?owner:form(t.substr(0,dot));
      return p.IsActorKiller(victim,form(tokens[at++]),value)||Fail("invalid death attribution actor");
    }
    if(query=="getcurrentaiprocedure"){
      const auto actor=dot==std::string::npos?owner:form(t.substr(0,dot));
      return p.ScriptActorProcedure(actor,value)||Fail("actor AI procedure unavailable");
    }
    if(query=="getopenstate"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(!p.Definitions().references.count(id))return Fail("open-state reference unavailable");
      value=p.ScriptOpenState(id);return true;
    }
    if(query=="getheadingangle"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(at==tokens.size())return Fail("missing heading target");
      const auto target=form(tokens[at++]);
      return p.ScriptHeadingAngle(id,target,value)||Fail("heading location or view unavailable");
    }
    if(query=="getdistance"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(at==tokens.size())return Fail("missing distance target");
      const auto target=form(tokens[at++]);
      fo3pipdata::Location a,b;float az=0,bz=0;
      if(!p.ScriptReferenceLocation(id,a,&az)||!p.ScriptReferenceLocation(target,b,&bz))return Fail("distance location unavailable");
      if(a.cell!=b.cell&&(!a.world||a.world!=b.world)){value=1e9f;return true;}
      value=std::hypot(a.x-b.x,a.y-b.y,az-bz);return true;
    }
    if(query=="getdead"||query=="isincombat"||query=="istalking"||query=="issneaking"||query=="getsitting"||query=="getfurnituremarkerid"||query=="getcurrentfurnitureref"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(query=="getsitting"||query=="getfurnituremarkerid"||query=="getcurrentfurnitureref"){
        uint32_t ref=0;uint8_t marker=0,state=0;if(!p.ScriptFurniture(id,ref,marker,state))return Fail("actor furniture observation unavailable");
        value=query=="getsitting"?state:query=="getfurnituremarkerid"?marker:ref;return true;
      }
      if(query=="issneaking"){if(id!=fo3player::PlayerRef)return Fail("NPC sneak posture unavailable");value=p.IsSneaking();return true;}
      if(query=="istalking"){value=p.ScriptActorTalking(id);return true;}
      if(query=="getdead"){bool dead=false;if(!p.ActorDeathState(id,dead))return Fail("actor death state unavailable");value=dead;return true;}
      if(id==fo3player::PlayerRef){
        value=std::any_of(p.Snapshot().actors.begin(),p.Snapshot().actors.end(),[](const auto& entry){
          return !entry.second.dead&&entry.second.hostile==fo3player::PlayerRef;
        });
        return true;
      }
      const auto actor=p.Snapshot().actors.find(id);if(actor==p.Snapshot().actors.end())return Fail("combat actor unavailable");
      value=actor->second.hostile!=0;return true;
    }
    if(query=="isplayertagskill"){
      if(at==tokens.size())return Fail("missing tagged actor value");
      const auto skill=fo3av::ID(tokens[at++]);
      if(skill<32||skill>45||skill==44)return Fail("invalid tagged actor value");
      value=p.IsPlayerTagSkill(skill);return true;
    }
    if(t=="getpcmiscstat"){
      if(at==tokens.size()||tokens[at++]!="\"bobbleheads found\"")return Fail("unsupported player miscellaneous statistic");
      value=float(p.Snapshot().pipboy.bobbleheadsFound);return true;
    }
    if(query=="getav"||query=="getpermanentactorvalue"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(at==tokens.size())return Fail("missing actor value");
      const auto name=tokens[at++];
      uint32_t valueId=fo3av::ID(name);
      if(name=="aggression")valueId=0;
      if(name=="assistance")valueId=57;
      if(name.size()==10&&name.substr(0,8)=="variable"&&name[8]>='0'&&name[8]<='1'&&name[9]>='0'&&name[9]<='9'){
        const unsigned slot=(name[8]-'0')*10+name[9]-'0';if(slot>=1&&slot<=10)valueId=61+slot;
      }
      return (query=="getav"?p.ActorValue(id,valueId,value):p.PermanentActorValue(id,valueId,value))||
        Fail("actor value unavailable "+name);
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
      value=query=="getlocked"?float(lock.locked?(lock.broken||lock.level>100?2:1):0):
            query=="getlocklevel"?float(lock.level):float(lock.broken);return true;
    }
    if(t=="getcurrenttime"){value=p.Snapshot().gameHour;return true;}
    if(t=="getsecondspassed"){value=float(p.ScriptSecondsPassed());return true;}
    if(query=="isidleplaying"){
      const auto actor=dot==std::string::npos?owner:form(t.substr(0,dot));
      if(!p.Definitions().actorPlacements.count(actor))return Fail("idle actor unavailable");
      value=p.ScriptIdlePlaying(actor);return true;
    }
    if(query=="isanimplaying"){
      const auto id=dot==std::string::npos?owner:form(t.substr(0,dot));bool playing=false;
      if(!p.ScriptAnimationPlaying(id,playing,animationGroup))return Fail("animation reference unavailable");
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
  bool Product(float& value){
    if(!Atom(value))return false;
    while(at<tokens.size()&&(tokens[at]=="*"||tokens[at]=="/")){
      const auto op=tokens[at++];float b;if(!Atom(b))return false;
      if(syntaxOnly){value=0;continue;}
      if(op=="/"&&b==0)return Fail("division by zero");
      value=op=="*"?value*b:value/b;
      if(!std::isfinite(value))return Fail("expression overflow");
    }
    return true;
  }
  bool Sum(float& value){if(!Product(value))return false;while(at<tokens.size()&&(tokens[at]=="+"||tokens[at]=="-")){auto op=tokens[at++];float b;if(!Product(b))return false;value=op=="+"?value+b:value-b;}return std::isfinite(value)||Fail("expression overflow");}
  bool Compare(float& value){if(!Sum(value))return false;if(at<tokens.size()){auto op=tokens[at];if(op=="=="||op=="!="||op=="<"||op==">"||op=="<="||op==">="){++at;float b;if(!Sum(b))return false;value=op=="=="?value==b:op=="!="?value!=b:op=="<"?value<b:op==">"?value>b:op=="<="?value<=b:value>=b;}}return true;}
  bool And(float& value){if(!Compare(value))return false;while(at<tokens.size()&&tokens[at]=="&&"){++at;float b;if(!Compare(b))return false;value=value!=0&&b!=0;}return true;}
  bool Or(float& value){if(!And(value))return false;while(at<tokens.size()&&tokens[at]=="||"){++at;float b;if(!And(b))return false;value=value!=0||b!=0;}return true;}
public:
  Expression(const fo3player::Player& player,uint32_t o,const std::string& text,std::string& e,uint32_t sid=0,fo3dialogue::CommandFormCapture* capture=nullptr,bool syntax=false):p(player),owner(o),scriptId(sid),error(e),forms(capture),syntaxOnly(syntax) {
    for(size_t i=0;i<text.size();) {
      if(std::isspace((unsigned char)text[i])){++i;continue;}
      const size_t start=i;const auto c=text[i];
      if(c=='"'){
        ++i;while(i<text.size()&&text[i]!='"')++i;
        if(i<text.size())++i;
      }else if(std::isalnum((unsigned char)c)||c=='_'||c=='.'){while(i<text.size()&&(std::isalnum((unsigned char)text[i])||text[i]=='_'||text[i]=='.'))++i;}
      else {++i;if(i<text.size()&&((text[i]=='='&&(c=='='||c=='!'||c=='<'||c=='>'))||(text[i]==c&&(c=='&'||c=='|'))))++i;}
      auto token=text.substr(start,i-start);for(auto& ch:token)ch=char(std::tolower((unsigned char)ch));tokens.push_back(token);
    }
  }
  bool Evaluate(float& value){
    if(Or(value)&&(at==tokens.size()||Fail("unsupported expression syntax")))return true;
    error+=" [";for(const auto& token:tokens){if(error.size()>240){error+="...";break;}error+=token+" ";}error+="]";return false;
  }
};
}
namespace fo3dialogue {
bool HasNumericExpression(const Command& c) {
  using C = CommandType;
  if (c.type == C::If || c.type == C::ElseIf || c.type == C::Variable || c.type == C::Global) return true;
  return !c.expression.empty() && (c.type == C::SetPlayerTeammate || c.type == C::SayTo || c.type == C::SetActorAIValue ||
      c.type == C::RewardXP || c.type == C::RewardKarma || c.type == C::ShowMessage ||
      c.type == C::ShowBarterMenu || c.type == C::AddItem || c.type == C::RemoveItem);
}

bool BindNumericExpression(const fo3player::Player& player, uint32_t owner, uint32_t scriptId,
    const std::string& expression, CommandFormCapture& forms, float& value, std::string& error) {
  return fo3script::Expression(player,owner,expression,error,scriptId,&forms).Evaluate(value);
}
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
    // End the previous quest before its result script displays the successor's
    // objective. That script must observe the new completion and tracking state.
    if((item.flags&3)&&!FinishQuest(id,(item.flags&2)?fo3pipdata::Completion::Failed:fo3pipdata::Completion::Complete))
      return fail("completion rejected");
    ScriptProgramOrigin origin;origin.kind=ScriptProgramKind::QuestStageItem;
    origin.record=id;origin.stage=index;origin.item=uint32_t(i);
    if(!RunScript(item.result,id,depth+1,budget,error,0,std::move(origin)))return fail("item "+std::to_string(i)+": "+error);
    // Reacquire after scripts: nested stages can rehash the quest instance map.
    if(!item.log.empty())state_.pipboy.quests.at(id).journal.push_back({index,uint32_t(i)});
  }
  return true;
}
bool Player::RunScript(const fo3pipdata::ResultScript& script,uint32_t owner,unsigned depth,unsigned& budget,std::string& error,uint32_t scriptId,ScriptProgramOrigin origin,bool* requestsTalk) {
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
  const bool authoredProgram=origin.kind!=ScriptProgramKind::UnboundResult;
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
    if(fo3dialogue::HasNumericExpression(c)){
      float v;
      if(!fo3script::Expression(*this,owner,c.expression,error,scriptId,nullptr,true).Evaluate(v))return false;
      // Capture resolvable original bindings without demanding capabilities
      // from a branch that may never run. Entered operands are resolved below,
      // against the state after preceding commands, inside the transaction.
      if(scriptTrace_){
        std::string capabilityError;
        (void)fo3dialogue::BindNumericExpression(*this,owner,scriptId,c.expression,expressionForms,v,capabilityError);
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
    if(origin.kind==ScriptProgramKind::ReferenceEvent||origin.kind==ScriptProgramKind::InventoryEvent||
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
  // Kill changes the actor immediately. Its event observes the caller's
  // completed instruction stream, including local writes following Kill.
  std::vector<std::pair<uint32_t,uint32_t>> deathEvents;
  std::vector<std::pair<uint32_t,RagdollPush>> pushes;
  const auto resolve=[&](uint64_t slot,uint32_t fallback){
    if(!slot)return fallback;
    const auto found=state_.pipboy.referenceVariables.find(slot);
    return found==state_.pipboy.referenceVariables.end()?0u:found->second;
  };
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
    if(c.type==C::Return)break;
    bool ok=true;
    switch(c.type) {
    case C::PlayerSex:ok=SetPlayerSex(c.index==2?!PlayerFemale():c.index==1);break;
    case C::PlayerScale:ok=SetPlayerScale(c.value);break;
    case C::PlayerAge:ok=AgePlayerRace(int(c.value));break;
    case C::PlayerToddler:ok=SetPlayerToddler(c.value!=0);break;
    case C::AutoObjectives:ok=SetAutoDisplayObjectives(c.value!=0,owner);break;
    case C::KillUpdates:ok=KillQuestUpdates();break;
    case C::PlayMovie:ok=PlayMovie(owner,c.expression,uint8_t(c.index));break;
    case C::AppearanceMenu:ok=OpenAppearanceMenu(owner);break;
    case C::SpecialBook:{
      unsigned total=uint32_t(c.value);
      if(c.index==2){
        for(auto attribute:state_.special)total+=attribute;
        if(total>70)total=70;
      }
      if(state_.specialMenuOwner&&state_.additionalSpecial!=(c.index==2)){ok=false;break;}
      ok=OpenSpecialBook(owner,total);
      if(ok)state_.additionalSpecial=c.index==2;
      break;
    }
    case C::PlayerTagSkill:ok=SetPlayerTagSkill(c.form,c.index);break;
    case C::TagSkills:{
      unsigned total=uint32_t(c.value);
      if(c.index==2){
        for(unsigned av=32;av<=45;++av)if(IsPlayerTagSkill(av))++total;
        if(total>4)total=4;
      }
      if(state_.tagMenuOwner&&state_.additionalTags!=(c.index==2)){ok=false;break;}
      ok=OpenTagSkills(owner,total,c.index!=0);
      if(ok)state_.additionalTags=c.index==2;
      break;
    }
    // Explicit temporary policy: retain existing race/face/default-hair assets.
    case C::DeferredAppearance:{fo3pipdata::Location location;ok=!c.form||ScriptReferenceLocation(c.form,location);break;}
    case C::PlayerNameMenu:ok=OpenPlayerNameMenu(owner);break;
    case C::CharacterGeneration:ok=SetCharacterGeneration(c.value!=0);break;
    case C::LocationLoadScreens:ok=SetLocationSpecificLoadScreens(c.value!=0);break;
    case C::PushActorAway:{
      fo3pipdata::Location from,to;
      ok=catalog_->actorPlacements.count(c.index)&&ScriptReferenceLocation(c.form,from)&&
         ScriptReferenceLocation(c.index,to)&&from.world==to.world&&(from.world||from.cell==to.cell);
      if(!ok)break;
      auto height=[&](uint32_t ref){
        if(ref==PlayerRef)return ScriptPlayerHeight();
        const auto actor=state_.actors.find(ref);
        return actor!=state_.actors.end()?actor->second.position[2]:catalog_->pipboy.targets.at(ref).z;
      };
      const auto* stats=ActorStatistics(c.index);
      if(!stats){ok=false;break;}
      const auto& rules=catalog_->rules;
      const float power=(rules.knockbackDamageBase+c.value*rules.knockbackDamageMult)*
                        (rules.knockbackAgilBase+10.f*stats->agility*rules.knockbackAgilMult);
      RagdollPush push;push.speed=power*.1f; // 7 NIF units per Havok unit, 70 per metre.
      push.originOffset={(from.x-to.x)/70.f,(height(c.form)-height(c.index))/70.f,(to.y-from.y)/70.f};
      if(!std::isfinite(push.speed)){ok=false;break;}
      pushes.emplace_back(c.index,push);break;
    }
    case C::Kill:{
      const bool alive=ActorHealth(c.form)>0;
      ok=KillActor(c.form,c.index);
      if(ok&&alive&&!ActorKnockedOut(c.form)&&HasReferenceEvent(c.form,"OnDeath"))
        deathEvents.emplace_back(c.form,c.index);
      break;
    }
    case C::MoveTo:ok=MoveActorTo(c.form,c.index,c.offset);break;
    case C::Resurrect:
      if(c.value!=0){error="animated resurrection requires an authored get-up clip";return false;}
      ok=ResurrectActor(c.form);break;
    case C::SetCombatStyle:{
      const auto style=resolve(c.targetLocal,c.index);
      ok=(!c.targetLocal||style)&&SetActorCombatStyle(resolve(c.receiverLocal,c.form),style);
      break;
    }
    case C::SetPlayerTeammate:
      value=c.value;
      if(!c.expression.empty())ok=fo3script::Expression(*this,owner,c.expression,error,scriptId).Evaluate(value);
      if(ok)ok=std::isfinite(value)&&double(value)>=INT32_MIN&&double(value)<=INT32_MAX&&
          SetPlayerTeammate(resolve(c.receiverLocal,c.form),int32_t(value)!=0);
      break;
    case C::SetGhost:ok=SetActorGhost(c.form,c.value!=0);break;
    case C::SetUnconscious:ok=SetActorUnconscious(c.form,c.value!=0);break;
    case C::ResetHealth:ok=ResetActorHealth(c.form);break;
    case C::Dispel:ok=c.form==PlayerRef&&DispelIngestible(c.index);break;
    case C::ChangeSpell:ok=c.form==PlayerRef&&SetAddiction(c.index,c.value!=0);break;
    case C::ActorValueDamage:{
      const bool canDie=c.form!=PlayerRef && c.index==16 && !c.variable && ActorHealth(c.form)>0;
      ok=ChangeActorValueDamage(c.form,c.index,c.value,c.variable!=0);
      if(ok&&canDie&&ActorHealth(c.form)==0&&!ActorKnockedOut(c.form)&&HasReferenceEvent(c.form,"OnDeath"))
        deathEvents.emplace_back(c.form,0);
      break;
    }
    case C::ModifyActorValue:ok=ModifyActorValue(c.form,c.index,int(c.value));break;
    case C::ModifyMiscStat: {
      auto& count=state_.pipboy.bobbleheadsFound;
      const auto next=uint32_t(std::clamp(int64_t(count)+int(c.value),int64_t(0),int64_t(UINT32_MAX)));
      if(next!=count){count=next;++revision_;}
      break;
    }
    case C::SetActorAIValue:
      value=c.value;
      if(!c.expression.empty())ok=fo3script::Expression(*this,owner,c.expression,error,scriptId).Evaluate(value);
      if(ok)ok=std::isfinite(value)&&std::floor(value)==value&&value>=0&&value<=fo3av::AIMax(c.index)&&SetActorAIValue(c.form==fo3dialogue::ActionReference?scriptActivator_:c.form,c.index,value);
      break;
    case C::SetAlert:ok=SetActorAlert(c.form,c.value!=0);break;
    case C::UseWeapon:{
      const auto* weapon=ActorWeapon(c.form);
      ok=weapon&&weapon->formId==c.index&&ActorHealth(c.form)>0&&ActorHealth(uint32_t(c.variable))>0&&
         catalog_->items.at(c.index).weapon.Firearm();
      if(ok){
        fo3script::Action action;action.kind=fo3script::ActionKind::UseWeapon;
        action.reference=c.form;action.form=c.index;action.flags=uint32_t(c.variable);action.info=uint32_t(c.value);
        ok=QueueScriptAction(std::move(action));
      }
      break;
    }
    case C::SetQuestDelay:ok=SetQuestScriptDelay(c.form,c.value);break;
    case C::FactionRelation:ok=SetFactionRelation(c.form,c.index,c.value!=0,(c.variable&1)!=0,(c.variable&2)!=0);break;
    case C::FactionMembership:ok=SetActorFactionRank(c.form,c.index,int(c.value),c.variable!=0);break;
    case C::SetEssential:ok=SetEssential(c.form,c.value!=0);break;
    case C::SetActorValue:ok=SetActorVariable(c.form,c.index,c.value);break;
    case C::Look:{fo3pipdata::Location location;ok=ScriptReferenceLocation(c.form,location)&&(!c.index||ScriptReferenceLocation(c.index,location));if(ok){fo3script::Action action;action.kind=fo3script::ActionKind::Look;action.reference=c.form;action.form=c.index;action.flags=c.value!=0;ok=QueueScriptAction(std::move(action));}break;}
    case C::ScriptPackage:ok=ScriptPackage(c.form==fo3dialogue::ActionReference?scriptActivator_:c.form,c.index,c.value!=0);break;
    case C::StartConversation:ok=QueueScriptSpeech(c.form,uint32_t(c.variable),c.index,true);break;
    case C::SetReference:{
      uint32_t form=c.form;
      if(c.expression=="getself")form=owner;
      else if(c.expression=="getactionref")form=scriptActivator_;
      else if(c.expression=="getlinkedref"){
        const auto ref=catalog_->pipboy.targets.find(owner);
        form=ref==catalog_->pipboy.targets.end()?0:ref->second.linkedReference;
      }
      auto& locals=state_.pipboy.referenceVariables;
      auto old=locals.find(c.variable);
      if(form){
        if(old==locals.end()&&locals.size()>=100000){ok=false;break;}
        if(old==locals.end()||old->second!=form){locals[c.variable]=form;++revision_;}
      }else if(old!=locals.end()){locals.erase(old);++revision_;}
      break;
    }
    case C::SayTo:{
      uint32_t target=c.index;
      if(c.targetLocal){
        const auto slot=state_.pipboy.referenceVariables.find(c.targetLocal);
        if(slot==state_.pipboy.referenceVariables.end()){error="SayTo listener is unset";return false;}
        target=slot->second;
      }
      value=c.noTargetLook?1.f:0.f;
      if(!c.expression.empty())ok=fo3script::Expression(*this,owner,c.expression,error,scriptId).Evaluate(value);
      if(ok)ok=QueueScriptSpeech(c.form,uint32_t(c.variable),target,false,c.value!=0,0,value!=0);
      break;
    }
    case C::Variable:case C::Global:
      ok=fo3script::Expression(*this,owner,c.expression,error,scriptId).Evaluate(value);
      if(ok)ok=c.type==C::Variable?SetDialogueVariable(c.variable,value):SetGlobalValue(c.form,value);
      break;
    case C::EnableControls:ok=SetPlayerControls(127,true);break;
    case C::PlayerControls:ok=SetPlayerControls(c.index,c.value!=0);break;
    case C::IgnoreCrime:ok=SetIgnoreCrime(c.form,c.value!=0);break;
    case C::Restrain:ok=SetRestrained(c.form,c.value!=0);break;
    case C::StartQuest:ok=StartQuest(c.form);break;
    case C::StopQuest:ok=StopQuest(c.form);break;
    case C::Stage:{
      const auto quest=catalog_->pipboy.quests.find(c.form);
      // A missing stage on a valid QUST has no result to run. Continue the
      // calling script so its combat, timer and variable updates still commit.
      if(!authoredProgram||quest==catalog_->pipboy.quests.end()||quest->second.stages.count(uint16_t(c.index)))
        ok=RunQuestStage(c.form,uint16_t(c.index),depth+1,budget,error);
      break;
    }
    case C::CompleteQuest:ok=EnsureQuestInstance(c.form)&&FinishQuest(c.form,fo3pipdata::Completion::Complete);break;
    case C::ObjectiveDisplay:case C::ObjectiveComplete: {
      auto q=state_.pipboy.quests.find(c.form);fo3pipdata::ObjectiveState existing;
      if(q!=state_.pipboy.quests.end()){auto o=q->second.objectives.find(c.index);if(o!=q->second.objectives.end())existing=o->second;}
      ok=EnsureQuestInstance(c.form)&&(!(c.type==C::ObjectiveDisplay&&c.value)||StartQuest(c.form))&&SetObjective(c.form,c.index,c.type==C::ObjectiveDisplay?bool(c.value):existing.displayed,
        c.type==C::ObjectiveComplete?(c.value?fo3pipdata::Completion::Complete:fo3pipdata::Completion::Active):existing.status);break;
    }
    case C::AddItem:case C::RemoveItem:
      value=c.value;
      if(!c.expression.empty())ok=fo3script::Expression(*this,owner,c.expression,error,scriptId).Evaluate(value);
      if(ok)ok=std::isfinite(value)&&value>=0&&value<=100000&&std::floor(value)==value;
      if(ok)ok=c.type==C::AddItem?ScriptAddItem(uint32_t(c.variable),c.form,int32_t(value),c.index!=0,c.offset[0]):ScriptRemoveItem(c.form,int32_t(value),uint32_t(c.variable));
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
    case C::RemoveAllItems:ok=ScriptRemoveAllItems(c.form,c.index,c.value!=0,c.variable!=0);break;
    case C::EquipItem:case C::UnequipItem:
      ok=c.type==C::EquipItem&&c.variable?ScriptEquipActorItem(c.index,c.form,c.value!=0):
          ScriptEquipItem(c.form,c.type==C::EquipItem,c.value!=0);break;
    case C::PipboyReset:case C::RadioRefresh:{fo3script::Action action;action.kind=c.type==C::PipboyReset?fo3script::ActionKind::PipboyReset:fo3script::ActionKind::RadioRefresh;ok=QueueScriptAction(std::move(action));break;}
    case C::TuneRadio:ok=TuneRadio(c.form);break;
    case C::RadioOff:ok=TuneRadio(0);break;
    case C::BroadcastState:case C::RadioConversation:{
      const auto& radio=catalog_->pipboy;
      const auto transmitter=radio.transmitters.find(c.form);
      if(transmitter==radio.transmitters.end()||!radio.stations.count(transmitter->second.base)){ok=false;break;}
      if(c.type==C::BroadcastState){
        const bool changed=c.value!=0?state_.pipboy.silentRadios.erase(c.form)!=0:state_.pipboy.silentRadios.insert(c.form).second;
        if(changed)++revision_;
      }else{
        const auto topic=radio.dialogueTopics.find(c.index?c.index:radio.radioHello);
        if(topic==radio.dialogueTopics.end()||topic->second.type!=7){ok=false;break;}
        fo3script::Action action;action.kind=fo3script::ActionKind::RadioConversation;
        action.reference=c.form;action.form=c.index;ok=QueueScriptAction(std::move(action));
      }
      break;
    }
    case C::AutoSave:{fo3script::Action action;action.kind=fo3script::ActionKind::AutoSave;ok=QueueScriptAction(std::move(action));break;}
    case C::DoorState:ok=ScriptDoorState(c.form,c.value!=0);break;
    case C::TerminalBack:ok=QueueTerminalBack(c.form);break;
    case C::LastExteriorDoor:{
      const auto placement=catalog_->pipboy.targets.find(c.form);
      fo3pipdata::Location location;
      ok=placement!=catalog_->pipboy.targets.end()&&catalog_->pipboy.doorBases.count(placement->second.base)&&
        ScriptReferenceLocation(c.form,location)&&TrackExteriorLocation(location);
      break;
    }
    case C::ShowMap:ok=RevealMapMarker(c.form,c.value!=0);break;
    case C::FastTravel:ok=SetFastTravelEnabled(c.value!=0);break;
    case C::SetQuestObject:ok=SetQuestObject(c.form,c.value!=0);break;
    case C::Lock:ok=ScriptLock(c.form,int(c.value));break;
    case C::Unlock:ok=ScriptUnlock(c.form);break;
    case C::RewardKarma:case C::RewardXP:
      value=c.value;
      if(!c.expression.empty())ok=fo3script::Expression(*this,owner,c.expression,error,scriptId).Evaluate(value);
      if(ok&&c.type==C::RewardKarma)ok=RewardKarma(value);
      else if(ok)ok=std::isfinite(value)&&value>=0&&double(value)<=2147483647.0&&std::floor(value)==value&&RewardExperience(uint32_t(value));
      break;
    case C::AddPerk:{
      const auto perk=catalog_->pipboy.perks.find(c.form);
      if(perk==catalog_->pipboy.perks.end()||!perk->second.ranks){ok=false;break;}
      const auto owned=state_.pipboy.perks.find(c.form);
      const unsigned rank=owned==state_.pipboy.perks.end()?0:owned->second;
      if(rank<perk->second.ranks)ok=GrantPerk(c.form,uint8_t(rank+1));
      break;
    }

    case C::CompleteAllObjectives:ok=CompleteAllObjectives(c.form);break;
    case C::SetCellFullName:ok=SetCellFullName(c.form,c.index);break;
    case C::AddAchievement:ok=AddAchievement(uint32_t(c.value));break;
    case C::Disable:ok=StageReferenceEnable(c.form,false,error);break;
    case C::Enable:ok=StageReferenceEnable(c.form,true,error);break;
    case C::TalkingActivator:ok=SetTalkingActivatorActor(c.form,c.index);break;
    case C::StartCombat:{
      bool callerDisabled=true,targetDisabled=false;
      ok=c.form!=c.index&&catalog_->actorPlacements.count(c.form)&&
         (c.index==PlayerRef||catalog_->actorPlacements.count(c.index))&&
         ActorHealth(c.form)>0&&ActorHealth(c.index)>0&&
         ReferenceDisabled(c.form,callerDisabled,error)&&!callerDisabled&&
         (c.index==PlayerRef||(ReferenceDisabled(c.index,targetDisabled,error)&&!targetDisabled))&&
         SetActorHostile(c.form,c.index);
      break;
    }
    case C::StopCombat:{
      const auto reference=c.form==fo3dialogue::ActionReference?scriptActivator_:c.form;
      const auto actor=state_.actors.find(reference);
      if(c.variable&&actor!=state_.actors.end()&&actor->second.hostile&&actor->second.hostile!=c.variable)break;
      ok=StopActorCombat(reference,c.index!=0);break;
    }
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
    case C::DefaultActivate:{
      const auto receiver=resolve(c.receiverLocal,c.form);
      const auto activator=resolve(c.targetLocal,c.index?c.index:scriptActivator_);
      if(!receiver||(c.targetLocal&&!activator)){error="Activate reference is unset";return false;}
      if(requestsTalk&&!c.variable&&receiver==owner&&activator==scriptActivator_){
        *requestsTalk=true;break;
      }
      if(!c.variable){ok=QueueScriptDefaultActivate(receiver,activator);break;}
      if(scriptActivationDepth_>=8){error="nested activation depth exceeded";return false;}
      if(activator!=PlayerRef&&!catalog_->actorPlacements.count(activator)){error="activation actor unavailable";return false;}
      bool disabled=true;if(!ReferenceDisabled(receiver,disabled,error)||disabled){ok=false;break;}
      if(!HasReferenceEvent(receiver,"OnActivate"))break; // No event is a native no-op with flag 1.
      struct ActivationScope {unsigned& depth;ActivationScope(unsigned& d):depth(d){++depth;}~ActivationScope(){--depth;}} scope(scriptActivationDepth_);
      ok=RunSourceEventDirect(catalog_->eventBaseScripts.at(catalog_->references.at(receiver).base),"OnActivate",receiver,activator,error);break;
    }
    case C::PlayIdle:{
      fo3script::Action action;action.kind=fo3script::ActionKind::PlayIdle;action.reference=c.form;action.form=c.index;
      ok=catalog_->actorPlacements.count(c.form)&&QueueScriptAction(std::move(action));break;
    }
    case C::PlayMagicShader:case C::StopMagicShader:{
      if((c.index!=PlayerRef&&!catalog_->references.count(c.index))||!catalog_->pipboy.effectShaders.count(c.form)){
        error="unknown magic shader target or effect";return false;
      }
      const auto key=std::make_pair(c.index,c.form);
      if(c.type==C::StopMagicShader){if(state_.magicShaders.erase(key))++revision_;}
      else{
        if(state_.magicShaders.size()>=1024&&!state_.magicShaders.count(key)){error="too many active magic shaders";return false;}
        state_.magicShaders[key]={0,c.value,uint32_t(revision_)^c.index^(c.form*2654435761u)};++revision_;
      }
      break;
    }
    case C::PlayGroup:ok=QueueScriptPlayGroup(c.form,c.expression,c.index);break;
    case C::NoActivationSound:ok=SetNoActivationSound(c.form);break;
    case C::ScreenBlood:{fo3script::Action action;action.kind=fo3script::ActionKind::ScreenBlood;action.flags=uint32_t(c.value);ok=QueueScriptAction(std::move(action));break;}
    case C::RemoveImageModifier:{fo3script::Action action;action.kind=fo3script::ActionKind::RemoveImageModifier;action.form=c.form;ok=QueueScriptAction(std::move(action));break;}
    case C::ImageModifier:{fo3script::Action action;action.kind=fo3script::ActionKind::ImageModifier;action.reference=owner;action.form=c.form;ok=QueueScriptAction(std::move(action));break;}
    case C::PlaySound:ok=QueueScriptSound(c.form);break;
    case C::PlaceExplosion:{
      for(unsigned i=0;ok&&i<unsigned(c.value);++i)ok=QueueExplosion(c.index,c.form,&error);
      if(!ok&&error.empty())error="PlaceAtMe requires a placed explosion with supported payload";
      break;
    }
    case C::DamageObject:ok=DamageDestructible(c.form,c.value);break;
    case C::PlaySound3D:{
      fo3pipdata::Location location;
      ok=catalog_->pipboy.sounds.count(c.form)&&ScriptReferenceLocation(c.index,location);
      if(ok){
        fo3script::Action action;action.kind=fo3script::ActionKind::SoundAt;
        action.reference=c.index;action.form=c.form;ok=QueueScriptAction(std::move(action));
      }
      break;
    }
    case C::AddTopic:if(!catalog_->pipboy.dialogueTopics.count(c.form))ok=false;else if(state_.pipboy.knownTopics.insert(c.form).second)++revision_;break;
    default:ok=false;break;
    }
    if(!ok){if(error.empty())error="script command rejected by canonical Player API (type="+std::to_string(int(c.type))+", form="+std::to_string(c.form)+", value="+std::to_string(c.value)+")";return false;}
  }
  for(const auto& [ref,push]:pushes){
    const auto actor=state_.actors.find(ref);
    if(actor==state_.actors.end()||!actor->second.dead){
      error="live PushActorAway needs knockdown and get-up";return false;
    }
    auto& pending=state_.ragdollPushes[ref];
    if(pending.size()>=64){error="too many pending ragdoll pushes";return false;}
    pending.push_back(push);++revision_;
  }
  for(const auto& [victim,killer]:deathEvents){
    if(scriptActivationDepth_>=8){error="nested death event depth exceeded";return false;}
    ++scriptActivationDepth_;
    const auto script=catalog_->eventBaseScripts.at(catalog_->references.at(victim).base);
    const bool handled=RunSourceEventDirect(script,"OnDeath",victim,killer,error);
    --scriptActivationDepth_;
    if(!handled)return false;
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
                            uint32_t activator,std::string& error,uint32_t filterForm) {
  auto plan=PrepareScriptTransaction([=](Player& candidate,std::string& e){
    return candidate.RunSourceEventDirect(scriptId,event,owner,activator,e,filterForm);
  },error);
  if(!plan)return false;
  if(plan->HasReferenceChanges()){error="world mutation requires resident coordinator";return false;}
  if(CommitScriptTransaction(*plan))return true;
  error="event transaction changed before publication";return false;
}
bool Player::RunSourceEventDirect(uint32_t scriptId,const char* event,uint32_t owner,
                            uint32_t activator,std::string& error,uint32_t filterForm,bool* requestsTalk,uint32_t inventoryBase,uint64_t inventoryStack) {
  error.clear();
  const auto def=catalog_->eventPrograms.find(scriptId);
  if(def==catalog_->eventPrograms.end()) {error="script definition unavailable";return false;}
  const auto& program=def->second;
  if(!program.error.empty()) {error=program.error;return false;}
  if(program.compiledOnly) {error="compiled-only event not supported";return false;}
  const auto name=fo3script::Lower(event);
  if(name=="onactivate"){
    const auto placed=catalog_->references.find(owner);
    if(placed!=catalog_->references.end()&&catalog_->items.count(placed->second.base)&&
       !CanPickupPlacedItem(owner,true)){error="placed item unavailable for activation";return false;}
  }
  const auto previousActivator=scriptActivator_,previousReference=scriptReference_;
  scriptActivator_=activator;scriptReference_=owner;
  struct RestoreContext {Player& p;uint32_t activator,reference;
    ~RestoreContext(){p.scriptActivator_=activator;p.scriptReference_=reference;}} context{*this,previousActivator,previousReference};
  unsigned budget=4096;
  for(const auto& block:program.blocks){
    if(block.event!=name)continue;
    // Event matching identity can be a weapon/package/topic, independently
    // of GetActionRef (the actor responsible for the event).
    const uint32_t match=filterForm?filterForm:activator;
    const auto filter=catalog_->pipboy.formNames.find(block.filter);
    if(block.filter=="player"&&match!=PlayerRef&&(filterForm||catalog_->references.count(match)))continue;
    if(!block.filter.empty()&&block.filter!="player"&&filter!=catalog_->pipboy.formNames.end()&&filter->second!=match)continue;
    if(!fo3script::EventFilterMatches(block.filter,match,PlayerRef) && !(filter!=catalog_->pipboy.formNames.end()&&filter->second==match)){
      error="unresolved event filter '"+block.filter+"' at line "+std::to_string(block.sourceLine);
      return false;
    }
    fo3pipdata::ResultScript body;body.source=block.body;
    ScriptProgramOrigin origin;origin.kind=inventoryBase?ScriptProgramKind::InventoryEvent:ScriptProgramKind::ReferenceEvent;
    origin.inventoryBase=inventoryBase;origin.inventoryStack=inventoryStack;
    origin.record=scriptId;origin.event=block.event;origin.sourceLine=block.sourceLine;origin.filterForm=filterForm;
    if(!RunScript(body,owner,0,budget,error,scriptId,std::move(origin),requestsTalk)){
      error="line "+std::to_string(block.sourceLine)+": "+error;
      return false;
    }
  }
  if(name=="gamemode")scriptButtons_.erase(owner);
  return true;
}
ScriptEventResult Player::DispatchReferenceEvent(uint32_t reference,const char* event,
                                                  uint32_t activator,std::string& error,uint32_t filterForm) {
  error.clear();
  if(!HasReferenceEvent(reference,event))return ScriptEventResult::NoHandler;
  const auto& ref=catalog_->references.at(reference);
  const auto scriptId=catalog_->eventBaseScripts.at(ref.base);
  if(!RunSourceEvent(scriptId,event,reference,activator,error,filterForm)){
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
void Player::SetScriptPlayerLocation(float x,float y,float z,uint32_t cell,uint32_t world,float triggerRadius,const std::array<float,3>* actorOrigin){
  if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(triggerRadius)||triggerRadius<0)return;
  // Trigger volumes use the body centre, while reference queries use the
  // KF accumulation root of a seated actor, independently of tracked head pose.
  if(actorOrigin&&!std::all_of(actorOrigin->begin(),actorOrigin->end(),[](float v){return std::isfinite(v);}))return;
  scriptPlayerPosition_={x,y,z};scriptPlayerActorOrigin_=actorOrigin?*actorOrigin:scriptPlayerPosition_;scriptPlayerTriggerRadius_=triggerRadius;scriptPlayerCell_=cell;scriptPlayerWorld_=world;scriptPlayerPositionKnown_=true;TouchMerchantCells(cell);
}
void Player::SetScriptPlayerHeading(float yaw){
  scriptPlayerHeadingKnown_=std::isfinite(yaw);
  scriptPlayerHeading_=scriptPlayerHeadingKnown_?std::remainder(yaw,6.283185307179586f):0;
}
void Player::ClearScriptActorFurniture(){if(!scriptFurniture_.empty()){scriptFurniture_.clear();++scriptFurnitureRevision_;}}
void Player::SetScriptActorFurniture(uint32_t actor,uint32_t ref,uint8_t marker,uint8_t state){
  if(!catalog_->actorPlacements.count(actor)||actor==PlayerRef)return;
  const auto placed=catalog_->pipboy.targets.find(ref);
  if(state>4||(ref&&(placed==catalog_->pipboy.targets.end()||!catalog_->pipboy.furniture.count(placed->second.base)||marker>=30))||(!ref&&(state||marker))){
    if(scriptFurniture_.erase(actor))++scriptFurnitureRevision_;
    return;
  }
  const std::array<uint32_t,3> observed{{ref,marker,state}};
  const auto old=scriptFurniture_.find(actor);if(old==scriptFurniture_.end()||old->second!=observed){scriptFurniture_[actor]=observed;++scriptFurnitureRevision_;}
}
bool Player::ScriptFurniture(uint32_t actor,uint32_t& ref,uint8_t& marker,uint8_t& state)const{
  if(actor==PlayerRef){ref=furnitureRef_;marker=furnitureMarker_;state=sittingState_;return true;}
  const auto observed=scriptFurniture_.find(actor);if(observed==scriptFurniture_.end())return false;
  ref=observed->second[0];marker=uint8_t(observed->second[1]);state=uint8_t(observed->second[2]);return true;
}
bool Player::ScriptHeadingAngle(uint32_t reference,uint32_t target,float& degrees)const{
  fo3pipdata::Location from,to;
  if(!ScriptReferenceLocation(reference,from)||!ScriptReferenceLocation(target,to))return false;
  if(from.cell!=to.cell&&(!from.world||from.world!=to.world))return false;
  if(reference!=PlayerRef&&!catalog_->actorPlacements.count(reference)){degrees=0;return true;}
  if(reference==PlayerRef&&!scriptPlayerHeadingKnown_)return false;
  const float yaw=reference==PlayerRef?scriptPlayerHeading_:from.facing;
  const float dx=to.x-from.x,dy=to.y-from.y;
  if(!std::isfinite(yaw)||!std::isfinite(dx)||!std::isfinite(dy))return false;
  if(dx==0&&dy==0){degrees=0;return true;}
  // Scene yaw turns north toward west; GECK heading offsets are clockwise.
  const double angle=std::atan2(double(dx),double(dy))+yaw;
  degrees=float(std::remainder(angle,6.2831853071795864769)*57.295779513082320877);
  return true;
}
bool Player::ScriptDistance(float& distance) const {
  const auto found=catalog_->pipboy.targets.find(scriptReference_);
  if(!scriptPlayerPositionKnown_||found==catalog_->pipboy.targets.end())return false;
  const auto& ref=found->second;
  if(ref.cell!=scriptPlayerCell_ && (!ref.world||ref.world!=scriptPlayerWorld_))return false;
  const double dx=double(ref.x)-scriptPlayerActorOrigin_[0],dy=double(ref.y)-scriptPlayerActorOrigin_[1],dz=double(ref.z)-scriptPlayerActorOrigin_[2];
  const double d=std::sqrt(dx*dx+dy*dy+dz*dz);
  if(!std::isfinite(d)||d>std::numeric_limits<float>::max())return false;
  distance=float(d);return true;
}
int Player::ScriptButton() const {
  const auto i=scriptButtons_.find(scriptReference_);
  return i==scriptButtons_.end()?-1:i->second;
}
float Player::DrawScriptRandomPercent()const{
  auto next=state_.scriptRandom?state_.scriptRandom:scriptRandomInitial_;
  next^=next>>12;next^=next<<25;next^=next>>27;state_.scriptRandom=next;++revision_;
  return float((next*2685821657736338717ull)%100);
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
bool Player::StopActorCombat(uint32_t reference,bool targetingActor){
  if(targetingActor){
    if(reference!=PlayerRef&&!catalog_->actorPlacements.count(reference))return false;
    std::vector<uint32_t> attackers;for(const auto& actor:state_.actors)if(actor.second.hostile==reference)attackers.push_back(actor.first);
    for(auto actor:attackers)if(!SetActorHostile(actor,0)||!ReevaluatePackage(actor))return false;
    fo3script::Action action;action.kind=fo3script::ActionKind::StopCombat;action.reference=reference;action.flags=1;
    return QueueScriptAction(std::move(action));
  }
  const auto placed=catalog_->actorPlacements.find(reference);
  // Ordinary StopCombat acts on this actor rather than its attackers.
  if(placed==catalog_->actorPlacements.end()||!catalog_->weapons.actors.count(placed->second.base))return false;
  if(!SetActorHostile(reference,0)||!ReevaluatePackage(reference))return false;
  fo3script::Action action;action.kind=fo3script::ActionKind::StopCombat;action.reference=reference;
  return QueueScriptAction(std::move(action));
}
bool Player::ReevaluatePackage(uint32_t reference){
  const auto p=catalog_->actorPlacements.find(reference);
  if(reference!=PlayerRef&&(p==catalog_->actorPlacements.end()||!catalog_->weapons.actors.count(p->second.base)))return false;
  // Q240's existing PackageRevision cache is invalidated by this revision.
  // The actual authored PACK selection happens in Q240SelectPackage.
  ++revision_;return true;
}
int32_t Player::ItemCount(uint32_t form) const {
  int64_t n=0;for(const auto& stack:state_.inventory)if(stack.formId==form)n+=stack.count;
  return int32_t(std::min<int64_t>(INT32_MAX,n));
}
bool Player::ScriptRemoveItem(uint32_t form,int32_t count,uint32_t receiver){
  if(count<0||!catalog_->items.count(form))return false;
  std::vector<Stack>* inventory=&state_.inventory;
  if(receiver!=PlayerRef){
    const auto ref=catalog_->references.find(receiver);if(ref==catalog_->references.end())return false;
    if(!PrepareInventory(receiver,catalog_->actorInventories.count(ref->second.base)!=0))return false;
    inventory=&state_.containers.at(receiver);
  }
  int64_t available=0;for(const auto& stack:*inventory)if(stack.formId==form)available+=stack.count;
  count=int32_t(std::min<int64_t>(count,available));
  if(!count)return true;
  // Native scripted consumption can remove equipped and quest items, unlike Drop.
  // Consume loose copies first so a partial removal preserves the equipped copy.
  for(bool equipped:{false,true})for(auto it=inventory->begin();it!=inventory->end()&&count;){
    if(it->formId!=form||it->equipped!=equipped){++it;continue;}
    const int32_t removed=std::min(count,it->count);count-=removed;it->count-=removed;
    if(!it->count){
      if(receiver!=PlayerRef)if(auto* actor=state_.actors.Edit(receiver))if(actor->equippedWeapon==it->id){
        actor->equippedWeapon=0;actor->weaponLocked=false;
      }
      it=inventory->erase(it);
    }else ++it;
  }
  ++revision_;return true;
}
bool Player::ScriptRemoveAllItems(uint32_t source,uint32_t destination,bool keepOwnership,bool silent){
  // The enclosing script transaction owns the staged inventories, including
  // any leveled contents first realized by this command.
  auto prepare=[&](uint32_t ref){
    if(ref==PlayerRef)return true;
    const auto placed=catalog_->references.find(ref);
    return placed!=catalog_->references.end()&&PrepareInventory(ref,catalog_->actorInventories.count(placed->second.base)!=0);
  };
  if(!prepare(source)||(destination&&!prepare(destination)))return false;
  if(source==destination)return true;
  auto inventory=[&](uint32_t ref)->std::vector<Stack>& {
    return ref==PlayerRef?state_.inventory:state_.containers.at(ref);
  };
  auto& from=inventory(source);
  const auto movable=[&](const Stack& stack){
    const auto& item=catalog_->items.at(stack.formId);
    return !(source==PlayerRef&&IsQuestObject(stack.formId))&&
           !(item.kind==ItemKind::Armour&&!item.playable&&item.bipedMask);
  };
  if(source==PlayerRef){
    std::vector<std::pair<uint64_t,uint32_t>> unequipped;
    for(auto& stack:from)if(stack.equipped&&movable(stack)&&catalog_->eventBaseScripts.count(stack.formId)){
      stack.equipped=false;
      unequipped.emplace_back(stack.id,stack.formId);
    }
    for(const auto& item:unequipped)if(!DispatchEquipmentEvent(item.first,item.second,"OnUnequip"))return false;
  }
  const auto count=static_cast<size_t>(std::count_if(from.begin(),from.end(),movable));
  if(!count)return true;
  if(destination){
    auto& to=inventory(destination);
    if(count>10000-to.size())return false;
    if(source==PlayerRef&&destination!=PlayerRef){
      size_t total=count;
      for(const auto& contents:state_.containers)total+=contents.second.size();
      if(total>30000)return false;
    }
    to.reserve(to.size()+count);
  }
  auto* actor=state_.actors.Edit(source);
  for(const auto& stack:from){
    if(!movable(stack))continue;
    if(actor&&actor->equippedWeapon==stack.id){actor->equippedWeapon=0;actor->weaponLocked=false;}
    if(!destination)continue;
    auto transferred=stack;
    transferred.equipped=false;transferred.equipmentLocked=false;
    if(!keepOwnership)transferred.ownership={};
    inventory(destination).push_back(transferred);
    if(destination==PlayerRef&&!silent&&pendingScriptItems_.size()<128){
      pendingScriptItems_.push_back({stack.formId,stack.count});++scriptEffectRevision_;
    }
  }
  from.erase(std::remove_if(from.begin(),from.end(),movable),from.end());
  ++revision_;
  return true;
}
bool Player::DispatchEquipmentEvent(uint64_t stack,uint32_t form,const char* event){
  const auto script=catalog_->eventBaseScripts.find(form);
  if(script==catalog_->eventBaseScripts.end())return true;
  const auto def=catalog_->eventPrograms.find(script->second);
  if(def==catalog_->eventPrograms.end())return true;
  const auto name=fo3script::Lower(event);
  if(std::none_of(def->second.blocks.begin(),def->second.blocks.end(),[&](const auto& b){return b.event==name;}))return true;
  std::string error;
  if(inventoryEventCandidate_){
    if(RunEquipmentEvent(stack,script->second,event,error))return true;
  }else{
    auto plan=PrepareScriptTransaction([&](Player& candidate,std::string& e){
      return candidate.RunEquipmentEvent(stack,script->second,event,e);
    },error,bool(inventoryScriptPublisher_));
    if(plan&&!plan->HasReferenceChanges()&&CommitScriptTransaction(*plan))return true;
    if(plan&&plan->HasReferenceChanges()){
      if(inventoryScriptPublisher_&&inventoryScriptPublisher_(std::move(plan)))return true;
      error="inventory script world publication unavailable";
    }
  }
  LogScriptEventError(script->second,event,error);
  // Like combat events, an unsupported handler cannot reverse a user's
  // physical action. An enclosing script can still roll its transaction back.
  return !inventoryEventCandidate_;
}
bool Player::RunEquipmentEvent(uint64_t stack,uint32_t script,const char* event,std::string& error){
  if(inventoryEventDepth_>=8){error="equipment event nesting exceeded";return false;}
  auto findStack=[&]() -> Stack* {
    for(auto& s:state_.inventory)if(s.id==stack)return &s;
    for(auto& container:state_.containers)for(auto& s:container.second)if(s.id==stack)return &s;
    for(auto& weapon:state_.worldWeapons)if(weapon.instance.id==stack)return &weapon.instance;
    return nullptr;
  };
  auto* current=findStack();if(!current){error="inventory instance no longer present";return false;}
  auto instance=std::make_shared<ItemScriptInstance>();
  if(current->scriptInstance)*instance=*current->scriptInstance;
  else{
    // FF is the engine's dynamic-reference namespace. Resolve collisions
    // independently of save stack ID width, including held and stored copies.
    std::unordered_set<uint32_t> used;
    auto remember=[&](const Stack& s){if(s.scriptInstance)used.insert(s.scriptInstance->reference);};
    for(const auto& s:state_.inventory)remember(s);
    for(const auto& c:state_.containers)for(const auto& s:c.second)remember(s);
    for(const auto& w:state_.worldWeapons)remember(w.instance);
    uint32_t slot=uint32_t(stack&0x00ffffffu);
    while(!slot||used.count(0xff000000u|slot)||catalog_->references.count(0xff000000u|slot))slot=(slot+1)&0x00ffffffu;
    instance->reference=0xff000000u|slot;
  }
  if(scriptReference_==instance->reference){error="recursive inventory event";return false;}
  current->scriptInstance=instance;
  const uint64_t owner=uint64_t(instance->reference)<<32;
  for(const auto& v:instance->numbers)state_.pipboy.dialogueVariables[owner|v.first]=v.second;
  for(const auto& v:instance->references)state_.pipboy.referenceVariables[owner|v.first]=v.second;
  ++inventoryEventDepth_;
  const bool ok=RunSourceEventDirect(script,event,instance->reference,PlayerRef,error,0,nullptr,current->formId,stack);
  --inventoryEventDepth_;
  // The interpreter uses reference-and-slot keys. Move this instance's
  // locals back into its stack before publishing or serializing player state.
  for(const auto& slot:catalog_->pipboy.scripts.at(script).variables){
    const uint64_t key=owner|slot.first;
    auto n=state_.pipboy.dialogueVariables.find(key);
    if(n!=state_.pipboy.dialogueVariables.end()){instance->numbers[slot.first]=n->second;state_.pipboy.dialogueVariables.erase(n);}
    else instance->numbers.erase(slot.first);
    auto ref=state_.pipboy.referenceVariables.find(key);
    if(ref!=state_.pipboy.referenceVariables.end()){instance->references[slot.first]=ref->second;state_.pipboy.referenceVariables.erase(ref);}
    else instance->references.erase(slot.first);
  }
  if(auto* saved=findStack())saved->scriptInstance=std::move(instance);
  return ok;
}
bool Player::ScriptEquipItem(uint32_t form,bool equip,bool lock){
  const auto item=catalog_->items.find(form);
  if(item==catalog_->items.end()||!(item->second.kind==ItemKind::Weapon||(item->second.kind==ItemKind::Armour&&item->second.bipedMask)))return false;
  if(!equip){
    std::vector<uint64_t> removed;
    for(auto& stack:state_.inventory)if(stack.formId==form&&(stack.equipped||stack.equipmentLocked!=lock)){
      if(stack.equipped)removed.push_back(stack.id);
      stack.equipped=false;stack.equipmentLocked=lock;++revision_;
    }
    for(auto id:removed)if(!DispatchEquipmentEvent(id,form,"OnUnequip"))return false;
    return true;
  }
  uint64_t selected=0;float condition=-1;
  for(const auto& stack:state_.inventory)if(stack.formId==form){
    if(stack.equipped){selected=stack.id;break;}
    if(stack.condition>condition){selected=stack.id;condition=stack.condition;}
  }
  if(!selected||!Equip(selected,true))return false;
  for(auto& stack:state_.inventory)if(stack.id==selected&&stack.equipmentLocked!=lock){
    stack.equipmentLocked=lock;++revision_;break;
  }
  return true;
}
bool Player::MakeScriptMessage(uint32_t form,float argument,bool hasArgument,uint32_t owner,ScriptMessage& request)const{
  const auto m=catalog_->pipboy.messages.find(form);
  if(m==catalog_->pipboy.messages.end()||!owner||!std::isfinite(argument))return false;
  request={};request.form=form;request.owner=owner;request.title=m->second.title;
  request.argument=argument;request.hasArgument=hasArgument;
  request.text=m->second.text;request.buttons=m->second.buttons;
  if(!(m->second.displayFlags&1u)&&request.buttons.empty())request.notificationSeconds=m->second.displaySeconds;
  if(hasArgument){
    const auto marker=request.text.find("%.0f");
    if(marker==std::string::npos||!std::isfinite(argument))return false;
    char number[48];std::snprintf(number,sizeof(number),"%.0f",double(argument));
    request.text.replace(marker,4,number);
  }
  for(size_t i=0;(i=request.text.find('%',i))!=std::string::npos;++i){
    if(i+1==request.text.size()||request.text[i+1]!='%')return false;
    request.text.erase(i,1); // Authored literal percent escape.
  }
  return true;
}
bool Player::QueueScriptMessage(uint32_t form,float argument,bool hasArgument,uint32_t owner){
  ScriptMessage request;
  if(pendingScriptMessages_.size()>=16||!MakeScriptMessage(form,argument,hasArgument,scriptReference_?scriptReference_:owner,request))return false;
  if(!request.buttons.empty()){
    if(scriptPrompts_.size()>=16)return false;
    scriptPrompts_.push_back(request);
  }
  pendingScriptMessages_.push_back(std::move(request));++scriptEffectRevision_;return true;
}
bool Player::ScriptIdlePlaying(uint32_t actor) const {
  return scriptPlayingIdles_.count(actor)||std::any_of(pendingScriptActions_.begin(),pendingScriptActions_.end(),[&](const auto& action){
    return (action.kind==fo3script::ActionKind::PlayIdle||action.kind==fo3script::ActionKind::PackageIdle)&&action.reference==actor;
  });
}
void Player::SetScriptRuntimeClock(double now,double elapsed){
  if(std::isfinite(now)&&std::isfinite(elapsed)&&elapsed>=0){scriptRuntimeSeconds_=now;scriptElapsedSeconds_=elapsed;}
}
void Player::SetScriptAnimationRegistry(std::shared_ptr<const fo3script::AnimationRegistry> registry){
  scriptAnimationDefinitions_=std::move(registry);++scriptEffectRevision_;
}
bool Player::ScriptAnimationPlaying(uint32_t reference,bool& playing,const std::string& group) const {
  if(!scriptAnimationDefinitions_||!scriptAnimationDefinitions_->count(reference))return false;
  const auto i=scriptAnimations_.find(reference);
  playing=i!=scriptAnimations_.end()&&(group.empty()||i->second.group==fo3script::Lower(group))&&
          fo3script::Playing(i->second,scriptRuntimeSeconds_);return true;
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
void Player::FinishScriptAnimation(uint32_t reference,const std::string& group){
  const auto playing=scriptAnimations_.find(reference);
  if(playing==scriptAnimations_.end()||playing->second.group!=fo3script::Lower(group))return;
  scriptAnimations_.erase(playing);++scriptEffectRevision_;
}
bool Player::QueueScriptDefaultActivate(uint32_t reference,uint32_t activator){
  const auto placed=catalog_->references.find(reference);
  if(placed==catalog_->references.end()||!placed->second.valid)return false;
  const auto base=placed->second.base;
  if(catalog_->parentActivateOnly.count(reference)&&!IsActivationParent(reference,activator))return false;
  auto parents=[&]{if(!catalog_->activationChildren.count(reference))return true;
    fo3script::Action action;action.kind=fo3script::ActionKind::ActivationParents;
    action.reference=reference;return QueueScriptAction(std::move(action));};
  const auto speaker=catalog_->pipboy.dialogueActors.find(base);
  if(speaker!=catalog_->pipboy.dialogueActors.end()&&speaker->second.talkingActivator){
    bool disabled=true;std::string error;
    if(activator!=PlayerRef||!ReferenceDisabled(reference,disabled,error)||disabled)return false;
    for(const auto topic:catalog_->pipboy.greetings){
      const auto before=pendingScriptActions_.size();
      if(!QueueScriptSpeech(reference,topic,activator,false,false))return false;
      if(pendingScriptActions_.size()!=before)break;
    }
    return parents();
  }
  if(catalog_->activatorBases.count(base)&&scriptAnimationDefinitions_){
    const auto animation=scriptAnimationDefinitions_->find(reference);
    if(animation!=scriptAnimationDefinitions_->end()&&!fo3script::OpenStateGroup(animation->second,true).empty()){
      bool disabled=false;std::string error;
      const int state=ScriptOpenState(reference);
      return ReferenceDisabled(reference,disabled,error)&&!disabled&&
             (state==2||state==4||ScriptDoorState(reference,state==3))&&parents();
    }
  }
  if(catalog_->activatorBases.count(base)){
    if(catalog_->activationChildren.count(reference))return parents();
    // A script can delegate Activate on its own plain ACTI. With no
    // native action to run, the script resumes after that instruction.
    if(reference==scriptReference_)return true;
  }
  if((catalog_->pipboy.furniture.count(base)||catalog_->pipboy.terminals.count(base))&&activator==PlayerRef){
    bool disabled=true;std::string error;
    if(!ReferenceDisabled(reference,disabled,error)||disabled)return false;
    fo3script::Action action;action.kind=fo3script::ActionKind::DefaultActivate;action.reference=reference;action.flags=activator;
    return QueueScriptAction(std::move(action));
  }
  // A control panel may delegate a linked door's native action. Door
  // access and the original activator are checked here and again at delivery.
  if(reference!=scriptReference_&&!catalog_->doorBases.count(base))return false;
  if(placed!=catalog_->references.end()&&catalog_->items.count(placed->second.base)){
    // Commit inventory, world removal, and the rest of OnActivate together.
    if(activator!=PlayerRef||pendingScriptItems_.size()>=128||!PickupPlacedItem(reference,true))return false;
    pendingScriptItems_.push_back({placed->second.base,placed->second.count});++scriptEffectRevision_;
    return true;
  }
  if(!CanScriptActivateDoor(reference,activator)){
    if(activator!=PlayerRef||!CanScriptActivateContainer(reference)||!PrepareInventory(reference,false))return false;
  }
  fo3script::Action action;action.kind=fo3script::ActionKind::DefaultActivate;
  action.reference=reference;action.flags=activator;
  return QueueScriptAction(std::move(action));
}
bool Player::QueueWeaponHit(uint32_t reference,uint32_t attacker,uint32_t weapon){
  const auto ref=catalog_->references.find(reference),source=catalog_->references.find(attacker);
  const auto item=catalog_->items.find(weapon);bool disabled=true;std::string error;
  if(ref==catalog_->references.end()||!ref->second.valid||catalog_->actorPlacements.count(reference)||
     (attacker!=PlayerRef&&(source==catalog_->references.end()||!catalog_->actorPlacements.count(attacker)))||
     item==catalog_->items.end()||!item->second.weapon.valid||!ReferenceDisabled(reference,disabled,error)||disabled)return false;
  if(!HasReferenceEvent(reference,"OnHit")&&!HasReferenceEvent(reference,"OnHitWith"))return true;
  fo3script::Action action;action.kind=fo3script::ActionKind::WeaponHit;action.reference=reference;action.form=weapon;action.flags=attacker;
  return QueueScriptAction(std::move(action));
}
const DestructionStage* Player::ObjectDamageStage(uint32_t ref,bool requireModel) const {
  const auto damage=state_.objectDamage.find(ref);
  if(damage==state_.objectDamage.end())return nullptr;
  const auto placed=catalog_->references.find(ref);
  if(placed==catalog_->references.end())return nullptr;
  const auto definition=catalog_->destructibles.find(placed->second.base);
  if(definition==catalog_->destructibles.end())return nullptr;
  const auto& d=definition->second;
  for(auto it=d.stages.rbegin();it!=d.stages.rend();++it)
    if((!requireModel||!it->model.empty())&&double(damage->second)*100>=double(d.health)*(100-it->healthPercent))return &*it;
  return nullptr;
}
bool Player::DamageDestructible(uint32_t ref,float amount) {
  const auto placed=catalog_->references.find(ref);
  if(placed==catalog_->references.end()||!std::isfinite(amount)||amount<0)return false;
  const auto definition=catalog_->destructibles.find(placed->second.base);
  if(definition==catalog_->destructibles.end())return false;
  const auto& d=definition->second;
  const auto previous=state_.objectDamage.find(ref);
  const float old=previous==state_.objectDamage.end()?0.f:previous->second;
  const auto* before=ObjectDamageStage(ref);
  if(amount==0||old>=d.health||(before&&(before->flags&4)))return true;
  float next=std::min(float(d.health),old+amount);
  const auto* previousModel=ObjectDamageStage(ref,true);
  // One impact enters the stage at its resulting health. An earlier capped
  // stage stops the impact there; skipped intermediate stages do not play.
  const DestructionStage* entered=nullptr;
  for(const auto& stage:d.stages){
    const float threshold=float(double(d.health)*(100-stage.healthPercent)/100.);
    if(threshold>next)break;
    if(threshold<=old&&previous!=state_.objectDamage.end())continue;
    entered=&stage;
    if(stage.flags&1){next=threshold;break;}
  }
  if(entered){
    if(entered->selfDamage||entered->debris)return false;
    if(entered->explosion&&!QueueExplosion(ref,entered->explosion))return false;
    if(entered->flags&2){std::string error;if(!StageReferenceEnable(ref,false,error))return false;}
  }
  state_.objectDamage[ref]=next;++revision_;
  const auto* currentModel=ObjectDamageStage(ref,true);
  if(currentModel&&(!previousModel||previousModel->model!=currentModel->model)){
    fo3script::Action action;action.kind=fo3script::ActionKind::DestructionModel;action.reference=ref;
    if(!QueueScriptAction(std::move(action)))return false;
    ++state_.damageModelRevision;
  }
  return true;
}
bool Player::ApplyPlacedExplosionHealthHit(uint32_t source,uint32_t explosion,uint32_t target,
                                         bool lineOfSight,float& applied){
  applied=0;
  const auto definition=catalog_->explosions.find(explosion);
  if(definition==catalog_->explosions.end())return false;
  const auto& blast=definition->second;
  fo3pipdata::Location from,to;
  float fromZ=0,toZ=0;
  if(!ScriptReferenceLocation(source,from,&fromZ)||!ScriptReferenceLocation(target,to,&toZ))return false;
  if(from.world!=to.world||(!from.world&&from.cell!=to.cell))return true;
  if(!lineOfSight&&!(blast.flags&0x10))return true;
  if(target!=PlayerRef){
    bool disabled=false;std::string error;
    if(!catalog_->actorPlacements.count(target)||!ReferenceDisabled(target,disabled,error))return false;
    if(disabled||ActorGhost(target))return true;
  }
  return ApplyPlacedHealthAtDistance({explosion,target,std::hypot(from.x-to.x,from.y-to.y,fromZ-toZ)},applied);
}
bool Player::ApplyPlacedHealthAtDistance(const PlacedBlastHit& hit,float& applied){
  applied=0;
  const auto target=hit.target;
  const auto& blast=catalog_->explosions.at(hit.explosion);
  const float health=ActorHealth(target);
  if(health<0)return false;
  const float damage=blast.PlacedHitDamage(hit.distance,catalog_->weapons)*(1-DamageResistance(target)/100.f);
  if(!std::isfinite(damage)||damage<0)return false;
  if(!health||!damage)return true;
  if(!ChangeActorValueDamage(target,16,damage,false))return false;
  applied=std::min(health,damage);
  return true;
}
bool Player::StartPlacedEffectsDirect(uint32_t enchantment,uint32_t target,std::string& reason){
  const auto& data=catalog_->pipboy;
  const auto found=data.enchantments.find(enchantment);
  if(found==data.enchantments.end()||found->second.effects.empty()||ActorHealth(target)<0){
    reason="missing enchantment or actor";return false;
  }
  for(size_t i=0;i<found->second.effects.size();++i){
    const auto& effect=found->second.effects[i];
    const auto m=data.magic.find(effect.id);
    if(m==data.magic.end()||effect.range!=1||effect.area||!effect.duration||effect.duration>1000000||
       !effect.conditions.empty()||state_.placedEffects.size()>=1024){
      reason="unsupported placed enchantment effect";return false;
    }
    const auto& magic=m->second;
    float magnitude=float(effect.magnitude);
    if(magic.archetype==fo3pipdata::MagicScript){
      const auto program=catalog_->eventPrograms.find(magic.script);
      if(program==catalog_->eventPrograms.end()){reason="missing enchantment script";return false;}
      for(const auto& block:program->second.blocks)if(block.event=="scripteffectupdate"){
        reason="continuous magic effect script unsupported";return false;
      }
      if(!RunSourceEventDirect(magic.script,"ScriptEffectStart",target,0,reason))return false;
    }else if(magic.archetype==fo3pipdata::MagicValue&&magic.av==16&&!(magic.flags&fo3pipdata::MagicRecover)){
      if((magic.flags&fo3pipdata::MagicDetrimental)&&magic.resistance!=UINT32_MAX&&ActorHealth(target)>0){
        float resistance=0;
        if(!ActorValue(target,magic.resistance,resistance)){reason="actor magic resistance unavailable";return false;}
        magnitude*=1-std::clamp(resistance,0.f,100.f)/100;
      }
    }else{reason="unsupported placed magic archetype";return false;}
    state_.placedEffects.push_back({target,enchantment,uint32_t(i),float(effect.duration),magnitude});
    ++revision_;
  }
  return true;
}

bool Player::BeginPlacedEnchantment(uint32_t enchantment,uint32_t target,std::string& error){
  auto plan=PrepareScriptTransaction([&](Player& p,std::string& reason){
    return p.StartPlacedEffectsDirect(enchantment,target,reason);
  },error);
  if(!plan)return false;
  if(plan->HasReferenceChanges()){error="enchantment needs a resident world transaction";return false;}
  return CommitScriptTransaction(*plan);
}
bool Player::ResolvePlacedBlastHit(const PlacedBlastHit& hit,std::string& error,bool deliverDeath){
  const auto& blast=catalog_->explosions.at(hit.explosion);
  const auto target=hit.target;
  const float health=ActorHealth(target);
  float applied=0;
  if(!ApplyPlacedHealthAtDistance(hit,applied)){error="blast health transition unavailable";return false;}
  if(blast.enchantment&&!StartPlacedEffectsDirect(blast.enchantment,target,error))return false;
  if(deliverDeath&&health>0&&ActorHealth(target)==0&&!ActorKnockedOut(target)&&HasReferenceEvent(target,"OnDeath")){
    const auto script=catalog_->eventBaseScripts.at(catalog_->references.at(target).base);
    if(!RunSourceEventDirect(script,"OnDeath",target,0,error))return false;
  }
  return true;
}

bool Player::AdvancePlacedEffects(double seconds,std::string& error){
  if(state_.placedBlastHits.empty()&&state_.placedEffects.empty()&&state_.magicShaders.empty())return true;
  if(!std::isfinite(seconds)||seconds<0){error="invalid effect clock";return false;}
  if(!seconds)return true;
  const auto advanceShaders=[&](auto& shaders){
    for(auto it=shaders.begin();it!=shaders.end();){
      auto& playback=it->second;
      playback.elapsed=float(std::min(double(std::numeric_limits<float>::max()),double(playback.elapsed)+seconds));
      if(playback.duration>=0&&playback.elapsed>=playback.duration)it=shaders.erase(it);
      else ++it;
    }
  };
  // Most frames on a dead actor only move clocks. Copy the script state
  // when a callback, health mutation or impact actually needs a transaction.
  const auto needsDelivery=[&](const PlacedEffect& entry){
    if(entry.remaining<=seconds)return true;
    const auto& effect=catalog_->pipboy.enchantments.at(entry.enchantment).effects.at(entry.index);
    return catalog_->pipboy.magic.at(effect.id).archetype==fo3pipdata::MagicValue&&ActorHealth(entry.target)>0;
  };
  if(state_.placedBlastHits.empty()&&std::none_of(state_.placedEffects.begin(),state_.placedEffects.end(),needsDelivery)){
    advanceShaders(state_.magicShaders);
    for(auto& entry:state_.placedEffects)entry.remaining=float(double(entry.remaining)-seconds);
    ++revision_;return true;
  }
  auto plan=PrepareScriptTransaction([&](Player& p,std::string& reason){
    advanceShaders(p.state_.magicShaders);
    auto& active=p.state_.placedEffects;
    for(auto it=active.begin();it!=active.end();){
      const auto& effect=catalog_->pipboy.enchantments.at(it->enchantment).effects.at(it->index);
      const auto& magic=catalog_->pipboy.magic.at(effect.id);
      const float dt=float(std::min(seconds,double(it->remaining)));
      const auto target=it->target;
      if(magic.archetype==fo3pipdata::MagicValue){
        const float before=p.ActorHealth(target);
        if(before>0&&it->magnitude>0&&!p.ChangeActorValueDamage(target,magic.av,it->magnitude*dt,!(magic.flags&fo3pipdata::MagicDetrimental))){
          reason="active enchantment health transition unavailable";return false;
        }
        if(before>0&&p.ActorHealth(target)==0&&!p.ActorKnockedOut(target)&&p.HasReferenceEvent(target,"OnDeath")){
          const auto script=catalog_->eventBaseScripts.at(catalog_->references.at(target).base);
          if(!p.RunSourceEventDirect(script,"OnDeath",target,0,reason))return false;
        }
      }
      it->remaining-=dt;
      if(it->remaining>0){++it;continue;}
      if(magic.archetype==fo3pipdata::MagicScript&&!p.RunSourceEventDirect(magic.script,"ScriptEffectFinish",target,0,reason))return false;
      it=active.erase(it);
    }
    // Resolve new hits after advancing existing effects. A newly started
    // enchantment gets its whole duration from this impact.
    auto hits=std::move(p.state_.placedBlastHits);
    p.state_.placedBlastHits.clear();
    for(const auto& hit:hits)if(!p.ResolvePlacedBlastHit(hit,reason,true))return false;
    ++p.revision_;return true;
  },error);
  if(!plan)return false;
  if(plan->HasReferenceChanges()){error="effect completion needs a resident world transaction";return false;}
  return CommitScriptTransaction(*plan);
}
bool Player::QueueExplosion(uint32_t reference,uint32_t form,std::string* error){
  const auto definition=catalog_->explosions.find(form);
  if(definition==catalog_->explosions.end())return false;
  const auto& e=definition->second;
  if(e.placedObject)return false;
  fo3pipdata::Location where;float z=0;
  if(!ScriptReferenceLocation(reference,where,&z)||!where.cell)return false;
  fo3script::Action action;action.kind=fo3script::ActionKind::Explosion;
  action.reference=reference;action.form=form;action.cell=where.cell;action.world=where.world;
  action.position={where.x,where.y,z};
  std::vector<PlacedBlastHit> hits;
  if(e.damage||e.enchantment){
    // Without an observed visibility query only the authored Ignore LOS
    // path is determinate. A placed EXPL has a non-actor magic caster.
    if(!(e.flags&0x10)||!scriptPlayerPositionKnown_||e.radius<=0){
      if(error)*error="damaging explosion requires resident positions and line of sight";
      return false;
    }
    const auto collect=[&](uint32_t id){
      fo3pipdata::Location loc;float height=0;
      if(!ScriptReferenceLocation(id,loc,&height)){
        const auto actor=catalog_->actorPlacements.find(id);
        if(actor==catalog_->actorPlacements.end())return false;
        const auto& placed=actor->second;
        loc={placed.cell,placed.world,placed.x,placed.y,-placed.rz};height=placed.z;
      }
      if(loc.world!=where.world||(!loc.world&&loc.cell!=where.cell))return true;
      const float distance=std::hypot(loc.x-where.x,loc.y-where.y,height-z);
      if(!std::isfinite(distance))return false;
      if(distance>=e.radius)return true;
      if(id!=PlayerRef){
        bool disabled=false;std::string why;
        if(!ReferenceDisabled(id,disabled,why))return false;
        if(disabled||ActorGhost(id))return true;
      }
      const float health=ActorHealth(id);
      if(health<0)return false;
      if(!health)return true;
      if(HasReferenceEvent(id,"OnHit")||HasReferenceEvent(id,"OnHitWith")){
        if(error)*error="unsupported explosion hit callback on "+std::to_string(id);
        return false;
      }
      const float damage=e.PlacedHitDamage(distance,catalog_->weapons)*(1-DamageResistance(id)/100.f);
      if((e.flags&12)&&(damage<health||Essential(id))){
        if(error)*error="unsupported living blast knockdown on "+std::to_string(id);
        return false;
      }
      hits.push_back({form,id,distance});
      return true;
    };
    if(!collect(PlayerRef))return false;
    for(const auto& entry:catalog_->actorPlacements)if(entry.first!=PlayerRef&&!collect(entry.first))return false;
    if(state_.placedBlastHits.size()+hits.size()>1024)return false;
    std::sort(hits.begin(),hits.end(),[](const auto& a,const auto& b){return a.target<b.target;});
    // Probe on an isolated candidate so an unsupported enchantment cannot
    // publish the placement and then leave an undeliverable impact in a save.
    Player probe(*this,ScriptCandidateTag{});std::string reason;
    for(const auto& hit:hits)if(!probe.ResolvePlacedBlastHit(hit,reason,false)){
      if(error)*error=reason;
      return false;
    }
  }
  if(!QueueScriptAction(std::move(action)))return false;
  state_.placedBlastHits.insert(state_.placedBlastHits.end(),hits.begin(),hits.end());
  return true;
}
bool Player::QueueScriptAction(fo3script::Action action){
  if(pendingScriptActions_.size()>=64)return false;
  if(action.kind==fo3script::ActionKind::ImageModifier&&(!catalog_->imageModifiers.count(action.form)||!catalog_->imageModifiers.at(action.form).valid))return false;
  if(action.kind==fo3script::ActionKind::DefaultActivate||action.kind==fo3script::ActionKind::ActivationParents)
    action.activationDepth=uint8_t(std::min(scriptActivationDepth_,8u));
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
    const bool inside=resident&&ReferenceDisabled(entry.first,disabled,error)&&!disabled&&fo3script::Contains(p,entry.second,scriptPlayerPosition_,scriptPlayerTriggerRadius_);
    if(inside!=bool(scriptInsideTriggers_.count(entry.first)))changes.push_back({entry.first,inside});
  }
  std::sort(changes.begin(),changes.end(),[](const auto& a,const auto& b){return a.first<b.first;});return changes;
}
std::vector<uint32_t> Player::ScriptOccupiedTriggers() const {
  std::vector<uint32_t> active;
  if(!scriptPlayerPositionKnown_)return active;
  for(uint32_t reference:scriptInsideTriggers_){
    const auto target=catalog_->pipboy.targets.find(reference);const auto shape=catalog_->pipboy.triggerPrimitives.find(reference);
    if(target==catalog_->pipboy.targets.end()||shape==catalog_->pipboy.triggerPrimitives.end())continue;
    const auto& p=target->second;bool disabled=true;std::string error;
    if((scriptPlayerWorld_?p.world==scriptPlayerWorld_:p.cell==scriptPlayerCell_)&&ReferenceDisabled(reference,disabled,error)&&!disabled&&fo3script::Contains(p,shape->second,scriptPlayerPosition_,scriptPlayerTriggerRadius_))active.push_back(reference);
  }
  std::sort(active.begin(),active.end());return active;
}
std::vector<std::pair<uint32_t,bool>> Player::ScriptActorTriggerChanges(
    uint32_t actor,uint32_t cell,uint32_t world,const std::array<float,3>& position,float yaw)const {
  const auto& data=catalog_->pipboy;
  const auto placement=data.targets.find(actor);
  const auto base=placement==data.targets.end()?data.dialogueActors.end():data.dialogueActors.find(placement->second.base);
  std::vector<std::pair<uint32_t,bool>> changes;
  const auto previous=scriptActorInsideTriggers_.find(actor);
  auto check=[&](uint32_t reference){
    const auto placed=catalog_->pipboy.targets.find(reference);
    const auto shape=catalog_->pipboy.triggerPrimitives.find(reference);
    if(placed==catalog_->pipboy.targets.end()||shape==catalog_->pipboy.triggerPrimitives.end())return;
    const auto& p=placed->second;
    const bool wasInside=previous!=scriptActorInsideTriggers_.end()&&previous->second.count(reference);
    bool disabled=true;std::string error;
    const bool inside=(world?p.world==world:(!p.world&&p.cell==cell))&&
        (base==data.dialogueActors.end()?fo3script::Contains(p,shape->second,position):
         fo3script::OverlapsActor(p,shape->second,position,yaw,placement->second.scale,base->second.objectBounds))&&ReferenceDisabled(reference,disabled,error)&&!disabled;
    if(inside!=wasInside)changes.emplace_back(reference,inside);
  };
  const auto named=catalog_->actorTriggerVolumes.find(actor);
  if(named!=catalog_->actorTriggerVolumes.end())for(auto ref:named->second)check(ref);
  const auto local=catalog_->unfilteredActorTriggersBySpace.find(world?world:cell);
  if(local!=catalog_->unfilteredActorTriggersBySpace.end())for(auto ref:local->second)check(ref);
  // An actor moving into another cell may have left an unfiltered volume
  // which is no longer in its destination's list.
  if(previous!=scriptActorInsideTriggers_.end())for(auto ref:previous->second)check(ref);
  std::sort(changes.begin(),changes.end());
  changes.erase(std::unique(changes.begin(),changes.end()),changes.end());
  return changes;
}
void Player::AcknowledgeScriptTrigger(uint32_t reference,bool inside,uint32_t actor){
  if(actor==PlayerRef){
    if(inside)scriptInsideTriggers_.insert(reference);else scriptInsideTriggers_.erase(reference);
  }else{
    if(inside){scriptActorInsideTriggers_[actor].insert(reference);return;}
    const auto previous=scriptActorInsideTriggers_.find(actor);
    if(previous!=scriptActorInsideTriggers_.end()){
      previous->second.erase(reference);
      if(previous->second.empty())scriptActorInsideTriggers_.erase(previous);
    }
  }
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
  const auto prompt=std::find_if(scriptPrompts_.begin(),scriptPrompts_.end(),[&](const auto& message){return message.owner==owner;});
  if(prompt!=scriptPrompts_.end()){
    if(size_t(index)>=prompt->buttons.size())return false;
    scriptPrompts_.erase(prompt);
  }
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
void Player::AppendResidentGameModeReferences(std::vector<uint32_t>& refs)const {
  // Invisible trigger volumes can own timers after the player leaves them.
  // Use the physical scene window, independent of drawables and occupancy.
  if(!scriptPlayerPositionKnown_)return;
  const auto triggers=catalog_->gameModeTriggersBySpace.find(scriptPlayerWorld_?scriptPlayerWorld_:scriptPlayerCell_);
  const auto world=catalog_->pipboy.worlds.find(scriptPlayerWorld_);
  const bool small=world!=catalog_->pipboy.worlds.end()&&(world->second.flags&1);
  const auto inScene=[&](uint32_t cell,uint32_t wrld,const std::array<float,3>& position){
    return fo3npc::Resident(scriptPlayerCell_,scriptPlayerWorld_,small,scriptPlayerPosition_[0],
                            scriptPlayerPosition_[1],cell,wrld,position);
  };
  if(triggers!=catalog_->gameModeTriggersBySpace.end())for(uint32_t ref:triggers->second){
    const auto& p=catalog_->pipboy.targets.at(ref);
    if(inScene(p.cell,p.world,{p.x,p.y,p.z}))refs.push_back(ref);
  }
  // A newly enabled actor's local script shares residency with its arrival
  // worker. Loading its rig or textures cannot hold the encounter's clock.
  for(uint32_t ref:catalog_->gameModeActors){
    const auto saved=state_.actors.find(ref);
    if(saved==state_.actors.end())continue;
    const auto& actor=saved->second;
    if((!actor.dead||actor.corpse||catalog_->AuthoredCorpse(ref))&&inScene(actor.cell,actor.world,actor.position))
      refs.push_back(ref);
  }
}
bool Player::PumpWorldAwareGameMode(double seconds,const std::vector<uint32_t>& resident,
    const std::function<bool(std::unique_ptr<ScriptTransactionPlan>)>& reserveWorld,bool menuMode) {
  if(!std::isfinite(seconds)||seconds<=0||seconds>1||!reserveWorld)return false;
  if(state_.appearanceMenuOwner||state_.tagMenuOwner||state_.specialMenuOwner||state_.nameMenuOwner||state_.movie.owner)menuMode=true;
  // Reference scripts retain their bounded cadence. Quest scripts use their
  // original DATA/SetQuestDelay interval, with at most one run per frame.
  if(!menuMode)questScriptAccumulator_=std::min(.5,questScriptAccumulator_+seconds);
  const bool referencesDue=!menuMode&&questScriptAccumulator_>=.25;
  const double referenceElapsed=questScriptAccumulator_;
  if(referencesDue)questScriptAccumulator_=0;
  if(!menuMode)questScriptClock_+=seconds;
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
      for(const auto& block:events->second.blocks)if(block.event=="gamemode"||block.event=="menumode"){
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
    const auto& blocks=catalog_->eventPrograms.at(q->second.script).blocks;
    const char* event=menuMode?"MenuMode":"GameMode";
    if(std::none_of(blocks.begin(),blocks.end(),[&](const auto& b){return b.event==fo3script::Lower(event);}))continue;
    if(menuMode){scriptElapsedSeconds_=0;if(run(q->second.script,event,id,false))return true;continue;}
    auto tick=questScriptLastTicks_.emplace(id,questScriptClock_-seconds).first;
    const double elapsed=questScriptClock_-tick->second;
    const float interval=QuestScriptDelay(id);
    if(!std::isfinite(interval)||interval<0){LogScriptEventError(q->second.script,"GameMode","invalid quest processing interval");continue;}
    if(elapsed+1e-7<interval)continue;
    tick->second=questScriptClock_;scriptElapsedSeconds_=elapsed;
    if(run(q->second.script,"GameMode",id,false))return true;
    if(state_.appearanceMenuOwner||state_.tagMenuOwner||state_.specialMenuOwner||state_.nameMenuOwner||state_.movie.owner)return false;
  }
  if(menuMode||!referencesDue)return false;
  scriptElapsedSeconds_=referenceElapsed;
  auto refs=resident;
  std::sort(refs.begin(),refs.end());
  refs.erase(std::unique(refs.begin(),refs.end()),refs.end());
  for(const auto id:refs){
    if(!HasReferenceEvent(id,"GameMode"))continue;
    const auto ref=catalog_->references.find(id);
    if(ref==catalog_->references.end()||(ref->second.flags&0x20u))continue;
    bool disabled=false;std::string error;
    if(!ReferenceDisabled(id,disabled,error)||disabled)continue;
    const auto script=catalog_->eventBaseScripts.at(ref->second.base);
    if(run(script,"GameMode",id,true))return true;
    if(state_.appearanceMenuOwner||state_.tagMenuOwner||state_.specialMenuOwner||state_.nameMenuOwner||state_.movie.owner)return false;
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
bool Player::ScriptReferenceLocation(uint32_t id,fo3pipdata::Location& out,float* height)const{
  if(id==PlayerRef){if(!scriptPlayerPositionKnown_)return false;if(height)*height=scriptPlayerActorOrigin_[2];out={scriptPlayerCell_,scriptPlayerWorld_,scriptPlayerActorOrigin_[0],scriptPlayerActorOrigin_[1],0};return true;}
  const auto actor=state_.actors.find(id);
  if(actor!=state_.actors.end()){const auto& a=actor->second;if(height)*height=a.position[2];out={a.cell,a.world,a.position[0],a.position[1],a.yaw};return true;}
  const auto placed=catalog_->pipboy.targets.find(id);if(placed==catalog_->pipboy.targets.end())return false;
  const auto& p=placed->second;if(height)*height=p.z;out={p.cell,p.world,p.x,p.y,-p.rz};return true;
}
bool Player::ScriptPackage(uint32_t reference,uint32_t package,bool add){
  const auto placed=catalog_->actorPlacements.find(reference);
  if((reference!=PlayerRef&&(placed==catalog_->actorPlacements.end()||!catalog_->weapons.actors.count(placed->second.base)))||(add&&!catalog_->pipboy.packages.count(package)))return false;
  if(reference==PlayerRef&&add){
    const auto& definition=catalog_->pipboy.packages.at(package);
    if(definition.type!=6||(!definition.procedureIdles[0]&&definition.idleAnimations.empty())||definition.scripted)return false;
  }
  if(!add&&!state_.scriptPackages.count(reference))return true;
  if(add&&!state_.scriptPackages.count(reference)&&state_.scriptPackages.size()>=10000)return false;
  if(add&&state_.scriptPackages.count(reference)){const auto& list=state_.scriptPackages.at(reference);if(list.size()>=128&&std::find(list.begin(),list.end(),package)==list.end())return false;}
  auto& packages=state_.scriptPackages[reference];
  const auto old=packages;
  if(!add&&!package){state_.scriptPackages.erase(reference);++revision_;++scriptEffectRevision_;return ReevaluatePackage(reference);}
  packages.erase(std::remove(packages.begin(),packages.end(),package),packages.end());
  if(add)packages.insert(packages.begin(),package);
  if(packages==old)return true;
  if(packages.empty())state_.scriptPackages.erase(reference);
  ++revision_;++scriptEffectRevision_;return ReevaluatePackage(reference);
}
}

namespace fo3player {
bool Player::QueueScriptSpeech(uint32_t reference,uint32_t topic,uint32_t target,bool conversation,bool forceSubtitle,uint32_t package,bool noTargetLook){
  if(package){
    if(conversation&&target==PlayerRef)return false; // Interactive player package direction is separate.
    const auto definition=catalog_->pipboy.packages.find(package);
    if(definition==catalog_->pipboy.packages.end()||!definition->second.dialogueValid||definition->second.dialogueType!=(conversation?0:1)||
       (definition->second.dialogueTopic&&definition->second.dialogueTopic!=topic)||definition->second.target.value!=target)return false;
  }
  static thread_local unsigned selecting=0;
  if(selecting>=8)return false;
  struct Guard{unsigned& depth;Guard(unsigned& d):depth(d){++depth;}~Guard(){--depth;}} guard(selecting);
  // Say selects an INFO; its results execute later in the voice adapter.
  // The enclosing captured SCPT must not attempt a nested detached publication.
  // Preflight against an isolated, read-only snapshot of this exact command state.
  Player selection(*this,ScriptCandidateTag{});
  fo3dialogue::Context context;context.player=&selection;context.talking=true;
  if(!fo3dialogue::ReferenceContext(*this,reference,context.speaker)||!fo3dialogue::ReferenceContext(*this,target,context.target))return false;
  if(package){context.speaker.package=package;context.speaker.packageKnown=true;}
  static thread_local std::mt19937 random{std::random_device{}()};
  // This only binds the authored INFO identity in the script transaction.
  // Scene mutation capability is admitted again by the actual voice adapter.
  const auto preview=[](const auto&,const auto&,std::string&){return true;};
  bool unsupported=false;
  const auto diagnostic=[&](const std::string& message){unsupported=true;if(eventDiagnostic_)eventDiagnostic_(message);};
  const auto selected=fo3dialogue::ResolveTopicInfo(topic,context,[&](const auto& info){return fo3dialogue::EligibleInfo(info,context,diagnostic,preview);},&random);
  // A legitimate empty selection is a no-op. An unsupported preflight must
  // roll back the caller too, rather than saving its waiting flags without speech.
  if(!selected)return !package&&!unsupported;
  fo3script::Action action;action.kind=conversation?fo3script::ActionKind::Conversation:fo3script::ActionKind::SayTo;action.reference=reference;action.form=topic;action.flags=target;action.info=selected->id;action.forceSubtitle=forceSubtitle;action.package=package;
  action.sourceActor=context.speaker.reference;action.targetActor=context.target.reference;
  action.noTargetLook=noTargetLook;
  return QueueScriptAction(std::move(action));
}
}
