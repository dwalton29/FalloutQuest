#include "player/fo3-lockpick.h"
#include "npc/fo3-actor-animation.h"
#include "ui/interaction/fo3-item-notifications.h"
#include "world/interaction/fo3-interaction.h"
#include <memory>
#include <map>
#include <cstdio>
#include <iostream>
#include <stdexcept>
std::unique_ptr<fo3player::Session> gPlayerSession;
float gWeaponHaptic[2]{};int flushes=0;bool loading=false;
void FlushFo3PlayerState(){++flushes;}
bool IsFo3LoadingVisible(){return loading;}
namespace fo3audio {std::map<std::string,int> sounds;int loops=0,stops=0;
void NamedSound(const std::string& name){++sounds[name];}
void MenuLoop(const std::string& name){if(name.empty())++stops;else ++loops;}}
// Headless presentation/audio adapters; include the actual modal runtime below.
namespace fo3lockui {
struct Model {std::vector<fo3anim::Clip> clips;};struct Assets {Model lock,pin;};
Assets assets;bool ready=true,anchored=false;float breakElapsed=-1,introElapsed=0;
void Advance(){}void SetHead(const float*){anchored=true;}
std::string String(const fo3player::Player&,const char*,const char* fallback){return fallback;}
}
#define FO3_LOCKPICK_HEADLESS
#include "ui/lockpick/fo3-lockpick-runtime.inc"
void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
fo3player::Catalog Fixture(){using namespace fo3player;Catalog c;c.bobbyPin=10;c.initial.skills[4]=100;c.theftKarma=-5;
 Item pin;pin.formId=10;c.items[10]=pin;c.initial.inventory={{1,10,2,1,false}};c.initial.nextStackId=2;
 c.doorBases.insert(100);c.references[20].base=100;c.references[20].locked=true;c.references[20].lockLevel=100;
 c.references[20].owner=90;c.pipboy.dialogueActors[90].karma=0;c.lockSettings["iXPRewardPickLockVeryHard"]=60;return c;}
int main(){try{
 auto c=Fixture();gPlayerSession=std::make_unique<fo3player::Session>(std::move(c));
 fo3anim::Clip intro;intro.name="Forward";intro.stop=1.333333f;fo3lockui::assets.lock.clips={intro};
 fo3anim::Clip broken;broken.name="Left";broken.stop=.466667f;fo3lockui::assets.pin.clips={broken};
 auto& p=gPlayerSession->player;double now=10;auto update=[&](float motion,float torque,bool a,bool b,bool focus=true,bool grip=false){now+=.05;UpdateFo3Lockpick(now,motion,torque,a,b,focus,nullptr,grip);};
 Check(BeginFo3Lockpick(20)&&Fo3LockpickFocus(),"actual modal enters");
 for(int i=0;i<35;++i)update(1,1,true,false);
 Check(gLockpick.cylinder==0&&p.ItemCount(10)==2&&p.GetLock(20).locked&&flushes==0,"held entry A/trigger cannot force/break/rotate");
 update(0,0,false,false);gLockpick.pin=gLockpick.sweet>0?-90.f:90.f;
 for(int i=0;i<22;++i)update(0,1,false,false);
 Check(p.ItemCount(10)==1&&fo3audio::sounds["UILockpickingPickBreak"]==1&&flushes==1,"one break consumes one pin, emits once");
 for(int i=0;i<12;++i)update(0,0,false,false);
 Check(gLockpick.durability==1&&p.GetLock(20).locked,"authored break animation finishes before replacement");
 update(0,0,false,false);gLockpick.pin=gLockpick.sweet;
 for(int i=0;i<24;++i)update(0,1,false,false);
 Check(!Fo3LockpickFocus()&&!p.GetLock(20).locked&&p.ItemCount(10)==1&&p.Snapshot().experience==60&&p.Snapshot().karma==-5,"modal success commits actual state once");
 Check(fo3audio::sounds["UILockpickingUnlock"]==1&&flushes==2,"one success audio/save across logical updates");
 Check(Fo3LockpickInputBlocked(),"closing retains held trigger latch");update(0,1,false,false);Check(Fo3LockpickInputBlocked(),"held trigger cannot leak to weapon");
 update(0,0,false,false,true,true);Check(Fo3LockpickInputBlocked(),"grip release required");update(0,0,false,false);Check(!Fo3LockpickInputBlocked(),"full release restores gameplay input");
 fo3player::KarmaEvent k;Check(p.PollKarmaEvent(k)&&!p.PollKarmaEvent(k),"one real Karma event");
 Check(p.ScriptLock(20,100)&&BeginFo3Lockpick(20),"script relock retry");for(int i=0;i<30;++i)update(0,0,false,false);
 gLockpickRandom.seed(42);update(0,0,true,false);Check(p.GetLock(20).broken&&!Fo3LockpickFocus()&&fo3audio::sounds["UILockpickingForceFail"]==1,"force fails and breaks actual lock");
 const auto revision=p.Revision();for(int i=0;i<20;++i)update(0,0,true,false);Check(p.Revision()==revision&&fo3audio::sounds["UILockpickingForceFail"]==1,"held A cannot repeat failure");
 Check(p.ScriptLock(20,100)&&BeginFo3Lockpick(20),"reopen for focus test");update(0,0,false,false,false);
 Check(!Fo3LockpickFocus()&&p.GetLock(20).locked&&p.ItemCount(10)==1,"focus loss cancels without fabricated unlock");
 std::cout<<"Lockpick modal/input/audio tests passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
