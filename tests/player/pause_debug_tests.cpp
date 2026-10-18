#include "ui/pause/fo3-debug-menu.h"
#include "world/fo3-resident-pause-clock.h"
#include "world/fo3-resident-input-barrier.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <cmath>
#include <unistd.h>
#include <fstream>
#include <zlib.h>
std::unique_ptr<fo3player::Session> gPlayerSession;
bool gFo3PauseOpen=false,gFo3FocusPaused=false,blocked=false,loading=false,gSceneReady=true,legacy=false;
bool gFo3ResidentVisualPlan=false,gFo3ResidentInteractionAdmission=false,gFo3ResidentReferencePublicationPlan=false;
struct {bool active=false;} gSceneLoad;
int flushes=0,pauses=0;
bool Fo3ResidentFrameBlocked(){return blocked;}
bool Fo3ResidentLegacyStreamActive(){return legacy;}
bool IsFo3LoadingVisible(){return loading;}
bool HasPendingFo3CellTransitionRequestQ74(){return false;}
void PauseFo3ResidentGameplayTime(){++pauses;}
void FlushFo3PlayerState(){++flushes;}
void ResetFo3LevelUpDraftAfterDebug(){}
namespace fo3audio {void NamedSound(const std::string&){}void PreloadSound(const std::string&) {}}
#include "ui/pause/fo3-pause-runtime.inc"
fo3player::Catalog Fixture(){
  fo3player::Catalog c;c.fingerprint=101;c.initial.special.fill(5);c.initial.skills.fill(20);c.initial.level=1;
  c.rules.healthEnduranceMult=20;c.rules.healthLevelMult=10;c.rules.apBase=65;c.rules.apMult=2;
  for(uint32_t id=1;id<=25;++id){auto& i=c.items[id];i.formId=id;i.name="Item "+std::to_string(id);i.maxCondition=id==1?100:0;i.kind=id==1?fo3player::ItemKind::Weapon:fo3player::ItemKind::Misc;}
  auto& perk=c.pipboy.perks[100];perk.name="Swift Learner";perk.ranks=3;perk.playable=true;
  fo3pipdata::PerkEffect e;e.type=2;e.entryPoint=9;e.parameterType=1;e.function=3;
  float multiplier=1.1f;e.parameters.resize(4);std::memcpy(e.parameters.data(),&multiplier,4);perk.effects.push_back(e);
  auto& unsupported=c.pipboy.perks[101];unsupported.name="Unsupported";unsupported.ranks=1;unsupported.playable=true;
  return c;
}
int main(int argc,char** argv){
  const auto c=Fixture();gPlayerSession=std::make_unique<fo3player::Session>(c);auto& p=gPlayerSession->player;
  auto dated=c;dated.initial.gameMonth=11;dated.initial.gameDay=31;dated.initial.gameHour=23;
  fo3player::Player yearEnd(dated);assert(yearEnd.DebugSetTime(0,0)&&yearEnd.Snapshot().gameYear==2278&&yearEnd.Snapshot().gameMonth==0&&yearEnd.Snapshot().gameDay==1);
  assert(!p.DebugSetTime(-1,0)&&!p.DebugSetTime(24,0)&&!p.DebugSetTime(12,60));
  assert(p.DebugSetTime(21,30)&&p.Snapshot().gameDaysPassed==0&&p.Snapshot().gameHour==21.5f);
  assert(p.DebugSetTime(9,0)&&p.Snapshot().gameDaysPassed==1&&p.Snapshot().gameDay==18);
  assert(p.DebugSetTime(9,0)&&p.Snapshot().gameDaysPassed==1);
  assert(!p.DebugSetPerkPoints(-1)&&!p.DebugSetPerkPoints(1001));
  assert(!p.DebugSetPerkRank(100,1)&&p.DebugSetPerkPoints(5));
  assert(p.DebugSetPerkRank(100,1)&&p.Snapshot().debugPerkPoints==4);
  assert(!p.DebugSetPerkRank(101,1)&&!p.DebugSetPerkRank(100,4));
  assert(p.DebugSetPerkRank(100,0)&&p.Snapshot().debugPerkPoints==5&&!p.Snapshot().pipboy.perks.count(100));
  assert(p.DebugSetPerkRank(100,1));
  uint32_t reward=0;assert(p.PreviewExperienceAward(100,reward)&&reward==110);
  assert(p.Snapshot().experience==0&&p.DebugReachNextLevel());
  assert(p.Snapshot().experience>=200&&p.Snapshot().experience<=201&&p.Snapshot().level==1);
  assert(p.ExperienceProgress().ready&&!p.DebugReachNextLevel());
  assert(p.RewardExperience(5000)&&p.ExperienceProgress().pendingLevels>1);
  fo3player::ExperienceNotice notice;assert(p.PollExperienceNotice(notice)&&notice.levelUp);
  const std::string path="/tmp/fq-pause-debug-"+std::to_string(getpid());std::string error;
  assert(p.Save(path,error));fo3player::Player restored(c);assert(restored.Restore(path,error));
  assert(restored.Snapshot().debugPerkPoints==4&&restored.Snapshot().pipboy.perks.at(100)==1);
  assert(restored.Snapshot().experience==p.Snapshot().experience&&restored.Snapshot().gameDaysPassed==1);
  // A valid CRC cannot make an out-of-range v19 budget acceptable. Restore
  // must leave the existing player untouched on extension validation failure.
  {
    std::ifstream in(path,std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});in.close();
    bytes[bytes.size()-4]=0xe9;bytes[bytes.size()-3]=3; // 1001
    const auto crc=uint32_t(crc32(0,bytes.data()+20,uInt(bytes.size()-20)));
    for(int i=0;i<4;++i)bytes[16+i]=uint8_t(crc>>(i*8));
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));out.close();
    const auto revision=restored.Revision();assert(!restored.Restore(path,error)&&restored.Revision()==revision&&restored.Snapshot().debugPerkPoints==4);
  }
  assert(p.DebugSetPerkPoints(0)&&p.Save(path,error)&&restored.Restore(path,error)&&restored.Snapshot().debugPerkPoints==0);unlink(path.c_str());
  fo3debug::Menu m;m.Open(p);const auto rev=p.Revision();m.selected=1;m.Confirm();assert(m.page==fo3debug::Menu::Page::Root&&p.Revision()==rev);
  m.Go(fo3debug::Menu::Page::Special);m.special=p.Snapshot().special;m.selected=0;m.Adjust(1);assert(p.Snapshot().special[0]==5);
  m.selected=7;assert(m.Confirm()&&p.Snapshot().special[0]==6);
  m.category=4;m.BuildItems();m.Go(fo3debug::Menu::Page::Items);assert(m.items.size()==24);
  m.selected=11;m.Confirm();assert(m.itemPage==1);m.selected=0;m.Confirm();assert(m.page==fo3debug::Menu::Page::Item);const auto id=m.form;
  m.selected=1;assert(m.Confirm()&&p.ItemCount(id)==10);assert(p.Snapshot().karma==0);
  m.Go(fo3debug::Menu::Page::Hex);m.hex=0xdeadbeef;m.selected=8;const auto before=p.Revision();assert(!m.Confirm()&&p.Revision()==before);
  m.hex=1;m.selected=8;m.Confirm();assert(m.page==fo3debug::Menu::Page::Item);m.selected=3;m.Adjust(-1);m.selected=0;assert(m.Confirm());
  const auto& weapon=p.Snapshot().inventory.back();assert(weapon.formId==1&&std::fabs(weapon.condition-.9f)<.001f);
  // Actual production input/admission bridge: held A never grants; explicit
  // confirms flush once. Focus/loading/publication cannot perform mutations.
  double t=10;auto input=[&](bool menu,bool a,bool b=false,float x=0,float y=0,bool focus=true){t+=.05;UpdateFo3Pause(menu,a,b,x,y,t,focus);};
  input(true,true);assert(gFo3PauseOpen&&pauses==1);input(false,true);assert(gFo3PauseOpen);
  input(false,false);gFo3PauseMenu.Go(fo3debug::Menu::Page::XP);const auto xp=p.Snapshot().experience;
  input(false,true);assert(p.Snapshot().experience==xp+110&&flushes==1);
  for(int i=0;i<10;++i) {input(false,true);}
  assert(flushes==1);
  input(false,false);blocked=true;input(false,true);assert(!gFo3PauseOpen&&flushes==1);blocked=false;
  input(false,false);loading=true;input(true,false);assert(!gFo3PauseOpen);loading=false;
  input(false,false);input(true,false);assert(gFo3PauseOpen);input(false,false,false,0,0,false);assert(!gFo3PauseOpen);
  fo3scene::ResidentInputBarrier barrier;assert(!barrier.Allow(true,false));assert(!barrier.Allow(false,false));assert(!barrier.Allow(false,true));assert(barrier.Allow(false,true));
  fo3scene::ResidentPauseClock clock;assert(clock.Sample(100,false)==100);clock.Pause();assert(clock.Sample(10000,true)==100);assert(clock.Sample(20000,false)==100);assert(clock.Sample(20010,false)==110);
  assert(p.Save(path,error)&&restored.Restore(path,error));
  assert(restored.Snapshot().special[0]==6&&restored.ItemCount(id)==10);
  assert(restored.Snapshot().inventory.back().condition==weapon.condition);unlink(path.c_str());
  if(argc>1){
    fo3player::Catalog original;assert(fo3player::LoadCatalog(argv[1],original,error));
    fo3player::Player real(original);
    const auto swift=original.pipboy.formNames.at("swiftlearner");
    assert(real.DebugSetPerkPoints(2)&&real.DebugSetPerkRank(swift,1));
    assert(real.PreviewExperienceAward(100,reward)&&reward==110);
    assert(real.DebugReachNextLevel()&&real.ExperienceProgress().ready);
    assert(real.Add(0x434f,1,.5f)&&real.Add(0x4241,100));
    assert(real.DebugSetTime(23,59)&&real.DebugSetTime(0,1));
    assert(real.Save(path,error));fo3player::Player copy(original);assert(copy.Restore(path,error));
    assert(copy.Snapshot().debugPerkPoints==1&&copy.Snapshot().experience==real.Snapshot().experience&&copy.ItemCount(0x4241)==real.ItemCount(0x4241));unlink(path.c_str());
    std::cout<<"Original Fallout3.esm: "<<original.items.size()<<" items, "<<original.pipboy.perks.size()<<" perks; real Swift Learner, XP, grants, time and save passed\n";
  }
  std::cout<<"Pause/debug time, XP modifiers, perk ranks/budget, SPECIAL, inventory, save and input tests passed\n";
}
