#pragma once
#include "dialogue/fo3-dialogue-results.h"
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace fo3script {
struct StageOpcode {const char* name; const char* arguments; unsigned required; bool query;};
inline StageOpcode StageOperation(unsigned opcode){
  switch(opcode){
#include "fo3-stage-opcodes.inc"
    default:return {nullptr,nullptr,0,false};
  }
}

// Each operand reader is confined to its own line or embedded-call payload.
struct StageReader {
  const uint8_t* at;
  const uint8_t* end;
  size_t left() const{return size_t(end-at);}
  bool take(unsigned count,uint32_t& out){
    if(count>4||count>left())return false;
    out=0;
    for(unsigned i=0;i<count;++i)out|=uint32_t(*at++)<<(8*i);
    return true;
  }
};

class StageDecoder {
  const fo3pipdata::Definitions& defs;
  const fo3pipdata::ResultScript& script;
  uint32_t owner;
  std::unordered_map<uint32_t,std::string> names;
  bool slot(StageReader& r,uint32_t& id) const {
    uint32_t n;
    if(!r.take(2,n)||!n||n>script.references.size())return false;
    id=script.references[n-1];return id!=0;
  }
  bool name(uint32_t id,std::string& out){
    if(id==0x14){out="player";return true;}
    auto cached=names.find(id);
    if(cached!=names.end()){out=cached->second;return true;}
    for(const auto& entry:defs.formNames)if(entry.second==id){
      if(entry.first.empty()||entry.first.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos)continue;
      names.emplace(id,entry.first);out=entry.first;return true;
    }
    return false;
  }
  bool variable(StageReader& r,uint32_t scope,std::string& out){
    uint32_t index;
    if(!r.take(2,index))return false;
    if(!fo3dialogue::VariableKey(defs,scope,index,defs.quests.count(scope)))return false;
    uint32_t sid=0;
    auto quest=defs.quests.find(scope);
    if(quest!=defs.quests.end())sid=quest->second.script;
    else {
      auto object=defs.objectReferenceScripts.find(scope);
      if(object!=defs.objectReferenceScripts.end())sid=object->second.first;
      else {
        auto ref=defs.referenceScripts.find(scope);
        if(ref==defs.referenceScripts.end())return false;
        auto actor=defs.dialogueActors.find(ref->second.second);
        if(actor!=defs.dialogueActors.end())sid=actor->second.script;
      }
    }
    auto sc=defs.scripts.find(sid);
    if(sc==defs.scripts.end())return false;
    auto v=sc->second.variables.find(index);
    if(v==sc->second.variables.end()||!name(scope,out))return false;
    const auto local=Lower(v->second);
    if(local.empty()||local.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos)return false;
    out+="."+local;return true;
  }
  bool target(StageReader& r,std::string& out){
    uint32_t scope=owner,tag;
    if(!r.take(1,tag))return false;
    if(tag=='G')return slot(r,scope)&&name(scope,out);
    if(tag=='r'&&(!slot(r,scope)||!r.take(1,tag)))return false;
    return (tag=='s'||tag=='f')&&variable(r,scope,out);
  }
  bool call(unsigned op,StageReader& r,std::string& out){
    auto spec=StageOperation(op);
    if(!spec.name)return false;
    out+=spec.name;
    uint32_t count=0;
    if(r.left()&&!r.take(2,count))return false;
    if(count<spec.required||count>std::strlen(spec.arguments))return false;
    for(unsigned i=0;i<count;++i){
      uint32_t tag,v;
      std::string arg;
      if(spec.arguments[i]=='s'){
        if(!r.take(2,v)||v>r.left())return false;
        arg.assign(reinterpret_cast<const char*>(r.at),v);r.at+=v;
        if(arg.find_first_of("\"\r\n;\\")!=std::string::npos||arg.find('\0')!=std::string::npos)return false;
        arg='"'+arg+'"';
      }else if(spec.arguments[i]=='g'){
        if(!r.take(2,v)||(v!=3&&v!=4))return false;
        arg=v==3?"forward":"backward";
      }else if(spec.arguments[i]=='a'){
        static const char* const skills[]={"barter","bigguns","energyweapons","explosives","lockpick","medicine","meleeweapons","repair","science","smallguns","sneak","speech","throwing","unarmed"};
        if(!r.take(2,v))return false;
        static const char* const limbs[]={"perceptioncondition","endurancecondition","leftattackcondition","rightattackcondition","leftmobilitycondition","rightmobilitycondition"};
        if(v>=25&&v<=30)arg=limbs[v-25];
        else if(v>=32&&v<=45)arg=skills[v-32];
        else if(v==0)arg="aggression";else if(v==1)arg="confidence";else if(v==16)arg="health";else if(v==54)arg="radiationrads";else if(v==57)arg="assistance";else return false;
      }else{
        if(spec.arguments[i]=='n'&&r.left()&&(*r.at=='r'||*r.at=='s'||*r.at=='f'||*r.at=='G')){
          if(!target(r,arg))return false;
          out+=" "+arg;continue;
        }
        if(!r.take(1,tag))return false;
        if(spec.arguments[i]=='r'&&tag=='r'){
          if(!slot(r,v)||!name(v,arg))return false;
        }else if(spec.arguments[i]=='n'&&tag=='n'){
          if(!r.take(4,v))return false;
          arg=std::to_string(int64_t(v)-(v>>31?int64_t(1)<<32:0));
        }else if(spec.arguments[i]=='n'&&tag=='z'){
          if(r.left()<8)return false;
          double d;std::memcpy(&d,r.at,8);r.at+=8;
          if(!std::isfinite(d))return false;
          std::ostringstream number;number<<std::setprecision(17)<<d;arg=number.str();
        }else return false;
      }
      out+=" "+arg;
    }
    if(op==0x1059&&r.left()==6)for(unsigned i=0;i<6;++i){if(*r.at++)return false;}
    return !r.left();
  }
  bool expression(StageReader& r,std::string& out){
    uint32_t length;
    if(!r.take(2,length)||length!=r.left())return false;
    std::vector<std::string> stack;
    unsigned tokens=0;
    while(r.left()){
      if(++tokens>512)return false;
      if(*r.at++!=' '||!r.left())return false;
      std::string item;
      if(*r.at=='r'||*r.at=='X'){
        if(*r.at=='r'){
          ++r.at;uint32_t id;
          if(!slot(r,id)||!r.left())return false;
          if(*r.at!='X'){
            if(*r.at!='s'&&*r.at!='f')return false;
            ++r.at;if(!variable(r,id,item))return false;
            stack.push_back(std::move(item));continue;
          }
          if(!name(id,item))return false;
          item+=".";
        }
        ++r.at;uint32_t op,size;
        if(!r.take(2,op)||!r.take(2,size)||size>r.left())return false;
        StageReader args{r.at,r.at+size};r.at+=size;
        if(!StageOperation(op).query||!call(op,args,item))return false;
      }else if(*r.at=='s'||*r.at=='f'||*r.at=='G'){
        if(!target(r,item))return false;
      }else{
        const auto start=r.at;
        while(r.left()&&*r.at!=' ')++r.at;
        item.assign(start,r.at);
        if(item=="~"){
          if(stack.empty())return false;
          stack.back()="(-"+stack.back()+")";continue;
        }
        if(item=="+"||item=="-"||item=="*"||item=="/"||item=="=="||item=="!="||item=="<"||item==">"||item=="<="||item==">="||item=="&&"||item=="||"){
          if(stack.size()<2)return false;
          auto rhs=std::move(stack.back());stack.pop_back();
          stack.back()="("+stack.back()+" "+item+" "+rhs+")";continue;
        }
        if(item.empty()||item.find_first_not_of("0123456789.")!=std::string::npos||item.find_first_of("0123456789")==std::string::npos||item.find('.')!=item.rfind('.'))return false;
      }
      stack.push_back(std::move(item));
    }
    if(stack.size()!=1)return false;
    out=std::move(stack.back());return true;
  }
public:
  StageDecoder(const fo3pipdata::Definitions& d,const fo3pipdata::ResultScript& s,uint32_t o):defs(d),script(s),owner(o){}
  bool decode(std::string& text){
    const auto& b=script.compiled;
    if(b.empty())return false;
    for(const auto& record:script.records)if(record.first=="SCRV"||(record.first=="SCRO"&&record.second.size()!=4))return false;
    StageReader stream{b.data(),b.data()+b.size()};
    struct Branch{size_t end;bool hasElse;};
    std::vector<Branch> branches;
    for(size_t line=0;stream.left();++line){
      uint32_t op,size,skip;
      std::string s,arg;
      if(!stream.take(2,op))return false;
      if(op==0x1c){
        uint32_t id;if(!slot(stream,id)||!name(id,s)||!stream.take(2,op)||op<0x1000)return false;
        s+=".";
      }
      if(!stream.take(2,size)||size>stream.left())return false;
      StageReader r{stream.at,stream.at+size};stream.at+=size;
      bool delimiter=op==0x17||op==0x18||op==0x19;
      if(delimiter!=(!branches.empty()&&line==branches.back().end))return false;
      if(op==0x15){
        if(!target(r,s)||!expression(r,arg))return false;
        s="set "+s+" to "+arg;
      }else if(op==0x16||op==0x18||op==0x17){
        if(!r.take(2,skip))return false;
        if(op!=0x16){if(branches.back().hasElse)return false;branches.pop_back();}
        if(!branches.empty()&&line+1+skip>=branches.back().end)return false;
        branches.push_back({line+1+skip,op==0x17});
        if(op==0x17){if(r.left())return false;s="else";}
        else {if(!expression(r,arg))return false;s=(op==0x16?"if ":"elseif ")+arg;}
      }else if(op==0x19){
        if(r.left())return false;
        branches.pop_back();s="endif";
      }else if(!call(op,r,s))return false;
      text+=s+"\n";
    }
    return branches.empty();
  }
};

inline bool DecodeResult(const fo3pipdata::Definitions& defs,const fo3pipdata::ResultScript& script,
    std::vector<fo3dialogue::Command>& result,fo3dialogue::CommandFormCapture* capture,uint32_t owner=0,uint32_t scriptId=0,bool allowUnavailable=false){
  if(DecodeSimpleResult(defs,script,result,capture,owner))return true;
  if(script.compiled.empty())return false;
  fo3pipdata::ResultScript decoded;
  if(!StageDecoder(defs,script,owner).decode(decoded.source))return false;
  std::string error;
  std::vector<fo3dialogue::Command> commands;
  if(!fo3dialogue::CompileSourceResult(defs,decoded,commands,error,owner,scriptId,capture,allowUnavailable))return false;
  if(!allowUnavailable)for(const auto& cmd:commands)if(cmd.type==fo3dialogue::CommandType::Unavailable)return false;
  result.swap(commands);return true;
}
}
