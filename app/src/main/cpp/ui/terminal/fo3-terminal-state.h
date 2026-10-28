#pragma once
#include "player/fo3-hacking.h"
#include "player/fo3-player-state.h"
#include "dialogue/fo3-dialogue-conditions.h"
namespace fo3terminal {
struct Cell {char glyph=' ';bool selected=false;};
struct Display {
  static constexpr int Columns=60,Rows=28;
  std::array<Cell,Columns*Rows> cells{};uint64_t revision=0;
  void Clear(){cells={};++revision;}
  void Text(int x,int y,const std::string& text,bool selected=false){
    const int start=x;for(char c:text){if(c=='\r')continue;if(c=='\n'){x=start;++y;continue;}
      if(x>=0&&x<Columns&&y>=0&&y<Rows)cells[size_t(y*Columns+x)]={c,selected};++x;}
  }
};
inline std::vector<std::string> Wrap(const std::string& text,size_t width=56){
  std::vector<std::string> lines;std::string line,word;
  auto flush=[&]{lines.push_back(line);line.clear();};
  for(char c:text){if(c=='\r')continue;if(c=='\n'){flush();continue;}
    line+=c;if(line.size()>=width){auto split=line.rfind(' ');if(split!=std::string::npos&&split>width/2){auto tail=line.substr(split+1);line.resize(split);flush();line=tail;}else flush();}}
  if(!line.empty())flush();return lines;
}
struct Session {
  enum class Page{Menu,Text,Hacking,Locked,Initializing};
  uint32_t reference=0,menu=0;Page page=Page::Menu;
  std::vector<uint32_t> parents;std::vector<size_t> entries;
  size_t selected=0,scroll=0;std::string result,image;uint32_t note=0;bool followSelection=true;
  fo3hacking::Game hacking;std::vector<std::string> log;Display display;
  bool released=false,lastA=true,lastB=true;double repeat=0,entered=0;uint64_t playerRevision=UINT64_MAX;
  std::string String(const fo3player::Player& p,const char* key,const char* original) const {
    auto i=p.Definitions().strings.find(key);return i==p.Definitions().strings.end()?original:i->second;
  }
  bool Visible(const fo3player::Player& p,const fo3pipdata::TerminalEntry& e) const {
    fo3dialogue::Context ctx;ctx.player=&p;ctx.speaker.reference=reference;
    auto ref=p.Definitions().references.find(reference);if(ref==p.Definitions().references.end())return false;
    ctx.speaker.base=ref->second.base;ctx.target.reference=fo3player::PlayerRef;ctx.target.base=fo3player::PlayerBase;
    std::string error;return fo3dialogue::Conditions(e.conditions,ctx,error);
  }
  void Refresh(fo3player::Player& p){
    display.Clear();if(!reference)return;playerRevision=p.Revision();
    const auto it=p.Definitions().pipboy.terminals.find(menu);if(it==p.Definitions().pipboy.terminals.end())return;
    const auto& terminal=it->second;
    if(page==Page::Hacking){
      display.Text(1,0,String(p,"sHackingHeader","ROBCO INDUSTRIES (TM) TERMLINK PROTOCOL"));
      display.Text(1,2,String(p,"sHackingHeader2","ENTER PASSWORD NOW"));
      display.Text(1,4,String(p,"sHackingHeader3","ATTEMPT(S) LEFT:")+" "+std::string(size_t(hacking.attempts),'#'));
      if(hacking.attempts==1)display.Text(1,5,String(p,"sHackingWarning","!!! WARNING: LOCKOUT IMMINENT !!!"));
      auto span=hacking.Span(hacking.cursor);
      for(int half=0;half<2;++half)for(int row=0;row<fo3hacking::Game::Rows;++row){
        const int start=half*fo3hacking::Game::Half+row*fo3hacking::Game::Columns;char address[12];
        std::snprintf(address,sizeof(address),"0x%04X",unsigned(hacking.address+start));int x=1+half*20;
        display.Text(x,7+row,address);
        for(int c=0;c<fo3hacking::Game::Columns;++c){int at=start+c;display.Text(x+7+c,7+row,std::string(1,hacking.dump[size_t(at)]),at>=span.first&&at<=span.second);}
      }
      int row=7;const size_t first=log.size()>12?log.size()-12:0;
      for(size_t i=first;i<log.size();++i)for(const auto& line:Wrap(log[i],17)){if(row<23)display.Text(42,row++,line);}
      if(span.first>=0)display.Text(42,24,">"+hacking.dump.substr(size_t(span.first),size_t(span.second-span.first+1)));
      display.Text(1,27,"A Select    B Exit    Stick Move");return;
    }
    display.Text(1,0,terminal.name);
    if(page==Page::Locked){
      for(size_t i=0;i<Wrap(result).size()&&i<21;++i)display.Text(1,3+int(i),Wrap(result)[i]);
      display.Text(1,27,"B Exit");return;
    }
    if(page==Page::Initializing){display.Text(1,3,"Initializing Robco Industries(TM) MF Boot Agent v2.3.0");display.Text(1,27,"B Exit");return;}
    if(page==Page::Text){const auto lines=Wrap(result);scroll=std::min(scroll,lines.size()>22?lines.size()-22:0);
      for(size_t i=scroll;i<lines.size()&&i<scroll+22;++i)display.Text(1,3+int(i-scroll),lines[i]);
      display.Text(1,27,"A / B Back    Stick Scroll");return;}
    std::vector<std::pair<std::string,bool>> lines;
    for(const auto& line:Wrap(terminal.description))lines.push_back({line,false});lines.push_back({"",false});
    entries.clear();for(size_t i=0;i<terminal.entries.size();++i)if(Visible(p,terminal.entries[i]))entries.push_back(i);
    if(entries.empty())selected=0;else selected=std::min(selected,entries.size()-1);
    size_t selectedRow=0;
    for(size_t i=0;i<entries.size();++i){auto label=Wrap(terminal.entries[entries[i]].text,53);if(i==selected)selectedRow=lines.size();
      for(size_t l=0;l<label.size();++l)lines.push_back({std::string(l==0?"> ":"  ")+label[l],i==selected});}
    if(followSelection&&!entries.empty()){
      if(selectedRow<scroll)scroll=selectedRow;else if(selectedRow>=scroll+23)scroll=selectedRow-22;
    }
    scroll=std::min(scroll,lines.size()>23?lines.size()-23:0);
    for(size_t i=scroll;i<lines.size()&&i<scroll+23;++i)display.Text(1,2+int(i-scroll),lines[i].first,lines[i].second);
    display.Text(1,27,"A Select  B Back  Stick Up/Down  Left/Right Scroll");
  }
  void Open(fo3player::Player& p,uint32_t ref,double now){
    *this={};reference=ref;menu=p.Definitions().references.at(ref).base;entered=now;lastA=lastB=true;
    if(p.GetLock(ref).locked){if(p.TerminalPassword(ref))p.OpenTerminalPassword(ref);
      else {page=Page::Initializing;if(!p.CanHackTerminal(ref)){page=Page::Locked;
        const auto lock=p.GetLock(ref);const int requirement=p.TerminalRequirement(ref);
        if(lock.broken)result=String(p,"sHackingLockout3","TERMINAL LOCKED")+"\n"+String(p,"sHackingLockout4","PLEASE CONTACT AN ADMINISTRATOR");
        else if(requirement<0)result=String(p,"sTerminalLocked","You cannot hack this computer.");
        else {char message[512];auto skill=p.Definitions().pipboy.skills[8].name;
          if(skill.empty())skill="Science";
          std::snprintf(message,sizeof(message),String(p,"sHackIneligible","A %s skill of %d is required to hack this terminal.").c_str(),skill.c_str(),requirement);result=message;}
      }}
    }Refresh(p);
  }
  bool BeginHack(fo3player::Player& p,const std::array<std::vector<std::string>,13>& dict,uint32_t seed){
    const int requirement=p.TerminalRequirement(reference);if(!p.CanHackTerminal(reference)||requirement<0)return false;
    const auto ref=p.Definitions().references.at(reference);const auto length=fo3hacking::WordLength(requirement/25,(ref.base&1)!=0);
    if(!hacking.Begin(dict[size_t(length)],fo3hacking::WordCount(float(p.Snapshot().skills[8]),requirement,
       int(p.LockSetting("iHackingMinWords",5)),int(p.LockSetting("iHackingMaxWords",20))),seed))return false;
    page=Page::Hacking;Refresh(p);return true;
  }
  void Back(fo3player::Player& p){
    if(page==Page::Text){page=Page::Menu;scroll=0;image.clear();note=0;Refresh(p);}
    else if(page==Page::Menu&&!parents.empty()){menu=parents.back();parents.pop_back();selected=0;Refresh(p);}
    else reference=0;
  }
};
}
