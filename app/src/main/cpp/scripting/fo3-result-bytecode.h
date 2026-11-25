#pragma once
#include "dialogue/fo3-dialogue-results.h"
#include <algorithm>
#include <cctype>

namespace fo3script {
// FNV ScriptRunner::GetNextLine supplies the word opcode/word byte-length
// envelope. IF's first word counts logical lines. Restrict admission to
// complete result streams. A reference prefix belongs to its following line.
inline bool DecodeSimpleResult(const fo3pipdata::Definitions& defs,const fo3pipdata::ResultScript& script,
                         std::vector<fo3dialogue::Command>& result,fo3dialogue::CommandFormCapture* capture,uint32_t owner=0) {
  using Type=fo3dialogue::CommandType;
  const auto& bytes=script.compiled;
  if(bytes.empty())return false;
  for(const auto& r:script.records)if(r.first=="SCRV"||(r.first=="SCRO"&&r.second.size()!=4))return false;
  auto word=[&](size_t i){return unsigned(bytes[i])+(unsigned(bytes[i+1])<<8);};
  auto ref=[&](size_t i)->uint32_t{
    const size_t slot=word(i);
    return slot&&slot<=script.references.size()?script.references[slot-1]:0;
  };
  std::vector<fo3dialogue::Command> commands;
  struct Branch {size_t delimiter;bool hasElse;};
  std::vector<Branch> ends;
  for(size_t pos=0;pos<bytes.size();){
    if(bytes.size()-pos<4)return false;
    uint32_t receiver=0;
    if(word(pos)==0x1c){
      receiver=ref(pos+2);pos+=4;
      if(!receiver||bytes.size()-pos<4)return false;
    }
    const auto op=word(pos),size=word(pos+2);pos+=4;
    if(receiver&&op!=0x1021&&op!=0x1022&&op!=0x105e)return false;
    if(size>bytes.size()-pos)return false;
    if(!ends.empty()&&commands.size()==ends.back().delimiter&&op!=0x19&&op!=0x17&&op!=0x18)return false;
    fo3dialogue::Command c{};
    if(op==0x16||op==0x18){
      if(op==0x18&&(ends.empty()||ends.back().hasElse||ends.back().delimiter!=commands.size()))return false;
      // Postfix: embedded GetStage(form), decimal integer literal, '<'.
      if(size<15||word(pos+2)!=size-4||bytes[pos+4]!=' '||bytes[pos+5]!='X'||
         word(pos+6)!=0x103a||word(pos+8)!=5||word(pos+10)!=1||bytes[pos+12]!='r')return false;
      auto id=ref(pos+13);auto q=defs.quests.find(id);
      if(!id||q==defs.quests.end())return false;
      size_t at=pos+15,end=pos+size;
      if(at>=end||bytes[at++]!=' ')return false;
      const size_t start=at;
      while(at<end&&bytes[at]>='0'&&bytes[at]<='9')++at;
      if(at==start||at-start>5||end-at!=2||bytes[at]!=' '||bytes[at+1]!='<')return false;
      std::string name=q->second.editor;
      for(char& ch:name){if(!std::isalnum(static_cast<unsigned char>(ch))&&ch!='_')return false;ch=char(std::tolower(static_cast<unsigned char>(ch)));}
      auto binding=defs.formNames.find(name);
      if(name.empty()||binding==defs.formNames.end()||binding->second!=id)return false;
      c.type=op==0x16?Type::If:Type::ElseIf;c.expression="getstage "+name+" < "+std::string(bytes.begin()+start,bytes.begin()+at);
      Branch next{commands.size()+1+word(pos),false};
      if(op==0x16)ends.push_back(next);else ends.back()=next;
    }else if(op==0x17){
      if(size!=2||ends.empty()||ends.back().hasElse||ends.back().delimiter!=commands.size())return false;
      ends.back()={commands.size()+1+word(pos),true};
      c.type=Type::Else;
    }else if(op==0x19){
      if(size||ends.empty()||ends.back().delimiter!=commands.size())return false;
      ends.pop_back();c.type=Type::EndIf;
    }else if(op==0x15){
      // SET global, length-prefixed postfix expression containing one literal.
      if(size<7||bytes[pos]!='G'||!(c.form=ref(pos+1))||word(pos+3)!=size-5||bytes[pos+5]!=' ')return false;
      size_t at=pos+6,end=pos+size;
      if(bytes[at]=='-')++at;
      bool point=false,digits=false;
      for(;at<end;++at){
        if(bytes[at]>='0'&&bytes[at]<='9')digits=true;
        else if(bytes[at]=='.'&&!point)point=true;
        else return false;
      }
      if(!digits)return false;
      c.type=Type::Global;c.expression.assign(bytes.begin()+pos+6,bytes.begin()+end);
    }else if(op==0x1021||op==0x1022||op==0x105e){
      if(size&&!(size==2&&word(pos)==0))return false;
      c.form=receiver?receiver:owner;
      if(!c.form)return false;
      c.type=op==0x105e?Type::EvaluatePackage:(op==0x1021?Type::Enable:Type::Disable);
    }else if(op==0x1036||op==0x1037||op==0x1039||op==0x11ad||op==0x117c){
      const bool stage=op==0x1039;
      if(size!=(stage?10u:5u)||word(pos)!=(stage?2u:1u)||bytes[pos+2]!='r'||!(c.form=ref(pos+3)))return false;
      if(op==0x117c){
        if(!defs.notes.count(c.form))return false;
        c.type=Type::AddNote;c.value=1;
      }else{
        if(!defs.quests.count(c.form))return false;
        switch(op){
          case 0x1036:c.type=Type::StartQuest;break;
          case 0x1037:c.type=Type::StopQuest;break;
          case 0x11ad:c.type=Type::CompleteAllObjectives;break;
          case 0x1039:
            if(bytes[pos+5]!='n'||word(pos+8)!=0)return false;
            c.type=Type::Stage;c.index=word(pos+6);break;
        }
      }
    }else{
      if(op!=0x11a2&&op!=0x11a3&&op!=0x1055)return false;
      if(size<5||bytes[pos+2]!='r'||!(c.form=ref(pos+3)))return false;
      const auto argc=word(pos);
      if(op==0x1055){
        if(!defs.markers.count(c.form)||(argc!=1&&argc!=2))return false;
        c.type=Type::ShowMap;
      }else{
        if(!defs.quests.count(c.form)||argc!=3)return false;
        c.type=op==0x11a2?Type::ObjectiveComplete:Type::ObjectiveDisplay;
      }
      if(size!=5+(argc-1)*5)return false;
      for(unsigned arg=1;arg<argc;++arg){
        const size_t at=pos+5*arg;
        if(bytes[at]!='n'||word(at+3)!=0)return false;
        unsigned value=word(at+1);
        if(arg==1&&op!=0x1055)c.index=value;
        else {if(value>1)return false;c.value=float(value);}
      }
    }
    commands.push_back(std::move(c));pos+=size;
  }
  if(!ends.empty())return false;
  if(capture){
    // Trace evidence uses the same immutable form-name bindings as source.
    for(const auto& c:commands)if(c.form){
      auto name=std::find_if(defs.formNames.begin(),defs.formNames.end(),[&](const auto& entry){return entry.second==c.form;});
      if(name==defs.formNames.end()||!capture->Add(c.form,name->first))return false;
    }
  }
  result.swap(commands);return true;
}
}

#include "fo3-stage-decoder.h"
