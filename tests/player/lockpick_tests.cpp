#include "player/fo3-lockpick.h"
#include "audio/fo3-audio-catalog.h"
#include "dialogue/fo3-dialogue-conditions.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <iterator>
#include <zlib.h>
using namespace fo3player;
void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
Catalog Fixture(){Catalog c;c.initial.baseHealth=100;c.initial.special.fill(1);c.fingerprint=207;c.bobbyPin=10;c.infiltrator=77;c.theftKarma=-5;c.initial.skills[4]=100;
 Item pin;pin.formId=10;pin.name="Bobby Pin";c.items[10]=pin;Item key;key.formId=11;key.kind=ItemKind::Key;c.items[11]=key;
 c.initial.inventory={{1,10,3,1,false}};c.initial.nextStackId=2;
 c.pipboy.perks[77].ranks=1;c.pipboy.dialogueActors[90].karma=0;
 for(int i=0;i<7;++i){Reference r;r.base=100;r.locked=true;r.lockLevel=uint8_t(i<5?i*25:255);r.key=11;c.references[uint32_t(20+i)]=r;}
 c.doorBases.insert(100);c.eventBaseNames[100]="Door";c.references[30]=c.references[22];c.references[30].base=200;c.containers[200]={};
 c.references[23].owner=90;c.pipboy.formNames["testlock"]=23;c.pipboy.targets[23].base=100;
 for(int i=0;i<5;++i)c.lockSettings[std::string("iXPRewardPickLock")+std::vector<std::string>{"VeryEasy","Easy","Average","Hard","VeryHard"}[i]]=float(20+i*10);
 return c;}
