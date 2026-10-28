#include "ui/terminal/fo3-terminal-state.h"
#include "ui/terminal/fo3-terminal-surface.h"
#include <memory>
#include <random>
#include <iostream>
#include <stdexcept>
static void Check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
std::unique_ptr<fo3player::Session> gPlayerSession;
struct Object {uint32_t refFormId=20;std::shared_ptr<const fo3terminal::Surface> terminalScreen;};
std::vector<Object> gObjects;uint32_t gCurrentCellFormId=1;float gQ210Head[4]{0,0,0,0};
std::mt19937 gLockpickRandom(31);uint64_t gFo3ResidentGameplayTicket=0;int saves=0;
bool IsFo3LoadingVisible(){return false;}bool Fo3ResidentFrameBlocked(){return false;}
void ResetFo3Pipboy(){}void FlushFo3PlayerState(){++saves;}
namespace fo3audio {int good=0,bad=0;uint32_t PlayingNote(){return 0;}
void Note(uint32_t,const std::vector<std::string>&){}void MenuLoop(const std::string&){}
void NamedSound(const std::string& s){good+=s=="UIHackingPassGood";bad+=s=="UIHackingPassBad";}}
fo3player::ScriptEventResult DispatchFo3ScriptedActivation(uint32_t,uint32_t,std::string&){return fo3player::ScriptEventResult::Executed;}
bool Fo3TerminalInputBlocked();int residentAttempts=0;
uint64_t BeginFo3ResidentPreparedWorkflow(int,std::unique_ptr<fo3player::ScriptTransactionPlan>){Check(!Fo3TerminalInputBlocked(),"terminal latch must not deadlock resident reservation");++residentAttempts;return 0;}
namespace fo3terminalui {
bool failed=false,ready=true;
struct {std::array<std::vector<std::string>,13> dictionary;} assets;
void Invalidate(){}void Advance(){}void AdvanceImage(const std::string&){}
void Render(const float*,const fo3terminal::Surface&,const fo3terminal::Display&,bool,bool){}
}
namespace fo3notify {struct Queue {void Enqueue(const std::string&) {}};Queue queue;Queue& Notifications(){return queue;}}
bool Fo3TerminalFocus();
#define FO3_TERMINAL_HEADLESS
#include "ui/terminal/fo3-terminal-runtime.inc"
int main(){try{
  using namespace fo3player;Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);c.initial.skills[8]=100;
  fo3pipdata::Terminal t;t.valid=true;t.name="Desk Terminal";t.difficulty=0;fo3pipdata::TerminalEntry e;e.text="Original entry";e.result="Original result";t.entries.push_back(e);
  fo3pipdata::TerminalEntry world;world.text="Disable sign";world.script.source="testsign.Disable";t.entries.push_back(world);
  c.references[30].base=300;c.pipboy.formNames["testsign"]=30;
  c.pipboy.terminals[100]=t;c.references[20].base=100;c.pipboy.targets[20].base=100;
  gPlayerSession=std::make_unique<Session>(c);auto& p=gPlayerSession->player;
  auto surface=std::make_shared<fo3terminal::Surface>();surface->width=surface->height=.3f;surface->triangles={0,1,2};gObjects.push_back({20,surface});
  fo3terminalui::assets.dictionary=fo3hacking::Dictionary("TEST REST BEST WEST NEST PEST ZEST");
  Check(BeginFo3Terminal(20)&&Fo3TerminalFocus()&&p.GetLock(20).locked,"A opens actual terminal without unlocking");
  double now=10;auto update=[&](bool a,bool b,bool focus=true){now+=.2;UpdateFo3Terminal(now,0,0,a,b,focus);};
  for(int i=0;i<10;++i)update(true,false);Check(gTerminal.page==fo3terminal::Session::Page::Initializing,"held entry A cannot select");
  update(false,false);update(false,false);Check(gTerminal.page==fo3terminal::Session::Page::Hacking,"real dictionary starts hacking");
  gTerminal.hacking.cursor=gTerminal.hacking.words[gTerminal.hacking.password].offset;update(true,false);
  Check(!p.GetLock(20).locked&&p.Snapshot().experience==20&&fo3audio::good==1&&gTerminal.page==fo3terminal::Session::Page::Menu,"winning hack commits once and opens original menu");
  for(int i=0;i<10;++i)update(true,false);Check(p.Snapshot().experience==20&&gTerminal.page==fo3terminal::Session::Page::Menu,"held win A does not select next menu entry");
  update(false,false);update(true,false);Check(gTerminal.page==fo3terminal::Session::Page::Text&&gTerminal.result=="Original result","A selects original terminal entry");
  update(false,false);update(false,true);Check(gTerminal.page==fo3terminal::Session::Page::Menu,"B returns to menu");
  update(false,false);update(false,true);Check(!Fo3TerminalFocus()&&Fo3TerminalInputBlocked(),"B closes with release latch");
  update(false,true);Check(Fo3TerminalInputBlocked(),"held close B cannot leak to weapon");
  now+=.2;UpdateFo3Terminal(now,0,0,false,false,true,1,false);Check(Fo3TerminalInputBlocked(),"held trigger remains blocked after terminal closes");
  now+=.2;UpdateFo3Terminal(now,0,0,false,false,true,0,true);Check(Fo3TerminalInputBlocked(),"held grip remains blocked after terminal closes");
  update(false,false);Check(!Fo3TerminalInputBlocked(),"full release restores gameplay");
  Check(BeginFo3Terminal(20),"reopen unlocked terminal");update(false,false,false);Check(!Fo3TerminalFocus()&&!p.GetLock(20).locked,"focus loss closes without relocking");
  Check(p.ScriptLock(20,0)&&BeginFo3Terminal(20),"start another hack");update(false,false);update(false,false);
  int guesses=0;for(size_t i=0;i<gTerminal.hacking.words.size()&&guesses<4;++i)if(i!=gTerminal.hacking.password){gTerminal.hacking.cursor=gTerminal.hacking.words[i].offset;update(true,false);update(false,false);++guesses;}
  Check(gTerminal.page==fo3terminal::Session::Page::Locked&&p.GetLock(20).broken&&fo3audio::bad==4,"four misses persist lockout and lock screen");
  CancelFo3Terminal();Check(p.ScriptUnlock(20)&&BeginFo3Terminal(20),"world action test opens terminal");
  update(false,false);gTerminal.selected=1;gTerminal.Refresh(p);update(true,false);
  Check(residentAttempts==1&&!Fo3TerminalFocus()&&Fo3TerminalInputBlocked(),"failed reservation closes safely and retains release latch");
  std::cout<<"Terminal runtime controls passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
