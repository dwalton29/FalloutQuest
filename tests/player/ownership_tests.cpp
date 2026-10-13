#include "player/fo3-player-state.h"
#include "audio/fo3-audio-catalog.h"
#include "ui/interaction/fo3-item-notifications.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <zlib.h>
#include <iterator>
using namespace fo3player;
void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
Catalog Fixture(){
 Catalog c;c.theftKarma=-5;c.initial.karma=10;c.fingerprint=123;
 Item i;i.formId=100;i.name="Cup";c.items[100]=i;i.formId=101;i.name="Plate";c.items[101]=i;
 c.pipboy.dialogueActors[PlayerBase]={};c.pipboy.dialogueActors[90].karma=0;
 c.pipboy.dialogueActors[91].karma=-250;
 c.factionFlags[80]=0x100;c.factionFlags[81]=0x102;c.factionFlags[82]=0;
 for(uint32_t id=1;id<=7;++id){Reference r;r.base=100;r.count=2;c.references[id]=r;}
 c.references[2].owner=PlayerBase;c.references[3].owner=90;c.references[4].owner=91;
 c.references[5].owner=80;c.references[5].ownership.rank=2;
 c.references[6].cell=60;c.cellOwnership[60].data={90,0,0,true,true};c.cellOwnership[60].flags=0x21;
 c.references[7].cell=60;c.references[7].ownership.explicitOwner=true;
 c.references[20].base=200;c.references[20].owner=90;
 c.containers[200].entries={{100,0,3,1,1,false},{101,0,2,1,1,false}};
 c.references[21].base=201;
 LootEntry a;a.form=100;a.count=2;a.owner=90;a.extra=true;
 LootEntry b;b.form=100;b.count=3;b.owner=PlayerBase;b.extra=true;
 LootEntry d;d.form=101;d.count=1;d.owner=80;d.ownerParameter=2;d.extra=true;
 c.containers[201].entries={a,b,d};
 return c;
}
void Synthetic(){
 auto c=Fixture();Player p(c);KarmaEvent k;TheftEvent e;
 Check(p.ResolveOwnership(1).kind==OwnershipKind::Unowned,"unowned");
 Check(p.Pickup(1)&&p.Snapshot().karma==10&&!p.PollTheftEvent(e),"unowned transfer");
 Check(p.Pickup(2)&&!p.PollKarmaEvent(k),"player-owned legal transfer");
 Check(p.ResolveOwnership(3).Theft()&&p.CanPickup(3),"owned means stealable");
 Check(p.Pickup(3)&&p.Snapshot().karma==5&&p.PollKarmaEvent(k)&&k.amount==-5,"NPC theft changes real karma once");
 Check(p.PollTheftEvent(e)&&e.owner==90&&e.count==2&&e.item==100&&e.player==PlayerRef,"crime boundary original source identity");
 Check(!p.Pickup(3)&&!p.PollKarmaEvent(k)&&!p.PollTheftEvent(e),"repeated input cannot duplicate");
 Check(p.Pickup(4)&&p.Snapshot().karma==5&&!p.PollKarmaEvent(k),"evil NPC exemption");
 Check(p.ResolveOwnership(5).Theft(),"faction membership required");
 c.pipboy.dialogueActors[PlayerBase].factions[80]=1;Player low(c);Check(low.ResolveOwnership(5).Theft(),"insufficient rank");
 c.pipboy.dialogueActors[PlayerBase].factions[80]=2;Player member(c);Check(member.ResolveOwnership(5).permitted,"sufficient original rank");
 c.pipboy.dialogueActors[PlayerBase].factions[80]=-1;Player removed(c);Check(removed.ResolveOwnership(5).Theft(),"negative rank is not membership");
 Check(p.ResolveOwnership(6).Theft()&&p.ResolveOwnership(6).inherited,"public CELL retains item ownership");
 Check(p.ResolveOwnership(7).Theft()&&p.ResolveOwnership(7).inherited,"null owner retains CELL inheritance");
 Check(p.Pickup(6)&&p.Snapshot().karma==0,"CELL theft");
 while(p.PollKarmaEvent(k)){}
 Check(p.PrepareContainer(20)&&p.Snapshot().karma==0&&!p.PollKarmaEvent(k),"viewing does not change karma or queue feedback");
 p.BeginContainerInteraction(20);auto s=p.ContainerContents(20)->front().id;
 Check(p.TakeContainerStack(20,s)&&p.Snapshot().karma==-5,"first successful container theft");
 Check(!p.TakeContainerStack(20,s),"duplicate stack rejected");
 while(p.PollKarmaEvent(k)){}
 Check(p.TakeContainerStack(20,p.ContainerContents(20)->front().id)&&p.Snapshot().karma==-5&&!p.PollKarmaEvent(k),"one penalty per view session");
 Player reopen(Fixture());Check(reopen.PrepareContainer(20),"prepare fresh");
 Check(reopen.TakeContainerStack(20,reopen.ContainerContents(20)->front().id),"first session");
 reopen.EndContainerInteraction();reopen.BeginContainerInteraction(20);
 Check(reopen.TakeContainerStack(20,reopen.ContainerContents(20)->front().id)&&reopen.Snapshot().karma==0,"reopen incurs another penalty");
 Player mixed(Fixture());Check(mixed.PrepareContainer(21)&&mixed.ContainerContents(21)->size()==3,"ownership prevents stack merging");
 const auto own=mixed.ContainerContents(21)->at(0).id,legal=mixed.ContainerContents(21)->at(1).id;
 Check(mixed.ContainerStackOwnership(21,own).Theft()&&mixed.ContainerStackOwnership(21,legal).permitted,"mixed ownership");
 Check(mixed.TakeContainerStack(21,legal)&&mixed.Snapshot().karma==10,"legal row in mixed contents");
 std::string error;Check(mixed.Save("/tmp/fq-ownership.fqps",error),error.c_str());
 Player restored(Fixture());Check(restored.Restore("/tmp/fq-ownership.fqps",error),error.c_str());
 Check(restored.ContainerStackOwnership(21,own).Theft()&&restored.TakeContainerStack(21,own)&&restored.Snapshot().karma==5,"v13 preserves depleted contents and ownership");
 Check(p.Save("/tmp/fq-theft.fqps",error),error.c_str());Player saved(Fixture());
 Check(saved.Restore("/tmp/fq-theft.fqps",error)&&saved.IsCollected(3)&&saved.IsCollected(6)&&saved.Snapshot().karma==-5&&!saved.PollKarmaEvent(k),"restart persists without replaying feedback");

 // Original v12 shape: remove only the empty v13 sparse-table count.
 std::ifstream input("/tmp/fq-theft.fqps",std::ios::binary);
 std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)),{});bytes.resize(bytes.size()-4);bytes[4]=12;
 auto put=[&](size_t at,uint32_t value){for(size_t j=0;j<4;++j)bytes[at+j]=static_cast<unsigned char>(value>>(8*j));};
 put(12,static_cast<uint32_t>(bytes.size()-20));put(16,static_cast<uint32_t>(crc32(0,bytes.data()+20,bytes.size()-20)));
 std::ofstream legacy("/tmp/fq-theft-v12.fqps",std::ios::binary);legacy.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());legacy.close();
 Check(saved.Restore("/tmp/fq-theft-v12.fqps",error)&&saved.Snapshot().karma==-5&&saved.IsCollected(3),"v12 karma and collection migration");
 std::ifstream ownedInput("/tmp/fq-ownership.fqps",std::ios::binary);
 bytes.assign(std::istreambuf_iterator<char>(ownedInput),{});
 // Two explicitly owned remaining rows, each 20 bytes. Invalid owner, valid CRC.
 put(bytes.size()-32,9999);put(16,static_cast<uint32_t>(crc32(0,bytes.data()+20,bytes.size()-20)));
 std::ofstream corrupt("/tmp/fq-ownership-corrupt.fqps",std::ios::binary);corrupt.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());corrupt.close();
 const auto restoredRevision=restored.Revision();const auto restoredKarma=restored.Snapshot().karma;
 Check(!restored.Restore("/tmp/fq-ownership-corrupt.fqps",error)&&restored.Revision()==restoredRevision&&restored.Snapshot().karma==restoredKarma,"corrupt ownership restore atomic");
 c=Fixture();c.references[20].locked=true;Player lock(c);Check(!lock.PrepareContainer(20),"locked container remains locked");
 c=Fixture();c.items[100].script=22;Player script(c);Check(!script.Pickup(3)&&script.Snapshot().karma==10,"script protection");
 c=Fixture();c.references[3].ownership.global=500;Player unresolved(c);Check(!unresolved.CanPickup(3),"unknown global permissions blocked");
 c=Fixture();c.items[100].playable=false;Player failed(c);Check(!failed.Pickup(3)&&!failed.IsCollected(3)&&failed.Snapshot().karma==10&&!failed.PollKarmaEvent(k),"failed action atomic");
 c=Fixture();c.references[5].owner=81;Player evil(c);Check(evil.Pickup(5)&&evil.Snapshot().karma==10,"evil faction no karma loss");
 c.references[5].owner=82;Player untracked(c);Check(untracked.Pickup(5)&&untracked.Snapshot().karma==5,"Track Crime is not a moral exemption");
 c=Fixture();c.pipboy.dialogueActors[90].factions[81]=0;Player individual(c);
 Check(individual.Pickup(3)&&individual.Snapshot().karma==5,"explicit NPC owner uses own karma, not faction flag");
 fo3pipdata::ResultScript reward;reward.source="RewardKarma 5\n";
 Player scripted(Fixture());Check(scripted.PreviewDialogueResult(reward,error)&&!scripted.PollKarmaEvent(k)&&scripted.Snapshot().karma==10,"preview feedback rollback");
 Check(scripted.ExecuteDialogueResult(reward,error)&&scripted.PollKarmaEvent(k)&&k.amount==5&&!scripted.PollKarmaEvent(k),"script reward one feedback event");
 reward.source="RewardKarma -5\nRemoveItem MissingForm 1\n";
 Check(!scripted.ExecuteDialogueResult(reward,error)&&scripted.Snapshot().karma==15&&!scripted.PollKarmaEvent(k),"failed script rolls back karma feedback");
}
void Original(const char* path){
 Catalog c;std::string error;const bool loaded=LoadCatalog(path,c,error);Check(loaded,error.c_str());
 Check(c.theftKarma==-5,"original fKarmaModStealing");
 Check(c.strings.at("sKarmaMinorLost")=="You've lost Karma!"&&c.strings.at("sKarmaMinorGained")=="You've gained Karma!","original notification wording");
 fo3audio::Catalog audio;Check(fo3audio::LoadCatalog(path,audio,error),error.c_str());
 Check(audio.sounds.at(audio.names.at("UIKarmaDown")).path=="sound/fx/ui/ui_karma_down.wav","original Karma loss sound");
 Check(audio.sounds.at(audio.names.at("UIKarmaUp")).path=="sound/fx/ui/ui_karma_up.wav","original Karma gain sound");
 Player p(std::move(c));size_t owned=0,unowned=0,inherited=0,ranked=0;
 for(const auto& ref:p.Definitions().references){auto o=p.ResolveOwnership(ref.first);
   if(p.CanPickup(ref.first)){if(o.Theft())++owned;else ++unowned;}
   if(o.inherited&&o.Theft())++inherited;
   if(o.data.rank>0)++ranked;
 }
 Check(owned>100&&unowned>100&&inherited>100,"original ownership census");
 uint32_t loose=0,container=0;
 for(const auto& ref:p.Definitions().references){
   auto cell=p.Definitions().cellOwnership.find(ref.second.cell);
   if(cell==p.Definitions().cellOwnership.end()||cell->second.editor.find("Megaton")==std::string::npos)continue;
   auto o=p.ResolveOwnership(ref.first);
   if(!loose&&p.CanPickup(ref.first)&&o.Theft()&&o.karmaEligible)loose=ref.first;
   if(!container&&p.CanLootContainer(ref.first)&&o.Theft()&&o.karmaEligible&&p.PrepareContainer(ref.first)&&!p.ContainerContents(ref.first)->empty())container=ref.first;
 }
 Check(loose&&container,"Megaton source-driven integration references");
 const auto before=p.Snapshot().karma;Check(p.Pickup(loose)&&p.Snapshot().karma==before-5&&!p.Pickup(loose),"Megaton original loose theft");
 p.BeginContainerInteraction(container);size_t takes=0;const auto start=p.Snapshot().karma;
 const auto contents=*p.ContainerContents(container);
 for(const auto& stack:contents)if(p.CanTakeContainerStack(container,stack.id)&&p.ContainerStackOwnership(container,stack.id).karmaEligible&&p.TakeContainerStack(container,stack.id))++takes;
 Check(takes&&p.Snapshot().karma==start-5,"Megaton container one penalty");
 Check(p.Save("/tmp/fq-original-theft.fqps",error),error.c_str());
 Check(p.Restore("/tmp/fq-original-theft.fqps",error)&&p.IsCollected(loose)&&p.Snapshot().karma==start-5,"original save/reload");
 std::cout<<"Original owned="<<owned<<" legal="<<unowned<<" inherited="<<inherited<<" ranked="<<ranked<<" Megaton loose="<<std::hex<<loose<<" container="<<container<<std::dec<<" takes="<<takes<<"\n";
}
int main(int argc,char**argv){try{Synthetic();if(argc>1)Original(argv[1]);std::cout<<"Ownership/theft tests passed\n";}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
