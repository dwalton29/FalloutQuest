#include "scripting/fo3-script-master-match.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <zlib.h>
using Bytes=std::vector<uint8_t>;
using Execution=fo3player::ScriptExecution;
using Kind=fo3player::ScriptProgramKind;
static void Put(Bytes& b,size_t p,uint32_t value){if(b.size()<p+4)b.resize(p+4);std::memcpy(b.data()+p,&value,4);}
static Bytes Word(uint32_t value){Bytes b(4);Put(b,0,value);return b;}
static void Append(Bytes& b,const Bytes& v){b.insert(b.end(),v.begin(),v.end());}
static void Sub(Bytes& b,const char* tag,const Bytes& v){b.insert(b.end(),tag,tag+4);b.push_back(v.size()&255);b.push_back(v.size()>>8);Append(b,v);}
static Bytes Text(const std::string& s){Bytes b(s.begin(),s.end());b.push_back(0);return b;}
static Bytes Record(const char* tag,uint32_t id,Bytes payload,uint32_t flags=0){
  Bytes b(24);std::memcpy(b.data(),tag,4);Put(b,4,payload.size());Put(b,8,flags);Put(b,12,id);Append(b,payload);return b;
}
static Bytes Group(uint32_t topic,Bytes payload){Bytes b(24);std::memcpy(b.data(),"GRUP",4);Put(b,4,24+payload.size());Put(b,8,topic);Put(b,12,7);Append(b,payload);return b;}
static Bytes Compressed(const Bytes& b){
  uLongf n=compressBound(b.size());Bytes out(n+4);Put(out,0,b.size());
  assert(compress2(out.data()+4,&n,b.data(),b.size(),Z_BEST_SPEED)==Z_OK);out.resize(n+4);return out;
}
static void Script(Bytes& b,const std::string& source){
  Sub(b,"SCHR",Bytes(20));Sub(b,"SCDA",{1,2});Sub(b,"SCTX",Text(source));
  Sub(b,"SCRO",Word(100));Sub(b,"SCRV",Word(2));
}
struct Fixture {
  Bytes quest,info,event;std::vector<Execution> programs;
  Fixture(){
    Sub(quest,"DATA",{0,50});
    Sub(quest,"INDX",{10,0});Sub(quest,"QSDT",{0});Script(quest,"RewardKarma 1");
    Sub(info,"TPIC",Word(40));Script(info,"RewardKarma 2");Sub(info,"NEXT",{});Script(info,"RewardKarma 3");
    Sub(event,"EDID",Text("FixtureScript"));Script(event,"Begin OnActivate\nRewardKarma 4\nEnd\nBegin GameMode\nRewardKarma 5\nEnd");
    fo3pipdata::Definitions d;fo3pipdata::Decode(d,"QUST",10,0,quest,0,0,0);
    fo3pipdata::Decode(d,"INFO",50,0,info,0,0,40);fo3pipdata::Decode(d,"SCPT",60,0,event,0,0,0);
    Execution q;q.origin.kind=Kind::QuestStageItem;q.origin.record=q.owner=10;q.origin.stage=10;
    q.program=d.quests.at(10).stages.at(10).items[0].result;programs.push_back(q);
    for(unsigned phase=0;phase<2;++phase){Execution i;i.origin.kind=Kind::DialogueInfo;
      i.origin.record=50;i.origin.topic=40;i.origin.item=phase;i.owner=100;
      i.program=phase?d.topics.at(40)[0].end:d.topics.at(40)[0].begin;programs.push_back(i);}
    Execution e;e.origin.kind=Kind::ReferenceEvent;e.origin.record=e.localScript=60;
    e.origin.sourceLine=1;e.origin.event="onactivate";e.owner=100;e.activator=0x14;
    e.container=d.scripts.at(60);e.program.source=fo3script::ParseEvents(d.scripts.at(60)).blocks[0].body;programs.push_back(e);
  }
  Bytes Master(bool compressed=false,bool extended=false) const {
    auto q=quest;if(extended){Bytes pref;Sub(pref,"XXXX",Word(100));Sub(pref,"EDID",{});Append(pref,Bytes(100,'x'));Append(pref,q);q=std::move(pref);}
    auto b=Record("QUST",10,compressed?Compressed(q):q,compressed?0x40000:0);
    Append(b,Group(40,Record("INFO",50,info)));Append(b,Record("SCPT",60,event));return b;
  }
};
int main(){
  Fixture f;const auto path="/tmp/fq-script-master-"+std::to_string(getpid());std::atomic<bool> cancel{false};std::string error;
  auto write=[&](const Bytes& b){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),b.size());assert(out);};
  fo3script::ScriptMasterMatch report;assert(!report.Verified()&&!report.Matches(f.programs));
  for(bool compressed:{false,true})for(bool extended:{false,true}){
    write(f.Master(compressed,extended));const bool valid=fo3script::MatchScriptProgramsToMaster(path,f.programs,report,cancel,error);
    if(!valid)std::cerr<<"compressed="<<compressed<<" extended="<<extended<<" "<<error<<'\n';
    assert(valid);
    assert(error.empty()&&report.Verified()&&report.Matches(f.programs));
  }
  write(f.Master());
  auto rejects=[&](const std::vector<Execution>& programs){
    assert(!fo3script::MatchScriptProgramsToMaster(path,programs,report,cancel,error)&&!error.empty());
    assert(report.Matches(f.programs)); // failed attempts never replace valid output
  };
  for(unsigned n=0;n<f.programs.size();++n)for(int field=0;field<5;++field){
    auto programs=f.programs;auto& s=programs[n].program;
    if(field==0)s.source+=' ';
    if(field==1)s.compiled.push_back(9);
    if(field==2)s.header.push_back(9);
    if(field==3)s.references.push_back(9);
    if(field==4)s.records.push_back({"SCRV",Word(9)});
    assert(!report.Matches(programs));rejects(programs);
  }
  for(int field=0;field<12;++field){
    auto p=f.programs;
    if(field==0)p[0].origin.kind=Kind::UnboundResult;
    if(field==1)++p[0].origin.record;
    if(field==2)++p[0].origin.stage;
    if(field==3)++p[0].origin.item;
    if(field==4)++p[0].owner;
    if(field==5)++p[1].origin.topic;
    if(field==6)++p[1].origin.item;
    if(field==7)p[3].origin.event="gamemode";
    if(field==8)++p[3].origin.sourceLine;
    if(field==9)++p[3].localScript;
    if(field==10)p[3].container.records.clear();
    if(field==11)++p[0].localScript;
    rejects(p);
  }
  auto context=f.programs;++context[3].activator;
  assert(!report.Matches(context)); // context is bound, not authenticated by ESM
  auto reversed=f.programs;std::swap(reversed[0],reversed[1]);assert(!report.Matches(reversed));
  auto repeats=f.programs;repeats.push_back(repeats[0]);fo3script::ScriptMasterMatch repeated;
  assert(fo3script::MatchScriptProgramsToMaster(path,repeats,repeated,cancel,error)&&repeated.Matches(repeats));
  rejects({});cancel=true;rejects(f.programs);cancel=false;
  for(int field=0;field<5;++field){
    auto p=f.programs;auto& s=p[3].container;
    if(field==0)s.source+=' ';
    if(field==1)s.compiled.push_back(9);
    if(field==2)s.header.push_back(9);
    if(field==3)s.references.push_back(9);
    if(field==4)s.records[0].second.push_back(9);
    rejects(p);
  }
  auto overflow=std::vector<Execution>(4097,f.programs[0]);rejects(overflow);
  for(int malformed=0;malformed<12;++malformed){
    auto b=f.Master();
    if(malformed==0)Append(b,Record("QUST",10,f.quest)); // scan beyond first match
    if(malformed==1)Append(b,Record("STAT",10,{}));
    if(malformed==2)Put(b,8,0x20);
    if(malformed==3)Put(b,12,999);
    if(malformed==4)b.pop_back();
    if(malformed==5)b.push_back(1);
    if(malformed==6){auto q=f.quest;q.push_back(0);b=Record("QUST",10,q);}
    if(malformed==7){auto q=f.quest;Sub(q,"XXXX",Word(200));Sub(q,"SCTX",{});b=Record("QUST",10,q);}
    if(malformed==8){auto q=f.quest;Sub(q,"SCHR",Bytes(20));b=Record("QUST",10,q);}
    if(malformed==9){auto q=f.quest;Sub(q,"SCRO",{1,2});b=Record("QUST",10,q);}
    if(malformed==10){b=Group(40,f.Master());Put(b,4,b.size()+1);}
    if(malformed==11){b=Record("QUST",10,{1,2,3,4},0x40000);}
    write(b);rejects(f.programs);
  }
  // INFO can use its enclosing topic when TPIC is absent, but not another one.
  auto fallback=f.info;fallback.erase(fallback.begin(),fallback.begin()+10);
  auto b=Record("QUST",10,f.quest);Append(b,Group(40,Record("INFO",50,fallback)));Append(b,Record("SCPT",60,f.event));
  write(b);assert(fo3script::MatchScriptProgramsToMaster(path,f.programs,repeated,cancel,error));
  for(int malformed=0;malformed<3;++malformed){
    auto info=f.info;
    if(malformed==0)Sub(info,"NEXT",{});
    if(malformed==1)Sub(info,"TPIC",Word(40));
    if(malformed==2)Sub(info,"SCDA",{});
    b=Record("QUST",10,f.quest);Append(b,Group(40,Record("INFO",50,info)));Append(b,Record("SCPT",60,f.event));
    write(b);rejects(f.programs);
  }
  b=f.Master();for(unsigned depth=0;depth<128;++depth)b=Group(40,std::move(b));
  // The fixture already has a nested INFO group, so this exceeds the limit.
  write(b);rejects(f.programs);
  auto owned=std::move(repeated);assert(owned.Verified()&&owned.Matches(f.programs));
  assert(!repeated.Verified()&&!repeated.Matches({}));
  std::remove(path.c_str());rejects(f.programs);
  std::cout<<"Strict master program matching, exact report binding and fail-closed framing passed\n";
}
