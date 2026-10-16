#include "world/fo3-resident-authored-admission.h"
#include "data/fo3-esm-reader.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <zlib.h>
using Bytes=std::vector<uint8_t>;
void u32(Bytes& b,size_t p,uint32_t v){if(b.size()<p+4)b.resize(p+4);std::memcpy(b.data()+p,&v,4);}
Bytes word(uint32_t v){Bytes b(4);u32(b,0,v);return b;}
Bytes text(const std::string& s){Bytes b(s.begin(),s.end());b.push_back(0);return b;}
void append(Bytes& b,const Bytes& v){b.insert(b.end(),v.begin(),v.end());}
void sub(Bytes& b,const char* t,const Bytes& v){b.insert(b.end(),t,t+4);b.push_back(v.size()&255);b.push_back(v.size()>>8);append(b,v);}
Bytes record(const char* t,uint32_t id,const Bytes& payload,uint32_t flags=0){
    Bytes b(24);std::memcpy(b.data(),t,4);u32(b,4,payload.size());u32(b,8,flags);u32(b,12,id);append(b,payload);return b;
}
Bytes group(uint32_t id,uint32_t type,const Bytes& payload){
    Bytes b(24);std::memcpy(b.data(),"GRUP",4);u32(b,4,payload.size()+24);u32(b,8,id);u32(b,12,type);append(b,payload);return b;
}
struct Options{
    const char* baseType="STAT";const char* refType="REFR";const char* extra=nullptr;
    uint32_t refFlags=0,baseFlags=0,owner=10,world=0;int malformed=0;
    bool nav=false,pack=false,light=false,duplicate=false,missing=false,script=false,interior=true;
    bool baseTarget=false,lightModel=false,deletedNav=false;uint32_t targetSize=12;
    const char* incoming=nullptr;const char* incomingType="REFR";
    uint32_t incomingTarget=30,incomingCell=11,incomingWorld=0,incomingFlags=0;
    int incomingSize=-1;bool duplicateLink=false,compressedIncoming=false,extendedIncoming=false;
};
Bytes fixture(Options o={}){
    Bytes b,cell,base,ref,children;
    sub(cell,"DATA",Bytes{uint8_t(o.interior?1:0)});
    append(b,record("CELL",10,cell));
    sub(base,"EDID",text("Fixture"));
    if(!o.light||o.lightModel)sub(base,"MODL",text("fixture.nif"));
    if(o.script)sub(base,"SCRI",word(777));
    if(!o.missing)append(b,record(o.light?"LIGH":o.baseType,20,base,o.baseFlags));
    sub(ref,"NAME",word(20));Bytes transform(24);
    if(o.malformed==1){float v=std::numeric_limits<float>::infinity();std::memcpy(transform.data(),&v,4);}
    sub(ref,"DATA",transform);
    if(o.extra)sub(ref,o.extra,word(888));
    if(o.malformed==2)sub(ref,"NAME",word(20));
    if(o.malformed==3)ref.push_back(1);
    if(o.malformed==4){Bytes ext;u32(ext,0,100);sub(ref,"XXXX",ext);sub(ref,"EDID",Bytes{0});}
    if(o.malformed==5)sub(ref,"XSCL",word(0));
    if(o.malformed==6)sub(ref,"XESP",Bytes(8));
    if(o.malformed==7)sub(ref,"XSCL",Bytes(3));
    append(children,record(o.refType,30,ref,o.refFlags));
    if(o.duplicate)append(children,record(o.refType,30,ref));
    auto scope=group(o.owner,6,group(o.owner,9,children));
    if(o.world)scope=group(o.world,1,scope);
    append(b,scope);
    if(o.incoming){
        Bytes incoming;sub(incoming,"NAME",word(20));sub(incoming,"DATA",Bytes(24));
        const size_t size=o.incomingSize>=0?size_t(o.incomingSize):
            !std::strcmp(o.incoming,"XESP")?8:!std::strcmp(o.incoming,"XTEL")?32:4;
        Bytes data(size);if(size>=4)u32(data,0,o.incomingTarget);
        if(o.extendedIncoming){sub(incoming,"XXXX",word(data.size()));sub(incoming,o.incoming,{});append(incoming,data);}
        else sub(incoming,o.incoming,data);
        if(o.duplicateLink)sub(incoming,o.incoming,data);
        if(o.compressedIncoming){
            uLongf n=compressBound(incoming.size());Bytes compressed(n+4);u32(compressed,0,incoming.size());
            assert(compress2(compressed.data()+4,&n,incoming.data(),incoming.size(),Z_BEST_SPEED)==Z_OK);
            compressed.resize(n+4);incoming=std::move(compressed);o.incomingFlags|=0x40000u;
        }
        auto dependency=record(o.incomingType,60,incoming,o.incomingFlags);
        auto foreign=group(o.incomingCell,6,dependency);
        if(o.incomingWorld)foreign=group(o.incomingWorld,1,foreign);
        append(b,foreign);
    }
    if(o.nav){Bytes data(12),p;u32(data,0,10);sub(p,"DATA",data);
        // Ownership must come from DATA, not this deliberately unrelated group.
        append(b,group(999,6,record("NAVM",40,p,o.deletedNav?0x20u:0)));}
    if(o.pack){Bytes data(o.targetSize),p;u32(data,4,o.baseTarget?20:30);sub(p,"PTDT",data);append(b,record("PACK",50,p));}
    if(o.malformed==8)b.push_back(1);
    if(o.malformed==9)u32(b,4,0xffffffffu);
    return b;
}
const std::string path="/tmp/falloutquest-resident-authored-admission.esm";
void write(const Bytes& b){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());}
void original(const std::string& master){
    // Resolve fixtures from Bethesda's records; no invented original form IDs.
    FILE* f=std::fopen(master.c_str(),"rb");assert(f);const auto size=fo3esm::FileSize(f);
    uint32_t cell=0;std::unordered_map<uint32_t,std::string> types;
    std::unordered_set<uint32_t> linkedTargets,interiorCells;
    while(ftello(f)<size){
        uint8_t h[24];assert(fo3esm::ReadExact(f,h,24));const auto n=fo3esm::ReadU32(h+4);
        if(!std::memcmp(h,"GRUP",4))continue;
        const auto id=fo3esm::ReadU32(h+12);const std::string type(reinterpret_cast<char*>(h),4);
        if(type=="STAT"||type=="LIGH"||type=="DOOR")types[id]=type;
        if(type=="CELL"){
            Bytes b;assert(fo3esm::ReadPayloadCurrent(f,n,fo3esm::ReadU32(h+8),b));
            fo3esm::WalkSubrecords(b,[&](const char* t,const uint8_t* p,uint32_t count){
                if(!std::memcmp(t,"EDID",4)&&fo3esm::ZString(p,count)=="MegatonTheBrassLantern")cell=id;
                if(!std::memcmp(t,"DATA",4)&&count&&(p[0]&1))interiorCells.insert(id);
            });
        }else if((type=="REFR"||type=="ACHR"||type=="ACRE")&&!(fo3esm::ReadU32(h+8)&0x20u)){
            Bytes b;assert(fo3esm::ReadPayloadCurrent(f,n,fo3esm::ReadU32(h+8),b));
            fo3esm::WalkSubrecords(b,[&](const char* t,const uint8_t* p,uint32_t count){
                if(!std::memcmp(t,"XLKR",4)&&count==4)linkedTargets.insert(fo3esm::ReadU32(p));
            });
        }else assert(!fseeko(f,n,SEEK_CUR));
    }
    assert(cell);assert(!fseeko(f,0,SEEK_SET));
    struct Frame{uint64_t end;uint32_t label,type;};std::vector<Frame> stack;
    std::unordered_map<uint32_t,uint32_t> refs;
    std::vector<std::pair<uint32_t,uint32_t>> incomingFixtures;
    while(ftello(f)<size){
        const auto start=ftello(f);while(!stack.empty()&&uint64_t(start)>=stack.back().end)stack.pop_back();
        uint8_t h[24];assert(fo3esm::ReadExact(f,h,24));const auto n=fo3esm::ReadU32(h+4);
        if(!std::memcmp(h,"GRUP",4)){
            stack.push_back({uint64_t(start)+n,fo3esm::ReadU32(h+8),fo3esm::ReadU32(h+12)});continue;
        }
        uint32_t owner=0;for(auto g=stack.rbegin();g!=stack.rend();++g)
            if(g->type==6||g->type==8||g->type==9||g->type==10){owner=g->label;break;}
        const auto id=fo3esm::ReadU32(h+12),flags=fo3esm::ReadU32(h+8);
        if(!std::memcmp(h,"REFR",4)&&(owner==cell||linkedTargets.count(id))){
            Bytes b;assert(fo3esm::ReadPayloadCurrent(f,n,fo3esm::ReadU32(h+8),b));
            uint32_t base=0;bool simple=true;
            fo3esm::WalkSubrecords(b,[&](const char* t,const uint8_t* p,uint32_t count){
                if(!std::memcmp(t,"NAME",4)&&count==4)base=fo3esm::ReadU32(p);
                const std::string tag(t,4);
                if(tag!="NAME"&&tag!="DATA"&&tag!="EDID"&&tag!="XSCL"&&tag!="XESP"&&
                    tag!="XOWN"&&tag!="XRNK"&&tag!="XGLB")simple=false;
            });
            if(owner==cell)refs[id]=base;
            const auto type=types.find(base);
            if(simple&&!(flags&(0x20u|0x8000u))&&interiorCells.count(owner)&&linkedTargets.count(id)&&
                type!=types.end()&&(type->second=="STAT"||type->second=="LIGH"))
                incomingFixtures.emplace_back(owner,id);
        }else assert(!fseeko(f,n,SEEK_CUR));
    }
    std::fclose(f);
    bool light=false,door=false,nav=false;std::atomic<bool> cancel{false};
    std::unordered_map<std::string,bool> logged;
    fo3scene::ResidentAuthoredAdmission out;std::string error;
    for(const auto& ref:refs){
        const auto type=types.find(ref.second);if(type==types.end())continue;
        if((type->second=="LIGH"&&light)||(type->second=="DOOR"&&door)||(type->second=="STAT"&&nav))continue;
        const bool admitted=fo3scene::PrepareResidentAuthoredAdmission(master,cell,{ref.first},out,cancel,error);
        if(!admitted&&!logged[type->second]){
            std::cout<<"Original "<<type->second<<" guard: "<<error<<std::endl;logged[type->second]=true;
        }
        if(type->second=="LIGH"&&admitted)light=true;
        if(type->second=="DOOR"){assert(!admitted);door=true;}
        if(type->second=="STAT"&&error=="geometry mutation requires navigation reconciliation")nav=true;
    }
    if(!light||!door||!nav)std::cerr<<"Original coverage light="<<light<<" door="<<door<<" nav="<<nav<<" last="<<error<<"\n";
    assert(light&&door&&nav);
    bool incoming=false;
    for(const auto& fixture:incomingFixtures){
        const auto previous=out;
        assert(!fo3scene::PrepareResidentAuthoredAdmission(master,fixture.first,{fixture.second},out,cancel,error));
        assert(out.cell==previous.cell&&out.affected==previous.affected);
        if(error.find("unowned incoming authored link: XLKR")!=std::string::npos){incoming=true;break;}
    }
    assert(incoming);
    std::cout<<"Original incoming XLKR target resolved from authored records and rejected\n";
    std::cout<<"Original Brass Lantern: light guard admitted, load door and navmeshed geometry rejected\n";
}
int main(int argc,char** argv){
    std::atomic<bool> cancel{false};std::string error;
    fo3scene::ResidentAuthoredAdmission out;out.cell=999;out.affected={888};
    auto rejected=[&](Options o){write(fixture(o));
        assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
        assert(!error.empty()&&out.cell==999&&out.affected==std::vector<uint32_t>{888});};
    for(const char* type:{"NPC_","CREA","DOOR","FURN","IDLM","CONT","MISC","SCOL","UNKN"}){
        Options o;o.baseType=type;rejected(o);
    }
    for(const char* type:{"ACHR","ACRE","UNKN"}){Options o;o.refType=type;rejected(o);}
    for(const char* tag:{"XTEL","XEMI","XPRM","XRMR","XLKR","UNKN"}){Options o;o.extra=tag;rejected(o);}
    for(uint32_t flags:{0x20u,0x8000u}){
        Options o;o.refFlags=flags;rejected(o);o.refFlags=0;o.baseFlags=flags;rejected(o);
    }
    for(int i=1;i<=9;++i){Options o;o.malformed=i;rejected(o);}
    {Options o;o.owner=11;rejected(o);}
    {Options o;o.world=11;rejected(o);}
    {Options o;o.duplicate=true;rejected(o);}
    {Options o;o.missing=true;rejected(o);}
    {Options o;o.script=true;rejected(o);}
    {Options o;o.interior=false;rejected(o);}
    {Options o;o.nav=true;rejected(o);}
    {Options o;o.pack=true;rejected(o);}
    {Options o;o.pack=true;o.targetSize=16;rejected(o);}
    {Options o;o.pack=true;o.baseTarget=true;rejected(o);}
    {Options o;o.pack=true;o.targetSize=13;rejected(o);}
    {Options o;o.light=true;o.lightModel=true;o.nav=true;rejected(o);}
    // Incoming dependencies anywhere in the master, not only resident meshes.
    for(const char* kind:{"REFR","ACHR","ACRE"})for(const char* tag:{"XLKR","XTEL","XEMI","XESP"}){
        Options o;o.incoming=tag;o.incomingType=kind;rejected(o);
        assert(error.find(tag)!=std::string::npos&&error.find("60")!=std::string::npos);
        o.incomingCell=10;rejected(o); // Same CELL is not sufficient ownership.
        o.incomingCell=11;o.incomingWorld=12;rejected(o);
        o.incomingWorld=0;o.incomingSize=1;rejected(o);
        o.incomingSize=-1;o.duplicateLink=true;rejected(o);
        o.duplicateLink=false;o.compressedIncoming=true;rejected(o);
        o.compressedIncoming=false;o.extendedIncoming=true;rejected(o);
    }
    write(fixture());cancel=true;
    assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));cancel=false;
    for(const auto& ids:std::vector<std::vector<uint32_t>>{{},{0},{30,30},{31,30},{31}}){
        assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,ids,out,cancel,error));
        assert(out.cell==999);
    }
    assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
    assert(error.empty()&&out.Matches(10,{30})&&!out.Matches(11,{30})&&!out.Matches(10,{31}));
    for(const char* tag:{"XLKR","XTEL","XEMI","XESP"}){
        Options o;o.incoming=tag;o.incomingTarget=99;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
        o.compressedIncoming=true;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
        o.compressedIncoming=false;o.extendedIncoming=true;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
        o.incomingTarget=30;o.incomingFlags=0x20;o.incomingSize=1;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
    }
    {Options o;o.incoming="XESP";o.incomingCell=10;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30,60},out,cancel,error));
        assert(out.Matches(10,{30,60}));
        o.incomingCell=11;write(fixture(o));
        assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,{30,60},out,cancel,error));
        assert(out.Matches(10,{30,60}));
        write(fixture());assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
    }
    {Options o;o.light=true;o.nav=true;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));}
    {Options o;o.nav=true;o.deletedNav=true;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));}
    // Complete logical set: a valid visual cannot hide an unresolvable no-mesh ID.
    assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,{30,99},out,cancel,error));
    assert(out.Matches(10,{30}));
    if(argc>1)original(argv[1]);
    std::remove(path.c_str());std::cout<<"Resident authored admission tests passed\n";
}
