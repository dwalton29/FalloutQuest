#include "scripting/fo3-script-master-match.h"
#include "scripting/fo3-script-evidence-budget.h"
#include "world/fo3-resident-authored-admission.h"
#include <cassert>
#include <algorithm>
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
static Bytes InfoPayload(const Bytes& metadata){
  Bytes b;Sub(b,"TPIC",Word(40));Sub(b,"QSTI",Word(20));Append(b,metadata);
  Script(b,"RewardKarma 2");Sub(b,"NEXT",{});Script(b,"RewardKarma 3");return b;
}
static Bytes ConditionBytes(unsigned size,uint32_t value){
  Bytes b(size);b[0]=1;b[8]=72;Put(b,4,value);Put(b,12,7);Put(b,16,8);
  if(size>=24)Put(b,20,1);
  if(size>=28)Put(b,24,0x14);
  return b;
}
static void Local(Bytes& b,uint32_t index,const std::string& name,bool extended=false){
  Bytes data(24);Put(data,0,index);Sub(b,"SLSD",data);
  if(extended){Sub(b,"XXXX",Word(name.size()+1));Sub(b,"SCVR",{});Append(b,Text(name));}
  else Sub(b,"SCVR",Text(name));
}
static Bytes Owner(bool compressed=false){
  Bytes base,ref;Sub(base,"SCRI",Word(60));Sub(ref,"NAME",Word(201));
  auto b=Record("ACTI",201,compressed?Compressed(base):base,compressed?0x40000:0);
  Append(b,Record("REFR",101,ref));return b;
}
static Bytes ParentQuest(bool compressed=false,uint32_t id=20){
  Bytes quest;Sub(quest,"DATA",{0,40});
  return Record("QUST",id,compressed?Compressed(quest):quest,compressed?0x40000:0);
}
static Bytes Speakers(bool compressed=false,bool includeQuest=true){
  Bytes base,ref;Sub(base,"EDID",Text("FixtureActor"));Sub(ref,"NAME",Word(202));
  auto b=Record("NPC_",202,compressed?Compressed(base):base,compressed?0x40000:0);
  Append(b,Record("ACHR",102,ref));
  if(includeQuest)Append(b,ParentQuest(compressed));
  return b;
}
struct Fixture {
  Bytes quest,info,event;std::vector<Execution> programs;
  Fixture(){
    Sub(quest,"DATA",{0,50});
    Sub(quest,"INDX",{10,0});Sub(quest,"QSDT",{0});Script(quest,"RewardKarma 1");
    Bytes metadata;Sub(metadata,"DATA",{0,0,0,0});Sub(metadata,"TRDT",Bytes(24));
    info=InfoPayload(metadata);
    Sub(event,"EDID",Text("FixtureScript"));Script(event,"Begin OnActivate\nRewardKarma 4\nEnd\nBegin GameMode\nRewardKarma 5\nEnd");
    fo3pipdata::Definitions d;fo3pipdata::Decode(d,"QUST",10,0,quest,0,0,0);
    fo3pipdata::Decode(d,"INFO",50,0,info,0,0,40);fo3pipdata::Decode(d,"SCPT",60,0,event,0,0,0);
    Execution q;q.origin.kind=Kind::QuestStageItem;q.origin.record=q.owner=10;q.origin.stage=10;
    q.program=d.quests.at(10).stages.at(10).items[0].result;programs.push_back(q);
    for(unsigned phase=0;phase<2;++phase){Execution i;i.origin.kind=Kind::DialogueInfo;
      i.origin.record=50;i.origin.topic=40;i.origin.item=phase;i.owner=102;
      i.owningQuest=20;
      i.program=phase?d.topics.at(40)[0].end:d.topics.at(40)[0].begin;programs.push_back(i);}
    Execution e;e.origin.kind=Kind::ReferenceEvent;e.origin.record=e.localScript=60;
    e.origin.sourceLine=1;e.origin.event="onactivate";e.owner=e.reference=101;e.activator=0x14;
    e.container=d.scripts.at(60);e.program.source=fo3script::ParseEvents(d.scripts.at(60)).blocks[0].body;programs.push_back(e);
  }
  Bytes Master(bool compressed=false,bool extended=false) const {
    auto q=quest;if(extended){Bytes pref;Sub(pref,"XXXX",Word(100));Sub(pref,"EDID",{});Append(pref,Bytes(100,'x'));Append(pref,q);q=std::move(pref);}
    auto b=Record("QUST",10,compressed?Compressed(q):q,compressed?0x40000:0);
    Append(b,Group(40,Record("INFO",50,info)));Append(b,Record("SCPT",60,event));Append(b,Owner(compressed));Append(b,Speakers(compressed));return b;
  }
};
int main(){
  // Exact-fit accounting, every retained field, and failure atomicity. These
  // checks do not allocate a second copy of oversized evidence.
  {
    fo3pipdata::ResultScript empty;size_t cost=0;
    assert(fo3script::ChargeScriptEvidence("",empty,nullptr,cost));
    size_t explicitEmpty=0;assert(fo3script::ChargeScriptEvidence("",empty,&empty,explicitEmpty)&&explicitEmpty==cost);
    size_t used=fo3script::ScriptEvidenceMaxBytes-cost;
    assert(fo3script::ChargeScriptEvidence("",empty,nullptr,used)&&used==fo3script::ScriptEvidenceMaxBytes);
    assert(!fo3script::ChargeScriptEvidence("",empty,nullptr,used)&&used==fo3script::ScriptEvidenceMaxBytes);
    used=SIZE_MAX;assert(!fo3script::ChargeScriptEvidence("",empty,nullptr,used)&&used==SIZE_MAX);
    for(unsigned field=0;field<7;++field){
      fo3pipdata::ResultScript script;std::string event;
      if(field==0)event="x";
      if(field==1)script.source="x";
      if(field==2)script.compiled={1};
      if(field==3)script.header={1};
      if(field==4)script.references={1};
      if(field==5)script.records={{"SCRO",{}}};
      if(field==6)script.records={{"",{1}}};
      size_t charged=0;assert(fo3script::ChargeScriptEvidence(event,script,nullptr,charged)&&charged>cost);
      used=fo3script::ScriptEvidenceMaxBytes-cost;
      assert(!fo3script::ChargeScriptEvidence(event,script,nullptr,used)&&used==fo3script::ScriptEvidenceMaxBytes-cost);
      size_t containerCost=0;assert(fo3script::ChargeScriptEvidence("",empty,&script,containerCost));
      if(field)assert(containerCost==charged);
    }
    std::vector<fo3pipdata::Condition> gates(1);
    for(bool quest:{false,true}){
      size_t charged=0;
      assert(fo3script::ChargeScriptEvidence("",empty,nullptr,charged,quest?nullptr:&gates,quest?&gates:nullptr));
      assert(charged==cost+sizeof(fo3pipdata::Condition));
      used=fo3script::ScriptEvidenceMaxBytes-cost;
      assert(!fo3script::ChargeScriptEvidence("",empty,nullptr,used,quest?nullptr:&gates,quest?&gates:nullptr));
      assert(used==fo3script::ScriptEvidenceMaxBytes-cost);
      used=fo3script::ScriptEvidenceMaxBytes-charged;
      assert(fo3script::ChargeScriptEvidence("",empty,nullptr,used,quest?nullptr:&gates,quest?&gates:nullptr));
      assert(used==fo3script::ScriptEvidenceMaxBytes);
    }
    std::vector<std::pair<uint32_t,std::string>> locals={{2,"Example"}};
    const auto localCost=sizeof(locals[0])+locals[0].second.size()+1;
    used=fo3script::ScriptEvidenceMaxBytes-localCost;
    assert(fo3script::ChargeScriptLocals(locals,used)&&used==fo3script::ScriptEvidenceMaxBytes);
    used=fo3script::ScriptEvidenceMaxBytes-localCost+1;
    assert(!fo3script::ChargeScriptLocals(locals,used)&&used==fo3script::ScriptEvidenceMaxBytes-localCost+1);
    std::unordered_map<uint32_t,std::string> map={{2,"Example"}};
    used=0;assert(fo3script::ChargeScriptLocals(map,used)&&used==localCost);
    used=fo3script::ScriptEvidenceMaxBytes-localCost;
    assert(fo3script::ChargeScriptForms(locals,used)&&used==fo3script::ScriptEvidenceMaxBytes);
    used=fo3script::ScriptEvidenceMaxBytes-localCost+1;
    assert(!fo3script::ChargeScriptForms(locals,used)&&used==fo3script::ScriptEvidenceMaxBytes-localCost+1);
  }
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
  // Bind decoded conditions, including float bits and order, to each entered
  // item/INFO and its original parent quest. This does not evaluate the gates.
  for(unsigned width:{20u,24u,28u})for(bool compressed:{false,true})for(bool parentFirst:{false,true}){
    auto quest=f.quest,info=f.info;Bytes parent;Sub(parent,"DATA",{0,40});
    for(uint32_t bits:{0x80000000u,0x7fc12345u}){
      const auto condition=ConditionBytes(width,bits);
      Sub(quest,"CTDA",condition);Sub(info,"CTDA",condition);Sub(parent,"CTDA",condition);
    }
    const auto parentRecord=Record("QUST",20,compressed?Compressed(parent):parent,compressed?0x40000:0);
    auto bytes=parentFirst?parentRecord:Bytes{};
    Append(bytes,Record("QUST",10,compressed?Compressed(quest):quest,compressed?0x40000:0));
    Append(bytes,Group(40,Record("INFO",50,compressed?Compressed(info):info,compressed?0x40000:0)));
    if(!parentFirst)Append(bytes,parentRecord);
    Append(bytes,Speakers(false,false));write(bytes);
    auto evidence=std::vector<Execution>{f.programs[0],f.programs[1],f.programs[2]};
    for(uint32_t bits:{0x80000000u,0x7fc12345u}){
      fo3pipdata::Condition c;c.flags=1;c.function=72;std::memcpy(&c.value,&bits,4);
      c.a=7;c.b=8;c.run=width>=24?1:0;c.reference=width>=28?0x14:0;
      for(auto& entry:evidence)entry.conditions.push_back(c);
      evidence[1].questConditions.push_back(c);evidence[2].questConditions.push_back(c);
    }
    fo3script::ScriptMasterMatch gates;
    assert(fo3script::MatchScriptProgramsToMaster(path,evidence,gates,cancel,error)&&gates.Matches(evidence));
    for(unsigned entry=0;entry<3;++entry)for(bool parentGate:{false,true})for(unsigned field=0;field<10;++field){
      if(!entry&&parentGate)continue;
      auto altered=evidence;auto& list=parentGate?altered[entry].questConditions:altered[entry].conditions;
      auto& c=list[0];
      if(field==0)c.flags^=1;
      if(field==1)++c.function;
      if(field==2){uint32_t bits=0;std::memcpy(&c.value,&bits,4);} // -0 differs from +0
      if(field==3)++c.a;
      if(field==4)++c.b;
      if(field==5)++c.run;
      if(field==6)++c.reference;
      if(field==7)list.clear();
      if(field==8)list.push_back(list[0]);
      if(field==9)std::swap(list[0],list[1]);
      assert(!gates.Matches(altered));
      assert(!fo3script::MatchScriptProgramsToMaster(path,altered,gates,cancel,error)&&gates.Matches(evidence));
    }
    auto wrongQuest=evidence;wrongQuest[1].owningQuest=21;
    assert(!gates.Matches(wrongQuest)&&!fo3script::MatchScriptProgramsToMaster(path,wrongQuest,gates,cancel,error));
    assert(gates.Matches(evidence));
    auto nan=evidence;uint32_t bits=0x7fc12346;std::memcpy(&nan[2].questConditions[1].value,&bits,4);
    assert(!gates.Matches(nan)&&!fo3script::MatchScriptProgramsToMaster(path,nan,gates,cancel,error));
    assert(gates.Matches(evidence));
  }
  write(f.Master());
  for(unsigned n=0;n<f.programs.size();++n)for(int field=0;field<5;++field){
    auto programs=f.programs;auto& s=programs[n].program;
    if(field==0)s.source+=' ';
    if(field==1)s.compiled.push_back(9);
    if(field==2)s.header.push_back(9);
    if(field==3)s.references.push_back(9);
    if(field==4)s.records.push_back({"SCRV",Word(9)});
    assert(!report.Matches(programs));rejects(programs);
  }
  for(int field=0;field<26;++field){
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
    if(field==12)++p[3].reference;
    if(field==13)p[3].parent=0;
    if(field==14)p[0].parent=0;
    if(field==15)p[0].reference=101;
    if(field==16)p[1].activator=0x14;
    if(field==17)p[1].parent=0;
    if(field==18)p[3].conditions.resize(1);
    if(field==19)p[3].questConditions.resize(1);
    if(field==20)p[0].questConditions.resize(1);
    if(field==21)p[0].owningQuest=10;
    if(field==22)p[3].owningQuest=20;
    if(field==23)p[1].owningQuest=0;
    if(field==24)p[0].localVariables={{1,"Unexpected"}};
    if(field==25)p[1].localVariables={{1,"Unexpected"}};
    rejects(p);
  }
  auto context=f.programs;++context[3].activator;
  assert(!report.Matches(context)); // context is bound, not authenticated by ESM
  auto reversed=f.programs;std::swap(reversed[0],reversed[1]);assert(!report.Matches(reversed));
  auto repeats=f.programs;repeats.push_back(repeats[0]);fo3script::ScriptMasterMatch repeated;
  assert(fo3script::MatchScriptProgramsToMaster(path,repeats,repeated,cancel,error)&&repeated.Matches(repeats));
  // Captured entry order is a preorder tree. Children are stage fragments and
  // inherit the implicit reference/activator of an active parent call.
  auto child=f.programs[0];child.parent=0;child.reference=101;child.activator=0x14;
  auto grandchild=child;grandchild.parent=1;
  auto sibling=child;
  auto siblingChild=child;siblingChild.parent=3;
  std::vector<Execution> tree={f.programs.back(),child,grandchild,sibling,siblingChild,f.programs[1]};
  fo3script::ScriptMasterMatch treeReport;
  assert(fo3script::MatchScriptProgramsToMaster(path,tree,treeReport,cancel,error));
  for(int malformed=0;malformed<7;++malformed){
    auto invalid=tree;
    if(malformed==0)invalid[1].reference=0;
    if(malformed==1)invalid[2].activator=0;
    if(malformed==2)invalid[1].parent=2; // forward edge
    if(malformed==3)invalid[1].parent=1; // self edge
    if(malformed==4)invalid[4].parent=2; // returned subtree
    if(malformed==5)invalid[1].parent=Execution::NoParent; // leaks inherited context into a root
    if(malformed==6){invalid.push_back(child);invalid.back().parent=0;} // closed by INFO root
    assert(!treeReport.Matches(invalid));rejects(invalid);
  }
  auto deep=std::vector<Execution>{f.programs.back()};
  for(unsigned n=1;n<32;++n){auto entry=child;entry.parent=n-1;deep.push_back(entry);}
  assert(fo3script::MatchScriptProgramsToMaster(path,deep,treeReport,cancel,error));
  deep.push_back(child);deep.back().parent=31;rejects(deep);
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
  auto tooLarge=f.programs;tooLarge.back().container.source.resize(fo3script::ScriptEvidenceMaxBytes);
  rejects(tooLarge);assert(error=="script master evidence byte limit exceeded");
  for(bool parentGate:{false,true}){
    auto large=f.programs;auto& gates=parentGate?large[1].questConditions:large[1].conditions;
    gates.resize(fo3script::ScriptEvidenceMaxBytes/sizeof(fo3pipdata::Condition));
    rejects(large);assert(error=="script master evidence byte limit exceeded");
  }
  {
    auto large=f.programs;large.back().localVariables={{1,std::string(fo3script::ScriptEvidenceMaxBytes,'x')}};
    assert(!fo3script::MatchScriptProgramsToMaster(path+"-missing",large,report,cancel,error));
    assert(error=="script master evidence byte limit exceeded"&&report.Matches(f.programs));
  }
  // Strict scope metadata cannot inherit the tolerant decoder's first-field
  // semantics. Scalar uniqueness is record-wide, including across fragments.
  for(const char* field:{"SCRI","ANAM","QSTI"})for(unsigned n:{0u,1u,3u,5u,8u}){
    const bool quest=std::strcmp(field,"SCRI")==0;auto payload=quest?f.quest:f.info;
    Sub(payload,field,Bytes(n));
    auto bytes=quest?Record("QUST",10,payload):Group(40,Record("INFO",50,payload));
    if(!quest)Append(bytes,Speakers());
    write(bytes);
    rejects(quest?std::vector<Execution>{f.programs[0]}:std::vector<Execution>{f.programs[1]});
  }
  for(const char* field:{"SCRI","ANAM","QSTI"}){
    const bool quest=std::strcmp(field,"SCRI")==0;Bytes payload;
    Sub(payload,field,Word(0));Append(payload,quest?f.quest:f.info);Sub(payload,field,Word(0));
    auto bytes=quest?Record("QUST",10,payload):Group(40,Record("INFO",50,payload));
    if(!quest)Append(bytes,Speakers());
    write(bytes);
    rejects(quest?std::vector<Execution>{f.programs[0]}:std::vector<Execution>{f.programs[1]});
  }
  for(bool quest:{false,true})for(unsigned n:{0u,1u,19u,20u,21u,23u,24u,25u,27u,28u,29u}){
    // Prefix the QUST condition before INDX so it is an ordinary root CTDA.
    Bytes payload;Sub(payload,"CTDA",Bytes(n));Append(payload,quest?f.quest:f.info);
    auto bytes=quest?Record("QUST",10,payload):Group(40,Record("INFO",50,payload));
    if(!quest)Append(bytes,Speakers());
    write(bytes);
    auto evidence=quest?std::vector<Execution>{f.programs[0]}:std::vector<Execution>{f.programs[1]};
    fo3script::ScriptMasterMatch conditions;
    const bool expected=n==20||n==24||n==28;
    if(expected&&!quest)evidence[0].conditions.resize(1);
    assert(fo3script::MatchScriptProgramsToMaster(path,evidence,conditions,cancel,error)==expected);
  }
  // Supplied command bindings authenticate their target EDIDs independently of
  // program kind, file order, compression and repeated lookups. This is not an
  // independent proof that the compiler supplied every lookup.
  for(bool expression:{false,true})for(bool first:{false,true})for(bool compressed:{false,true})for(bool extended:{false,true}){
    auto evidence=f.programs;
    (expression?evidence[0].expressionForms:evidence[0].commandForms)={{700,"namedtarget"},{701,"other"},{700,"namedtarget"}};
    (expression?evidence.back().commandForms:evidence.back().expressionForms)={{701,"other"}};
    auto named=[&](const Bytes& payload,uint32_t id=700,uint32_t flags=0){return Record("STAT",id,compressed?Compressed(payload):payload,flags|(compressed?0x40000:0));};
    Bytes target,other;Sub(other,"EDID",Text("Other"));
    if(extended){Sub(target,"XXXX",Word(12));Sub(target,"EDID",{});Append(target,Text("NamedTarget"));}
    else Sub(target,"EDID",Text("NamedTarget"));
    auto bindings=named(target);Append(bindings,named(other,701));
    auto good=first?bindings:f.Master();Append(good,first?f.Master():bindings);write(good);
    fo3script::ScriptMasterMatch bound;
    assert(fo3script::MatchScriptProgramsToMaster(path,evidence,bound,cancel,error)&&bound.Matches(evidence));
    for(unsigned change=0;change<5;++change){
      auto bad=evidence;
      auto& bindings=expression?bad[0].expressionForms:bad[0].commandForms;
      if(change==0)bindings[0].first=702;
      if(change==1)bindings[0].second="other";
      if(change==2)bindings.pop_back();
      if(change==3)std::swap(bindings[0],bindings[1]);
      if(change==4)(expression?bad.back().commandForms:bad.back().expressionForms).clear();
      assert(!bound.Matches(bad));
      if(change<2)assert(!fo3script::MatchScriptProgramsToMaster(path,bad,bound,cancel,error)&&bound.Matches(evidence));
    }
    for(unsigned malformed=0;malformed<10;++malformed){
      Bytes payload;
      if(malformed==0){} // no EDID
      if(malformed==1)Sub(payload,"EDID",{});
      if(malformed==2)Sub(payload,"EDID",Bytes{'N','a','m','e','d','T','a','r','g','e','t'});
      if(malformed==3)Sub(payload,"EDID",Bytes{'N',0,'X',0});
      if(malformed==4){Sub(payload,"EDID",Text("NamedTarget"));Sub(payload,"EDID",Text("NamedTarget"));}
      if(malformed==5){Sub(payload,"XXXX",Word(100));Sub(payload,"EDID",Text("NamedTarget"));}
      if(malformed>=6)payload=target;
      auto bad=f.Master();if(malformed!=6)Append(bad,named(payload,700,malformed==7?0x20:0));
      Append(bad,named(other,701));
      if(malformed==8)Append(bad,named(target));
      if(malformed==9)Append(bad,Record("MISC",700,{}));
      write(bad);assert(!fo3script::MatchScriptProgramsToMaster(path,evidence,bound,cancel,error)&&bound.Matches(evidence));
    }
  }
  {
    auto evidence=f.programs;auto quest=f.quest;Sub(quest,"EDID",Text("QuestIdentity"));
    evidence[0].commandForms={{10,"questidentity"}};evidence[0].expressionForms={{60,"fixturescript"}};
    auto bytes=Record("QUST",10,quest);Append(bytes,Group(40,Record("INFO",50,f.info)));
    Append(bytes,Record("SCPT",60,f.event));Append(bytes,Owner());Append(bytes,Speakers());write(bytes);
    fo3script::ScriptMasterMatch shared;
    assert(fo3script::MatchScriptProgramsToMaster(path,evidence,shared,cancel,error)&&shared.Matches(evidence));
  }
  for(bool expression:{false,true})for(unsigned invalid=0;invalid<5;++invalid){
    auto evidence=f.programs;auto& bindings=expression?evidence[0].expressionForms:evidence[0].commandForms;bindings={{700,"namedtarget"}};
    if(invalid==0)bindings[0].first=0;
    if(invalid==1)bindings[0].second.clear();
    if(invalid==2)bindings[0].second="NamedTarget";
    if(invalid==3)bindings[0].second=std::string("name\0tail",9);
    if(invalid==4)bindings[0].second.resize(fo3script::ScriptEvidenceMaxBytes,'x');
    assert(!fo3script::MatchScriptProgramsToMaster(path+"-missing",evidence,report,cancel,error)&&report.Matches(f.programs));
    assert(error==(invalid==4?"script master evidence byte limit exceeded":"malformed script form binding"));
  }
  // Null and nonnull authored SCRI preserve exact quest local-scope matching.
  for(uint32_t script:{0u,60u}){
    auto payload=f.quest;Sub(payload,"SCRI",Word(script));auto bytes=Record("QUST",10,payload);
    if(script)Append(bytes,Record("SCPT",60,f.event));
    write(bytes);
    auto evidence=std::vector<Execution>{f.programs[0]};evidence[0].localScript=script;
    if(script)evidence[0].container=f.programs.back().container;
    fo3script::ScriptMasterMatch scoped;
    assert(fo3script::MatchScriptProgramsToMaster(path,evidence,scoped,cancel,error));
    evidence[0].localScript=script?0:60;
    assert(!fo3script::MatchScriptProgramsToMaster(path,evidence,scoped,cancel,error));
  }
  // Quest-only evidence needs a second scan for its SCRI declarations even
  // without any placed owner. Named locals bind by index, never map order.
  for(bool first:{false,true})for(bool compressed:{false,true})for(bool extended:{false,true}){
    auto quest=f.quest;Sub(quest,"SCRI",Word(61));Bytes local;
    Script(local,"short Alpha\nfloat Zulu");
    Local(local,9,"Alpha",extended);Local(local,2,"Zulu",extended);
    const auto script=Record("SCPT",61,compressed?Compressed(local):local,compressed?0x40000:0);
    const auto parent=Record("QUST",10,quest);
    auto bytes=first?script:parent;Append(bytes,first?parent:script);write(bytes);
    auto evidence=std::vector<Execution>{f.programs[0]};evidence[0].localScript=61;
    fo3pipdata::Definitions d;fo3pipdata::Decode(d,"SCPT",61,0,local,0,0,0);
    evidence[0].container=d.scripts.at(61);
    evidence[0].localVariables={{2,"Zulu"},{9,"Alpha"}};
    fo3script::ScriptMasterMatch locals;
    assert(fo3script::MatchScriptProgramsToMaster(path,evidence,locals,cancel,error)&&locals.Matches(evidence));
    for(unsigned field=0;field<9;++field){
      auto altered=evidence;auto& variables=altered[0].localVariables;
      if(field==0)variables[0].second+="x";
      if(field==1)++variables[0].first;
      if(field==2)variables.clear();
      if(field==3)variables.push_back({10,"Extra"});
      if(field==4)std::swap(variables[0],variables[1]);
      if(field==5)variables[1].first=2;
      if(field==6)altered[0].localScript=62;
      if(field==7)altered[0].container.source+='x';
      if(field==8)altered[0].container.records.push_back({"SCVR",Text("Extra")});
      assert(!locals.Matches(altered)&&!fo3script::MatchScriptProgramsToMaster(path,altered,locals,cancel,error));
      assert(locals.Matches(evidence));
    }
    for(unsigned malformed=0;malformed<6;++malformed){
      auto bad=parent;
      if(malformed!=0)Append(bad,Record(malformed==1?"STAT":"SCPT",61,malformed==4?Bytes{1,2,3}:local,
        malformed==2?0x20:malformed==4?0x40000:0));
      if(malformed==3)Append(bad,script);
      if(malformed==5)Append(bad,Record("STAT",61,{}));
      write(bad);
      assert(!fo3script::MatchScriptProgramsToMaster(path,evidence,locals,cancel,error)&&locals.Matches(evidence));
    }
  }
  {
    auto script=f.event;Local(script,2,"Original");
    fo3pipdata::Definitions d;fo3pipdata::Decode(d,"SCPT",60,0,script,0,0,0);
    auto event=f.programs.back();event.container=d.scripts.at(60);event.localVariables={{2,"Original"}};
    auto bytes=Owner();Append(bytes,Record("SCPT",60,script));write(bytes);
    fo3script::ScriptMasterMatch locals;
    assert(fo3script::MatchScriptProgramsToMaster(path,{event},locals,cancel,error));
    auto altered=event;altered.localVariables[0].second="Forged";
    assert(!locals.Matches({altered})&&!fo3script::MatchScriptProgramsToMaster(path,{altered},locals,cancel,error));
    assert(locals.Matches({event})); // same raw container, changed decoded lookup
  }
  // Exact raw/decoded declarations still cannot authenticate an ambiguous
  // case-insensitive lookup. Test both local-scope entry kinds and raw layouts.
  for(bool event:{false,true})for(bool compressed:{false,true})for(bool extended:{false,true})
      for(unsigned kind=0;kind<4;++kind){
    auto script=f.event;Local(script,2,kind==2?"":"Count",extended);
    Local(script,9,kind==0?"Count":kind==1?"COUNT":"Counter",extended);
    fo3pipdata::Definitions d;fo3pipdata::Decode(d,"SCPT",60,0,script,0,0,0);
    auto entry=event?f.programs.back():f.programs[0];entry.localScript=60;entry.container=d.scripts.at(60);
    entry.localVariables={{2,kind==2?"":"Count"},{9,kind==0?"Count":kind==1?"COUNT":"Counter"}};
    auto quest=f.quest;Sub(quest,"SCRI",Word(60));
    auto bytes=event?Owner():Record("QUST",10,quest);
    Append(bytes,Record("SCPT",60,compressed?Compressed(script):script,compressed?0x40000:0));write(bytes);
    fo3script::ScriptMasterMatch locals;
    const bool valid=fo3script::MatchScriptProgramsToMaster(path,{entry},locals,cancel,error);
    assert(valid==(kind==3));
    if(kind!=3){
      assert(error=="local names are empty or ambiguous");
      assert(!fo3script::MatchScriptProgramsToMaster(path+"-missing",{entry},report,cancel,error));
      assert(error=="local names are empty or ambiguous"&&report.Matches(f.programs));
    }else assert(locals.Matches({entry}));
  }
  // Repeated SLSD indexes occur in the original master. Preserve decoder
  // last-assignment semantics; policy applies to its effective variable table.
  {
    auto script=f.event;Local(script,2,"Replaced");Local(script,2,"Current");Local(script,9,"Other");
    fo3pipdata::Definitions d;fo3pipdata::Decode(d,"SCPT",60,0,script,0,0,0);
    auto entry=f.programs.back();entry.container=d.scripts.at(60);entry.localVariables={{2,"Current"},{9,"Other"}};
    auto bytes=Owner();Append(bytes,Record("SCPT",60,script));write(bytes);
    fo3script::ScriptMasterMatch locals;
    assert(fo3script::MatchScriptProgramsToMaster(path,{entry},locals,cancel,error)&&locals.Matches({entry}));
  }
  // Tolerant decoding must not authenticate orphan/stale slot associations or
  // truncated/hidden-tail names, even when raw and decoded evidence agree.
  for(bool event:{false,true})for(bool compressed:{false,true})for(unsigned kind=0;kind<11;++kind){
    auto script=f.event;Bytes slot(24);Put(slot,0,2);
    if(kind==0)Sub(script,"SCVR",Text("Count")); // no preceding slot
    else if(kind==1){Sub(script,"SLSD",slot);Sub(script,"SLSD",slot);Sub(script,"SCVR",Text("Count"));}
    else if(kind==2)Sub(script,"SLSD",slot); // no name
    else if(kind>=3&&kind<=6){Sub(script,"SLSD",Bytes(kind==3?0:kind==4?4:kind==5?23:25));Sub(script,"SCVR",Text("Count"));}
    else {
      Sub(script,"SLSD",slot);
      Sub(script,"SCVR",kind==7?Bytes{}:kind==8?Bytes{'C','o','u','n','t'}:
        kind==9?Bytes{'C','o','u','n','t',0,'X',0}:Text("Count"));
      if(kind==10)Sub(script,"SCVR",Text("Again"));
    }
    fo3pipdata::Definitions d;fo3pipdata::Decode(d,"SCPT",60,0,script,0,0,0);
    auto entry=event?f.programs.back():f.programs[0];entry.localScript=60;entry.container=d.scripts.at(60);
    for(const auto& v:d.scripts.at(60).variables)entry.localVariables.push_back(v);
    std::sort(entry.localVariables.begin(),entry.localVariables.end());
    assert(!fo3script::MatchScriptProgramsToMaster(path+"-missing",{entry},report,cancel,error));
    assert(error=="malformed local records"&&report.Matches(f.programs));
    // A structurally valid retained container cannot conceal malformed disk
    // payloads: pass one (event) and pass two (quest SCRI) both validate framing.
    auto quest=f.quest;Sub(quest,"SCRI",Word(60));auto bytes=event?Owner():Record("QUST",10,quest);
    Append(bytes,Record("SCPT",60,compressed?Compressed(script):script,compressed?0x40000:0));write(bytes);
    entry.container=f.programs.back().container;entry.localVariables.clear();
    assert(!fo3script::MatchScriptProgramsToMaster(path,{entry},report,cancel,error));
    assert(error==(event?"malformed script master payload":"malformed local script payload")&&report.Matches(f.programs));
  }
  // Exact master identity cannot admit missing source-to-slot bindings or a
  // declaration classified as both reference and numeric before Begin.
  for(bool event:{false,true})for(bool compressed:{false,true})for(unsigned kind=0;kind<8;++kind){
    const std::string headers[]={"short Missing\n","ref Count\nfloat COUNT\n","float Count\nref count\n",
      "short\n","SHORT count ; comment\r\nfloat COUNT\n","ref COUNT\nref count\n",
      "scn Example\n; short Missing\nshort cOuNt\nBegin GameMode\nshort Missing\nEnd\n",""};
    Bytes script;Script(script,headers[kind]);Local(script,2,"Count");Local(script,9,"Extra");
    fo3pipdata::Definitions d;fo3pipdata::Decode(d,"SCPT",60,0,script,0,0,0);
    auto entry=event?f.programs.back():f.programs[0];entry.localScript=60;entry.container=d.scripts.at(60);
    entry.localVariables={{2,"Count"},{9,"Extra"}};
    if(event){
      entry.container.source=headers[kind]+"\nBegin OnActivate\nRewardKarma 4\nEnd";
      script.clear();Script(script,entry.container.source);Local(script,2,"Count");Local(script,9,"Extra");
      fo3pipdata::Definitions actual;fo3pipdata::Decode(actual,"SCPT",60,0,script,0,0,0);entry.container=actual.scripts.at(60);
      const auto events=fo3script::ParseEvents(actual.scripts.at(60));
      for(const auto& block:events.blocks)if(block.event=="onactivate")entry.origin.sourceLine=block.sourceLine;
    }
    auto quest=f.quest;Sub(quest,"SCRI",Word(60));auto bytes=event?Owner():Record("QUST",10,quest);
    Append(bytes,Record("SCPT",60,compressed?Compressed(script):script,compressed?0x40000:0));write(bytes);
    fo3script::ScriptMasterMatch declarations;
    const bool valid=fo3script::MatchScriptProgramsToMaster(path,{entry},declarations,cancel,error);
    assert(valid==(kind>=4));
    if(kind<4){
      assert(error=="local declarations do not match source");
      assert(!fo3script::MatchScriptProgramsToMaster(path+"-missing",{entry},report,cancel,error));
      assert(error=="local declarations do not match source"&&report.Matches(f.programs));
    }
  }
  write(f.Master());
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
  Append(b,Owner());
  Append(b,Speakers());
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
  // INFO speaker evidence resolves independently of an actor's optional SCRI.
  // Both placement kinds, both record orders, compression and XXXX NAME work.
  const std::vector<Execution> infoOnly={f.programs[1],f.programs[2]};
  // Exact bodies and actor/quest identities cannot bypass live dialogue's
  // static supported-structure policy. Each attempt includes both phases.
  auto checkInfo=[&](const Bytes& payload,bool expected,bool compressed=false){
    auto bytes=Group(40,Record("INFO",50,compressed?Compressed(payload):payload,compressed?0x40000:0));
    Append(bytes,Speakers());write(bytes);
    fo3script::ScriptMasterMatch structure;
    assert(fo3script::MatchScriptProgramsToMaster(path,infoOnly,structure,cancel,error)==expected);
    if(expected)assert(structure.Matches(infoOnly));else rejects(infoOnly);
  };
  for(unsigned size:{0u,1u,2u,3u,4u,5u,8u})for(bool compressed:{false,true}){
    Bytes metadata;Sub(metadata,"DATA",Bytes(size));Sub(metadata,"TRDT",Bytes(24));
    checkInfo(InfoPayload(metadata),size==3||size==4,compressed);
  }
  {Bytes metadata;Sub(metadata,"TRDT",Bytes(24));checkInfo(InfoPayload(metadata),false);}
  for(unsigned type=0;type<256;++type){
    Bytes metadata;Sub(metadata,"DATA",{uint8_t(type),0,0,0});Sub(metadata,"TRDT",Bytes(24));
    checkInfo(InfoPayload(metadata),type==0||type==3);
  }
  for(unsigned field=1;field<4;++field)for(unsigned bit=0;bit<8;++bit){
    Bytes data(4);data[field]=uint8_t(1u<<bit);
    Bytes metadata;Sub(metadata,"DATA",data);Sub(metadata,"TRDT",Bytes(24));
    checkInfo(InfoPayload(metadata),field==2&&bit!=7);
  }
  // DATA uniqueness is record-wide, even after NEXT; XXXX has the same rules.
  for(bool after:{false,true}){
    Bytes extra;Sub(extra,"DATA",{0,0,0,0});
    auto payload=after?f.info:extra;Append(payload,after?extra:f.info);checkInfo(payload,false);
  }
  for(unsigned size:{3u,4u,5u})for(bool compressed:{false,true}){
    Bytes metadata;Sub(metadata,"XXXX",Word(size));Sub(metadata,"DATA",{});Append(metadata,Bytes(size));
    Sub(metadata,"TRDT",Bytes(16));checkInfo(InfoPayload(metadata),size!=5,compressed);
  }
  for(unsigned size:{0u,1u,12u,15u,16u,20u,21u,24u,25u})for(bool extra:{false,true}){
    Bytes metadata;Sub(metadata,"DATA",{0,0,0,0});Sub(metadata,"TRDT",Bytes(size));
    if(extra)Sub(metadata,"TRDT",Bytes(24)); // a valid response cannot hide a short one
    checkInfo(InfoPayload(metadata),size>=16);
  }
  {Bytes metadata;Sub(metadata,"DATA",{0,0,0,0});checkInfo(InfoPayload(metadata),false);}
  auto noQuest=f.info;noQuest.erase(noQuest.begin()+10,noQuest.begin()+20); // fixture QSTI
  // Parent identity is resolved on the complete second scan, independently of
  // record order, compression or the quest's authored initial running flag.
  for(bool before:{false,true})for(bool compressed:{false,true})for(uint8_t flags:{0,1}){
    Bytes quest;Sub(quest,"DATA",{flags,40});
    auto parent=Record("QUST",20,compressed?Compressed(quest):quest,compressed?0x40000:0);
    Bytes info;Sub(info,"XXXX",Word(4));Sub(info,"QSTI",{});Append(info,Word(20));Append(info,noQuest);
    auto record=Group(40,Record("INFO",50,info));
    b=Speakers(false,false);Append(b,before?parent:record);Append(b,before?record:parent);write(b);
    fo3script::ScriptMasterMatch parents;
    assert(fo3script::MatchScriptProgramsToMaster(path,infoOnly,parents,cancel,error)&&parents.Matches(infoOnly));
  }
  for(int malformed=0;malformed<19;++malformed){
    Bytes quest;Sub(quest,"DATA",{0,40});auto info=f.info;
    if(malformed==6){quest.clear();Sub(quest,"EDID",Text("missing DATA"));}
    if(malformed==7){quest.clear();Sub(quest,"DATA",{0});}
    if(malformed==8)Sub(quest,"QSDT",{0}); // orphan item, decoder diagnostic
    if(malformed==9){Sub(quest,"INDX",{1,0});Sub(quest,"INDX",{1,0});}
    if(malformed==10)quest.push_back(0);
    if(malformed==11)Sub(quest,"SCRI",{1,2,3});
    if(malformed==12)Sub(quest,"CTDA",Bytes(19));
    if(malformed==13)quest={1,2,3,4};
    if(malformed>=15){
      info=noQuest;
      if(malformed!=16)Sub(info,"QSTI",Word(malformed==15?0:malformed==17?50:202));
    }
    b=Group(40,Record("INFO",50,info));Append(b,Speakers(false,false));
    if(malformed!=0)Append(b,Record(malformed==1?"STAT":"QUST",malformed==14?21:20,quest,
      malformed==2?0x20:malformed==13?0x40000:0));
    if(malformed==3)Append(b,ParentQuest());
    if(malformed==4)Append(b,Record("STAT",20,{}));
    if(malformed==5)Append(b,Record("QUST",20,quest,0x20));
    write(b);rejects(infoOnly);
    if(malformed<15)assert(error.find("owning quest")!=std::string::npos);
  }
  // Sharing one quest is valid; two independent INFOs require both parents.
  {
    Bytes second;Sub(second,"QSTI",Word(21));Append(second,noQuest);
    b=Group(40,Record("INFO",50,f.info));Append(b,Group(40,Record("INFO",51,second)));Append(b,Speakers());
    const auto missingSecond=b;Append(b,ParentQuest(false,21));write(b);
    auto evidence=infoOnly;evidence[1].origin.record=51;evidence[1].origin.item=0;evidence[1].program=evidence[0].program;
    evidence[1].owningQuest=21;
    fo3script::ScriptMasterMatch both;
    assert(fo3script::MatchScriptProgramsToMaster(path,evidence,both,cancel,error));
    write(missingSecond);
    assert(!fo3script::MatchScriptProgramsToMaster(path,evidence,both,cancel,error)&&both.Matches(evidence));
  }
  {
    Bytes info;Sub(info,"XXXX",Word(4));Sub(info,"ANAM",{});Append(info,Word(202));
    Append(info,f.info);
    b=Record("QUST",10,f.quest);Append(b,Group(40,Record("INFO",50,info)));Append(b,Speakers());write(b);
    fo3script::ScriptMasterMatch extendedScope;
    assert(fo3script::MatchScriptProgramsToMaster(path,infoOnly,extendedScope,cancel,error));
  }
  // ANAM is the original explicit speaker base restriction. It applies to
  // both phases and regardless of actor/INFO record order or compression.
  for(uint32_t speaker:{0u,202u,102u,999u})for(bool actorFirst:{false,true})for(bool compressed:{false,true}){
    auto info=f.info;Sub(info,"ANAM",Word(speaker));
    auto infoRecord=Group(40,Record("INFO",50,compressed?Compressed(info):info,compressed?0x40000:0));
    b=actorFirst?Speakers(compressed):infoRecord;Append(b,actorFirst?infoRecord:Speakers(compressed));write(b);
    fo3script::ScriptMasterMatch restriction;
    const bool accepted=fo3script::MatchScriptProgramsToMaster(path,infoOnly,restriction,cancel,error);
    assert(accepted==(speaker==0||speaker==202));
    if(!accepted)assert(error=="dialogue speaker differs from original INFO ANAM");
  }
  for(bool creature:{false,true})for(bool reversed:{false,true})for(bool compressed:{false,true}){
    Bytes actor,placement;Sub(actor,"EDID",Text("Speaker"));Sub(actor,"SCRI",Word(999));
    if(compressed){Sub(placement,"XXXX",Word(4));Sub(placement,"NAME",{});Append(placement,Word(202));}
    else Sub(placement,"NAME",Word(202));
    auto baseRecord=Record(creature?"CREA":"NPC_",202,compressed?Compressed(actor):actor,compressed?0x40000:0);
    auto placedRecord=Record(creature?"ACRE":"ACHR",102,compressed?Compressed(placement):placement,compressed?0x40000:0);
    b=Group(40,Record("INFO",50,f.info));
    Append(b,reversed?placedRecord:baseRecord);Append(b,reversed?baseRecord:placedRecord);Append(b,ParentQuest());write(b);
    assert(fo3script::MatchScriptProgramsToMaster(path,infoOnly,owned,cancel,error)&&owned.Matches(infoOnly));
  }
  for(int malformed=0;malformed<17;++malformed){
    Bytes actor,placement;Sub(actor,"EDID",Text("Speaker"));Sub(placement,"NAME",Word(202));
    if(malformed==0)placement.clear();
    if(malformed==1){placement.clear();Sub(placement,"NAME",Word(0));}
    if(malformed==2){placement.clear();Sub(placement,"NAME",{1,2,3});}
    if(malformed==3)Sub(placement,"NAME",Word(202));
    if(malformed==4)actor.push_back(0);
    b=Group(40,Record("INFO",50,f.info));
    if(malformed!=5)Append(b,Record(malformed==6?"REFR":malformed==7?"QUST":"ACHR",102,placement,malformed==8?0x20:0));
    if(malformed!=9)Append(b,Record(malformed==10?"ACTI":malformed==11?"CREA":"NPC_",202,actor,malformed==12?0x20:0));
    if(malformed==13)Append(b,Record("STAT",102,{}));
    if(malformed==14)Append(b,Record("NPC_",202,actor,0x20));
    auto evidence=infoOnly;
    if(malformed==15)evidence[0].owner=0;
    if(malformed==16)evidence[1].owner=999;
    Append(b,ParentQuest());
    write(b);rejects(evidence);
    assert(owned.Matches(infoOnly));
  }
  // Shared actor identity must satisfy the event SCRI as well as INFO binding.
  for(uint32_t script:{60u,999u}){
    Bytes actor,placement;Sub(actor,"SCRI",Word(script));Sub(placement,"NAME",Word(202));
    b=Group(40,Record("INFO",50,f.info));Append(b,Record("SCPT",60,f.event));
    Append(b,Record("NPC_",202,actor));Append(b,Record("ACHR",102,placement));Append(b,ParentQuest());write(b);
    auto shared=infoOnly;auto event=f.programs.back();event.owner=event.reference=102;shared.push_back(event);
    fo3script::ScriptMasterMatch sharedReport;
    assert(fo3script::MatchScriptProgramsToMaster(path,shared,sharedReport,cancel,error)==(script==60));
  }
  // Source identity alone never authenticates an event owner. Every requested
  // owner/base is unique, nondeleted, correctly typed and directly associated.
  const std::vector<Execution> eventOnly={f.programs.back()};
  for(int malformed=0;malformed<21;++malformed){
    Bytes owner,base;Sub(owner,"NAME",Word(malformed==0?202:201));
    Sub(base,"SCRI",Word(malformed==1?61:60));
    if(malformed==2)Sub(owner,"NAME",Word(201));
    if(malformed==3)Sub(base,"SCRI",Word(60));
    if(malformed==4){owner.clear();Sub(owner,"NAME",{1,2,3});}
    if(malformed==5){base.clear();Sub(base,"SCRI",{1,2,3});}
    if(malformed==6){owner.clear();Sub(owner,"EDID",Text("missing"));}
    if(malformed==7){base.clear();Sub(base,"EDID",Text("missing"));}
    if(malformed==16){owner.clear();Sub(owner,"NAME",Word(0));}
    if(malformed==17){base.clear();Sub(base,"SCRI",Word(0));}
    if(malformed==18)base.push_back(0);
    b=Record("SCPT",60,f.event);
    Append(b,Record(malformed==8?"STAT":"REFR",101,owner,malformed==9?0x20:0));
    if(malformed!=10)Append(b,Record(malformed==11?"QUST":"ACTI",201,base,malformed==12?0x20:0));
    if(malformed==13)Append(b,Record("STAT",101,{}));
    if(malformed==14)Append(b,Record("STAT",201,{}));
    if(malformed==19)Append(b,Record("REFR",101,owner,0x20));
    if(malformed==20)Append(b,Record("ACTI",201,base,0x20));
    auto evidence=eventOnly;if(malformed==15)evidence[0].owner=102;
    write(b);rejects(evidence);
  }
  // Quest-owned GameMode uses the quest's own SCRI, not a placed NAME.
  auto questEvent=eventOnly;questEvent[0].owner=questEvent[0].reference=10;
  Bytes questOwner;Sub(questOwner,"SCRI",Word(60));
  b=Record("QUST",10,questOwner);Append(b,Record("SCPT",60,f.event));write(b);
  assert(fo3script::MatchScriptProgramsToMaster(path,questEvent,owned,cancel,error));
  Sub(questOwner,"SCRI",Word(60));b=Record("QUST",10,questOwner);Append(b,Record("SCPT",60,f.event));write(b);
  assert(!fo3script::MatchScriptProgramsToMaster(path,questEvent,owned,cancel,error)&&owned.Matches(questEvent));
  // Base-before-owner, owner-before-base and every supported placement kind.
  for(const char* type:{"REFR","ACHR","ACRE"}){
    Bytes ref,base;Sub(ref,"NAME",Word(201));Sub(base,"SCRI",Word(60));
    const char* baseType=std::strcmp(type,"ACHR")==0?"NPC_":std::strcmp(type,"ACRE")==0?"CREA":"ACTI";
    b=Record(type,101,ref);Append(b,Record("SCPT",60,f.event));Append(b,Record(baseType,201,base));write(b);
    assert(fo3script::MatchScriptProgramsToMaster(path,eventOnly,owned,cancel,error));
    b=Record(type,101,ref);Append(b,Record("SCPT",60,f.event));Append(b,Record(std::strcmp(type,"REFR")==0?"NPC_":"ACTI",201,base));write(b);
    assert(!fo3script::MatchScriptProgramsToMaster(path,eventOnly,owned,cancel,error)&&owned.Matches(eventOnly));
  }
  Bytes extendedOwner,extendedBase;
  Sub(extendedOwner,"XXXX",Word(4));Sub(extendedOwner,"NAME",{});Append(extendedOwner,Word(201));
  Sub(extendedBase,"XXXX",Word(4));Sub(extendedBase,"SCRI",{});Append(extendedBase,Word(60));
  b=Record("REFR",101,Compressed(extendedOwner),0x40000);
  Append(b,Record("ACTI",201,Compressed(extendedBase),0x40000));Append(b,Record("SCPT",60,f.event));write(b);
  assert(fo3script::MatchScriptProgramsToMaster(path,eventOnly,owned,cancel,error));
  // Source equality cannot make an unsupported or unsatisfied Begin filter
  // executable. The worker uses the same filter predicate as the interpreter.
  for(const std::string filter:{"", " Player", " OtherRef", " player extra"}){
    Bytes filtered;Script(filtered,"Begin OnActivate"+filter+"\nRewardKarma 4\nEnd");
    fo3pipdata::Definitions definitions;fo3pipdata::Decode(definitions,"SCPT",60,0,filtered,0,0,0);
    auto evidence=eventOnly;evidence[0].container=definitions.scripts.at(60);
    for(uint32_t activator:{0u,fo3player::PlayerRef,99u}){
      evidence[0].activator=activator;
      b=Owner();Append(b,Record("SCPT",60,filtered));write(b);
      const bool expected=filter.empty()||(filter==" Player"&&activator==fo3player::PlayerRef);
      fo3script::ScriptMasterMatch filteredReport;
      assert(fo3script::MatchScriptProgramsToMaster(path,evidence,filteredReport,cancel,error)==expected);
      assert(filteredReport.Verified()==expected);
    }
  }
  // Exact master evidence does not exempt even the same SCPT's incoming edge.
  Bytes cell,base,ref;Sub(cell,"DATA",{1});Sub(base,"MODL",Text("fixture.nif"));
  Sub(ref,"NAME",Word(901));Sub(ref,"DATA",Bytes(24));
  b=Record("CELL",900,cell);Append(b,Record("STAT",901,base));
  auto children=Record("REFR",100,ref);auto scope=Group(900,children);Put(scope,12,6);Append(b,scope);
  Append(b,Owner());
  const auto world=b;
  Append(b,Record("SCPT",60,f.event));write(b);
  assert(fo3script::MatchScriptProgramsToMaster(path,eventOnly,owned,cancel,error)&&owned.Matches(eventOnly));
  fo3scene::ResidentAuthoredAdmission admission;admission.cell=999;admission.affected={888};
  assert(!fo3scene::PrepareResidentAuthoredAdmission(path,900,{100},admission,cancel,error));
  assert(error.find("SCRO from SCPT 60 to 100")!=std::string::npos&&admission.Matches(999,{888}));
  assert(owned.Matches(eventOnly));
  // A matched script can reference only a list, while nested/cyclic list
  // metadata reaches the affected reference without any direct SCRO hit.
  Bytes indirect,outer,inner;
  Sub(indirect,"EDID",Text("IndirectFixture"));Sub(indirect,"SCHR",Bytes(20));
  Sub(indirect,"SCDA",{1,2});Sub(indirect,"SCTX",Text(eventOnly[0].container.source));
  Sub(indirect,"SCRO",Word(70));Sub(indirect,"SCRV",Word(2));
  fo3pipdata::Definitions definitions;fo3pipdata::Decode(definitions,"SCPT",60,0,indirect,0,0,0);
  auto throughList=eventOnly;throughList[0].container=definitions.scripts.at(60);
  Sub(outer,"LNAM",Word(71));Sub(inner,"LNAM",Word(70));Sub(inner,"LNAM",Word(100));
  b=world;Append(b,Record("SCPT",60,indirect));Append(b,Record("FLST",70,outer));Append(b,Record("FLST",71,inner));write(b);
  assert(fo3script::MatchScriptProgramsToMaster(path,throughList,owned,cancel,error)&&owned.Matches(throughList));
  assert(!fo3scene::PrepareResidentAuthoredAdmission(path,900,{100},admission,cancel,error));
  assert(error.find("FLST LNAM from FLST 71 to 100")!=std::string::npos&&admission.Matches(999,{888}));
  assert(owned.Matches(throughList));
  std::remove(path.c_str());rejects(f.programs);
  std::cout<<"Strict master program matching, exact report binding and fail-closed framing passed\n";
}
