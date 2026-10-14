#include "world/interaction/fo3-door-resources.h"
#include "data/fo3-esm-reader.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
static bool failAlloc=false;
void* operator new(std::size_t n){if(failAlloc)throw std::bad_alloc();if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using Bytes=std::vector<uint8_t>;
void u32(Bytes& b,size_t p,uint32_t v){if(b.size()<p+4)b.resize(p+4);std::memcpy(b.data()+p,&v,4);}
void f32(Bytes& b,size_t p,float v){if(b.size()<p+4)b.resize(p+4);std::memcpy(b.data()+p,&v,4);}
Bytes word(uint32_t v){Bytes b(4);u32(b,0,v);return b;}
Bytes text(const std::string& s){Bytes b(s.begin(),s.end());b.push_back(0);return b;}
void sub(Bytes& b,const char* t,const Bytes& v){b.insert(b.end(),t,t+4);b.push_back(v.size()&255);b.push_back(v.size()>>8);b.insert(b.end(),v.begin(),v.end());}
void append(Bytes& b,const Bytes& v){b.insert(b.end(),v.begin(),v.end());}
Bytes record(const char* t,uint32_t id,const Bytes& payload,uint32_t flags=0){Bytes b(24);std::memcpy(b.data(),t,4);u32(b,4,payload.size());u32(b,8,flags);u32(b,12,id);append(b,payload);return b;}
Bytes group(uint32_t id,uint32_t type,const Bytes& payload){Bytes b(24);std::memcpy(b.data(),"GRUP",4);u32(b,4,payload.size()+24);u32(b,8,id);u32(b,12,type);append(b,payload);return b;}
Bytes named(const std::string& name){Bytes b;sub(b,"FULL",text(name));return b;}
Bytes ref(uint32_t base,uint32_t destination=0,uint32_t parent=0,uint8_t parentFlags=0){Bytes b;sub(b,"NAME",word(base));Bytes pos(24);f32(pos,4,-140);sub(b,"DATA",pos);if(destination){Bytes x(32);u32(x,0,destination);f32(x,4,100);f32(x,16,0.5f);sub(b,"XTEL",x);}if(parent){Bytes x(8);u32(x,0,parent);x[4]=parentFlags;sub(b,"XESP",x);}return b;}
const std::string path="/tmp/falloutquest-door-resources.esm";
Bytes fixture(int invalid=0){
 Bytes b;append(b,record("CELL",10,named("Source")));
 append(b,record("DOOR",20,named(invalid==8?"":"Vault \xe9 Door")));
 append(b,record("DOOR",21,named("Local")));
 Bytes world=record("CELL",11,named(invalid==7||invalid==8?"":"Destination"));
 append(world,group(11,6,record("REFR",90,ref(invalid==4?999:20))));
 append(b,record("WRLD",12,named(invalid==8?"":"Wasteland")));
 append(b,group(12,1,world));
 Bytes children;auto source=ref(20,invalid==1?999:90,invalid==2?999:invalid==3?30:0);
 if(invalid==5){source.clear();sub(source,"NAME",word(20));sub(source,"XTEL",Bytes(27));}
 if(invalid==6){source=ref(20,90);Bytes nan(24);f32(nan,0,std::numeric_limits<float>::quiet_NaN());sub(source,"DATA",nan);}
 if(invalid==10){source=ref(999,90);}
 if(invalid==11){source=ref(20,90,31,4);}
 if(invalid==12){source=ref(20,90);Bytes x(28);u32(x,0,90);f32(x,4,std::numeric_limits<float>::infinity());sub(source,"XTEL",x);}
 if(invalid==9)source.push_back(0); // Truncated subrecord header.
 append(children,record("REFR",30,source));
 append(children,record("REFR",31,ref(20,90),0x800));
 append(children,record("REFR",32,ref(20,90,31,2))); // Presentation bit does not invert.
 append(children,record("REFR",33,ref(20,90,31,1)));
 append(children,record("REFR",34,ref(21))); // Local swing door has no XTEL.
 append(b,group(10,6,group(10,9,children)));return b;
}
void write(const Bytes& b){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());}
bool ray(const Fo3DoorResources& b,Fo3DoorAimQ1700& aim){return QueryFo3DoorAnchorResources(b.cells.at(10).anchors,0,0,0,70,0,0,0,0,0,0,0,1,&aim);}
void synthetic(){
 write(fixture());std::atomic<bool> cancel{false};std::string error;
 fo3world::ReferenceStateSnapshot before,after;
 after.states[30]=fo3world::ReferenceEnableState::Disabled;
 after.states[31]=after.states[32]=fo3world::ReferenceEnableState::Enabled;
 after.states[33]=fo3world::ReferenceEnableState::Disabled;
 Fo3DoorResources old,candidate;old.prompts[777]="Other cell";old.cells[888].attempted=true;
 assert(PrepareFo3ResidentDoorResources(path,10,before,before,{},old,old,cancel,error));
 assert(old.cells.at(10).anchors.size()==2&&old.metadata.count(30)&&old.metadata.count(33));
 assert(!old.metadata.count(34)&&old.prompts.at(30)=="Open Vault \xe9 Door to Destination");
 assert(PrepareFo3ResidentDoorResources(path,10,before,after,{30,31,32,33},old,candidate,cancel,error));
 assert(candidate.cells.at(10).anchors.size()==2&&candidate.metadata.count(31)&&candidate.metadata.count(32));
 assert(!candidate.metadata.count(30)&&!candidate.prompts.count(33)&&candidate.prompts.at(777)=="Other cell"&&candidate.cells.count(888));
 auto& live=Fo3LiveDoorResources();MarkFo3DoorResourcesChanged();live=old;
 // No MODL is required for the authored ray fallback.
 Fo3DoorAimQ1700 aim;assert(ray(live,aim)&&aim.sourceDoorRef==30&&aim.x==100&&aim.rx==0.5f);
 const auto* buffer=live.cells.at(10).anchors.data();const auto* prompt=&live.prompts.at(30);
 auto revision=GetFo3DoorResourceRevision();
 assert(!PrepareFo3ResidentDoorPlan(revision+1,candidate));
 assert(!PrepareFo3ResidentDoorPlan(revision,live));
 failAlloc=true;bool threw=false;try{PrepareFo3ResidentDoorPlan(revision,candidate);}catch(const std::bad_alloc&){threw=true;}failAlloc=false;
 assert(threw&&!HasFo3ResidentDoorPlan()&&candidate.metadata.count(31)&&live.metadata.count(30));
 assert(PrepareFo3ResidentDoorPlan(revision,candidate)&&candidate.cells.empty());
 assert(CanCommitFo3ResidentDoorPlan());failAlloc=true;
 const bool committed=CommitFo3ResidentDoorPlan();failAlloc=false;assert(committed);
 assert(CanRollbackFo3ResidentDoorPlan()&&Fo3ResidentDoorPlanApplied()&&ray(live,aim)&&aim.sourceDoorRef==31);
 assert(!live.prompts.count(30)&&live.prompts.count(31));
 failAlloc=true;const bool rolled=RollbackFo3ResidentDoorPlan();failAlloc=false;assert(rolled);
 assert(live.cells.at(10).anchors.data()==buffer&&&live.prompts.at(30)==prompt);
 assert(ray(live,aim)&&aim.sourceDoorRef==30&&!CommitFo3ResidentDoorPlan());DiscardFo3ResidentDoorPlan();
 candidate=old;assert(PrepareFo3ResidentDoorPlan(GetFo3DoorResourceRevision(),candidate));
 MarkFo3DoorResourcesChanged();assert(!CanCommitFo3ResidentDoorPlan()&&!CommitFo3ResidentDoorPlan());DiscardFo3ResidentDoorPlan();
 candidate=old;assert(PrepareFo3ResidentDoorPlan(GetFo3DoorResourceRevision(),candidate)&&CommitFo3ResidentDoorPlan());
 MarkFo3DoorResourcesChanged();assert(!CanRollbackFo3ResidentDoorPlan()&&!RollbackFo3ResidentDoorPlan());DiscardFo3ResidentDoorPlan();
 Fo3DoorResources out;out.prompts[123]="sentinel";
 auto reject=[&](const std::vector<uint32_t>& affected,const Fo3DoorResources& captured){assert(!PrepareFo3ResidentDoorResources(path,10,before,after,affected,captured,out,cancel,error));assert(out.cells.empty()&&out.prompts.at(123)=="sentinel"&&!error.empty());};
 reject({},old);reject({30,30},old);reject({0},old);
 auto wrong=old;wrong.cells.at(10).anchors[0].sourceX+=1;reject({30,31,32,33},wrong);
 wrong=old;wrong.metadata.at(30).sourceBase=21;reject({30,31,32,33},wrong);
 wrong=old;wrong.prompts.at(30)="invented";reject({30,31,32,33},wrong);
 wrong=old;wrong.cells.at(10).anchors[1]=wrong.cells.at(10).anchors[0];reject({30,31,32,33},wrong);
 cancel=true;reject({30,31,32,33},old);cancel=false;
 for(int invalid:{1,2,3,4,5,6,8,9,10,11,12}){write(fixture(invalid));reject({30,31,32,33},{});}
 write(fixture(7));assert(PrepareFo3ResidentDoorResources(path,10,before,before,{}, {},out,cancel,error));assert(out.prompts.at(30)=="Open Vault \xe9 Door to Wasteland");
 std::string formatted="unchanged";assert(!FormatFo3DoorPrompt("","",formatted)&&formatted=="unchanged");
 assert(FormatFo3DoorPrompt("","Megaton",formatted)&&formatted=="Open to Megaton");
 assert(!QueryFo3DoorAnchorResources(old.cells.at(10).anchors,0,0,0,70,0,0,0,0,0,0,0,0,&aim)&&!aim.valid);
 assert(!QueryFo3DoorAnchorResources(old.cells.at(10).anchors,0,0,0,70,0,0,0,0,0,std::numeric_limits<float>::infinity(),0,1,&aim));
 std::remove(path.c_str());
}
void original(const std::string& esm){
 // Resolve the original CELL by EDID instead of assigning an invented form ID.
 FILE* file=std::fopen(esm.c_str(),"rb");assert(file);uint32_t cell=0;const auto size=fo3esm::FileSize(file);
 while(ftello(file)<size){uint8_t h[24];assert(fo3esm::ReadExact(file,h,24));const auto n=fo3esm::ReadU32(h+4);if(!std::memcmp(h,"GRUP",4))continue;
  if(!std::memcmp(h,"CELL",4)){std::vector<uint8_t> b;assert(fo3esm::ReadPayloadCurrent(file,n,fo3esm::ReadU32(h+8),b));fo3esm::WalkSubrecords(b,[&](const char* tag,const uint8_t* p,uint32_t len){if(!std::memcmp(tag,"EDID",4)&&fo3esm::ZString(p,len)=="MegatonTheBrassLantern")cell=fo3esm::ReadU32(h+12);});}
  else assert(!fseeko(file,n,SEEK_CUR));
 }
 std::fclose(file);assert(cell);
 std::atomic<bool> cancel{false};std::string error;Fo3DoorResources old,next;fo3world::ReferenceStateSnapshot before,after;
 const bool loaded=PrepareFo3ResidentDoorResources(esm,cell,before,before,{}, {},old,cancel,error);if(!loaded)std::cerr<<error<<'\n';assert(loaded);
 const auto count=old.cells.at(cell).anchors.size();assert(count);
 const auto anchor=old.cells.at(cell).anchors.front();after.states[anchor.sourceDoorRef]=fo3world::ReferenceEnableState::Disabled;
 assert(PrepareFo3ResidentDoorResources(esm,cell,before,after,{anchor.sourceDoorRef},old,next,cancel,error));
 assert(next.cells.at(cell).anchors.size()+1==count&&!next.prompts.count(anchor.sourceDoorRef));
 auto& live=Fo3LiveDoorResources();MarkFo3DoorResourcesChanged();live=std::move(old);const auto* data=live.cells.at(cell).anchors.data();
 assert(PrepareFo3ResidentDoorPlan(GetFo3DoorResourceRevision(),next));assert(CommitFo3ResidentDoorPlan());assert(RollbackFo3ResidentDoorPlan());
 assert(live.cells.at(cell).anchors.data()==data&&live.prompts.count(anchor.sourceDoorRef));DiscardFo3ResidentDoorPlan();
 std::cout<<"Original Brass Lantern load doors: "<<count<<" -> "<<count-1<<" -> "<<count<<"\n";
}
int main(int argc,char** argv){synthetic();if(argc>1)original(argv[1]);std::cout<<"Door resource tests passed\n";}
