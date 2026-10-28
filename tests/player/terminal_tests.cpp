#include "ui/terminal/fo3-terminal-state.h"
#include "ui/terminal/fo3-terminal-surface.h"
#include "scripting/fo3-script-master-match.h"
#include "dialogue/fo3-dialogue-results.h"
#include <map>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace fo3player;
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static Catalog Fixture(){
  Catalog c;c.fingerprint=256;c.initial.special.fill(5);c.initial.baseHealth=100;c.initial.skills[8]=100;
  fo3pipdata::Terminal t;t.valid=true;t.name="Terminal";t.description="Authored welcome";t.difficulty=2;
  fo3pipdata::TerminalEntry e;e.text="Unlock door";e.result="Door unlocked";e.script.source="ref myLink\nset myLink to GetLinkedRef\nmyLink.Unlock\n";t.entries.push_back(e);
  c.pipboy.terminals[100]=t;c.references[20].base=100;c.pipboy.targets[20].base=100;c.pipboy.targets[20].linkedReference=21;
  c.references[21].base=200;c.references[21].locked=true;c.references[21].lockLevel=50;c.doorBases.insert(200);
  c.pipboy.formNames["computerwhiz"]=77;c.pipboy.perks[77].ranks=1;
  c.references[22].base=101;t.difficulty=5;t.password=10;c.pipboy.terminals[101]=t;
  Item note;note.kind=ItemKind::Note;note.formId=10;note.name="Password";c.items[10]=note;
  return c;
}
static void HackingRules(){
  for(int t=0;t<5;++t){Check(fo3hacking::WordLength(t,false)==4+2*t,"original even word lengths");Check(fo3hacking::WordLength(t,true)==std::min(12,5+2*t),"original base flag odd word lengths");}
  Check(fo3hacking::WordCount(50,50)==20&&fo3hacking::WordCount(100,50)==5&&fo3hacking::WordCount(100,100)==13,"original Science candidate formula and very hard half case");
  auto dict=fo3hacking::Dictionary("TEST REST BEST WEST TEST BAD bad LONGER FOO 1234");Check(dict[4].size()==4,"dictionary filters duplicates and non-original glyph tokens");
  fo3hacking::Game g;Check(!g.Begin({},20,1),"missing dictionary fails explicitly");
  Check(g.Begin(dict[4],20,1234)&&g.words.size()==4&&g.attempts==4,"real dictionary initialization");
  auto repeat=g;Check(repeat.dump==g.dump,"copy stable");
  Check(fo3hacking::Game::Likeness("FALL","FILL")==3,"likeness is exact position matches");
  for(size_t i=0;i<g.words.size();++i)if(i!=g.password){g.cursor=g.words[i].offset;auto result=g.Select();Check(result==fo3hacking::Game::Event::Denied,"wrong candidates consume attempts");int remaining=g.attempts;Check(g.Select()==fo3hacking::Game::Event::None&&g.attempts==remaining,"repeat guess cannot consume attempts");}
  g.cursor=g.words[g.password].offset;Check(g.Select()==fo3hacking::Game::Event::Granted,"correct candidate succeeds");
  auto d=fo3hacking::Dictionary("TEST REST BEST WEST NEST PEST ZEST");Check(g.Begin(d[4],7,9),"lockout fixture");int denied=0;
  for(size_t i=0;i<g.words.size()&&g.attempts;++i)if(i!=g.password){g.cursor=g.words[i].offset;auto result=g.Select();++denied;if(denied==4)Check(result==fo3hacking::Game::Event::Lockout,"fourth wrong guess locks out");}
  Check(g.attempts==0&&g.Select()==fo3hacking::Game::Event::None,"locked game cannot select");
  Check(g.Begin(d[4],7,12),"bracket fixture");g.dump.replace(0,12,"(!!)...[!!!]");
  for(auto& w:g.words)if(w.offset<12)w.offset+=24;
  g.cursor=0;auto bonus=g.Select();Check(bonus==fo3hacking::Game::Event::Dud||bonus==fo3hacking::Game::Event::Reset,"matched bracket bonus");
  Check(g.dump[0]=='.'&&g.Select()==fo3hacking::Game::Event::None,"bonus cannot be reused");
  Check(!g.words[g.password].removed,"bracket never removes password");
  g.cursor=11;g.Move(1,0);Check(g.cursor==fo3hacking::Game::Half,"right crosses to next dump column");g.Move(-1,0);Check(g.cursor==11,"left crosses back");
}
static void PersistenceAndMenus(){
  auto c=Fixture();Player p(c);Check(p.GetLock(20).locked&&p.TerminalRequirement(20)==50&&p.CanHackTerminal(20),"base terminal difficulty drives Science gate");
  auto low=c;low.initial.skills[8]=49;Player q(low);Check(!q.CanHackTerminal(20),"Science gate cannot use Lockpick skill");
  std::string error;Check(!p.PrepareTerminalTransaction(20,100,0,error),"locked terminal cannot run menu action");
  Check(p.FailTerminalHack(20)&&p.GetLock(20).broken&&!p.CanHackTerminal(20),"lockout commits actual reference");
  Check(p.Save("/tmp/fq-terminal-test.fqps",error),error.c_str());Player restored(c);Check(restored.Restore("/tmp/fq-terminal-test.fqps",error)&&restored.GetLock(20).broken&&!restored.CanHackTerminal(20),"lockout survives save and reload");
  Check(restored.GrantPerk(77,1)&&restored.CanHackTerminal(20)&&restored.LevelUpPerkSupported(77),"Computer Whiz is functional and selectable");
  Check(restored.FailTerminalHack(20)&&!restored.CanHackTerminal(20),"Computer Whiz retry exhausted");
  Check(p.GrantPerk(77,1)&&p.CompleteTerminalHack(20)&&!p.GetLock(20).locked&&p.Snapshot().experience==40,"hack unlocks and rewards original tier XP");
  Check(!p.CompleteTerminalHack(20)&&p.ScriptLock(20,50)&&p.CompleteTerminalHack(20)&&p.Snapshot().experience==40,"relocking does not duplicate XP");
  auto plan=p.PrepareTerminalTransaction(20,100,0,error,ScriptProgramCapture::EnteredPrograms);
  Check(bool(plan),error.c_str());Check(p.GetLock(21).locked,"menu preparation does not change linked door");
  Check(p.CommitScriptTransaction(*plan)&&!p.GetLock(21).locked,"authored linked-reference result unlocks actual door");
  Check(plan->ExecutedScripts().size()==1&&plan->ExecutedScripts()[0].origin.kind==ScriptProgramKind::TerminalEntry&&plan->ExecutedScripts()[0].terminalLinkedReference==21,"terminal source identity captured");
  fo3terminal::Session session;session.Open(p,20,1);Check(session.page==fo3terminal::Session::Page::Menu&&session.entries.size()==1,"unlocked root menu is displayed");
  const auto xp=p.Snapshot().experience;session.reference=0;Check(p.Snapshot().experience==xp,"closing does not change gameplay");
  Check(p.TerminalRequirement(22)==-1&&!p.CanHackTerminal(22)&&p.Add(10,1)&&p.TerminalPassword(22)&&p.OpenTerminalPassword(22)&&!p.GetLock(22).locked,"requires-key terminal accepts actual password note");
  Check(p.Snapshot().experience==xp,"password bypass awards no hacking XP");
  auto lev=c;lev.pipboy.terminals[100].flags=1;lev.initial.level=20;Player leveled(lev);Check(leveled.TerminalRequirement(20)==100,"original leveled difficulty cap");
  Check(leveled.FailTerminalHack(20)&&leveled.GrantPerk(77,1)&&leveled.TerminalRequirement(20)==100&&leveled.CanHackTerminal(20),"leveled requirement retained on Computer Whiz retry");
  Check(p.ScriptDoorState(21,true),"terminal opens actual door target");
  Check(p.Save("/tmp/fq-terminal-test.fqps",error),error.c_str());Player unlocked(c);Check(unlocked.Restore("/tmp/fq-terminal-test.fqps",error)&&!unlocked.GetLock(20).locked&&!unlocked.GetLock(21).locked&&!unlocked.GetLock(22).locked,"terminal and linked-door persistence");
  Check(unlocked.Snapshot().doorStates.at(21),"door open target survives v20 save");
  auto bad=c;bad.pipboy.terminals[100].flags=2;bad.pipboy.terminals[100].entries[0].script.source+="\nunsupported command\n";Player blocked(bad);
  Check(!blocked.PrepareTerminalTransaction(20,100,0,error)&&blocked.GetLock(21).locked,"unsupported action is atomic, no false unlock");
}
static void Surface(){
  fo3terminal::Surface s;std::vector<fo3terminal::Point> p{{-.2f,.2f,-1},{.2f,.2f,-1},{-.2f,-.2f,-1},{.2f,-.2f,-1},{2,2,2},{2.01f,2,2},{2,2.01f,2}};
  std::vector<fo3terminal::Point> n(p.size(),{0,0,1});
  Check(s.Build(p,{0,2,1,1,2,3,4,5,6},n)&&s.triangles.size()==6,"largest connected CRT selected without keyboard lamps");
  auto tl=s.At(0,0),br=s.At(1,1);Check(tl[0]<br[0]&&tl[1]>br[1]&&std::fabs(tl[2]+.9985f)<1e-5f,"world CRT coordinates are rightward and downward, above glass");
  Check(!s.Build(p,{500,1,2},n),"invalid surface rejected");
}
static void Original(const char* path,const char* dictionary){
  Catalog c;std::string error;Check(LoadCatalog(path,c,error),error.c_str());Check(c.pipboy.terminals.size()==378,"all original terminals decoded");
  size_t entries=0,invalid=0,scripts=0;for(const auto& t:c.pipboy.terminals){entries+=t.second.entries.size();invalid+=!t.second.valid;for(const auto& e:t.second.entries)scripts+=!e.script.compiled.empty();}
  Check(entries==1339&&invalid==0&&scripts>300,"all authored menu entries and result bodies preserved");
  if(dictionary){std::ifstream f(dictionary);std::string raw((std::istreambuf_iterator<char>(f)),{});auto d=fo3hacking::Dictionary(raw);for(int len=4;len<=12;++len)Check(d[size_t(len)].size()>=20,"original dictionary covers every difficulty length");}
  Player player(c);Check(player.ScriptUnlock(0xa36c2),"original Tenpenny terminal available");
  auto plan=player.PrepareTerminalTransaction(0xa36c2,0xa36c0,0,error,ScriptProgramCapture::EnteredPrograms);
  Check(bool(plan),error.c_str());fo3script::ScriptMasterMatch match;std::atomic<bool> cancel{false};
  Check(fo3script::MatchScriptProgramsToMaster(path,plan->ExecutedScripts(),match,cancel,error),error.c_str());
  Check(match.Matches(plan->ExecutedScripts()),"original terminal identity and XLKR evidence match master");
  std::map<std::string,int> errors;size_t supported=0,attempted=0;
  for(const auto& r:c.references){auto t=c.pipboy.terminals.find(r.second.base);if(t==c.pipboy.terminals.end())continue;
    for(const auto& entry:t->second.entries){std::vector<fo3dialogue::Command> commands;std::string why;
      if(entry.script.source.empty()&&entry.script.compiled.empty())continue;++attempted;
      if(fo3dialogue::CompileResult(c.pipboy,entry.script,commands,why,r.first))++supported;else ++errors[why];}}
  std::ofstream audit("/tmp/fq-terminal-script-audit.txt");audit<<supported<<"/"<<attempted<<" placed terminal action programs supported\n";
  for(const auto& e:errors)audit<<e.second<<" "<<e.first<<"\n";
  std::cout<<"Original ESM: "<<c.pipboy.terminals.size()<<" terminals, "<<entries<<" entries, "<<scripts<<" compiled action bodies\n";
}
int main(int argc,char** argv){try{HackingRules();PersistenceAndMenus();Surface();if(argc>1)Original(argv[1],argc>2?argv[2]:nullptr);std::cout<<"Terminal and hacking tests passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