void Synthetic(){auto c=Fixture();Player p(c);KarmaEvent karma;TheftEvent crime;
 for(int i=0;i<5;++i){Check(p.LockRequirement(20+i)==i*25,"five tier requirements");auto low=c;low.initial.skills[4]=uint8_t(i?i*25-1:0);Player q(low);Check(q.CanPickLock(20+i)==(i?PickAccess::Skill:PickAccess::Ready),"skill gate");}
 Check(p.CanPickLock(25)==PickAccess::RequiresKey,"impossible");c.references[26].lockFlags=4;Player leveled(c);Check(leveled.GetLock(26).leveled&&leveled.CanPickLock(26)==PickAccess::Unsupported,"leveled explicitly unresolved");
 Check(p.CanPickLock(30)==PickAccess::Ready&&!p.CanLootContainer(30),"actual locked container");
 auto no=c;no.initial.inventory.clear();Player empty(no);Check(empty.CanPickLock(20)==PickAccess::NoPins,"no fabricated pins");
 auto script=c;script.containers[200].script=99;Player blocked(script);Check(blocked.CanPickLock(30)==PickAccess::Unsupported,"scripted container protection");
 fo3lockpick::Game game;Check(game.Begin(p,24,.5f),"begin actual reference");Check(p.Snapshot().karma==0&&!p.PollKarmaEvent(karma),"looking/entry no penalty");
 game.pin=game.sweet;for(int i=0;i<19;++i)Check(game.Step(.05f,0,1)==fo3lockpick::Game::Event::None,"proper cylinder travel");
 Check(game.Step(.05f,0,1)==fo3lockpick::Game::Event::Unlock&&game.cylinder==90,"correct angle opens");
 Check(p.CompleteLockpick(24)&&!p.GetLock(24).locked&&p.ItemCount(10)==3&&p.Snapshot().experience==60,"one real unlock reward and pin retained");
 Check(!p.CompleteLockpick(24)&&p.Snapshot().experience==60,"no duplicate XP");
 // Lock XP is sent through the same acquired-perk modifiers as scripted XP.
 auto swift=c;swift.pipboy.perks[88].ranks=1;
 fo3pipdata::PerkEffect effect;effect.type=2;effect.rank=0;
 effect.entryPoint=9;effect.function=3;effect.parameterType=1;
 effect.parameters={0xCD,0xCC,0x8C,0x3F}; // 1.1f multiplier
 swift.pipboy.perks[88].effects.push_back(effect);
 Player learner(swift);Check(learner.GrantPerk(88,1),"XP perk acquired");
 Check(learner.CompleteLockpick(24)&&learner.Snapshot().experience==66,
       "lock XP applies 10 percent perk modifier");
 ExperienceNotice earned;
 Check(learner.PollExperienceNotice(earned)&&earned.amount==66&&
       earned.total==66&&!earned.levelUp,"modified lock XP displayed once");
 Check(!learner.PollExperienceNotice(earned)&&
       !learner.CompleteLockpick(24),"lock cannot award twice");

 Check(game.Begin(p,23,.5f),"owned lock begin");game.pin=80;Check(game.StopAngle()==0,"incorrect angle binds");
 Check(game.Step(.05f,1,1)==fo3lockpick::Game::Event::Bind&&game.pin==80,"pin immobile under torque");const auto damaged=game.durability;
 game.Step(.05f,0,0);Check(game.durability==damaged,"release does not repair pin");
 for(int i=0;i<20;++i)game.Step(.05f,0,1);Check(game.durability==0&&p.ItemCount(10)==3,"mechanics alone cannot change inventory");
 Check(p.BreakBobbyPin(23)&&p.ItemCount(10)==2&&p.Snapshot().karma==0,"break consumes one real pin no karma");game.Replace();Check(game.durability==1&&game.cylinder==0,"replacement retains target");
 game={};Check(p.GetLock(23).locked,"cancel cannot unlock");
 Check(p.ForceLockChance(24)==10&&p.ForceLockChance(20)==100,"force boundaries");
 Check(p.ForceLock(23,99)&&p.GetLock(23).broken&&p.CanPickLock(23)==PickAccess::Broken&&p.Snapshot().karma==0,"failed force persistent broken no karma");
 Check(!p.ForceLock(23,0),"broken lock cannot repeat force");Check(p.GrantPerk(77,1)&&p.CanPickLock(23)==PickAccess::Ready,"real perk permits retry");
 Check(p.ForceLock(23,99)&&p.GetLock(23).forceFailures==2&&p.CanPickLock(23)==PickAccess::Broken,"one extra failed force only");
 Check(p.Add(11,1)&&p.UnlockByKey(23)&&!p.GetLock(23).locked&&p.Snapshot().experience==60&&p.Snapshot().karma==0,"key works after break without XP/Karma");
 Check(p.CompleteLockpick(30)&&p.PrepareContainer(30)&&p.ContainerContents(30)->empty(),"container unlock unlocks actual loot path without fake items");
 Check(p.ScriptLock(23,75)&&p.CompleteLockpick(23)&&p.Snapshot().experience==150&&p.Snapshot().karma==-5,"original owned success awards XP and Karma");
 Check(p.PollKarmaEvent(karma)&&karma.amount==-5&&!p.PollKarmaEvent(karma),"one original feedback event");
 Check(p.PollTheftEvent(crime)&&crime.action==CrimeAction::PickLock&&crime.owner==90&&crime.item==0,"future crime boundary distinguishes lockpick");
 Check(p.ScriptLock(23,75)&&p.CompleteLockpick(23)&&p.Snapshot().experience==150&&p.Snapshot().karma==-10,"script relock no repeated XP, subsequent completed crime");
 fo3pipdata::ResultScript result;result.source="testlock.Lock 50\ntestlock.Unlock\n";std::string error;
 Check(p.PreviewDialogueResult(result,error)&&!p.GetLock(23).locked,"lock script preview rolls back");
 Check(p.ExecuteDialogueResult(result,error)&&!p.GetLock(23).locked,"source lock/unlock functions");
 result.source="testlock.Lock 50\nplayer.AddItem MissingForm 1\n";Check(!p.ExecuteDialogueResult(result,error)&&!p.GetLock(23).locked,"failed source transaction rolls back lock");
 Check(p.ScriptLock(23,50),"script lock");result.source="if testlock.GetLocked == 1 && testlock.GetLockLevel == 50\ntestlock.Unlock\nendif\n";
 Check(p.ExecuteDialogueResult(result,error)&&!p.GetLock(23).locked,"source queries see mutable lock");
 Check(p.Save("/tmp/fq-lock.fqps",error),error.c_str());Player restored(c);Check(restored.Restore("/tmp/fq-lock.fqps",error),error.c_str());
 Check(!restored.GetLock(23).locked&&restored.GetLock(23).forceFailures==2&&restored.ItemCount(10)==2&&restored.Snapshot().karma==-10&&restored.Snapshot().experience==150,"save inventory locks XP Karma");
 std::ifstream valid("/tmp/fq-lock.fqps",std::ios::binary);const std::vector<uint8_t> savedBytes((std::istreambuf_iterator<char>(valid)),{});
 const size_t lockStart=savedBytes.size()-8*p.Snapshot().locks.size();
 auto rejected=[&](size_t position,uint32_t value){auto bytes=savedBytes;
   auto put=[&](size_t at,uint32_t v){for(int k=0;k<4;++k)bytes[at+k]=uint8_t(v>>(k*8));};put(position,value);
   put(16,uint32_t(crc32(0,bytes.data()+20,bytes.size()-20)));std::ofstream bad("/tmp/fq-lock-bad.fqps",std::ios::binary);bad.write(reinterpret_cast<char*>(bytes.data()),bytes.size());bad.close();
   const auto revision=restored.Revision();Check(!restored.Restore("/tmp/fq-lock-bad.fqps",error)&&restored.Revision()==revision&&!restored.GetLock(23).locked,"corrupt lock restore is atomic");};
 rejected(lockStart,0xfffffffeu);const uint32_t first=uint32_t(savedBytes[lockStart])|(uint32_t(savedBytes[lockStart+1])<<8)|(uint32_t(savedBytes[lockStart+2])<<16)|(uint32_t(savedBytes[lockStart+3])<<24);
 rejected(lockStart+8,first);rejected(lockStart+4,0xffffffffu);
 auto clean=Fixture();Player legacy(clean);Check(legacy.Save("/tmp/fq-lock-legacy.fqps",error),error.c_str());
 std::ifstream input("/tmp/fq-lock-legacy.fqps",std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});bytes.resize(bytes.size()-8);bytes[4]=13;
 auto put=[&](size_t at,uint32_t v){for(int k=0;k<4;++k)bytes[at+k]=uint8_t(v>>(k*8));};put(12,uint32_t(bytes.size()-20));put(16,uint32_t(crc32(0,bytes.data()+20,bytes.size()-20)));
 std::ofstream out("/tmp/fq-lock-v13.fqps",std::ios::binary);out.write(reinterpret_cast<char*>(bytes.data()),bytes.size());out.close();
 Check(restored.Restore("/tmp/fq-lock-v13.fqps",error)&&restored.GetLock(23).locked&&restored.Snapshot().experience==0,"v13 migration uses original initial locks");
}
void Original(const char* path){Catalog c;std::string error;Check(LoadCatalog(path,c,error),error.c_str());Check(c.bobbyPin==10,"original Bobby Pin EDID");
 c.initial.skills[4]=100; // Explicit original-data test character fixture, never runtime inventory.
 Player p(c);Check(p.Add(c.bobbyPin,3),"test fixture pins");uint32_t door=0,container=0;
 for(const auto& r:c.references){auto cell=c.cellOwnership.find(r.second.cell);if(cell==c.cellOwnership.end()||cell->second.editor.find("Megaton")==std::string::npos)continue;
   if(p.CanPickLock(r.first)!=PickAccess::Ready)continue;
   if(c.containers.count(r.second.base)){if(!container||r.first<container)container=r.first;}
   else if(c.doorBases.count(r.second.base)){if(!door||r.first<door)door=r.first;}
 }
 Check(door&&container,"original Megaton door and container");const auto xp=p.Snapshot().experience;
 for(auto ref:{door,container}){const auto lock=p.GetLock(ref);Check(lock.locked&&lock.level<=100,"original decoded XLOC");const auto ownership=p.ResolveOwnership(ref);const auto before=p.Snapshot().karma;
   Check(p.CompleteLockpick(ref)&&p.CanOpenDoor(ref),"original door/container state");Check(p.ResolveOwnership(ref).data.owner==ownership.data.owner,"unlock cannot transfer ownership");
   Check(p.Snapshot().karma==before+(ownership.Theft()&&ownership.karmaEligible?-5:0),"original unlock karma conditions");
 }
 Check(p.CanLootContainer(container)&&p.PrepareContainer(container),"original content access");Check(p.Snapshot().experience>xp,"original XP settings");
 Check(p.Save("/tmp/fq-lock-original.fqps",error)&&p.Restore("/tmp/fq-lock-original.fqps",error)&&!p.GetLock(door).locked&&!p.GetLock(container).locked,"original save/reload");
 fo3audio::Catalog audio;Check(fo3audio::LoadCatalog(path,audio,error),error.c_str());
 for(const char* sound:{"UILockpickingEnter","UILockpickingForceFail","UILockpickingUnlock","UILockpickingPickMovement","UILockpickingPickTensionLPM","UILockpickingPickBreak","UILockpickingCylinderTurn","UILockpickingCylinderStop","UILockpickingCylinderSqueakA","UILockpickingCylinderSqueakB"})Check(!audio.sounds.at(audio.names.at(sound)).path.empty(),"original SOUN paths including directories");
 std::cout<<"Original Megaton door="<<std::hex<<door<<" container="<<container<<std::dec<<" XP="<<p.Snapshot().experience<<" Karma="<<p.Snapshot().karma<<"\n";
}
int main(int argc,char** argv){try{Synthetic();if(argc>1)Original(argv[1]);std::cout<<"Lockpick tests passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
